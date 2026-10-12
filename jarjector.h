#pragma once

#include <iostream>
#include <string>
#include <utility>

class JarJector {
public:
    explicit JarJector(std::string path)
        : path_(std::move(path)) {}

    bool inject(const std::string& target) const {
        std::cout << "Injecting jar: " << path_
                  << " into: " << target << '\n';
        return true;
    }

private:
    std::string path_;
};

inline int run_example() {
    JarJector injector("C://___/___/___/(example).jar");
    if (injector.inject("minecraft.exe")) {
        std::cout << "FabricLoader26.3 loaded successfully.\n";
        return 0;
    }

    return 1;
}