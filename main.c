
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>

#define bugp printf("<bug: FILE: %s LINE: %d NAME: %s>", __FILE__, __LINE__, __func__)

typedef const char* FLOW;

FLOW FLOW_ERROR = "FLOE_ERROR";
FLOW FLOW_NONE = "FLOW_NONE";
FLOW FLOW_TRUE = "FLOW_TRUE";
FLOW FLOW_FALSE = "FLOW_FALSE";

struct AS {
    uint64_t* ptr;
    uint64_t ptr_size;
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

    for(int i = 0; i < 5; i++) {
        INT play = (1 << i);
        INT bcmp = play < size;
        INT RV = play * bcmp;
        value = ((value >> RV) & value);
        size -= RV;
        //printf("%ld %ld %ld\t%lb\n", size, play, bcmp, value);
    }

    for(int i = 4; i < 6; i++) {
        INT play = (64 >> i);
        INT bcmp = play < size;
        INT RV = play * bcmp;
        value = ((value >> RV) & value);
        size -= RV;
        //printf("%ld %ld %ld\t%lb\n", size, play, bcmp, value);
    }

    for(int i = 0; i < 1; i++) {
        INT play = (1);
        INT bcmp = play < size;
        INT RV = play * bcmp;
        value = ((value >> RV) & value);
        size -= RV;
        //printf("%ld %ld %ld\t%lb\n", size, play, bcmp, value);
    }

    /* for(int i = 1; i < size; i++) {
        value = ((value >> 1) & value);
    } */
    
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

inline static INT ASofLevel(INT size) {
    return (63 - ofRightBit(size)) / 6;
}

FN ASExtendSpace(ARAS AS) {
    uint64_t size = AS->ptr_size;
    AS->ptr_size <<= 2;
    uint64_t * ptr = realloc(AS->ptr, AS->ptr_size * sizeof( uint64_t ) << 3 );
    if(!ptr) return FLOW_ERROR;
    AS->ptr = ptr;
    INT level = ASofLevel(AS->ptr_size);
    int64_t low = ofStartMaskPoint8(size << 3, level), high = ofStartMaskPoint8(AS->ptr_size << 3, level);
    //printf("<<level: %ld>>\n", level);
    for(int i = low + 1; i <= high; i++) {
        INT point = ofMaskPoint8(i, level);
        AS->ptr[point] = 0;
        AS->ptr[point + 1] = 0;
    }
    return FLOW_NONE;
}

int ASofMemoryPoint(ARAS AS, INT arg_size) {
    INT point = 0;
    INT sizeToLevel = ASofLevel(arg_size);
    INT level = ASofLevel(AS->ptr_size);
    int i = level;
    INT space_size = 1 << 6 * level + 3;
    INT size = (arg_size - 1 >> 6 * sizeToLevel) + 1;
    for(i; sizeToLevel < i - 1; i--) {
        INT thisMaskAll = AS->ptr[ ofMaskPoint8(point, i) ];
        INT thisMaskAny = AS->ptr[ ofMaskPoint8(point, i) + 1 ];
        point = ofLeftBit( ~thisMaskAll ) + (point << 6); // 중단점
    }
    INT index = ofLeftBit( toContiBit(~AS->ptr[ofMaskPoint8(point, i)], size) );
    
    return index;
    //point = ofLeftBit( ~AS->ptr[ofMaskPoint8(point, i)] ); // 중단점
}

clock_t ofTimeTast(ARAS AS) {
    volatile const char* rs = FLOW_NONE;
    //volatile void* code[1<<10] = {0};
    srand(time(NULL)); volatile int rands[1<<10] = {0}; for(int i = 0; i < 1 << 10; i++) rands[i] = rand(); clock_t t = clock();
    for(int i = 0; i < 1 << 10; i++) {
        rands[i] = ASofMemoryPoint(AS, 32);
    }
    return clock() - t;
}

int main(int argc, char *argv[]) {
    INT NULLINT = 0b0000000000000000000000000000000000000000000000000000000000000000;
    INT FULLINT = 0b1111111111111111111111111111111111111111111111111111111111111111;
    INT TASTINT = 0b0000000000000000000000000000000011111111111111111111111111111111;
    //printf("<len: %ld>", strlen("100000000000000000000000000000000"));
    /* int l = -2;
    for(int i = 0; i < 1 << 16; i++) {
        int r = ofStartMaskPoint8(i, 1);
        if(l != r) {
            l = r;
            printf("<%d>", i);
        }
    }
    for(int i = 0; i < 12; i++) {
        printf("<%ld>", ofMaskPoint8(i, 1));
    } */
    struct AS * AS = createAS(); 
    for(int i = 0; i < 10; i++) {
        ASExtendSpace(AS);
    }
    AS->ptr[ofMaskPoint8(0, 4)] = 0b11;
    AS->ptr[ofMaskPoint8(2, 3)] = 0b111;
    AS->ptr[ofMaskPoint8(131, 2)] = 0b01;
    AS->ptr[ofMaskPoint8(131*64+1, 1)] = 0b000000000000000000000000000000000000000000000000100001000010001;
    //ASofMemoryPoint(AS, 32);
    printf("<time: %lf>\n", (double)ofTimeTast(AS) / (1<<10) );
    AS = destroyAS(AS);
    return 0;
}