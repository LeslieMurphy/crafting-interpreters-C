#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "compiler.h"
#include "memory.h"
#include "scanner.h"
#include "parseRules.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

// static void initCompiler(Compiler* compiler, FunctionType type) {
//ObjFunction* compile(const char* source) {

// Ch 22.1 pg 401 local variables

typedef struct {
    Token name;
    int depth;  // 0 is a Global, 1 is the first local scope, 2 is the second nested local scope etc.
    bool isCaptured;   // for Closures
} Local;

typedef enum {
    TYPE_FUNCTION,
    TYPE_INITIALIZER,
    TYPE_METHOD,
    TYPE_SCRIPT  // a dummy function used for the main program logic
} FunctionType;

typedef struct Compiler {
    struct Compiler* enclosing;  // linked list added Ch 24.4.1 pg 448
    ObjFunction* function;
    FunctionType type;

    Local locals[UINT8_COUNT];
    int localCount;  // count of locals in scope
    int scopeDepth;

    // LLM Upvalue upvalues[UINT8_COUNT]; // Closures upvalues-array
        
} Compiler;

typedef struct ClassCompiler {
    struct ClassCompiler* enclosing;
    //> Superclasses has-superclass
    bool hasSuperclass;
    //< Superclasses has-superclass
} ClassCompiler;

typedef struct {
    Token current;
    Token previous;
    bool hadError;
    bool panicMode;
} Parser;

Parser parser;
Compiler* current = NULL;

static void expression();
static void declaration();
static void statement();
static void parsePrecedence(Precedence precedence);
static void consume(TokenType type, const char* message);
static uint8_t identifierConstant(Token* name);
static void emitBytes(uint8_t byte1, uint8_t byte2);
static void declareVariable();
static void error(const char* message);
static uint8_t makeConstant(Value value);
static bool check(TokenType type);
static bool match(TokenType type);

// ch 21.2 pg 389
// take token and and lexeme to chunk constant table as string
// return index of the constant - to lookup the variable in future usages

static uint8_t identifierConstant(Token* name) {
    return makeConstant(OBJ_VAL(copyString(name->start, name->length)));
}

// local identifiers pg 406 in ch 22.3
static bool identifiersEqual(Token* a, Token* b) {
    if (a->length != b->length) return false;
    return memcmp(a->start, b->start, a->length) == 0;
}

// Ch 22.3 local vars pg 405
static void addLocal(Token name) {
    if (current->localCount == UINT8_COUNT) {
        error("Too many local variables in function.");
        return;
    }

    Local* local = &current->locals[current->localCount++];
    local->name = name;
    local->depth = -1; // Ch 22.4.2 pg 411
    // local->depth = current->scopeDepth;  // Ch 22.3 pg 405 

    //local->isCaptured = false; // for closures
}

static uint8_t parseVariable(const char* errorMessage) {
    consume(TOKEN_IDENTIFIER, errorMessage);

    // for local var support ch 22.1 pg 404
    declareVariable();
    // When we are in a local scope, no need to add variable name to the constants table
    if (current->scopeDepth > 0) return 0;

    return identifierConstant(&parser.previous);
}

static void markInitialized() {
    if (current->scopeDepth == 0) return; // if top level function, there are no local vars only global pg 447
    current->locals[current->localCount - 1].depth = current->scopeDepth;
}

// Ch 22.3 local vars pg 405/406
// Declare is when var is added to scope; not available for use until fully defined
static void declareVariable() {
    // global variables are late bound so we don't keep track of them here
    if (current->scopeDepth == 0) return;

    Token* name = &parser.previous;

    for (int i = current->localCount - 1; i >= 0; i--) {
        Local* local = &current->locals[i];
        if (local->depth != -1 && local->depth < current->scopeDepth) {
            break;
        }

        if (identifiersEqual(name, &local->name)) {
            error("Already a variable with this name in this scope.");
        }
    }

    addLocal(*name);
}

// define is when variable becomes available for use
static void defineVariable(uint8_t global) {

    // if in local scope, the local var is already on top of stack - see pg 404
    if (current->scopeDepth > 0) {
        markInitialized(); // pg 411
        return;
    } 
    emitBytes(OP_DEFINE_GLOBAL, global); // ch 21.2 pg 389
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


// pg 408 - first local var is at slot 0, second at 1 (up the stack) etc.
// index into the lookup stack will exactly match the slot in the runtime VM stack
static int resolveLocal(Compiler* compiler, Token* name) {
    for (int i = compiler->localCount - 1; i >= 0; i--) {
        Local* local = &compiler->locals[i];
        if (identifiersEqual(name, &local->name)) {
            // added pg 411 to distinguish declared vs defined
            // when we first add a new local we set it's depth to a flag of -1 meaning uninitialized
            // see markInitialized
            if (local->depth == -1) {
                error("Can't read local variable in its own initializer.");
            }
            return i;
        }
    }

    return -1; // not found, must be Global var
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

static void error(const char* message) {
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

static void emitBytes(uint8_t byte1, uint8_t byte2) {
    emitByte(byte1);
    emitByte(byte2);
}

static int emitJump(uint8_t instruction) {
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
    compiler->function = NULL;
    compiler->type = type;
    
    compiler->localCount = 0;
    compiler->scopeDepth = 0;
    compiler->function = newFunction();  // pg 437
    compiler->type = type;

    current = compiler;
    if (type != TYPE_SCRIPT) {
        current->function->name = copyString(parser.previous.start,
            parser.previous.length);
    }

    // Reserve stack slot zero for VM internal use
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

// static void endCompiler() prior to Ch 24
static ObjFunction* endCompiler() {
    emitReturn();
    ObjFunction* function = current->function;

#ifdef DEBUG_PRINT_CODE
    if (!parser.hadError) {
        printf("clean compile! here is the bytecode\n");
        // disassembleChunk(currentChunk(), "code"); prior to Ch 24
        //  new logic pg 438
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


//static void statement();
//static void declaration();
//
//static ParseRule* getRule(TokenType type);
//static void parsePrecedence(Precedence precedence);

static void expression() {
    parsePrecedence(PREC_ASSIGNMENT); // line 1032
}

#define MAXVARSINDECLARE 20

// 11/17/25 add support for multiple
// var x=66, a=33; print a;  print x;
// fun a() {var z=55, q=44+z; print z; print q;} 

static void varDeclaration() {
    // varSlot can be a slot in Globals if the declaration is outside a function
    //   in this case a OP_SET_GLOBAL is generated
    // or can be the slot on the local stack when declaring variables inside of a function
    uint8_t varSlot = parseVariable("Expect variable name.");
    int numVariablesDefined = 0;

    if (match(TOKEN_EQUAL)) {
        expression();
    }
    else {
        emitByte(OP_NIL);
    }
    // note that defineVariable will define a global; for a local it marks it as initialized
    defineVariable(varSlot);
  
    // Are there more variables to declare?
    if (match(TOKEN_COMMA)) {
        do {
            if (numVariablesDefined > MAXVARSINDECLARE)
                error("Can't define more than 20 variables at a time, sorry.");
            varSlot = parseVariable("Expect variable name.");
            if (match(TOKEN_EQUAL)) {
                expression();
            }
            else {
                emitByte(OP_NIL);
            }
            defineVariable(varSlot);
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

    consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");
    
    if (!check(TOKEN_RIGHT_PAREN)) {
        do {
            current->function->arity++;
            if (current->function->arity > 255) {
                errorAtCurrent("Can't have more than 255 parameters.");
            }
            uint8_t constant = parseVariable("Expect parameter name.");
            defineVariable(constant);
        } while (match(TOKEN_COMMA));
    }

    consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
    consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");
    block();

    ObjFunction* function = endCompiler();
    emitBytes(OP_CONSTANT, makeConstant(OBJ_VAL(function)));
    // emitBytes(OP_CLOSURE, makeConstant(OBJ_VAL(function)));

    //> parameters
    
}

// added in 24.4 pg 446
static void funDeclaration() {
    uint8_t global = parseVariable("Expect function name.");
    markInitialized();
    function(TYPE_FUNCTION);
    defineVariable(global);
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
static void namedVariable(Token name, bool canAssign) {
    uint8_t getOp, setOp;
    int arg = resolveLocal(current, &name);
    if (arg != -1) {
        getOp = OP_GET_LOCAL;
        setOp = OP_SET_LOCAL;
    }
    //else if ((arg = resolveUpvalue(current, &name)) != -1) {
    //    getOp = OP_GET_UPVALUE;
    //    setOp = OP_SET_UPVALUE;
    //}
    else {
        arg = identifierConstant(&name);
        getOp = OP_GET_GLOBAL;
        setOp = OP_SET_GLOBAL;
    }

    if (canAssign && match(TOKEN_EQUAL)) { //pg 408
        expression();
        emitBytes(setOp, (uint8_t)arg);
    }
    else {
        emitBytes(getOp, (uint8_t)arg);
    }
}

// added in Ch 21.3 pg 391; canAssign added on pg 395
static void variable(bool canAssign) {
    namedVariable(parser.previous, canAssign);
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