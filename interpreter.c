#include "parser.h"
#include "lexer.h"
#include <stddef.h>
#include <stddef.h>
#include <stdarg.h>

#include <math.h>
#include <stdlib.h> 
#include <stdbool.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include "uthash.h"
#define VERSION "ALPHA.0.99.45.2"
//Licenced under BMLL2.0, see LICENCE for further info

// --- GC, Value, and Object System ---

// Forward declarations for our types
typedef struct Object Object;
typedef struct Value Value;

typedef enum {
    VAL_INT,
    VAL_VOID,
    VAL_ERR,
    VAL_OBJECT,
    VAL_FLOAT,
    VAL_RETURN,
    VAL_BREAK
} ValueType;

typedef enum {
    OBJ_STRING,
    OBJ_LIST,
    OBJ_DICT
} ObjectType;

// The header for all heap-allocated objects in the language
struct Object {
    ObjectType type;
    bool isMarked;
    struct Object* next; // Intrusive list to track all allocations
};

// The language's core Value type. Can be a number or a pointer to an Object.
struct Value {
    ValueType type;
    union {
        int number;
        Object* obj;
        Exception exc;
        float decimal;
    } as;
};

// Helper macros for the new Value/Object system
#define IS_OBJECT(value)    ((value).type == VAL_OBJECT)
#define AS_OBJECT(value)    ((value).as.obj)
#define OBJ_TYPE(value)     (AS_OBJECT(value)->type)

#define IS_STRING(value)    (IS_OBJECT(value) && OBJ_TYPE(value) == OBJ_STRING)
#define IS_LIST(value)      (IS_OBJECT(value) && OBJ_TYPE(value) == OBJ_LIST)
#define IS_DICT(value)      (IS_OBJECT(value) && OBJ_TYPE(value) == OBJ_DICT)

// Cast a Value to a specific C type. Only use after checking with IS_ macros.
typedef struct ObjString {
    Object obj;
    size_t length;
    char chars[];
} ObjString;

#define AS_STRING(value)    ((ObjString*)AS_OBJECT(value))
#define AS_CSTRING(value)   (AS_STRING(value)->chars)

typedef struct ValueList {
    Object obj;
    Value* items;
    int count;
    int capacity;
} ValueList;

#define AS_LIST(value)      ((ValueList*)AS_OBJECT(value))

typedef struct DictEntry {
    char* key;
    Value value;
    UT_hash_handle hh;
} DictEntry;

typedef struct ValueDict {
    Object obj;
    DictEntry* head;
} ValueDict;

#define AS_DICT(value)      ((ValueDict*)AS_OBJECT(value))

// Value constructors
Value makeInt(int n) { Value v; v.type = VAL_INT; v.as.number = n; return v; }
Value makeObj(Object* obj) { Value v; v.type = VAL_OBJECT; v.as.obj = obj; return v; }
Value makeVoid() { Value v; v.type = VAL_VOID; return v; }
Value makeFloat(float n) { Value v; v.type = VAL_FLOAT; return v;}
Value makeReturn(Value val) { Value v; v.type = VAL_RETURN; v.as.obj = val.as.obj;v.as.number = val.as.number; return v; }
Value makeBreak() { Value v; v.type = VAL_BREAK; return v; }
Exception makeException(ExceptionType type, char* message) {Exception exc;exc.type = type;exc.message = message;return exc;}
Value throwException(ExceptionType type, const char* fmt, ...) {
    char* buffer = malloc(1024);
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, 1024, fmt, args);
    va_end(args);

    Value v;
    v.type = VAL_ERR;
    v.as.exc = makeException(type, buffer);
    printf("%s\n",v.as.exc.message);
    return v;
}


// --- Garbage Collector ---

Object* allObjects = NULL; // Head of the intrusive list of all objects

static Object* allocateObject(size_t size, ObjectType type) {
    Object* object = (Object*)malloc(size);
    if (object == NULL) {
        fprintf(stderr, "Out of memory\n");
        exit(1);
    }
    object->type = type;
    object->isMarked = false;
    object->next = allObjects; // Add to the list of all objects
    allObjects = object;
    return object;
}

ObjString* allocateString(char* chars, size_t length) {
    ObjString* string = (ObjString*)allocateObject(sizeof(ObjString) + length + 1, OBJ_STRING);
    string->length = length;
    memcpy(string->chars, chars, length);
    string->chars[length] = '\0';
    return string;
}

ValueList* allocateList(int capacity) {
    ValueList* list = (ValueList*)allocateObject(sizeof(ValueList), OBJ_LIST);
    list->items = malloc(sizeof(Value) * capacity);
    list->count = 0;
    list->capacity = capacity;
    return list;
}

ValueDict* allocateDict() {
    ValueDict* dict = (ValueDict*)allocateObject(sizeof(ValueDict), OBJ_DICT);
    dict->head = NULL;
    return dict;
}

void markValue(Value value); // Forward declare for recursion

void markObject(Object* object) {
    if (object == NULL || object->isMarked) return;
    object->isMarked = true;

    switch (object->type) {
        case OBJ_STRING:
            break; // Strings don't have outgoing references
        case OBJ_LIST: {
            ValueList* list = (ValueList*)object;
            for (int i = 0; i < list->count; i++) {
                markValue(list->items[i]);
            }
            break;
        }
        case OBJ_DICT: {
            ValueDict* dict = (ValueDict*)object;
            DictEntry *s, *tmp;
            HASH_ITER(hh, dict->head, s, tmp) {
                markValue(s->value);
            }
            break;
        }
    }
}

void markValue(Value value) {
    if (IS_OBJECT(value)) {
        markObject(AS_OBJECT(value));
    }
}

// --- Symbol Table System ---

typedef Value (*NativeFn)(int argCount, Value* args);

typedef struct Symbol {
    char* name; // key
    Value value;
    ASTNode* funcNode;
    NativeFn nativeFunc;
    UT_hash_handle hh;
} Symbol;

typedef struct Scope {
    Symbol *symbols; // hash table for this scope
    struct Scope *prev;
} Scope;

Scope* currentScope = NULL; // Head of scope stack

// Helper: Get variable value from the stack (Scope)
Value getVariable(char* name) {
    Scope* scope = currentScope;
    while (scope) {
        Symbol* s;
        HASH_FIND_STR(scope->symbols, name, s);
        // We only want variables, not functions
        if (s && !s->funcNode && !s->nativeFunc) {
            return s->value;
        }
        scope = scope->prev;
    }
    return throwException(IdentifierNotFoundException, "IdentifierNotFoundException: Variable '%s' not defined.\n", name);
}

// Helper: Set variable (Update existing or create new in global/current scope)
void setVariable(char* name, Value val) {
    // Search from current scope up to global to find existing variable
    Scope* scope = currentScope;
    while (scope != NULL) {
        Symbol* s;
        HASH_FIND_STR(scope->symbols, name, s);
        if (s && !s->funcNode && !s->nativeFunc) {
            s->value = val;
            return;
        }
        scope = scope->prev;
    }
    
    // If not found, add to the current scope's hash table.
    Symbol* sym = malloc(sizeof(Symbol));
    sym->name = strdup(name);
    sym->value = val;
    sym->funcNode = NULL;
    sym->nativeFunc = NULL;
    HASH_ADD_STR(currentScope->symbols, name, sym);
}

// Helper: Define a function in the symbol table
void defineFunction(char* name, ASTNode* node) {
    Symbol* sym = malloc(sizeof(Symbol));
    sym->name = strdup(name);
    sym->funcNode = node;
    sym->nativeFunc = NULL;
    HASH_ADD_STR(currentScope->symbols, name, sym);
}

// Helper: Define a native C function
void defineNative(const char* name, NativeFn func) {
    Symbol* sym = malloc(sizeof(Symbol));
    sym->name = strdup(name);
    sym->funcNode = NULL;
    sym->nativeFunc = func;
    HASH_ADD_STR(currentScope->symbols, name, sym);
}

// Helper: Retrieve a callable symbol (user or native function)
Symbol* getCallable(char* name) {
    Scope* scope = currentScope;
    while (scope != NULL) {
        Symbol* s;
        HASH_FIND_STR(scope->symbols, name, s);
        if (s && (s->funcNode != NULL || s->nativeFunc != NULL)) {
            return s;
        }
        scope = scope->prev;
    }
    return NULL;
}

// New scope management functions
void enterScope() {
    Scope* newScope = malloc(sizeof(Scope));
    newScope->symbols = NULL;
    newScope->prev = currentScope;
    currentScope = newScope;
}

void exitScope() {
    if (currentScope && currentScope->prev) { // Don't pop the global scope
        Scope* toFree = currentScope;
        currentScope = currentScope->prev;

        Symbol *s, *tmp;
        HASH_ITER(hh, toFree->symbols, s, tmp) {
            HASH_DEL(toFree->symbols, s);
            free(s->name);
            free(s);
        }
        free(toFree);
    }
}

// This replaces pushVariable
void defineVariable(char* name, Value val) {
    Symbol* sym = malloc(sizeof(Symbol));
    sym->name = strdup(name);
    sym->value = val;
    sym->funcNode = NULL;
    sym->nativeFunc = NULL;
    HASH_ADD_STR(currentScope->symbols, name, sym);
}

void markRoots() {
    Scope* scope = currentScope;
    while (scope) {
        Symbol *s, *tmp;
        HASH_ITER(hh, scope->symbols, s, tmp) {
            markValue(s->value);
            // Note: We don't need to mark s->funcNode as it's part of the AST, not a GC'd object.
        }
        scope = scope->prev;
    }
}

void freeSymbolTable() {
    while (currentScope) {
        Scope* scopeToFree = currentScope;
        currentScope = currentScope->prev;

        Symbol *s, *tmp;
        HASH_ITER(hh, scopeToFree->symbols, s, tmp) {
            HASH_DEL(scopeToFree->symbols, s);
            if (s->name) free(s->name);
            // Note: We don't free s->value strings here as ownership is complex.
            // The GC will handle strings allocated with ml_alloc.
            free(s);
        }
        free(scopeToFree);
    }
}

static void freeObject(Object* object) {
    switch (object->type) {
        case OBJ_STRING:
            free(object); // ObjString is a single allocation
            break;
        case OBJ_LIST: {
            ValueList* list = (ValueList*)object;
            free(list->items); // Free the items array
            free(list);        // Free the list struct itself
            break;
        }
        case OBJ_DICT: {
            ValueDict* dict = (ValueDict*)object;
            DictEntry *s, *tmp;
            HASH_ITER(hh, dict->head, s, tmp) {
                HASH_DEL(dict->head, s);
                free(s->key); // Free the strdup'd key
                free(s);      // Free the DictEntry struct
            }
            free(dict); // Free the dict container
            break;
        }
    }
}

void sweep() {
    Object* previous = NULL;
    Object* object = allObjects;
    while (object != NULL) {
        if (object->isMarked) {
            object->isMarked = false; // Unmark for next cycle
            previous = object;
            object = object->next;
        } else {
            Object* unreached = object;
            object = object->next;
            if (previous != NULL) {
                previous->next = object;
            } else {
                allObjects = object;
            }
            freeObject(unreached);
        }
    }
}

void collectGarbage() {
    markRoots();
    sweep();
}

void printValueRecursive(Value val) {
    if (val.type == VAL_INT) printf("%d", val.as.number);
    else if (val.type == VAL_VOID) printf("non");
    else if (IS_STRING(val)) printf("%s", AS_CSTRING(val));
    else if (IS_LIST(val)) {
        ValueList* list = AS_LIST(val);
        printf("[");
        for (int i = 0; i < list->count; i++) {
            printValueRecursive(list->items[i]);
            if (i < list->count - 1) printf(", ");
        }
        printf("]");
    } else if (IS_DICT(val)) {
        ValueDict* dict = AS_DICT(val);
        printf("{");
        DictEntry *s, *tmp;
        int i = 0;
        int count = HASH_COUNT(dict->head);
        HASH_ITER(hh, dict->head, s, tmp) {
            printf("\"%s\": ", s->key);
            printValueRecursive(s->value);
            if (i < count - 1) printf(", ");
            i++;
        }
        printf("}");
    }
}

void printValue(Value val) {
    if (val.type == VAL_ERR) {
        // Errors are already printed when they occur
        return;
    }
    if (val.type == VAL_VOID) { // Don't print anything for void
        return;
    }
    printValueRecursive(val);
    printf("\n");
    fflush(stdout);
}

// Forward declaration
char* readFile(const char* path);

// --- Evaluator ---

Value evaluate(ASTNode* node) {
    if (node == NULL) return makeVoid();

    // 1. Literals
    if (node->type == NODE_NUMBER) {
        return makeInt(atoi(node->value));
    }
    if (node->type == NODE_STRING) {
        return makeObj((Object*)allocateString(node->value, strlen(node->value)));
    }
    
    // 2. Variables
    if (node->type == NODE_VARIABLE) {
        return getVariable(node->value);
    }
    // 3. Binary Operations
    if (node->type == NODE_BINARY_OP) {
        Value left = evaluate(node->left);
        if (left.type == VAL_ERR) return left;
        
        Value right = evaluate(node->right);
        if (right.type == VAL_ERR) return right;

        if (strcmp(node->value, "+") == 0) {
            if (left.type == VAL_INT && right.type == VAL_INT) {
                return makeInt(left.as.number + right.as.number);
            }
            if (IS_STRING(left) && IS_STRING(right)) {
                size_t len1 = AS_STRING(left)->length;
                size_t len2 = AS_STRING(right)->length;
                char* result_chars = malloc(len1 + len2 + 1);
                memcpy(result_chars, AS_CSTRING(left), len1);
                memcpy(result_chars + len1, AS_CSTRING(right), len2 + 1);
                ObjString* result_obj = allocateString(result_chars, len1 + len2);
                free(result_chars);
                return makeObj((Object*)result_obj);
            }
        }
        
        // For other operators, ensure we have numbers
        if (left.type != VAL_INT || right.type != VAL_INT) {
            return throwException(TypeException, "TypeException: Type mismatch in binary operation %s", node->value);
        }

        if (strcmp(node->value, "-") == 0) { // -
            if (left.type == VAL_FLOAT || right.type == VAL_FLOAT) {
                float l = left.type == VAL_INT ? (float_t)left.as.number : left.as.decimal;
                float r = right.type == VAL_INT ? (float_t)right.as.number : right.as.decimal;
                return makeFloat(l - r);
            }
            if (left.type == VAL_INT && right.type == VAL_INT) {
                return makeInt(left.as.number - right.as.number);
            }
        }
        if (strcmp(node->value, "+") == 0) { // +
            if (left.type == VAL_FLOAT || right.type == VAL_FLOAT) {
                float l = left.type == VAL_INT ? (float_t)left.as.number : left.as.decimal;
                float r = right.type == VAL_INT ? (float_t)right.as.number : right.as.decimal;
                return makeFloat(l + r);
            }
            if (left.type == VAL_INT && right.type == VAL_INT) {
                return makeInt(left.as.number + right.as.number);
            }
        }
        if (strcmp(node->value, "*") == 0) { // *
            if (left.type == VAL_FLOAT || right.type == VAL_FLOAT) {
                float l = left.type == VAL_INT ? (float_t)left.as.number : left.as.decimal;
                float r = right.type == VAL_INT ? (float_t)right.as.number : right.as.decimal;
                return makeFloat(l * r);
            }
            if (left.type == VAL_INT && right.type == VAL_INT) {
                return makeInt(left.as.number - right.as.number);
            }
        }
        if (strcmp(node->value, "/") == 0) {
            if (left.type == VAL_FLOAT || right.type == VAL_FLOAT) {
                float l = left.type == VAL_INT ? (float_t)left.as.number : left.as.decimal;
                float r = right.type == VAL_INT ? (float_t)right.as.number : right.as.decimal;
                if (r == 0) return throwException(DivideByZeroException, "DivideByZeroException: Division by zero.\n");
                return makeFloat(l / r);
            }
            if (left.type == VAL_INT && right.type == VAL_INT) {
                if(right.as.number == 0) throwException(DivideByZeroException, "DivideByZeroException: Division by zero. \n");
                return makeInt(left.as.number / right.as.number);
            }
        }
        if (strcmp(node->value, "//") == 0) {
            if (left.type == VAL_FLOAT || right.type == VAL_FLOAT) {
                float l = left.type == VAL_INT ? (float_t)left.as.number : left.as.decimal;
                float r = right.type == VAL_INT ? (float_t)right.as.number : right.as.decimal;
                if (r == 0) return throwException(DivideByZeroException, "DivideByZeroException: Division by zero.\n");
                return makeInt((int)floor(l / r));
            }
            if (left.type == VAL_INT && right.type == VAL_INT) {
                if (right.as.number == 0) return throwException(DivideByZeroException, "DivideByZeroException: Division by zero.\n");
                return makeInt((int)floor((float)left.as.number / (float)right.as.number));
            }
        }
        if (strcmp(node->value, "**")==0) {
            if (left.type == VAL_FLOAT || right.type == VAL_FLOAT) {
                float l = left.type == VAL_INT ? (float_t)left.as.number : left.as.decimal;
                float r = right.type == VAL_INT ? (float_t)right.as.number : right.as.decimal;
                return makeFloat(pow(l, r));
            }
            if (left.type == VAL_INT && right.type == VAL_INT) {
                return makeInt(pow(left.as.number, right.as.number));
            }
        }

        // Logic & Comparison
        if (strcmp(node->value, "and") == 0) return makeInt(left.as.number && right.as.number);
        if (strcmp(node->value, "or") == 0) return makeInt(left.as.number || right.as.number);
        
        if (strcmp(node->value, "==") == 0) {
            if (IS_STRING(left) && IS_STRING(right)) return makeInt(strcmp(AS_CSTRING(left), AS_CSTRING(right)) == 0);
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
        ASTNode* lvalue = node->left;
        Value rvalue = evaluate(node->right);
        if (rvalue.type == VAL_ERR) return rvalue;

        if (lvalue->type == NODE_VARIABLE) {
            setVariable(lvalue->value, rvalue);
            return rvalue;
        }

        if (lvalue->type == NODE_INDEX_ACCESS) {
            Value collection = evaluate(lvalue->left);
            if (collection.type == VAL_ERR) return collection;
            Value index = evaluate(lvalue->right);
            if (index.type == VAL_ERR) return index;

            if (IS_LIST(collection)) {
                if (index.type != VAL_INT) {return throwException(TypeException, "TypeException: List index must be an integer.\n");}
                int i = index.as.number;
                ValueList* list = AS_LIST(collection);
                if (i < 0 || i >= list->count) {
                    return throwException(IndexOutOfBoundsException, "IndexOutOfBoundsException: List index out of bounds.\n");
                }
                list->items[i] = rvalue;
                return rvalue;
            }

            if (IS_DICT(collection)) {
                if (!IS_STRING(index)) {
                    return throwException(TypeException, "TypeException: Dictionary key must be a string.\n");
                }
                char* key = AS_CSTRING(index);
                ValueDict* dict = AS_DICT(collection);
                DictEntry* entry;
                HASH_FIND_STR(dict->head, key, entry);
                if (entry) {
                    entry->value = rvalue;
                } else {
                    entry = malloc(sizeof(DictEntry));
                    entry->key = strdup(key);
                    entry->value = rvalue;
                    HASH_ADD_STR(dict->head, key, entry);
                }
                return rvalue;
            }

            return throwException(SyntaxException, "SyntaxException: Can only index into lists or dictionaries.\n");//Plans for string indexing exist
        }

        return throwException(RuntimeException, "RuntimeException: Invalid assignment target.\n");
    }

    // 5. Function Definitions
    if (node->type == NODE_FUNCTION_DEF) {
        defineFunction(node->value, node);
        return makeVoid();
    }

    // 6. Function Calls
    if (node->type == NODE_FUNCTION_CALL) {
        Symbol* callable = getCallable(node->value);
        if (!callable) {
            return throwException(IdentifierNotFoundException, "IdentifierNotFoundException: Function '%s' not defined.\n", node->value);
        }

        Value result = makeVoid();
        int argCount = node->childCount;

        // A. Evaluate Arguments
        Value* argValues = malloc(sizeof(Value) * argCount);
        for(int i=0; i<argCount; i++) {
            argValues[i] = evaluate(node->children[i]);
            if (argValues[i].type == VAL_ERR) {
                free(argValues);
                return argValues[i];
            }
        }

        if (callable->nativeFunc) {
            result = callable->nativeFunc(argCount, argValues);
        } else { // User-defined function
            ASTNode* funcDef = callable->funcNode;
            int paramCount = funcDef->childCount - 1;

            if (paramCount != argCount) {
                result = throwException(ArgumentException, "ArgumentException: Function '%s' expected %d arguments but got %d.\n", funcDef->value, paramCount, argCount);
                goto end_call;
            }

            enterScope();
            for(int i=0; i<paramCount; i++) {
                char* paramName = funcDef->children[i]->value;
                defineVariable(paramName, argValues[i]);
            }
            // B.2. Execute Body
            ASTNode* body = funcDef->children[paramCount];
            if (body && body->children) {
                for(int i=0; i<body->childCount; i++) {
                    ASTNode* stmt = body->children[i];
                    if (stmt->type == NODE_RETURN) {
                        result = makeReturn(evaluate(stmt->right));
                        goto end_call_user; // Return immediately
                    }
                    Value stmtVal = evaluate(stmt);
                    if (stmtVal.type == VAL_ERR) {
                        result = stmtVal;
                        goto end_call_user;
                    }
                }
            }
            end_call_user: exitScope();
        }

        end_call: free(argValues);
        return result;
    }

    // List
    if (node->type == NODE_LIST_LITERAL) {
        ValueList* list = allocateList(node->childCount);
        list->count = node->childCount;

        for (int i = 0; i < list->count; i++) {
            list->items[i] = evaluate(node->children[i]);
            if (list->items[i].type == VAL_ERR) {
                // The GC will clean up the partially created list if an error is thrown.
                return list->items[i];
            }
        }
        return makeObj((Object*)list);
    }

    // Dictionary
    if (node->type == NODE_DICT_LITERAL) {
        ValueDict* dict = allocateDict();
        for (int i = 0; i < node->childCount; i += 2) {
            Value keyVal = evaluate(node->children[i]);
            if (!IS_STRING(keyVal)) {
                return throwException(TypeException, "TypeException: Dictionary keys must be strings.\n");
            }

            Value val = evaluate(node->children[i+1]);
            if (val.type == VAL_ERR) return val;

            char* key = AS_CSTRING(keyVal);
            DictEntry* entry;
            HASH_FIND_STR(dict->head, key, entry);
            if (entry) {
                entry->value = val;
            } else {
                entry = malloc(sizeof(DictEntry));
                entry->key = strdup(key); // The key is owned by the dict entry now
                entry->value = val;
                HASH_ADD_STR(dict->head, key, entry);
            }
        }
        return makeObj((Object*)dict);
    }

    // Index Access
    if (node->type == NODE_INDEX_ACCESS) {
        Value collection = evaluate(node->left);
        if (collection.type == VAL_ERR) return collection;
        Value index = evaluate(node->right);
        if (index.type == VAL_ERR) return index;

        if (IS_LIST(collection)) {
            if (index.type != VAL_INT) { return throwException(TypeException, "TypeException: List index must be an integer.\n"); }
            int i = index.as.number;
            ValueList* list = AS_LIST(collection);
            if (i < 0 || i >= list->count) { return throwException(IndexOutOfBoundsException, "IndexOutOfBoundsException: List index out of bounds.\n"); }
            return list->items[i];
        }

        if (IS_DICT(collection)) {
            if (!IS_STRING(index)) { return throwException(TypeException, "TypeException: Dictionary key must be a string.\n"); }
            ValueDict* dict = AS_DICT(collection);
            char* key = AS_CSTRING(index);
            DictEntry* entry;
            HASH_FIND_STR(dict->head, key, entry);
            if (entry) return entry->value;
            
            return throwException(EntryNotFoundException, "EntryNotFoundException: Key '%s' not found in dictionary.\n", key);
        }
        return throwException(SyntaxException, "SyntaxException: Can only index into lists or dictionaries.\n");
    }

    // 8. Native Print
    if (node->type == NODE_PRINT) {
        Value val = evaluate(node->right);
        if (val.type == VAL_ERR) return val;
        printValue(val);
        return makeVoid();
    }

    // 9. If Statements
    if (node->type == NODE_IF) {
        Value cond = evaluate(node->children[0]);
        if (cond.type == VAL_ERR) return cond;
        // Treat 0 as false, anything else as true
        int isTrue = 0; // Default to false
        if (cond.type == VAL_INT) isTrue = cond.as.number != 0;
        if (IS_STRING(cond)) isTrue = AS_STRING(cond)->length > 0; // Empty string is false, non-empty is true

        if (isTrue) {
            Value res = evaluate(node->children[1]); // Execute body once
            if (res.type == VAL_ERR || res.type == VAL_RETURN || res.type == VAL_BREAK) return res;
        }
        return makeVoid();
    }

    // 10. While Loops
    if (node->type == NODE_WHILE) {
        while(1) {
            Value cond = evaluate(node->children[0]);
            if (cond.type == VAL_ERR) return cond;
            int isTrue = 0; // Default to false
            if (cond.type == VAL_INT) isTrue = cond.as.number != 0;
            if (IS_STRING(cond)) isTrue = AS_STRING(cond)->length > 0; // Empty string is false, non-empty is true

            if (!isTrue) break;

            Value res = evaluate(node->children[1]); // Execute body once
            if (res.type == VAL_ERR) return res;
            if (res.type == VAL_BREAK) break;
            if (res.type == VAL_RETURN) return res;
        }
        return makeVoid();
    }

    // 11. Import
    if (node->type == NODE_IMPORT) {
        Value pathVal = evaluate(node->right);
        if (!IS_STRING(pathVal)) {
            return throwException(TypeException, "TypeException: Import path must be a string.\n");
        }
        char* source = readFile(AS_CSTRING(pathVal));
        if (strcmp(source, "")==0){return throwException(FileNotFoundException, "FileNotFoundException: File coudn't be found or is not existant.");}
        TokenStream* Stream = lex(source);
        ASTNode* Root = parseFile(*Stream);
        if (Root) {
            Value Result = evaluate(Root);
            freeAST(Root);
            if(Result.type == VAL_ERR) return Result; //pass the error up
        }
        finalCleanup(Stream);
        free(source);
        return makeVoid();
    }

    // 12. Break
    if (node->type == NODE_BREAK) {
        return makeBreak();
    }

    // 7. Program / Block Execution
    if (node->type == NODE_PROGRAM) {
        Value lastResult = makeVoid();
        if (node->children) {
            for(int i=0; i<node->childCount; i++) {
                lastResult = evaluate(node->children[i]);
                if (lastResult.type == VAL_ERR) return lastResult;
                if (lastResult.type == VAL_RETURN) return lastResult;
                if (lastResult.type == VAL_BREAK) return lastResult;
            }
        }
        return lastResult;
    }

    return makeVoid();
}

// --- Native C Functions ---

Value native_input(int argCount, Value* args) {
    (void)args; // Unused parameter
    if (argCount != 0) {
        return throwException(ArgumentException, "ArgumentException: input() takes 0 arguments.\n");
    }

    char line[1024];
    if (fgets(line, sizeof(line), stdin)) {
        // Remove trailing newline
        line[strcspn(line, "\n")] = 0;
        return makeObj((Object*)allocateString(line, strlen(line)));
    }
    return makeObj((Object*)allocateString("", 0)); // Return empty string on EOF or error
}

Value native_clock(int argCount, Value* args) {
    (void)args; // Unused parameter
    if (argCount != 0) {
        return throwException(ArgumentException, "ArgumentException: clock() takes 0 arguments.\n");
    }
    // Return milliseconds
    return makeInt((int)(clock() * 1000 / CLOCKS_PER_SEC));
}

// --- Initialization ---

void initSymbolTable() {
    currentScope = malloc(sizeof(Scope));
    currentScope->symbols = NULL;
    currentScope->prev = NULL;
}

void initNatives() {
    // These will be defined in the global scope created by initSymbolTable
    defineNative("input", native_input);
    defineNative("clock", native_clock);
}

// Helper function to read a file into a string buffer
char* readFile(const char* path) {
    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        printf("File not found: %s", path);
        return "";
    }

    fseek(file, 0L, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);

    char* buffer = (char*)malloc(fileSize + 1);
    if (buffer == NULL) {
        exit(74);
    }

    size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
    if (bytesRead < fileSize && !feof(file)) {
        exit(74);
    }

    buffer[bytesRead] = '\0'; // Lo hace una string

    fclose(file);
    return buffer;
}

void runREPL() {
    char line[1024];
    printf("Meaningful Shell %s\n", VERSION);
    printf("Type 'exit' to quit.\n");

    while (1) {
        printf(">>>");
        if (!fgets(line, sizeof(line), stdin)) break;

        // Remove the newline character
        line[strcspn(line, "\n")] = 0;

        if (strcmp(line, "exit") == 0) break;
        if (strlen(line) == 0) continue;

        // 1. Lex
        TokenStream* stream = lex(line);
        // 2. Parse
        ASTNode* root = parseFile(*stream);

        if (root) {
            // 3. Evaluate
            Value rslt = evaluate(root); // NOTE: everything needed is printed in evaluate, no need to print here.
            if (rslt.type == VAL_ERR) {freeAST(root);finalCleanup(stream);break;}
            freeAST(root);
        }

        // 4. Cleanup for this turn
        finalCleanup(stream);
    }
    // Cleanup when REPL exits
    freeSymbolTable();
    collectGarbage();
}

int main(int argc, char *argv[]){
    if (argc != 2 && argc != 1) {
        printf("Usage: meaningful [name].mean\n");
        return 64;
    } 
    initSymbolTable();
    initNatives();
    if (argc == 1){
        runREPL();
        return 0;
    }

    if (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0){
        printf("Meaningful Language, Licenced under BMLL 2.0\n                      Version %s", VERSION);
        return 0;
    }
    if (strcmp(argv[1], "-src") == 0 || strcmp(argv[1], "--source") == 0){
        system("start https://github.com/Banaolf/Meaningful-Language");
        return 0;
    }
    if (strcmp(argv[1], "-lic") == 0 || strcmp(argv[1], "--licence") == 0){
        system("start https://github.com/Banaolf/Meaningful-Language/blob/main/LICENCE");
        return 0;
    }

    char* src = readFile(argv[1]);
    if (strcmp(src, "")==0){
        exit(2);
    }
    TokenStream* stream = lex(src);
    ASTNode* root = parseFile(*stream);

    if (root) {
        Value result = evaluate(root);
        if (result.type == VAL_ERR) {
            return 99;
        }
        freeAST(root);
    }

    finalCleanup(stream);
    free(src);
    freeSymbolTable();
    collectGarbage();
    return 0;
}