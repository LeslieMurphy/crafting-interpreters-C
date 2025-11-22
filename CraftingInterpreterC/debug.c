//> Chunks of Bytecode debug-c
#include <stdio.h>

#include "debug.h"
#include "value.h"

#define READ_SHORT() \
    (frame->ip += 2, \
    (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))

#define DEBUG_READ_SHORT() \
    (offset += 2, \
    (uint16_t)((chunk->code[offset-2] << 8) | chunk->code[offset-1]))

// Ch 14.5.3
// The constant index is in the next byte of bytecode
static int constantInstruction(const char* name, Chunk* chunk, int offset) {
    uint8_t constant = chunk->code[offset + 1];
    printf("%-16s %4d '", name, constant);
    printValue(chunk->constants.values[constant]);
    printf("'\n");
    return offset + 2; // next instruction is 2 bytecodes later
}

static int invokeInstruction(const char* name, Chunk* chunk,
    int offset) {
    uint8_t constant = chunk->code[offset + 1];
    uint8_t argCount = chunk->code[offset + 2];
    printf("%-16s (%d args) %4d '", name, argCount, constant);
    printValue(chunk->constants.values[constant]);
    printf("'\n");
    return offset + 3;
}

static int simpleInstruction(const char* name, int offset) {
	printf("%s\n", name);
	return offset + 1;
}

// Added in Ch 22.4.1 pg 410 for local variables
// Show the slot number of the variable
// TODO add tracking at runtime for the associated name of the variable for debugging / error messages
static int byteInstruction(const char* name, Chunk* chunk, int offset) {
    uint8_t slot = chunk->code[offset + 1];
    printf("%-16s %4d\n", name, slot);
    return offset + 2;
}

static int arrayRefInstruction(const char* name, Chunk* chunk, int offset, bool isSetOperation) {
    uint8_t constantNameIdx = chunk->code[offset + 1];
    printf("%-16s %4d '", name, constantNameIdx);
    printValue(chunk->constants.values[constantNameIdx]);

    if (isSetOperation){
        uint8_t start_rhs_ip = chunk->code[offset + 2];
        printf("'  start of RHS ip counter  = % d\n", start_rhs_ip);
        offset++;
    }

    // uint8_t slot = chunk->code[offset + 1];
    uint8_t numSubscripts = chunk->code[offset + 2];
    printf("' #subscripts = % d\n", numSubscripts);
    return offset + 3;
}

static int arrayDefInstruction(const char* name, Chunk* chunk, int offset) {
    uint8_t constantNameIdx = chunk->code[offset + 1];
    printf("%-16s %4d '", name, constantNameIdx);
    printValue(chunk->constants.values[constantNameIdx]);

    // uint8_t slot = chunk->code[offset + 1];
    uint8_t numSubscripts = chunk->code[offset + 2];
    offset = offset + 3;
    short numVars = DEBUG_READ_SHORT();
    printf("' #subscripts = %d varCount = %d   indices ", numSubscripts, numVars);
    // offset = offset + 3;
    for (int i = 0; i < numSubscripts; i++) {
        printf(" (");
        short lBound = DEBUG_READ_SHORT();
        short uBound = DEBUG_READ_SHORT();
        //uint8_t lBound = chunk->code[offset + 4 + i *4];
        //uint8_t uBound = chunk->code[offset + 6 + i * 4];
        printf("%d:%d)  ", lBound, uBound);
    }
    printf("\n");
    //return offset + 3 + numSubscripts * 4;
    return offset;
}




// Added in Ch 23.2 pg 420
static int jumpInstruction(const char* name, int sign,
    Chunk* chunk, int offset) {
    uint16_t jump = (uint16_t)(chunk->code[offset + 1] << 8);
    jump |= chunk->code[offset + 2];
    printf("%-16s %4d -> %d\n", name, offset,
        offset + 3 + sign * jump);
    return offset + 3;
}


int disassembleInstruction(Chunk* chunk, int offset) {
    printf("%04d ", offset);
    ////> show-location
    if (offset > 0 &&
        chunk->lines[offset] == chunk->lines[offset - 1]) {
        printf("   | ");
    }
    else {
        printf("%4d ", chunk->lines[offset]);
    }
    ////< show-location

    uint8_t instruction = chunk->code[offset];
    switch (instruction) {
    case OP_CONSTANT:
        return constantInstruction("OP_CONSTANT", chunk, offset);
    case OP_NIL:
        return simpleInstruction("OP_NIL", offset);
    case OP_TRUE:
        return simpleInstruction("OP_TRUE", offset);
    case OP_FALSE:
        return simpleInstruction("OP_FALSE", offset);
    case OP_POP:
        return simpleInstruction("OP_POP", offset);
    case OP_GET_LOCAL:
        return byteInstruction("OP_GET_LOCAL", chunk, offset);
    case OP_SET_LOCAL:
        return byteInstruction("OP_SET_LOCAL", chunk, offset);
    case OP_GET_GLOBAL:
        return constantInstruction("OP_GET_GLOBAL", chunk, offset);
    case OP_DEFINE_GLOBAL:
        return constantInstruction("OP_DEFINE_GLOBAL", chunk, offset);
    case OP_SET_GLOBAL:
        return constantInstruction("OP_SET_GLOBAL", chunk, offset);
    
    case OP_GET_GLOBAL_ARRAY:
        return arrayRefInstruction("OP_GET_GLOBAL_ARRAY", chunk, offset, false);
    case OP_SET_GLOBAL_ARRAY:
        return arrayRefInstruction("OP_SET_GLOBAL_ARRAY", chunk, offset, true);
    case OP_DEFINE_GLOBAL_ARRAY:
        return arrayDefInstruction("OP_DEFINE_GLOBAL_ARRAY", chunk, offset);
    
    case OP_GET_UPVALUE:
        return byteInstruction("OP_GET_UPVALUE", chunk, offset);
    case OP_SET_UPVALUE:
        return byteInstruction("OP_SET_UPVALUE", chunk, offset);
    case OP_GET_PROPERTY:
        return constantInstruction("OP_GET_PROPERTY", chunk, offset);
    case OP_SET_PROPERTY:
        return constantInstruction("OP_SET_PROPERTY", chunk, offset);
    case OP_GET_SUPER:
        return constantInstruction("OP_GET_SUPER", chunk, offset);
    
    // introduced for 18.4.2 pg 339
    case OP_EQUAL:
        return simpleInstruction("OP_EQUAL", offset); 
    case OP_GREATER:
        return simpleInstruction("OP_GREATER", offset);
    case OP_LESS:
        return simpleInstruction("OP_LESS", offset);

        //> A Virtual Machine disassemble-binary
    case OP_ADD:
        return simpleInstruction("OP_ADD", offset);
    case OP_SUBTRACT:
        return simpleInstruction("OP_SUBTRACT", offset);
    case OP_MULTIPLY:
        return simpleInstruction("OP_MULTIPLY", offset);
    case OP_DIVIDE:
        return simpleInstruction("OP_DIVIDE", offset);
    case OP_RANDOM:
        return simpleInstruction("OP_RANDOM", offset);
        
    case OP_NOT:
        return simpleInstruction("OP_NOT", offset);
        //< Types of Values disassemble-not
        //< A Virtual Machine disassemble-binary
        //> A Virtual Machine disassemble-negate
    case OP_NEGATE:
        return simpleInstruction("OP_NEGATE", offset);
        //< A Virtual Machine disassemble-negate
        //> Global Variables disassemble-print
    case OP_PRINT:
        return simpleInstruction("OP_PRINT", offset);
        //< Global Variables disassemble-print
        //> Jumping Back and Forth disassemble-jump
    case OP_JUMP:
        return jumpInstruction("OP_JUMP", 1, chunk, offset);
    case OP_JUMP_IF_FALSE:
        return jumpInstruction("OP_JUMP_IF_FALSE", 1, chunk, offset);
    case OP_LOOP:
        return jumpInstruction("OP_LOOP", -1, chunk, offset);
    case OP_CALL:
        return byteInstruction("OP_CALL", chunk, offset);
        //> Methods and Initializers disassemble-invoke
    case OP_INVOKE:
        return invokeInstruction("OP_INVOKE", chunk, offset);
    case OP_SUPER_INVOKE:
        return invokeInstruction("OP_SUPER_INVOKE", chunk, offset);
    //case OP_CLOSURE: {
    //    offset++;
    //    uint8_t constant = chunk->code[offset++];
    //    printf("%-16s %4d ", "OP_CLOSURE", constant);
    //    printValue(chunk->constants.values[constant]);
    //    printf("\n");
    //    //> disassemble-upvalues

    //    ObjFunction* function = AS_FUNCTION(
    //        chunk->constants.values[constant]);
    //    for (int j = 0; j < function->upvalueCount; j++) {
    //        int isLocal = chunk->code[offset++];
    //        int index = chunk->code[offset++];
    //        printf("%04d      |                     %s %d\n",
    //            offset - 2, isLocal ? "local" : "upvalue", index);
    //    }

    //    //< disassemble-upvalues
    //    return offset;
    //}
                   //< Closures disassemble-closure
                   //> Closures disassemble-close-upvalue
    case OP_CLOSE_UPVALUE:
        return simpleInstruction("OP_CLOSE_UPVALUE", offset);
    case OP_RETURN:
        return simpleInstruction("OP_RETURN", offset);
    case OP_CLASS:
        return constantInstruction("OP_CLASS", chunk, offset);
    case OP_INHERIT:
        return simpleInstruction("OP_INHERIT", offset);
    case OP_METHOD:
        return constantInstruction("OP_METHOD", chunk, offset);
    default:
        printf("Unknown opcode %d\n", instruction);
        return offset + 1;
    }
}

void disassembleChunk(Chunk* chunk, const char* name) {
    printf("== %s ==\n", name);

    for (int offset = 0; offset < chunk->count;) {
        offset = disassembleInstruction(chunk, offset);
    }
}

#undef DEBUG_READ_SHORT