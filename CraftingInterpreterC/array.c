#include "memory.h"
#include "array.h"
#include <stdio.h>
#include <stdlib.h>

int calculateVarCount(int lbound, int ubound) {
	return 1 + abs(ubound - lbound); // -5 to -3 = len 3  -3 to 2 = len 6   2 to 5 len 4
}

int calculateArraySize(int dimensions, int lBounds[MAXARRAYDIMENSIONS], int uBounds[MAXARRAYDIMENSIONS]) {
	int varCount = calculateVarCount(lBounds[0], uBounds[0]);
	for (int i = 1; i < dimensions; i++) {
		int dimCount = 1 + calculateVarCount(lBounds[i], lBounds[i]);
		varCount *= dimCount;
	}
	return varCount;
}

ArrayVariable* allocateNewArrayVar(ArrayVariables* av, int numBounds, int varCount) {
	int currSize = av->bytesCurrAllocated;
	
	// we need room for the ArrayVariable definition plus 1 to n bounds
	
	int newVarDefnSize = sizeof(ArrayVariable) + numBounds * sizeof(ArrayBound);
	int arrayContentsSize = varCount * sizeof(Value);

	// grow the pool
	av->memoryPool = realloc(av->memoryPool, currSize + newVarDefnSize + arrayContentsSize);

	if (!av->memoryPool) {
		perror("realloc failed");
		exit(1);
	}

	ArrayVariable* ptr = (ArrayVariable*)(av->memoryPool + currSize); 	// pointer to new variable defn

	if (av->arrayVarCount > MAXARRAYVARIABLES) {
		fprintf(stderr, "Too many ArrayVariables!\n");
		exit(99); // TODO handle overflow better - should never happen due to Compiler and VM bytecode guards though
	}

	av->arrayVars[av->arrayVarCount++] = (ArrayVariable*) ptr;
	av->bytesCurrAllocated += newVarDefnSize + arrayContentsSize;


	ptr->arrayValues = (Value*)((char*)ptr + sizeof(ArrayVariable));  // the array contents are stored right after the definition

	return ptr;
}

// get a value out of the array using the dimensions
// bounds check the request and generate runtime error if invalid
Value* getArrayValue(ArrayVariable* varDefn, Value subscripts[], char* errbuf, size_t errbuf_size) {
	// check and process subscript 1 first
	int subscript1 = (int) subscripts[0].as.number;
	ArrayBound bnd = varDefn->bounds[0];
	if (subscript1 < bnd.lBound || subscript1 > bnd.uBound) {
		snprintf(errbuf, errbuf_size, "Subscript value %d is not in array bounds between %d and %d", subscript1, bnd.lBound, bnd.uBound);
		return NULL;
	}
	return &varDefn->arrayValues[(int)(subscripts[0].as.number - 1)];
}



// varDefn->arrayValues[(int)(subscripts[0].as.number - 1)] = rhs;  // TODO fix this!  hardcoded to first subscript

bool setArrayValue(ArrayVariable* varDefn, Value* newvalue, Value subscripts[], char* errbuf, size_t errbuf_size) {
	// only process subscript 1 for initial implementation
	if (subscripts[0].type == VAL_ARRAY_STAR) {
		int boundsSubscript1 = calculateVarCount(varDefn->bounds[0].lBound, varDefn->bounds[0].uBound);
		for (int i = 0; i < boundsSubscript1; i++) {
			varDefn->arrayValues[i] = *newvalue;
		}

	}
	else {
		int subscript1 = (int)subscripts[0].as.number;
		ArrayBound bnd = varDefn->bounds[0];
		if (subscript1 < bnd.lBound || subscript1 > bnd.uBound) {
			snprintf(errbuf, errbuf_size, "Subscript value %d is not in array bounds between %d and %d", subscript1, bnd.lBound, bnd.uBound);
			return false;
		}
		varDefn->arrayValues[(int)(subscripts[0].as.number - 1)] = *newvalue;
	}

	return true;

}