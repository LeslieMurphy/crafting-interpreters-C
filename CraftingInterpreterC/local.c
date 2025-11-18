
#include <stdio.h>
#include "local.h"
#include "compiler.h"

void error(const char* message);

// local identifiers pg 406 in ch 22.3
static bool identifiersEqual(Token* a, Token* b) {
    if (a->length != b->length) return false;
    return memcmp(a->start, b->start, a->length) == 0;
}

// Ch 22.3 local vars pg 405
static void addLocal(Compiler* current, Token name) {
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

// Ch 22.3 local vars pg 405/406
// Declare is when var is added to scope; not available for use until fully defined
void declareLocalVariable(Compiler* current, Token* localVarToken) {
    // global variables are late bound so we don't keep track of them here
    // if (current->scopeDepth == 0) return;
    
    for (int i = current->localCount - 1; i >= 0; i--) {
        Local* local = &current->locals[i];
        if (local->depth != -1 && local->depth < current->scopeDepth) {
            break;
        }

        if (identifiersEqual(localVarToken, &local->name)) {
            error("Already a variable with this name in this scope.");
        }
    }

    addLocal(current , *localVarToken);
}


// pg 408 - first local var is at slot 0, second at 1 (up the stack) etc.
// index into the lookup stack will exactly match the slot in the runtime VM stack
// This resolves global variables as well ... need to rename it!
int resolveLocal(Compiler* compiler, Token* name) {
    for (int i = compiler->localCount - 1; i >= 0; i--) {
        Local* local = &compiler->locals[i];
        if (identifiersEqual(name, &local->name)) {
            // added pg 411 to distinguish declared vs defined
            // when we first add a new local we set it's depth to a flag of -1 meaning uninitialized
            // see markInitialized
            if (local->depth == -1) {
                char buffer[200];
                snprintf(buffer, sizeof(buffer), "Can't read local variable '%.*s' in its own initializer.", name->length, name->start);
                error(buffer);
            }
            return i;
        }
    }

    return -1; // not found, must be Global var
}
