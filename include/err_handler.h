//
// Created by brwolf on 10/30/26.
//

#ifndef MEANINGFUL_ERR_HANDLER_H
#define MEANINGFUL_ERR_HANDLER_H
#define hasError GLOBAL_ERROR.type != ERRTY_Clean
#define checkAndHalt if (hasError) halt();

typedef enum error_type_enum {
    ERRTY_Clean=0,
    ERRTY_SyntaxError,
    ERRTY_InternalError,
} errty_t;

typedef struct error_struct {
    errty_t type;
    char* message;
    unsigned int line;
    unsigned int column;
} error_t;

extern error_t GLOBAL_ERROR;

error_t make_error(errty_t ty, const char* msg, unsigned int ln, unsigned int col);
void set_err(const error_t* err);
void halt();

#endif //MEANINGFUL_ERR_HANDLER_H
