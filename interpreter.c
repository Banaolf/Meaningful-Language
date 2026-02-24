#include "syntax.h"
#include "lexer.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- Runtime Garbage Collector ---

typedef struct GCNode {
    void* ptr;
    struct GCNode* next;
} GCNode;

GCNode* gcHead = NULL;

void gc_track(void* ptr) {
    GCNode* node = malloc(sizeof(GCNode));
    node->ptr = ptr;
    node->next = gcHead;
    gcHead = node;
}

void gc_free_all() {
    GCNode* current = gcHead;
    while (current) {
        GCNode* next = current->next;
        free(current->ptr);
        free(current);
        current = next;
    }
    gcHead = NULL;
}

// --- Symbol Table System ---

typedef enum { VAL_INT, VAL_STRING, VAL_VOID } ValueType;

typedef struct {
    ValueType type;
    union {
        int number;
        char* string;
    } as;
} Value;

Value makeInt(int n) { Value v; v.type = VAL_INT; v.as.number = n; return v; }
Value makeStr(char* s) { Value v; v.type = VAL_STRING; v.as.string = s; return v; }
Value makeVoid() { Value v; v.type = VAL_VOID; return v; }

typedef struct Symbol {
    char* name;
    Value value;       // For variables
    ASTNode* funcNode; // For functions
    struct Symbol* next;
} Symbol;

Symbol* symbolTable = NULL;

// Helper: Get variable value from the stack (Scope)
Value getVariable(char* name) {
    Symbol* current = symbolTable;
    while (current != NULL) {
        // We only want variables, not functions
        if (strcmp(current->name, name) == 0 && current->funcNode == NULL) {
            return current->value;
        }
        current = current->next;
    }
    fprintf(stderr, "Runtime Error: Undefined variable '%s'\n", name);
    return makeInt(0);
}

// Helper: Set variable (Update existing or create new in global/current scope)
void setVariable(char* name, Value val) {
    Symbol* current = symbolTable;
    while (current != NULL) {
        if (strcmp(current->name, name) == 0 && current->funcNode == NULL) {
            current->value = val;
            return;
        }
        current = current->next;
    }
    
    // If not found, add to the head (Global or Current Scope)
    Symbol* sym = malloc(sizeof(Symbol));
    sym->name = strdup(name);
    sym->value = val;
    sym->funcNode = NULL;
    sym->next = symbolTable;
    symbolTable = sym;
}

// Helper: Define a function in the symbol table
void defineFunction(char* name, ASTNode* node) {
    Symbol* sym = malloc(sizeof(Symbol));
    sym->name = strdup(name);
    sym->funcNode = node;
    sym->next = symbolTable;
    symbolTable = sym;
}

// Helper: Retrieve a function node
ASTNode* getFunction(char* name) {
    Symbol* current = symbolTable;
    while (current != NULL) {
        if (strcmp(current->name, name) == 0 && current->funcNode != NULL) {
            return current->funcNode;
        }
        current = current->next;
    }
    return NULL;
}

// Helper: Push a local variable (Shadowing)
void pushVariable(char* name, Value val) {
    Symbol* sym = malloc(sizeof(Symbol));
    sym->name = strdup(name);
    sym->value = val;
    sym->funcNode = NULL;
    sym->next = symbolTable;
    symbolTable = sym;
}

// Helper: Pop variables (Exit Scope)
void popVariables(int count) {
    for (int i = 0; i < count; i++) {
        if (symbolTable != NULL) {
            Symbol* temp = symbolTable;
            symbolTable = symbolTable->next;
            free(temp->name);
            free(temp);
        }
    }
}

void freeSymbolTable() {
    Symbol* current = symbolTable;
    while (current != NULL) {
        Symbol* next = current->next;
        if (current->name) free(current->name);
        // Note: We don't free current->value strings here as ownership is complex in this design
        free(current);
        current = next;
    }
    symbolTable = NULL;
}

// --- Evaluator ---

Value evaluate(ASTNode* node) {
    if (node == NULL) return makeVoid();

    // 1. Literals
    if (node->type == NODE_NUMBER) {
        return makeInt(atoi(node->value));
    }
    if (node->type == NODE_STRING) {
        return makeStr(node->value);
    }

    // 2. Variables
    if (node->type == NODE_VARIABLE) {
        return getVariable(node->value);
    }

    // 3. Binary Operations
    if (node->type == NODE_BINARY_OP) {
        Value left = evaluate(node->left);
        Value right = evaluate(node->right);

        if (strcmp(node->value, "+") == 0) {
            if (left.type == VAL_INT && right.type == VAL_INT) {
                return makeInt(left.as.number + right.as.number);
            }
            if (left.type == VAL_STRING && right.type == VAL_STRING) {
                char* result = malloc(strlen(left.as.string) + strlen(right.as.string) + 1);
                strcpy(result, left.as.string);
                strcat(result, right.as.string);
                gc_track(result); // Track this string so we can free it later
                return makeStr(result);
            }
        }
        
        // For other operators, ensure we have numbers
        if (left.type != VAL_INT || right.type != VAL_INT) return makeInt(0);

        if (strcmp(node->value, "-") == 0) return makeInt(left.as.number - right.as.number);
        if (strcmp(node->value, "*") == 0) return makeInt(left.as.number * right.as.number);
        if (strcmp(node->value, "/") == 0) {
            if (right.as.number == 0) {
                fprintf(stderr, "Runtime Error: Division by zero.\n");
                return makeInt(0);
            }
            return makeInt(left.as.number / right.as.number);
        }

        // Logic & Comparison
        if (strcmp(node->value, "and") == 0) return makeInt(left.as.number && right.as.number);
        if (strcmp(node->value, "or") == 0) return makeInt(left.as.number || right.as.number);
        
        if (strcmp(node->value, "==") == 0) {
            if (left.type == VAL_STRING && right.type == VAL_STRING) return makeInt(strcmp(left.as.string, right.as.string) == 0);
            return makeInt(left.as.number == right.as.number);
        }
        if (strcmp(node->value, "!=") == 0) return makeInt(left.as.number != right.as.number);
        
        if (strcmp(node->value, "<") == 0) return makeInt(left.as.number < right.as.number);
        if (strcmp(node->value, ">") == 0) return makeInt(left.as.number > right.as.number);
        if (strcmp(node->value, "<=") == 0) return makeInt(left.as.number <= right.as.number);
        if (strcmp(node->value, ">=") == 0) return makeInt(left.as.number >= right.as.number);
    }

    // 4. Assignments
    if (node->type == NODE_ASSIGNMENT) {
        Value val = evaluate(node->right);
        setVariable(node->value, val);
        return val;
    }

    // 5. Function Definitions
    if (node->type == NODE_FUNCTION_DEF) {
        defineFunction(node->value, node);
        return makeVoid();
    }

    // 6. Function Calls
    if (node->type == NODE_FUNCTION_CALL) {
        ASTNode* funcDef = getFunction(node->value);
        if (!funcDef) {
            fprintf(stderr, "Runtime Error: Undefined function '%s'\n", node->value);
            return makeInt(0);
        }

        // funcDef children layout: [arg1, arg2, ..., BODY]
        int paramCount = funcDef->childCount - 1;
        int argCount = node->childCount;

        if (paramCount != argCount) {
            fprintf(stderr, "Error: Function '%s' expects %d args, got %d\n", node->value, paramCount, argCount);
            return makeInt(0);
        }

        // A. Evaluate Arguments
        Value* argValues = malloc(sizeof(Value) * argCount);
        for(int i=0; i<argCount; i++) {
            argValues[i] = evaluate(node->children[i]);
        }

        // B. Push Scope (Parameters)
        for(int i=0; i<paramCount; i++) {
            char* paramName = funcDef->children[i]->value;
            pushVariable(paramName, argValues[i]);
        }
        free(argValues);

        // C. Execute Body
        ASTNode* body = funcDef->children[paramCount];
        Value result = makeVoid();
        
        if (body && body->children) {
            for(int i=0; i<body->childCount; i++) {
                ASTNode* stmt = body->children[i];
                if (stmt->type == NODE_RETURN) {
                    result = evaluate(stmt->right);
                    goto end_call; // Return immediately
                }
                evaluate(stmt);
            }
        }

        end_call:
        // D. Pop Scope
        popVariables(paramCount);
        return result;
    }

    // 8. Native Print
    if (node->type == NODE_PRINT) {
        Value val = evaluate(node->right);
        if (val.type == VAL_INT) printf("%d\n", val.as.number);
        if (val.type == VAL_STRING) printf("%s\n", val.as.string);
        fflush(stdout);
        return val;
    }

    // 9. If Statements
    if (node->type == NODE_IF) {
        Value cond = evaluate(node->children[0]);
        // Treat 0 as false, anything else as true
        int isTrue = 0;
        if (cond.type == VAL_INT) isTrue = cond.as.number != 0;
        if (cond.type == VAL_STRING) isTrue = 1; // Strings are truthy

        if (isTrue) {
            evaluate(node->children[1]); // Execute body
        }
        return makeVoid();
    }

    // 10. While Loops
    if (node->type == NODE_WHILE) {
        while(1) {
            Value cond = evaluate(node->children[0]);
            int isTrue = 0;
            if (cond.type == VAL_INT) isTrue = cond.as.number != 0;
            if (cond.type == VAL_STRING) isTrue = 1;

            if (!isTrue) break;

            evaluate(node->children[1]); // Execute body
        }
        return makeVoid();
    }

    // 7. Program / Block Execution
    if (node->type == NODE_PROGRAM) {
        Value lastResult = makeVoid();
        if (node->children) {
            for(int i=0; i<node->childCount; i++) {
                lastResult = evaluate(node->children[i]);
            }
        }
        return lastResult;
    }

    return makeVoid();
}

// Helper function to read a file into a string buffer
char* readFile(const char* path) {
    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "Could not open file \"%s\".\n", path);
        exit(74);
    }

    fseek(file, 0L, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);

    char* buffer = (char*)malloc(fileSize + 1);
    if (buffer == NULL) {
        fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
        exit(74);
    }

    size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
    if (bytesRead < fileSize) {
        fprintf(stderr, "Could not read file \"%s\".\n", path);
        exit(74);
    }

    buffer[bytesRead] = '\0';

    fclose(file);
    return buffer;
}

int main(int argc, char *argv[]){
    if (argc != 2) {
        printf("Usage: meaningful [script.mean]\n");
        return 64;
    }
    if (argv[1] == "-v"){
        printf("Meaningful Language, Licenced under BMLL 2.0\n                      Version ALPHA.0.88");
    }

    char* src = readFile(argv[1]);
    TokenStream* stream = lex(src);
    ASTNode* root = parseFile(*stream);

    if (root) {
        evaluate(root);
        freeAST(root);
    }

    finalCleanup(stream);
    free(src); // Free the buffer from readFile
    freeSymbolTable();
    gc_free_all(); // Free all runtime strings
    return 0;
}