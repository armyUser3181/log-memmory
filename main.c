
#include "Mallocter.h"
#include <stdio.h>

struct Mallocter {
    uint64_t *space;
    uint64_t size;
};

#define INT uint64_t
inline static INT ofMaskPoint64(INT index, INT level) {
    return (1 + index << 6 * level) - (level >> 2) - 1;
}
inline static INT ofMaskPoint8(INT index, INT level) {
    return (1 + index << 3 + 6 * level) - (level << 1);
}
inline static INT ofStartMaskPoint8(INT low, INT level) {
    return (low + (level << 1) >> 3 + 6 * level ) - 1;
}
#undef INT

int main() {

    struct MallocterManager mmg = createMallocterManager();
    struct Mallocter* mc = mmg.create();
    puts (mmg.extend(mc));
    puts (mmg.extend(mc));
    puts (mmg.extend(mc));
    puts (mmg.extend(mc));
    uint64_t *a[8] = { 0 };
    for(int i = 0; i < 8; i++) {
        a[i] = mmg.take(mc, 16);
    }
    for(int i = 0; i < 8; i++) {
        // *a[i] = i * 3;
    }
    for(int i = 0; i < 8; i++) {
        printf("%p \n", a[i]);
    }
    printf("%p", mc->space);
    mmg.destroy(mc);
    printf("hello world\n");
    uint64_t level = 2;
    for(int i = 0; i < 4; i++) {
        uint64_t target = ofMaskPoint8(i, level);
        uint64_t child_level = level - 1;
        int64_t low = ofStartMaskPoint8( i * 8 << level * 6, child_level) + 1;
        int64_t high = ofStartMaskPoint8(target, child_level);
        int64_t count = 0;
        printf("<%ld <", target);
        for(int64_t i = (int64_t)low; i < high; i++) {
            printf("%ld ", ofMaskPoint8(i, child_level));
            count++;
            if(count == 8) {
                puts("");
                count = 0;
            }
        }
        printf("> : inline>\n");
        printf("%ld %ld\n", low, high);
    }
}