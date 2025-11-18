//#include "common.h"
//#include "compiler.h"
//#include "debug.h"
// #include "object.h"
//#include "memory.h"
//#include "vm.h"
//#include "value.h"
#include "native.h"
#include <time.h>


Value clockNative(int argCount, Value* args) {
	return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}