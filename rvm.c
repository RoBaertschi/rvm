#include "rvm.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

// If we cannot alloc, we still have to somehow tell the user about that
static rvm_error alloc_error = {
    .code = RVME_ALLOC,
};

rvm_error *make_error(rvm_error err) {
    rvm_error *ptr = malloc(sizeof(err));
    if (ptr == NULL) {
        return &alloc_error;
    }
    *ptr = err;
    return ptr;
}

rvm_error *rvme_make_error(rvm_error err) {
    return make_error(err);
}

void rvme_free(rvm_error *err) {
    // Don't free the static alloc error
    if (err != &alloc_error) {
        free(err);
    }
}

void rvmo_encode_object(rvm_vm *vm, FILE* file, u64 *pos, rvm_object obj, rvm_error** err) {
    ASSERT(err != NULL);
    ASSERT(obj.type != RVMO_LAST && "A object of type RVMO_LAST is invalid and should be impossible.");
    // Write type
    u8 type = 0;
    size_t write = fwrite(&type, sizeof(u8), 1, file);
    *pos += write;
    if (write != sizeof(u8)) {
        goto write_fail;
    }
    obj.type = type;
    switch (obj.type) {
        case RVMO_U64: {
            size_t write = fwrite(&type, sizeof(obj.obj_u64), 1, file);
            *pos += write;
            if (write != sizeof(obj.obj_u64)) {
                goto write_fail;
            }
            return;
        }
        case RVMO_POINTER: {
            size_t write = fwrite(&type, sizeof(obj.obj_pointer), 1, file);
            *pos += write;
            if (write != sizeof(obj.obj_pointer)) {
                goto write_fail;
            }
            return;
        }
        case RVMO_LAST:
            UNREACHABLE();
            break;
        }

    return;
write_fail:
    *err = make_error((rvm_error){.code = RVME_FILE_ERROR});
    return;
}

rvm_object rvmo_decode_object(rvm_vm *vm, FILE* file, u64 *pos, rvm_error** err) {
    ASSERT(err != NULL);
    rvm_object obj = {0};
    rvm_object_type type;
    size_t read = fread(&type, sizeof(u8), 1, file);
    *pos += read;
    if (read != sizeof(u8)) {
        goto read_error;
    }

    if (type >= RVMO_LAST) {
        *err = make_error((rvm_error){
            .code = RVME_INVALID_OBJECT,
            .data = { .rvme_invalid_object = { .pos = *pos } }
        });
        return obj;
    }

    switch (type) {
        case RVMO_U64: {
            u64 obj_u64;
            read = fread(&obj_u64, sizeof(obj_u64), 1, file);
            if (read != sizeof(obj_u64)) {
                goto read_error;
            }
            obj.obj_u64 = obj_u64;
            break;
        }
        case RVMO_POINTER: {
            u64 obj_pointer;
            read = fread(&obj_pointer, sizeof(obj_pointer), 1, file);
            if (read != sizeof(obj_pointer)) {
                goto read_error;
            }
            obj.obj_pointer = obj_pointer;
            break;
        }
        case RVMO_LAST:
            // unreachable
            UNREACHABLE();
            break;
    }

    // Skip the error handling
    goto end;

read_error:
        if (feof(file)) {
            *err = make_error(
                (rvm_error){
                    .code = RVME_EARLY_EOF,
                }
            );
            return obj;
        } else {
            *err = make_error((rvm_error){.code = RVME_FILE_ERROR});
            return obj;
        }
end:

    *err = NULL;
    return obj;
}
