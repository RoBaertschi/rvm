#pragma once

#include <cstdint>
#include <cstdio>
#include <stack>
#include <string_view>
#include <tuple>
#include <variant>
#include <vector>

namespace rvm {
using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

class Error;
class Object;
class VM;
using Stack = std::stack<Object>;
using Heap = std::vector<Object>;

#define INSTRUCTION_KIND                                        \
    _X(Nop,  0) /* No operation, do nothing */                  \
    _X(Push, 1) /* Push the following object onto the stack */  \
    /* Pops 2 values from the stack and returns the result */   \
    _X(Add, 0)                                                  \
    _X(Sub, 0)

enum class InstructionKind: u8 {
#define _X(kind, ...) kind,
    INSTRUCTION_KIND
#undef _X

    Last,
};

char const* instruction_string(const InstructionKind& kind);

static_assert(InstructionKind::Last <= static_cast<InstructionKind>((1 << 7)), "The last bit is reserved for multi byte instructions");

struct Instruction {
    InstructionKind kind;
    // NOTE: Nullable
    Object         *value;

    Instruction(InstructionKind kind);
    Instruction(InstructionKind kind, Object *value);

    // Does not check if this is a valid instruction
    // FIXME(robin): Add validation of instruction, optionally or required
    void write(FILE *file, Error **error);

    bool same(const Instruction& other) const;
    bool operator==(const Instruction& other) const;

    std::string string();
};

enum class ObjectKind: u8 {
    U64,
    Pointer,

    Last,
};

class Object {
public:
    ObjectKind        kind;
    std::variant<u64> data;

    // Does not check if this is a valid instruction
    // FIXME(robin): Add validation of instruction, optionally or required
    void write(FILE *file, Error **error);

    bool same(const Object& other) const;
    bool operator==(const Object& other) const;

    std::string string();
};

std::vector<Instruction> bytecode_from_file(std::string_view filename, Error **error);
std::vector<Instruction> bytecode_from_file(FILE *file, Error **error);

class VM {
public:
    u64                      pc;
    Stack                    stack;
    Heap                     heap;
    std::vector<Instruction> bytecode;
};

enum class ErrorKind {
    InvalidObject,
    InvalidInstruction,

    FileNotFound,
    FileError,
    UnexpectedEOF
};

class Error {
public:
    Error(ErrorKind kind, char const *error_value = "", bool cleanup_error_value = false);
    Error(ErrorKind kind, std::tuple<char const*, bool> error_value = {"", false});
    ErrorKind kind;
    char const *error_value = "";
    bool cleanup_error_value = false;
    char const *what();
    ~Error();
};

// NOTE: Internal stuff, not public API and not stable

#define STRING_JOIN2(arg1, arg2) DO_STRING_JOIN2(arg1, arg2)
#define DO_STRING_JOIN2(arg1, arg2) arg1 ## arg2
#define defer(...) \
    auto STRING_JOIN2(_defer_, __LINE__) = rvm::internal::make_defer([=](){__VA_ARGS__;})

namespace internal {
template <class F>
struct Defer {
    F f;
    Defer(F f) : f(f) {}
    ~Defer() { f(); }
};

template <class F>
Defer<F> make_defer(F f) {
    return Defer(f);
}

}
}
