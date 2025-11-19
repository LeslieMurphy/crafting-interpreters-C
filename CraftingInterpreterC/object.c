//> Strings object-c
#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"

#include "value.h"
#include "vm.h"
#include "hashtable.h"

#define ALLOCATE_OBJ(type, objectType) \
    (type*)allocateObject(sizeof(type), objectType)

// ch 19.4 page 348
static Obj* allocateObject(size_t size, ObjType type) {
    Obj* object = (Obj*)reallocate(NULL, 0, size);
    object->type = type;
    object->isMarked = false;

    object->next = vm.objects; // Ch 19.5 - chain for eventual GC
    vm.objects = object;
    
    //> Garbage Collection debug-log-allocate
#ifdef DEBUG_LOG_GC
    printf("%p allocate %zu for %d\n", (void*)object, size, type);
#endif

    return object;
}

// New for Ch 24.1
ObjFunction* newFunction() {
    ObjFunction* function = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);
    function->arity = 0;
    function->upvalueCount = 0;
    function->name = NULL;
    initChunk(&function->chunk);
    return function;
}

//ObjInstance* newInstance(ObjClass* klass) {
//    ObjInstance* instance = ALLOCATE_OBJ(ObjInstance, OBJ_INSTANCE);
//    instance->klass = klass;
//    initTable(&instance->fields);
//    return instance;
//}

ObjNative* newNative(NativeFn function) {
    ObjNative* native = ALLOCATE_OBJ(ObjNative, OBJ_NATIVE);
    native->function = function;
    return native;
}

/*
//> Methods and Initializers new-bound-method
ObjBoundMethod* newBoundMethod(Value receiver,
    ObjClosure* method) {
    ObjBoundMethod* bound = ALLOCATE_OBJ(ObjBoundMethod,
        OBJ_BOUND_METHOD);
    bound->receiver = receiver;
    bound->method = method;
    return bound;
}
//< Methods and Initializers new-bound-method
//> Classes and Instances new-class
ObjClass* newClass(ObjString* name) {
    ObjClass* klass = ALLOCATE_OBJ(ObjClass, OBJ_CLASS);
    klass->name = name; // [klass]
    
    // LLM initTable(&klass->methods);
    
    return klass;
}
//< Classes and Instances new-class
//> Closures new-closure
ObjClosure* newClosure(ObjFunction* function) {
    //> allocate-upvalue-array
    ObjUpvalue** upvalues = ALLOCATE(ObjUpvalue*,
        function->upvalueCount);
    for (int i = 0; i < function->upvalueCount; i++) {
        upvalues[i] = NULL;
    }

    //< allocate-upvalue-array
    ObjClosure* closure = ALLOCATE_OBJ(ObjClosure, OBJ_CLOSURE);
    closure->function = function;
    //> init-upvalue-fields
    closure->upvalues = upvalues;
    closure->upvalueCount = function->upvalueCount;
    //< init-upvalue-fields
    return closure;
}
//< Closures new-closure

//> Classes and Instances new-instance

ObjInstance* newInstance(ObjClass* klass) {
    ObjInstance* instance = ALLOCATE_OBJ(ObjInstance, OBJ_INSTANCE);
    instance->klass = klass;
    initTable(&instance->fields);
    return instance;
}
*/

// Ch 19.4 page 348
static ObjString* allocateString(char* chars, int length, uint32_t hash) {
    ObjString* string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    string->hash = hash;

    // Garbage Collection push-string added in CH 26.6 
    // push into vm stack temporarily before doing GC ...

    // LLM GC push(OBJ_VAL(string));
    
    tableSet(&vm.strings, string, NIL_VAL);
    
    // LLM GC pop();

    //< Garbage Collection pop-string
    //< Hash Tables allocate-store-string
    return string;
}

// Added in Ch 20.4.1 - this is a FNV-1a hash (Fowler–Noll–Vo)
static uint32_t hashString(const char* key, int length) {
    uint32_t hash = 2166136261u;
    for (int i = 0; i < length; i++) {
        hash ^= (uint8_t)key[i];  // XOR
        hash *= 16777619;
    }
    return hash;
}


// ch 19.4.1 page 351 take ownership of string

ObjString* takeString(char* chars, int length) {
    
    uint32_t hash = hashString(chars, length);  // added in Ch 20.4.1
  
    /* Added in Ch 20.5 pg 378 */
    ObjString* interned = tableFindString(&vm.strings, chars, length,
        hash);
    if (interned != NULL) {
        FREE_ARRAY(char, chars, length + 1);
        return interned;
    }

    return allocateString(chars, length, hash);
}

// file and method created in Ch 19.3 page 347
ObjString* copyString(const char* chars, int length) {
    uint32_t hash = hashString(chars, length);  // added in Ch 20.4.1
    
    /* Added in Ch 20.5 pg 378 */
    ObjString* interned = tableFindString(&vm.strings, chars, length,
        hash);
    if (interned != NULL) return interned;
        
    char* heapChars = ALLOCATE(char, length + 1);
    memcpy(heapChars, chars, length);
    heapChars[length] = '\0';
    return allocateString(heapChars, length, hash);
}

/*
//> Closures new-upvalue
ObjUpvalue* newUpvalue(Value* slot) {
    ObjUpvalue* upvalue = ALLOCATE_OBJ(ObjUpvalue, OBJ_UPVALUE);
    //> init-closed
    upvalue->closed = NIL_VAL;
    //< init-closed
    upvalue->location = slot;
    //> init-next
    upvalue->next = NULL;
    //< init-next
    return upvalue;
}
*/

static void printFunction(ObjFunction* function) {
    if (function->name == NULL) {
        printf("<script>");
        return;
    }
    printf("<fn %s>", function->name->chars);
}

void printObject(Value value) {
    switch (OBJ_TYPE(value)) {
    /*
    case OBJ_BOUND_METHOD:
        printFunction(AS_BOUND_METHOD(value)->method->function);
        break;
    case OBJ_CLASS:
        printf("%s", AS_CLASS(value)->name->chars);
        break;
    case OBJ_INSTANCE:
        printf("%s instance",
            AS_INSTANCE(value)->klass->name->chars);
        break;
    case OBJ_CLOSURE:
        printFunction(AS_CLOSURE(value)->function);
        break;
    case OBJ_UPVALUE:
        printf("upvalue");
        break;
   
    
    */
    case OBJ_NATIVE:
        printf("<native fn>");
        break;
    case OBJ_STRING:
        printf("%s", AS_CSTRING(value));
        break;
    case OBJ_FUNCTION:
        printFunction(AS_FUNCTION(value));  // added Ch 24.1
        break;
    }
    
}
//< print-object