
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define DEBUGER_DEBUG_TRUE 1



#if DEBUGER_DEBUG_TRUE

static const char *__ERROR_FALSE__ = "FALSE";
#define FALSE __ERROR_FALSE__
#define fn static inline void

#define FIRST_MAG "ERROR"
#define CDA FIRST_MAG, __FILE__, __LINE__
#define CFA const char* message, const char* file, size_t line
#define CIA message, file, line
#define CALLS(extends) calls(CIA, extends)

#define DEBUG_OVERFLOW(value) debug_overflow(CDA, value)
#define DEBUG_TOBITRUN(result, value, len) debug_toBitRun(CDA, result, value, len)
#define DEBUG_EXIST(value) debug_exist(CDA, value)
#define DEBUG_REALLOC(value) debug_exist(CDA, value)
#define DEBUG_OVER(value) debug_over(CDA, value)

static inline void calls(CFA, const char *extends) {
    if( extends == __ERROR_FALSE__ ) {
        // error none
    } else {
        fprintf(stderr, "(%s:%ld): %s\n %s\n", file, line, message, extends);
        abort();
    }
}

fn debug_overflow(CFA, uint64_t value) {
    int exp = ( !value || !~value );
    CALLS(exp ? "OVERFLOW" : FALSE );
}

fn debug_toBitRun(CFA, uint64_t result, uint64_t value, uint64_t size) {
    uint64_t debug_value = value;
    uint64_t debug_size = size;
    for (int i = 1; i < size; i++)
    {
        value = ((value >> 1) & value);
    }
    char buffer[1024] = {0};
    sprintf(buffer, "BitRun ERROR VALUE: (value: %ld, size: %ld) \n (high: %ld) (low: %ld)", debug_value, debug_size, result, value);
    CALLS( result == value ? FALSE : buffer);
}

fn debug_exist(CFA, void* value) {
    CALLS(value == NULL ? "NULL ERROR" : FALSE);
}

fn debug_realloc(CFA, void* value) {
    CALLS(value == NULL ? "REALLOC ERROR" : FALSE);
}

fn debug_over(CFA, int value) {
    CALLS(value ? "VALUE OVER" : FALSE);
}

#undef fn

#else

#define DEBUG_OVERFLOW ;



#endif