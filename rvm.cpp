#include "rvm.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <variant>
#include <vector>
#include <format>

namespace rvm {

char const* instruction_kind_string(const InstructionKind& kind) {
    switch(kind) {
#define _X(kind, args) case InstructionKind::kind: return #kind;
    INSTRUCTION_KIND
#undef _X
        default:
            return "INVALID INSTRUCTION KIND";
    }
}

Object *object_from_file(FILE *file, Error **error);

std::tuple<char const*, bool> error_concat(char const* prefix, char const* error_str) {
    size_t prefix_len = strlen(prefix);
    size_t error_str_len = strlen(error_str);
    char* new_str = static_cast<char*>(malloc(prefix_len + error_str_len + 1));
    if (new_str == nullptr) {
        return {error_str, false};
    }
    memcpy(new_str, prefix, prefix_len);
    memcpy(new_str + prefix_len, error_str, error_str_len);
    new_str[prefix_len + error_str_len] = '\0';
    return {new_str, true};
}

std::string Instruction::string() {
    return std::format("{} {}", instruction_kind_string(kind), value != nullptr ? value->string() : "<no args>");
}

void Instruction::check(Error **error) {
    switch(kind) {
        case InstructionKind::Push: {
            if (value == nullptr) {
                *error = new Error(ErrorKind::InvalidInstruction, "Push requires an object as an argument, but found none");
                return;
            }
            value->check(error);
            return;
        }
        case InstructionKind::Nop:
        case InstructionKind::Add:
        case InstructionKind::Sub: {
            if (value != nullptr) {
                *error = new Error(ErrorKind::InvalidInstruction, strdup(std::format("{} does not allow an object argument", instruction_kind_string(kind)).c_str()), true);
            }
            return;
        }
        default:
            *error = new Error(ErrorKind::InvalidInstruction, strdup(std::format("invalid instruction kind {}", static_cast<std::underlying_type_t<InstructionKind>>(kind)).c_str()), true);
    }
}

Instruction::Instruction(InstructionKind kind) : kind(kind), value(nullptr) {}
Instruction::Instruction(InstructionKind kind, Object *value) : kind(kind), value(value) {}

void Instruction::write(FILE *file, Error **error) {
    size_t write = fwrite(&kind, sizeof kind, 1, file);
    if (write != 1) {
        goto write_error;
    }

    if (value != nullptr) {
        value->write(file, error);
        if (*error != nullptr) {
            return;
        }
    }

    return;

write_error:
    if (feof(file)) {
        *error = new Error(ErrorKind::UnexpectedEOF, "EOF was encountered while reading a instruction");
        return;
    } else {
        *error = new Error(ErrorKind::FileError, error_concat("failed to read file: ", strerror(errno)));
        return;
    }
}

bool Instruction::operator==(const Instruction& other) const {
    return same(other);
};

bool Instruction::same(const Instruction& other) const {
    if (this == &other) return true;

    if (this->kind != other.kind) return false;
    if (this->value == nullptr) {
        return other.value == nullptr;
    }

    return this->value->same(*other.value);
}

std::string Object::string() {
    switch (kind) {
    case ObjectKind::U64:
        return std::format("U64 {}", std::get<u64>(data));
    case ObjectKind::Pointer:
        return std::format("Pointer {}", std::get<u64>(data));
    default:
        return "INVALID OBJECT KIND";
    }
}

void Object::write(FILE *file, Error **error) {
    size_t write = fwrite(&kind, sizeof kind, 1, file);
    if (write != 1) {
        goto write_error;
    }

    if (std::holds_alternative<u64>(data)) {
        auto data = std::get<u64>(this->data);
        write = fwrite(&data, sizeof data, 1, file);
        if (write != 1) {
            goto write_error;
        }
    }

    return;

write_error:
    if (feof(file)) {
        *error = new Error(ErrorKind::UnexpectedEOF, "EOF was encountered while reading a instruction");
        return;
    } else {
        *error = new Error(ErrorKind::FileError, error_concat("failed to read file: ", strerror(errno)));
        return;
    }
}

bool Object::operator==(const Object& other) const {
    return same(other);
}

bool Object::same(const Object& other) const {
    if (this == &other) return true;
    if (this->kind != other.kind) return false;
    if (this->data != other.data) return false;

    return true;
}

void Object::check(Error **error) {

    switch (kind) {
        case ObjectKind::U64:
        case ObjectKind::Pointer: {
            if (!std::holds_alternative<u64>(data)) {
                *error = new Error(ErrorKind::InvalidObject, strdup(
                    std::format("invalid object data, expected u64, got {}",
                                // NOTE: Extract to function if used twice
                                // https://stackoverflow.com/questions/53696720/get-currently-held-typeid-of-stdvariant-like-boostvariant-type
                                std::visit([](auto&& x)->decltype(auto){return typeid(x);}, data).name()
                                )
                    .c_str()
                ), true);
                return;
            }
            break;
        }
        default:
        case ObjectKind::Last: {
            *error = new Error(ErrorKind::InvalidObject, strdup(std::format("invalid object kind {}", static_cast<std::underlying_type_t<ObjectKind>>(kind)).c_str()), true);
            return;
        }
    }
}

Object Object::apply_operator(Operator op, Object rhs, Error **error) {
    if (kind != rhs.kind) {
        *error = new Error(ErrorKind::InvalidOperator, "object are not of same type");
    }

    u64 lhs_data = std::get<u64>(data);
    u64 rhs_data = std::get<u64>(rhs.data);

    if (op == Operator::Sub) {
        return Object(kind, lhs_data - rhs_data);
    } else {
        return Object(kind, lhs_data + rhs_data);
    }
}

std::vector<Instruction> bytecode_from_file(std::string_view filename, Error **error) {
    FILE* file = fopen(filename.data(), "r");
    if (file == nullptr) {
        *error = new Error(ErrorKind::FileNotFound, strerror(errno));
        return {};
    }
    return bytecode_from_file(file, error);
}


std::vector<Instruction> bytecode_from_file(FILE *file, Error **error) {
    std::vector<Instruction> instructions = {};

    while(!feof(file) && !ferror(file)) {
        u8 read_instruction = 0;
        size_t read = fread(&read_instruction, sizeof read_instruction, 1, file);
        if (read != 1) {
            if (feof(file)) break;
            goto error;
        }

        if (static_cast<InstructionKind>(read_instruction) >= InstructionKind::Last) {
            *error = new Error(ErrorKind::InvalidInstruction, strdup(std::format("Invalid instruction byte '{}'", read_instruction).c_str()), true);
            return instructions;
        }

        InstructionKind instruction = static_cast<InstructionKind>(read_instruction);

        switch (instruction) {
            case InstructionKind::Push: {
                // Read object
                auto obj = object_from_file(file, error);
                if (*error != nullptr) {
                    return instructions;
                }
                instructions.push_back(Instruction(instruction, obj));
                break;
            }
            case InstructionKind::Nop:
            case InstructionKind::Add:
            case InstructionKind::Sub:
                instructions.push_back(Instruction(instruction));
                break;
            case InstructionKind::Last:
                break;
        }
    }


    if (ferror(file)) {
        goto error;
    }

    *error = nullptr;
    return instructions;

error:
    if (feof(file)) {
        *error = new Error(ErrorKind::UnexpectedEOF, "EOF was encountered while reading a instruction");
        return instructions;
    } else {
        *error = new Error(ErrorKind::FileError, error_concat("failed to read file: ", strerror(errno)));
        return instructions;
    }
}

Object *object_from_file(FILE *file, Error **error) {
    u8 read_object = 0;
    ObjectKind object_kind;
    size_t read = fread(&read_object, sizeof read_object, 1, file);
    if (read != 1) {
        goto error;
    }

    if (static_cast<ObjectKind>(read_object) >= ObjectKind::Last) {
        *error = new Error(ErrorKind::InvalidInstruction, strdup(std::format("found unkown instruction '{}'", read_object).c_str()), true);
        return {};
    }

    object_kind = static_cast<ObjectKind>(read_object);
    Object *object;

    switch (object_kind) {
        case ObjectKind::Pointer:
        case ObjectKind::U64: {
            u64 obj_u64;
            read = fread(&obj_u64, sizeof(obj_u64), 1, file);
            if (read != 1) {
                goto error;
            }
            object = new Object(object_kind, obj_u64);
            break;
        }
        case ObjectKind::Last:
            std::abort();
    }

    *error = nullptr;
    return object;

error:
    if (feof(file)) {
        *error = new Error(ErrorKind::UnexpectedEOF, "EOF was encountered while reading a object");
        return {};
    } else {
        *error = new Error(ErrorKind::FileError, error_concat("failed to read file: ", strerror(errno)));
        return {};
    }
}

// ______
//|  ____|
//| |__   _ __ _ __ ___  _ __
//|  __| | '__| '__/ _ \| '__|
//| |____| |  | | | (_) | |
//|______|_|  |_|  \___/|_|

Error::Error(ErrorKind kind, char const *error_value, bool cleanup_error_value) : kind(kind), error_value(error_value), cleanup_error_value(cleanup_error_value) {}
Error::Error(ErrorKind kind, std::tuple<char const*, bool> error_value) :
    kind(kind),
    error_value(std::get<char const*>(error_value)),
    cleanup_error_value(std::get<bool>(error_value)) {}

Error::~Error() {
    if (cleanup_error_value) {
        free(const_cast<char*>(error_value));
    }
}

char const* Error::what() {
    return error_value;
}


// __      ____  __ 
// \ \    / /  \/  |
//  \ \  / /| \  / |
//   \ \/ / | |\/| |
//    \  /  | |  | |
//     \/   |_|  |_|

VM::VM(std::vector<Instruction> bytecode) : pc(0), stack({}), heap({}), bytecode(bytecode) {}

void VM::tick(Error **error) {
    Instruction instruction{InstructionKind::Last};

    try {
        instruction = bytecode.at(pc);
        pc += 1;
    } catch(std::out_of_range e) {
        *error = new Error(ErrorKind::NoMoreInstructions, strdup(std::format("no more instructions. pc={}", pc).c_str()), true);
        return;
    }
    instruction.check(error);
    if (*error != nullptr) {
        return;
    }

    switch (instruction.kind) {
        case InstructionKind::Push: {
            stack.push(*instruction.value);
            break;
        }

        case InstructionKind::Nop:
            break;
        case InstructionKind::Add: {
            auto rhs = stack.pop();
            auto lhs = stack.pop();

            auto result = lhs.apply_operator(Operator::Add, rhs, error);
            if (*error != nullptr) {
                return;
            }
            stack.push(result);
            break;
        }
        case InstructionKind::Sub: {
            auto rhs = stack.pop();
            auto lhs = stack.pop();

            auto result = lhs.apply_operator(Operator::Sub, rhs, error);
            if (*error != nullptr) {
                return;
            }
            stack.push(result);
            break;
        }
        case InstructionKind::Last:
        default:
            abort();
    }
}

};

