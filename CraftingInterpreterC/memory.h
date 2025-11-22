#pragma once
#ifndef clox_memory_h
#define clox_memory_h

#include "common.h"
#include "object.h"

// added CH 19.3 page 347
#define ALLOCATE(type, count) \
    (type*)reallocate(NULL, 0, sizeof(type) * (count))

// added in Ch 19.5
#define FREE(type, pointer) reallocate(pointer, sizeof(type), 0)

#define GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity) * 2)

#define GROW_ARRAY(type, pointer, oldCount, newCount) \
    (type*)reallocate(pointer, sizeof(type) * (oldCount), \
        sizeof(type) * (newCount))

#define FREE_ARRAY(type, pointer, oldCount) \
    reallocate(pointer, sizeof(type) * (oldCount), 0)

void* allocate(size_t size);
void* reallocate(void* pointer, size_t oldSize, size_t newSize);

/*

//> Garbage Collection 
void markObject(Obj* object);
void markValue(Value value);
void collectGarbage();
*/

void freeObjects(); // Ch 19.5

#endif
