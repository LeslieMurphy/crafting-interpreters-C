#pragma once
#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"
#include "value.h"
#include "object.h"
#include "hashtable.h"

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)
// #define STACK_MAX 256  // prior to ch 24

// Ch 24.3.3 provide call stack frames for functions
// CallFrame is for a single ongoing function call - pg 442
typedef struct {
	ObjFunction* function;
	// ObjClosure* closure;
	uint8_t* ip;
	Value* slots;
} CallFrame;

typedef struct {
	CallFrame frames[FRAMES_MAX];
	int frameCount;
	/* removed in Ch 24 
	Chunk* chunk;
	uint8_t* ip;*/

	Value stack[STACK_MAX];
	Value* stackTop;
	Table globals; // added Ch 21.2 pg 390 for global vars
	Table strings; // added Ch 20.5 pg 377 for string interning
	Obj* objects; //  added in Ch 19.5 page 352 as starting point for eventual GC implementation

	long long instructionCount;
	long long pushCount;
	long long popCount;

} VM;

void initVM();
void freeVM();


typedef enum {
	INTERPRET_OK,
	INTERPRET_COMPILE_ERROR,
	INTERPRET_RUNTIME_ERROR
} InterpretResult;

InterpretResult interpret(const char* source);
// InterpretResult interpretChunk(Chunk *chunk);

extern VM vm;

// added in Ch 15.2.1
void push(Value value);
Value pop();

#endif