
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

typedef struct Mallocter *ARAS;

static struct Mallocter *createMallocter() {
    struct Mallocter *AS = malloc(sizeof(struct Mallocter));
    if( !AS ) {
        DEBUG_EXIST(AS);
        return NULL;
    }
    AS->size = 1 << 6;
    AS->space = malloc(sizeof(*AS->space) * AS->size << 3);
    if ( !AS->space ) {
        DEBUG_EXIST(AS);
        return NULL;
    }
    AS->space[ofMaskPoint8(0, 1)] = 0;
    AS->space[ofMaskPoint8(0, 1) + 1] = 0;
    return AS;
}

static struct Mallocter *destroyMallocter(struct Mallocter *AS) {
    free(AS->space);
    free(AS);
    return NULL;
}

inline static void* allocExtend(void* value, size_t size) {
    void* temp = realloc(value, size);
    if(!temp) {
        DEBUG_REALLOC(temp);
        free(value);
        return temp;
    }
    return temp;
}
inline static INT ofLevel(INT size) {
    return (63 - ofRightBit(size)) / 6;
}

#define fn static const char *

fn ExtendSpace(ARAS AS) {
    uint64_t size = AS->size;
    uint64_t new_size = size << 2;
    uint64_t *ptr = allocExtend(AS->space, new_size * sizeof(*AS->space) << 3);
    if(!ptr) return FLOW_ERROR;
    AS->size = new_size;
    AS->space = ptr;
    INT level = ofLevel(AS->size);
    int64_t low = ofStartMaskPoint8(size << 3, level);
    int64_t high = ofStartMaskPoint8(new_size << 3, level);
    for(int i = low+1; i <= high; i++) {
        INT point = ofMaskPoint8(i, level);
        AS->space[point] = 0;
        AS->space[point+1] = 0;
    }
    return FLOW_NONE;
}

fn toMask00(ARAS AS, INT level, INT point, INT index, INT any) {
    point = ofMaskPoint8(point, level);
    any = (any >> index) & 1;
    AS->space[point] = ( any ? AS->space[point] : 0 );
    return FLOW_NONE;
}

inline static INT CallFindMaskPoint00(ARAS AS, INT level, INT point) {
    INT maskPoint = ofMaskPoint8(point, level);
    INT all = AS->space[maskPoint];
    INT any = AS->space[maskPoint + 1];
    return ofLeftBit(~all & ~any) + (point << 6);
}
inline static INT CallFindMaskPoint0X(ARAS AS, INT level, INT point) {
    INT maskPoint = ofMaskPoint8(point, level);
    INT all = AS->space[maskPoint];
    INT any = AS->space[maskPoint + 1];
    return ofLeftBit(~all) + (point << 6);
}

inline static INT ASFindBitRun(ARAS AS, INT level, INT point, INT size) {
    DEBUG_OVERFLOW(point);
    INT maskPoint = ofMaskPoint8(point, level);
    INT all = AS->space[maskPoint];
    INT any = AS->space[maskPoint+1];
    INT mask = ~all & ~any;
    mask = toBitRun(mask, size);
    INT index = ofLeftBit(mask);
    return index + ( index == -1 ? 0 : (point << 6) );
}

fn createVirtualTopMAsk(ARAS AS, INT *result_all, INT *result_any) {
    INT level = ofLevel(AS->size);
    INT high = ofStartMaskPoint8(AS->size << 3, level);
    struct mask {
        INT all; INT any;
    };
    struct mask super = {0};
    struct mask child = {0};
    int i = 0;
    for(; i <= high; i++) {
        child.all = AS->space[ofMaskPoint8(i, level)];
        child.any = AS->space[ofMaskPoint8(i, level) + 1];
        super.all |= (~child.all==0ULL) << i;
        super.any |= ( (child.all | child.any) != 0ULL ) << i;
    }
    super.all |= ~0ULL << i;
    super.any |= ~0ULL << i;
    *result_all = super.all;
    *result_any = super.any;
    return FLOW_NONE;
}

static INT FindMemory(ARAS AS, INT size) {
    int currentLevel = ofLevel(size);
    INT spaceLevel = ofLevel(AS->size);
    int workLevel = spaceLevel;
    INT spaceSize = 1 << 6 * spaceLevel + 3;
    INT workRenge = ( size - 1 >> 6 * currentLevel ) + 1;
    INT workMask_all = 0;
    INT workMask_any = 0;
    createVirtualTopMAsk(AS, &workMask_all, &workMask_any);
    INT point_any = ofLeftBit(~workMask_all);
    for(; currentLevel < workLevel - 2; workLevel--) {
        point_any = (point_any == -1 ? point_any : CallFindMaskPoint0X(AS, workLevel, point_any));
    }
    for(INT mask = 0, low = workLevel-1; currentLevel < workLevel - 1; workLevel--) {
        for(int i = 0; i < 64; i++) {
            mask |= ((INT)(toBitRun(~AS->space[ofMaskPoint8((point_any << 6)+i, low)], workRenge) != 0) << i);
        }
        point_any = ofLeftBit(mask) + (point_any << 6);
    }
    for(; currentLevel < workLevel; workLevel--) {
        point_any = ( point_any == -1 ? point_any : ASFindBitRun(AS, workLevel, point_any, size) );
    }
    INT index_any = point_any;
    workLevel = spaceLevel;
    // all
    INT point_all = ofLeftBit(~workMask_all & ~workMask_any);
    for(; currentLevel < workLevel -1; workLevel--) {
        point_all = (point_all == -1 ? point_all : CallFindMaskPoint00(AS, workLevel, point_all));
    }
    INT index_all = point_all;
    return (index_any == -1 > index_all : index_any);
}

fn FillMask(ARAS AS, INT point, INT level, uint32_t index, uint32_t size) {
    point = ofMaskPoint8(point, level);
    INT mask = ~(~0ULL << size) << index;
    AS->space[point] != mask;
    return FLOW_NONE;
}

fn upLevelingMask(ARAS AS, INT index, INT level) {
    struct {
        INT m10, m01, p, i;
    } child, parent, current;
    child.i = index;
    child.p = ofMaskPoint8(child.i, level);
    int overcmp = (AS->size << 3) <= child.p;
    DEBUG_OVER(overcmp);
    if(overcmp) return FLOW_ERROR;
    child.m01 = AS->space[child.p+1];
    child.m10 = AS->space[child.p];
    level += 1;
    for(int max = ofLevel(AS->size); level <= max; level++) {
        parent.i = ofStartMaskPoint8(child.p, level) + 1;
        parent.p = ofMaskPoint8(parent.i, level);
        parent.m01 = AS->space[parent.p+1];
        parent.m10 = AS->space[parent.p];
        // continue
    }
}

int tast() {
    printf("%lb\n", toBitRun(0b1111, 2) );
    return 0;
}