
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#define bugp printf("<bug: FILE: %s LINE: %d NAME: %s>", __FILE__, __LINE__, __func__)

typedef const char* FLOW;

FLOW FLOW_ERROR = "FLOE_ERROR";
FLOW FLOW_NONE = "FLOW_NONE";
FLOW FLOW_TRUE = "FLOW_TRUE";
FLOW FLOW_FALSE = "FLOW_FALSE";

struct AS {
    uint64_t* ptr;
    uint64_t ptr_size;
    uint64_t level;
    FLOW message;
};

typedef FLOW FN;
typedef uint64_t INT;

INT ofLeftBit(INT value) {
    if(!value) return -1;
    INT index = 0;
    for(int i = 1; i < 7; i++) {
        INT size = 64 >> i;
        INT mask = ( (~0ULL) >> (64 - size) );
        INT bcmp = (value & mask) == 0;
        INT RV = size * bcmp;
        value >>= RV;
        index += RV;
    }
    return index;
}

INT ofRightBit(INT value) {
    if(!value) return -1;
    INT index = 0;
    for(int i = 1; i < 7; i++) {
        INT size = 64 >> i;
        INT mask = ( (~0ULL) << (64 - size) );
        INT bcmp = (value & mask) == 0;
        INT RV = size * bcmp;
        value <<= RV;
        index += RV;
    }
    return index;
}

INT toContiBit(INT value, INT size) {
    for(int i = 0; i < 7; i++) {
        INT play = (64 >> i);
        INT bcmp = play < size;
        INT RV = play * bcmp;
        value = (value >> RV) & value;
        size -= RV;
    }
    return value;
}

inline static INT ofMaskPoint64(INT index, INT level) {
    //return (1 + index) * ( 1 << ( 6 * level ) ) - level;
    return (1 + index << 6 * level ) - (level >> 2) - 1;
}

inline static INT ofMaskPoint8(INT index, INT level) {
    //return ( (1 + index << 6 * level ) << 3 ) - ( level << 1 );
    return (1 + index << 3 + 6 * level ) - ( level << 1 );
}

inline static INT ofStartMaskPoint8(INT low, INT level) {
    return (low + ( level << 1 ) >> 3 + 6 * level) - 1;
}

typedef struct AS * const ARAS;

struct AS * createAS() {
    ARAS AS = malloc( sizeof(struct AS) );
    if(!AS) return NULL;
    AS->level = 1;
    AS->ptr_size = 1 << 6;
    AS->ptr = malloc( sizeof( uint64_t ) * AS->ptr_size << 3 );
    if(!AS->ptr) {
        free(AS);
        return NULL;
    }
    AS->ptr[ofMaskPoint8(0, 1)] = 0;
    AS->ptr[ofMaskPoint8(0, 1) + 1] = 0;
    return AS;
}

struct AS * destroyAS(ARAS AS) {
    free(AS->ptr); free(AS);
    return NULL;
}

FN ASExtendSpace(ARAS AS) {
    uint64_t size = AS->ptr_size;
    AS->ptr_size <<= 2;
    uint64_t * ptr = realloc(AS->ptr, AS->ptr_size * sizeof( uint64_t ) << 3 );
    if(!ptr) return FLOW_ERROR;
    AS->ptr = ptr;
    INT low = ofStartMaskPoint8(size, AS->level), high = ofStartMaskPoint8(AS->ptr_size, AS->level);
    for(int i = low + 1; i <= high; i++) {
        INT point = ofMaskPoint8(i, AS->level);
        AS->ptr[point] = 0;
        AS->ptr[point + 1] = 0;
    }
    return FLOW_NONE;
}

FN ASExtendLevel(ARAS AS) {
    AS->level += 1;
    INT low = ofStartMaskPoint8(0, AS->level), high = ofStartMaskPoint8(AS->ptr_size, AS->level);
    for(int i = low + 1; i <= high; i++) {
        INT point = ofMaskPoint8(i, AS->level);
        AS->ptr[point] = 0;
        AS->ptr[point + 1] = 0;
    }
    return FLOW_NONE;
}

FN ASofMemoryPoint(ARAS AS, INT size) {
    INT point = 0;
    INT sizeToLevel = ofRightBit(size);
    int i = AS->level - 1;
    for(i; sizeToLevel <= i; i--) {
        point = ofLeftBit( ~AS->ptr[ofMaskPoint8(point, i)] );
    }
    point = ofLeftBit( ~AS->ptr[ofMaskPoint8(point, i)] ); // 중단점
}

int main(int argc, char *argv[]) {
    INT NULLINT = 0b0000000000000000000000000000000000000000000000000000000000000000;
    INT FULLINT = 0b1111111111111111111111111111111111111111111111111111111111111111;
    INT TASTINT = 0b0000000000000000000000000000000011111111111111111111111111111111;
    printf("<%ld %ld>", ofMaskPoint8(0, 1), ofStartMaskPoint8(510, 1) );
    printf("<");
    for(INT i = ofStartMaskPoint8(ofMaskPoint8(0, 1) + 300, 1); i <= ofStartMaskPoint8(ofMaskPoint8(1, 1), 1); i++ ) {
        printf("%ld", i);
    }
    printf(">\n");
    struct AS * AS = createAS();
    
    AS = destroyAS(AS);
    return 0;
}