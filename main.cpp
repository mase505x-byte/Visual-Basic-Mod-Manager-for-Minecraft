#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#endif

namespace fs = std::filesystem;

struct ModFile {
    fs::path path;
    bool enabled;
};

std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool endsWithCaseInsensitive(const std::string& value, const std::string& suffix) {
    if (value.size() < suffix.size()) {
        return false;
    }
    return toLower(value.substr(value.size() - suffix.size())) == toLower(suffix);
}

std::vector<ModFile> scanMods(const fs::path& folder, std::error_code& error) {
    std::vector<ModFile> mods;
    fs::directory_iterator it(folder, error);
    const fs::directory_iterator end;

    while (!error && it != end) {
        std::error_code statusError;
        if (it->is_regular_file(statusError) && !statusError) {
            const std::string filename = it->path().filename().string();
            if (endsWithCaseInsensitive(filename, ".jar.disabled")) {
                mods.push_back({it->path(), false});
            } else if (endsWithCaseInsensitive(filename, ".jar")) {
                mods.push_back({it->path(), true});
            }
        }
        it.increment(error);
    }

    std::sort(mods.begin(), mods.end(), [](const ModFile& left, const ModFile& right) {
        return toLower(left.path.filename().string()) <
               toLower(right.path.filename().string());
    });
    return mods;
}

fs::path defaultModsFolder() {
    const char* appData = std::getenv("APPDATA");
    if (appData != nullptr && *appData != '\0') {
        return fs::path(appData) / ".minecraft" / "mods";
    }
    return {};
}

bool readMenuChoice(int& choice) {
    std::string line;
    std::cout << "Choose an option: ";
    if (!std::getline(std::cin, line)) {
        return false;
    }

    try {
        size_t consumed = 0;
        choice = std::stoi(line, &consumed);
        if (consumed != line.size()) {
            throw std::invalid_argument("not a whole number");
        }
        return true;
    } catch (const std::exception&) {
        std::cout << "Please enter a number from the menu.\n";
        return false;
    }
}

std::string readLine(const std::string& prompt) {
    std::string value;
    std::cout << prompt;
    std::getline(std::cin, value);
    return value;
}

fs::path readPath(const std::string& prompt) {
    std::string value = readLine(prompt);
    if (value.size() >= 2 &&
        ((value.front() == '"' && value.back() == '"') ||
         (value.front() == '\'' && value.back() == '\''))) {
        value = value.substr(1, value.size() - 2);
    }
    return fs::path(value);
}

bool getMods(const fs::path& folder, std::vector<ModFile>& mods) {
    std::error_code error;
    if (!fs::is_directory(folder, error) || error) {
        std::cout << "Mods folder is not available: " << folder.string()
                  << "\nUse option 8 to choose the correct folder.\n";
        return false;
    }

    mods = scanMods(folder, error);
    if (error) {
        std::cout << "Could not read the mods folder: " << error.message() << "\n";
        return false;
    }
    return true;
}

void listMods(const fs::path& folder) {
    std::vector<ModFile> mods;
    if (!getMods(folder, mods)) {
        return;
    }

    std::cout << "\nMods folder: " << folder.string() << "\n";
    if (mods.empty()) {
        std::cout << "No .jar mods found. Put mod .jar files in this folder.\n\n";
        return;
    }

    for (size_t i = 0; i < mods.size(); ++i) {
        std::string filename = mods[i].path.filename().string();
        if (!mods[i].enabled) {
            filename.resize(filename.size() - std::string(".disabled").size());
        }
        std::cout << (i + 1) << ". [" << (mods[i].enabled ? "Enabled" : "Disabled")
                  << "] " << filename << "\n";
    }
    std::cout << "\n";
}

bool readModNumber(size_t& number, size_t modCount) {
    const std::string input = readLine("Mod number: ");
    try {
        size_t consumed = 0;
        const unsigned long long parsed = std::stoull(input, &consumed);
        if (consumed != input.size() || parsed == 0 || parsed > modCount) {
            throw std::out_of_range("mod number");
        }
        number = static_cast<size_t>(parsed - 1);
        return true;
    } catch (const std::exception&) {
        std::cout << "Enter a number from 1 to " << modCount << ".\n";
        return false;
    }
}

void toggleMod(const fs::path& folder) {
    std::vector<ModFile> mods;
    if (!getMods(folder, mods)) {
        return;
    }
    if (mods.empty()) {
        std::cout << "No mod .jar files found.\n";
        return;
    }

    for (size_t i = 0; i < mods.size(); ++i) {
        std::cout << (i + 1) << ". [" << (mods[i].enabled ? "Enabled" : "Disabled")
                  << "] " << mods[i].path.filename().string() << "\n";
    }
    size_t selected = 0;
    if (!readModNumber(selected, mods.size())) {
        return;
    }

    const fs::path oldPath = mods[selected].path;
    const fs::path newPath = mods[selected].enabled
        ? fs::path(oldPath.string() + ".disabled")
        : fs::path(oldPath.string().substr(0, oldPath.string().size() -
                                           std::string(".disabled").size()));
    std::error_code error;
    if (fs::exists(newPath, error) || error) {
        std::cout << "Cannot change mod state because the destination already exists or "
                     "cannot be checked.\n";
        return;
    }
    fs::rename(oldPath, newPath, error);
    if (error) {
        std::cout << "Could not change mod state: " << error.message() << "\n";
        return;
    }
    std::cout << "Mod is now " << (mods[selected].enabled ? "disabled" : "enabled")
              << ": " << newPath.filename().string() << "\n";
}

void installMod(const fs::path& folder) {
    const fs::path source = readPath("Path to the mod .jar file: ");
    if (source.empty() || !endsWithCaseInsensitive(source.filename().string(), ".jar")) {
        std::cout << "Select a file with a .jar extension.\n";
        return;
    }

    std::error_code error;
    if (!fs::is_regular_file(source, error) || error) {
        std::cout << "Mod JAR does not exist or cannot be read: " << source.string() << "\n";
        return;
    }

    fs::create_directories(folder, error);
    if (error) {
        std::cout << "Could not create the mods folder: " << error.message() << "\n";
        return;
    }

    const fs::path destination = folder / source.filename();
    const fs::path disabledDestination = fs::path(destination.string() + ".disabled");
    if (fs::exists(destination, error) || error) {
        std::cout << "A file with that name is already in the mods folder.\n";
        return;
    }
    if (fs::exists(disabledDestination, error) || error) {
        std::cout << "A disabled mod with that name is already in the mods folder.\n";
        return;
    }

    fs::copy_file(source, destination, fs::copy_options::none, error);
    if (error) {
        std::cout << "Could not install the mod: " << error.message() << "\n";
        return;
    }
    std::cout << "Installed " << destination.filename().string() << "\n";
}

bool installModsFromFolder(const fs::path& modsFolder) {
    const fs::path sourceFolder = readPath("Folder containing downloaded mod .jar files: ");
    std::error_code error;
    if (sourceFolder.empty() || !fs::is_directory(sourceFolder, error) || error) {
        std::cout << "Selected source folder is not accessible.\n";
        return false;
    }

    fs::create_directories(modsFolder, error);
    if (error) {
        std::cout << "Could not create the Minecraft mods folder: "
                  << error.message() << "\n";
        return false;
    }

    size_t found = 0;
    size_t installed = 0;
    size_t skipped = 0;
    bool success = true;
    fs::directory_iterator it(sourceFolder, error);
    const fs::directory_iterator end;
    while (!error && it != end) {
        std::error_code statusError;
        if (it->is_regular_file(statusError) && !statusError &&
            endsWithCaseInsensitive(it->path().filename().string(), ".jar")) {
            ++found;
            const fs::path destination = modsFolder / it->path().filename();
            std::error_code copyError;
            if (fs::exists(destination, copyError) || copyError) {
                std::cout << "Skipped " << it->path().filename().string()
                          << ": a file with that name already exists.\n";
                ++skipped;
            } else {
                fs::copy_file(it->path(), destination, fs::copy_options::none, copyError);
                if (copyError) {
                    std::cout << "Could not install " << it->path().filename().string()
                              << ": " << copyError.message() << "\n";
                    success = false;
                } else {
                    ++installed;
                }
            }
        } else if (statusError) {
            std::cout << "Could not read an item in the source folder: "
                      << statusError.message() << "\n";
            success = false;
        }
        it.increment(error);
    }

    if (error) {
        std::cout << "Could not finish reading the source folder: "
                  << error.message() << "\n";
        success = false;
    }
    if (found == 0) {
        std::cout << "No .jar files found directly in the selected folder.\n";
        return false;
    }

    std::cout << "Found " << found << " JAR(s); installed " << installed
              << ", skipped " << skipped << ".\n";
    return success;
}

void removeMod(const fs::path& folder) {
    std::vector<ModFile> mods;
    if (!getMods(folder, mods)) {
        return;
    }
    if (mods.empty()) {
        std::cout << "No mod .jar files found.\n";
        return;
    }

    for (size_t i = 0; i < mods.size(); ++i) {
        std::cout << (i + 1) << ". " << mods[i].path.filename().string() << "\n";
    }
    size_t selected = 0;
    if (!readModNumber(selected, mods.size())) {
        return;
    }

    const std::string confirmation =
        readLine("Permanently delete this mod file? Type yes to confirm: ");
    if (toLower(confirmation) != "yes") {
        std::cout << "Removal cancelled.\n";
        return;
    }

    std::error_code error;
    if (!fs::remove(mods[selected].path, error) || error) {
        std::cout << "Could not remove the mod: "
                  << (error ? error.message() : "file was not removed") << "\n";
        return;
    }
    std::cout << "Removed " << mods[selected].path.filename().string() << "\n";
}

bool setAllModsEnabled(const fs::path& folder, bool enabled) {
    std::error_code error;
    if (!fs::exists(folder, error)) {
        if (error) {
            std::cout << "Could not check the mods folder: " << error.message() << "\n";
            return false;
        }
        if (enabled) {
            std::cout << "Mods folder not found: " << folder.string()
                      << "\nChoose the correct folder before running Modded.\n";
            return false;
        }
        std::cout << "No mods folder found; Vanilla can run without mods.\n";
        return true;
    }
    if (!fs::is_directory(folder, error) || error) {
        std::cout << "Mods folder is not accessible: " << folder.string() << "\n";
        return false;
    }

    std::vector<ModFile> mods = scanMods(folder, error);
    if (error) {
        std::cout << "Could not read the mods folder: " << error.message() << "\n";
        return false;
    }

    size_t changed = 0;
    bool success = true;
    for (const ModFile& mod : mods) {
        if (mod.enabled == enabled) {
            continue;
        }

        const fs::path destination = enabled
            ? fs::path(mod.path.string().substr(
                  0, mod.path.string().size() - std::string(".disabled").size()))
            : fs::path(mod.path.string() + ".disabled");
        error.clear();
        if (fs::exists(destination, error) || error) {
            std::cout << "Could not change " << mod.path.filename().string()
                      << ": destination already exists or cannot be checked.\n";
            success = false;
            continue;
        }
        fs::rename(mod.path, destination, error);
        if (error) {
            std::cout << "Could not change " << mod.path.filename().string()
                      << ": " << error.message() << "\n";
            success = false;
            continue;
        }
        ++changed;
    }

    if (!success) {
        std::cout << "Some mods could not be changed. The Launcher was not opened; "
                     "check the folder and resolve the errors above.\n";
        return false;
    }
    std::cout << (enabled ? "Enabled " : "Disabled ") << changed
              << " mod(s) in " << folder.string() << ".\n";
    return true;
}

bool listEnabledMods(const fs::path& folder) {
    std::error_code error;
    const std::vector<ModFile> mods = scanMods(folder, error);
    if (error) {
        std::cout << "Mods were enabled, but the mod list could not be read: "
                  << error.message() << "\n";
        return false;
    }

    std::cout << "\nMods enabled for NeoForge:\n";
    size_t count = 0;
    for (const ModFile& mod : mods) {
        if (mod.enabled) {
            std::cout << "- " << mod.path.filename().string() << "\n";
            ++count;
        }
    }
    if (count == 0) {
        std::cout << "(none)\n";
    }
    std::cout << "\n";
    return true;
}

bool openMinecraftLauncher() {
#ifdef _WIN32
    const INT_PTR result = reinterpret_cast<INT_PTR>(
        ShellExecuteW(nullptr, L"open",
                     L"shell:AppsFolder\\Microsoft.4297127D64EC6_8wekyb3d8bbwe!Minecraft",
                     nullptr, nullptr, SW_SHOWNORMAL));
    if (result <= 32) {
        std::cout << "Windows could not open the Minecraft Launcher. Start it from "
                     "the Start menu and choose Java Edition.\n";
        return false;
    }
    std::cout << "Opened Minecraft Launcher. Choose your NeoForge installation and click Play.\n";
    return true;
#else
    std::cout << "Opening the Minecraft Launcher is supported on Windows. Start it "
                 "from the Start menu and choose Java Edition.\n";
    return false;
#endif
}

void runMinecraft(const fs::path& folder, bool withMods) {
    if (!setAllModsEnabled(folder, withMods)) {
        return;
    }
    if (withMods && !listEnabledMods(folder)) {
        return;
    }

    const std::string answer = readLine(
        "Open Minecraft Launcher now? Type yes to open it: ");
    if (toLower(answer) == "yes") {
        openMinecraftLauncher();
    } else {
        std::cout << "Open Minecraft Launcher, select your "
                  << (withMods ? "NeoForge" : "Vanilla Java")
                  << " installation, and click Play.\n";
    }
}

void installAndRunModded(const fs::path& folder) {
    if (!installModsFromFolder(folder)) {
        std::cout << "Mods were not launched. Fix the folder or copy errors and try again.\n";
        return;
    }
    if (!setAllModsEnabled(folder, true)) {
        return;
    }
    if (!listEnabledMods(folder)) {
        return;
    }
    const std::string answer = readLine(
        "Open Minecraft Launcher now? Type yes to open it: ");
    if (toLower(answer) == "yes") {
        openMinecraftLauncher();
    } else {
        std::cout << "Mods are installed and enabled. Open Minecraft Launcher, "
                     "select your NeoForge installation, and click Play.\n";
    }
}

void printMenu(const fs::path& folder) {
    std::cout << "\n=== Minecraft Java Mod Manager ===\n"
              << "Mods folder: " << folder.string() << "\n"
              << "1. List/scan mods\n"
              << "2. Enable/disable a mod\n"
              << "3. Install a mod .jar\n"
              << "4. Remove a mod\n"
              << "5. Install folder of mods and open Modded Java\n"
              << "6. Run Vanilla Java (disable all mods)\n"
              << "7. Run Modded Java (enable all mods)\n"
              << "8. Choose mods folder\n"
              << "9. Exit\n"
              << "==============================\n";
}

int main() {
    fs::path modsFolder = defaultModsFolder();
    if (modsFolder.empty()) {
        std::cout << "Could not find the Windows AppData folder.\n";
        modsFolder = readLine("Enter your Minecraft mods folder path: ");
        if (modsFolder.empty()) {
            std::cout << "No folder selected. Exiting.\n";
            return 1;
        }
    }

    std::cout << "Minecraft Java Mod Manager\n"
              << "Default Minecraft mods folder: " << modsFolder.string() << "\n"
              << "This manager handles Java Edition .jar mods, not Bedrock add-ons.\n"
              << "For a different Minecraft installation, choose option 8.\n";

    while (true) {
        printMenu(modsFolder);
        int choice = 0;
        if (!readMenuChoice(choice)) {
            if (!std::cin) {
                std::cout << "\nInput closed. Exiting.\n";
                return 0;
            }
            continue;
        }

        switch (choice) {
            case 1:
                listMods(modsFolder);
                break;
            case 2:
                toggleMod(modsFolder);
                break;
            case 3:
                installMod(modsFolder);
                break;
            case 4:
                removeMod(modsFolder);
                break;
            case 5: {
                installAndRunModded(modsFolder);
                break;
            }
            case 6: {
                runMinecraft(modsFolder, false);
                break;
            }
            case 7: {
                runMinecraft(modsFolder, true);
                break;
            }
            case 8: {
                const fs::path selectedFolder = readPath("Enter the Minecraft mods folder path: ");
                if (selectedFolder.empty()) {
                    std::cout << "Folder path cannot be empty.\n";
                    break;
                }
                modsFolder = selectedFolder;
                std::cout << "Using mods folder: " << modsFolder.string() << "\n";
                break;
            }
            case 9:
                std::cout << "Goodbye!\n";
                return 0;
            default:
                std::cout << "Invalid option. Please choose 1 to 9.\n";
                break;
        }
    }
}
