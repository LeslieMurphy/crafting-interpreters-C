#pragma once
//> Chunks of Bytecode value-h
#ifndef clox_value_h
#define clox_value_h
//> Optimization include-string

#include <string.h>
//< Optimization include-string

#include "common.h"

// Ch 19.2 pg 343 Struct Inheritance
typedef struct Obj Obj;

typedef struct ObjString ObjString;
//< forward-declare-obj-string

//< Strings forward-declare-obj
//> Optimization nan-boxing
#ifdef NAN_BOXING
//> qnan

//> sign-bit
#define SIGN_BIT ((uint64_t)0x8000000000000000)
//< sign-bit
#define QNAN     ((uint64_t)0x7ffc000000000000)
//< qnan
//> tags

#define TAG_NIL   1 // 01.
#define TAG_FALSE 2 // 10.
#define TAG_TRUE  3 // 11.
//< tags

typedef enum {
    VAL_BOOL,
    VAL_NIL, 
    VAL_NUMBER,
    VAL_OBJ
} ValueType;


// typedef double Value; // ch 17

// Value is 16 bytes. 

/* CH 18 */
typedef struct {
    ValueType type;
    union {
        bool boolean;
        double number;
        Obj* obj;
    } as;
} Value;

/*
typedef struct {
    ValueType type;  // likely 4 bytes (enum stored as int) uint8_t  ??
    union {
        bool boolean;  // 1 byte
        double number; // 8 bytes
        Obj* obj;      // 8 bytes on 64-bit systems
    } as;
} Value;
*/

// typedef uint64_t Value;
//> is-number


/*  version  NAN_BOXING
#define IS_BOOL(value)      (((value) | 1) == TRUE_VAL)
#define IS_NIL(value)       ((value) == NIL_VAL)
#define IS_NUMBER(value)    (((value) & QNAN) != QNAN)
#define IS_OBJ(value) \
    (((value) & (QNAN | SIGN_BIT)) == (QNAN | SIGN_BIT))

#define AS_BOOL(value)      ((value) == TRUE_VAL)
#define AS_NUMBER(value)    valueToNum(value)
#define AS_OBJ(value) \
    ((Obj*)(uintptr_t)((value) & ~(SIGN_BIT | QNAN)))

#define BOOL_VAL(b)     ((b) ? TRUE_VAL : FALSE_VAL)
#define FALSE_VAL       ((Value)(uint64_t)(QNAN | TAG_FALSE))
#define TRUE_VAL        ((Value)(uint64_t)(QNAN | TAG_TRUE))
#define NIL_VAL         ((Value)(uint64_t)(QNAN | TAG_NIL))
#define NUMBER_VAL(num) numToValue(num)
#define OBJ_VAL(obj) \
    (Value)(SIGN_BIT | QNAN | (uint64_t)(uintptr_t)(obj))
*/

#define IS_BOOL(value)    ((value).type == VAL_BOOL)
#define IS_NIL(value)     ((value).type == VAL_NIL)
#define IS_NUMBER(value)  ((value).type == VAL_NUMBER)
#define IS_OBJ(value)     ((value).type == VAL_OBJ)

#define AS_OBJ(value)     ((value).as.obj)
#define AS_BOOL(value)    ((value).as.boolean)
#define AS_NUMBER(value)  ((value).as.number)

#define BOOL_VAL(value)   ((Value){VAL_BOOL, {.boolean = value}})
#define NIL_VAL           ((Value){VAL_NIL, {.number = 0}})
#define NUMBER_VAL(value) ((Value){VAL_NUMBER, {.number = value}})
#define OBJ_VAL(object)   ((Value){VAL_OBJ, {.obj = (Obj*)object}})

static inline double valueToNum(Value value) {
    double num;
    memcpy(&num, &value, sizeof(Value));
    return num;
}
//< value-to-num
//> num-to-value

static inline Value numToValue(double num) {
    Value value;
    memcpy(&value, &num, sizeof(double));
    return value;
}
//< num-to-value

#else

//< Optimization nan-boxing
//> Types of Values value-type
typedef enum {
    VAL_BOOL,
    VAL_NIL, // [user-types]
    VAL_NUMBER,
    VAL_OBJ
} ValueType;

//< Types of Values value-type
/* Chunks of Bytecode value-h < Types of Values value
typedef double Value;
*/

typedef struct {
    ValueType type;
    union {
        bool boolean;
        double number;
        //> Strings union-object
        // llm future Obj* obj;
        //< Strings union-object
    } as; 
} Value;

//> Types of Values is-macros

#define IS_BOOL(value)    ((value).type == VAL_BOOL)
#define IS_NIL(value)     ((value).type == VAL_NIL)
#define IS_NUMBER(value)  ((value).type == VAL_NUMBER)
//> Strings is-obj
#define IS_OBJ(value)     ((value).type == VAL_OBJ)
//< Strings is-obj
//< Types of Values is-macros
//> Types of Values as-macros

//> Strings as-obj
#define AS_OBJ(value)     ((value).as.obj)
//< Strings as-obj
#define AS_BOOL(value)    ((value).as.boolean)
#define AS_NUMBER(value)  ((value).as.number)
//< Types of Values as-macros
//> Types of Values value-macros

#define BOOL_VAL(value)   ((Value){VAL_BOOL, {.boolean = value}})
#define NIL_VAL           ((Value){VAL_NIL, {.number = 0}})
#define NUMBER_VAL(value) ((Value){VAL_NUMBER, {.number = value}})
//> Strings obj-val
#define OBJ_VAL(object)   ((Value){VAL_OBJ, {.obj = (Obj*)object}})
//< Strings obj-val
//< Types of Values value-macros
//> Optimization end-if-nan-boxing

#endif
//< Optimization end-if-nan-boxing
//> value-array

typedef struct {
    int capacity;
    int count;
    Value* values;
} ValueArray;
//< value-array
//> array-fns-h

//> Types of Values values-equal-h
bool valuesEqual(Value a, Value b);  // introduced in 18.4 pg 338
//< Types of Values values-equal-h
void initValueArray(ValueArray* array);
void writeValueArray(ValueArray* array, Value value);
void freeValueArray(ValueArray* array);
//< array-fns-h
//> print-value-h
void printValue(Value value);
//< print-value-h

#endif
