#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include "scanner.h"
//#include "compiler.h"

// forward declaration
struct Compiler; 
typedef struct Compiler Compiler;

// Ch 22.1 pg 401 local variables

typedef struct {
    Token name;
    int depth;  // 0 is a Global, 1 is the first local scope, 2 is the second nested local scope etc.
    bool isCaptured;   // for Closures (not implemented in this repo)
} Local;

void declareLocalVariable(Compiler* current, Token* localVarToken);
int resolveLocal(Compiler* compiler, Token* name);