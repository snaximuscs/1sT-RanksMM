#pragma once

#include <ISmmPlugin.h>
#include <igameevents.h>
#include <iserver.h>
#include <networksystem/inetworkmessages.h>
#include <networksystem/inetworkserializer.h>
#include <inetchannel.h>
#include <convar.h>

#include <atomic>
#include <map>
#include <mutex>
#include <string>

#include "ehandle.h"
#include "entitysystem.h"
#include "CCSPlayerController.h"
#include "module.h"
#include "include/menus.h"
#include "include/mysql_mm.h"

class OneSTRanks final : public ISmmPlugin, public IMetamodListener
{
public:
    bool Load(PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late) override;
    bool Unload(char *error, size_t maxlen) override;
    void AllPluginsLoaded() override;
    void GameFrame(bool simulating, bool bFirstTick, bool bLastTick);
    bool LoadConfig();
    void RefreshCache();

private:
    const char *GetAuthor() override;
    const char *GetName() override;
    const char *GetDescription() override;
    const char *GetURL() override;
    const char *GetLicense() override;
    const char *GetVersion() override;
    const char *GetDate() override;
    const char *GetLogTag() override;

    bool ConnectDatabase();
    void ApplyPlayerRank(int slot);
    int GetDisplayRankForLevel(int level) const;
    std::string Steam2FromAccountId(uint32 accountId) const;
    void RevealRanks(CPlayerBitVec &filter);

    std::string m_tableName = "lvl_base";
    int m_rankType = 12;
    int m_wins = 777;
    float m_refreshSeconds = 30.0f;
    bool m_revealOnScoreboard = true;
    bool m_connected = false;
    std::atomic_bool m_queryRunning = false;
    float m_nextRefresh = 0.0f;
    uint64_t m_oldButtons[64] = {};
    std::map<int, int> m_rankMap;
    std::map<std::string, int> m_playerLevels;
    std::mutex m_cacheMutex;
};
