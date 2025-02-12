#include "rvm.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <tuple>
#include <vector>
#include <format>

namespace rvm {

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

    while(!feof(file) || !ferror(file)) {
        u8 read_instruction = 0;
        size_t read = fread(&read_instruction, sizeof read_instruction, 1, file);
        if (read != 1) {
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
                if (error != nullptr) {
                    return instructions;
                }
                instructions.push_back(Instruction(instruction, obj));
            }
        case InstructionKind::Nop:
        case InstructionKind::Add:
        case InstructionKind::Sub:
            instructions.push_back(Instruction(instruction));
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
            if (read != sizeof(obj_u64)) {
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
        *error = new Error(ErrorKind::UnexpectedEOF, "EOF was encountered while reading a instruction");
        return {};
    } else {
        *error = new Error(ErrorKind::FileError, error_concat("failed to read file: ", strerror(errno)));
        return {};
    }
}

Error::Error(ErrorKind kind, char const *error_value, bool cleanup_error_value) : kind(kind), error_value(error_value), cleanup_error_value(cleanup_error_value) {}
Error::Error(ErrorKind kind, std::tuple<char const*, bool> error_value) : kind(kind), error_value(std::get<char const*>(error_value)), cleanup_error_value(std::get<bool>(error_value)) {}

Error::~Error() {
    if (cleanup_error_value) {
        free(const_cast<char*>(error_value));
    }
}
};

