#pragma once

#include <stdint.h>

typedef uint64_t u64;
typedef uint8_t  u8;

#define RVM_BIT(num) 1 << num

typedef enum rvm_instruction_flags {
    RVM_INSTRUCTION_FLAG_NONE,
    RVM_INSTRUCTION_FLAG = RVM_BIT(0),
} rvm_instruction;

// This struct contains all the required registers for the virtual machine.
typedef struct rvm_registers {
    // Program counter
    u64 pc;

    // Stack related registers

    // Stack pointer, points to the top of the stack.
    u64 sp;
    // Base pointer, points to the base of the function stack
    // It should always be set to sp at the start of a function
    u64 bp;

    // General purpose registers
    u64 r0;
    u64 r1;
    u64 r2;
    u64 r3;
    u64 r4;
    u64 r5;
    u64 r6;
    u64 r7;
} rvm_registers;

typedef struct rvm_stack {
    u8 *ptr;
    u64 size;
} rvm_stack;

// The heap available to the whole program, it can be increased by requesting for more via an instruction
typedef struct rvm_heap {
    u8 *ptr;
    u64 size;
} rvm_heap;

typedef struct rvm_vm {
    rvm_registers registers;
    rvm_stack     stack;
    rvm_heap      heap;
} rvm_vm;
