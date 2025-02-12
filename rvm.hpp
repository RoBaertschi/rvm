#pragma once

#include <cstdint>
#include <stack>
#include <variant>
#include <vector>

namespace rvm {
using u8 = uint8_t ;
using u64 = uint64_t ;

class Object;
class VM;
using Stack = std::stack<Object>;
using Heap = std::vector<Object>;

enum class InstructionType: u8 {
    Nop,  // No operation, do nothing
    Push, // Push the following object onto the stack

    // Pops 2 values from the stack and returns the result
    Add,
    Sub,

    Last,
};

static_assert(InstructionType::Last <= static_cast<InstructionType>((1 << 7)), "The last bit is reserved for multi byte instructions");

struct Instruction {
    InstructionType type;
    Object         *value;
};

enum class ObjectType: u8 {
    U64,
    Pointer,

    Last,
};

class Object {
public:
    ObjectType        type;
    std::variant<u64> data;
};

class VM {
public:
    u64   pc;
    Stack stack;
    Heap  heap;
};
}


