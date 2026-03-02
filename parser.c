#include "parser.h"
#include "lexer.h"
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
//Licenced under BMLL2.0, see LICENSE for further info
int currentIndex = 0;
int scopeLevel = 0;
TokenStream* globalStream;

int parserError = 0;

char* tokenTypeToString(TokenType type){
    switch (type) {
        case TOKEN_KEYWORD: return "TOKEN_KEYWORD";
        case TOKEN_IDENTIFIER: return "TOKEN_IDENTIFIER";
        case TOKEN_NUMBER: return "TOKEN_NUMBER";
        case TOKEN_CURLY: return "TOKEN_CURLY";
        case TOKEN_SQUARE: return "TOKEN_SQUARE";
        case TOKEN_BOOLEAN: return "TOKEN_BOOLEAN";
        case TOKEN_NON: return "TOKEN_NON";
        case TOKEN_OPERATOR: return "TOKEN_OPERATOR";
        case TOKEN_EQUALS: return "TOKEN_EQUALS";
        case TOKEN_COMPARISON: return "TOKEN_COMPARISON";
        case TOKEN_STRING: return "TOKEN_STRING";
        case TOKEN_EOF: return "TOKEN_EOF";
        case TOKEN_PARENTHESIS: return "TOKEN_PARENTHESIS";
        case TOKEN_COMMA: return "TOKEN_COMMA";
        case TOKEN_SEMICOLON: return "TOKEN_SEMICOLON";
        case TOKEN_COLON: return "TOKEN_COLON";
        case TOKEN_COMPOUND_ASSIGN: return "TOKEN_COMPOUND_ASSIGN";
        case TOKEN_DOT: return "TOKEN_DOT";
        case TOKEN_FLOAT: return "TOKEN_FLOAT";
        case ERR: return "ERR";
        default: return "UNKNOWN";
    }
}

const char* keywords[] = { 
    "set",
    "print",
    "if",
    "while",
    "end", 
    "Import",
    "break",
    "repeat",
    NULL
};

bool isKeyword(const char* keyword){
    for (int i = 0; keywords[i] != NULL; i++) {
        if (strcmp(keyword, keywords[i]) == 0) {
            return true;
        }
    }
    return false;
}

Token peek(int x) {
    if (currentIndex + x >= globalStream->size) {
        return globalStream->tokens[globalStream->size - 1]; // Return the EOF token
    }
    
    if (currentIndex + x < 0) {
        return globalStream->tokens[0];
    }

    return globalStream->tokens[currentIndex + x];
}
bool is(TokenType checker, int ind) {
    return peek(ind).type == checker;
}
TokenType get(int ind) {
    return peek(ind).type;
}
void throwVoid(){
    Token errorToken = peek(0);
    printf("[PARSER]At line %d, character %d: Unexpected token: %s", errorToken.ln, errorToken.character, tokenTypeToString(get(0)));
    parserError = 1;
    exit(1); //Made to avoid endless loops, replacement when available.
}
void throw(TokenType expected){
    Token errorToken = peek(0);
    fprintf(stderr, "[PARSER]At line %d, character %d: Expected %s, got %s\n", errorToken.ln, errorToken.character, tokenTypeToString(expected), tokenTypeToString(errorToken.type));
    parserError = 1;
    exit(1); //Made to avoid endless loops, replacement when available. (WARNING: THERES MEMORY LEAKS WITH THIS.)
}
void throwMultiple(int expectedCount, ...){
    TokenType got = get(0);
    va_list args;
    va_start(args, expectedCount);
    for (int i = 0; i < expectedCount; i++) {
        TokenType expected = va_arg(args, TokenType);
        if (i > 0) {printf("\n or \n");}
        printf("%s", tokenTypeToString(expected));
    }
    va_end(args);
    printf("\n, got %s.", tokenTypeToString(got));
    parserError = 1;
    exit(1); //Made to avoid endless loops, replacement when available.
}

void advance() {
    if (currentIndex < globalStream->size) currentIndex++;
}

ASTNode* createNode(NodeType type, char* value) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = type;
    node->value = value ? strdup(value) : NULL;
    node->left = NULL;
    node->right = NULL;
    node->children = NULL;
    node->childCapacity = 0;
    node->childCount = 0;
    return node;
}

void addChild(ASTNode* parent, ASTNode* child) {
    if (child == NULL) return;

    // 1. If this is the first child, initialize the array
    if (parent->children == NULL) {
        parent->childCapacity = 4; // Start with space for 4 statements
        parent->childCount = 0;
        parent->children = malloc(sizeof(ASTNode*) * parent->childCapacity);
    }

    // 2. If the array is full, double the size
    if (parent->childCount >= parent->childCapacity) {
        parent->childCapacity *= 2;
        parent->children = realloc(parent->children, sizeof(ASTNode*) * parent->childCapacity);
    }

    // 3. Add the child and increment the count
    parent->children[parent->childCount] = child;
    parent->childCount++;
}
//PARSERS---------------------------------------------------------------------------------------------------------------------------------------------------------------------------

// Parse expression
ASTNode* parseExpression(); 
// Parse statement
ASTNode* parseStatement(); 

// Parse primary expression
ASTNode* parsePrimaryExpression();

// New function to handle postfix operators like () and []
ASTNode* parsePostfixExpression() {
    ASTNode* node = parsePrimaryExpression(); 
    if (!node) return NULL;

    while (true) {
        if (parserError) break;
        if (is(TOKEN_PARENTHESIS, 0) && strcmp(peek(0).value, "(") == 0) {
            // Function call
            if (node->type != NODE_VARIABLE) {
                printf("[PARSER]At line %d, character %d: .\n", peek(0).ln, peek(0).character);
                parserError = 1;
                freeAST(node);
                return NULL;
            }
            
            advance(); // Eat '('
            ASTNode* callNode = createNode(NODE_FUNCTION_CALL, node->value);
            free(node->value); // name is now in callNode
            free(node);        // free the original NODE_VARIABLE

            // Parse arguments
            if (!is(TOKEN_PARENTHESIS, 0) || strcmp(peek(0).value, ")") != 0) {
                while (true) {
                    ASTNode* arg = parseExpression();
                    if (!arg) { freeAST(callNode); return NULL; }
                    addChild(callNode, arg);
                    if (is(TOKEN_COMMA, 0)) {
                        advance();
                    } else {
                        break;
                    }
                }
            }

            if (is(TOKEN_PARENTHESIS, 0) && strcmp(peek(0).value, ")") == 0) {
                advance(); // Eat ')'
            } else {
                throw(TOKEN_PARENTHESIS);
                freeAST(callNode);
                return NULL;
            }
            node = callNode; // The call node is now the primary node
        } else if (is(TOKEN_SQUARE, 0) && strcmp(peek(0).value, "[") == 0) {
            // Index access: list[index]
            advance(); // Eat '['
            ASTNode* indexNode = parseExpression();
            if (!indexNode) { freeAST(node); return NULL; }

            if (is(TOKEN_SQUARE, 0) && strcmp(peek(0).value, "]") == 0) {
                advance(); // Eat ']'
            } else {
                throw(TOKEN_SQUARE); // Expected ']'
                freeAST(node);
                freeAST(indexNode);
                return NULL;
            }
            ASTNode* accessNode = createNode(NODE_INDEX_ACCESS, "[]");
            accessNode->left = node; // The thing being indexed
            accessNode->right = indexNode; // The index
            node = accessNode;
        } else {
            break; // No more postfix operators
        }
    }
    return node;
}

ASTNode* parsePrimaryExpression() {
    if (is(ERR,0)){
        Token t = peek(0);
        printf("SyntaxException: Invalid token %s, Line %d Character %d", t.value, t.ln, t.character);
    }

    // Unary not
    if (is(TOKEN_OPERATOR, 0) && strcmp(peek(0).value, "!") == 0) {
        advance();
        ASTNode* operand = parsePrimaryExpression();
        if (!operand) return NULL;
        ASTNode* zero = createNode(NODE_NUMBER, "0");
        ASTNode* node = createNode(NODE_BINARY_OP, "==");
        node->left = operand;
        node->right = zero;
        return node;
    }
    if (is(TOKEN_NUMBER, 0)) {
        Token t = peek(0);
        advance();
        return createNode(NODE_NUMBER, t.value);
    }

    if (is(TOKEN_FLOAT, 0)) {
        Token t = peek(0);
        advance();
        return createNode(NODE_FLOAT, t.value);
    }

    if (is(TOKEN_BOOLEAN, 0)) { //So, internally treated as ints.
        Token t = peek(0);
        advance();
        // convert true/false to 1/0
        int val = strcmp(t.value, "true") == 0 ? 1 : 0;
        char* strVal = val ? "1" : "0";
        return createNode(NODE_NUMBER, strVal);
    }

    if (is(TOKEN_STRING, 0)) {
        Token t = peek(0);
        advance();
        return createNode(NODE_STRING, t.value);
    }
    
    if (is(TOKEN_IDENTIFIER, 0)) {
        Token t = peek(0);
        advance();
        return createNode(NODE_VARIABLE, t.value);
    }

    // List Literal: [ elem1, elem2, ... ]
    if (is(TOKEN_SQUARE, 0) && strcmp(peek(0).value, "[") == 0) {
        advance(); // Eat '['
        ASTNode* listNode = createNode(NODE_LIST_LITERAL, "list");
        
        if (!is(TOKEN_SQUARE, 0) || strcmp(peek(0).value, "]") != 0) {
            while (true) {
                ASTNode* elem = parseExpression();
                if (!elem) { freeAST(listNode); return NULL; }
                addChild(listNode, elem);
                if (is(TOKEN_COMMA, 0)) {
                    advance();
                } else {
                    break;
                }
            }
        }

        if (is(TOKEN_SQUARE, 0) && strcmp(peek(0).value, "]") == 0) {
            advance(); // Eat ']'
        } else {
            throw(TOKEN_SQUARE); // Expected ']'
            freeAST(listNode);
            return NULL;
        }
        return listNode;
    }

    // Dictionary Literal: { "key": value, ... }
    if (is(TOKEN_CURLY, 0) && strcmp(peek(0).value, "{") == 0) {
        advance(); // Eat '{'
        ASTNode* dictNode = createNode(NODE_DICT_LITERAL, "dict");

        if (!is(TOKEN_CURLY, 0) || strcmp(peek(0).value, "}") != 0) {
            while(true) {
                ASTNode* key = parseExpression();
                if (!key) { freeAST(dictNode); return NULL; }
                addChild(dictNode, key);

                if (is(TOKEN_COLON, 0)) {
                    advance();
                } else {
                    throw(TOKEN_COLON);
                    freeAST(dictNode);
                    return NULL;
                }

                ASTNode* value = parseExpression();
                if (!value) { freeAST(dictNode); return NULL; }
                addChild(dictNode, value);

                if (is(TOKEN_COMMA, 0)) {
                    advance();
                } else {
                    break;
                }
            }
        }

        if (is(TOKEN_CURLY, 0) && strcmp(peek(0).value, "}") == 0) {
            advance(); // Eat '}'
        } else {
            throw(TOKEN_CURLY);
            freeAST(dictNode);
            return NULL;
        }
        return dictNode;
    }

    if (is(TOKEN_PARENTHESIS, 0) && strcmp(peek(0).value, "(") == 0) {
        advance();
        ASTNode* node = parseExpression();
        if (is(TOKEN_PARENTHESIS, 0) && strcmp(peek(0).value, ")") == 0) {
            advance();
        } else {
            throw(TOKEN_PARENTHESIS);;
            return NULL;
        }
        return node;
    }

    throwVoid();
    return NULL;
}

ASTNode* parsePower() {
    ASTNode* left = parsePostfixExpression();
    if (is(TOKEN_OPERATOR, 0) && strcmp(peek(0).value, "**") == 0) {
        Token op = peek(0);
        advance();
        ASTNode* right = parsePower(); // recurse for right-associativity
        ASTNode* node = createNode(NODE_BINARY_OP, op.value);
        node->left = left;
        node->right = right;
        return node;
    }
    return left;
}

ASTNode* parseTerm() {
    ASTNode* left = parsePower();

    while (is(TOKEN_OPERATOR, 0) && (strcmp(peek(0).value, "*") == 0 || strcmp(peek(0).value, "/") == 0 || strcmp(peek(0).value, "//") == 0)){
        if (parserError) break; 
        Token op = peek(0);
        advance();
        ASTNode* right = parsePower();
        
        ASTNode* node = createNode(NODE_BINARY_OP, op.value);
        node->left = left;
        node->right = right;
        left = node;
    }
    return left;
}

ASTNode* parseAddSub() {
    ASTNode* left = parseTerm();
    while (is(TOKEN_OPERATOR, 0) && (strcmp(peek(0).value, "+") == 0 || strcmp(peek(0).value, "-") == 0)) {
        if (parserError) break;
        Token op = peek(0);
        advance();
        ASTNode* right = parseTerm();
        
        ASTNode* node = createNode(NODE_BINARY_OP, op.value);
        node->left = left;
        node->right = right;
        left = node;
    }
    return left;
}

ASTNode* parseComparison() {
    ASTNode* left = parseAddSub();

    while (is(TOKEN_COMPARISON, 0) && (strcmp(peek(0).value, "<") == 0 || strcmp(peek(0).value, ">") == 0 || strcmp(peek(0).value, "<=") == 0 || strcmp(peek(0).value, ">=") == 0)) {
        if (parserError) break;
        Token op = peek(0);
        advance();
        ASTNode* right = parseAddSub();
        
        ASTNode* node = createNode(NODE_BINARY_OP, op.value);
        node->left = left;
        node->right = right;
        left = node;
    }
    return left;
}

ASTNode* parseEquality() {
    ASTNode* left = parseComparison();

    while (is(TOKEN_COMPARISON, 0) && (strcmp(peek(0).value, "==") == 0 || strcmp(peek(0).value, "!=") == 0)) {
        if (parserError) break;
        Token op = peek(0);
        advance();
        ASTNode* right = parseComparison();
        
        ASTNode* node = createNode(NODE_BINARY_OP, op.value);
        node->left = left;
        node->right = right;
        left = node;
    }
    return left;
}

ASTNode* parseLogic() {
    ASTNode* left = parseEquality();

    while (is(TOKEN_OPERATOR, 0) && (strcmp(peek(0).value, "and") == 0 || strcmp(peek(0).value, "or") == 0)) {
        if (parserError) break;
        Token op = peek(0);
        advance();
        ASTNode* right = parseEquality();
        
        ASTNode* node = createNode(NODE_BINARY_OP, op.value);
        node->left = left;
        node->right = right;
        left = node;
    }
    return left;
}

ASTNode* parseExpression() {
    return parseLogic();
}

ASTNode* parseFunctionDefinition(char* name) {
    advance();
    ASTNode* funcNode = createNode(NODE_FUNCTION_DEF, name);
    
    // 1. Parse Arguments: (arg1, arg2)
    if (!is(TOKEN_PARENTHESIS, 0) || strcmp(peek(0).value, ")") != 0) {
        while (true) {
            if (parserError) break;
            if (is(TOKEN_IDENTIFIER, 0)) {
                Token t = peek(0);
                advance();
                ASTNode* arg = createNode(NODE_VARIABLE, t.value);
                addChild(funcNode, arg);
            } else {
                throw(TOKEN_IDENTIFIER);
            }

            if (is(TOKEN_COMMA, 0)) {
                advance();
            } else {
                break;
            }
        }
    }

    if (is(TOKEN_PARENTHESIS, 0) && strcmp(peek(0).value, ")") == 0) {
        advance(); 
    } else {
        throw(TOKEN_PARENTHESIS);
        return NULL;
    }

    // 2. Parse Body: statements until 'end'
    ASTNode* body = createNode(NODE_PROGRAM, "BODY");
    addChild(funcNode, body);

    while (!is(TOKEN_EOF, 0)) {
        if (parserError) break;
        if (is(TOKEN_KEYWORD, 0) && strcmp(peek(0).value, "end") == 0) {
            advance();
            break;
        }
        if (is(TOKEN_SEMICOLON, 0)){
            advance();
            continue;
        }
        ASTNode* stmt = parseStatement();
        if (stmt) {
            addChild(body, stmt);
        } else if (!parserError) {
            advance(); // unstick the parser
        }

    }
    
    return funcNode;
}

ASTNode* copyAST(ASTNode* node) {
    if (!node) return NULL;
    ASTNode* new_node = createNode(node->type, node->value);
    new_node->left = copyAST(node->left);
    new_node->right = copyAST(node->right);
    // Note: children array not copied as it's not used for assignment targets (VARIABLE/INDEX_ACCESS)
    return new_node;
}

ASTNode* parseStatement() {
    // Handle function definitions with 'set'
    if (is(TOKEN_KEYWORD, 0) && strcmp(peek(0).value, "set") == 0) {
        if (is(TOKEN_IDENTIFIER, 1) && is(TOKEN_PARENTHESIS, 2)) {
            advance(); // eat set
            Token nameToken = peek(0);
            advance(); // eat name
            return parseFunctionDefinition(nameToken.value); 
        } else if (is(TOKEN_IDENTIFIER, 1) && is(TOKEN_EQUALS, 2)){
            advance();
            Token nameToken = peek(0);
            advance();
            advance();
            ASTNode* expr = parseExpression();
            ASTNode* node = createNode(NODE_ASSIGNMENT, "=");
            node->left = createNode(NODE_VARIABLE, nameToken.value);
            node->right = expr;
            return node;
        } else {
            throwMultiple(2, TOKEN_KEYWORD, TOKEN_IDENTIFIER);
            return NULL;
        }
    }

    if (is(TOKEN_KEYWORD, 0) && strcmp(peek(0).value, "return") == 0) {
        advance(); // Eat 'return'
        ASTNode* expr = parseExpression();
        ASTNode* node = createNode(NODE_RETURN, "return");
        node->right = expr;
        return node;
    }

    if (is(TOKEN_KEYWORD, 0) && strcmp(peek(0).value, "break") == 0) {
        advance(); // Eat 'break'
        return createNode(NODE_BREAK, "break");
    }

    if (is(TOKEN_KEYWORD, 0) && strcmp(peek(0).value, "print") == 0) {
        advance(); // Eat 'print'
        ASTNode* expr = parseExpression();
        ASTNode* node = createNode(NODE_PRINT, "print");
        node->right = expr;
        return node;
    }

    if (is(TOKEN_KEYWORD, 0) && strcmp(peek(0).value, "Import")==0){
        advance(); // Eat 'import'
        ASTNode* expr = parseExpression();
        ASTNode* node = createNode(NODE_IMPORT, "Import");
        node->right = expr;
        return node;
    }

    if (is(TOKEN_KEYWORD, 0) && strcmp(peek(0).value, "repeat")== 0){
        advance();
        ASTNode* times = parseExpression();
        ASTNode* node = createNode(NODE_REPEAT, "repeat");
        addChild(node, times);
        ASTNode* body = createNode(NODE_PROGRAM, "BODY");
        addChild(node, body);
        if (is(TOKEN_SEMICOLON, 0)) {advance();}
        while (!is(TOKEN_EOF, 0)) {
            if (parserError) break;
            if (is(TOKEN_KEYWORD, 0) && strcmp(peek(0).value, "end") == 0) {
                advance(); // Eat 'end'
                break;
            }
            ASTNode* stmt = parseStatement();
            if (stmt) {
                addChild(body, stmt);
            } else if (!parserError) {
                advance(); // unstick the parser
            }
        }
        return node;
    }

    if (is(TOKEN_KEYWORD, 0) && strcmp(peek(0).value, "if") == 0) {
        advance(); // Eat if
        ASTNode* cond = parseExpression();
        ASTNode* node = createNode(NODE_IF, "if");
        addChild(node, cond);
        
        ASTNode* body = createNode(NODE_PROGRAM, "IF_BODY");
        addChild(node, body);
        if (is(TOKEN_SEMICOLON, 0)){advance();}
        while (!is(TOKEN_EOF, 0)) { //After condition, there should be a newline or a semicolon.
            if (parserError) break;
            if (is(TOKEN_KEYWORD, 0) && strcmp(peek(0).value, "end") == 0) {
                advance(); // Eat 'end'
                break;
            }
            ASTNode* stmt = parseStatement();
            if (stmt) {
                addChild(body, stmt);
            } else if (!parserError) {
                advance(); // unstick the parser
            }
        }
        return node;
    }

    if (is(TOKEN_KEYWORD, 0) && strcmp(peek(0).value, "while") == 0) {
        advance(); // Eat 'while'
        ASTNode* cond = parseExpression();
        ASTNode* node = createNode(NODE_WHILE, "while");
        addChild(node, cond);
        
        ASTNode* body = createNode(NODE_PROGRAM, "WHILE_BODY");
        addChild(node, body);
        
        if (is(TOKEN_SEMICOLON, 0)) {advance();}
        while (!is(TOKEN_EOF, 0)) {
            if(parserError) break; 
            if (is(TOKEN_KEYWORD, 0) && strcmp(peek(0).value, "end") == 0) {
                advance(); // Eat 'end'
                break;
            }
            ASTNode* stmt = parseStatement();
            if (stmt) {
                addChild(body, stmt);
            } else if (!parserError) {
                advance(); // unstick the parser
            }
        }
        return node;
    }

    // Handle assignments (`set x = ...` or `x = ...`) and expression statements
    ASTNode* expr = parseExpression();
    if (!expr) return NULL;

    if (is(TOKEN_EQUALS, 0) || is(TOKEN_COMPOUND_ASSIGN, 0)) { // It's an assignment
        if (expr->type != NODE_VARIABLE && expr->type != NODE_INDEX_ACCESS) {
            printf("[PARSER]SyntaxException: Invalid target for assignment.\n"); //Since this expected nodes, we allow this not to be throw().
            parserError = 1;
            freeAST(expr);
            return NULL;
        }
        
        Token opToken = peek(0);
        advance(); // eat '=' or '+=' etc

        ASTNode* rvalue = parseExpression();
        if (!rvalue) { freeAST(expr); return NULL; }

        if (opToken.type == TOKEN_COMPOUND_ASSIGN) {
            char opChar[2] = { opToken.value[0], '\0' };
            ASTNode* binaryOp = createNode(NODE_BINARY_OP, opChar);
            binaryOp->left = copyAST(expr);
            binaryOp->right = rvalue;
            rvalue = binaryOp;
        }

        ASTNode* assignmentNode = createNode(NODE_ASSIGNMENT, "=");
        assignmentNode->left = expr;
        assignmentNode->right = rvalue;
        return assignmentNode;
    }

    // It was not an assignment, just a standalone expression 
    return expr;
}

ASTNode* parseFile(TokenStream Tokens){
    if (globalStream == NULL) globalStream = malloc(sizeof(TokenStream));
    *globalStream = Tokens;

    currentIndex = 0;
    parserError = 0;
    ASTNode* root = createNode(NODE_PROGRAM, "ROOT");

    while (!is(TOKEN_EOF, 0)) {
        if (parserError) break;
        if (is(TOKEN_SEMICOLON, 0)) {
            advance();
            continue;
        }

        ASTNode* stmt = parseStatement(); 

        if (stmt) {
            addChild(root, stmt);
        } else {
            advance(); 
        }
    }
    
    // Cleanup the global parser state
    if (globalStream) {
        free(globalStream);
        globalStream = NULL;
    }

    if (parserError) {
        freeAST(root);
        return NULL;
    }

    return root;
}

void freeAST(ASTNode* node) {
    if (node == NULL) return;
    if (node->value) free(node->value);
    freeAST(node->left);
    freeAST(node->right);
    if (node->children) {
        for (int i = 0; i < node->childCount; i++) freeAST(node->children[i]);
        free(node->children);
    }
    free(node);
}