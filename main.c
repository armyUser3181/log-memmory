
#include "Mallocter.h"
#include <stdio.h>

struct Mallocter {
    uint64_t *space;
    uint64_t size;
};

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
}