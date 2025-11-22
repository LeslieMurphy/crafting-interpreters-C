//> Chunks of Bytecode memory-c
// LES see https://github.com/munificent/craftinginterpreters/blob/master/c/memory.c
#include <stdlib.h>

//< Garbage Collection memory-include-compiler
#include "memory.h"
#include "vm.h"

// LLM for logging
#include <stdio.h>

void* allocate(size_t size) {
    void* result = malloc(size);
    if (result == NULL) exit(1);
    return result;
}

void* reallocate(void* pointer, size_t oldSize, size_t newSize) {
    if (newSize == 0) {
        free(pointer);
        return NULL;
    }

    void* result = realloc(pointer, newSize);
    //> out-of-memory
    if (result == NULL) exit(1);
    //< out-of-memory
    return result;
}

// Added Ch 19.5
static void freeObject(Obj* object) {
    // #ifdef DEBUG_LOG_GC
    printf("%p free type %d\n", (void*)object, object->type);
    // #endif
    switch (object->type) {
        /*
        case OBJ_BOUND_METHOD:
            FREE(ObjBoundMethod, object);
            break;
        case OBJ_CLASS: {
            ObjClass* klass = (ObjClass*)object;
            freeTable(&klass->methods);
            FREE(ObjClass, object);
            break;
        } 
        case OBJ_CLOSURE: {
            ObjClosure* closure = (ObjClosure*)object;
            FREE_ARRAY(ObjUpvalue*, closure->upvalues,
                closure->upvalueCount);
            FREE(ObjClosure, object);
            break;
        }
         case OBJ_INSTANCE: {
            ObjInstance* instance = (ObjInstance*)object;
            // freeTable(&instance->fields);
            FREE(ObjInstance, object);
            break;
        }
        case OBJ_UPVALUE:
            FREE(ObjUpvalue, object);
            break;
        */
        case OBJ_FUNCTION: {  // added Ch 24.1 pg 435
            ObjFunction* function = (ObjFunction*)object;
            freeChunk(&function->chunk);
            FREE(ObjFunction, object);
            break;
        }

       
        case OBJ_NATIVE:
            FREE(ObjNative, object);
            break;
        case OBJ_STRING: {
            ObjString* string = (ObjString*)object;
            printf("  free string %s\n", string->chars);
            FREE_ARRAY(char, string->chars, string->length + 1);
            FREE(ObjString, object);
            break;
        }
        
    }
}

// Added Ch 19.5
void freeObjects() {
    printf("free All Objects\n");
    Obj* object = vm.objects;
    while (object != NULL) {
        Obj* next = object->next;
        freeObject(object);
        object = next;
    }
}