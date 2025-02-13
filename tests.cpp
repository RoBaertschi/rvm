#include "rvm.hpp"
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <functional>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <format>

#define ESC "\033"
#define CSI ESC "["
#define COLOR(fg) CSI #fg "m"
#define RESET CSI "0m"

#define RED_FG   COLOR(31)
#define GREEN_FG COLOR(32)

struct Context {
    std::string current_test;

    FILE *temp_file(std::string_view to_write) {
        FILE *tmp = tmpfile();
        if (tmp == nullptr) return tmp;

        if (fwrite(to_write.data(), sizeof to_write.data()[0], to_write.length(), tmp) != to_write.length()) {
            fail(std::format("could not write to tmpfile because {}", strerror(errno)));
            fclose(tmp);
            return nullptr;
        }
        return tmp;
    }

    void begin(std::string test_name) {
        current_test = test_name;
    }

    void fail(std::string error) {
        std::cerr << RED_FG "FAIL " << current_test << ": " << error << "\n" RESET;
    }

    int end(int result) {
        if (result) {
            std::cerr << "TEST " << current_test << " " RED_FG "❌" RESET " " << result << std::endl;
        } else {
            std::cerr << "TEST " << current_test << " " GREEN_FG "✅" RESET << std::endl;
        }
        return result;
    }
};

int parse_bytecode_correctly(Context *ctx) {
    ctx->begin("parse_bytecode_correctly");

    std::vector<rvm::Instruction> instructions{
        rvm::InstructionKind::Nop,
        rvm::InstructionKind::Add,
        rvm::InstructionKind::Sub,
        { rvm::InstructionKind::Push, new rvm::Object{rvm::ObjectKind::U64, static_cast<rvm::u32>(1)} }
    };

    FILE *file = tmpfile();

    for (auto instruction : instructions) {
        rvm::Error *error;
        instruction.write(file, &error);
        if (error != nullptr) {
            ctx->fail(std::format("Failed to write instruction: {}", error->what()));
            return ctx->end(1);
        }
    }

    fclose(file);

    return ctx->end(0);
}

int main() {
    std::vector<std::function<int(Context*)>> tests{
        parse_bytecode_correctly
    };

    Context ctx{};

    for(auto test : tests) {
        int result = test(&ctx);
        if (result != 0) {
            return result;
        }
    }
}
