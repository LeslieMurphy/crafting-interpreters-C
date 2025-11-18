#pragma once
#ifndef clox_object_h
#define clox_object_h

#include "common.h"
#include "chunk.h"
#include "value.h"

// AS_OBJ is defined in value.h, gets us the (value).as.obj

#define OBJ_TYPE(value)        (AS_OBJ(value)->type)


#define IS_BOUND_METHOD(value) isObjType(value, OBJ_BOUND_METHOD)
#define IS_CLASS(value)        isObjType(value, OBJ_CLASS)
#define IS_CLOSURE(value)      isObjType(value, OBJ_CLOSURE)
#define IS_FUNCTION(value)     isObjType(value, OBJ_FUNCTION)
#define IS_INSTANCE(value)     isObjType(value, OBJ_INSTANCE)
#define IS_NATIVE(value)       isObjType(value, OBJ_NATIVE)
#define IS_STRING(value)       isObjType(value, OBJ_STRING)

#define AS_BOUND_METHOD(value) ((ObjBoundMethod*)AS_OBJ(value))
#define AS_CLASS(value)        ((ObjClass*)AS_OBJ(value))
#define AS_CLOSURE(value)      ((ObjClosure*)AS_OBJ(value))
#define AS_FUNCTION(value)     ((ObjFunction*)AS_OBJ(value))
#define AS_INSTANCE(value)     ((ObjInstance*)AS_OBJ(value))
#define AS_NATIVE(value) \
    (((ObjNative*)AS_OBJ(value))->function)
#define AS_STRING(value)       ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value)      (((ObjString*)AS_OBJ(value))->chars)

typedef enum {
    OBJ_BOUND_METHOD,
    OBJ_CLASS,
    OBJ_CLOSURE,
    OBJ_FUNCTION, // introduced Ch 24.1 page 435
    OBJ_INSTANCE,
    OBJ_NATIVE,
    OBJ_STRING, // introduced Ch 19.2 page 344
    OBJ_UPVALUE
} ObjType;

// introduced Ch 19.2 page 344
struct Obj {
    ObjType type;
    //> Garbage Collection is-marked-field
    bool isMarked;
    //< Garbage Collection is-marked-field

    struct Obj* next; // this added in Ch 19.5 page 352 as starting point for eventual GC implementation
};

// added in Ch 24.1 pg 434 - will have separate bytecode chunk for each function
// functions are first class Lox objects
typedef struct {
    Obj obj;
    int arity;
    int upvalueCount;
    Chunk chunk;
    ObjString* name;
} ObjFunction;

typedef Value(*NativeFn)(int argCount, Value* args);

typedef struct {
    Obj obj;
    NativeFn function;
} ObjNative;

// introduced Ch 19.2 page 344
// we can cast this to Obj*, or downcast an Obj* to an ObjString*
// in debugger use a watch (ObjString*) value.as.obj
struct ObjString {
    Obj obj;
    int length;
    char* chars;
    uint32_t hash; // aded in Ch 20.4.1 pg 367
};

// Array dimension
typedef struct ObjArrayDimension {
    short lBound;
    short uBound;
    short offSet;
} ObjArrayDimension;

// Array
struct ObjArray {
    Obj obj;
    short numDimensions;
    ObjArrayDimension* dimension;
    uint32_t hash; // aded in Ch 20.4.1 pg 367
};


/* not implemented in this repo
typedef struct ObjUpvalue {
    Obj obj;
    Value* location;
    //> closed-field
    Value closed;
    //< closed-field
    //> next-field
    struct ObjUpvalue* next;
    //< next-field
} ObjUpvalue;


typedef struct {
    Obj obj;
    ObjFunction* function;
    //> upvalue-fields
    ObjUpvalue** upvalues;
    int upvalueCount;
    //< upvalue-fields
} ObjClosure;


typedef struct {
    Obj obj;
    ObjString* name;
    //> Methods and Initializers class-methods
    // LLM future  Table methods;
    //< Methods and Initializers class-methods
} ObjClass;

typedef struct {
    Obj obj;
    ObjClass* klass;
    // LLM future Table fields; // [fields]
} ObjInstance;

typedef struct {
    Obj obj;
    Value receiver;
    ObjClosure* method;
} ObjBoundMethod;
*/

/*
//< Methods and Initializers obj-bound-method
//> Methods and Initializers new-bound-method-h
ObjBoundMethod* newBoundMethod(Value receiver,
    ObjClosure* method);
//< Methods and Initializers new-bound-method-h
//> Classes and Instances new-class-h
ObjClass* newClass(ObjString* name);
//< Classes and Instances new-class-h
//> Closures new-closure-h
ObjClosure* newClosure(ObjFunction* function);
//< Closures new-closure-h
//> Classes and Instances new-instance-h
ObjInstance* newInstance(ObjClass* klass);
//< Classes and Instances new-instance-h
//> Closures new-upvalue-h
ObjUpvalue* newUpvalue(Value* slot);
//< Closures new-upvalue-h
*/

ObjFunction* newFunction(); // Ch 24.1 pg 434
ObjNative* newNative(NativeFn function);  // Ch 24.7 pg 459
ObjString* takeString(char* chars, int length); // ch 19.4.1 page 351 take ownership of string
ObjString* copyString(const char* chars, int length);
void printObject(Value value);

// introduced Ch 19.2 page 345
static inline bool isObjType(Value value, ObjType type) {
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif