#include "syntax.h"
#include "lexer.h"
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
//Licenced under BMLL2.0, see LICENSE for further info
int currentIndex = 0;
int scopeLevel = 0;
TokenStream* globalStream;

struct Exception* currentException = NULL;
int parserError = 0;

const char* keywords[] = {
    "set",
    "print",
    "if",
    "while",
    "end",
    NULL
};

Token peek(int x) {
    // Safety check: Ensure we don't go out of bounds (past EOF)
    if (currentIndex + x >= globalStream->size) {
        return globalStream->tokens[globalStream->size - 1]; // Return the EOF token
    }
    
    // Safety check: Ensure we don't go backwards before the start
    if (currentIndex + x < 0) {
        return globalStream->tokens[0];
    }

    return globalStream->tokens[currentIndex + x];
}

void throw(char* msg){
    Token errorToken = peek(0);
    fprintf(stderr, "Error on Line %d, Char %d: %s\n", 
            errorToken.ln, 
            errorToken.character, 
            msg);
    parserError = 1;
}

// Helper: Move to the next token
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

bool is(TokenType checker, int ind) {
    return peek(ind).type == checker;
}
TokenType get(int ind) {
    return peek(ind).type;
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

ASTNode* parseExpression(); // Forward declaration
ASTNode* parseStatement();  // Forward declaration

ASTNode* parseFactor() {
    if (is(TOKEN_NUMBER, 0)) {
        Token t = peek(0);
        advance();
        return createNode(NODE_NUMBER, t.value);
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

    if (is(TOKEN_FUNCTION, 0)) {
        Token t = peek(0);
        advance(); // Eat function name
        
        ASTNode* node = createNode(NODE_FUNCTION_CALL, t.value);
        
        if (is(TOKEN_PARENTHESIS, 0) && strcmp(peek(0).value, "(") == 0) {
            advance();
        } else {
            throw("Expected '(' after function name");
            return NULL;
        }

        if (!is(TOKEN_PARENTHESIS, 0) || strcmp(peek(0).value, ")") != 0) {
            while (true) {
                ASTNode* arg = parseExpression();
                addChild(node, arg);
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
            throw("Expected ')' after function arguments");
            return NULL;
        }
        return node;
    }

    if (is(TOKEN_PARENTHESIS, 0) && strcmp(peek(0).value, "(") == 0) {
        advance();
        ASTNode* node = parseExpression();
        if (is(TOKEN_PARENTHESIS, 0) && strcmp(peek(0).value, ")") == 0) {
            advance();
        } else {
            throw("Expected ')'");
            return NULL;
        }
        return node;
    }

    throw("Unexpected token in expression");
    return NULL;
}

ASTNode* parseTerm() {
    ASTNode* left = parseFactor();

    while (is(TOKEN_OPERATOR, 0) && (strcmp(peek(0).value, "*") == 0 || strcmp(peek(0).value, "/") == 0)) {
        Token op = peek(0);
        advance();
        ASTNode* right = parseFactor();
        
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

    while (is(TOKEN_COMPARISON, 0) && (strcmp(peek(0).value, "<") == 0 || strcmp(peek(0).value, ">") == 0 || 
                                     strcmp(peek(0).value, "<=") == 0 || strcmp(peek(0).value, ">=") == 0)) {
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
    advance(); // Eat '('
    
    ASTNode* funcNode = createNode(NODE_FUNCTION_DEF, name);
    
    // 1. Parse Arguments: (arg1, arg2)
    if (!is(TOKEN_PARENTHESIS, 0) || strcmp(peek(0).value, ")") != 0) {
        while (true) {
            if (is(TOKEN_IDENTIFIER, 0)) {
                Token t = peek(0);
                advance();
                ASTNode* arg = createNode(NODE_VARIABLE, t.value);
                addChild(funcNode, arg);
            } else {
                throw("Expected argument name");
            }

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
        throw("Expected ')' after arguments");
        return NULL;
    }

    // 2. Parse Body: statements until 'end'
    ASTNode* body = createNode(NODE_PROGRAM, "BODY");
    addChild(funcNode, body);

    while (!is(TOKEN_EOF, 0)) {
        if (is(TOKEN_KEYWORD, 0) && strcmp(peek(0).value, "end") == 0) {
            advance(); // Eat 'end'
            break;
        }
        ASTNode* stmt = parseStatement();
        if (stmt) addChild(body, stmt);
    }
    
    return funcNode;
}

ASTNode* parseStatement() {
    // 1. Check for 'set' keyword
    if (is(TOKEN_KEYWORD, 0) && strcmp(peek(0).value, "set") == 0) {
        advance(); // Eat 'set'

        Token nameToken = peek(0);
        if (nameToken.type != TOKEN_IDENTIFIER) {
            throw("Expected variable or function name after 'set'");
            return NULL;
        }
        advance(); // Eat the name

        // Check: Is this a Function Definition? e.g., set y(num) do
        if (is(TOKEN_PARENTHESIS, 0)) {
            return parseFunctionDefinition(nameToken.value); 
        } 
        
        // Check: Is this a Variable Assignment? e.g., set x = 10
        if (is(TOKEN_EQUALS, 0)) {
            advance(); // Eat '='
            ASTNode* value = parseExpression();
            
            ASTNode* node = createNode(NODE_ASSIGNMENT, nameToken.value);
            node->right = value;
            return node;
        }

        throw("Expected '=' or '(' after 'set'");
        return NULL;
    }

    // 2. Check for other keywords (like 'return')
    if (is(TOKEN_KEYWORD, 0) && strcmp(peek(0).value, "return") == 0) {
        advance(); // Eat 'return'
        ASTNode* expr = parseExpression();
        ASTNode* node = createNode(NODE_RETURN, "return");
        node->right = expr;
        return node;
    }

    // 3. Check for 'print'
    if (is(TOKEN_KEYWORD, 0) && strcmp(peek(0).value, "print") == 0) {
        advance(); // Eat 'print'
        ASTNode* expr = parseExpression();
        ASTNode* node = createNode(NODE_PRINT, "print");
        node->right = expr;
        return node;
    }

    // 4. Check for 'if'
    if (is(TOKEN_KEYWORD, 0) && strcmp(peek(0).value, "if") == 0) {
        advance(); // Eat 'if'
        ASTNode* cond = parseExpression();
        ASTNode* node = createNode(NODE_IF, "if");
        addChild(node, cond);
        
        ASTNode* body = createNode(NODE_PROGRAM, "IF_BODY");
        addChild(node, body);
        
        while (!is(TOKEN_EOF, 0)) {
            if (is(TOKEN_KEYWORD, 0) && strcmp(peek(0).value, "end") == 0) {
                advance(); // Eat 'end'
                break;
            }
            ASTNode* stmt = parseStatement();
            if (stmt) addChild(body, stmt);
        }
        return node;
    }

    // 5. Check for 'while'
    if (is(TOKEN_KEYWORD, 0) && strcmp(peek(0).value, "while") == 0) {
        advance(); // Eat 'while'
        ASTNode* cond = parseExpression();
        ASTNode* node = createNode(NODE_WHILE, "while");
        addChild(node, cond);
        
        ASTNode* body = createNode(NODE_PROGRAM, "WHILE_BODY");
        addChild(node, body);
        
        while (!is(TOKEN_EOF, 0)) {
            if (is(TOKEN_KEYWORD, 0) && strcmp(peek(0).value, "end") == 0) {
                advance(); // Eat 'end'
                break;
            }
            ASTNode* stmt = parseStatement();
            if (stmt) addChild(body, stmt);
        }
        return node;
    }

    // 6. Check for implicit assignment: variable = value
    if (is(TOKEN_IDENTIFIER, 0) && is(TOKEN_EQUALS, 1)) {
        Token nameToken = peek(0);
        advance(); // Eat name
        advance(); // Eat '='
        ASTNode* value = parseExpression();
        ASTNode* node = createNode(NODE_ASSIGNMENT, nameToken.value);
        node->right = value;
        return node;
    }

    // 7. If no keywords match, it must be a standalone expression (like a function call)
    return parseExpression();
}

ASTNode* parseFile(TokenStream Tokens){
    if (globalStream == NULL) globalStream = malloc(sizeof(TokenStream));
    *globalStream = Tokens;

    parserError = 0;
    ASTNode* root = createNode(NODE_PROGRAM, "ROOT");

    while (!is(TOKEN_EOF, 0)) {
    if (parserError) break;
    if (is(TOKEN_SEMICOLON, 0)) {
        advance();
        continue;
    }

    // This function acts like a "Dispatcher"
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