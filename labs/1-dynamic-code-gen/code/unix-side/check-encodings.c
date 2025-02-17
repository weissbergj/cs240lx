#include <assert.h>
#include <sys/types.h>
#include <string.h>
#include "libunix.h"
#include <unistd.h>
#include "code-gen.h"
#include "armv6-insts.h"

/*
 *  1. emits <insts> into a temporary file.
 *  2. compiles it.
 *  3. reads back in.
 *  4. returns pointer to it.
 */
uint32_t *insts_emit(unsigned *nbytes, char *insts) {
    // check libunix.h --- create_file, write_exact, run_system, read_file.
    // unimplemented();
    char tmp_s[] = "tmpXXXXXX.s";
    char tmp_o[] = "tmpXXXXXX.o";
    char tmp_elf[] = "tmpXXXXXX.elf";
    char tmp_bin[] = "tmpXXXXXX.bin";
    int fd_s = create_file(tmp_s);
    char header[] = ".text\n.global code_start\ncode_start:\n";
    write_exact(fd_s, header, strlen(header));
    write_exact(fd_s, insts, strlen(insts));
    write_exact(fd_s, "\n", 1);
    close(fd_s);

    run_system("arm-none-eabi-as --warn --fatal-warnings -mcpu=arm1176jzf-s -march=armv6zk %s -o %s", tmp_s, tmp_o);
    run_system("arm-none-eabi-ld %s -T memmap -o %s", tmp_o, tmp_elf);
    run_system("arm-none-eabi-objcopy -O binary %s %s", tmp_elf, tmp_bin);

    unsigned len;
    uint8_t *bin = read_file(&len, tmp_bin);
    *nbytes = len;
    return (uint32_t *)bin;
}

/*
 * a cross-checking hack that uses the native GNU compiler/assembler to 
 * check our instruction encodings.
 *  1. compiles <insts>
 *  2. compares <code,nbytes> to it for equivalance.
 *  3. prints out a useful error message if it did not succeed!!
 */
void insts_check(char *insts, uint32_t *code, unsigned nbytes) {
    // make sure you print out something useful on mismatch!
    // unimplemented();
    unsigned gen_nbytes;
    uint32_t *gen = insts_emit(&gen_nbytes, insts);
    if(gen_nbytes != nbytes) {
        output("error: size mismatch: got=%d, expected=%d\n", gen_nbytes, nbytes);
        return;
    }
    if(memcmp(gen, code, nbytes)) {
        output("error: encoding mismatch for <%s>\n", insts);
        for(unsigned i = 0; i < nbytes/4; i++) {
            if(gen[i] != code[i]) 
                output("  mismatch at word %d: got=0x%x, expected=0x%x\n", i, gen[i], code[i]);
        }
    }
}

// check a single instruction.
void check_one_inst(char *insts, uint32_t inst) {
    return insts_check(insts, &inst, 4);
}

// helper function to make reverse engineering instructions a bit easier.
void insts_print(char *insts) {
    // emit <insts>
    unsigned gen_nbytes;
    uint32_t *gen = insts_emit(&gen_nbytes, insts);

    // print the result.
    output("getting encoding for: < %20s >\t= [", insts);
    unsigned n = gen_nbytes / 4;
    for(int i = 0; i < n; i++)
         output(" 0x%x ", gen[i]);
    output("]\n");
}


// helper function for reverse engineering.  you should refactor its interface
// so your code is better.
uint32_t emit_rrr(const char *op, const char *d, const char *s1, const char *s2) {
    char buf[1024];
    sprintf(buf, "%s %s, %s, %s", op, d, s1, s2);

    uint32_t n;
    uint32_t *c = insts_emit(&n, buf);
    assert(n == 4);
    return *c;
}

void derive_op_rrr(const char *name, const char *opcode, 
        const char **dst, const char **src1, const char **src2) {

    const char *d0 = dst[0];
    const char *s10 = src1[0];
    const char *s20 = src2[0];
    assert(d0 && s10 && s20);

    unsigned d_off = 0, src1_off = 0, src2_off = 0, op = ~0;

    // ------------------------------------------------------------------
    // destination register bit offset
    {
        uint32_t always_0 = ~0, always_1 = ~0;
        // Vary destination register; src1,  src2 fixed
        for (unsigned i = 0; dst[i]; i++) {
            uint32_t u = emit_rrr(opcode, dst[i], s10, s20);
            always_0 &= ~u;  // bits that are always 0
            always_1 &=  u;  // bits that are always 1
        }
        if (always_0 & always_1)
            panic("error: always_0 = %x, always_1 = %x\n", always_0, always_1); //cannot have nothing change

        uint32_t never_changed = always_0 | always_1;
        uint32_t changed = ~never_changed;

        d_off = ffs(changed); //offset from changed; mask where every bit did change is set to 1; ffs is some defined function somewhere; 1 - LSB
        if (((changed >> d_off) & ~0xf) != 0) panic("dst: %x\n", changed); //field width less than 4
        op &= never_changed;
    }

    // ------------------------------------------------------------------
    // src1 offset
    {
        uint32_t always_0 = ~0, always_1 = ~0;
        // Vary src1; fix dst, src2 to "r0"
        for (unsigned i = 0; src1[i]; i++) {
            uint32_t u = emit_rrr(opcode, "r0", src1[i], s20);
            always_0 &= ~u;
            always_1 &=  u;
        }
        uint32_t never_changed = always_0 | always_1;
        uint32_t changed = ~never_changed;

        src1_off = ffs(changed);
        if (((changed >> src1_off) & ~0xf) != 0)
            panic("panic; src1: %x\n", changed);
        op &= never_changed;
    }

    // ------------------------------------------------------------------
    // src2 offset
    {
        uint32_t always_0 = ~0, always_1 = ~0;
        // Vary src2; fix dst,  src1 to "r0"
        for (unsigned i = 0; src2[i]; i++) {
            uint32_t u = emit_rrr(opcode, "r0", "r0", src2[i]);
            always_0 &= ~u;
            always_1 &=  u;
        }
        uint32_t never_changed = always_0 | always_1;
        uint32_t changed = ~never_changed;

        src2_off = ffs(changed);
        if (((changed >> src2_off) & ~0xf) != 0)
            panic("panic; src2: %x\n", changed);
        op &= never_changed;
    }

    output("opcode is in =%x\n", op);

    // ------------------------------------------------------------------
    output("static int %s(uint32_t dst, uint32_t src1, uint32_t src2) {\n", name);
    output("    return %x | (dst << %d) | (src1 << %d) | (src2 << %d);\n",
           op,
           d_off,
           src1_off,
           src2_off);
    output("}\n");
}


/*
 * 1. we start by using the compiler / assembler tool chain to get / check
 *    instruction encodings.  this is sleazy and low-rent.   however, it 
 *    lets us get quick and dirty results, removing a bunch of the mystery.
 *
 * 2. after doing so we encode things "the right way" by using the armv6
 *    manual (esp chapters a3,a4,a5).  this lets you see how things are 
 *    put together.  but it is tedious.
 *
 * 3. to side-step tedium we use a variant of (1) to reverse engineer 
 *    the result.
 *
 *    we are only doing a small number of instructions today to get checked off
 *    (you, of course, are more than welcome to do a very thorough set) and focus
 *    on getting each method running from beginning to end.
 *
 * 4. then extend to a more thorough set of instructions: branches, loading
 *    a 32-bit constant, function calls.
 *
 * 5. use (4) to make a simple object oriented interface setup.
 *    you'll need: 
 *      - loads of 32-bit immediates
 *      - able to push onto a stack.
 *      - able to do a non-linking function call.
 */
int main(void) {
    // part 1: implement the code to do this.
    output("-----------------------------------------\n");
    output("part1: checking: correctly generating assembly.\n");
    insts_print("add r0, r0, r1");
    insts_print("bx lr");
    insts_print("mov r0, #1");
    insts_print("nop");
    output("\n");
    output("success!\n");

    // part 2: implement the code so these checks pass.
    // these should all pass.
    output("\n-----------------------------------------\n");
    output("part 2: checking we correctly compare asm to machine code.\n");
    check_one_inst("add r0, r0, r1", 0xe0800001);
    check_one_inst("bx lr", 0xe12fff1e);
    check_one_inst("mov r0, #1", 0xe3a00001);
    check_one_inst("nop", 0xe320f000);
    output("success!\n");

    // part 3: check that you can correctly encode an add instruction.
    output("\n-----------------------------------------\n");
    output("part3: checking that we can generate an <add> by hand\n");
    check_one_inst("add r0, r1, r2", arm_add(arm_r0, arm_r1, arm_r2));
    check_one_inst("add r3, r4, r5", arm_add(arm_r3, arm_r4, arm_r5));
    check_one_inst("add r6, r7, r8", arm_add(arm_r6, arm_r7, arm_r8));
    check_one_inst("add r9, r10, r11", arm_add(arm_r9, arm_r10, arm_r11));
    check_one_inst("add r12, r13, r14", arm_add(arm_r12, arm_r13, arm_r14));
    check_one_inst("add r15, r7, r3", arm_add(arm_r15, arm_r7, arm_r3));
    output("success!\n");

    // part 4: implement the code so it will derive the add instruction.
    output("\n-----------------------------------------\n");
    output("part4: checking that we can reverse engineer an <add>\n");

    const char *all_regs[] = {
                "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
                "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
                0 
    };
    // XXX: should probably pass a bitmask in instead.
    derive_op_rrr("arm_add", "add", all_regs,all_regs,all_regs);
    output("did something: now use the generated code in the checks above!\n");

    // part5: ADDITIONAL CHECKS I JUST ADDED
    output("\n-----------------------------------------\n");
    output("part5: checking additional instructions.\n");
    check_one_inst("cmp r1, r2", arm_cmp(arm_r1, arm_r2));
    check_one_inst("cmp r4, #0x12", arm_cmp_imm8(arm_r4, 0x12));
    check_one_inst("ldr r2, [r7, #0x10]", arm_ldr_imm(arm_r2, arm_r7, 0x10));
    check_one_inst("str r0, [r1, #0x20]", arm_str_imm(arm_r0, arm_r1, 0x20));
    // check_one_inst("b 0x100", arm_b(0x100));
    // check_one_inst("bl 0x200", arm_bl(0x200));
    
    check_one_inst("sub r0, r1, r2", arm_sub(arm_r0, arm_r1, arm_r2));
    check_one_inst("sub r3, r4, #0x10", arm_sub_imm8(arm_r3, arm_r4, 0x10));
    check_one_inst("rsb r2, r3, r4", arm_rsb(arm_r2, arm_r3, arm_r4));
    check_one_inst("rsb r0, r1, #0x1", arm_rsb_imm8(arm_r0, arm_r1, 0x1));

    check_one_inst("and r0, r1, r2", arm_and(arm_r0, arm_r1, arm_r2));
    check_one_inst("and r1, r2, #0x1", arm_and_imm8(arm_r1, arm_r2, 0x1));
    check_one_inst("eor r4, r5, r6", arm_eor(arm_r4, arm_r5, arm_r6));
    check_one_inst("eor r5, r6, #0x2", arm_eor_imm8(arm_r5, arm_r6, 0x2));

    check_one_inst("mov r0, r1", arm_mov(arm_r0, arm_r1));
    check_one_inst("mov r4, #0xab", arm_mov_imm8(arm_r4, 0xab));

    check_one_inst("tst r0, r1", arm_tst(arm_r0, arm_r1));
    check_one_inst("tst r2, #0x5", arm_tst_imm8(arm_r2, 0x5));
    check_one_inst("teq r3, r4", arm_teq(arm_r3, arm_r4));
    check_one_inst("teq r3, #0x7", arm_teq_imm8(arm_r3, 0x7));
    check_one_inst("cmn r2, r1", arm_cmn(arm_r2, arm_r1));
    check_one_inst("cmn r4, #0x8", arm_cmn_imm8(arm_r4, 0x8));

    output("success!\n");

    // get encodings for other instructions, loads, stores, branches, etc.
    return 0;
}
