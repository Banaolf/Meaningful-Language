//
// Created by brwolf on 10/30/26.
//
#include "err_handler.h"

#define gerr GLOBAL_ERROR
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

error_t make_error(const errty_t ty, const char* msg, const unsigned int ln, const unsigned int col) {
    error_t err;
    err.type = ty;
    err.message = strdup(msg);
    err.line = ln;
    err.column = col;
    return err;
}

error_t gerr = {.type = ERRTY_Clean, .message = NULL, .line = 0, .column = 0};

void set_err(const error_t* err) {
    gerr.type = err->type;
    free(gerr.message); gerr.message = (err->message == NULL) ? NULL : strdup(err->message);
    gerr.line = err->line;
    gerr.column = err->column;
}

static char* err_type_to_str(const errty_t type) {
    switch (type) {
        case ERRTY_Clean: return "Clean";
        case ERRTY_SyntaxError: return "SyntaxError";
        case ERRTY_InternalError: return "InternalError";
    }
    return "Unknown";
}

void halt() {
    if (hasError)
        fprintf(stderr, "[%s]: %s | Line %d col %d\n", err_type_to_str(gerr.type), gerr.message, gerr.line, gerr.column);
    free(gerr.message);
    exit(gerr.type);
}