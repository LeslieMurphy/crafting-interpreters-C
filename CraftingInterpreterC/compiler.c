#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "compiler.h"
#include "memory.h"
#include "scanner.h"
#include "parseRules.h"
#include "local.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

// static void initCompiler(Compiler* compiler, FunctionType type) {
//ObjFunction* compile(const char* source) {

/*
typedef struct ClassCompiler {
    struct ClassCompiler* enclosing;
    bool hasSuperclass;
} ClassCompiler;
*/

#define MAX_GLOBAL_FUNCTIONS 5

static ObjFunction globalFunctions[MAX_GLOBAL_FUNCTIONS];  // todo is the ObjString pointer valid at time of use?
int globalFunctionCount;

Parser parser;
Compiler* current = NULL;

static void expression();
static void declaration();
static void statement();
static void parsePrecedence(Precedence precedence);
static void consume(TokenType type, const char* message);
static uint8_t identifierConstant(Token* name);
static void emitBytes(OpCode byte1, uint8_t byte2);
static uint8_t makeConstant(Value value);
static bool check(TokenType type);
static bool match(TokenType type);
static bool parseIntSlice(const char* ptr, int len, int* outValue);

// ch 21.2 pg 389
// take token and and lexeme to chunk constant table as string
// return index of the constant - to lookup the variable in future usages

static uint8_t identifierConstant(Token* name) {
    return makeConstant(OBJ_VAL(copyString(name->start, name->length)));
}

static uint8_t parseVariable(const char* errorMessage) {
    consume(TOKEN_IDENTIFIER, errorMessage);

    // If we have a global, there is no need to add variable name to the locals lookup table
    // We late bind globals ...
    if (current->scopeDepth != 0) { 
        // for local var support ch 22.1 pg 404 void declareLocalVariable(Compiler* current, Token* localVarToken) 
        declareLocalVariable(current, &parser.previous);
    }
   
    return identifierConstant(&parser.previous);
}

static void markInitialized() {
    if (current->scopeDepth == 0) return; // if top level function, there are no local vars only global pg 447
    current->locals[current->localCount - 1].depth = current->scopeDepth;
}

// Emit the VM opcode to define global variable now that it becomes available for use
// for local just mark it as initialized
static void defineVariable(OpCode opcode, uint8_t globalVarSlot) {

    // if in local scope, the local var is already on top of stack - see pg 404
    if (current->scopeDepth > 0) {
        markInitialized(); // pg 411
        return;
    } 
    emitBytes(opcode, globalVarSlot); // ch 21.2 pg 389
}

// Ch 24 pg 450
static uint8_t argumentList() {
    uint8_t argCount = 0;
    if (!check(TOKEN_RIGHT_PAREN)) {
        do {
            expression();
            if (argCount == 255) {
                error("Can't have more than 255 arguments.");
            }
            argCount++;
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
    return argCount;
}



// les these now in parseRules.h

//typedef enum {
//    PREC_NONE,
//    PREC_ASSIGNMENT,  // =
//    PREC_OR,          // or
//    PREC_AND,         // and
//    PREC_EQUALITY,    // == !=
//    PREC_COMPARISON,  // < > <= >=
//    PREC_TERM,        // + -
//    PREC_FACTOR,      // * /
//    PREC_UNARY,       // ! -
//    PREC_CALL,        // . ()
//    PREC_PRIMARY
//} Precedence;

//// typedef void (*ParseFn)(bool canAssign);
//typedef void (*ParseFn)();
//
//typedef struct {
//    ParseFn prefix;
//    ParseFn infix;
//    Precedence precedence;
//} ParseRule;

Chunk* compilingChunk;

static Chunk* currentChunk() {
    // return compilingChunk;  prior to Ch 24
    return &current->function->chunk;
}

static void errorAt(Token* token, const char* message) {
    if (parser.panicMode) return;
    parser.panicMode = true;
    fprintf(stderr, "[line %d] Error", token->line);

    if (token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    }
    else if (token->type == TOKEN_ERROR) {
        // Nothing.
    }
    else {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
    parser.hadError = true;
}

void error(const char* message) {
    errorAt(&parser.previous, message);
}

static void errorAtCurrent(const char* message) {
    errorAt(&parser.current, message);
}

static void advance() {
    parser.previous = parser.current;

    for (;;) {
        parser.current = scanToken();
        //printf("current token type is %d\n", parser.current.type);
        if (parser.current.type != TOKEN_ERROR) break;

        errorAtCurrent(parser.current.start);
    }
}

static void consume(TokenType type, const char* message) {
    if (parser.current.type == type) {
        advance();
        return;
    }
    errorAtCurrent(message);
}

// added in Ch 21.1.1
static bool check(TokenType type) {
   /* if (parser.current.type == TOKEN_EOF)
        printf("compiler check: current token type is EOF -  expecting %d\n", type);
    else
        printf("compiler check: current token type is %d expecting %d\n", parser.current.type, type);*/
    return parser.current.type == type;
}

static bool match(TokenType type) {
    if (!check(type)) return false;
    advance();
    return true;
}

static void emitByte(uint8_t byte) {
    writeChunk(currentChunk(), byte, parser.previous.line);
}

static void emitShort(short val) {
    emitByte((val >> 8) & 0xff);
    emitByte(val & 0xff);
}

static void emitBytes(OpCode byte1, uint8_t byte2) {
    emitByte(byte1);
    emitByte(byte2);
}

static int emitJump(OpCode instruction) {
    emitByte(instruction);
    emitByte(0xff);
    emitByte(0xff);
    return currentChunk()->count - 2;
}

// conditionally jump backwards - used for while Ch 23.3 pg 423
static void emitLoop(int loopStart) {
    emitByte(OP_LOOP);

    int offset = currentChunk()->count - loopStart + 2;
    if (offset > UINT16_MAX) error("Loop body too large.");

    emitByte((offset >> 8) & 0xff);
    emitByte(offset & 0xff);
}

//  return without any value - function without return statement
static void emitReturn() {
    emitByte(OP_NIL); // push nil onto stack during return from function - pg 457

  /*  if (current->type == TYPE_INITIALIZER) {
        emitBytes(OP_GET_LOCAL, 0);
    }
    else {
        emitByte(OP_NIL);
    }*/

    emitByte(OP_RETURN);
}

static uint8_t makeConstant(Value value) {
    int constant = addConstant(currentChunk(), value);
    if (constant > UINT8_MAX) {
        error("Too many constants in one chunk.");
        return 0;
    }

    return (uint8_t)constant;
}


static void emitConstant(Value value) {
    emitBytes(OP_CONSTANT, makeConstant(value));
}

static void patchJump(int offset) {
    // -2 to adjust for the bytecode for the jump offset itself.
    int jump = currentChunk()->count - offset - 2;

    if (jump > UINT16_MAX) {
        error("Too much code to jump over.");
    }

    currentChunk()->code[offset] = (jump >> 8) & 0xff;
    currentChunk()->code[offset + 1] = jump & 0xff;
}

// FunctionType added in Ch 24.2 pg 436
// The main program is in a dummy function of TYPE_SCRIPT, any program code functions are TYPE_FUNCTION
static void initCompiler(Compiler* compiler, FunctionType type) { 
    compiler->enclosing = current;  // link to parent instance ch 24.4.1 pg 448
    compiler->localCount = 0;
    compiler->scopeDepth = 0;
    compiler->function = newFunction();  // pg 437
    compiler->type = type;
    
    current = compiler;
    if (type != TYPE_SCRIPT) {
        current->function->name = copyString(parser.previous.start,
            parser.previous.length);
    }

    // Reserve stack slot zero for VM internal use ch 24.2.1 pg 438
    Local* local = &current->locals[current->localCount++];
    local->depth = 0;
    local->name.start = "";  //  name is empty so user can't refer to it
    local->name.length = 0;

    /*if (type != TYPE_SCRIPT) {
        current->function->name = copyString(parser.previous.start,
            parser.previous.length);
        Local* local = &current->locals[current->localCount++];
        local->depth = 0;
        local->isCaptured = false;
        // more ... if (type != TYPE_FUNCTION) {
    }*/

   
    /* Calls and Functions init-function-slot < Methods and Initializers slot-zero
      local->name.start = "";
      local->name.length = 0;
    */
    ////> Methods and Initializers slot-zero
    //if (type != TYPE_FUNCTION) {
    //    local->name.start = "this";
    //    local->name.length = 4;
    //}
    //else {
    //    local->name.start = "";
    //    local->name.length = 0;
    //}
}

static ObjFunction* endCompiler() {
    emitReturn();
    ObjFunction* function = current->function;

#ifdef DEBUG_PRINT_CODE
    if (!parser.hadError) {
        printf("clean compile! here is the bytecode\n");
        disassembleChunk(currentChunk(), function->name != NULL
            ? function->name->chars : "<script>");
    }
#endif
    
    current = current->enclosing;  // restore parent instance pg 448
    return function;
}

// Ch 22.2 pg 403
static void beginScope() {
    current->scopeDepth++;
}

// Ch 22.2 pg 403 and 407
static void endScope() {
    current->scopeDepth--;
   
    // Ch 22.3 Locals pg 407
    while (current->localCount > 0 &&
        current->locals[current->localCount - 1].depth >
        current->scopeDepth) {
        // TODO possible optimization POPN see page 407
        emitByte(OP_POP);
        //// Closures end-scope
        //if (current->locals[current->localCount - 1].isCaptured) {
        //    emitByte(OP_CLOSE_UPVALUE);
        //}
        //else {
        //    emitByte(OP_POP);
        //}
        ////< Closures end-scope
        current->localCount--;
    }
 
}

static void expression() {
    parsePrecedence(PREC_ASSIGNMENT); 
}

static void consumeInteger(char* message, int* outValue) {
    bool isNegative = match(TOKEN_MINUS);
    int value;
    consume(TOKEN_NUMBER, message);
    bool isValid = parseIntSlice(parser.previous.start, parser.previous.length, &value);
    if (!isValid) error(message);
    *outValue = isNegative ? -value : value;
}

// moved into array.c
//int calculateArraySize(int dimensions, int lBounds[MAXARRAYDIMENSIONS], int uBounds[MAXARRAYDIMENSIONS]) {
//    int varCount = 0;
//    varCount = 1 + abs(uBounds[0] - lBounds[0]); // -5 to -3 = len 3  -3 to 2 = len 6   2 to 5 len 4
//    for (int i = 1; i < dimensions; i++) {
//        int dimCount = 1 + abs(uBounds[0] - lBounds[0]);
//        varCount *= dimCount;
//    }
//    return varCount;
//}

#define MAXVARSINDECLARE 20


// 11/17/25 add support for multiple
// var x=66, a=33; print a;  print x;
// fun a() {var z=55, q=44+z; print z; print q;} 

static void varDeclaration() {
    // varSlot can be a slot in Globals if the declaration is outside a function
    //   in this case a OP_SET_GLOBAL is generated
    // or can be the slot on the local stack when declaring variables inside of a function

    // this will consume(TOKEN_IDENTIFIER, errorMessage);
    // and we need it to set up a array version if this is an array!
    OpCode vmDefineOpcode = OP_DEFINE_GLOBAL;
    uint8_t varNameSlot = parseVariable("Expect variable name."); // gets us a slot for the constant name for this var
    int dimensions = 0;
    int lBounds[MAXARRAYDIMENSIONS], uBounds[MAXARRAYDIMENSIONS];

    // is this an array declaration?
    if (match(TOKEN_LEFT_PAREN)) {
        vmDefineOpcode = OP_DEFINE_GLOBAL_ARRAY;
       
        
        do {
            int lBound, uBound = 0;
            bool hasUBound = false;
            dimensions++;
            if (dimensions > MAXARRAYDIMENSIONS)
                error("Can't define more than 3 array dimensions, sorry.");
            
            consumeInteger("Expect integer array bounds", &lBound);
            
            printf("Array bound %d dimension is %d\n", dimensions, lBound);
            if (match(TOKEN_COLON)) {
                consumeInteger("Expect integer upper array bounds", &uBound);
                printf("Array bound %d dimension lower bound %d upper %d\n", dimensions, lBound, uBound);
                if (lBound >= uBound) error("lower bound must be less than upper bound");
                hasUBound = true;
            }

            if (!hasUBound && lBound < 1) error("Array cannot have negative or zero size.");

            // Process the array bound.
            // Need to store this as an extended attribute on the Value object.
            if (hasUBound) {
                lBounds[dimensions - 1] = lBound;
                uBounds[dimensions - 1] = uBound;
            }
            else {
                lBounds[dimensions - 1] = 1;
                uBounds[dimensions - 1] = lBound;
            }
            
            
            
        } while (match(TOKEN_COMMA));

        consume(TOKEN_RIGHT_PAREN,
            "Expect ')' after array bounds.");
        // Allocate memory for the Array
        // Setup a struct that defines the array
        // Set the lookup value in the Global Arrays dict to point to our array definition

    }

    


    int numVariablesDefined = 0;

    if (match(TOKEN_EQUAL)) {
        expression();
    }
    else {
        emitByte(OP_NIL);
    }

    // note that defineVariable will define a global; for a local it marks it as initialized
    defineVariable(vmDefineOpcode, varNameSlot);

    

    if (vmDefineOpcode == OP_DEFINE_GLOBAL_ARRAY) {
        int varCount = calculateArraySize(dimensions, lBounds, uBounds);

        emitByte(dimensions); // provide the runtime with the # subscripts
        emitShort(varCount);  // provide the runtime with count of Values needed 
        
        for (int i = 0; i < dimensions; i++) {
            emitShort(lBounds[i]);
            emitShort(uBounds[i]);
        }

        // TODO 11/21/25 - the bytecode should be self-sufficient
        // do NOT want coupling to the compile time structures

        // bytecode
        //  OP_DEFINE_GLOBAL_ARRAY
        //  # subscripts
        //  2 byte short lowbound 1
        //  2 byte short upbound 1
        // repeat for additional subscripts
        //  then next opCode
    }
  
    // Are there more variables to declare?
    // TODO oops can't declare arrays here yet !
    if (match(TOKEN_COMMA)) {
        do {
            if (numVariablesDefined++ > MAXVARSINDECLARE)
                error("Can't define more than 20 variables at a time, sorry.");
            varNameSlot = parseVariable("Expect variable name.");
            if (match(TOKEN_EQUAL)) {
                expression();
            }
            else {
                emitByte(OP_NIL);
            }
            defineVariable(OP_DEFINE_GLOBAL, varNameSlot);
        } while (match(TOKEN_COMMA));
    }

    consume(TOKEN_SEMICOLON,
        "Expect ';' after variable declaration.");


}

// Original Lox version only supported one var declaration at a time.
//static void varDeclaration() {
//    uint8_t global = parseVariable("Expect variable name.");
//
//    if (match(TOKEN_EQUAL)) {
//        expression();
//    }
//    else {
//        emitByte(OP_NIL);
//    }
//    consume(TOKEN_SEMICOLON,
//        "Expect ';' after variable declaration.");
//
//    defineVariable(global);
//}
//
// added ch 21.1.2 pg 386
static void expressionStatement() {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
    emitByte(OP_POP);
}

// added ch 23 pg 415 - 419
static void ifStatement() {
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition."); // [paren]

    int thenJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
    statement();  // THEN branch
    int elseJump = emitJump(OP_JUMP);
    patchJump(thenJump);
    emitByte(OP_POP);

    if (match(TOKEN_ELSE)) statement(); // ELSE branch
    patchJump(elseJump);

    ////> jump-over-else
    //int elseJump = emitJump(OP_JUMP);

    ////< jump-over-else
    //patchJump(thenJump);
    ////> pop-end
    //emitByte(OP_POP);
    ////< pop-end
    ////> compile-else

    //if (match(TOKEN_ELSE)) statement();
    ////< compile-else
    ////> patch-else
    //patchJump(elseJump);
    ////< patch-else
}

// added ch 23 pg 422
static void whileStatement() {
    int loopStart = currentChunk()->count;  // The bytecode location of the while condition
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    int exitJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
    statement();
    emitLoop(loopStart);

    patchJump(exitJump);
    emitByte(OP_POP);
}

// added ch 23.4 pg 425
static void forStatement() {

    /* pg 425
    // to handle infinite loop for (;;)
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
    consume(TOKEN_SEMICOLON, "Expect ';'.");
    int loopStart = currentChunk()->count;
    consume(TOKEN_SEMICOLON, "Expect ';'.");
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");
    statement();
    emitLoop(loopStart);*/

    beginScope();  // in case the initializer declares a variable
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");

    // Handle initializer
    if (match(TOKEN_SEMICOLON)) {
        // No initializer.
    }
    else if (match(TOKEN_VAR)) {
        varDeclaration();
    }
    else {
        expressionStatement();  // will consume the semicolon after initializer
    }
 
    int loopStart = currentChunk()->count;

    // Next is the condition
    int exitJump = -1;
    // if next token is not a semicolon, we have a condition
    if (!match(TOKEN_SEMICOLON)) {
        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after loop condition.");

        // Jump out of the loop if the condition is false.
        exitJump = emitJump(OP_JUMP_IF_FALSE);
        emitByte(OP_POP); // Condition.
    }

    // increment clause
    // compile this in place in the bytecode, then jump over it
    // it runs after the loop, but is injected above the body
    if (!match(TOKEN_RIGHT_PAREN)) {
        int bodyJump = emitJump(OP_JUMP);
        int incrementStart = currentChunk()->count;
        expression();
        emitByte(OP_POP);
        consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");

        emitLoop(loopStart);
        loopStart = incrementStart;
        patchJump(bodyJump);
    }
    //< for-increment

    statement();
    emitLoop(loopStart);

    // if there is a condition clause, patch - pg 436
    // if not, there is no jump to patch and no condition to pop
    if (exitJump != -1) {
        patchJump(exitJump);
        emitByte(OP_POP); // Condition.
    }

    endScope();
}

// added ch 23.2 pg 403
static void block() {
    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
        declaration();
    }
    consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

// added in 24.4 pg 447
static void function(FunctionType type) {
    Compiler compiler;
    initCompiler(&compiler, type);
    beginScope(); // [no-end-scope]

    if (globalFunctionCount < MAX_GLOBAL_FUNCTIONS) {
        globalFunctions[globalFunctionCount++].name = current->function->name;
    }

    consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");
    
    if (!check(TOKEN_RIGHT_PAREN)) {
        do {
            current->function->arity++;
            if (current->function->arity > 255) {
                errorAtCurrent("Can't have more than 255 parameters.");
            }
            uint8_t constantSlotForVarName = parseVariable("Expect parameter name.");
            defineVariable(OP_DEFINE_GLOBAL, constantSlotForVarName);
        } while (match(TOKEN_COMMA));
    }

    consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
    consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");
    block();

    ObjFunction* function = endCompiler();
    emitBytes(OP_CONSTANT, makeConstant(OBJ_VAL(function)));
    //if (globalFunctionCount < MAX_GLOBAL_FUNCTIONS) {
    //    globalFunctions[globalFunctionCount++].name = function->name;
    //}
    //
    // emitBytes(OP_CLOSURE, makeConstant(OBJ_VAL(function)));

    //> parameters
    
}

// added in 24.4 pg 446
static void funDeclaration() {
    uint8_t constantsSlotForFunName = parseVariable("Expect function name.");
    markInitialized();
    function(TYPE_FUNCTION);
    defineVariable(OP_DEFINE_GLOBAL, constantsSlotForFunName);
    
}

// added in 21.1.1 pg 384
static void printStatement() {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after value.");
    emitByte(OP_PRINT);
}

// added in 24.6 pg 457
static void returnStatement() {
    if (current->type == TYPE_SCRIPT) {
        error("Can't return from top-level code.");
    }

    if (match(TOKEN_SEMICOLON)) {
        emitReturn();  // nil
    }
    else {
        if (current->type == TYPE_INITIALIZER) {
            error("Can't return a value from an initializer.");
        }

        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
        emitByte(OP_RETURN);    }
}

// ch 21.1.3 pg 387
static void synchronize() {
    parser.panicMode = false;

    while (parser.current.type != TOKEN_EOF) {
        if (parser.previous.type == TOKEN_SEMICOLON) return;
        switch (parser.current.type) {
        case TOKEN_CLASS:
        case TOKEN_FUN:
        case TOKEN_VAR:
        case TOKEN_FOR:
        case TOKEN_IF:
        case TOKEN_WHILE:
        case TOKEN_PRINT:
        case TOKEN_RETURN:
            return;

        default:
            ; // Do nothing.
        }

        advance();
    }
}

static void declaration() {
    // statement(); // ch 21.1 pg 383

    // Ch 24.4
    if (match(TOKEN_FUN)) {
        funDeclaration();
    }

    // ch 21.2 pg 388
    else if (match(TOKEN_VAR)) {
        varDeclaration();
    }
    else {
        statement();
    }

    if (parser.panicMode) synchronize(); // ch 21.1.3 pg 387
    
    ////> Classes and Instances match-class
    //if (match(TOKEN_CLASS)) {
    //    classDeclaration();
    //    /* Calls and Functions match-fun < Classes and Instances match-class
    //      if (match(TOKEN_FUN)) {
    //    */
    //}
  
   
}

// Ch 21.1 pg 383 adds statement, declaration
static void statement() {
    if (match(TOKEN_PRINT)) {
        printStatement();
    }
    else if (match(TOKEN_FOR)) { // added Ch 23.4 pg 424
        forStatement();
    } else if (match(TOKEN_IF)) { // added Ch 23 pg 414
        ifStatement();
    } else if (match(TOKEN_RETURN)) {
        returnStatement();
    } else if (match(TOKEN_WHILE)) {  // Ch 23.3 pg 422
        whileStatement();
    } else if (match(TOKEN_LEFT_BRACE)) { // added Ch  22.2 block stmts pg 403
       beginScope();
       block();
       endScope();
   }
    else {
        expressionStatement(); // Ch 21.1.2 pg 385
    }
}

static void binary(bool canAssign) {
    TokenType operatorType = parser.previous.type;
    printf("in binary. Operator type=%d \n", operatorType);
    ParseRule* rule = getRule(operatorType);
    parsePrecedence((Precedence)(rule->precedence + 1));
    switch (operatorType) {
        /* new in 18.4.2 page 338*/
        case TOKEN_BANG_EQUAL:    emitBytes(OP_EQUAL, OP_NOT); break;
        case TOKEN_EQUAL_EQUAL:   emitByte(OP_EQUAL); break;
        case TOKEN_GREATER:       emitByte(OP_GREATER); break;
        case TOKEN_GREATER_EQUAL: emitBytes(OP_LESS, OP_NOT); break;
        case TOKEN_LESS:          emitByte(OP_LESS); break;
        case TOKEN_LESS_EQUAL:    emitBytes(OP_GREATER, OP_NOT); break;

        case TOKEN_PLUS:          emitByte(OP_ADD);  printf("in binary: emitted ADD opcode\n"); break;
        case TOKEN_MINUS:         emitByte(OP_SUBTRACT); break;
        case TOKEN_STAR:          emitByte(OP_MULTIPLY); break;
        case TOKEN_SLASH:         emitByte(OP_DIVIDE); break;
        case TOKEN_RANDOM:        emitByte(OP_RANDOM); break;
        default: return; // Unreachable.
    }
}

// Ch 24 pg 450
static void call(bool canAssign) {
    uint8_t argCount = argumentList();
    emitBytes(OP_CALL, argCount);
}

// new for ch18 - to handle false, true, nil tokens
static void literal(bool canAssign) {
    switch (parser.previous.type) {
        case TOKEN_FALSE: emitByte(OP_FALSE); break;
        case TOKEN_NIL: emitByte(OP_NIL); break;
        case TOKEN_TRUE: emitByte(OP_TRUE); break;
        default: return; // Unreachable.
    }
}

// new for ch23.2
// LH side already compiled and result will be at top of stack
static void and_(bool canAssign) {
    int endJump = emitJump(OP_JUMP_IF_FALSE);

    emitByte(OP_POP); // pop the LH side since it is true and no longer needed
    parsePrecedence(PREC_AND);  // The RH side

    patchJump(endJump);
}

// see pg 421 for logic flow.  
// If LH side false we jump over a jump and evaluate the RH side
// If LH side true we fall thru into the jump and skip evaluating the RH side
// Could add more VM opcodes to simplify this ...
static void or_(bool canAssign) {
    int elseJump = emitJump(OP_JUMP_IF_FALSE);
    int endJump = emitJump(OP_JUMP);

    patchJump(elseJump);
    emitByte(OP_POP);

    parsePrecedence(PREC_OR);
    patchJump(endJump);
}

// 17.4.2 grouping in expression  pg 314
// Used to insert a lower precedence expression where a higher precedence is expected
static void grouping(bool canAssign) { // line 666
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void number(bool canAssign) {
    double value = strtod(parser.previous.start, NULL);
    // emitConstant(value); // ch 17
    emitConstant(NUMBER_VAL(value));  // changed in Ch 18
}

static bool parseIntSlice(const char* ptr, int len, int* outValue) {
    // if (!ptr || len <= 0 || !outValue) return false;

    int value = 0;
    int sign = 1;
    int i = 0;

    if (ptr[0] == '-') {
        sign = -1;
        i = 1;
        if (len == 1) return false;  // Just a minus sign no good
    }

    for (; i < len; i++) {
        if (!isDigit((unsigned char)ptr[i])) return false;
        value = value * 10 + (ptr[i] - '0');
    }

    *outValue = sign * value;
    return true;
}



// in ch 19.3 page 346
static void string(bool canAssign) {
    // strip quote marks off literal
    // TODO transform embedded escape chars here
    emitConstant(OBJ_VAL(copyString(parser.previous.start + 1,
        parser.previous.length - 2)));
}



//static void string(bool canAssign) {
//    emitConstant(OBJ_VAL(copyString(parser.previous.start + 1,
//        parser.previous.length - 2)));
//}

//static void namedVariable(Token name, bool canAssign) {

//// this was for assignment logic - ch 21.4 pg 393
//static void namedVariable(Token name, bool canAssign) {
//    uint8_t arg = identifierConstant(&name);
//    if (canAssign && match(TOKEN_EQUAL)) {
//        expression();
//        emitBytes(OP_SET_GLOBAL, arg);
//    }
//    else {
//        emitBytes(OP_GET_GLOBAL, arg);
//    }
//}

// for assignment logic - ch 22.4 pg 407 local vars
static void namedVariable(Token name, bool canAssign, int numArraySubscripts) {
    OpCode getOp, setOp;
    int arg = -1;
    if (numArraySubscripts != 0) {
        // TODO lookup the array definition to set arg
        arg = identifierConstant(&name);
        getOp = OP_GET_GLOBAL_ARRAY;
        setOp = OP_SET_GLOBAL_ARRAY;

    }
    else {
        arg = resolveLocal(current, &name);
        if (arg != -1) {
            getOp = OP_GET_LOCAL;
            setOp = OP_SET_LOCAL;
        }
        else {
            arg = identifierConstant(&name);
            getOp = OP_GET_GLOBAL;
            setOp = OP_SET_GLOBAL;
        }
    }

    if (canAssign && match(TOKEN_EQUAL)) { //pg 408
        uint8_t bytecode_start_rhs_ip = current->function->chunk.count;  // save off the start of the assignment
        printf("ip (bytecode offset) for start of the rhs is %d\n", current->function->chunk.count);
        expression(); // This is the RH side of the assignment
        emitBytes(setOp, (uint8_t)arg);
        if (setOp == OP_SET_GLOBAL_ARRAY) {
            // put the ip of the opcode that starts the RHS into the bytecode
            emitByte((uint8_t)bytecode_start_rhs_ip);
        }
    }
    else {
        emitBytes(getOp, (uint8_t)arg);
    }

    if (numArraySubscripts != 0) { // TODO  must always have subscripts for OP_GET_GLOBAL_ARRAY! assert this!
        emitByte((uint8_t)numArraySubscripts);
    }
}

// added in Ch 21.3 pg 391; canAssign added on pg 395
static void variable(bool canAssign) {
    int numArraySubscripts = 0;
    Token variableToken = parser.previous;
    // Is the variable subscripted?  if so it could be an array reference
    // OR it can be a function call!
    
    // see if the variableToken refers to a function.
    //char* tokenend = variableToken.start + variableToken.length;
    //char save = *tokenend;
    //*tokenend = '\0'; // null terminate temp


    // globalFunctions[globalFunctionCount++].name = function->name;

    // TODO native functions are registered at runtime in the VM
    //  the compiler needs to know about them too!  defineNative .. Clock

    
    bool varIsFunction = false;

    if (variableToken.length == 5 && memcmp("clock", variableToken.start, 5) == 0) {
        varIsFunction = true;
    }

    for (int i = 0; i < MAX_GLOBAL_FUNCTIONS && !varIsFunction && globalFunctions[i].name != NULL; i++) {
        if (variableToken.length == globalFunctions[i].name->length &&
            memcmp(globalFunctions[i].name->chars, variableToken.start, variableToken.length) == 0) {
            varIsFunction = true;
        }
    }

//    *tokenend = save; // restore char

    // if not a function, see if its an array
    if (!varIsFunction && match(TOKEN_LEFT_PAREN)) {

       
        
        do {
            numArraySubscripts++;
            if (numArraySubscripts > MAXARRAYDIMENSIONS)
                error("Can't access more than 3 array dimensions, sorry.");

            // the subscript can be an integer (negative is allowed)
            // or any expression that yields an integer
            // 
            // Cross sectioning - need to generalize this 
            // Instead of the kludge workaround, emit a for loop for cross sectioning
            // 
            // 
            // 
            // or the special case of a '*' which means all array elements
            //   .e.g. A(*) or B(*,*) or MATRIX(5,*) etc.
            ///  B(*) = .N will set the values to 1,2,3,4,5 in the array
            
            if (match(TOKEN_STAR)) {
                emitConstant(ARRAY_STAR_VAL);  
            }
            else if (match(TOKEN_COLON)) {
                // e.g. A(1:N)
                error("Not implemented yet - support for A(1:3) or even A(3:1) for A(3) then A(2) then A(1). Sorry.");
            }
            else {
                expression();
            }
            

        } while (match(TOKEN_COMMA));

        printf("Array reference has %d dimensions\n", numArraySubscripts);
        
        consume(TOKEN_RIGHT_PAREN,
            "Expect ')' after array reference.");
    }
    

    namedVariable(variableToken, canAssign, numArraySubscripts);
   
}


static void unary(bool canAssign) {
    TokenType operatorType = parser.previous.type;
    printf("in unary. Operator type=%d \n", operatorType);
    
    parsePrecedence(PREC_UNARY); 
    
    switch (operatorType) {
        case TOKEN_BANG: emitByte(OP_NOT); break;  // ch 18.4.1 pg 336
        case TOKEN_MINUS: emitByte(OP_NEGATE); break;
        default: return; // Unreachable.
    }
}


//static void statement();
//static void declaration();

static ParseRule* getRule(TokenType type);

static void parsePrecedence(Precedence precedence) {
    advance();
    ParseFn prefixRule = getRule(parser.previous.type)->prefix;
    if (prefixRule == NULL) {
        error("parsePrecedence: Expected expression after prefix.");
        return;
    }
    
    bool canAssign = precedence <= PREC_ASSIGNMENT;  // added canAssign in Ch 21.4 pg 394
    prefixRule(canAssign);
    
    while (precedence <= getRule(parser.current.type)->precedence) {
        advance();
        ParseFn infixRule = getRule(parser.previous.type)->infix;
        infixRule(canAssign);
      
        // added ch 21.4 pg 395
        if (canAssign && match(TOKEN_EQUAL)) {
            error("Invalid assignment target.");
        }
      
    }
}


//void compile(const char* source) {
// bool compile(const char* source, Chunk * chunk) { prior to ch 24
ObjFunction* compile(const char* source) {
	initScanner(source);

    globalFunctionCount = 0;
    for (int i = 0; i < MAX_GLOBAL_FUNCTIONS; i++) {
        globalFunctions[i].name = NULL;
    }
    
    Compiler compiler;
    initCompiler(&compiler, TYPE_SCRIPT); // pg 437
    
    parser.hadError = false;
    parser.panicMode = false;

    /* version as of start of Ch 17 pg 307
    advance();
    expression();
    consume(TOKEN_EOF, "Expect end of expression.");
    */

    /* added in Ch 21.1 pg 383 */
    advance();
    while (!match(TOKEN_EOF)) {
        declaration();
        
    }
    // consume(TOKEN_EOF, "Expect end of expression LES.");

    ObjFunction* function = endCompiler();
    return parser.hadError ? NULL : function;

    /* prior to Ch 24
    endCompiler();
    return !parser.hadError;*/
}

#undef MAXVARSINDECLARE
#undef MAXARRAYDIMENSIONS
#undef MAX_GLOBAL_FUNCTIONS


#define MAX_GLOBAL_FUNCTIONS 5
//////