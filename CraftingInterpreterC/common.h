#pragma once
//> Chunks of Bytecode common-h
#ifndef clox_common_h
#define clox_common_h

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
//> A Virtual Machine define-debug-trace

//> Optimization define-nan-boxing
#define NAN_BOXING
//< Optimization define-nan-boxing
//> Compiling Expressions define-debug-print-code
#define DEBUG_PRINT_CODE

// to get stack trace at each step in VM
// #define DEBUG_TRACE_EXECUTION

//> Garbage Collection define-stress-gc

#define DEBUG_STRESS_GC
//< Garbage Collection define-stress-gc
//> Garbage Collection define-log-gc
#define DEBUG_LOG_GC
//< Garbage Collection define-log-gc
//> Local Variables uint8-count

// this is the limit for Local variables - 1 byte worth Pg 401
#define UINT8_COUNT (UINT8_MAX + 1)


#endif
//> omit
// In the book, we show them defined, but for working on them locally,
// we don't want them to be.
// #undef DEBUG_PRINT_CODE
// #undef DEBUG_TRACE_EXECUTION
#undef DEBUG_STRESS_GC
#undef DEBUG_LOG_GC
//< omit
