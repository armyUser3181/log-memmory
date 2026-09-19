
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>

#define bugp printf("<bug: FILE: %s LINE: %d NAME: %s>", __FILE__, __LINE__, __func__)
#define DEBUG_LOG(fmt, ...)                                                                          \
    do                                                                                               \
    {                                                                                                \
        fprintf(stderr, "[DEBUG] %s:%d %s: " fmt "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__); \
    } while (0)

typedef const char *FLOW;

FLOW FLOW_ERROR = "FLOE_ERROR";
FLOW FLOW_NONE = "FLOW_NONE";
FLOW FLOW_TRUE = "FLOW_TRUE";
FLOW FLOW_FALSE = "FLOW_FALSE";

struct AS
{
    uint64_t *ptr;
    uint64_t ptr_size;
};

typedef FLOW FN;
typedef uint64_t INT;

static INT ofLeftBit(INT value)
{
    if (!value)
        return -1;
    INT index = 0;
    for (int i = 1; i < 7; i++)
    {
        INT size = 64 >> i;
        INT mask = ((~0ULL) >> (64 - size));
        INT bcmp = (value & mask) == 0;
        INT RV = size * bcmp;
        value >>= RV;
        index += RV;
    }
    return index;
}

static INT ofRightBit(INT value)
{
    if (!value)
        return -1;
    INT index = 0;
    for (int i = 1; i < 7; i++)
    {
        INT size = 64 >> i;
        INT mask = ((~0ULL) << (64 - size));
        INT bcmp = (value & mask) == 0;
        INT RV = size * bcmp;
        value <<= RV;
        index += RV;
    }
    return index;
}

static INT toContiBit(INT value, INT size)
{

    for (int i = 0; i < 4; i++)
    {
        INT play = (1 << i);
        INT bcmp = play < size;
        INT RV = play * bcmp;
        value = ((value >> RV) & value);
        size -= RV;
        // printf("%ld %ld %ld\t%lb\n", size, play, bcmp, value);
    }

    for (int i = 2; i < 6; i++)
    {
        INT play = (64 >> i);
        INT bcmp = play < size;
        INT RV = play * bcmp;
        value = ((value >> RV) & value);
        size -= RV;
    }

    for (int i = 0; i < 1; i++)
    {
        INT play = (1);
        INT bcmp = play < size;
        INT RV = play * bcmp;
        value = ((value >> RV) & value);
        size -= RV;
    }

    /* for(int i = 1; i < size; i++) {
        value = ((value >> 1) & value);
    } */

    return value;
}

static INT toContiBitLow(INT value, INT size)
{
    for (int i = 1; i < size; i++)
    {
        value = ((value >> 1) & value);
    }
    return value;
}

inline static INT ofMaskPoint64(INT index, INT level)
{
    // return (1 + index) * ( 1 << ( 6 * level ) ) - level;
    return (1 + index << 6 * level) - (level >> 2) - 1;
}

inline static INT ofMaskPoint8(INT index, INT level)
{
    // return ( (1 + index << 6 * level ) << 3 ) - ( level << 1 );
    return (1 + index << 3 + 6 * level) - (level << 1);
}

inline static INT ofStartMaskPoint8(INT low, INT level)
{
    return (low + (level << 1) >> 3 + 6 * level) - 1;
}

typedef struct AS *const ARAS;

static struct AS *createAS()
{
    ARAS AS = malloc(sizeof(struct AS));
    if (!AS)
        return NULL;
    AS->ptr_size = 1 << 6;
    AS->ptr = malloc(sizeof(uint64_t) * AS->ptr_size << 3);
    if (!AS->ptr)
    {
        free(AS);
        return NULL;
    }
    AS->ptr[ofMaskPoint8(0, 1)] = 0;
    AS->ptr[ofMaskPoint8(0, 1) + 1] = 0;
    return AS;
}

static struct AS *destroyAS(ARAS AS)
{
    free(AS->ptr);
    free(AS);
    return NULL;
}

inline static INT ofLevel(INT size)
{
    return (63 - ofRightBit(size)) / 6;
}

static FN ExtendSpace(ARAS AS)
{
    uint64_t size = AS->ptr_size;
    AS->ptr_size <<= 2;
    uint64_t *ptr = realloc(AS->ptr, AS->ptr_size * sizeof(uint64_t) << 3);
    if (!ptr)
        return FLOW_ERROR;
    AS->ptr = ptr;
    INT level = ofLevel(AS->ptr_size);
    int64_t low = ofStartMaskPoint8(size << 3, level), high = ofStartMaskPoint8(AS->ptr_size << 3, level);
    // printf("<<level: %ld>>\n", level);
    for (int i = low + 1; i <= high; i++)
    {
        INT point = ofMaskPoint8(i, level);
        AS->ptr[point] = 0;
        AS->ptr[point + 1] = 0;
    }
    return FLOW_NONE;
}

inline static FN toMask00(ARAS AS, INT level, INT point, INT index, INT any) // 중단점 // 테스트
{
    point = ofMaskPoint8(point, level);
    any = (any >> index) & 1;
    AS->ptr[point] = ( any ? AS->ptr[point] : 0 );
    return FLOW_NONE;
}

inline static INT CallFindMaskPoint00(ARAS AS, INT level, INT point)
{
    INT maskPoint = ofMaskPoint8(point, level);
    INT all = AS->ptr[maskPoint];
    INT any = AS->ptr[maskPoint + 1];
    return ofLeftBit(~all & ~any) + (point << 6);
}

inline static INT CallFindMaskPoint0X(ARAS AS, INT level, INT point)
{
    INT maskPoint = ofMaskPoint8(point, level);
    INT all = AS->ptr[maskPoint];
    INT any = AS->ptr[maskPoint + 1];
    return ofLeftBit(~all) + (point << 6);
}

inline static INT CallFindMaskContiPoint(ARAS AS, INT level, INT point, INT size) {
    INT maskPoint = ofMaskPoint8(point, level);
    INT all = AS->ptr[maskPoint];
    INT any = AS->ptr[maskPoint+1];
    INT mask = ~all & ~any;
    mask = toContiBit(mask, size);
    return ofLeftBit(mask) + (point << 6);
}

static INT FindMemory(ARAS AS, INT arg_size)
{
    int currentLevel = ofLevel(arg_size);
    INT spaceLevel = ofLevel(AS->ptr_size);
    int workLevel = spaceLevel;
    INT spaceSize = 1 << 6 * spaceLevel + 3;
    INT workRange = (arg_size - 1 >> 6 * currentLevel) + 1;

    // any 계산
    INT point_any = 0; // ofLeftBit( toContiBit(~AS->ptr[ofMaskPoint8(0, level)], size) );
    for (; currentLevel < workLevel - 2; workLevel--) point_any = (point_any == -1 ? point_any : CallFindMaskPoint0X(AS, workLevel, point_any));
    for (INT mask = 0, low = workLevel-1; currentLevel < workLevel - 1; workLevel--) {
        for (int i = 0; i < 64; i++) mask |= ((INT)(toContiBit((~AS->ptr[ofMaskPoint8((point_any << 6) + i, low)]), workRange) != 0) << i);
        point_any = ofLeftBit(mask) + (point_any << 6);
    }
    for (; currentLevel < workLevel; workLevel--) {
        point_any = CallFindMaskContiPoint(AS, workLevel, point_any, arg_size);
    }
    INT index_any = point_any;
    workLevel = spaceLevel;

    // all 계산
    INT point_all = CallFindMaskPoint0X(AS, spaceLevel, 0);
    for (; currentLevel < workLevel - 1; workLevel--) point_all = (point_all == -1 ? point_all : CallFindMaskPoint00(AS, workLevel, point_all));
    for (; currentLevel < workLevel; workLevel--) {
        printf("%ld %ld", ofMaskPoint8(point_all, workLevel), AS->ptr_size << 3);
        //point_all = CallFindMaskContiPoint(AS, workLevel, point_all, arg_size);
        return 0;
    }

    // debug code
    if (1 << 24 < AS->ptr_size) {
        printf("<s: %ld %d %ld>", spaceLevel, currentLevel, point_any);
        // printf("<0: %ld, 1: %ld>", AS->ptr[ofMaskPoint8(0, 1)], AS->ptr[ofMaskPoint8(1, 1)]);
        // assert( 0 && "max size" );
        return AS->ptr_size;
    }
    // printf("<t: %ld %ld %ld>", index_any, point_all, point_any);

    INT return_index = (index_any == -1 ? point_all : index_any);
    if (point_all == -1) ExtendSpace(AS);
    return point_all == -1 ? FindMemory(AS, arg_size) : return_index;
}

inline static FN FillMask(ARAS AS, INT point, INT level, uint32_t index, uint32_t size)
{
    point = ofMaskPoint8(point, level);
    INT mask = ~(~0ULL << size) << index;
    AS->ptr[point] |= mask;
    return FLOW_NONE;
}

typedef struct
{
    INT mask10;
    INT mask01;
    INT point;
    INT index;
} MASK;

static FN upLevelingMask(ARAS AS, INT index, INT level)
{
    MASK child, parent, current;
    child.index = index;
    child.point = ofMaskPoint8(child.index, level);
    child.mask01 = AS->ptr[child.point + 1];
    child.mask10 = AS->ptr[child.point + 0];
    /* printf("[up:start] index=%llu level=%llu child.point=%llu child.mask10=0x%016llx child.mask01=0x%016llx\n",
           (unsigned long long)index,
           (unsigned long long)level,
           (unsigned long long)child.point,
           (unsigned long long)child.mask10,
           (unsigned long long)child.mask01); */
    level += 1;

    for (int max = ofLevel(AS->ptr_size); level <= max; level++)
    {
        parent.index = ofStartMaskPoint8(child.point, level) + 1;
        parent.point = ofMaskPoint8(parent.index, level);
        parent.mask01 = AS->ptr[parent.point + 1];
        parent.mask10 = AS->ptr[parent.point];
        current.mask01 = (0ULL != child.mask01 ? 1 : 0);
        current.mask10 = (~0ULL == child.mask10 ? 1 : 0);
        INT pointMask = (1 << child.index);
        current.mask01 = (current.mask01 << child.index);
        current.mask10 = (current.mask10 << child.index);
        parent.mask01 &= ~pointMask;
        parent.mask10 &= ~pointMask;
        parent.mask01 |= current.mask01;
        parent.mask10 |= current.mask10;
        AS->ptr[parent.point + 1] = parent.mask01;
        AS->ptr[parent.point] = parent.mask10;
        /* printf("[up:step] level=%llu child.index=%llu parent.index=%llu parent.point=%llu\n",
               (unsigned long long)level,
               (unsigned long long)child.index,
               (unsigned long long)parent.index,
               (unsigned long long)parent.point);
        printf("[up:mask] parent.before10=0x%016llx parent.before01=0x%016llx pointMask=0x%016llx current10=0x%016llx current01=0x%016llx\n",
               (unsigned long long)AS->ptr[parent.point],
               (unsigned long long)AS->ptr[parent.point + 1],
               (unsigned long long)pointMask,
               (unsigned long long)current.mask10,
               (unsigned long long)current.mask01);
        printf("[up:write] point=%llu mask10=0x%016llx mask01=0x%016llx\n",
               (unsigned long long)parent.point,
               (unsigned long long)AS->ptr[parent.point],
               (unsigned long long)AS->ptr[parent.point + 1]); */
        child = parent;
    }
    /* printf("[up:end] index=%llu level=%llu\n",
           (unsigned long long)index,
           (unsigned long long)level); */
    return FLOW_NONE;
}

static INT getAlloc(ARAS AS, INT size) {
    INT hendle = FindMemory(AS, size);
    INT level = ofLevel(size);
    INT power = level * 6 - 6;
    INT range = 64 << power;
    INT element = 1 << power;
    INT point = ofStartMaskPoint8(hendle, level) + 1;
    INT index = (hendle - point * 8) / element;

    //FillMask(AS, )
}

static INT tastCase(INT value, INT size, INT R)
{
    return toContiBit(value, size) + R;
}

static clock_t ofTimeTast(ARAS AS)
{
    volatile const char *rs = FLOW_NONE;
    volatile INT code[1 << 10] = {0};
    int top = 0;
    srand(time(NULL));
    volatile INT rands[1 << 10] = {0};
    for (int i = 0; i < 1 << 10; i++)
        rands[i] = ((uint64_t)rand() << 32) | rand();
    clock_t t = clock();
    for (int i = 0; i < 1 << 10; i++)
    {
        /* code[i] = (tastCase(rands[i], rands[i] % 64, rands[i] + i ) == (toContiBitLow(rands[i], rands[i] % 64) + rands[i] + i));
        printf(code[i] ? "" : "error");
        code[i] = tastCase(rands[i], rands[i] % 64, rands[i] + i); */
    }
    /* for(INT value = 0; ~value != 0ULL; value++ ) for(INT size = 0; size != 65; size++) {
        //printf("<value: %ld, size: %ld>\n<%lb>\n<%lb>\n", ~value, size, ~value, toContiBit(~value, size));
        code[top] = ( tastCase(~value, size, rands[top]) == (toContiBitLow(~value, size) + rands[top]));
        //if( value == (1ULL << 32) ) printf("f");
        top = top % (1<<10) + 1;
    } */
    return clock() - t;
}

static FN tastFunction(ARAS AS)
{
    INT point = (512 << 6) - 5;
    printf("<1: %ld, 2: %ld, 3: %ld>\n", ofStartMaskPoint8(point, 1) + 1, ofStartMaskPoint8(point, 2) + 1, ofStartMaskPoint8(point, 3) + 1);

    FN flow = FLOW_NONE;
    printf("<1: %ld, 2: %ld>\n", AS->ptr[ofMaskPoint8(0, 1)], AS->ptr[ofMaskPoint8(0, 2)]);
    flow = upLevelingMask(AS, 0, 1);
    printf("<1: %ld, 2: %ld>\n", AS->ptr[ofMaskPoint8(0, 1)], AS->ptr[ofMaskPoint8(0, 2)]);
}

int main(int argc, char *argv[])
{
    // start
    INT NULLINT = 0b0000000000000000000000000000000000000000000000000000000000000000;
    INT FULLINT = 0b1111111111111111111111111111111111111111111111111111111111111111;
    INT TASTINT = 0b0000000000000000000000000000000011111111111111111111111111111111;
    // printf("<len: %ld>", strlen("100000000000000000000000000000000"));
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
    struct AS *AS = createAS();
    printf("<count: %d>\n", printf("<%lb>\n<%lb>\n-----\n", TASTINT, toContiBitLow(TASTINT, 31)) - (5 + 4 + 3));
    /* for(int i = 0; i < 10; i++) {
        ExtendSpace(AS);
    } */
    /* AS->ptr[ofMaskPoint8(0, 4)] = 0b11;
    AS->ptr[ofMaskPoint8(2, 3)] = 0b111;
    AS->ptr[ofMaskPoint8(131, 2)] = 0b01;
    AS->ptr[ofMaskPoint8(131*64+1, 1)] = 0b0000000000000000000000000000000000000000000000000100001000010001; */
    // FindMemory(AS, 32);
    ExtendSpace(AS); ExtendSpace(AS);
    AS->ptr[ofMaskPoint8(0, 1)] = 0b1100000000000000000000000000000001111111111111111111111111111111;
    AS->ptr[ofMaskPoint8(0, 1) + 1] = FULLINT;
    //AS->ptr[ofMaskPoint8(1, 1)] = 0b0000000000000000000000000000000011111111111111111111111111111110;
    //AS->ptr[ofMaskPoint8(1, 1) + 1] = FULLINT;
    // printf("<level: %ld>", ofLevel(64) );
    printf("<AS|size: %ld>", AS->ptr_size << 6);
    printf("<point: %ld>", FindMemory(AS, 32));
    return 0;
    puts("");
    printf("<size: %ld>", AS->ptr_size);
    printf("<rpo: %ld>", ofMaskPoint8(1, 0));
    printf("<time: %lf>\n", (double)ofTimeTast(AS) / (1 << 10));
    puts("");
    printf("tast: ");
    //tastFunction(AS);
    // end
    puts("");
    AS = destroyAS(AS);
    return 0;
}