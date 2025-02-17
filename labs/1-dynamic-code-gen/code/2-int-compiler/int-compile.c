#include "rpi.h"
#include "../unix-side/armv6-insts.h"

#define NELEM(x) (sizeof(x) / sizeof((x)[0]))
#include "cycle-util.h"

typedef void (*int_fp)(void);

static volatile unsigned cnt = 0;

// fake little "interrupt" handlers: useful just for measurement.
void int_0() { cnt++; }
void int_1() { cnt++; }
void int_2() { cnt++; }
void int_3() { cnt++; }
void int_4() { cnt++; }
void int_5() { cnt++; }
void int_6() { cnt++; }
void int_7() { cnt++; }

void generic_call_int(int_fp *intv, unsigned n) { 
    for(unsigned i = 0; i < n; i++)
        intv[i]();
}

//edited here: replaced static definition with a function pointer dispatch.
static void (*specialized_call_fn)(void) = 0;

//edited here: our outward-facing specialized_call_int calls the generated code.
void specialized_call_int(void) {
    // int_0();
    // int_1();
    // int_2();
    // int_3();
    // int_4();
    // int_5();
    // int_6();
    // int_7();
    specialized_call_fn();
}

//edited here: helper to compute a BL or B instruction.
static inline uint32_t arm_bl(uint32_t pc, uint32_t target) {
    int32_t off = ((int32_t)target - ((int32_t)pc + 8)) >> 2;
    return 0xeb000000 | (off & 0x00ffffff);
}
static inline uint32_t arm_b(uint32_t pc, uint32_t target) {
    int32_t off = ((int32_t)target - ((int32_t)pc + 8)) >> 2;
    return 0xea000000 | (off & 0x00ffffff);
}

//edited here: compile a specialized sequence that directly calls each handler.
static void *int_compile(int_fp *intv, unsigned n) {
    static uint32_t code[64];
    unsigned idx = 0;

    // push {lr} => store lr onto stack
    code[idx++] = 0xe92d4000; // push {lr}

    // for all but last: do a BL <handler>
    for(unsigned i = 0; i < n - 1; i++) {
        uint32_t pc = (uint32_t)&code[idx];
        code[idx++] = arm_bl(pc, (uint32_t)intv[i]);
    }

    // pop {lr}
    code[idx++] = 0xe8bd4000; // pop {lr}

    // final call: do a B <handler> so it returns to original caller
    {
        uint32_t pc = (uint32_t)&code[idx];
        code[idx++] = arm_b(pc, (uint32_t)intv[n - 1]);
    }

    // return the start of the code
    return code;
}

//everything above until generic call is new

void notmain(void) {
    int_fp intv[] = {
        int_0,
        int_1,
        int_2,
        int_3,
        int_4,
        int_5,
        int_6,
        int_7
    };

    cycle_cnt_init();

    unsigned n = NELEM(intv);

    // try with and without cache: but if you modify the routines to do 
    // jump-threadig, must either:
    //  1. generate code when cache is off.
    //  2. invalidate cache before use.
    // enable_cache();

    cnt = 0;
    TIME_CYC_PRINT10("cost of generic-int calling",  generic_call_int(intv,n));
    demand(cnt == n*10, "cnt=%d, expected=%d\n", cnt, n*10);

    //edited here: compile a specialized sequence dynamically
    specialized_call_fn = int_compile(intv, n);

    cnt = 0;
    TIME_CYC_PRINT10("cost of specialized int calling", specialized_call_int());
    demand(cnt == n*10, "cnt=%d, expected=%d\n", cnt, n*10);

    clean_reboot();
}
