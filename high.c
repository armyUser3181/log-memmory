
#include "Mallocter.h"
#include "debuger.h"

static const char *FLOW_ERROR = "FLOW_ERROR";
static const char *FLOW_NONE = "FLOW_NONE";
static const char *FLOW_TRUE = "FLOW_TRUE";
static const char *FLOW_FALSE = "FLOW_FALSE";

struct Mallocter {
    uint64_t *space;
    uint64_t size;
};

typedef uint64_t INT;

static inline INT ofLeftBit(INT value) {
    DEBUG_OVERFLOW(value);
    INT index = 0;
    for(int i = 1; i < 7; i++) {
        INT size = 64 >> i;
        INT mask = ~0ULL >> 64 - size;
        INT bcmp = ( value & mask ) == 0;
        INT RV = size * bcmp;
        value >>= RV;
        index += RV;
    }
    return index;
}

static inline INT ofRightBit(INT value) {
    DEBUG_OVERFLOW(value);
    INT index = 0;
    for(int i = 1; i < 7; i++) {
        INT size = 64 >> i;
        INT mask = ~0ULL << 64 - size;
        INT bcmp = ( value & mask ) == 0;
        INT RV = size * bcmp;
        value <<= RV;
        index += RV;
    }
}

static INT toBitRun(INT value, INT size) {
    INT debug_value = value;
    INT debug_size = size;
    for(int i = 0; i < 4; i++) {
        INT play = 1 << i;
        INT bcmp = play < size;
        INT RV = play * bcmp;
        value = (value >> RV) & value;
        size -= RV;
    }
    for(int i = 2; i < 6; i++) {
        INT play = 64 >> i;
        INT bcmp = play < size;
        INT RV = play * bcmp;
        value = (value >> RV) & value;
        size -= RV;
    }
    for(int i = 0; i < 1; i++) {
        INT play = (1);
        INT bcmp = play < size;
        INT RV = play * bcmp;
        value = (value >> RV) & value;
        size -= RV;
    }
    DEBUG_TOBITRUN(value, debug_value, debug_size);
    return value;
}

inline static INT ofMaskPoint64(INT index, INT level) {
    return (1 + index << 6 * level) - (level >> 2) - 1;
}
inline static INT ofMaskPoint8(INT index, INT level) {
    return (1 + index << 3 + 6 * level) - (level << 1);
}
inline static INT ofStartMaskPoint8(INT low, INT level) {
    return (low + (level << 1) >> 3 + 6 * level ) - 1;
}

typedef struct Mallocter *ASRS;

static struct Mallocter *createMallocter() {
    
}



int tast() {
    printf("%lb\n", toBitRun(0b1111, 2) );
    return 0;
}