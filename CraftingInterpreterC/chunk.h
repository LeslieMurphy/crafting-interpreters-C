#pragma once
//> Chunks of Bytecode chunk-h
#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"
//> chunk-h-include-value
#include "value.h"
//< chunk-h-include-value
//> op-enum

typedef enum {
	OP_INVALID,  // this is zero and could show up if we run off the end of the VM bytecode by accident!
	OP_CONSTANT,

	OP_NIL,
	OP_TRUE,
	OP_FALSE,
	
	// Global Variables pop-op
	OP_POP,
	
	OP_GET_LOCAL,
	OP_SET_LOCAL,
	OP_GET_GLOBAL,
	OP_DEFINE_GLOBAL,
	OP_SET_GLOBAL,
	OP_GET_UPVALUE,
	OP_SET_UPVALUE,
	
	OP_GET_PROPERTY,
	OP_SET_PROPERTY,
	
	OP_GET_SUPER,
	
	// introduced in 18.4.2 pg 337 - handle !=, <= and >= as bytecode emitter logic
	OP_EQUAL,
	OP_GREATER,
	OP_LESS,
	
	//> A Virtual Machine binary-ops
	OP_ADD,
	OP_SUBTRACT,
	OP_MULTIPLY,
	OP_DIVIDE,
	OP_NOT,
	OP_NEGATE,
	OP_PRINT,
	OP_JUMP,
	OP_JUMP_IF_FALSE,
	OP_LOOP,
	OP_CALL,
	OP_INVOKE,
	OP_SUPER_INVOKE,
	OP_CLOSURE,
	OP_CLOSE_UPVALUE,
	OP_RETURN,
	OP_CLASS,
	OP_INHERIT,
	OP_METHOD,
	OP_RANDOM
	
} OpCode;
//< op-enum
//> chunk-struct

typedef struct {
	//> count-and-capacity
	int count;
	int capacity;
	//< count-and-capacity
	uint8_t* code;
	//> chunk-lines
	int* lines;
	//< chunk-lines
	//> chunk-constants
	ValueArray constants;
	//< chunk-constants
} Chunk;
//< chunk-struct
//> init-chunk-h

void initChunk(Chunk* chunk);
//< init-chunk-h
//> free-chunk-h
void freeChunk(Chunk* chunk);
//< free-chunk-h

//void writeChunk(Chunk* chunk, uint8_t byte);

//> write-chunk-with-line-h
void writeChunk(Chunk* chunk, uint8_t byte, int line);
//< write-chunk-with-line-h
//> add-constant-h
int addConstant(Chunk* chunk, Value value);
//< add-constant-h

#endif