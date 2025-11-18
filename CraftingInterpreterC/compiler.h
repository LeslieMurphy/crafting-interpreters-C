#pragma once
//> Scanning on Demand compiler-h
#ifndef clox_compiler_h
#define clox_compiler_h

#include "object.h"

//> Compiling Expressions compile-h
#include "vm.h"

//< Compiling Expressions compile-h

// void compile(const char* source);
// bool compile(const char* source, Chunk* chunk); // prior to ch 24

ObjFunction* compile(const char* source);

/* Compiling Expressions compile-h < Calls and Functions compile-h

*/
//> Calls and Functions compile-h
//ObjFunction* compile(const char* source);
//< Calls and Functions compile-h
//> Garbage Collection mark-compiler-roots-h
//void markCompilerRoots();
//< Garbage Collection mark-compiler-roots-h

#endif