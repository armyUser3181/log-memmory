
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
    if(!value) return -1;
    if(!~value) return 0;
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
    return index;
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
inline static INT ofSpaceSize(INT size) {
    return size * sizeof( uint64_t ) << 3;
}
inline static INT ofRangeSize64(INT size) {
    return size;
}
inline static INT ofRangeSize8(INT size) {
    return size << 3;
}

typedef struct Mallocter *ARAS;

static struct Mallocter *createMallocter() {
    struct Mallocter *AS = malloc(sizeof(struct Mallocter));
    if( !AS ) {
        DEBUG_EXIST(AS);
        return NULL;
    }
    AS->size = 1 << 6;
    AS->space = malloc( ofSpaceSize(AS->size) );
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
    uint64_t *ptr = allocExtend(AS->space, ofSpaceSize(new_size));
    if(!ptr) return FLOW_ERROR;
    AS->size = new_size;
    AS->space = ptr;
    INT level = ofLevel(AS->size);
    int64_t low = ofStartMaskPoint8(ofRangeSize8(size), level);
    int64_t high = ofStartMaskPoint8(ofRangeSize8(new_size), level);
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
    if(0 != point) DEBUG_OVERFLOW(point);
    INT maskPoint = ofMaskPoint8(point, level);
    INT all = AS->space[maskPoint];
    INT any = AS->space[maskPoint+1];
    INT mask = ~all & ~any;
    mask = toBitRun(mask, size);
    INT index = ofLeftBit(mask);
    return index + ( index == -1 ? 0 : (point << 6) );
}

fn createVirtualTopMask(ARAS AS, INT *result_all, INT *result_any) {
    INT level = ofLevel(AS->size);
    INT high = ofStartMaskPoint8( ofRangeSize8(AS->size) , level);
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



static INT miniFindMemory(ARAS AS, INT size, INT level, INT point) {
    INT currentLevel = ofLevel(size);
    INT all = AS->space[ofMaskPoint8(point, level)];
    INT any = AS->space[ofMaskPoint8(point, level) + 1];
    if( currentLevel + 1 == level ) {
        uint64_t value = ASFindBitRun(AS, level, point, size);
        printf("hello world %ld, %ld, %ld, %ld\n", level, point, size, value);
        return value;
    } else {
        //printf("<%ld, %ld, %ld, %ld>\n", level, point, all, any);
        for(uint64_t index; index = ofLeftBit(~all), index != -1; all |= 1 << index) {
            //AS->space[ofMaskPoint8(index, level)] = ~all & ~any & 1 << index ? 0 : AS->space[ofMaskPoint8(index, level)];
            printf("miniFindMemory %ld, %ld, %ld, %ld, %ld\n", level, point, index, size, all);
            uint64_t value = miniFindMemory(AS, size, level - 1, index + point << 6);
            if(value != -1) return value;
        }
    }
    return -1;
}

static INT FindMemory(ARAS AS, INT size) {
    INT currentLevel = ofLevel(size);
    INT spaceLevel = ofLevel(AS->size);
    INT workLevel = spaceLevel;
    INT spaceSize = 1 << 6 * spaceLevel + 3;
    INT workRenge = ( size - 1 >> 6 * currentLevel ) + 1;
    INT workMask_all = 0;
    INT workMask_any = 0;
    createVirtualTopMask(AS, &workMask_all, &workMask_any);
    //printf("ture %d \n", __LINE__);
    uint64_t index = ofLeftBit(~workMask_all);
    for( ; index = ofLeftBit(~workMask_all), index != -1; workMask_all |= 1 << index ) {
        //AS->space[ofMaskPoint8(index, workLevel)] = ~workMask_all & ~workMask_any & 1 << index ? 0 : AS->space[ofMaskPoint8(index, workLevel)];
        uint64_t value = miniFindMemory(AS, size, workLevel, index);
        //printf("all %ld, any %ld, level %ld, index %ld, size %ld, value %ld\n", workMask_all, workMask_any, workLevel, index, size, value);
        if(value != -1) return value;
    }
    return -1;
}

fn FillMask(ARAS AS, INT point, INT level, uint32_t index, uint32_t size) {
    point = ofMaskPoint8(point, level);
    INT mask = ~(~0ULL << size) << index;
    AS->space[point] |= mask;
    return FLOW_NONE;
}

fn upLevelingMask(ARAS AS, INT index, INT level) {
    struct {
        INT m10, m01, p, i;
    } child, parent, current;
    child.i = index;
    child.p = ofMaskPoint8(child.i, level);
    int overcmp = ofRangeSize8(AS->size) <= child.p;
    if(overcmp) {
        printf("upLevelingMask overcmp range %ld, size %ld, p %ld, level %ld, i %ld\n", ofRangeSize8(AS->size), AS->size, child.p, level, index);
    }
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
        
        current.m01 = (0ULL != child.m01 ? 1 : 0);
        current.m10 = (~0ULL == child.m10 ? 1 : 0);
        INT indexShift = 1 << child.i;
        current.m01 = current.m01 << child.i;
        current.m10 = current.m10 << child.i;

        parent.m01 = ~indexShift;
        parent.m10 = ~indexShift;
        parent.m01 |= current.m01;
        parent.m10 += current.m10;

        AS->space[parent.p + 1] = parent.m01;
        AS->space[parent.p] = parent.m10;
        child = parent;
    }
    return FLOW_NONE;
}

static void* getMemory(ARAS AS, INT size) {
    INT hendle = FindMemory(AS, size);
    //printf("<hendle %ld>\n", hendle);
    if(hendle == -1) return NULL;
    INT level = ofLevel(size);
    INT power = level * 6;
    INT range = 64 << power;
    INT element = 1 << power;
    INT point = hendle >> (power + 6);
    INT index = hendle - (point << power + 6);
    hendle == -1 ? 0 : FillMask(AS, point, level + 1, index, size);
    //printf("<%ld, %ld, %ld, %ld>\n", level, point, index, size);
    upLevelingMask(AS, hendle == -1 ? 0 : point, level + 1);
    return (void*)(hendle + (INT)AS->space);
}

fn freeMemory(ARAS AS, void* ptr, INT size) {
    INT hendle = (uint64_t*)ptr - AS->space;
    INT level = ofLevel(size);
    INT power = level * 6;
    INT range = 64 << power;
    INT element = 1 << power;
    INT point = hendle >> power + 6;
    INT index = hendle - (point << power + 6);
    INT mask = ~( ~0ULL << size ) << index;
    AS->space[ofMaskPoint8(point, level + 1)] &= ~mask;
    upLevelingMask(AS, point, level + 1);
    return FLOW_NONE;
}

fn miniPrintMallocter(ARAS AS, INT point, INT level) {
    int64_t high = ofStartMaskPoint8(point, level - 1) + 1;
    int64_t low_point = ofStartMaskPoint8( point, level ) << 3 << level * 6;
    int64_t low = ofStartMaskPoint8( low_point, level - 1) + 1;
    /**
     * ofStartMaskPoint8( ofMaskPoint8(i, level), level ) << 3 << level * 6
     * ofStartMaskPoint8( , child_level) + 1;
     */
    if(1 < level) for(int64_t i = low; i < high; i++ ) {
        miniPrintMallocter( AS, ofMaskPoint8(i, level - 1), level - 1);
    } else {
        low = 0;
        high = 0;
    }
    if(0 < level) {
        printf("<(level: %ld, point: %ld, low: %ld, high: %ld)> \n", level, point, low, high);
        printf("<%lb, %lb> \n", AS->space[point], AS->space[point + 1]);
    }
    return FLOW_NONE;
}

fn printMallocter(ARAS AS) {
    INT level = ofLevel(AS->size);
    int64_t low = 0;
    int64_t high = ofStartMaskPoint8( ofRangeSize8(AS->size), level) + 1;
    printf(" <space %p size %ld> \n", AS->space, AS->size);
    for(int64_t i = low; i < high; i++) {
        miniPrintMallocter( AS, ofMaskPoint8(i, level), level );
    }
}

extern int tast(ARAS AS) {
    AS->space[ofMaskPoint8(0, 1)] = ~0b0000000000000000000000000000000000000000000000000000000000111111;
    //upLevelingMask(AS, 0, 1);
    //printMallocter(AS);
    for(int i = 0; i < 1; i++) {
        INT result = (INT)getMemory(AS, 33);
        INT offset = (INT)AS->space;
        INT hendle = result ? result - offset : -1;
        printf("<result %ld>\n", hendle);
    }
    //FillMask(AS, 0, 1, 0, 16);
    printMallocter(AS);
    return 0;
}

extern struct MallocterManager createMallocterManager() {
    struct MallocterManager self = { 0 };
    self.create = createMallocter;
    self.destroy = destroyMallocter;
    self.extend = ExtendSpace;
    self.find = FindMemory;
    self.take = getMemory;
    self.free = freeMemory;
    return self;
}