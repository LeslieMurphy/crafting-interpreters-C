#pragma once
#ifndef clox_compiler_h
#define clox_compiler_h

#include "object.h"
#include "vm.h"
#include "local.h"

void error(const char* message);

// void compile(const char* source);
// bool compile(const char* source, Chunk* chunk); // prior to ch 24

ObjFunction* compile(const char* source);

typedef enum {
    TYPE_FUNCTION,
    TYPE_INITIALIZER,
    TYPE_METHOD,
    TYPE_SCRIPT  // a dummy function used for the main program logic
} FunctionType;

typedef struct Compiler {
    struct Compiler* enclosing;  // linked list added Ch 24.4.1 pg 448
    ObjFunction* function;
    FunctionType type;

    Local locals[UINT8_COUNT];
    int localCount;  // count of locals in scope
    int scopeDepth;
    // LLM Upvalue upvalues[UINT8_COUNT]; // Closures upvalues-array
} Compiler;

typedef struct {
    Token current;
    Token previous;
    bool hadError;
    bool panicMode;
} Parser;

//> Garbage Collection mark-compiler-roots-h
//void markCompilerRoots();
#endif