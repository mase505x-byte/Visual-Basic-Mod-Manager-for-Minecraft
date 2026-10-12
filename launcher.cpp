#define UNICODE
#define _UNICODE
#include <windows.h> 
#include <commdlg.h>
#include <shlobj.h>
#include <shellapi.h>
#include <cwchar>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

enum : int {
    ID_GAME_DIR = 101, ID_BROWSE_GAME_DIR, ID_LAUNCHER_PATH,
    ID_BROWSE_LAUNCHER, ID_VERSION, ID_REFRESH, ID_LAUNCH,
    ID_OPEN_MODS, ID_ADD_MODS
};

static HWND g_gameDir, g_launcherPath, g_version, g_status;

static std::wstring ControlText(HWND control) {
    int length = GetWindowTextLengthW(control);
    std::wstring value(static_cast<size_t>(length) + 1, L'\0');
    GetWindowTextW(control, value.data(), length + 1);
    value.resize(static_cast<size_t>(length));
    return value;
}

static void SetStatus(const std::wstring& text) {
    SetWindowTextW(g_status, text.c_str());
}

static HWND MakeButton(HWND parent, const wchar_t* text, int id,
                       int x, int y, int width, int height) {
    return CreateWindowW(L"BUTTON", text, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x, y, width, height, parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);
}

static HWND MakeEdit(HWND parent, int id, int x, int y, int width) {
    return CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, x, y, width, 25, parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);
}

static std::wstring BrowseFolder(HWND owner) {
    BROWSEINFOW info{};
    info.hwndOwner = owner;
    info.lpszTitle = L"Select your Minecraft game folder";
    info.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    PIDLIST_ABSOLUTE item = SHBrowseForFolderW(&info);
    if (!item) return {};
    wchar_t path[MAX_PATH]{};
    std::wstring result;
    if (SHGetPathFromIDListW(item, path)) result = path;
    CoTaskMemFree(item);
    return result;
}

static void RefreshVersions() {
    SendMessageW(g_version, CB_RESETCONTENT, 0, 0);
    try {
        const fs::path versions = fs::path(ControlText(g_gameDir)) / L"versions";
        if (fs::is_directory(versions)) {
            for (const auto& entry : fs::directory_iterator(versions)) {
                if (entry.is_directory()) {
                    const std::wstring name = entry.path().filename().wstring();
                    SendMessageW(g_version, CB_ADDSTRING, 0,
                                 reinterpret_cast<LPARAM>(name.c_str()));
                }
            }
        }
    } catch (const fs::filesystem_error&) {
        // An inaccessible or incomplete game directory simply has no listed versions.
    }
    if (SendMessageW(g_version, CB_GETCOUNT, 0, 0) > 0)
        SendMessageW(g_version, CB_SETCURSEL, 0, 0);
    SetStatus(L"Choose an installed version. Use the official launcher to sign in and play.");
}

static std::wstring FindOfficialLauncher() {
    wchar_t base[MAX_PATH]{};
    DWORD size = GetEnvironmentVariableW(L"ProgramFiles(x86)", base, MAX_PATH);
    if (size && size < MAX_PATH) {
        fs::path candidate = fs::path(base) / L"Minecraft Launcher" / L"MinecraftLauncher.exe";
        if (fs::is_regular_file(candidate)) return candidate.wstring();
    }
    size = GetEnvironmentVariableW(L"LOCALAPPDATA", base, MAX_PATH);
    if (size && size < MAX_PATH) {
        fs::path candidate = fs::path(base) / L"Programs" / L"Minecraft Launcher" / L"MinecraftLauncher.exe";
        if (fs::is_regular_file(candidate)) return candidate.wstring();
    }
    return {};
}

static void ChooseLauncher(HWND owner) {
    wchar_t path[MAX_PATH]{};
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = owner;
    dialog.lpstrFilter = L"Applications (*.exe)\0*.exe\0All files (*.*)\0*.*\0\0";
    dialog.lpstrFile = path;
    dialog.nMaxFile = MAX_PATH;
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (GetOpenFileNameW(&dialog)) SetWindowTextW(g_launcherPath, path);
}

static void LaunchOfficialLauncher(HWND owner) {
    const std::wstring executable = ControlText(g_launcherPath);
    if (executable.empty() || !fs::is_regular_file(executable)) {
        MessageBoxW(owner, L"Select the official Minecraft Launcher executable first.",
                    L"Launcher not found", MB_OK | MB_ICONWARNING);
        return;
    }
    const HINSTANCE result = ShellExecuteW(owner, L"open", executable.c_str(),
                                           nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(result) <= 32) {
        SetStatus(L"Could not start Minecraft Launcher. Check the executable path.");
        return;
    }
    SetStatus(L"Official Minecraft Launcher started. Sign in there to play the selected installation.");
}

static fs::path ModsDirectory() {
    return fs::path(ControlText(g_gameDir)) / L"mods";
}

static void OpenMods(HWND owner) {
    const fs::path mods = ModsDirectory();
    std::error_code error;
    fs::create_directories(mods, error);
    if (error) {
        SetStatus(L"Could not create the mods folder. Check the game folder path.");
        return;
    }
    ShellExecuteW(owner, L"open", mods.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

static void AddMods(HWND owner) {
    std::vector<wchar_t> buffer(32768, L'\0');
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = owner;
    dialog.lpstrFilter = L"Minecraft mod files (*.jar)\0*.jar\0All files (*.*)\0*.*\0\0";
    dialog.lpstrFile = buffer.data();
    dialog.nMaxFile = static_cast<DWORD>(buffer.size());
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_ALLOWMULTISELECT | OFN_EXPLORER;
    if (!GetOpenFileNameW(&dialog)) return;

    std::vector<fs::path> files;
    wchar_t* first = buffer.data();
    wchar_t* next = first + std::wcslen(first) + 1;
    if (*next == L'\0') {
        files.emplace_back(first);
    } else {
        const fs::path folder(first);
        while (*next != L'\0') {
            files.push_back(folder / next);
            next += std::wcslen(next) + 1;
        }
    }

    const fs::path destination = ModsDirectory();
    std::error_code error;
    fs::create_directories(destination, error);
    if (error) {
        SetStatus(L"Could not create the mods folder. Check the game folder path.");
        return;
    }
    size_t copied = 0;
    for (const auto& file : files) {
        error.clear();
        fs::copy_file(file, destination / file.filename(),
                      fs::copy_options::overwrite_existing, error);
        if (!error) ++copied;
    }
    SetStatus(L"Copied " + std::to_wstring(copied) + L" mod file(s) to the mods folder.");
}

static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_CREATE) {
        CreateWindowW(L"STATIC", L"Minecraft game folder", WS_CHILD | WS_VISIBLE,
                      18, 16, 250, 20, window, nullptr, nullptr, nullptr);
        g_gameDir = MakeEdit(window, ID_GAME_DIR, 18, 38, 465);
        MakeButton(window, L"Browse...", ID_BROWSE_GAME_DIR, 493, 38, 90, 25);

        CreateWindowW(L"STATIC", L"Official Minecraft Launcher", WS_CHILD | WS_VISIBLE,
                      18, 76, 250, 20, window, nullptr, nullptr, nullptr);
        g_launcherPath = MakeEdit(window, ID_LAUNCHER_PATH, 18, 98, 465);
        MakeButton(window, L"Browse...", ID_BROWSE_LAUNCHER, 493, 98, 90, 25);

        CreateWindowW(L"STATIC", L"Installed versions in this game folder", WS_CHILD | WS_VISIBLE,
                      18, 137, 300, 20, window, nullptr, nullptr, nullptr);
        g_version = CreateWindowW(L"COMBOBOX", L"",
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
            18, 160, 400, 240, window,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_VERSION)), nullptr, nullptr);
        MakeButton(window, L"Refresh", ID_REFRESH, 430, 160, 90, 26);

        MakeButton(window, L"Launch Minecraft", ID_LAUNCH, 18, 210, 175, 36);
        MakeButton(window, L"Open mods folder", ID_OPEN_MODS, 205, 210, 170, 36);
        MakeButton(window, L"Add mod files...", ID_ADD_MODS, 387, 210, 170, 36);
        g_status = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE,
            18, 266, 565, 48, window, nullptr, nullptr, nullptr);

        wchar_t appData[MAX_PATH]{};
        const DWORD length = GetEnvironmentVariableW(L"APPDATA", appData, MAX_PATH);
        if (length && length < MAX_PATH)
            SetWindowTextW(g_gameDir, (fs::path(appData) / L".minecraft").c_str());
        const std::wstring launcher = FindOfficialLauncher();
        if (!launcher.empty()) SetWindowTextW(g_launcherPath, launcher.c_str());
        RefreshVersions();
        return 0;
    }
    if (message == WM_COMMAND) {
        switch (LOWORD(wParam)) {
        case ID_BROWSE_GAME_DIR: {
            const std::wstring folder = BrowseFolder(window);
            if (!folder.empty()) {
                SetWindowTextW(g_gameDir, folder.c_str());
                RefreshVersions();
            }
            return 0;
        }
        case ID_BROWSE_LAUNCHER: ChooseLauncher(window); return 0;
        case ID_REFRESH: RefreshVersions(); return 0;
        case ID_LAUNCH: LaunchOfficialLauncher(window); return 0;
        case ID_OPEN_MODS: OpenMods(window); return 0;
        case ID_ADD_MODS: AddMods(window); return 0;
        }
    }
    if (message == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    const HRESULT comInit = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool comInitialized = SUCCEEDED(comInit) || comInit == RPC_E_CHANGED_MODE;

    const wchar_t className[] = L"VbCraftModManagerLauncher";
    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.lpszClassName = className;
    if (!RegisterClassW(&windowClass)) {
        if (comInitialized) CoUninitialize();
        return 1;
    }

    HWND window = CreateWindowExW(0, className,
        L"VbCraft ModManager - Minecraft Launcher",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 620, 365,
        nullptr, nullptr, instance, nullptr);
    if (!window) {
        if (comInitialized) CoUninitialize();
        return 1;
    }
    ShowWindow(window, showCommand);
    UpdateWindow(window);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    if (comInitialized) CoUninitialize();
    return static_cast<int>(message.wParam);
}
