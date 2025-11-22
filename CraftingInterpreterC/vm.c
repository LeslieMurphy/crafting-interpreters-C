//> A Virtual Machine vm-c
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "object.h"
#include "memory.h"
#include "vm.h"
#include "native.h"
#include "array.h"

VM vm; // [one]

static void resetStack() {
	vm.stackTop = vm.stack;
	vm.frameCount = 0;  // added Ch 24
	// vm.openUpvalues = NULL;
}

static void runtimeError(const char* format, ...) {
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	fputs("\n", stderr);

	

	/* prior to Ch 24
	size_t instruction = vm.ip - vm.chunk->code - 1;
	int line = vm.chunk->lines[instruction];
	fprintf(stderr, "[line %d] in script\n", line);*/

	/* first part of Ch 24
	CallFrame* frame = &vm.frames[vm.frameCount - 1];
	size_t instruction = frame->ip - frame->function->chunk.code - 1;
	int line = frame->function->chunk.lines[instruction];
	fprintf(stderr, "[line %d] in script\n", line);*/

	// call stack
	for (int i = vm.frameCount - 1; i >= 0; i--) {
		CallFrame* frame = &vm.frames[i];
		ObjFunction* function = frame->function;
		// ObjFunction* function = frame->closure->function;
		
		size_t instruction = frame->ip - function->chunk.code - 1;
		fprintf(stderr, "[line %d] in ", // [minus]
			function->chunk.lines[instruction]);
		if (function->name == NULL) {
			fprintf(stderr, "script\n");
		}
		else {
			fprintf(stderr, "%s()\n", function->name->chars);
		}
	}
	
	resetStack();
}

// helper to define a native function - pg 461
static void defineNative(const char* name, NativeFn function) {
	push(OBJ_VAL(copyString(name, (int)strlen(name))));
	push(OBJ_VAL(newNative(function)));
	tableSet(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
	pop();
	pop();
}

void initVM() {
	////> call-reset-stack
	resetStack();
	vm.objects = NULL; // Ch 19.5 page 353
	
	// vm.strings is for string interning - a unique place for each string so we can compare equality by a simple pointer compare
	// this table is more of a hashset - the entries themselves are not used
	initTable(&vm.strings); // Ch 20.5 used for String interning - unique place for each string so we can compare equality

	initTable(&vm.globals); // ch 21.2 for global vars pg 390
	initTable(&vm.globalArrayVars); 
	
	//typedef struct {
	//	ArrayVariable* arrayVars[MAXARRAYVARIABLES];
	//	int arrayVarCount;
	//} ArrayVariables;

	vm.arrayVarList.arrayVarCount = 0;
	
	defineNative("clock", clockNative);

	vm.instructionCount = 0;
	vm.pushCount = 0;
	vm.popCount = 0;

	srand((unsigned int)time(NULL)); // seed rng

	//// GC
	//vm.bytesAllocated = 0;
	//vm.nextGC = 1024 * 1024;
	//vm.grayCount = 0;
	//vm.grayCapacity = 0;
	//vm.grayStack = NULL;
	
	//// initString
	//vm.initString = NULL;
	//vm.initString = copyString("init", 4);

}

void freeVM() {
	debugPrintTable(&vm.strings, "Strings", true);
	printf("\n");
	debugPrintTable(&vm.globals, "Globals", false);
	printf("\n");
	freeTable(&vm.strings); // Ch 20.5
	freeTable(&vm.globals); // ch 21.2
	freeTable(&vm.globalArrayVars);
	freeObjects();  // Ch 19.5 

}

// added in Ch 15.2.1
void push(Value value) {
	vm.pushCount++;
	*vm.stackTop = value;
	// in debugger we can use a watch ==> (ObjString*) value.as.obj
	/*if (IS_OBJ(value)) {
		Obj* o = value.as.obj;
		ObjString* s = (void*) o;
		printf("String value is %.*s", s->length, s->chars);
	}*/
	
	vm.stackTop++;
}

// Added by Les 11/11/25 for optimization binary op
void discardMultipleItemsFromStack(int distance) {
	vm.stackTop = vm.stackTop - distance;  // reset upwards in stack without popping anything
}

Value pop() {
	vm.popCount++;
	vm.stackTop--;
	return *vm.stackTop;
}

static Value peek(int distance) {
	return vm.stackTop[-1 - distance];
}

// Ch 24 pg 453
// Initialize new CallFrame on the stack
static bool call(ObjFunction* function, int argCount) {
	if (argCount != function->arity) {
		runtimeError("Expected %d arguments but got %d.",
			function->arity, argCount);
		return false;
	}

	if (vm.frameCount == FRAMES_MAX) {
		runtimeError("Stack overflow.");
		return false;
	}

	CallFrame* frame = &vm.frames[vm.frameCount++];
	frame->function = function;
	frame->ip = function->chunk.code;

	/*frame->closure = closure;
	frame->ip = closure->function->chunk.code;*/

	// line up arguments on the stack - in effect binding them
	frame->slots = vm.stackTop - argCount - 1;
	return true;
}

static bool callValue(Value callee, int argCount) {
	if (IS_OBJ(callee)) {
		switch (OBJ_TYPE(callee)) {
		case OBJ_FUNCTION: 
			return call(AS_FUNCTION(callee), argCount);

		//case OBJ_BOUND_METHOD: {
		//	ObjBoundMethod* bound = AS_BOUND_METHOD(callee);
		//	vm.stackTop[-argCount - 1] = bound->receiver;
		//	return call(bound->method, argCount);
		//}
		//case OBJ_CLASS: {
		//	ObjClass* klass = AS_CLASS(callee);
		//	vm.stackTop[-argCount - 1] = OBJ_VAL(newInstance(klass));
		//	//> Methods and Initializers call-init
		//	Value initializer;
		//	if (tableGet(&klass->methods, vm.initString,
		//		&initializer)) {
		//		return call(AS_CLOSURE(initializer), argCount);
		//		//> no-init-arity-error
		//	}
		//	else if (argCount != 0) {
		//		runtimeError("Expected 0 arguments but got %d.",
		//			argCount);
		//		return false;
		//		//< no-init-arity-error
		//	}
		//	//< Methods and Initializers call-init
		//	return true;
		//}
		//			  //< Classes and Instances call-class
		//			  //> Closures call-value-closure
		//case OBJ_CLOSURE:
		//	return call(AS_CLOSURE(callee), argCount);
		//	//< Closures call-value-closure
		//	/* Calls and Functions call-value < Closures call-value-closure
		//		  case OBJ_FUNCTION: // [switch]
		//			return call(AS_FUNCTION(callee), argCount);
		//	*/
		//	//> call-native
		case OBJ_NATIVE: {
			NativeFn native = AS_NATIVE(callee);
			Value result = native(argCount, vm.stackTop - argCount);
			vm.stackTop -= argCount + 1;
			push(result);
			return true;
		}
		default:
			break; // Non-callable object type.
		}
	}
	runtimeError("Can only call functions and classes.");
	return false;
}

// ch 18.4.1 pg 337
static bool isFalsey(Value value) {
	return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

// ch 19.4.1 pg 350
static void concatenate() {
	ObjString* b = AS_STRING(pop());
	ObjString* a = AS_STRING(pop());
	
	//> Garbage Collection concatenate-peek
	// LLM ObjString* b = AS_STRING(peek(0));
	// LLM  ObjString* a = AS_STRING(peek(1));
	//< Garbage Collection concatenate-peek

	int length = a->length + b->length;
	char* chars = ALLOCATE(char, length + 1);
	memcpy(chars, a->chars, a->length);
	memcpy(chars + a->length, b->chars, b->length);
	chars[length] = '\0';

	ObjString* result = takeString(chars, length);
	
	/* LLM 
	
	//> Garbage Collection concatenate-pop
	pop();
	pop();
	//< Garbage Collection concatenate-pop
	*/
	push(OBJ_VAL(result));  // cH 19.4.1
}

/* for debugging can add this to macro
      Value v0 = vm.stackTop[-1]; \
	  Value v1 = vm.stackTop[-2]; \
	  printf("V0 type is %d v1 type is %d\n", v0.type, v1.type); \
	  printf("V0 value is %f \n", v0.as.number); \
	  printf("V1 value is %f \n", v1.as.number); \
*/

// new BINARY_OP for ch 18
#define BINARY_OP(valueType, op) \
    do { \
      if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
        runtimeError("Operands must be numbers."); \
        return INTERPRET_RUNTIME_ERROR; \
      } \
      double b = AS_NUMBER(pop()); \
      double a = AS_NUMBER(pop()); \
      push(valueType(a op b)); \
    } while (false)


static InterpretResult binaryAdd() {
	printf("binaryAdd\n");
	BINARY_OP(NUMBER_VAL, +);
	return INTERPRET_OK;
}

// return an integer value converted to double
double randomNumber(double a, double b) { 
	int min = (int)a;
	int max = (int)b;
	if (min > max) {
		int temp = min;
		min = max;
		max = temp;
	}

	int randNum = min + rand() % (max - min + 1);
	// printf("random between %d and %d is %d\n", min, max, randNum);
	return (double)randNum;
}

static Value valueMemoize;

static InterpretResult run() {
	
	valueMemoize.type = VAL_NIL;

	CallFrame* frame = &vm.frames[vm.frameCount - 1];  // Added Ch 24
#define READ_BYTE() (*frame->ip++)
#define READ_CONSTANT() \
	(frame->function->chunk.constants.values[READ_BYTE()])
//#define READ_CONSTANT() \
//    (frame->closure->function->chunk.constants.values[READ_BYTE()])
#define READ_SHORT() \
    (frame->ip += 2, \
    (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))

//#define READ_BYTE() (*vm.ip++)
//#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
//#define READ_SHORT() \
//    (vm.ip += 2, (uint16_t)((vm.ip[-2] << 8) | vm.ip[-1]))

#define READ_STRING() AS_STRING(READ_CONSTANT())

	printf("\nExecution VM trace:\n");
	debugPrintTable(&vm.strings, "Strings", true);
	printf("\n");
	debugPrintTable(&vm.globals, "Globals", false);
	printf("\n");

	for (;;) {
		vm.instructionCount++;

		/*disassembleInstruction(&frame->function->chunk,
			(int)(frame->ip - frame->function->chunk.code));*/

#ifdef DEBUG_TRACE_EXECUTION
		printf("** Stack trace ***\n");
		printf("          ");
		for (Value* slot = vm.stack; slot < vm.stackTop; slot++) {
			printf("[ ");
			printValue(*slot);
			printf(" ]");
		}
		printf("\n");
		disassembleInstruction(&frame->function->chunk,
			(int)(frame->ip - frame->function->chunk.code));
		/* prior to Ch 24
		disassembleInstruction(vm.chunk,
			(int)(vm.ip - vm.chunk->code));*/
#endif
		uint8_t instruction;
		switch (instruction = READ_BYTE()) {


		case OP_INVALID:
			printf(" ** FATAL ERROR BAD BYTECODE **\n");
			return INTERPRET_RUNTIME_ERROR;

			// Ch 23.1.1 pg 418 (jump forward)
		case OP_JUMP: {
			uint16_t offset = READ_SHORT();
			//vm.ip += offset;
			frame->ip += offset;
			break;
		}

					// Ch 23.1 pg 416
		case OP_JUMP_IF_FALSE: {
			uint16_t offset = READ_SHORT();
			//if (isFalsey(peek(0))) vm.ip += offset;
			if (isFalsey(peek(0))) frame->ip += offset;
			break;
		}

							 // ch 23.3 pg 423 for while (loop backwards)
		case OP_LOOP: {
			uint16_t offset = READ_SHORT();
			//vm.ip -= offset;
			frame->ip -= offset;
			break;
		}

		case OP_CALL: {
			// function call pg 452
			int argCount = READ_BYTE();
			if (!callValue(peek(argCount), argCount)) {
				return INTERPRET_RUNTIME_ERROR;
			}
			// Next VM instruction will start running the function 
			frame = &vm.frames[vm.frameCount - 1];
			break;
		}

					/* removed on ch 21 pg 384
					case OP_RETURN: {
						printValue(pop()); <<==
						printf("\n"); <<==
						return INTERPRET_OK;
					}*/

					// Les 11/8/25 - needed to include OP_RETURN around page 385 to avoid issues
					// see also https://github.com/munificent/craftinginterpreters/issues/877
		case OP_RETURN: {
			// return INTERPRET_OK; // prior to Ch 24

			// New logic on pg 456 24.5.4
			// pop the function's result so we can hold onto it
			Value result = pop();
			// closeUpvalues(frame->slots);
			vm.frameCount--;
			if (vm.frameCount == 0) {
				pop();
				return INTERPRET_OK;
			}
			// discard the slots used by callee for temps and the arg parameters
			vm.stackTop = frame->slots;
			// function return now goes to top of stack
			push(result);
			frame = &vm.frames[vm.frameCount - 1];
			break;
		}

					  // case OP_NEGATE:   push(-pop()); break; // ch17
		case OP_NEGATE:
			if (!IS_NUMBER(peek(0))) {
				runtimeError("Operand must be a number.");
				return INTERPRET_RUNTIME_ERROR;
			}
			push(NUMBER_VAL(-AS_NUMBER(pop())));
			break;

		case OP_CONSTANT: { // Added in ch 18.4
			Value constant = READ_CONSTANT();
			/*printf("Debug: Got constant: ");
			printValue(constant);
			printf("\n");
			*/
			/* A Virtual Machine op-constant < A Virtual Machine push-constant
					printValue(constant);
					printf("\n");
			*/
			//> push-constant
			push(constant);
			//< push-constant
			break;
		}

						/* CH 17 version
						case OP_ADD:      BINARY_OP(+); break;
						case OP_SUBTRACT: BINARY_OP(-); break;
						case OP_MULTIPLY: BINARY_OP(*); break;
						case OP_DIVIDE:   BINARY_OP(/); break;*/


						// case OP_ADD:		BINARY_OP(NUMBER_VAL, +); break; // ch 18 version page 333
						// ch 19.4.1 pg 351 string concat
		case OP_ADD: {
			// printf("OP ADD\n");
			if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
				concatenate();
			}
			else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
				/* Unoptimized version
				double b = AS_NUMBER(pop());
				double a = AS_NUMBER(pop());
				push(NUMBER_VAL(a + b));*/

				/* optimized version eliminates 2 stack movements - LLM 11/11/25 */
				double b = ((peek(0)).as.number);
				double a = ((peek(1)).as.number);

				// first optimization approach puts a Value back on stack without popping the two operands

				/*
				discardMultipleItemsFromStack(2);
				push(((Value) {
					VAL_NUMBER, { .number = a + b }
				}));*/

				// second optimization leverages that that slot is already a number, so we just overlay the LH operand number in place!
				discardMultipleItemsFromStack(1);  // a is now top of stack; will overlay it's value
				//				double bPrime = ((peek(0)).as.number);
				(*(vm.stackTop - 1)).as.number = a + b;  // should really create a new stack operation for this - overlayTopStackNumberValue
				//				double bPrime2 = ((peek(0)).as.number);
								// printf("done");

			}
			else {
				runtimeError(
					"Operands must be two numbers or two strings.");
				return INTERPRET_RUNTIME_ERROR;
			}
			break;
		}

				   // case OP_SUBTRACT:	BINARY_OP(NUMBER_VAL, -); break;  unoptimized
		case OP_SUBTRACT: {
			if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
				double b = ((peek(0)).as.number);
				double a = ((peek(1)).as.number);
				discardMultipleItemsFromStack(1);
				(*(vm.stackTop - 1)).as.number = a - b;
			}
			else {
				runtimeError(
					"Operands for subtract must be two numbers.");
				return INTERPRET_RUNTIME_ERROR;
			}
			break;
		}

		case OP_MULTIPLY:	BINARY_OP(NUMBER_VAL, *); break;


		case OP_DIVIDE:		BINARY_OP(NUMBER_VAL, / ); break;

			// new ch 18.4.1 pg 336
		case OP_NOT:
			push(BOOL_VAL(isFalsey(pop())));
			break;

			// new ch 21.1.1 pg 384
		case OP_PRINT: {

			// very fast for benchmarking
			/* for (int i = 0; i < 10000000; i++) {
				Value x = peek(0);
			}
			for (int i = 0; i < 1000000000; i++) {
				Value result = pop();
				push(result);
			}
			*/

			printValue(pop());
			printf("\n");
			break;
		}

					 // new ch 18.4 pg 335
		case OP_NIL:		push(NIL_VAL); break;
		case OP_TRUE:		push(BOOL_VAL(true)); break;
		case OP_FALSE:		push(BOOL_VAL(false)); break;

		case OP_POP: pop(); break;  // ch 21.1.2 pg 386

		case OP_GET_GLOBAL: { // ch 21.3
			ObjString* name = READ_STRING();
			//if (strcmp(name->chars, "fib") == 0) {
			//	if (valueMemoize.type != VAL_NIL) {
			//		// printf("using fib memoize for %s\n", name->chars);
			//		push(valueMemoize);
			//		break;
			//	}
			//	else {
			//		// printf("save fib memoize for %s\n", name->chars);
			//		Value value;
			//		if (!tableGet(&vm.globals, name, &value)) {
			//			runtimeError("Undefined variable '%s'.", name->chars);
			//			return INTERPRET_RUNTIME_ERROR;
			//		}
			//		push(value);
			//		valueMemoize = value;
			//		break;
			//	}
			//	
			//}
			//else {

			/* the real code! */
			// printf("get global for %s\n", name->chars);
			Value value;
			if (!tableGet(&vm.globals, name, &value)) {
				runtimeError("Undefined variable '%s'.", name->chars);
				return INTERPRET_RUNTIME_ERROR;
			}
			push(value);
			break;

		}

		case OP_SET_GLOBAL: { // Ch 21.4 pg 393
			ObjString* name = READ_STRING();
			if (tableSet(&vm.globals, name, peek(0))) {
				// if the global variable was not defined, remove it from the globals for REPL session, and report error
				tableDelete(&vm.globals, name); // [delete]
				runtimeError("Undefined variable '%s'.", name->chars);
				return INTERPRET_RUNTIME_ERROR;
			}
			break;
		}
		case OP_GET_GLOBAL_ARRAY: { // get a value or set of values from an Array element based on subscripts
			ObjString* name = READ_STRING();
			Value value = NIL_VAL;

			printf("get global array for %s\n", name->chars);
			if (!tableGet(&vm.globalArrayVars, name, &value)) {
				runtimeError("Undefined variable '%s'.", name->chars);
				return INTERPRET_RUNTIME_ERROR;
			}

			/*
			TODO lookup the name in the global strings
// get the ptr to the definition
// Confirm subscripts match
// index into the vars and get the value
 handle *
 */
			ArrayVariable* varDefn;
			// Value arrayDefinition = { VAL_ARRAY_REF , .as.obj = varDefn };
			varDefn = (ArrayVariable*) value.as.obj;
			int dimensions = varDefn->dimensions;

			// int idOfArrayVar = READ_BYTE();
			int subscriptCount = READ_BYTE();
			
			// TODO the subscript can be * or a range such as 5:10

			if (subscriptCount == 0) {
				runtimeError("array var ref without any subscripts");
				return INTERPRET_RUNTIME_ERROR;
			}

			if (subscriptCount != dimensions) {
				runtimeError("array subscripts do not match array dimensions");
				return INTERPRET_RUNTIME_ERROR;
			}
			
			Value subscripts[MAXARRAYDIMENSIONS];

			for (int i = 0; i < subscriptCount; i++) {
				Value temp = peek(i - i);
				subscripts[i] = peek(i - i);
				printf("Subscript % d is ", i);
				printValue(subscripts[i]);
			}
			printf("\n");

			// TODO optimize so we just do the pops no peeks
			for (int i = 0; i < subscriptCount; i++) {
				pop();
			}

			// varDefn->arrayValues[2] = fake;
			
			//value = varDefn->arrayValues[(int) (subscripts[0].as.number - 1) ];  // TODO fix this!
			//push(value);


			// getArrayValue(ArrayVariable* varDefn, Value subscripts[], char* errbuf, size_t errbuf_size);
			char err_buffer[200];
			Value* valuePtr;
			valuePtr = getArrayValue(varDefn, &subscripts, err_buffer, sizeof(err_buffer));
			if (valuePtr == NULL) {
				runtimeError(err_buffer);
				return INTERPRET_RUNTIME_ERROR;
			}
			push(*valuePtr);
			


			// TODO the bounds are offsets so subtract 1
			// TODO multiple dimensions?
			// TODO * get would need to return an array slice

		
			

			
			
			//if (!tableGet(&vm.globals, name, &value)) {
			//	runtimeError("Undefined variable '%s'.", name->chars);
			//	return INTERPRET_RUNTIME_ERROR;
			//}
			
			break;

		}

		case OP_SET_GLOBAL_ARRAY: { // Ch 21.4 pg 393
			// 0004    | OP_SET_GLOBAL_ARRAY    1 Subscripts=1
			
			// (frame->function->chunk.constants.values[READ_BYTE()])

			// for globalArrayVars the Value will not be the scalar value
			//  instead it is a Object that points into our Array struct

			//  READ_STRING  ((ObjString*)(((frame->function->chunk.constants.values[(*frame->ip++)])).as.obj))
			uint8_t* frame_ip = frame->ip;
			ObjString* name = READ_STRING();
			Value value = NIL_VAL;

			printf("set global array for %s\n", name->chars);
			Value rhs = peek(0);

			// For regular globals, the vm.globals is a dynamic lookup to an entry, and we simply set its Value to the rhs contents
			// For array globals, we could  have a rhs that is in itself an array, such as B(1:5) or B(*)
			// 
			// Initial logic
			//   globalArrayVars when being defined takes a Value object that is what defines the bounds of the array and the pointer to its storage location as an array of Values
			//   here in SET (or in GET) we need to pull out the definition, apply index logic, and locate the actual Value(s)

			//if (tableSet(&vm.globalArrayVars, name, peek(0))) {
			//	tableDelete(&vm.globalArrayVars, name); 
			//	runtimeError("Undefined global array variable '%s'.", name->chars);
			//	return INTERPRET_RUNTIME_ERROR;
			//}

			if (!tableGet(&vm.globalArrayVars, name, &value)) {
				runtimeError("Undefined global array variable '%s'.", name->chars);
				return INTERPRET_RUNTIME_ERROR;
			}

			ArrayVariable* varDefn = (ArrayVariable*)value.as.obj;
			int dimensions = varDefn->dimensions;
						
			uint8_t start_rhs_ip = READ_BYTE(); // new so we can handle star assigment e.g. var a(10); a(*) = 1?100;
			int subscriptCount = READ_BYTE();

			if (subscriptCount == 0) {
				runtimeError("array var ref without any subscripts");
				return INTERPRET_RUNTIME_ERROR;
			}

			printf("there are %d subscripts.  Rhs ip start is %d\n", subscriptCount, start_rhs_ip);

			
			if (subscriptCount != dimensions) {
				runtimeError("array subscripts do not match array dimensions");
				return INTERPRET_RUNTIME_ERROR;
			}

			Value subscripts[MAXARRAYDIMENSIONS];

			for (int i = 0; i < subscriptCount; i++) {
				subscripts[i] = peek(subscriptCount - i);
				printf("Subscript % d is ", i);
				printValue(subscripts[i]);
				printf("\n");
			}


			//varDefn->arrayValues[(int)(subscripts[0].as.number - 1)] = rhs;  // TODO fix this!  hardcoded to first subscript
			char err_buffer[200];
			bool success = setArrayValue(varDefn, &rhs, &subscripts, &err_buffer, sizeof(err_buffer));
			if (!success) {
				runtimeError(err_buffer);
				return INTERPRET_RUNTIME_ERROR;
			}

			
			// we have trouble here because we need to pop the subscripts but the very top of stack is RH side!

			// TODO optimize so we just do the pops no peeks
			//for (int i = 0; i < subscriptCount; i++) {
			//	pop();
			//}

			break;
		}

	

		case OP_DEFINE_GLOBAL: { // ch 21.2
			ObjString* name = READ_STRING();
			Value rhs = peek(0);  // will be NIL if there is no assignment
			tableSet(&vm.globals, name, peek(0));
			pop();
			break;
		}

		case OP_DEFINE_GLOBAL_ARRAY: { 
			ObjString* name = READ_STRING();
			// Initializer not present set each Array element to nil?
			Value rhs = peek(0); // TODO handle an initializer on an array.  

			if (vm.arrayVarList.arrayVarCount > MAXARRAYVARIABLES) error("Too many global array variables");

			int subscriptCount = READ_BYTE();
			int varCount = READ_SHORT(); // TODO support larger arrays
			printf("Global var %s with subscript count %d total variable count %d\n", name->chars, subscriptCount, varCount);
			
			
			
			// TODO get the bounds and each bound dimensions from our bytecode 
			
			//ArrayVariable* varDefn = allocateArrayVar(bounds);
			//vm.arrayVarList.arrayVars[vm.arrayVarList.arrayVarCount] = varDefn;
			
			ArrayVariable* varDefn = allocateNewArrayVar(&vm.arrayVarList, subscriptCount, varCount);
			varDefn->variableName = name->chars;
			varDefn->dimensions = subscriptCount;
			for (int i = 0; i < subscriptCount; i++) {
				short lbound = READ_SHORT();
				short ubound = READ_SHORT();
				printf("Definition for subscript %d is %d:%d\n", i, lbound, ubound);
				varDefn->bounds[i].lBound = lbound;
				varDefn->bounds[i].uBound = ubound;
			}
			


			Value arrayDefinition = { VAL_ARRAY_REF , .as.obj = varDefn };
			// the lookup of the global variable name for the array will get us a pointer back to the definition
			tableSet(&vm.globalArrayVars, name, arrayDefinition); 

			// temp code
			Value fake2 = { VAL_NUMBER, .as.number = 123 };
			varDefn->arrayValues[2] = fake2;
			Value fake3 = { VAL_NUMBER, .as.number = 456 };
			varDefn->arrayValues[3] = fake3;
				// varDefn->arrayValues[2];
			pop();
			break;
		}




							 // Ch 22.4.1 pg 409 
		case OP_GET_LOCAL: {
			uint8_t slot = READ_BYTE();
			//push(vm.stack[slot]); // copy the variable from deeper in the stack onto the top for use
			// printf("getting local var from slot %d and putting it at top of stack.  Type is %d\n", slot, frame->slots[slot].type);
			push(frame->slots[slot]);
			break;
		}

		case OP_SET_LOCAL: {
			// Every assignment is also an Expression.  
			// Every expression produces a value.
			uint8_t slot = READ_BYTE();
			//vm.stack[slot] = peek(0);
			frame->slots[slot] = peek(0);
			// printf("set local var from top of stack back into slot %d. Type is %d\n", slot, frame->slots[slot].type);

			break;
		}

						 // new ch 18.4.2 pg 338
		case OP_EQUAL: {
			Value b = pop();
			Value a = pop();
			push(BOOL_VAL(valuesEqual(a, b)));
			break;
		}
		case OP_GREATER:  BINARY_OP(BOOL_VAL, > ); break;
		//case OP_LESS:     BINARY_OP(BOOL_VAL, < ); break; // unoptimized
		
		case OP_LESS:     {
			if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
				double b = ((peek(0)).as.number);
				double a = ((peek(1)).as.number);
				discardMultipleItemsFromStack(1);
				(*(vm.stackTop - 1)).as.boolean = a < b;
				(*(vm.stackTop - 1)).type = VAL_BOOL;  // turn the number on the stack into a Bool in place
			}
			else {
				runtimeError(
					"Operands for less than must be two numbers.");
				return INTERPRET_RUNTIME_ERROR;
			}
			break;
		}
		case OP_RANDOM: {
			if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
				double b = ((peek(0)).as.number);
				double a = ((peek(1)).as.number);
				discardMultipleItemsFromStack(1);
				(*(vm.stackTop - 1)).as.number = randomNumber(a, b);
			}
			else {
				runtimeError(
					"Operands for random must be two numbers.");
				return INTERPRET_RUNTIME_ERROR;
			}
			break;
		}
					

		}
	}
}

/* used for initial ch 15 - hardcoded bytecode chunks 
InterpretResult interpretChunk(Chunk* chunk) {
	vm.chunk = chunk;
	vm.ip = vm.chunk->code;
	return run();
}
*/


/* Virtual Machine interpreter */
InterpretResult interpret(const char* source) {
	//compile(source);
	//return INTERPRET_OK;

	ObjFunction* function = compile(source);
	if (function == NULL) return INTERPRET_COMPILE_ERROR;

	// resetStack(); // added LLM 11/10/25

	push(OBJ_VAL(function));  // stack slot zero used to hold the pointer to the main ("SCRIPT") wrapper function
	//  -- this breaks the runtime stacktrace !!!!!

	call(function, 0);  // pg 453 - set up first frame for top-level code.  Needed to remove code from pg 445

	return run();

	//ObjClosure* closure = newClosure(function);
	//pop();
	//push(OBJ_VAL(closure));
	//call(closure, 0);

	/* from 24.3.3 pg 445  - removed on page 453
	CallFrame* frame = &vm.frames[vm.frameCount++];
	frame->function = function;
	frame->ip = function->chunk.code;
	frame->slots = vm.stack;
	*/
	
	/* version prior to Ch 24
	Chunk chunk;
	initChunk(&chunk);

	if (!compile(source, &chunk)) {
		freeChunk(&chunk);
		return INTERPRET_COMPILE_ERROR;
	}

	vm.chunk = &chunk;
	vm.ip = vm.chunk->code;*/

	

	/* Prior to Ch 24
	InterpretResult result = run();
	freeChunk(&chunk);
	return result;*/

}

#undef READ_BYTE
#undef READ_CONSTANT
#undef READ_SHORT
#undef READ_STRING
#undef BINARY_OP//////////////////////////