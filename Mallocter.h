
#include <stdio.h>
#include <stdint.h>
#include <string.h>

struct Mallocter;
struct MallocterManager {
    struct Mallocter* (*create)();
    struct Mallocter* (*destroy)(struct Mallocter*);
    const char* (*extend)(struct Mallocter*);
    uint64_t (*find)(struct Mallocter*, uint64_t size);
    void* (*take)(struct Mallocter*, uint64_t size);
    const char* (*free)(struct Mallocter*, void* ptr, uint64_t size);
};

extern struct MallocterManager createMallocterManager();