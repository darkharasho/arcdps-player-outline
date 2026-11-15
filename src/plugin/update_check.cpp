#include "update_check.hpp"

#include <windows.h>
#include <winhttp.h>
#include <cstdio>
#include <cstring>

#include "version.hpp"

namespace plugin {
namespace {

// Everything the worker thread touches lives in static storage, so nothing it
// uses can be freed under it.
SRWLOCK      g_lock = SRWLOCK_INIT;
UpdateStatus g_status;
bool         g_started = false;
char         g_current[64] = "";

void set_status(UpdateState state, const char* tag = "") {
    AcquireSRWLockExclusive(&g_lock);
    g_status.state = state;
    std::snprintf(g_status.tag, sizeof(g_status.tag), "%s", tag);
    ReleaseSRWLockExclusive(&g_lock);
}

// Fetches where /releases/latest redirects to. The website, not
// api.github.com: the API allows 60 unauthenticated calls an hour per IP,
// shared by every plugin on the connection.
bool fetch_latest_location(char* out, size_t out_size) {
    wchar_t agent[96];
    _snwprintf(agent, 96, L"arcdps_player_outline/%hs", g_current);
    agent[95] = L'\0';

    bool ok = false;
    HINTERNET session = WinHttpOpen(agent, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    HINTERNET connect = nullptr, request = nullptr;
    if (session) {
        WinHttpSetTimeouts(session, 10000, 10000, 10000, 10000);
        connect = WinHttpConnect(session, L"github.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
    }
    if (connect)
        request = WinHttpOpenRequest(connect, L"GET",
                                     L"/darkharasho/arcdps-player-outline/releases/latest",
                                     nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                     WINHTTP_FLAG_SECURE);
    if (request) {
        DWORD never = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
        wchar_t location[512];
        DWORD bytes = sizeof(location);
        if (WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY, &never, sizeof(never)) &&
            WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                               WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
            WinHttpReceiveResponse(request, nullptr) &&
            WinHttpQueryHeaders(request, WINHTTP_QUERY_LOCATION, WINHTTP_HEADER_NAME_BY_INDEX,
                                location, &bytes, WINHTTP_NO_HEADER_INDEX)) {
            ok = WideCharToMultiByte(CP_UTF8, 0, location, -1, out, (int)out_size,
                                     nullptr, nullptr) > 0;
        }
    }
    if (request) WinHttpCloseHandle(request);
    if (connect) WinHttpCloseHandle(connect);
    if (session) WinHttpCloseHandle(session);
    return ok;
}

DWORD WINAPI worker(LPVOID module) {
    char location[1024];
    char tag[32];
    if (fetch_latest_location(location, sizeof(location)) &&
        core::parse_release_tag(location, tag, sizeof(tag))) {
        if (core::is_newer(tag, g_current)) set_status(UpdateState::Available, tag);
        else                                set_status(UpdateState::UpToDate);
    } else {
        set_status(UpdateState::Failed);
    }
    // Drop the reference that kept this DLL loaded while the thread ran.
    if (module) FreeLibraryAndExitThread((HMODULE)module, 0);
    return 0;
}

}  // namespace

void start_update_check(const char* current_version) {
    AcquireSRWLockExclusive(&g_lock);
    const bool first = !g_started;
    g_started = true;
    ReleaseSRWLockExclusive(&g_lock);
    if (!first) return;

    std::snprintf(g_current, sizeof(g_current), "%s", current_version ? current_version : "");
    set_status(UpdateState::Checking);

    // Hold a reference to this DLL for the thread's lifetime: if the plugin is
    // unloaded mid-request, the thread's code must not be unmapped under it.
    HMODULE self = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCWSTR)&worker, &self);
    HANDLE th = CreateThread(nullptr, 0, worker, self, 0, nullptr);
    if (th) {
        CloseHandle(th);
    } else {
        if (self) FreeLibrary(self);
        set_status(UpdateState::Failed);
    }
}

UpdateStatus update_status() {
    AcquireSRWLockShared(&g_lock);
    UpdateStatus s = g_status;
    ReleaseSRWLockShared(&g_lock);
    return s;
}

}  // namespace plugin
