#include <stdio.h>
#include <stdlib.h>
#include "common.h"

#include "vm.h"
// #include <windows.h>



// need setting /TC in VS project settings to compile as C

// New features added:
// Logging instruction and push/pop counts
// var x = 5, y = x+10;  // multiple declarations
// print 5 ? 10;         // new binary op print from 1 to 10
// (partial) optimizations in VM for reducing stack churn of pop and push for binary ops


char* getMultipleLines(void);

// Page 281 ch 15 - hardcoded bytecode chunks - no longer valid as of ch 18
/*
void testChunks() {
    Chunk chunk;
    initChunk(&chunk);

    printf("Test bytecode chunks \n");

    // int constant = addConstant(&chunk, 1.2); // ch 15

    // ch 18
    Value v;
    v.type = VAL_NUMBER;
    v.as.number = 1.2;
    int constant = addConstant(&chunk, v);
    writeChunk(&chunk, OP_CONSTANT, 127);
    writeChunk(&chunk, constant, 127);

    // constant = addConstant(&chunk, 3.4);  // page 281 
    v.as.number = 3.4;
    constant = addConstant(&chunk, v);  // for ch 18
    writeChunk(&chunk, OP_CONSTANT, 127);
    writeChunk(&chunk, constant, 127);

    writeChunk(&chunk, OP_ADD, 127);

    //constant = addConstant(&chunk, 5.6);
    v.as.number = 5.6;
    constant = addConstant(&chunk, v);
    
    writeChunk(&chunk, OP_CONSTANT, 127);
    writeChunk(&chunk, constant, 127);

    writeChunk(&chunk, OP_DIVIDE, 127);
    writeChunk(&chunk, OP_RETURN, 127);

    // disassembleChunk(&chunk, "test chunk");
    interpretChunk(&chunk);
    printf("disassy done \n");
    freeChunk(&chunk);
    printf("chunk freed \n");
}
*/

static void repl() {
    // char line[1024];
    for (;;) {
        printf("> ");

        //if (!fgets(line, sizeof(line), stdin)) {
        //    printf("\n");
        //    break;
        //}
        //
        //// testChunks();
        //interpret(line);

        char* sourceLines = getMultipleLines();
        if (!sourceLines)
            break;
        interpret(sourceLines);
        free(sourceLines);
        printf("VM Statistics:\n");
        printf("VM instruction count=%lld, push=%lld; pop=%lld\n", vm.instructionCount, vm.pushCount, vm.popCount);
    }
}

static char* readFile(const char* path) {
    FILE* file = fopen(path, "rb");
    //> no-file
    if (file == NULL) {
        fprintf(stderr, "Could not open file \"%s\".\n", path);
        exit(74);
    }
    //< no-file

    fseek(file, 0L, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);

    char* buffer = (char*)malloc(fileSize + 1);
    //> no-buffer
    if (buffer == NULL) {
        fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
        exit(74);
    }

    //< no-buffer
    size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
    //> no-read
    if (bytesRead < fileSize) {
        fprintf(stderr, "Could not read file \"%s\".\n", path);
        exit(74);
    }

    //< no-read
    buffer[bytesRead] = '\0';

    fclose(file);
    return buffer;
}

static void runFile(const char* path) {
    char* source = readFile(path);
    InterpretResult result = interpret(source);
    free(source); // [owner]

    if (result == INTERPRET_COMPILE_ERROR) exit(65);
    if (result == INTERPRET_RUNTIME_ERROR) exit(70);
}

#define MAX_TOTAL_LEN 4096   // Maximum total characters to store
#define MAX_LINE_LEN  512    // Maximum characters per line
char * getMultipleLines(void) {
    char* buffer = malloc(MAX_TOTAL_LEN);
    if (!buffer) {
        fprintf(stderr, "Memory allocation failed.\n");
        return NULL;
    }
    buffer[0] = '\0'; // Start with empty string

    char line[MAX_LINE_LEN];
    size_t total_len = 0;

    printf("Enter LOX source code (empty line to finish):\n");
   
    while (1) {
        printf("> ");
        if (!fgets(line, sizeof(line), stdin)) {
            // EOF or read error
            break;
        }

        
        if (strcmp(line, "\n") == 0) {
            // return NULL;
            break;
        }

        size_t line_len = strlen(line);

        // Check if appending this line would overflow buffer
        if (total_len + line_len >= MAX_TOTAL_LEN) {
            fprintf(stderr, "Input too long. Truncating.\n");
            break;
        }

        // strcat(buffer, line); // unsafe
        strcat_s(buffer, MAX_TOTAL_LEN, line);
        total_len += line_len;
    }

    // printf("\n--- Combined Input ---\n%s", buffer);

    // Stop when user enters an empty buffer
    if (strlen(buffer) == 0)
        return NULL;

    // free(buffer);
    return buffer;
}


int main(int argc, const char* argv[]) {
    //HANDLE thread = GetCurrentThread();

    //// Set affinity to core 0 (bitmask: 1 << 0)
    //DWORD_PTR mask = 1 << 0; // Core 0
    //DWORD_PTR result = SetThreadAffinityMask(thread, mask);

    //if (result == 0) {
    //    printf("Failed to set thread affinity.\n");
    //}
    //else {
    //    printf("Thread locked to core 0.\n");
    //}

    initVM();

    if (argc == 1) {
        repl();
    }
    else if (argc == 2) {
        runFile(argv[1]);
    }
    else {
        fprintf(stderr, "Usage: clox [path]\n");
        exit(64);
    }
    
    freeVM();
	return 0;
}