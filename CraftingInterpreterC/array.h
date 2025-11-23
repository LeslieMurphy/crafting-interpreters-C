#pragma once
#include "value.h"
#define MAXARRAYDIMENSIONS 3
#define MAXARRAYVARIABLES  5

// Array support

typedef struct {
    int lBound;
    int uBound;
} ArrayBound;

typedef struct {
    char* variableName;
    int dimensions;
    ArrayBound bounds[MAXARRAYDIMENSIONS];
    Value* arrayValues;
} ArrayVariable;

typedef struct {
    ArrayVariable* arrayVars[MAXARRAYVARIABLES];
    int arrayVarCount;
    int bytesCurrAllocated;
    char* memoryPool;  // contiguous using arena allocator pattern
} ArrayVariables;

typedef struct CallFrame CallFrame;  // forward ref



typedef struct {
    CallFrame* frame;
    int start;         // start index in bytecode to run for rhs
    int end;           // end index in bytecode to create new rhs
} ValueContext;


ArrayVariable* allocateNewArrayVar(ArrayVariables* av, int numBounds, int varCount);

int calculateVarCount(int lbound, int ubound);

int calculateArraySize(int dimensions, int lBounds[MAXARRAYDIMENSIONS], int uBounds[MAXARRAYDIMENSIONS]);

// get a value out of the array using the dimensions
// bounds check the request and generate runtime error if invalid
Value* getArrayValue(ArrayVariable* varDefn, Value subscripts[], char* errbuf, size_t errbuf_size);

// callback for setArrayValue
typedef Value* (*ValueProvider)(ValueContext* ctx);

// void get_new_rhs_for_set_array(ValueProvider provider, ValueContext* ctx);

// set a single value, or if one or more of the subscripts has the star operator set all values in that slice
// TODO handle subscript range e.g. foo[7:10] = 0;
// bool setArrayValue(ArrayVariable* varDefn, Value* newvalue, Value subscripts[], char* errbuf, size_t errbuf_size);
bool setArrayValue(ArrayVariable* varDefn, ValueProvider provider, ValueContext* ctx, Value subscripts[], char* errbuf, size_t errbuf_size);


