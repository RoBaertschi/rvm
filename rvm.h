// ml2 ft=c
#pragma once

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

typedef uint64_t u64;
typedef uint8_t  u8;

#define RVM_BIT(num) 1 << num
#define UNREACHABLE() abort()
#define ASSERT(expr) assert(expr)


// The first byte of an instruction is reserved, for further extension
typedef enum rvm_instruction_type {
    RVMI_NOP, // No operation, do nothing
    RVMI_PUSH, // Push the following object onto the stack

    // Pops 2 values from the stack and returns the result
    RVMI_ADD,
    RVMI_SUB,

    RVMI_LAST,
} rvm_instruction_type;


_Static_assert(RVMI_LAST <= (1 << 7), "The last bit is reserved for multi byte instructions");

typedef struct rvm_instruction {
    rvm_instruction_type type;
    // Can, and will be mostly NULL
    struct rvm_object   *value;
} rvm_instruction;

// This is the currently loaded bytecode
// Every instructions has an address, the address is the index into the instructions array
typedef struct rvm_bytecode {
    rvm_instruction *instructions;
    u64 size;
    u64 cap;
} rvm_bytecode;

struct rvm_object;

// The Stack is a list of objects. It grows upwards.
typedef struct rvm_stack {
    struct rvm_object *objects;
    u64         size;
    u64         cap;
} rvm_stack;

// The Heap is a collection of objects, a rvm pointer is currently just a index into the heap.
typedef struct rvm_heap {
    struct rvm_object *objects;
    u64         size;
    u64         cap;
} rvm_heap;

typedef struct rvm_vm {
    u64          pc;
    rvm_stack    stack;
    rvm_heap     heap;
    rvm_bytecode bytecode;
} rvm_vm;

typedef enum rvm_error_code {
    RVME_SUCCESS,
    RVME_ALLOC,             // Malloc failed
    RVME_EARLY_EOF,         // EOF was returned and the bytecode parser was not done
    RVME_FILE_ERROR,        // A file stream has error set
    RVME_INVALID_OBJECT,    // The object at the returned pos is invalid
} rvm_error_code;

// Errors are imutable, you, the user, should never change anything within an error
typedef struct rvm_error {
    rvm_error_code code;
    union {
        struct rvme_invalid_object {
            u64 pos;
        } rvme_invalid_object;
    } data;
} rvm_error;

rvm_error *rvme_make_error(rvm_error err);
void rvme_free(rvm_error *err);

typedef enum rvm_object_type {
    RVMO_U64,
    RVMO_POINTER,
    RVMO_LAST,
} rvm_object_type;

_Static_assert(RVMO_LAST - 1 <= UINT8_MAX, "The rvm object types should only be one byte");

typedef struct rvm_object {
    rvm_object_type type;
    union {
        u64 obj_u64;
        u64 obj_pointer;
    };
} rvm_object;

// Decodes a object from a file stream, returns the decoded object
// The object is only valid, if the value of the variable in err is NULL
rvm_object rvmo_decode_object(struct rvm_vm *vm, FILE* file, u64 *pos, rvm_error** err);
