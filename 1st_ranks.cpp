#include "1st_ranks.h"

#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <sstream>

#include "filesystem.h"
#include "schemasystem/schemasystem.h"

#ifndef SEMVER
#define SEMVER "Local"
#endif

OneSTRanks g_OneSTRanks;
PLUGIN_EXPOSE(OneSTRanks, g_OneSTRanks);

IVEngineServer2 *engine = nullptr;
extern ISource2Server *g_pSource2Server;
IGameEventSystem *g_pGameEventSystem = nullptr;
extern INetworkMessages *g_pNetworkMessages;
extern IFileSystem *g_pFullFileSystem;
CEntitySystem *g_pEntitySystem = nullptr;
CGlobalVars *gpGlobals = nullptr;
IUtilsApi *g_pUtils = nullptr;
IMySQLClient *g_pMysqlClient = nullptr;
IMySQLConnection *g_pConnection = nullptr;

SH_DECL_HOOK3_void(IServerGameDLL, GameFrame, SH_NOATTRIB, 0, bool, bool, bool);

CGameEntitySystem *GameEntitySystem()
{
    return reinterpret_cast<CGameEntitySystem *>(g_pEntitySystem);
}

CON_COMMAND_EXTERN(mm_ranks_reload, ReloadOneSTRanksConfig, "Reload 1sT-Ranks config and cache");
CON_COMMAND_EXTERN(mm_ranks_refresh, RefreshOneSTRanksCache, "Refresh 1sT-Ranks cache");

void ReloadOneSTRanksConfig(const CCommandContext &, const CCommand &)
{
    g_OneSTRanks.LoadConfig();
    g_OneSTRanks.RefreshCache();
}

void RefreshOneSTRanksCache(const CCommandContext &, const CCommand &)
{
    g_OneSTRanks.RefreshCache();
}

static void StartupServer()
{
    if (g_pUtils)
    {
        g_pEntitySystem = g_pUtils->GetCEntitySystem();
        gpGlobals = g_pUtils->GetCGlobalVars();
    }
}

bool OneSTRanks::Load(PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late)
{
    PLUGIN_SAVEVARS();
    GET_V_IFACE_CURRENT(GetEngineFactory, engine, IVEngineServer2, SOURCE2ENGINETOSERVER_INTERFACE_VERSION);
    GET_V_IFACE_CURRENT(GetServerFactory, g_pSource2Server, ISource2Server, SOURCE2SERVER_INTERFACE_VERSION);
    GET_V_IFACE_ANY(GetEngineFactory, g_pGameEventSystem, IGameEventSystem, GAMEEVENTSYSTEM_INTERFACE_VERSION);
    GET_V_IFACE_CURRENT(GetEngineFactory, g_pNetworkMessages, INetworkMessages, NETWORKMESSAGES_INTERFACE_VERSION);
    GET_V_IFACE_CURRENT(GetFileSystemFactory, g_pFullFileSystem, IFileSystem, FILESYSTEM_INTERFACE_VERSION);

    SH_ADD_HOOK(IServerGameDLL, GameFrame, g_pSource2Server, SH_MEMBER(this, &OneSTRanks::GameFrame), true);
    ConVar_Register(FCVAR_RELEASE | FCVAR_SERVER_CAN_EXECUTE | FCVAR_GAMEDLL);

    LoadConfig();
    return true;
}

bool OneSTRanks::Unload(char *error, size_t maxlen)
{
    SH_REMOVE_HOOK(IServerGameDLL, GameFrame, g_pSource2Server, SH_MEMBER(this, &OneSTRanks::GameFrame), true);
    if (g_pUtils)
        g_pUtils->ClearAllHooks(g_PLID);
    if (g_pConnection)
    {
        g_pConnection->Destroy();
        g_pConnection = nullptr;
    }
    return true;
}

void OneSTRanks::AllPluginsLoaded()
{
    int ret = 0;
    g_pUtils = (IUtilsApi *)g_SMAPI->MetaFactory(Utils_INTERFACE, &ret, nullptr);
    if (ret == META_IFACE_FAILED)
    {
        META_CONPRINTF("[%s] Missing cs2-menus Utils plugin\n", GetLogTag());
        engine->ServerCommand(("meta unload " + std::to_string(g_PLID)).c_str());
        return;
    }

    ISQLInterface *sql = (ISQLInterface *)g_SMAPI->MetaFactory(SQLMM_INTERFACE, &ret, nullptr);
    if (ret == META_IFACE_FAILED)
    {
        META_CONPRINTF("[%s] Missing sql_mm plugin\n", GetLogTag());
        engine->ServerCommand(("meta unload " + std::to_string(g_PLID)).c_str());
        return;
    }

    g_pMysqlClient = sql->GetMySQLClient();
    g_pUtils->RegCommand(g_PLID, {}, {"!rank", "/rank"}, [this](int slot, const char *args) {
        return ShowRankCommand(slot, args);
    });
    g_pUtils->StartupServer(g_PLID, StartupServer);
    if (!ConnectDatabase())
        return;
}

bool OneSTRanks::LoadConfig()
{
    m_rankMap.clear();
    KeyValues *kv = new KeyValues("1sT-Ranks");
    if (!kv->LoadFromFile(g_pFullFileSystem, "addons/configs/1sT-Ranks.cfg"))
    {
        META_CONPRINTF("[%s] Failed to load addons/configs/1sT-Ranks.cfg\n", GetLogTag());
        delete kv;
        return false;
    }

    m_tableName = kv->GetString("TableName", "lvl_base");
    m_rankType = kv->GetInt("RankType", 12);
    m_wins = kv->GetInt("Wins", 777);
    m_refreshSeconds = std::max(5.0f, static_cast<float>(std::atof(kv->GetString("RefreshSeconds", "30"))));
    m_revealOnScoreboard = kv->GetInt("RevealOnScoreboard", 1) != 0;

    KeyValues *rankKv = kv->FindKey("FakeRank", false);
    if (rankKv)
    {
        FOR_EACH_VALUE(rankKv, value)
        {
            m_rankMap[std::atoi(value->GetName())] = value->GetInt(nullptr, 0);
        }
    }
    delete kv;
    return true;
}

bool OneSTRanks::ConnectDatabase()
{
    KeyValues *db = new KeyValues("Databases");
    if (!db->LoadFromFile(g_pFullFileSystem, "addons/configs/databases.cfg"))
    {
        META_CONPRINTF("[%s] Failed to load addons/configs/databases.cfg\n", GetLogTag());
        delete db;
        return false;
    }

    KeyValues *ranks = db->FindKey("ranks", false);
    if (!ranks)
        ranks = db->FindKey("rank", false);
    if (!ranks)
    {
        META_CONPRINTF("[%s] Missing \"ranks\" or \"rank\" section in addons/configs/databases.cfg\n", GetLogTag());
        delete db;
        return false;
    }

    MySQLConnectionInfo info;
    info.host = ranks->GetString("host", nullptr);
    info.user = ranks->GetString("user", nullptr);
    info.pass = ranks->GetString("pass", nullptr);
    info.database = ranks->GetString("database", nullptr);
    info.port = ranks->GetInt("port", 3306);
    info.timeout = ranks->GetInt("timeout", 10);

    if (g_pConnection)
        g_pConnection->Destroy();
    g_pConnection = g_pMysqlClient->CreateMySQLConnection(info);
    delete db;

    g_pConnection->Connect([this](bool ok) {
        m_connected = ok;
        if (!ok)
        {
            META_CONPRINTF("[%s] Failed to connect to ranks database\n", GetLogTag());
            return;
        }
        META_CONPRINTF("[%s] Connected to ranks database\n", GetLogTag());
        RefreshCache();
    });
    return true;
}

void OneSTRanks::RefreshCache()
{
    if (!m_connected || !g_pConnection || m_queryRunning)
        return;

    m_queryRunning = true;
    char query[512];
    g_SMAPI->Format(query, sizeof(query), "SELECT `steam`, `rank`, `value` FROM `%s`", m_tableName.c_str());
    g_pConnection->Query(query, [this](ISQLQuery *sqlQuery) {
        std::map<std::string, int> fresh;
        if (sqlQuery)
        {
            ISQLResult *result = sqlQuery->GetResultSet();
            if (result)
            {
                unsigned int steamCol = 0, rankCol = 1;
                result->FieldNameToNum("steam", &steamCol);
                result->FieldNameToNum("rank", &rankCol);
                while (result->MoreRows())
                {
                    result->FetchRow();
                    const char *steam = result->GetString(steamCol);
                    if (steam && steam[0])
                        fresh[steam] = result->GetInt(rankCol);
                }
            }
        }

        size_t cachedRows = 0;
        {
            std::lock_guard<std::mutex> lock(m_cacheMutex);
            m_playerLevels.swap(fresh);
            cachedRows = m_playerLevels.size();
        }
        m_queryRunning = false;
        META_CONPRINTF("[%s] Cached %zu rank rows from `%s`\n", GetLogTag(), cachedRows, m_tableName.c_str());
    });
}

void OneSTRanks::GameFrame(bool simulating, bool bFirstTick, bool bLastTick)
{
    if (!m_connected)
        RETURN_META(MRES_IGNORED);

    const float now = gpGlobals ? gpGlobals->curtime : static_cast<float>(std::time(nullptr));
    if (now >= m_nextRefresh)
    {
        m_nextRefresh = now + m_refreshSeconds;
        RefreshCache();
    }

    CPlayerBitVec revealFilter;
    int revealCount = 0;

    for (int slot = 0; slot < 64; ++slot)
    {
        CCSPlayerController *controller = CCSPlayerController::FromSlot(slot);
        if (!controller || !controller->IsConnected() || controller->m_steamID() == 0)
            continue;

        ApplyPlayerRank(slot);

        CBasePlayerPawn *pawn = controller->m_hPawn().Get();
        if (!pawn || !m_revealOnScoreboard)
            continue;

        auto movement = pawn->m_pMovementServices();
        if (!movement)
            continue;

        uint64_t buttons = movement->m_nButtons().m_pButtonStates()[0];
        if ((buttons & (1ULL << 33)) && !(m_oldButtons[slot] & (1ULL << 33)))
        {
            revealFilter.Set(slot);
            ++revealCount;
        }
        m_oldButtons[slot] = buttons;
    }

    if (revealCount > 0)
        RevealRanks(revealFilter);

    RETURN_META(MRES_IGNORED);
}

void OneSTRanks::ApplyPlayerRank(int slot)
{
    int level = 0;
    if (!GetPlayerLevel(slot, level))
        return;

    CCSPlayerController *controller = CCSPlayerController::FromSlot(slot);
    if (!controller)
        return;

    const int displayRank = GetDisplayRankForLevel(level);
    if (displayRank <= 0)
        return;

    controller->m_iCompetitiveWins() = m_wins;
    controller->m_iCompetitiveRanking() = displayRank;
    controller->m_iCompetitiveRankType() = m_rankType;

    if (g_pUtils)
    {
        g_pUtils->SetStateChanged((CBaseEntity *)controller, "CCSPlayerController", "m_iCompetitiveWins");
        g_pUtils->SetStateChanged((CBaseEntity *)controller, "CCSPlayerController", "m_iCompetitiveRanking");
        g_pUtils->SetStateChanged((CBaseEntity *)controller, "CCSPlayerController", "m_iCompetitiveRankType");
    }
}

bool OneSTRanks::GetPlayerLevel(int slot, int &level)
{
    CCSPlayerController *controller = CCSPlayerController::FromSlot(slot);
    if (!controller || !controller->IsConnected() || controller->m_steamID() == 0)
        return false;

    const std::string steam = Steam2FromAccountId(controller->m_steamID());
    {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        auto found = m_playerLevels.find(steam);
        if (found == m_playerLevels.end())
            return false;
        level = found->second;
    }
    return true;
}

bool OneSTRanks::ShowRankCommand(int slot, const char *)
{
    if (!g_pUtils)
        return false;

    if (!m_connected)
    {
        g_pUtils->PrintToChat(slot, " [1sT-Ranks] Database not connected.");
        return true;
    }

    int level = 0;
    if (!GetPlayerLevel(slot, level))
    {
        RefreshCache();
        g_pUtils->PrintToChat(slot, " [1sT-Ranks] Rank not found yet.");
        return true;
    }

    const int displayRank = GetDisplayRankForLevel(level);
    g_pUtils->PrintToChat(slot, " [1sT-Ranks] Rank: %d | Scoreboard icon: %d", level, displayRank);
    return true;
}

int OneSTRanks::GetDisplayRankForLevel(int level) const
{
    auto exact = m_rankMap.find(level);
    if (exact != m_rankMap.end())
        return exact->second;
    if (m_rankMap.empty())
        return 0;

    auto upper = m_rankMap.upper_bound(level);
    if (upper == m_rankMap.begin())
        return 0;
    --upper;
    return upper->second;
}

std::string OneSTRanks::Steam2FromAccountId(uint32 accountId) const
{
    std::ostringstream out;
    out << "STEAM_1:" << (accountId & 1) << ":" << (accountId / 2);
    return out.str();
}

void OneSTRanks::RevealRanks(CPlayerBitVec &filter)
{
    INetworkMessageInternal *netmsg = g_pNetworkMessages->FindNetworkMessagePartial("CCSUsrMsg_ServerRankRevealAll");
    if (!netmsg)
        return;

    CNetMessage *msg = netmsg->AllocateMessage();
    g_pGameEventSystem->PostEventAbstract(-1, false, ABSOLUTE_PLAYER_LIMIT, reinterpret_cast<const uint64 *>(filter.Base()), netmsg, msg, 0, NetChannelBufType_t::BUF_RELIABLE);
    delete msg;
}

const char *OneSTRanks::GetAuthor() { return "1sT / based on Pisex LR_FakeRanks"; }
const char *OneSTRanks::GetName() { return "1sT-Ranks"; }
const char *OneSTRanks::GetDescription() { return "Read-only DB backed fake scoreboard ranks"; }
const char *OneSTRanks::GetURL() { return "https://github.com/Pisex/cs2-lvl_ranks_modules/tree/main/LR_FakeRanks"; }
const char *OneSTRanks::GetLicense() { return "GPL-3.0"; }
const char *OneSTRanks::GetVersion() { return SEMVER; }
const char *OneSTRanks::GetDate() { return __DATE__; }
const char *OneSTRanks::GetLogTag() { return "1sT-Ranks"; }
