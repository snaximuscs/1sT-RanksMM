#pragma once

#include <functional>
#include <string>
#include <vector>

class CBaseEntity;
class CBaseModelEntity;
class CEntityInstance;
class CEntityKeyValues;
class CSteamID;
class CGameEntitySystem;
class CEntitySystem;
class CGlobalVars;
class IGameEvent;
class IGameEventManager2;
class CCSGameRules;

#define Utils_INTERFACE "IUtilsApi"

typedef std::function<void()> StartupCallback;

class IUtilsApi
{
public:
    virtual void PrintToChat(int iSlot, const char* msg, ...) = 0;
    virtual void PrintToChatAll(const char* msg, ...) = 0;
    virtual void NextFrame(std::function<void()> fn) = 0;
    virtual CCSGameRules* GetCCSGameRules() = 0;
    virtual CGameEntitySystem* GetCGameEntitySystem() = 0;
    virtual CEntitySystem* GetCEntitySystem() = 0;
    virtual CGlobalVars* GetCGlobalVars() = 0;
    virtual IGameEventManager2* GetGameEventManager() = 0;
    virtual const char* GetLanguage() = 0;
    virtual void StartupServer(SourceMM::PluginId id, StartupCallback fn) = 0;
    virtual void OnGetGameRules(SourceMM::PluginId id, StartupCallback fn) = 0;
    virtual void RegCommand(SourceMM::PluginId id, const std::vector<std::string> &console, const std::vector<std::string> &chat, const std::function<bool(int, const char*)> &callback) = 0;
    virtual void AddChatListenerPre(SourceMM::PluginId id, std::function<bool(int, const char*, bool)> callback) = 0;
    virtual void AddChatListenerPost(SourceMM::PluginId id, std::function<bool(int, const char*, bool, bool)> callback) = 0;
    virtual void HookEvent(SourceMM::PluginId id, const char* sName, std::function<void(const char*, IGameEvent*, bool)> callback) = 0;
    virtual void SetStateChanged(CBaseEntity* entity, const char* sClassName, const char* sFieldName, int extraOffset = 0) = 0;
    virtual void ClearAllHooks(SourceMM::PluginId id) = 0;
};
