#include <stdio.h>
#include <string.h>
#include "object.h"
#include "memory.h"
#include "value.h"


void printValueType(Value value) {
    // printf("%g", value); // ch 17
    //printf("%g", AS_NUMBER(value)); // ch 18
    switch (value.type) {
    case VAL_BOOL: printf("BOOL"); break;
    case VAL_NIL: printf("nil"); break;
    case VAL_NUMBER: printf("NUMBER"); break;
    case VAL_OBJ: printf("OBJECT"); break;
    case VAL_ARRAY_REF: printf(" *array ref ** "); break;
    case VAL_ARRAY_STAR: printf("*"); break;
    default:
        printf("UNKNOWN!!");
    }
}

void printValue(Value value) {
    // printf("%g", value); // ch 17
    //printf("%g", AS_NUMBER(value)); // ch 18
    switch (value.type) {
        case VAL_BOOL:
            printf(AS_BOOL(value) ? "true" : "false");
            break;
        case VAL_NIL: printf("nil"); break;
        case VAL_NUMBER: printf("%g", AS_NUMBER(value)); break;
        case VAL_OBJ: printObject(value); break;
        case VAL_ARRAY_REF: printf(" *array ref ** "); break;
        case VAL_ARRAY_STAR: printf("*"); break;
    }
}

bool valuesEqual(Value a, Value b) { // added ch 18.4.2 page 339
    if (a.type != b.type) return false;
    switch (a.type) {
        case VAL_BOOL:   return AS_BOOL(a) == AS_BOOL(b);
        case VAL_NIL:    return true;
        case VAL_NUMBER: return AS_NUMBER(a) == AS_NUMBER(b);
            /* added in ch 19.4 page 349 
        case VAL_OBJ: {  // addeed in ch 19.4 page 349
            ObjString* aString = AS_STRING(a);
            ObjString* bString = AS_STRING(b);
            return aString->length == bString->length &&
                memcmp(aString->chars, bString->chars,
                    aString->length) == 0;
        }         */
        // TODO as of end of Ch 20 this seems to break expression "x" == "x"
        case VAL_OBJ:    return AS_OBJ(a) == AS_OBJ(b);  // optimization using interning - Ch20.5 pg 380
        default:         return false; // Unreachable.
    }
}

void initValueArray(ValueArray* array) {
    array->values = NULL;
    array->capacity = 0;
    array->count = 0;
}

//> write-value-array
void writeValueArray(ValueArray* array, Value value) {
    if (array->capacity < array->count + 1) {
        int oldCapacity = array->capacity;
        array->capacity = GROW_CAPACITY(oldCapacity);
        array->values = GROW_ARRAY(Value, array->values,
            oldCapacity, array->capacity);
    }

    array->values[array->count] = value;
    array->count++;
}
//< write-value-array

//> free-value-array
void freeValueArray(ValueArray* array) {
    FREE_ARRAY(Value, array->values, array->capacity);
    initValueArray(array);
}
