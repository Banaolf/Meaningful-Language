#ifndef SYNTAX_H
#define SYNTAX_H
#include <stddef.h>
#include "lexer.h"
extern const char* keywords[];

typedef enum {
    NODE_NUMBER,
    NODE_VARIABLE,
    NODE_OPERATOR,
    NODE_ASSIGNMENT,
    NODE_FUNCTION_DEF,
    NODE_FUNCTION_CALL,
    NODE_PROGRAM,
    NODE_RETURN,
    NODE_PRINT,
    NODE_REPRESENT,
    NODE_IF,
    NODE_IMPORT,
    NODE_BREAK,
    NODE_STRING,
    NODE_WHILE,
    NODE_FLOAT,
    NODE_LIST_LITERAL,
    NODE_DICT_LITERAL,
    NODE_INDEX_ACCESS,
    NODE_MEMBER_ACCESS,
    NODE_METHOD_CALL,
    NODE_REPEAT,
    NODE_LOGIC,
    NODE_UNARY,
    NODE_NON,
    NODE_COMPARRISON,
    NODE_ELSE,
    NODE_READFILE,
    NODE_FILE_INTERACTION,
    NODE_CMD,
    NODE_CONVERSION,
    NODE_FOREACH,
    NODE_IN,
    NODE_UNSET,
    NODE_ADDRESS_OF,
    NODE_DEREF,
    NODE_CREATEFILE,
    NODE_TERNARY,
    NODE_COLON,
    NODE_BINARY
} NodeType;

typedef struct ASTNode {
    NodeType type;
    char* value;
    char* type_notation;
    char* type_condition;
    struct ASTNode* left;
    struct ASTNode* right;
    struct ASTNode** children; 
    int childCount;
    int childCapacity;
    int bitlength;
} ASTNode;

typedef enum {
    RuntimeException, // General one.
    DivideByZeroException,
    SyntaxException,
    IdentifierNotFoundException,
    FileNotFoundException,
    ArgumentException,
    IndexOutOfBoundsException,
    EntryNotFoundException,
    TypeException,
    ConversionException,
    NotImplementedException,
    NullPointerException
}ExceptionType;

typedef struct{
    ExceptionType type;
    char* message;
}Exception;

ASTNode* parseFile(TokenStream Tokens);
void freeAST(ASTNode* node);

#endif