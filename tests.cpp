#include "rvm.hpp"
#include <cerrno>
#include <csetjmp>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <exception>
#include <functional>
#include <iostream>
#include <ostream>
#include <string>
#include <string_view>
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

    void fail(std::string_view error) {
        std::cerr << RED_FG "FAIL " << current_test << ": " << error << "\n" RESET;
    }

    // Frees the error if it is not nullptr and also fails with some prefix
    int handle_error(rvm::Error *error, std::string_view prefix = "") {
        if (error != nullptr) {
            fail(std::format("{}{}", prefix, error->what()));
            delete error;
            return 1;
        }

        return 0;
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

#define HANDLE_ERROR(error, prefix) do {            \
    int result = ctx->handle_error(error, prefix);  \
    if (result != 0) {                              \
        return ctx->end(result);                    \
    }                                               \
} while(0)

#define ASSERT(expr) if (!(expr)) {\
    ctx->fail(std::format("assertion failed: ({})", #expr));\
    return ctx->end(1);\
}

int parse_bytecode_correctly(Context *ctx) {
    ctx->begin("parse_bytecode_correctly");

    std::vector<rvm::Instruction> instructions{
        rvm::InstructionKind::Nop,
        rvm::InstructionKind::Add,
        rvm::InstructionKind::Sub,
        { rvm::InstructionKind::Push, new rvm::Object{rvm::ObjectKind::U64, static_cast<rvm::u32>(1)} }
    };

    FILE *file = tmpfile();
    defer(fclose(file));

    rvm::Error *error = nullptr;
    for (auto instruction : instructions) {
        instruction.write(file, &error);
        HANDLE_ERROR(error, "Failed to write instruction: ");
    }

    fseek(file, 0, SEEK_SET);

    auto bytecode = rvm::bytecode_from_file(file, &error);
    HANDLE_ERROR(error, "error while parsing bytecode: ");

    ASSERT(bytecode == instructions);

    return ctx->end(0);
}

std::jmp_buf signal_jmp_buf;

volatile std::sig_atomic_t signal_status = 0;

extern "C" void signal_handler(int sig) {
    signal_status = sig;
    longjmp(signal_jmp_buf, sig);
}

int main() {
    Context ctx{};
    if (setjmp(signal_jmp_buf) != 0) {
        std::cerr << RED_FG "Encountered signal " << signal_status << " while running test " << ctx.current_test << RESET << std::endl;
        return -1;
    }
    signal(SIGSEGV, signal_handler);

    std::vector<std::function<int(Context*)>> tests{
        parse_bytecode_correctly
    };

    for(auto test : tests) {
        try {
            int result = test(&ctx);
            if (result != 0) {
                return result;
            }
        } catch(std::exception e) {
            ctx.fail(std::format("exception thrown while running test {}: {}", ctx.current_test, e.what()));
        }
    }
}
