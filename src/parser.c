#include "parser.h"
#include "lexer.h"
#include "err_handler.h"
#include <stdarg.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
//Licensed under BMLL2.0, see LICENSE for further info

static int currentIndex = 0;
static int scopeLevel = 0;
static TokenStream* globalStream;
static int checkMode = 0;

static const char* tokenTypeToString(TokenType type){
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
        case TOKEN_UNARY: return "TOKEN_UNARY";
        case TOKEN_AT: return "TOKEN_AT";
        case TOKEN_DLRSIGN: return "TOKEN_DLRSIGN";
        case TOKEN_CARET: return "TOKEN_CARET";
        case TOKEN_HASHTAG: return "TOKEN_HASHTAG";
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
    "return",
    "Import",
    "break",
    "repeat",
    "and",
    "or",
    "equals",
    "isnt",
    "else",
    "readfile",
    "as",
    "overwrite",
    "put",
    "represent",
    "forEach",
    "in",
    "unset",
    "do",
    "createfile",
    NULL
};

static bool isKeyword(const char* keyword){
    for (int i = 0; keywords[i] != NULL; i++) {
        if (strcmp(keyword, keywords[i]) == 0) {
            return true;
        }
    }
    return false;
}

static Token peek(const int x) {
    if (currentIndex + x >= globalStream->size) {
        return globalStream->tokens[globalStream->size - 1]; // Return the EOF token
    }

    if (currentIndex + x < 0) {
        return globalStream->tokens[0];
    }
    return globalStream->tokens[currentIndex + x];
}

static bool is(const TokenType checker, const int ind) {
    return peek(ind).type == checker;
}

static bool is_multiple(const TokenType type, const int ind, ...) {
    if (!is(type, ind)) return false;
    va_list list;
    va_start(list, ind);
    const char* expect;

    while ((expect = va_arg(list, const char*)) != NULL) {
        if (strcmp(expect, peek(ind).value) == 0) {
            va_end(list);
            return true;
        }
    }
    va_end(list);
    return false;
}

static TokenType get(const int ind) {
    return peek(ind).type;
}

static void throwVoid(){
    Token errorToken = peek(0);
    char msg[128];
    snprintf(msg, sizeof(msg), "Unexpected token: %s", tokenTypeToString(get(0)));
    error_t err = make_error(ERRTY_SyntaxError, msg, errorToken.ln, errorToken.character);
    set_err(&err);
    halt();
}

static void throw(const TokenType expected){
    const Token errorToken = peek(0);
    char msg[128];
    snprintf(msg, sizeof(msg), "Expected %s, got %s", tokenTypeToString(expected), tokenTypeToString(errorToken.type));
    error_t err = make_error(ERRTY_SyntaxError, msg, errorToken.ln, errorToken.character);
    set_err(&err);
    halt();
}

static void throwMultiple(const int expectedCount, ...){
    const Token errorToken = peek(0);
    const TokenType got = get(0);
    char msg[256] = "Expected ";
    va_list args;
    va_start(args, expectedCount);
    for (int i = 0; i < expectedCount; i++) {
        const TokenType expected = va_arg(args, TokenType);
        if (i > 0) {
            strcat(msg, " or ");
        }
        strcat(msg, tokenTypeToString(expected));
    }
    va_end(args);
    char suffix[64];
    snprintf(suffix, sizeof(suffix), ", got %s", tokenTypeToString(got));
    strcat(msg, suffix);

    error_t err = make_error(ERRTY_SyntaxError, msg, errorToken.ln, errorToken.character);
    set_err(&err);
    halt();
}

static void advance() {
    if (currentIndex < globalStream->size) currentIndex++;
}

static ASTNode* createNode(const NodeType type, const char* value) {
    ASTNode* node = malloc(sizeof(ASTNode));
    if (!node) {
        error_t err = make_error(ERRTY_InternalError, "Couldn't allocate ASTNode", 0, 0);
        set_err(&err);
        halt();
    }
    node->type = type;
    node->type_notation = NULL;
    node->type_condition = NULL;
    node->value = value ? strdup(value) : NULL;
    node->left = NULL;
    node->right = NULL;
    node->children = NULL;
    node->childCapacity = 0;
    node->childCount = 0;
    node->bitlength = 0;
    node->isDeclaration = 0;
    return node;
}

static void addChild(ASTNode* parent, ASTNode* child) {
    if (child == NULL) return;

    // 1. If this is the first child, initialize the array
    if (parent->children == NULL) {
        parent->childCapacity = 4; // Start with space for 4 statements
        parent->childCount = 0;
        parent->children = malloc(sizeof(ASTNode*) * parent->childCapacity);
        if (!parent->children) {
            error_t err = make_error(ERRTY_InternalError, "Couldn't allocate children array", 0, 0);
            set_err(&err);
            halt();
        }
    }

    // 2. If the array is full, double the size
    if (parent->childCount >= parent->childCapacity) {
        parent->childCapacity *= 2;
        void* realloc_result = realloc(parent->children, sizeof(ASTNode*) * parent->childCapacity);
        if (!realloc_result) {
            error_t err = make_error(ERRTY_InternalError, "Couldn't reallocate children array", 0, 0);
            set_err(&err);
            halt();
        }
        parent->children = realloc_result;
    }

    // 3. Add the child and increment the count
    parent->children[parent->childCount] = child;
    parent->childCount++;
}
//---------------------------------------------------------------------------------PARSERS----------------------------------------------------------------------------

static ASTNode* parseExpression();

static ASTNode* parseStatement();

static ASTNode* parsePrimaryExpression();

static ASTNode* parsePostfixExpression() {
    ASTNode* node = parsePrimaryExpression();
    if (!node) return NULL;

    while (true) {
        if (hasError) {
            freeAST(node);
            return NULL;
        }

        if (is(TOKEN_DOT, 0)) {
            // Member access: obj.attr  or  obj.method(...)
            advance(); // Eat '.'

            if (!is(TOKEN_IDENTIFIER, 0)) {
                throw(TOKEN_IDENTIFIER);
                freeAST(node);
                return NULL;
            }

            Token memberToken = peek(0);
            advance(); // Eat the attribute/method name

            if (is_multiple(TOKEN_PARENTHESIS, 0, "(", NULL)) {
                // Method call: obj.method(args...)
                advance(); // Eat '('
                ASTNode* callNode = createNode(NODE_METHOD_CALL, memberToken.value);
                callNode->left = node; // The object being called on

                // Parse arguments
                if (!is_multiple(TOKEN_PARENTHESIS, 0, ")", NULL)) {
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

                if (is_multiple(TOKEN_PARENTHESIS, 0, ")", NULL)) {
                    advance(); // Eat ')'
                } else {
                    throw(TOKEN_PARENTHESIS);
                    freeAST(callNode);
                    return NULL;
                }
                node = callNode;
            } else {
                // Plain attribute access: obj.attr
                ASTNode* accessNode = createNode(NODE_MEMBER_ACCESS, memberToken.value);
                accessNode->left = node;
                node = accessNode;
            }

        } else if (is_multiple(TOKEN_PARENTHESIS, 0, "(", NULL)) {
            // Function call
            if (node->type != NODE_VARIABLE) {
                Token errToken = peek(0);
                error_t err = make_error(ERRTY_SyntaxError, "Cannot call a non-variable expression as a function", errToken.ln, errToken.character);
                set_err(&err);
                freeAST(node);
                return NULL;
            }

            advance(); // Eat '('
            ASTNode* callNode = createNode(NODE_FUNCTION_CALL, node->value);
            free(node->value); // name is now in callNode
            free(node);        // free the original NODE_VARIABLE

            // Parse arguments
            if (!is_multiple(TOKEN_PARENTHESIS, 0, ")", NULL)) {
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

            if (is_multiple(TOKEN_PARENTHESIS, 0, ")", NULL)) {
                advance(); // Eat ')'
            } else {
                throw(TOKEN_PARENTHESIS);
                freeAST(callNode);
                return NULL;
            }
            node = callNode; // The call node is now the primary node
        } else if (is_multiple(TOKEN_SQUARE, 0, "[", NULL)) {
            // Index access: list[index]
            advance(); // Eat '['
            ASTNode* indexNode = parseExpression();
            if (!indexNode) { freeAST(node); return NULL; }

            if (is_multiple(TOKEN_SQUARE, 0, "]", NULL)) {
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
        } else if (is(TOKEN_CARET, 0)) {
            advance();
            ASTNode* deref = createNode(NODE_DEREF, "^");
            deref->left = node;
            node = deref;
        } else {
            break; // No more postfix operators
        }
    }
    return node;
}

static ASTNode* parseFunctionDefinition(const char* name);

ASTNode* parsePrimaryExpression() {
    if (is(ERR,0)){
        Token t = peek(0);
        printf("SyntaxException: Invalid token %s, Line %d Character %d", t.value, t.ln, t.character);
    }

    // Unary not
    if (is(TOKEN_UNARY, 0)) {
        advance();
        ASTNode* operand = parsePrimaryExpression();
        if (!operand) return NULL;
        ASTNode* node = createNode(NODE_UNARY, "not");
        node->right = operand;
        return node;
    }

    if (is(TOKEN_BINARY, 0)) {
        Token t = peek(0);
        advance();
        ASTNode* node = createNode(NODE_BINARY, t.value);
        node->bitlength = t.bitlength;
        return node;
    }

    if (is_multiple(TOKEN_OPERATOR, 0, "-", NULL)) {
        advance();
        ASTNode* operand = parsePrimaryExpression();
        if (!operand) return NULL;
        // Desugar to 0 - operand
        ASTNode* zero = createNode(NODE_NUMBER, "0");
        ASTNode* node = createNode(NODE_OPERATOR, "-");
        node->left = zero;
        node->right = operand;
        return node;
    }

    if (is(TOKEN_DLRSIGN, 0)) {
        advance();
        ASTNode* cmd = parsePrimaryExpression(); // string
        if (!cmd) return NULL;
        ASTNode* node = createNode(NODE_CMD, "$");
        node->right = cmd;
        return node;
    }

    if (is(TOKEN_NUMBER, 0)) {
        Token t = peek(0);
        advance();
        return createNode(NODE_NUMBER, t.value);
    }

    if (is(TOKEN_CARET, 0)) {
        advance();
        ASTNode* node = createNode(NODE_ADDRESS_OF, NULL);
        node->right = parseExpression(); // the variable being pointed to
        return node;
    }

    if (is(TOKEN_NON, 0)) {
        Token t = peek(0);
        advance();
        return createNode(NODE_NON, t.value);
    }

    if (is(TOKEN_FLOAT, 0)) {
        Token t = peek(0);
        advance();
        return createNode(NODE_FLOAT, t.value);
    }

    if (is(TOKEN_BOOLEAN, 0)) {
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
        if (is(TOKEN_AT, 0)) {
            advance();
            if (!is(TOKEN_IDENTIFIER, 0)) {throw(TOKEN_IDENTIFIER); return NULL;}
            Token typeToken = peek(0);
            ASTNode* node = createNode(NODE_CONVERSION, t.value);
            node->left = createNode(NODE_VARIABLE, t.value);
            node->right = createNode(NODE_VARIABLE, typeToken.value);
            advance();
            return node;
        }
        return createNode(NODE_VARIABLE, t.value);
    }

    // List Literal: [ elem1, elem2, ... ]
    if (is_multiple(TOKEN_SQUARE, 0, "[", NULL)) {
        advance(); // Eat '['
        ASTNode* listNode = createNode(NODE_LIST_LITERAL, "list");

        if (!is_multiple(TOKEN_SQUARE, 0, "]", NULL)) {
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

        if (is_multiple(TOKEN_SQUARE, 0, "]", NULL)) {
            advance(); // Eat ']'
        } else {
            throw(TOKEN_SQUARE); // Expected ']'
            freeAST(listNode);
            return NULL;
        }
        return listNode;
    }

    // Dictionary Literal: { "key": value, ... }
    if (is_multiple(TOKEN_CURLY, 0, "{", NULL)) {
        advance(); // Eat '{'
        ASTNode* dictNode = createNode(NODE_DICT_LITERAL, "dict");

        if (!is_multiple(TOKEN_CURLY, 0, "}", NULL)) {
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

        if (is_multiple(TOKEN_CURLY, 0, "}", NULL)) {
            advance(); // Eat '}'
        } else {
            throw(TOKEN_CURLY);
            freeAST(dictNode);
            return NULL;
        }
        return dictNode;
    }

    if (is_multiple(TOKEN_PARENTHESIS, 0, "(", NULL)) {
        advance();
        ASTNode* node = parseExpression();
        if (is_multiple(TOKEN_PARENTHESIS, 0, ")", NULL)) {
            advance();
        } else {
            throw(TOKEN_PARENTHESIS);;
            return NULL;
        }
        return node;
    }

    if (is_multiple(TOKEN_KEYWORD, 0, "do", NULL)) {
        advance();
        return parseFunctionDefinition("_LAMBDA_");
    }

    throwVoid();
    return NULL;
}

static ASTNode* parsePower() {
    ASTNode* left = parsePostfixExpression();
    if (!left || hasError) return NULL;

    if (is_multiple(TOKEN_OPERATOR, 0, "**", NULL)) {
        Token op = peek(0);
        advance();
        ASTNode* right = parsePower(); // recurse for right-associativity
        if (!right || hasError) {
            freeAST(left);
            freeAST(right);
            return NULL;
        }
        ASTNode* node = createNode(NODE_OPERATOR, op.value);
        node->left = left;
        node->right = right;
        return node;
    }
    return left;
}

static ASTNode* parseTerm() {
    ASTNode* left = parsePower();
    if (!left || hasError) return NULL;

    while (is_multiple(TOKEN_OPERATOR, 0, "/", "*", "//", "-/", "%%", NULL)){
        if (hasError) {
            freeAST(left);
            return NULL;
        }
        Token op = peek(0);
        advance();
        ASTNode* right = parsePower();
        if (!right || hasError) {
            freeAST(left);
            freeAST(right);
            return NULL;
        }

        ASTNode* node = createNode(NODE_OPERATOR, op.value);
        node->left = left;
        node->right = right;
        left = node;
    }
    return left;
}

static ASTNode* parseAddSub() {
    ASTNode* left = parseTerm();
    if (!left || hasError) return NULL;

    while (is_multiple(TOKEN_OPERATOR, 0, "+", "-", NULL)) {
        if (hasError) {
            freeAST(left);
            return NULL;
        }
        Token op = peek(0);
        advance();
        ASTNode* right = parseTerm();
        if (!right || hasError) {
            freeAST(left);
            freeAST(right);
            return NULL;
        }

        ASTNode* node = createNode(NODE_OPERATOR, op.value);
        node->left = left;
        node->right = right;
        left = node;
    }
    return left;
}

static ASTNode* parseComparison() {
    ASTNode* left = parseAddSub();
    if (!left || hasError) return NULL;

    while (is(TOKEN_COMPARISON, 0) || is_multiple(TOKEN_KEYWORD, 0, "equals", "isnt", NULL)) {
        if (hasError) {
            freeAST(left);
            return NULL;
        }
        const char* v = peek(0).value;
        if (strcmp(v, "<")  != 0 && strcmp(v, ">")  != 0 &&
            strcmp(v, "<=") != 0 && strcmp(v, ">=") != 0 &&
            strcmp(v, "==") != 0 && strcmp(v, "!=") != 0 &&
            strcmp(v, "equals") != 0 && strcmp(v, "isnt") != 0) {
            break;
        }
        Token op = peek(0);
        advance();
        ASTNode* right = parseAddSub();
        if (!right || hasError) {
            freeAST(left);
            freeAST(right);
            return NULL;
        }

        ASTNode* node = createNode(NODE_COMPARRISON, op.value);
        node->left = left;
        node->right = right;
        left = node;
    }
    return left;
}

static ASTNode* parseLogic() {
    ASTNode* left = parseComparison();
    if (!left || hasError) return NULL;

    while (is_multiple(TOKEN_KEYWORD, 0, "and", "or", NULL)) {
        if (hasError) {
            freeAST(left);
            return NULL;
        }
        Token op = peek(0);
        advance();
        ASTNode* right = parseComparison();
        if (!right || hasError) {
            freeAST(left);
            freeAST(right);
            return NULL;
        }

        ASTNode* node = createNode(NODE_LOGIC, op.value);
        node->left = left;
        node->right = right;
        left = node;
    }
    return left;
}

ASTNode* parseExpression() {
    ASTNode* left = parseLogic();
    if (!left || hasError) return NULL;

    if (is(TOKEN_QUESTION_MARK, 0)) {
        advance();

        ASTNode* thenExpr = parseExpression();
        if (!thenExpr || hasError) {
            freeAST(left);
            freeAST(thenExpr);
            return NULL;
        }

        if (!is(TOKEN_COLON, 0)) {
            throw(TOKEN_COLON);
            freeAST(left);
            freeAST(thenExpr);
            return NULL;
        } else {
            advance(); // skip :
        }

        ASTNode* elseExpr = parseExpression();
        if (!elseExpr || hasError) {
            freeAST(left);
            freeAST(thenExpr);
            freeAST(elseExpr);
            return NULL;
        }

        ASTNode* colonNode = createNode(NODE_COLON, ":");
        colonNode->left = thenExpr;
        colonNode->right = elseExpr;

        ASTNode* node = createNode(NODE_TERNARY, "?");
        node->left = left;
        node->right = colonNode;
        return node;
    }
    return left;
}

static ASTNode* parseFunctionDefinition(const char* name) {
    advance();
    ASTNode* funcNode = createNode(NODE_FUNCTION_DEF, name);

    // 1. Parse Arguments: (arg1, arg2)
    if (!is_multiple(TOKEN_PARENTHESIS, 0, ")", NULL)) {
        while (true) {
            if (hasError) {
                freeAST(funcNode);
                return NULL;
            }
            if (is(TOKEN_IDENTIFIER, 0)) {
                Token t = peek(0);
                advance();
                ASTNode* arg = createNode(NODE_VARIABLE, t.value);
                addChild(funcNode, arg);
                if (is(TOKEN_AT, 0)) {
                    advance();
                    if (!is(TOKEN_IDENTIFIER, 0)) {
                        throw(TOKEN_IDENTIFIER);
                        freeAST(funcNode);
                        return NULL;
                    }
                    Token typeToken = peek(0);
                    advance();
                    arg->type_notation = strdup(typeToken.value);
                    if (is(TOKEN_COLON, 0)) {
                        advance(); // eat :
                        if (!is(TOKEN_IDENTIFIER, 0)) {
                            throw(TOKEN_IDENTIFIER);
                            freeAST(funcNode);
                            return NULL;
                        }
                        Token condToken = peek(0);
                        advance();
                        arg->type_condition = strdup(condToken.value);
                    }
                }
                if (is(TOKEN_EQUALS, 0)) {
                    advance();
                    ASTNode* defaultVal = parseExpression();
                    if (!defaultVal || hasError) {
                        freeAST(funcNode);
                        return NULL;
                    }
                    arg->right = defaultVal;
                }
            } else {
                throw(TOKEN_IDENTIFIER);
                freeAST(funcNode);
                return NULL;
            }

            if (is(TOKEN_COMMA, 0)) {
                advance();
            } else {
                break;
            }
        }
    }

    if (is_multiple(TOKEN_PARENTHESIS, 0, ")", NULL)) {
        advance();
    } else {
        throw(TOKEN_PARENTHESIS);
        freeAST(funcNode);
        return NULL;
    }

    // 2. Parse Body: statements until 'end'
    ASTNode* body = createNode(NODE_PROGRAM, "BODY");
    addChild(funcNode, body);

    while (!is(TOKEN_EOF, 0)) {
        if (hasError) {
            freeAST(funcNode);
            return NULL;
        }
        if (is_multiple(TOKEN_KEYWORD, 0, "end", NULL)) {
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
        } else if (!(hasError)) {
            advance(); // unstick the parser
        }
    }

    return funcNode;
}

static ASTNode* copyAST(ASTNode* node) {
    if (!node || hasError) return NULL;
    ASTNode* new_node = createNode(node->type, node->value);
    if (node->type_notation) new_node->type_notation = strdup(node->type_notation);
    if (node->type_condition) new_node->type_condition = strdup(node->type_condition);
    new_node->left = copyAST(node->left);
    new_node->right = copyAST(node->right);
    // Note: children array not copied as it's not used for assignment targets (VARIABLE/INDEX_ACCESS)
    return new_node;
}

ASTNode* parseStatement() {
    if (hasError) return NULL;

    // Handle function definitions with 'set'
    if (is_multiple(TOKEN_KEYWORD, 0, "set", NULL)) {
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
            if (!expr || hasError) {
                freeAST(expr);
                return NULL;
            }
            ASTNode* node = createNode(NODE_ASSIGNMENT, "=");
            node->isDeclaration = 1; // written with 'set': allowed to create the variable
            node->left = createNode(NODE_VARIABLE, nameToken.value);
            node->right = expr;
            return node;
        } else {
            throwMultiple(2, TOKEN_KEYWORD, TOKEN_IDENTIFIER);
            return NULL;
        }
    }

    if (is_multiple(TOKEN_KEYWORD, 0, "unset", NULL)) {
        advance();
        ASTNode* target = parsePostfixExpression();
        if (!target || hasError) {
            freeAST(target);
            return NULL;
        }
        ASTNode* node = createNode(NODE_UNSET, NULL);
        if (target->type == NODE_DEREF) {
            node->left = target;
        } else if (target->type == NODE_VARIABLE) {
            node->value = strdup(target->value);
            freeAST(target);
        } else {
            freeAST(target);
            throwVoid();
            return NULL;
        }
        return node;
    }

    if (is_multiple(TOKEN_KEYWORD, 0, "return", NULL)) {
        advance(); // Eat 'return'
        ASTNode* expr = parseExpression();
        if (hasError) {
            freeAST(expr);
            return NULL;
        }
        ASTNode* node = createNode(NODE_RETURN, "return");
        node->right = expr;
        return node;
    }

    if (is_multiple(TOKEN_KEYWORD, 0, "break", NULL)) {
        advance(); // Eat 'break'
        return createNode(NODE_BREAK, "break");
    }

    if (is_multiple(TOKEN_KEYWORD, 0, "print", NULL)) {
        advance(); // Eat 'print'
        ASTNode* expr = parseExpression();
        if (hasError) {
            freeAST(expr);
            return NULL;
        }
        ASTNode* node = createNode(NODE_PRINT, "print");
        node->right = expr;
        return node;
    }

    if (is_multiple(TOKEN_KEYWORD, 0, "represent", NULL)) {
        advance(); // Eat 'represent'
        ASTNode* expr = parseExpression();
        if (hasError) {
            freeAST(expr);
            return NULL;
        }
        ASTNode* node = createNode(NODE_REPRESENT, "represent");
        node->right = expr;
        return node;
    }

    if (is_multiple(TOKEN_KEYWORD, 0, "Import", NULL)){
        advance(); // Eat 'Import'
        ASTNode* expr = parseExpression();
        if (hasError) {
            freeAST(expr);
            return NULL;
        }
        ASTNode* node = createNode(NODE_IMPORT, "Import");
        node->right = expr;
        return node;
    }

    if (is_multiple(TOKEN_KEYWORD, 0, "forEach", NULL)) {
        advance();
        Token varTkn = peek(0);
        advance(); //eat varTkn
        ASTNode* varDef = createNode(NODE_VARIABLE, varTkn.value);
        ASTNode* node = createNode(NODE_FOREACH, "forEach");
        if (!is_multiple(TOKEN_KEYWORD, 0, "in", NULL)) {
            throw(TOKEN_KEYWORD);
            freeAST(varDef);
            freeAST(node);
            return NULL;
        }
        ASTNode* inNode = createNode(NODE_IN, "in"); advance();
        if (!is(TOKEN_IDENTIFIER, 0)) {
            throw(TOKEN_IDENTIFIER);
            freeAST(varDef);
            freeAST(node);
            freeAST(inNode);
            return NULL;
        }
        ASTNode* varGet = parseExpression();
        if (!varGet || hasError) {
            freeAST(varDef);
            freeAST(node);
            freeAST(inNode);
            freeAST(varGet);
            return NULL;
        }
        inNode->left = varDef; inNode->right = varGet;
        addChild(node, inNode);
        ASTNode* body = createNode(NODE_PROGRAM, "BODY");
        addChild(node, body);
        if (is(TOKEN_SEMICOLON, 0)) {advance();}
        while (!is(TOKEN_EOF, 0)) {
            if (hasError) {
                freeAST(node);
                return NULL;
            }
            if (is_multiple(TOKEN_KEYWORD, 0, "end", NULL)) {
                advance();
                break;
            }
            ASTNode* stmt = parseStatement();
            if (stmt) {
                addChild(body, stmt);
            } else if (!(hasError)) {
                advance();
            }
        }
        return node;
    }

    if (is_multiple(TOKEN_KEYWORD, 0, "repeat", NULL)){
        advance();
        ASTNode* times = parseExpression();
        if (!times || hasError) {
            freeAST(times);
            return NULL;
        }
        ASTNode* node = createNode(NODE_REPEAT, "repeat");
        addChild(node, times);
        ASTNode* body = createNode(NODE_PROGRAM, "BODY");
        addChild(node, body);
        if (is(TOKEN_SEMICOLON, 0)) {advance();}
        while (!is(TOKEN_EOF, 0)) {
            if (hasError) {
                freeAST(node);
                return NULL;
            }
            if (is_multiple(TOKEN_KEYWORD, 0, "end", NULL)) {
                advance(); // Eat 'end'
                break;
            }
            ASTNode* stmt = parseStatement();
            if (stmt) {
                addChild(body, stmt);
            } else if (!(hasError)) {
                advance(); // unstick the parser
            }
        }
        return node;
    }

    if (is_multiple(TOKEN_KEYWORD, 0, "if", NULL)) {
        advance(); // Eat if
        ASTNode* cond = parseExpression();
        if (!cond || hasError) {
            freeAST(cond);
            return NULL;
        }
        ASTNode* node = createNode(NODE_IF, "if");
        addChild(node, cond);

        ASTNode* body = createNode(NODE_PROGRAM, "IF_BODY");
        addChild(node, body);
        if (is(TOKEN_SEMICOLON, 0)){advance();}
        while (!is(TOKEN_EOF, 0)) { //After condition, there should be a newline or a semicolon.
            if (hasError) {
                freeAST(node);
                return NULL;
            }
            if (is_multiple(TOKEN_KEYWORD, 0, "end", NULL)) {
                advance(); // Eat 'end'
                break;
            } else if (is_multiple(TOKEN_KEYWORD, 0, "else", NULL)) {
                ASTNode* elsenode = parseStatement();
                if (elsenode) {addChild(node, elsenode);}
                break;
            }
            ASTNode* stmt = parseStatement();
            if (stmt) {
                addChild(body, stmt);
            } else if (!(hasError)) {
                advance(); // unstick the parser
            }
        }
        return node;
    }

    if (is_multiple(TOKEN_KEYWORD, 0, "else", NULL)) {
        advance(); // Eat else
        if (!is_multiple(TOKEN_KEYWORD, 0, "end", "print", "set", "while", "repeat", "if", "return", "break", "Import", "readfile", "overwrite", "put", NULL) && !is(TOKEN_EOF, 0) && !is(TOKEN_SEMICOLON, 0)) {
            ASTNode* cond = parseExpression();
            if (!cond || hasError) {
                freeAST(cond);
                return NULL;
            }
            ASTNode* node = createNode(NODE_ELSE, "else");
            addChild(node, cond);

            ASTNode* body = createNode(NODE_PROGRAM, "ELSE_BODY");
            addChild(node, body);
            if (is(TOKEN_SEMICOLON, 0)) advance();
            while (!is(TOKEN_EOF, 0)) {
                if (hasError) {
                    freeAST(node);
                    return NULL;
                }
                if (is_multiple(TOKEN_KEYWORD, 0, "end", NULL)) { advance(); break; }
                else if (is_multiple(TOKEN_KEYWORD, 0, "else", NULL)) {
                    ASTNode* elsenode = parseStatement();
                    if (elsenode) {addChild(node, elsenode);}
                    break;
                }
                ASTNode* stmt = parseStatement();
                if (stmt) addChild(body, stmt);
                else if (!(hasError)) advance();
            }
            return node;
        } else {
            ASTNode* node = createNode(NODE_ELSE, "else");

            ASTNode* body = createNode(NODE_PROGRAM, "ELSE_BODY");
            addChild(node, body);
            if (is(TOKEN_SEMICOLON, 0)) advance();
            while (!is(TOKEN_EOF, 0)) {
                if (hasError) {
                    freeAST(node);
                    return NULL;
                }
                if (is_multiple(TOKEN_KEYWORD, 0, "end", NULL)) { advance(); break; }
                else if (is_multiple(TOKEN_KEYWORD, 0, "else", NULL)) {
                    ASTNode* elsenode = parseStatement();
                    if (elsenode) {addChild(node, elsenode);}
                    break;
                }
                ASTNode* stmt = parseStatement();
                if (stmt) addChild(body, stmt);
                else if (!(hasError)) advance();
            }
            return node;
        }
    }

    if (is_multiple(TOKEN_KEYWORD, 0, "while", NULL)) {
        advance(); // Eat 'while'
        ASTNode* cond = parseExpression();
        if (!cond || hasError) {
            freeAST(cond);
            return NULL;
        }
        ASTNode* node = createNode(NODE_WHILE, "while");
        addChild(node, cond);

        ASTNode* body = createNode(NODE_PROGRAM, "WHILE_BODY");
        addChild(node, body);

        if (is(TOKEN_SEMICOLON, 0)) {advance();}
        while (!is(TOKEN_EOF, 0)) {
            if(hasError) {
                freeAST(node);
                return NULL;
            }
            if (is_multiple(TOKEN_KEYWORD, 0, "end", NULL)) {
                advance(); // Eat 'end'
                break;
            }
            ASTNode* stmt = parseStatement();
            if (stmt) {
                addChild(body, stmt);
            } else if (!(hasError)) {
                advance(); // unstick the parser
            }
        }
        return node;
    }

    if (is_multiple(TOKEN_KEYWORD, 0, "readfile", NULL)) {
        advance(); // Eat 'readfile'
        ASTNode* expr = parseExpression();
        if (!expr || hasError) {
            freeAST(expr);
            return NULL;
        }
        ASTNode* node = createNode(NODE_READFILE, NULL); // NULL until we know the name
        addChild(node, expr);

        // Handle optional 'as var'
        if (is_multiple(TOKEN_KEYWORD, 0, "as", NULL)) {
            advance(); // Eat 'as'
            if (!is(TOKEN_IDENTIFIER, 0)) {
                throw(TOKEN_IDENTIFIER);
                freeAST(node);
                return NULL;
            }
            Token nameToken = peek(0);
            advance(); // Eat the name
            node->value = strdup(nameToken.value);
        }

        ASTNode* body = createNode(NODE_PROGRAM, "READFILE_BODY");
        addChild(node, body);

        if (is(TOKEN_SEMICOLON, 0)) {advance();}
        while (!is(TOKEN_EOF, 0)) {
            if(hasError) {
                freeAST(node);
                return NULL;
            }
            if (is_multiple(TOKEN_KEYWORD, 0, "end", NULL)) {
                advance(); // Eat 'end'
                break;
            }
            ASTNode* stmt = parseStatement();
            if (stmt) {
                addChild(body, stmt);
            } else if (!(hasError)) {
                advance(); // unstick the parser
            }
        }

        return node;
    }

    if (is_multiple(TOKEN_KEYWORD, 0, "createfile", NULL)) {
        advance();
        ASTNode* expr = parsePrimaryExpression(); // PATH
        if (!expr || hasError) {
            freeAST(expr);
            return NULL;
        }
        ASTNode* node = createNode(NODE_CREATEFILE, NULL);
        node->left = expr;
        if (is_multiple(TOKEN_KEYWORD, 0, "as", NULL)) {
            advance();
            if (!is(TOKEN_IDENTIFIER, 0)) {
                throw(TOKEN_IDENTIFIER);
                freeAST(node);
                return NULL;
            }
            Token t = peek(0);
            advance();
            node->value = strdup(t.value);
        }
        return node;
    }

    if (is_multiple(TOKEN_KEYWORD, 0, "overwrite", NULL)) {
        advance();
        ASTNode* expr = parseExpression();
        if (!expr || hasError) {
            freeAST(expr);
            return NULL;
        }
        ASTNode* node = createNode(NODE_FILE_INTERACTION, "overwrite");
        addChild(node, expr);
        return node;
    } else if (is_multiple(TOKEN_KEYWORD, 0, "put", NULL)) {
        advance();
        ASTNode* expr = parseExpression();
        if (!expr || hasError) {
            freeAST(expr);
            return NULL;
        }
        ASTNode* node = createNode(NODE_FILE_INTERACTION, "put");
        addChild(node, expr);
        return node;
    }

    // Handle assignments and expression statements
    ASTNode* expr = parseExpression();
    if (!expr || hasError) return NULL;

    if (is(TOKEN_EQUALS, 0) || is(TOKEN_COMPOUND_ASSIGN, 0)) { // It's an assignment
        if (expr->type != NODE_VARIABLE && expr->type != NODE_INDEX_ACCESS && expr->type != NODE_MEMBER_ACCESS && expr->type != NODE_DEREF) {
            Token errToken = peek(0);
            error_t err = make_error(ERRTY_SyntaxError, "Invalid target for assignment", errToken.ln, errToken.character);
            set_err(&err);
            freeAST(expr);
            return NULL;
        }

        Token opToken = peek(0);
        advance(); // eat '=' or '+=' etc

        ASTNode* rvalue = parseExpression();
        if (!rvalue || hasError) {
            freeAST(expr);
            freeAST(rvalue);
            return NULL;
        }

        if (opToken.type == TOKEN_COMPOUND_ASSIGN) {
            char opChar[2] = { opToken.value[0], '\0' };
            ASTNode* binaryOp = createNode(NODE_OPERATOR, opChar);
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

ASTNode* parseFile(const TokenStream Tokens) {
    if (globalStream == NULL) globalStream = malloc(sizeof(TokenStream));
    if (!globalStream) {
        error_t err = make_error(ERRTY_InternalError, "Couldn't allocate global TokenStream container", 0, 0);
        set_err(&err);
        return NULL;
    }
    *globalStream = Tokens;

    currentIndex = 0;
    ASTNode* root = createNode(NODE_PROGRAM, "ROOT");

    while (!is(TOKEN_EOF, 0)) {
        if (hasError) break;
        if (is(TOKEN_SEMICOLON, 0)) {
            advance();
            continue;
        }

        ASTNode* stmt = parseStatement(); //test

        if (stmt) {
            addChild(root, stmt);
        } else if (!(hasError)) {
            advance();
        }
    }

    // If an error occurred, discard the partial AST
    if (hasError) {
        freeAST(root);
        root = NULL;
    }

    // Cleanup the global parser state
    if (globalStream) {
        free(globalStream);
        globalStream = NULL;
    }

    return root;
}

void freeAST(ASTNode* node) {
    if (node == NULL) return;
    if (node->value) free(node->value);
    if (node->type_notation) free(node->type_notation);
    if (node->type_condition) free(node->type_condition);
    freeAST(node->left);
    freeAST(node->right);
    if (node->children) {
        for (int i = 0; i < node->childCount; i++) {
            freeAST(node->children[i]);
        }
        free(node->children);
    }
    free(node);
}