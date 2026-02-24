#ifndef SYNTAX_H
#define SYNTAX_H
#include <stddef.h>
#include "lexer.h"
extern const char* keywords[];

typedef enum {
    NODE_NUMBER,
    NODE_VARIABLE,
    NODE_BINARY_OP, // For +, -, *, /
    NODE_ASSIGNMENT, // For SET
    NODE_FUNCTION_DEF,
    NODE_FUNCTION_CALL,
    NODE_PROGRAM,
    NODE_RETURN,
    NODE_PRINT,
    NODE_IF,
    NODE_STRING,
    NODE_WHILE
} NodeType;

typedef struct Exception{
    char* text;
    int line;
    int character;
} Exception;

typedef struct ASTNode {
    NodeType type;
    char* value;                // The name or number (as a string)
    struct ASTNode* left;       // Left child
    struct ASTNode* right;      // Right child
    struct ASTNode** children; 
    int childCount;
    int childCapacity;
} ASTNode;

ASTNode* parseFile(TokenStream Tokens);
void freeAST(ASTNode* node);

#endif