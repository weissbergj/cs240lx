#include "rpi.h"
#include "../unix-side/armv6-insts.h"

extern char __header_start__; //for memmap
extern char __header_end__;  //for memmap

void hello(void) { 
    printk("hello world\n");
}

// i would call this instead of printk if you have problems getting
// ldr figured out.
void foo(int x) { 
    printk("foo was passed %d\n", x);
}

void notmain(void) {
    // generate a dynamic call to hello world.
    // 1. you'll have to save/restore registers
    // 2. load the string address [likely using ldr]
    // 3. call printk

    const char *hdr_str = &__header_start__ + 4; //skips the 4-byte branch
    printk("For part 3, header text: <%s>\n", hdr_str);

    static uint32_t code[16];
    unsigned n = 0;

    // unimplemented();
    {
        // We will generate a "BL hello" followed by "BX LR".
        // For ARM, the branch offset is computed as:
        //   offset = (target_addr - (pc_of_this_instr + 8)) >> 2
        // because pc is ahead of the current instruction by 8 bytes.
        uint32_t base = (uint32_t)&code[n];
        uint32_t target = (uint32_t)hello;
        int32_t off = ((int32_t)target - ((int32_t)base + 8)) >> 2;

        // encode "BL <offset>" (condition=1110 (AL), bits24..0 = offset)
        code[n++] = 0xeb000000 | (off & 0x00ffffff);

        // encode "BX LR"
        code[n++] = 0xe12fff1e; 
    }

    printk("emitted code:\n");
    for(int i = 0; i < n; i++) 
        printk("code[%d]=0x%x\n", i, code[i]);

    void (*fp)(void) = (typeof(fp))code;
    printk("about to call: %x\n", fp);
    printk("--------------------------------------\n");
    fp();
    printk("--------------------------------------\n");
    printk("success!\n");
    clean_reboot();
}
