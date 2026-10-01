#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * 완성형 예제 할당기
 *
 * 요청 크기는 바이트 단위로 받지만, 실제 공간은 max_align_t의 정렬
 * 단위로 반올림해 관리합니다. 각 단위의 사용 여부는 비트맵 한 비트로
 * 표현하며, 빈 비트가 연속으로 필요한 만큼 있는 첫 위치를 선택합니다.
 *
 * arena 저장소는 최대 크기로 한 번만 확보하고 논리 용량만 확장합니다.
 * 따라서 용량 확장 중에도 이미 반환한 사용자 포인터는 이동하지 않습니다.
 * 최대 크기의 실제 메모리를 생성 시 확보하는 설계이므로 큰 최대 용량을
 * 지정하면 생성 시점에 메모리 확보가 실패할 수 있습니다.
 */

#define ALLOCATION_UNIT ((size_t)_Alignof(max_align_t))

typedef struct AllocationRecord {
    size_t first_unit;
    size_t unit_count;
    size_t requested_size;
    bool active;
} AllocationRecord;

typedef struct Allocator {
    unsigned char *arena;
    unsigned char *bitmap;
    size_t capacity_units;
    size_t maximum_units;
    size_t bitmap_bytes;
    AllocationRecord *records;
    size_t record_count;
    size_t record_capacity;
} Allocator;

/* 바이트 크기를 정렬 단위 개수로 올림 변환하고 오버플로를 검사합니다. */
static bool bytes_to_units(size_t bytes, size_t *units)
{
    if (bytes == 0 || bytes > SIZE_MAX - (ALLOCATION_UNIT - 1)) {
        return false;
    }

    *units = (bytes + ALLOCATION_UNIT - 1) / ALLOCATION_UNIT;
    return true;
}

static bool bitmap_is_set(const unsigned char *bitmap, size_t unit)
{
    return (bitmap[unit / 8] & (unsigned char)(1u << (unit % 8))) != 0;
}

static void bitmap_set(unsigned char *bitmap, size_t unit)
{
    bitmap[unit / 8] |= (unsigned char)(1u << (unit % 8));
}

static void bitmap_clear(unsigned char *bitmap, size_t unit)
{
    bitmap[unit / 8] &= (unsigned char)~(1u << (unit % 8));
}

/*
 * 레코드 배열은 arena와 별도이므로 레코드 확장으로 사용자 메모리가
 * 움직이지 않습니다. realloc 실패 시 기존 배열은 그대로 유지됩니다.
 */
static bool reserve_record(Allocator *allocator)
{
    if (allocator->record_count < allocator->record_capacity) {
        return true;
    }

    size_t new_capacity = allocator->record_capacity == 0
        ? 16
        : allocator->record_capacity * 2;
    if (new_capacity < allocator->record_capacity ||
        new_capacity > SIZE_MAX / sizeof(*allocator->records)) {
        return false;
    }

    AllocationRecord *new_records = realloc(
        allocator->records, new_capacity * sizeof(*allocator->records));
    if (new_records == NULL) {
        return false;
    }

    allocator->records = new_records;
    allocator->record_capacity = new_capacity;
    return true;
}

/* 현재 논리 용량에서 요청한 길이의 연속 빈 구간을 first-fit으로 찾습니다. */
static bool find_free_run(const Allocator *allocator,
                          size_t needed_units,
                          size_t *first_unit)
{
    size_t run_start = 0;
    size_t run_length = 0;

    for (size_t unit = 0; unit < allocator->capacity_units; ++unit) {
        if (bitmap_is_set(allocator->bitmap, unit)) {
            run_length = 0;
            continue;
        }

        if (run_length == 0) {
            run_start = unit;
        }
        ++run_length;

        if (run_length == needed_units) {
            *first_unit = run_start;
            return true;
        }
    }

    return false;
}

/*
 * 최대 용량을 넘지 않도록 논리 용량을 두 배씩 확장합니다. arena와
 * 비트맵은 최대 크기 기준으로 이미 확보되어 있어 포인터 이동이 없습니다.
 */
static bool grow_capacity(Allocator *allocator)
{
    if (allocator->capacity_units >= allocator->maximum_units) {
        return false;
    }

    size_t new_capacity = allocator->capacity_units;
    if (new_capacity == 0) {
        new_capacity = 1;
    } else if (new_capacity > allocator->maximum_units / 2) {
        new_capacity = allocator->maximum_units;
    } else {
        new_capacity *= 2;
    }

    if (new_capacity > allocator->maximum_units) {
        new_capacity = allocator->maximum_units;
    }

    allocator->capacity_units = new_capacity;
    return true;
}

/*
 * 초기 논리 용량과 최대 용량을 바이트로 받습니다. 크기는 정렬 단위로
 * 올림되며, 최대 arena와 비트맵을 생성 시 확보합니다. 실패 시 NULL입니다.
 */
static Allocator *allocator_create(size_t initial_bytes, size_t maximum_bytes)
{
    size_t initial_units;
    size_t maximum_units;
    if (!bytes_to_units(initial_bytes, &initial_units) ||
        !bytes_to_units(maximum_bytes, &maximum_units) ||
        initial_units > maximum_units ||
        maximum_units > SIZE_MAX / ALLOCATION_UNIT ||
        maximum_units > SIZE_MAX - 7) {
        return NULL;
    }

    size_t bitmap_bytes = (maximum_units + 7) / 8;
    Allocator *allocator = calloc(1, sizeof(*allocator));
    if (allocator == NULL) {
        return NULL;
    }

    allocator->arena = malloc(maximum_units * ALLOCATION_UNIT);
    allocator->bitmap = calloc(bitmap_bytes, sizeof(*allocator->bitmap));
    if (allocator->arena == NULL || allocator->bitmap == NULL) {
        free(allocator->arena);
        free(allocator->bitmap);
        free(allocator);
        return NULL;
    }

    allocator->capacity_units = initial_units;
    allocator->maximum_units = maximum_units;
    allocator->bitmap_bytes = bitmap_bytes;
    return allocator;
}

/*
 * 최소 1바이트 할당을 지원하며 반환 주소는 max_align_t 정렬을 만족합니다.
 * 용량이 모자라면 확장 후 다시 탐색합니다. 반환 포인터는 arena 소유이며
 * allocator_destroy 호출 전까지만 유효합니다.
 */
static void *allocator_alloc(Allocator *allocator, size_t requested_size)
{
    if (allocator == NULL || requested_size == 0) {
        return NULL;
    }

    size_t needed_units;
    if (!bytes_to_units(requested_size, &needed_units) ||
        needed_units > allocator->maximum_units) {
        return NULL;
    }

    size_t first_unit;
    while (!find_free_run(allocator, needed_units, &first_unit)) {
        if (!grow_capacity(allocator)) {
            return NULL;
        }
    }

    /* 해제된 레코드를 재사용해 반복 작업의 메타데이터가 계속 쌓이지 않게 합니다. */
    size_t record_index = 0;
    while (record_index < allocator->record_count &&
           allocator->records[record_index].active) {
        ++record_index;
    }
    if (record_index == allocator->record_count) {
        /* 메타데이터 확보 실패 시 비트맵은 변경하지 않아 상태가 보존됩니다. */
        if (!reserve_record(allocator)) {
            return NULL;
        }
        ++allocator->record_count;
    }

    for (size_t unit = first_unit; unit < first_unit + needed_units; ++unit) {
        bitmap_set(allocator->bitmap, unit);
    }

    allocator->records[record_index] = (AllocationRecord){
        .first_unit = first_unit,
        .unit_count = needed_units,
        .requested_size = requested_size,
        .active = true,
    };

    return allocator->arena + first_unit * ALLOCATION_UNIT;
}

/*
 * 정확히 이 할당기에서 반환된, 아직 해제되지 않은 시작 포인터만 해제합니다.
 * 내부 포인터, 외부 포인터, 중복 해제는 false를 반환하고 상태를 바꾸지 않습니다.
 */
static bool allocator_free(Allocator *allocator, void *pointer)
{
    if (allocator == NULL || pointer == NULL) {
        return false;
    }

    uintptr_t base_address = (uintptr_t)allocator->arena;
    uintptr_t pointer_address = (uintptr_t)pointer;
    if (pointer_address < base_address) {
        return false;
    }

    uintptr_t byte_offset = pointer_address - base_address;
    size_t current_bytes = allocator->capacity_units * ALLOCATION_UNIT;
    if (byte_offset >= current_bytes || byte_offset % ALLOCATION_UNIT != 0) {
        return false;
    }

    size_t first_unit = (size_t)(byte_offset / ALLOCATION_UNIT);
    for (size_t index = 0; index < allocator->record_count; ++index) {
        AllocationRecord *record = &allocator->records[index];
        if (!record->active || record->first_unit != first_unit) {
            continue;
        }

        for (size_t unit = record->first_unit;
             unit < record->first_unit + record->unit_count;
             ++unit) {
            bitmap_clear(allocator->bitmap, unit);
        }
        record->active = false;
        return true;
    }

    return false;
}

/*
 * 테스트와 진단용 불변 조건 검사기입니다. 활성 레코드가 범위 안에 있고
 * 서로 겹치지 않으며, 레코드가 설명하는 사용 상태와 비트맵이 일치해야 합니다.
 */
static bool allocator_validate(const Allocator *allocator)
{
    if (allocator == NULL || allocator->arena == NULL ||
        allocator->bitmap == NULL || allocator->capacity_units == 0 ||
        allocator->capacity_units > allocator->maximum_units) {
        return false;
    }

    unsigned char *expected = calloc(allocator->bitmap_bytes, sizeof(*expected));
    if (expected == NULL) {
        return false;
    }

    bool valid = true;
    for (size_t index = 0; index < allocator->record_count && valid; ++index) {
        const AllocationRecord *record = &allocator->records[index];
        if (!record->active) {
            continue;
        }

        if (record->unit_count == 0 ||
            record->first_unit > allocator->capacity_units ||
            record->unit_count > allocator->capacity_units - record->first_unit ||
            record->requested_size == 0 ||
            record->requested_size > record->unit_count * ALLOCATION_UNIT) {
            valid = false;
            break;
        }

        for (size_t unit = record->first_unit;
             unit < record->first_unit + record->unit_count;
             ++unit) {
            if (bitmap_is_set(expected, unit)) {
                valid = false;
                break;
            }
            bitmap_set(expected, unit);
        }
    }

    if (valid && memcmp(expected, allocator->bitmap, allocator->bitmap_bytes) != 0) {
        valid = false;
    }

    free(expected);
    return valid;
}

static size_t allocator_capacity(const Allocator *allocator)
{
    return allocator == NULL ? 0 : allocator->capacity_units * ALLOCATION_UNIT;
}

static void allocator_destroy(Allocator *allocator)
{
    if (allocator == NULL) {
        return;
    }

    free(allocator->records);
    free(allocator->bitmap);
    free(allocator->arena);
    free(allocator);
}

/* 테스트 실패 시 조건과 소스 행을 출력하고 현재 테스트를 중단합니다. */
#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            return false;                                                       \
        }                                                                       \
    } while (0)

static bool test_create_and_invalid_requests(void)
{
    CHECK(allocator_create(0, 128) == NULL);
    CHECK(allocator_create(128, 64) == NULL);

    Allocator *allocator = allocator_create(64, 256);
    CHECK(allocator != NULL);
    CHECK(allocator_alloc(allocator, 0) == NULL);
    CHECK(allocator_alloc(allocator, SIZE_MAX) == NULL);
    CHECK(allocator_alloc(allocator, 257) == NULL);
    CHECK(allocator_validate(allocator));
    allocator_destroy(allocator);
    return true;
}

static bool test_alignment_growth_and_pointer_stability(void)
{
    Allocator *allocator = allocator_create(2 * ALLOCATION_UNIT,
                                             8 * ALLOCATION_UNIT);
    CHECK(allocator != NULL);

    unsigned char *first = allocator_alloc(allocator, 1);
    unsigned char *second = allocator_alloc(allocator, ALLOCATION_UNIT);
    CHECK(first != NULL && second != NULL);
    CHECK((uintptr_t)first % _Alignof(max_align_t) == 0);
    CHECK((uintptr_t)second % _Alignof(max_align_t) == 0);
    CHECK(second == first + ALLOCATION_UNIT);

    memset(first, 0xA5, ALLOCATION_UNIT);
    unsigned char *third = allocator_alloc(allocator, 3 * ALLOCATION_UNIT);
    CHECK(third != NULL);
    CHECK(allocator_capacity(allocator) >= 4 * ALLOCATION_UNIT);
    for (size_t index = 0; index < ALLOCATION_UNIT; ++index) {
        CHECK(first[index] == 0xA5);
    }

    CHECK(!allocator_free(allocator, first + 1));
    CHECK(!allocator_free(allocator, &allocator));
    CHECK(allocator_free(allocator, second));
    CHECK(!allocator_free(allocator, second));
    CHECK(allocator_validate(allocator));

    allocator_destroy(allocator);
    return true;
}

static bool test_contiguous_reuse(void)
{
    Allocator *allocator = allocator_create(8 * ALLOCATION_UNIT,
                                             8 * ALLOCATION_UNIT);
    CHECK(allocator != NULL);

    void *blocks[4];
    for (size_t index = 0; index < 4; ++index) {
        blocks[index] = allocator_alloc(allocator, 2 * ALLOCATION_UNIT);
        CHECK(blocks[index] != NULL);
    }

    CHECK(allocator_free(allocator, blocks[0]));
    CHECK(allocator_free(allocator, blocks[2]));
    CHECK(allocator_alloc(allocator, 3 * ALLOCATION_UNIT) == NULL);
    CHECK(allocator_free(allocator, blocks[1]));
    void *joined_run = allocator_alloc(allocator, 4 * ALLOCATION_UNIT);
    CHECK(joined_run == blocks[0]);
    CHECK(allocator_validate(allocator));

    allocator_destroy(allocator);
    return true;
}

static uint32_t next_random(uint32_t *state)
{
    *state = *state * UINT32_C(1664525) + UINT32_C(1013904223);
    return *state;
}

/* 고정 시드 반복으로 할당, 해제, 확장 뒤에도 비트맵 불변 조건을 검사합니다. */
static bool test_deterministic_workload(void)
{
    enum { SLOT_COUNT = 32, OPERATION_COUNT = 3000 };
    struct TestAllocation {
        unsigned char *pointer;
        size_t size;
        unsigned char marker;
    } slots[SLOT_COUNT] = {{0}};

    Allocator *allocator = allocator_create(8 * ALLOCATION_UNIT,
                                             1024 * ALLOCATION_UNIT);
    CHECK(allocator != NULL);
    uint32_t random_state = UINT32_C(0xC0FFEE);

    for (size_t operation = 0; operation < OPERATION_COUNT; ++operation) {
        size_t slot_index = next_random(&random_state) % SLOT_COUNT;
        struct TestAllocation *slot = &slots[slot_index];

        if (slot->pointer != NULL) {
            for (size_t byte = 0; byte < slot->size; ++byte) {
                CHECK(slot->pointer[byte] == slot->marker);
            }
            CHECK(allocator_free(allocator, slot->pointer));
            slot->pointer = NULL;
        } else {
            size_t size = 1 + next_random(&random_state) % (5 * ALLOCATION_UNIT);
            unsigned char *pointer = allocator_alloc(allocator, size);
            if (pointer != NULL) {
                unsigned char marker = (unsigned char)(operation + 1);
                memset(pointer, marker, size);
                slot->pointer = pointer;
                slot->size = size;
                slot->marker = marker;
            }
        }

        CHECK(allocator_validate(allocator));
        CHECK(allocator->record_count <= SLOT_COUNT);
    }

    for (size_t index = 0; index < SLOT_COUNT; ++index) {
        if (slots[index].pointer != NULL) {
            CHECK(allocator_free(allocator, slots[index].pointer));
        }
    }
    CHECK(allocator_validate(allocator));

    allocator_destroy(allocator);
    return true;
}

int main(void)
{
    struct TestCase {
        const char *name;
        bool (*run)(void);
    } tests[] = {
        {"create and invalid requests", test_create_and_invalid_requests},
        {"alignment, growth, and pointer stability",
         test_alignment_growth_and_pointer_stability},
        {"contiguous reuse", test_contiguous_reuse},
        {"deterministic workload", test_deterministic_workload},
    };

    size_t test_count = sizeof(tests) / sizeof(tests[0]);
    for (size_t index = 0; index < test_count; ++index) {
        if (!tests[index].run()) {
            return EXIT_FAILURE;
        }
        printf("PASS %s\n", tests[index].name);
    }

    printf("All %zu tests passed (allocation unit: %zu bytes).\n",
           test_count, ALLOCATION_UNIT);
    return EXIT_SUCCESS;
}