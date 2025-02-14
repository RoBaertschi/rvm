#include "rvm.hpp"
#include <cstdio>
#include <iostream>
#include <vector>

int main(int argc, char **argv) {
    std::vector<char*> args = std::vector<char*>(argv, argv + argc);

    if (args.size() < 2) {
        std::cerr << "ERROR: rvm requires at least 1 argument\n";
        return 1;
    }

    FILE *file = fopen(args[1], "r");
    defer(fclose(file));

    rvm::Error *error = nullptr;
    auto bytecode = rvm::bytecode_from_file(file, &error);

    if (error != nullptr) {
        std::cerr << "ERROR: " << error->what();
        return 2;
    }

    for (auto instruction : bytecode) {
        std::cout << instruction.string() << "\n";
    }
}
