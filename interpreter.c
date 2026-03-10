#include "parser.h"
#include "lexer.h"
#include <ctype.h>
#include <stddef.h>
#include <io.h>
#include <stddef.h>
#include <stdarg.h>

#include <math.h>
#include <stdlib.h> 
#include <stdbool.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include "uthash.h"
#define VERSION "ALPHA.0.99.53.1"
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
    VAL_BREAK,
    VAL_NON,
    VAL_FUNCTION
} ValueType;

typedef enum {
    list,
    string,
    integer,
    Float,
    dict,
    file,
    function
} AllTypes;

typedef enum {
    OBJ_STRING,
    OBJ_LIST,
    OBJ_DICT,
    OBJ_FILE
} ObjectType;

// The header for all heap-allocated objects in the language
struct Object {
    ObjectType type;
    bool isMarked;
    struct Object* next; // Intrusive list to track all allocations
};

typedef struct ObjString {
    Object obj;
    size_t length;
    char chars[];
} ObjString;

typedef struct ObjFile {
    Object obj;
    FILE* file;
    char* path;
    char* contents;
    bool isOpen;
} ObjFile;

//Helpful macros :)
#define IS_OBJECT(value)    ((value).type == VAL_OBJECT)
#define AS_OBJECT(value)    ((value).as.obj)
#define OBJ_TYPE(value)     (AS_OBJECT(value)->type)
#define IS_OBJ_TYPE(value, type)  (IS_OBJECT(value) && OBJ_TYPE(value) == type) //Replace all IS_ with this.

#define AS_STRING(value)    ((ObjString*)AS_OBJECT(value))
#define AS_CSTRING(value)   (AS_STRING(value)->chars)
#define AS_DICT(value)      ((ValueDict*)AS_OBJECT(value))
#define AS_LIST(value)      ((ValueList*)AS_OBJECT(value))
#define AS_FILE(value)      ((ObjFile*)AS_OBJECT(value))

typedef struct Symbol Symbol;
// The language's core Value type. Can be a number or a pointer to an Object.
struct Value {
    ValueType type;
    ValueType returnType;
    union {
        int number;
        Object* obj;
        Exception exc;
        float decimal;
        Symbol* func;
    } as;
};

typedef struct ValueList {
    Object obj;
    Value* items;
    int count;
    int capacity;
} ValueList;


typedef struct DictEntry {
    char* key;
    Value value;
    UT_hash_handle hh;
} DictEntry;

typedef struct ValueDict {
    Object obj;
    DictEntry* head;
    int len;
} ValueDict;

bool isVal(Value val, ValueType type){
    if (val.type == type) {
        return true;
    } else {
        return false;
    }
}

// Value constructors
Value make(ValueType type, ...) {
    va_list args;
    va_start(args, type);
    Value result;
    result.type = type;
    if (type == VAL_INT){ // Takes one argument, int
        int argument = va_arg(args, int);
        result.as.number = argument;
    } else if (type == VAL_OBJECT) {
        Object* argument = va_arg(args, Object*);
        result.as.obj = argument;
    } else if (type == VAL_FLOAT) {
        double argument = va_arg(args, double);
        result.as.decimal = argument;
    } else if (type == VAL_RETURN) {
        Value argument = va_arg(args, Value);
        result.as.obj = argument.as.obj;
        result.as.number = argument.as.number;
        result.as.decimal = argument.as.decimal;
        result.as.exc = argument.as.exc;
    } else if (type == VAL_FUNCTION) {
        Symbol* arg = va_arg(args, Symbol*);
        result.as.func = arg;
    }
    va_end(args);
    return result;
}

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

bool checkType(char* notation, Value val) {
    if (strcmp(notation, "string") == 0) return IS_OBJ_TYPE(val, OBJ_STRING);
    if (strcmp(notation, "int")    == 0) return val.type == VAL_INT;
    if (strcmp(notation, "float")  == 0) return val.type == VAL_FLOAT;
    if (strcmp(notation, "list")   == 0) return IS_OBJ_TYPE(val, OBJ_LIST);
    if (strcmp(notation, "dict")   == 0) return IS_OBJ_TYPE(val, OBJ_DICT);
    if (strcmp(notation, "file")   == 0) return IS_OBJ_TYPE(val, OBJ_FILE);
    if (strcmp(notation, "bool")   == 0) return val.type == VAL_INT; // booleans are ints internally
    if (strcmp(notation, "non") == 0) return val.type == VAL_NON;
    if (strcmp(notation, "function") == 0) return val.type == VAL_FUNCTION; // how the hell do i do this?
    //Note: Handle @optional elsewhere
    return false;
}

bool checkTypeConversable(char* notation) {
    if (strcmp(notation, "string") == 0) return true;
    if (strcmp(notation, "int")    == 0) return true;
    if (strcmp(notation, "float")  == 0) return true;
    if (strcmp(notation, "list")   == 0) return true;
    if (strcmp(notation, "dict")   == 0) return true;
    if (strcmp(notation, "bool")   == 0) return true;
    return false;
}

bool checkCondition(char* condition, Value val) {
    if (val.type == VAL_OBJECT) {
        if (strcmp(condition, "not_empty") == 0) {
            if (IS_OBJ_TYPE(val, OBJ_STRING)) return AS_STRING(val)->length > 0;
            if (IS_OBJ_TYPE(val, OBJ_LIST))   return AS_LIST(val)->count > 0;
            if (IS_OBJ_TYPE(val, OBJ_DICT))   return HASH_COUNT(AS_DICT(val)->head) > 0;
            if (IS_OBJ_TYPE(val, OBJ_FILE)) 
            return true;
        }
    } else if (val.type == VAL_INT || val.type == VAL_FLOAT) { //so general numbers
        if (strcmp(condition, "positive_with_zero") == 0) {
            if (val.type == VAL_INT)   return val.as.number >= 0;
            if (val.type == VAL_FLOAT) return val.as.decimal >= 0;
            return true;
        }
        if (strcmp(condition, "positive") == 0) {
            if (val.type == VAL_INT)   return val.as.number > 0;
            if (val.type == VAL_FLOAT) return val.as.decimal > 0;
            return true;
        }
    }
    return NULL;
}

// --- Garbage Collector ---

Object* allObjects = NULL; 

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
        case OBJ_FILE: {
            break; // Files don't have outgoing references
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

bool is_falsy(Value val) {
    return ( //Made easy to edit for the future
        (val.type == VAL_INT && val.as.number == 0) || 
        (IS_OBJ_TYPE(val, OBJ_STRING) && AS_STRING(val)-> length == 0) || 
        val.type == VAL_NON || 
        (val.type == VAL_FLOAT && val.as.decimal == 0.0f));
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
            free(s);
        }
        free(scopeToFree);
    }
}

ObjFile* currentFile;

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
        case OBJ_FILE: {
            ObjFile* file = (ObjFile*)object;
            if (file->isOpen && file->file != NULL) {fclose(file->file); file->isOpen = false;}
            file->file = NULL;
            if (file->path != NULL) {
                free(file->path);
                file->path = NULL;
            }
            if (file->contents != NULL) { // If the contents are already NULL means it was never assigned meaning no need to free it.
                free(file->contents);
                file->contents = NULL;
            }
            free(file);
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
    else if (val.type == VAL_NON) printf("non");
    else if (val.type == VAL_FLOAT) printf("%g", val.as.decimal);
    else if (IS_OBJ_TYPE(val, OBJ_STRING)) printf("%s", AS_CSTRING(val));
    else if (val.type == VAL_FUNCTION) printf("<function %s>", val.as.func->name);
    else if (IS_OBJ_TYPE(val, OBJ_LIST)) {
        ValueList* list = AS_LIST(val);
        printf("[");
        for (int i = 0; i < list->count; i++) {
            printValueRecursive(list->items[i]);
            if (i < list->count - 1) printf(", ");
        }
        printf("]");
    } else if (IS_OBJ_TYPE(val, OBJ_DICT)) {
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
void printValueReprRecursive(Value val) { //Sorry for the repetition, I couldn't find any other more efficient way.
    if (val.type == VAL_INT) printf("%d", val.as.number);
    else if (val.type == VAL_NON) printf("non");
    else if (val.type == VAL_FLOAT) printf("%g", val.as.decimal);
    else if (IS_OBJ_TYPE(val, OBJ_STRING)) printf("\"%s\"", AS_CSTRING(val));
    else if (val.type == VAL_FUNCTION) printf("<function %s>", val.as.func->name);
    else if (IS_OBJ_TYPE(val, OBJ_LIST)) {
        ValueList* list = AS_LIST(val);
        printf("[");
        for (int i = 0; i < list->count; i++) {
            printValueReprRecursive(list->items[i]);
            if (i < list->count - 1) printf(", ");
        }
        printf("]");
    } else if (IS_OBJ_TYPE(val, OBJ_DICT)) {
        ValueDict* dict = AS_DICT(val);
        printf("{");
        DictEntry *s, *tmp;
        int i = 0;
        int count = HASH_COUNT(dict->head);
        HASH_ITER(hh, dict->head, s, tmp) {
            printf("\"%s\": ", s->key);
            printValueReprRecursive(s->value);
            if (i < count - 1) printf(", ");
            i++;
        }
        printf("}");
    }
}

void printValue(Value val) {
    if (val.type == VAL_ERR) {return;}
    if (val.type == VAL_VOID) {return;}
    printValueRecursive(val);
    printf("\n");
    fflush(stdout);
}

void printReprValue(Value val) {
    if (val.type == VAL_ERR) {return;}
    if (val.type == VAL_VOID) {return;}
    printValueReprRecursive(val);
    printf("\n");
    fflush(stdout);
}


// Forward declaration
char* readFile(const char* path);

char* valueToString(Value val) {
    char tmp[64];
    if (IS_OBJ_TYPE(val, OBJ_STRING)) {
        return strdup(AS_CSTRING(val));
    } else if (val.type == VAL_INT) {
        snprintf(tmp, sizeof(tmp), "%d", val.as.number);
        return strdup(tmp);
    } else if (val.type == VAL_FLOAT) {
        snprintf(tmp, sizeof(tmp), "%g", val.as.decimal);
        return strdup(tmp);
    } else if (val.type == VAL_NON || val.type == VAL_VOID) {
        return strdup("non");
    } else if (IS_OBJ_TYPE(val, OBJ_LIST)) {
        ValueList* list = AS_LIST(val);
        size_t cap = 64;
        size_t len = 0;
        char* buf = malloc(cap);
        buf[len++] = '[';
        buf[len] = '\0';
        for (int i = 0; i < list->count; i++) {
            char* item = valueToString(list->items[i]);
            size_t itemLen = strlen(item);
            while (len + itemLen + 4 > cap) { cap *= 2; buf = realloc(buf, cap); }
            memcpy(buf + len, item, itemLen);
            len += itemLen;
            free(item);
            if (i < list->count - 1) { buf[len++] = ','; buf[len++] = ' '; }
        }
        buf[len++] = ']';
        buf[len] = '\0';
        return buf;
    } else if (IS_OBJ_TYPE(val, OBJ_DICT)) {
        ValueDict* dict = AS_DICT(val);
        size_t cap = 64;
        size_t len = 0;
        char* buf = malloc(cap);
        buf[len++] = '{';
        buf[len] = '\0';
        DictEntry *s, *tmp2;
        int i = 0, count = HASH_COUNT(dict->head);
        HASH_ITER(hh, dict->head, s, tmp2) {
            // Key
            size_t keyLen = strlen(s->key);
            while (len + keyLen + 6 > cap) { cap *= 2; buf = realloc(buf, cap); }
            buf[len++] = '"';
            memcpy(buf + len, s->key, keyLen);
            len += keyLen;
            buf[len++] = '"';
            buf[len++] = ':'; buf[len++] = ' ';
            buf[len] = '\0';
            // Value
            char* valStr = valueToString(s->value);
            size_t valLen = strlen(valStr);
            while (len + valLen + 4 > cap) { cap *= 2; buf = realloc(buf, cap); }
            memcpy(buf + len, valStr, valLen);
            len += valLen;
            free(valStr);
            if (i < count - 1) { buf[len++] = ','; buf[len++] = ' '; }
            i++;
        }
        buf[len++] = '}';
        buf[len] = '\0';
        return buf;
    }
    return strdup("<unknown>");
}

Value toString(Value val) {
    char* str = valueToString(val);
    Value result = make(VAL_OBJECT, (Object*)allocateString(str, strlen(str)));
    free(str);
    return result;
}

// --- Evaluator ---

Value evaluate(ASTNode* node) {
    if (node == NULL) return make(VAL_VOID);

    // 1. Literals
    if (node->type == NODE_NUMBER) {
        return make(VAL_INT, atoi(node->value));
    }
    if (node->type == NODE_FLOAT) {
        return make(VAL_FLOAT, atof(node->value));
    }
    if (node->type == NODE_STRING) {
        return make(VAL_OBJECT, (Object*)allocateString(node->value, strlen(node->value)));
    }
    if (node->type == NODE_NON) {
        return make(VAL_NON);
    }
    
    // 2. Variables
    if (node->type == NODE_VARIABLE) { // Prioritise variables over functions
        Value v = getVariable(node->value);
        if (v.type != VAL_ERR) return v;
        Symbol* callable = getCallable(node->value);
        if (callable) return make(VAL_FUNCTION, callable);
        return v; // return the original error
    }

    // 3. Operations
    if (node->type == NODE_OPERATOR) {
        Value left = evaluate(node->left);
        if (left.type == VAL_ERR) return left;
        
        Value right = evaluate(node->right);
        if (right.type == VAL_ERR) return right;

        // String concatenation
        if (strcmp(node->value, "+") == 0 && IS_OBJ_TYPE(left, OBJ_STRING) && IS_OBJ_TYPE(right, OBJ_STRING)) {
            size_t len1 = AS_STRING(left)->length;
            size_t len2 = AS_STRING(right)->length;
            char* result_chars = malloc(len1 + len2 + 1);
            memcpy(result_chars, AS_CSTRING(left), len1);
            memcpy(result_chars + len1, AS_CSTRING(right), len2 + 1);
            ObjString* result_obj = allocateString(result_chars, len1 + len2);
            free(result_chars);
            return make(VAL_OBJECT, (Object*)result_obj);
        }

        // Float/int arithmetic (promote to float if either side is float)
        if (strcmp(node->value, "+") == 0 || strcmp(node->value, "-") == 0 ||
            strcmp(node->value, "*") == 0 || strcmp(node->value, "/") == 0 ||
            strcmp(node->value, "//") == 0 || strcmp(node->value, "**") == 0 ||
            strcmp(node->value, "%%") == 0 || strcmp(node->value, "-/") == 0 ) {

            bool eitherFloat = (left.type == VAL_FLOAT || right.type == VAL_FLOAT);
            float l = left.type == VAL_FLOAT ? left.as.decimal : (float)left.as.number;
            float r = right.type == VAL_FLOAT ? right.as.decimal : (float)right.as.number;

            if (left.type != VAL_INT && left.type != VAL_FLOAT)
                return throwException(TypeException, "TypeException: Type mismatch in binary operation '%s'.\n", node->value);
            if (right.type != VAL_INT && right.type != VAL_FLOAT)
                return throwException(TypeException, "TypeException: Type mismatch in binary operation '%s'.\n", node->value);
            if (strcmp(node->value, "+") == 0) {
                return eitherFloat ? make(VAL_FLOAT, l + r) : make(VAL_INT, (int)(l + r));
            }
            if (strcmp(node->value, "-") == 0) {
                return eitherFloat ? make(VAL_FLOAT, l - r) : make(VAL_INT, (int)(l - r));
            }
            if (strcmp(node->value, "*") == 0) {
                return eitherFloat ? make(VAL_FLOAT, l * r) : make(VAL_INT, (int)(l * r));
            }
            if (strcmp(node->value, "/") == 0) {
                if (r == 0) return throwException(DivideByZeroException, "DivideByZeroException: Division by zero.\n");
                return make(VAL_FLOAT, l / r); // true division always returns float
            }
            if (strcmp(node->value, "//") == 0) {
                if (r == 0) return throwException(DivideByZeroException, "DivideByZeroException: Division by zero.\n");
                return make(VAL_INT, (int)floor(l / r));
            }
            if (strcmp(node->value, "**") == 0) {
                float res = powf(l, r);
                return eitherFloat ? make(VAL_FLOAT, res) : make(VAL_INT, (int)res);
            } 
            if (strcmp(node->value, "%%") == 0) {
                if (r == 0) return throwException(DivideByZeroException, "DivideByZeroException: Division by zero.\n");
                return make(VAL_INT, (int)l % (int)r);
            }
            if (strcmp(node->value, "-/") == 0) {
                float base = left.type == VAL_FLOAT ? left.as.decimal : (float)left.as.number;
                float num  = right.type == VAL_FLOAT ? right.as.decimal : (float)right.as.number;
                if (base == 0) return throwException(DivideByZeroException, "DivideByZeroException: Root base cannot be 0.\n");
                float result = powf(num, 1.0f / base);
                // Return int only if both inputs were ints and result is whole
                if (left.type == VAL_INT && right.type == VAL_INT && floorf(result) == result) {
                    return make(VAL_INT, (int)result);
                }
                return make(VAL_FLOAT, result);
            }
        }
    }

    if (node->type == NODE_COMPARRISON) {
        Value left = evaluate(node->left);
        if (left.type == VAL_ERR) return left;
        
        Value right = evaluate(node->right);
        if (right.type == VAL_ERR) return right;

        if (strcmp(node->value, "==") == 0 || strcmp(node->value, "equals") == 0) {
            if (IS_OBJ_TYPE(left, OBJ_STRING) && IS_OBJ_TYPE(right, OBJ_STRING)) return make(VAL_INT, strcmp(AS_CSTRING(left), AS_CSTRING(right)) == 0);
            return make(VAL_INT, left.as.number == right.as.number);
        }
        if (strcmp(node->value, "!=") == 0 || strcmp(node->value, "isnt") == 0) return make(VAL_INT, left.as.number != right.as.number);
        if (strcmp(node->value, "<")  == 0) return make(VAL_INT, left.as.number < right.as.number);
        if (strcmp(node->value, ">")  == 0) return make(VAL_INT, left.as.number > right.as.number);
        if (strcmp(node->value, "<=") == 0) return make(VAL_INT, left.as.number <= right.as.number);
        if (strcmp(node->value, ">=") == 0) return make(VAL_INT, left.as.number >= right.as.number);
    }

    if (node->type == NODE_LOGIC){
        Value left = evaluate(node->left);
        if (left.type == VAL_ERR) return left;
        
        Value right = evaluate(node->right);
        if (right.type == VAL_ERR) return right;

        if (strcmp(node->value, "and") == 0) return make(VAL_INT, !is_falsy(left) && !is_falsy(right));
        if (strcmp(node->value, "or") == 0)  return make(VAL_INT, !is_falsy(left) || !is_falsy(right));
    }

    if (node->type == NODE_UNARY) {
        Value operand = evaluate(node->right);
        if (operand.type == VAL_ERR) return operand;
        if (is_falsy(operand)) return make(VAL_INT, 1);
        return make(VAL_INT, 0);
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

            if (IS_OBJ_TYPE(collection, OBJ_LIST)) {
                if (index.type != VAL_INT) return throwException(TypeException, "TypeException: List index must be an integer.\n");
                int i = index.as.number;
                ValueList* list = AS_LIST(collection);
                if (i < 0 || i >= list->count) return throwException(IndexOutOfBoundsException, "IndexOutOfBoundsException: List index out of bounds.\n");
                list->items[i] = rvalue;
                return rvalue;
            }

            if (IS_OBJ_TYPE(collection, OBJ_DICT)) {
                if (!IS_OBJ_TYPE(index, OBJ_STRING)) return throwException(TypeException, "TypeException: Dictionary key must be a string.\n");
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

            return throwException(SyntaxException, "SyntaxException: Can only index into lists or dictionaries.\n");
        }

        // Member assignment: obj.key = value  (obj must be a dict)
        if (lvalue->type == NODE_MEMBER_ACCESS) {
            Value obj = evaluate(lvalue->left);
            if (obj.type == VAL_ERR) return obj;
            if (!IS_OBJ_TYPE(obj, OBJ_DICT)) return throwException(TypeException, "TypeException: Attribute assignment is only supported on dictionaries.\n");

            ValueDict* dict = AS_DICT(obj);
            char* key = lvalue->value; // attribute name is stored in node->value
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

        return throwException(RuntimeException, "RuntimeException: Invalid assignment target.\n");
    }

    //4.1 Conversions
    if (node->type == NODE_CONVERSION) {
        Value var = getVariable(node->left->value);
        if (var.type == VAL_ERR) return var;

        char* right = node->right->value;
        if (checkTypeConversable(right)) {
            if (strcmp(right, "int") == 0) {
                if (var.type == VAL_OBJECT && IS_OBJ_TYPE(var, OBJ_STRING)) {
                    for (int i = 0; i < AS_STRING(var)->length; i++) {
                        if (!isdigit((unsigned char)AS_CSTRING(var)[i])) return throwException(TypeException, "TypeException: All characters must be digits in order to convert to integer");                        
                    }
                    return make(VAL_INT, atoi(AS_CSTRING(var)));
                } else if (var.type == VAL_FLOAT) {
                    return make(VAL_INT, floor(var.as.decimal));
                } else if (IS_OBJ_TYPE(var, OBJ_DICT)) {
                    return make(VAL_INT, HASH_COUNT(AS_DICT(var)->head));
                } else if (IS_OBJ_TYPE(var, OBJ_LIST)) {
                    return make(VAL_INT, AS_LIST(var)->count);
                } else if (var.type == VAL_INT) {
                    return var;
                } else return throwException(ConversionException, "ConversionException: Cannot transform '%s' to integer.\n", right);
            } else if (strcmp(right, "string") == 0) {
                return toString(var);
            } else if (strcmp(right, "list") == 0) {
                return make(VAL_OBJECT, AS_LIST(var));
            } else if (strcmp(right, "dict")==0) {
                return make(VAL_OBJECT, AS_DICT(var));
            } else if (strcmp(right, "bool") == 0) {
                if (is_falsy(var)) return make(VAL_INT, 0);
                return make(VAL_INT, 1);
            } else if (strcmp(right, "float") == 0) {
                if (var.type == VAL_OBJECT && IS_OBJ_TYPE(var, OBJ_STRING)) {
                    for (int i = 0; i < AS_STRING(var)->length; i++) {
                        if (!isdigit((unsigned char)AS_CSTRING(var)[i]) || (unsigned char)AS_CSTRING(var)[i] != '.') return throwException(TypeException, "TypeException: All characters must be digits in order to convert to integer");                        
                    }
                    return make(VAL_FLOAT, (float)atof(AS_CSTRING(var)));
                } else if (var.type == VAL_FLOAT) {
                    return var;
                } else if (IS_OBJ_TYPE(var, OBJ_DICT)) {
                    return make(VAL_FLOAT, (float)HASH_COUNT(AS_DICT(var)->head)); //This is meant to do this (which is the same length())
                } else if (IS_OBJ_TYPE(var, OBJ_LIST)) {
                    return make(VAL_FLOAT, (float)AS_LIST(var)->count);
                } else if (var.type == VAL_INT) {
                    return make(VAL_FLOAT, (float)var.as.number);
                } else return throwException(ConversionException, "ConversionException: Cannot transform '%s' to integer.\n", right);
            } else return throwException(ConversionException, "ConversionException: Cannot convert to '%s'.\n", right);
        } else return throwException(TypeException, "TypeException: Type does not exist or can't be converted to.");
    }

    // 5. Function Definitions
    if (node->type == NODE_FUNCTION_DEF) {
        defineFunction(node->value, node);
        return make(VAL_VOID);
    }

    // 6. Function Calls
    if (node->type == NODE_FUNCTION_CALL) {
        Symbol* callable = getCallable(node->value);
        if (!callable) {
            return throwException(IdentifierNotFoundException, "IdentifierNotFoundException: Function '%s' not defined.\n", node->value);
        }
        Value result = make(VAL_VOID);
        int argCount = node->childCount;
        Value* argValues = malloc(sizeof(Value) * (argCount > 0 ? argCount : 1));
        for (int i = 0; i < argCount; i++) {
            argValues[i] = evaluate(node->children[i]);
            if (argValues[i].type == VAL_ERR) { free(argValues); return argValues[i]; }
        }
        if (callable->nativeFunc) {
            result = callable->nativeFunc(argCount, argValues);
        } else {
            ASTNode* funcDef = callable->funcNode;
            int paramCount = funcDef->childCount - 1;
            if (paramCount != argCount) {
                result = throwException(ArgumentException, "ArgumentException: Function '%s' expected %d arguments but got %d.\n", funcDef->value, paramCount, argCount);
                goto end_call;
            }
            enterScope();
            for (int i = 0; i < paramCount; i++) {
                ASTNode* param = funcDef->children[i];
                Value arg = argValues[i];

                if (param->type_notation) {
                    if (!checkType(param->type_notation, arg)) {
                        result = throwException(TypeException,
                            "TypeException: Argument '%s' expected type '%s'.\n",
                            param->value, param->type_notation);
                        exitScope();
                        goto end_call;
                    }
                }

                if (param->type_condition) {
                    if (!checkCondition(param->type_condition, arg)) {
                        result = throwException(ArgumentException,
                            "ArgumentException: Argument '%s' failed condition '%s'.\n",
                            param->value, param->type_condition);
                        exitScope();
                        goto end_call;
                    }
                }

                defineVariable(param->value, arg);
            }
            ASTNode* body = funcDef->children[paramCount];
            if (body && body->children) {
                for (int i = 0; i < body->childCount; i++) {
                    ASTNode* stmt = body->children[i];
                    if (stmt->type == NODE_RETURN) {
                        Value inner = evaluate(stmt->right);
                        result = make(VAL_RETURN, inner);
                        result.returnType = inner.type;
                        goto end_call_user;
                    }
                    Value stmtVal = evaluate(stmt);
                    if (stmtVal.type == VAL_ERR) { result = stmtVal; goto end_call_user; }
                }
            }
            end_call_user: 
                exitScope();
                if (result.type == VAL_RETURN) result.type = result.returnType;
        }
        end_call: 
            free(argValues);
            return result;
    }

    // 6b. Method Calls
    if (node->type == NODE_METHOD_CALL) {
        Value obj = evaluate(node->left);
        if (obj.type == VAL_ERR) return obj;

        int argCount = node->childCount;
        Value* argValues = malloc(sizeof(Value) * (argCount > 0 ? argCount : 1));
        for (int i = 0; i < argCount; i++) {
            argValues[i] = evaluate(node->children[i]);
            if (argValues[i].type == VAL_ERR) { free(argValues); return argValues[i]; }
        }

        const char* method = node->value;
        Value result = make(VAL_VOID);

        // --- String methods ---
        if (IS_OBJ_TYPE(obj, OBJ_STRING)) {
            char* str = AS_CSTRING(obj);
            size_t len = AS_STRING(obj)->length;

            if (strcmp(method, "length") == 0) {
                if (argCount != 0) { free(argValues); return throwException(ArgumentException, "ArgumentException: length() takes 0 arguments.\n"); }
                result = make(VAL_INT, (int)len);

            } else if (strcmp(method, "isSpace") == 0){
                if (argCount != 0) return throwException(ArgumentException, "ArgumentException: isSpace() takes 0 arguments.\n");
                if (len < 0) return make(VAL_INT, 0);
                for (int i = 0; i < len; i++) {
                    if (!isspace((unsigned char)str[i])) return make(VAL_INT, 0);
                }
                result = make(VAL_INT, 1);
                free(argValues);
            } else if (strcmp(method, "isAlphabetical") == 0){
                if (argCount != 0) return throwException(ArgumentException, "ArgumentException: isAlphabetical() takes 0 arguments.\n");
                if (len < 0) return make(VAL_INT, 0);
                for (int i = 0; i < len; i++) {
                    if (!isalpha((unsigned char)str[i])) return make(VAL_INT, 0);
                }
                result = make(VAL_INT, 1);
                free(argValues);
            } else if (strcmp(method, "isAlphaNumeric") == 0){
                if (argCount != 0) return throwException(ArgumentException, "ArgumentException: isAlphaNumeric() takes 0 arguments.\n");
                if (len < 0) return make(VAL_INT, 0);
                for (int i = 0; i < len; i++) {
                    if (!isalnum((unsigned char)str[i])) return make(VAL_INT, 0);
                }
                result = make(VAL_INT, 1);
                free(argValues);
            } else if (strcmp(method, "isDigit")==0) {
                if (argCount != 0) return throwException(ArgumentException, "ArgumentException: isDigit() takes 0 arguments.\n");
                if (len < 0) return make(VAL_INT, 0);
                for (int i = 0; i < len; i++) {
                    if (!isdigit((unsigned char)str[i])) return make(VAL_INT, 0);
                }
                result = make(VAL_INT, 1);
                free(argValues);
            } else if (strcmp(method, "upper") == 0) {
                if (argCount != 0) { free(argValues); return throwException(ArgumentException, "ArgumentException: upper() takes 0 arguments.\n"); }
                char* buf = malloc(len + 1);
                for (size_t i = 0; i < len; i++) buf[i] = toupper((unsigned char)str[i]);
                buf[len] = '\0';
                result = make(VAL_OBJECT, (Object*)allocateString(buf, len));
                free(buf);

            } else if (strcmp(method, "lower") == 0) {
                if (argCount != 0) { free(argValues); return throwException(ArgumentException, "ArgumentException: lower() takes 0 arguments.\n"); }
                char* buf = malloc(len + 1);
                for (size_t i = 0; i < len; i++) buf[i] = tolower((unsigned char)str[i]);
                buf[len] = '\0';
                result = make(VAL_OBJECT, (Object*)allocateString(buf, len));
                free(buf);

            } else if (strcmp(method, "contains") == 0) {
                if (argCount != 1 || !IS_OBJ_TYPE(argValues[0], OBJ_STRING)) { free(argValues); return throwException(ArgumentException, "ArgumentException: contains() takes 1 string argument.\n"); }
                result = make(VAL_INT, strstr(str, AS_CSTRING(argValues[0])) != NULL ? 1 : 0);

            } else if (strcmp(method, "slice") == 0) {
                if (argCount != 2 || argValues[0].type != VAL_INT || argValues[1].type != VAL_INT) { free(argValues); return throwException(ArgumentException, "ArgumentException: slice() takes 2 integer arguments.\n"); }
                int start = argValues[0].as.number;
                int end   = argValues[1].as.number;
                if (start < 0) start = 0;
                if (end > (int)len) end = (int)len;
                if (start >= end) { result = make(VAL_OBJECT, (Object*)allocateString("", 0)); }
                else {
                    result = make(VAL_OBJECT, (Object*)allocateString(str + start, end - start));
                }

            }else {
                free(argValues);
                return throwException(IdentifierNotFoundException, "IdentifierNotFoundException: String has no method '%s'.\n", method);
            }

        // --- List methods ---
        } else if (IS_OBJ_TYPE(obj, OBJ_LIST)) {
            ValueList* list = AS_LIST(obj);

            if (strcmp(method, "append") == 0) {
                if (argCount != 1) { free(argValues); return throwException(ArgumentException, "ArgumentException: append() takes 1 argument.\n"); }
                if (list->count >= list->capacity) {
                    list->capacity = list->capacity == 0 ? 4 : list->capacity * 2;
                    list->items = realloc(list->items, sizeof(Value) * list->capacity);
                }
                list->items[list->count++] = argValues[0];
                result = make(VAL_VOID);

            } else if (strcmp(method, "pop") == 0) {
                if (argCount != 0) { free(argValues); return throwException(ArgumentException, "ArgumentException: pop() takes 0 arguments.\n"); }
                if (list->count == 0) { free(argValues); return throwException(IndexOutOfBoundsException, "IndexOutOfBoundsException: Cannot pop from an empty list.\n"); }
                result = list->items[--list->count];

            } else if (strcmp(method, "length") == 0) {
                if (argCount != 0) { free(argValues); return throwException(ArgumentException, "ArgumentException: length() takes 0 arguments.\n"); }
                result = make(VAL_INT, list->count);

            } else {
                free(argValues);
                return throwException(IdentifierNotFoundException, "IdentifierNotFoundException: List has no method '%s'.\n", method);
            }

        // --- Dict methods ---
        } else if (IS_OBJ_TYPE(obj, OBJ_DICT)) {
            ValueDict* dict = AS_DICT(obj);

            if (strcmp(method, "has") == 0) {
                if (argCount != 1 || !IS_OBJ_TYPE(argValues[0], OBJ_STRING)) { free(argValues); return throwException(ArgumentException, "ArgumentException: has() takes 1 string argument.\n"); }
                DictEntry* entry;
                HASH_FIND_STR(dict->head, AS_CSTRING(argValues[0]), entry);
                result = make(VAL_INT, entry != NULL ? 1 : 0);

            } else if (strcmp(method, "remove") == 0) {
                if (argCount != 1 || !IS_OBJ_TYPE(argValues[0], OBJ_STRING)) { free(argValues); return throwException(ArgumentException, "ArgumentException: remove() takes 1 string argument.\n"); }
                DictEntry* entry;
                char* key = AS_CSTRING(argValues[0]);
                HASH_FIND_STR(dict->head, key, entry);
                if (entry) {
                    HASH_DEL(dict->head, entry);
                    free(entry->key);
                    free(entry);
                }
                result = make(VAL_VOID);

            } else if (strcmp(method, "length") == 0) {
                if (argCount != 0) { free(argValues); return throwException(ArgumentException, "ArgumentException: length() takes 0 arguments.\n"); }
                result = make(VAL_INT, (int)HASH_COUNT(dict->head));

            } else {
                free(argValues);
                return throwException(IdentifierNotFoundException, "IdentifierNotFoundException: Dict has no method '%s'.\n", method);
            }

        } else if (IS_OBJ_TYPE(obj, OBJ_FILE)) { // File Methods
            ObjFile* file = (ObjFile*)AS_OBJECT(obj);

            if (strcmp(method, "contents") == 0) {
                if (argCount != 0) {free(argValues); return throwException(ArgumentException, "ArgumentException: contents() takes 0 arguments.\n");}
                if (!file->isOpen) {free(argValues); return throwException(RuntimeException, "RuntimeException: File has to be open.");}
                result = make(VAL_OBJECT, (Object*)allocateString(file->contents, strlen(file->contents)));
            } else if (strcmp(method, "path")==0) {
                if (argCount != 0) {free(argValues); return throwException(ArgumentException, "ArgumentException: path() takes 0 arguments.\n");}
                result = make(VAL_OBJECT, (Object*)allocateString(file->path, strlen(file->path)));
            }
        } else {
            free(argValues);
            return throwException(TypeException, "TypeException: Value does not support method calls.\n");
        }

        free(argValues);
        return result;
    }

    // 6c. Member Access: obj.attr  (attr read from a dict by key)
    if (node->type == NODE_MEMBER_ACCESS) {
        Value obj = evaluate(node->left);
        if (obj.type == VAL_ERR) return obj;

        if (!IS_OBJ_TYPE(obj, OBJ_DICT)) return throwException(TypeException, "TypeException: Attribute access with '.' is only supported on dictionaries.\n");

        ValueDict* dict = AS_DICT(obj);
        DictEntry* entry;
        HASH_FIND_STR(dict->head, node->value, entry);
        if (!entry) return throwException(EntryNotFoundException, "EntryNotFoundException: Key '%s' not found.\n", node->value);
        return entry->value;
    }

    // List
    if (node->type == NODE_LIST_LITERAL) {
        ValueList* list = allocateList(node->childCount > 0 ? node->childCount : 1);
        list->count = node->childCount;
        for (int i = 0; i < list->count; i++) {
            list->items[i] = evaluate(node->children[i]);
            if (list->items[i].type == VAL_ERR) return list->items[i];
        }
        return make(VAL_OBJECT, (Object*)list);
    }

    // Dictionary
    if (node->type == NODE_DICT_LITERAL) {
        ValueDict* dict = allocateDict();
        for (int i = 0; i < node->childCount; i += 2) {
            Value keyVal = evaluate(node->children[i]);
            if (!IS_OBJ_TYPE(keyVal, OBJ_STRING)) return throwException(TypeException, "TypeException: Dictionary keys must be strings.\n");

            Value val = evaluate(node->children[i+1]);
            if (val.type == VAL_ERR) return val;

            char* key = AS_CSTRING(keyVal);
            DictEntry* entry;
            HASH_FIND_STR(dict->head, key, entry);
            if (entry) {
                entry->value = val;
            } else {
                entry = malloc(sizeof(DictEntry));
                entry->key = strdup(key);
                entry->value = val;
                HASH_ADD_STR(dict->head, key, entry);
            }
        }
        return make(VAL_OBJECT, (Object*)dict);
    }

    // Index Access
    if (node->type == NODE_INDEX_ACCESS) {
        Value collection = evaluate(node->left);
        if (collection.type == VAL_ERR) return collection;
        Value index = evaluate(node->right);
        if (index.type == VAL_ERR) return index;

        if (IS_OBJ_TYPE(collection, OBJ_LIST)) {
            if (index.type != VAL_INT) return throwException(TypeException, "TypeException: List index must be an integer.\n");
            int i = index.as.number;
            ValueList* list = AS_LIST(collection);
            if (i < 0 || i >= list->count) return throwException(IndexOutOfBoundsException, "IndexOutOfBoundsException: List index out of bounds.\n");
            return list->items[i];
        }

        if (IS_OBJ_TYPE(collection, OBJ_DICT)) {
            if (!IS_OBJ_TYPE(index, OBJ_STRING)) return throwException(TypeException, "TypeException: Dictionary key must be a string.\n");
            ValueDict* dict = AS_DICT(collection);
            char* key = AS_CSTRING(index);
            DictEntry* entry;
            HASH_FIND_STR(dict->head, key, entry);
            if (entry) return entry->value;
            return throwException(EntryNotFoundException, "EntryNotFoundException: Key '%s' not found in dictionary.\n", key);
        }

        if (IS_OBJ_TYPE(collection, OBJ_STRING)) {
            if (index.type != VAL_INT) return throwException(TypeException, "TypeException: String index must be an integer.\n");
            int i = index.as.number;
            ObjString* s = AS_STRING(collection);
            if (i < 0 || i >= (int)s->length) return throwException(IndexOutOfBoundsException, "IndexOutOfBoundsException: String index out of bounds.\n");
            char ch[2] = { s->chars[i], '\0' };
            return make(VAL_OBJECT, (Object*)allocateString(ch, 1));
        }

        return throwException(SyntaxException, "SyntaxException: Can only index into lists, dictionaries, or strings.\n");
    }

    // 8. Native Print
    if (node->type == NODE_PRINT) {
        Value val = evaluate(node->right);
        if (val.type == VAL_ERR) return val;
        printValue(val);
        return make(VAL_VOID);
    }

    // 8.1 Native represent
    if (node->type == NODE_REPRESENT) {
        Value val = evaluate(node->right);
        if (val.type == VAL_ERR) return val;
        printReprValue(val);
        return make(VAL_VOID);
    }

    // 9. If Statements
    if (node->type == NODE_IF) {
        Value cond = evaluate(node->children[0]);
        if (cond.type == VAL_ERR) return cond;
        if (!is_falsy(cond)) {
            Value res = evaluate(node->children[1]);
            if (res.type == VAL_ERR || res.type == VAL_RETURN || res.type == VAL_BREAK) return res;
        } else if (node->childCount == 3) {
            Value res = evaluate(node->children[2]);
            if (res.type == VAL_ERR || res.type == VAL_RETURN || res.type == VAL_BREAK) return res;
        }
        return make(VAL_VOID);
    }

    //9.1 Else statements
    if (node->type == NODE_ELSE) {
        if (node->childCount == 2) {
            Value cond = evaluate(node->children[0]);
            if (cond.type == VAL_ERR) return cond;
            if (!is_falsy(cond)) {
                Value res = evaluate(node->children[1]);
                if (res.type == VAL_ERR || res.type == VAL_RETURN || res.type == VAL_BREAK) return res;
            } else if (node->childCount == 3) {
                Value res = evaluate(node->children[2]);
                if (res.type == VAL_ERR || res.type == VAL_RETURN || res.type == VAL_BREAK) return res;
            }
            return make(VAL_VOID);
        } else {
            Value res = evaluate(node->children[0]);
            if (res.type == VAL_ERR || res.type == VAL_RETURN || res.type == VAL_BREAK) return res;
            
            return make(VAL_VOID);
        }
    }

    // 10. While Loops
    if (node->type == NODE_WHILE) {
        while (true) {
            Value cond = evaluate(node->children[0]);if (cond.type == VAL_ERR) return cond;
            if (is_falsy(cond)) break;
            Value res = evaluate(node->children[1]);
            if (res.type == VAL_ERR) return res;
            if (res.type == VAL_BREAK) break;
            if (res.type == VAL_RETURN) return res;
        }
        return make(VAL_VOID);
    }

    // 10.1 Repeat loops
    if (node->type == NODE_REPEAT) {
        Value times = evaluate(node->children[0]);
        if (times.type == VAL_ERR) return times;
        const int timesInt = times.as.number;
        if (timesInt <= 0) return throwException(ArgumentException, "ArgumentException: repeat expects count to be more than 0.\n");
        for (int i = 0; i < timesInt; i++) {
            Value res = evaluate(node->children[1]);
            if (res.type == VAL_ERR) return res;
            if (res.type == VAL_BREAK) break;
            if (res.type == VAL_RETURN) return res;
        }
        return make(VAL_VOID);
    }

    // 11. Import
    if (node->type == NODE_IMPORT) {
        Value pathVal = evaluate(node->right);
        if (pathVal.type == VAL_ERR) return pathVal;
        if (!IS_OBJ_TYPE(pathVal, OBJ_STRING)) return throwException(TypeException, "TypeException: Import path must be a string.\n");

        char* importPath = AS_CSTRING(pathVal);
        char* source = "";

        // 1. Try the path as-is first (relative or absolute)
        source = readFile(importPath);

        // 2. If not found, try the lib folder relative to the executable
        if (strcmp(source, "") == 0) {
            char libPath[512];
            snprintf(libPath, sizeof(libPath), "lib/%s", importPath);
            source = readFile(libPath);
        }

        // 3. Still not found
        if (strcmp(source, "")==0) {
            return throwException(FileNotFoundException, 
                "FileNotFoundException: Could not find '%s' or 'lib/%s'.\n", 
                importPath, importPath);
        }

        TokenStream* Stream = lex(source);
        ASTNode* Root = parseFile(*Stream);
        free(source); // Free here so it's always freed regardless of parse result

        if (Root) {
            Value Result = evaluate(Root);
            freeAST(Root);
            finalCleanup(Stream);
            if (Result.type == VAL_ERR) return Result;
        } else {
            finalCleanup(Stream);
            return throwException(RuntimeException, "RuntimeException: Failed to parse '%s'.\n", importPath);
        }

        return make(VAL_VOID);
    }

    // 12. Break
    if (node->type == NODE_BREAK) {
        return make(VAL_BREAK);
    }

    // 13. Readfile (builtin I/O)
    if (node->type == NODE_READFILE) {
        Value valPath = evaluate(node->children[0]);
        if (valPath.type == VAL_ERR) return valPath;

        char* path = NULL;
        if (IS_OBJ_TYPE(valPath, OBJ_STRING)) {
            path = AS_CSTRING(valPath);
        } else if (IS_OBJECT(valPath) && AS_OBJECT(valPath)->type == OBJ_FILE) {
            ObjFile* existing = (ObjFile*)AS_OBJECT(valPath);
            path = existing->path;
        } else {
            return throwException(TypeException, "TypeException: readfile expects a file or a string (path).\n");
        }

        ObjFile* file = (ObjFile*)allocateObject(sizeof(ObjFile), OBJ_FILE);
        file->file = NULL;
        file->path = NULL;
        file->contents = NULL;
        file->isOpen = false;

        file->file = fopen(path, "r+");
        if (file->file == NULL) {
            return throwException(FileNotFoundException, "FileNotFoundException: File couldn't be found or does not exist.\n");
        }

        file->path = strdup(path);
        file->isOpen = true;

        // Read contents
        fseek(file->file, 0L, SEEK_END);
        size_t size = ftell(file->file);
        rewind(file->file);
        file->contents = malloc(size + 1);
        fread(file->contents, sizeof(char), size, file->file);
        file->contents[size] = '\0';

        // Bind to variable if 'as name' was provided
        if (node->value && strlen(node->value) > 0) {
            setVariable(node->value, make(VAL_OBJECT, (Object*)file));
        }

        // Set currentFile for file interaction statements inside the block
        ObjFile* previousFile = currentFile;
        currentFile = file;

        Value blockResult = evaluate(node->children[1]);

        // Always close on block exit
        if (file->isOpen && file->file != NULL) {
            fclose(file->file);
            file->file = NULL;
            file->isOpen = false;
        }
        free(file->contents);
        file->contents = NULL;

        // Restore previous file context (supports nesting)
        currentFile = previousFile;

        if (blockResult.type == VAL_ERR) return blockResult;
        return make(VAL_VOID);
    }

    // 13.1 File interactions
    if (node->type == NODE_FILE_INTERACTION) {
        if (strcmp(node->value, "overwrite") == 0) {
            if (!currentFile->isOpen || !currentFile->file) return throwException(RuntimeException, "RuntimeException: File has to be opened to perform this action.\n");
            Value result = evaluate(node->children[0]);
            if (result.type == VAL_ERR) return result;
            if (!IS_OBJ_TYPE(result, OBJ_STRING)) return throwException(TypeException, "TypeException: overwrite expects a string.\n");
            rewind(currentFile->file);
            _chsize(fileno(currentFile->file), 0); // Clear the file (WINDOWS ONLY)
            fprintf(currentFile->file, "%s", AS_CSTRING(result));

        } else if (strcmp(node->value, "put") == 0) {
            if (!currentFile->isOpen || !currentFile->file) return throwException(RuntimeException, "RuntimeException: File has to be opened to perform this action.\n");
            Value result = evaluate(node->children[0]);
            if (result.type == VAL_ERR) return result;
            if (!IS_OBJ_TYPE(result, OBJ_STRING)) return throwException(TypeException, "TypeException: put expects a string.\n");
            fseek(currentFile->file, 0, SEEK_END); // Make sure we're at the end
            fprintf(currentFile->file, "%s", AS_CSTRING(result));
        }
    }

    // 14. Command Execution
    if (node->type == NODE_CMD) {
        Value cmd = evaluate(node->right);
        if (cmd.type == VAL_ERR) return cmd;
        if (!IS_OBJ_TYPE(cmd, OBJ_STRING)) return throwException(TypeException, "TypeException: $ expects a string.\n");
        int result = system(AS_CSTRING(cmd));
        return make(VAL_INT, result);
    }

    // 15. Program / Block Execution
    if (node->type == NODE_PROGRAM) {
        Value lastResult = make(VAL_VOID);
        if (node->children) {
            for (int i = 0; i < node->childCount; i++) {
                lastResult = evaluate(node->children[i]);
                if (lastResult.type == VAL_ERR) return lastResult;
                if (lastResult.type == VAL_RETURN) return lastResult;
                if (lastResult.type == VAL_BREAK) return lastResult;
            }
        }
        return lastResult;
    }

    return make(VAL_VOID);
}

// --- Native C Functions ---

Value native_input(int argCount, Value* args) {
    (void)args;
    if (argCount != 0) return throwException(ArgumentException, "ArgumentException: input() takes 0 arguments.\n");
    char line[1024];
    if (fgets(line, sizeof(line), stdin)) {
        line[strcspn(line, "\n")] = 0;
        return make(VAL_OBJECT, (Object*)allocateString(line, strlen(line)));
    }
    return make(VAL_OBJECT, (Object*)allocateString("", 0));
}

Value native_clock(int argCount, Value* args) {
    (void)args;
    if (argCount != 0) return throwException(ArgumentException, "ArgumentException: clock() takes 0 arguments.\n");
    return make(VAL_INT, (int)(clock() * 1000 / CLOCKS_PER_SEC));
}

Value native_length(int argc, Value* args) {
    if (argc != 1) return throwException(ArgumentException, "ArgumentException: length() takes 1 argument.");
    Value val = args[0];
    if (IS_OBJ_TYPE(val, OBJ_STRING)) return make(VAL_INT, AS_STRING(val)->length);
    if (IS_OBJ_TYPE(val, OBJ_DICT))   return make(VAL_INT, (int)HASH_COUNT(AS_DICT(val)->head));
    if (IS_OBJ_TYPE(val, OBJ_LIST))   return make(VAL_INT, AS_LIST(val)->count);
    if (isVal(val, VAL_INT)) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", val.as.number);
        return make(VAL_INT, (int)strlen(buf));
    }
    if (isVal(val, VAL_FLOAT)) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%g", val.as.decimal);
        return make(VAL_INT, (int)strlen(buf));
    }
    return throwException(TypeException, "TypeException: Length needs a valid type.\n");
}

Value native_none(int argCount, Value* args) {
    if (argCount == 0) return make(VAL_INT, 0);
    for (int i = 0; i < argCount; i++) {
        if (!is_falsy(args[i])) return make(VAL_INT, 0);
    }
    return make(VAL_INT, 1);
}

Value native_all(int argc, Value* args) {
    if (argc == 0) return make(VAL_INT, 0);
    for (int i = 0; i < argc; i++) {
        if (is_falsy(args[i])) return make(VAL_INT, 0);
    }
    return make(VAL_INT, 1);
}

Value native_any(int argc, Value* args) {
    if (argc == 0) return make(VAL_INT, 0);
    for (int i = 0; i < argc; i++) {
        if (!is_falsy(args[i])) return make(VAL_INT, 1);
    }
    return make(VAL_INT, 0);
}

// --- Initialization ---

void initNatives() {
    defineNative("input", native_input);
    defineNative("clock", native_clock);
    defineNative("length", native_length);
    defineNative("all", native_all);
    defineNative("any", native_any);
    defineNative("none", native_none);
}

void initSymbolTable() {
    currentScope = malloc(sizeof(Scope));
    currentScope->symbols = NULL;
    currentScope->prev = NULL;
}

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
    if (buffer == NULL) exit(74);

    size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
    if (bytesRead < fileSize && !feof(file)) exit(74);

    buffer[bytesRead] = '\0';
    fclose(file);
    return buffer;
}

void runREPL() {
    char line[1024];
    char source[65536] = {0};
    printf("Meaningful Shell %s\n", VERSION);
    printf("Type 'exit' to quit.\n");

    while (1) {
        printf(strlen(source) > 0 ? "... " : ">>> ");
        if (!fgets(line, sizeof(line), stdin)) break;
        line[strcspn(line, "\n")] = 0;
        if (strcmp(line, "exit") == 0) break;
        if (strlen(line) == 0) continue;

        strcat(source, line);
        strcat(source, "\n");

        // Count unmatched block openers
        int depth = 0;
        TokenStream* probe = lex(source);
        for (int i = 0; i < probe->size; i++) {
            Token t = probe->tokens[i];
            if (t.type != TOKEN_KEYWORD) continue;
            if (strcmp(t.value, "while")  == 0 ||
                strcmp(t.value, "if")     == 0 ||
                strcmp(t.value, "repeat") == 0) {
                depth++;
            } else if (strcmp(t.value, "set") == 0) {
                if (i + 2 < probe->size &&
                    probe->tokens[i+1].type == TOKEN_IDENTIFIER &&
                    probe->tokens[i+2].type == TOKEN_PARENTHESIS) {
                    depth++;
                }
            } else if (strcmp(t.value, "end") == 0) {
                depth--;
            }
        }
        finalCleanup(probe);

        if (depth > 0) continue; // still inside a block, keep reading

        // Ready to evaluate
        TokenStream* stream = lex(source);
        ASTNode* root = parseFile(*stream);
        if (root) {
            Value rslt = evaluate(root);
            if (rslt.type == VAL_ERR) { freeAST(root); finalCleanup(stream); memset(source, 0, sizeof(source)); continue; }
            freeAST(root);
        }
        finalCleanup(stream);
        memset(source, 0, sizeof(source));
    }

    freeSymbolTable();
    collectGarbage();
    currentFile = NULL;
    free(currentFile);
}

int main(int argc, char *argv[]) {
    int returningVal = 0;
    if (argc != 3 && argc != 2 && argc != 1) {
        printf("Usage:\n meaningful [name].mean\n meaningful -v/--version\n meaningful -src/--source\n meaningful -lic/--licence\n meaningful -t/--test [name].mean\n meaningful");
        return 3;
    }
    initSymbolTable();
    initNatives();
    if (argc == 1) {
        runREPL();
        return 0;
    }
    if (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0) {
        if (argc != 2) { printf("Usage: meaningful -v/--version"); return 3; }
        printf("Meaningful Language, Licenced under BMLL 2.0\n                      Version %s", VERSION);
        return 0;
    } else if (strcmp(argv[1], "-src") == 0 || strcmp(argv[1], "--source") == 0) {
        if (argc != 2) { printf("Usage: meaningful -src/--source"); return 3; }
        system("start https://github.com/Banaolf/Meaningful-Language");
        return 0;
    } else if (strcmp(argv[1], "-lic") == 0 || strcmp(argv[1], "--licence") == 0) {
        if (argc != 2) { printf("Usage: meaningful -lic/--licence"); return 3; }
        system("start https://github.com/Banaolf/Meaningful-Language/blob/main/LICENCE");
        return 0;
    } else if (strcmp(argv[1], "-t") == 0 || strcmp(argv[1], "--test") == 0) {
        if (argc != 3) { printf("Usage: meaningful -t/--test [name].mean"); return 3; }
        char* src = readFile(argv[2]);
        if (strcmp(src, "") == 0) exit(2);
        TokenStream* stream = lex(src);
        ASTNode* root = parseFile(*stream);
        if (root) {
            Value result = evaluate(root);
            if (result.type == VAL_ERR) {
                printf("Test: Something went wrong :(\nFile: %s", argv[2]);
                freeAST(root);
                finalCleanup(stream);
                free(src);
                returningVal = 4;
                goto end_of_interpreter;
            } else printf("Test: Successful");
            freeAST(root);
        }
        finalCleanup(stream);
        free(src);
        returningVal = 0;
        goto end_of_interpreter;
    } else {
        char* src = readFile(argv[1]);
        if (strcmp(src, "") == 0) exit(2);
        TokenStream* stream = lex(src);
        ASTNode* root = parseFile(*stream);
        if (root) {
            Value result = evaluate(root);
            if (result.type == VAL_ERR) {returningVal = 4; freeAST(root); finalCleanup(stream); free(src); goto end_of_interpreter;}
            freeAST(root);
        }
        finalCleanup(stream);
        free(src);
        returningVal = 0; 
    }//same thing as using the goto
    end_of_interpreter: //Added just for -t
        freeSymbolTable();
        collectGarbage();
        currentFile = NULL;
        free(currentFile);
        return returningVal;
}
/*
Exit codes
Pass: 0
General failure: 1
File not found: 2
Malformed flags: 3
Runtime Error: 4
Parser Error: 5 Not being used rn
Lexer Error 6 Not being used rn
*/