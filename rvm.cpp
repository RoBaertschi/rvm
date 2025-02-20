#include "rvm.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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

size_t instruction_argument_amount(const InstructionKind& kind) {
    switch(kind) {
#define _X(kind, args) case InstructionKind::kind: return args;
    INSTRUCTION_KIND
#undef _X
        default:
            return 0;
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
        case InstructionKind::Jmp:
        case InstructionKind::JmpIf: {
            if (value == nullptr) {
                *error = new Error(ErrorKind::InvalidInstruction, strdup(std::format("{} requires an object as an argument, but found none", instruction_kind_string(kind)).c_str()), true);
                return;
            }
            if (value->kind != ObjectKind::U64) {
                *error = new Error(ErrorKind::InvalidInstruction, strdup(std::format("{} requires an object argument of type U64", instruction_kind_string(kind)).c_str()), true);
                return;
            }
            value->check(error);
            return;
        }
        case InstructionKind::Push: {
            if (value == nullptr) {
                *error = new Error(ErrorKind::InvalidInstruction, strdup(std::format("{} requires an object as an argument, but found none", instruction_kind_string(kind)).c_str()), true);
                return;
            }
            value->check(error);
            return;
        }
        case InstructionKind::Nop:
        case InstructionKind::JmpO:
        case InstructionKind::JmpIfO:
        case InstructionKind::Add:
        case InstructionKind::Sub: {
            if (value != nullptr) {
                *error = new Error(ErrorKind::InvalidInstruction, strdup(std::format("{} does not allow an object argument", instruction_kind_string(kind)).c_str()), true);
            }
            return;
        }
        case InstructionKind::Last:
            *error = new Error(ErrorKind::InvalidInstruction, strdup(std::format("invalid instruction kind {}", static_cast<std::underlying_type_t<InstructionKind>>(kind)).c_str()), true);
    }
}

Instruction::Instruction(InstructionKind kind) : kind(kind), value(nullptr) {}
Instruction::Instruction(InstructionKind kind, Object *value) : kind(kind), value(value) {}

Instruction::Instruction(Instruction const& rhs) {
    kind = rhs.kind; 
    if (rhs.value) {
        value = new Object(rhs.value->kind, rhs.value->data);
    } else {
        value = nullptr;
    }
}

Instruction& Instruction::operator=(Instruction const& rhs) {
    kind = rhs.kind; 
    if (rhs.value) {
        value = new Object(rhs.value->kind, rhs.value->data);
    }

    return *this;
}

Instruction::Instruction(Instruction &&rhs) noexcept {
    kind = std::move(rhs.kind);
    value = std::move(rhs.value);
    rhs.value = nullptr;
    rhs.kind = InstructionKind::Last; // Make it invalid
}

Instruction& Instruction::operator=(Instruction &&rhs) {
    if (this != &rhs) {
        if (value != nullptr) {
            delete value;
        }
        value = std::move(rhs.value);
        rhs.value = nullptr;
        rhs.kind = InstructionKind::Last; // Make it invalid
    }
    return *this;
}

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

Instruction::~Instruction() {
    if (value != nullptr) delete value;
}

char const* object_kind_string(const ObjectKind& kind) {
    switch(kind) {
#define _X(kind, args) case ObjectKind::kind: return #kind;
        OBJECT_KIND
#undef _X
        default:
            return "INVALID INSTRUCTION KIND";
    }
}

std::string Object::string() {
    switch (kind) {
    case ObjectKind::U64:
        return std::format("U64 {}", std::get<u64>(data));
    case ObjectKind::Pointer:
        return std::format("Pointer {}", std::get<u64>(data));
    case ObjectKind::Bool:
        return std::format("Bool {}", std::get<bool>(data));
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
    } else if(std::holds_alternative<bool>(data)) {
        auto data = std::get<bool>(this->data);
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

template <class T>
bool Object::holds(Error **error) {
    if (!std::holds_alternative<T>(data)) {
        *error = new Error(ErrorKind::InvalidObject, strdup(
            std::format("invalid object data, expected {}, got {}",
                        typeid(T).name(),
                        // NOTE: Extract to function if used twice
                        // https://stackoverflow.com/questions/53696720/get-currently-held-typeid-of-stdvariant-like-boostvariant-type
                        std::visit([](auto&& x)->decltype(auto){return typeid(x);}, data).name()
                        )
            .c_str()
        ), true);
        return false;
    }
    return true;
}

void Object::check(Error **error) {
    switch (kind) {
        case ObjectKind::U64:
        case ObjectKind::Pointer: {
            holds<u64>(error);
            break;
        }
        case ObjectKind::Bool: {
            holds<bool>(error);
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
    if (!std::holds_alternative<u64>(data)) {
        *error = new Error(ErrorKind::InvalidOperator, "operators not supported for Bool");
        return Object();
    }

    if (!std::holds_alternative<u64>(rhs.data)) {
        *error = new Error(ErrorKind::InvalidOperator, "operators not supported for Bool");
        return Object();
    }

    if (kind != rhs.kind) {
        *error = new Error(ErrorKind::InvalidOperator, "object are not of same type");
        return Object();
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
    if (file == NULL) {
        *error = new Error(ErrorKind::FileNotFound, strerror(errno));
        return {};
    }
    defer(fclose(file));
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
            case InstructionKind::Jmp:
            case InstructionKind::JmpIf:
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
            case InstructionKind::JmpO:
            case InstructionKind::JmpIfO:
            case InstructionKind::Add:
            case InstructionKind::Sub:
                instructions.push_back(Instruction(instruction));
                break;
            case InstructionKind::Last:
                break;
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
        case ObjectKind::Bool: {
            u8 obj_bool;
            read = fread(&obj_bool, sizeof(obj_bool), 1, file);
            if (read != 1) {
                goto error;
            }
            object = new Object(object_kind, obj_bool != 0);
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

Error::Error(ErrorKind kind) { Error(kind, "", false); }
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
    } catch(std::out_of_range) {
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
        case InstructionKind::Jmp: {
            if(instruction.value->kind != ObjectKind::U64) {
                *error = new Error(ErrorKind::InvalidInstructionArgument, strdup(std::format("the instruction object at {} is not a U64", pc).c_str()), true);
                return;
            }

            pc = std::get<u64>(instruction.value->data);
            break;
        }
        case InstructionKind::JmpIf: {
            auto cond = stack.pop();
            if(instruction.value->kind != ObjectKind::U64) {
                *error = new Error(ErrorKind::InvalidInstructionArgument, strdup(std::format("the instruction object at {} is not a U64", pc).c_str()), true);
                return;
            }

            if (cond.kind != ObjectKind::Bool) {
                *error = new Error(ErrorKind::InvalidInstructionArgument, strdup(std::format("the instruction object at {} is not a Bool", pc).c_str()), true);
                return;
            }

            if (std::get<bool>(cond.data)) {
                pc = std::get<u64>(instruction.value->data);
            }
            break;
        }
        case InstructionKind::JmpO: {
            auto address = stack.pop();

            if (address.kind != ObjectKind::U64) {
                *error = new Error(ErrorKind::InvalidInstructionArgument, strdup(std::format("the instruction object at {} is not a U64", pc).c_str()), true);
                return;
            }

            pc = std::get<u64>(address.data);
            break;
        }
        case InstructionKind::JmpIfO: {
            auto address = stack.pop();
            auto cond = stack.pop();

            if (address.kind != ObjectKind::U64) {
                *error = new Error(ErrorKind::InvalidInstructionArgument, strdup(std::format("the instruction object at {} is not a U64", pc).c_str()), true);
                return;
            }
            if (cond.kind != ObjectKind::Bool) {
                *error = new Error(ErrorKind::InvalidInstructionArgument, strdup(std::format("the instruction object at {} is not a Bool", pc).c_str()), true);
                return;
            }

            if (std::get<bool>(cond.data)) {
                pc = std::get<u64>(address.data);
            }
            break;
        }
        case InstructionKind::Last: {
            abort();
            break;
        }
    }
}

};

