#ifndef __ARMV6_ENCODINGS_H__
#define __ARMV6_ENCODINGS_H__
// engler, cs240lx: simplistic instruction encodings for r/pi ARMV6.
// this will compile both on our bare-metal r/pi and unix.

// bit better type checking to use enums.
enum {
    arm_r0 = 0, 
    arm_r1, 
    arm_r2,
    arm_r3,
    arm_r4,
    arm_r5,
    arm_r6,
    arm_r7,
    arm_r8,
    arm_r9,
    arm_r10,
    arm_r11,
    arm_r12,
    arm_r13,
    arm_r14,
    arm_r15,
    arm_sp = arm_r13,
    arm_lr = arm_r14,
    arm_pc = arm_r15
};
_Static_assert(arm_r15 == 15, "bad enum");


// condition code.
enum {
    arm_EQ = 0,
    arm_NE,
    arm_CS,
    arm_CC,
    arm_MI,
    arm_PL,
    arm_VS,
    arm_VC,
    arm_HI,
    arm_LS,
    arm_GE,
    arm_LT,
    arm_GT,
    arm_LE,
    arm_AL,
};
_Static_assert(arm_AL == 0b1110, "bad enum list");

// data processing ops.
enum {
    arm_and_op = 0, 
    arm_eor_op,
    arm_sub_op,
    arm_rsb_op,
    arm_add_op,
    arm_adc_op,
    arm_sbc_op,
    arm_rsc_op,
    arm_tst_op,
    arm_teq_op,
    arm_cmp_op,
    arm_cmn_op,
    arm_orr_op,
    arm_mov_op,
    arm_bic_op,
    arm_mvn_op,
};
_Static_assert(arm_mvn_op == 0b1111, "bad num list");

/************************************************************
 * instruction encodings.  should do:
 *      bx lr
 *      ld *
 *      st *
 *      cmp
 *      branch
 *      alu 
 *      alu_imm
 *      jump
 *      call
 */

static inline unsigned arm_add(uint8_t rd, uint8_t rs1, uint8_t rs2) {
    assert(arm_add_op == 0b0100);
    return (0xe << 28) 
           | (arm_add_op << 21) 
           | (rs1 << 16) 
           | (rd << 12) 
           | (rs2 & 0xf);
}

static inline uint32_t arm_add_imm8(uint8_t rd, uint8_t rs1, uint8_t imm) {
    return (0xe << 28)
           | (1 << 25)
           | (arm_add_op << 21)
           | (rs1 << 16)
           | (rd << 12)
           | (imm & 0xff);
}

static inline uint32_t arm_bx(uint8_t reg) {
    return 0xe12fff10 | (reg & 0xf);
}

static inline uint32_t 
arm_or_imm_rot(uint8_t rd, uint8_t rs1, uint8_t imm8, uint8_t rot_nbits) {
    assert((rot_nbits % 2) == 0);
    return (0xe << 28)
           | (1 << 25)
           | (arm_orr_op << 21)
           | (rs1 << 16)
           | (rd << 12)
           | (((rot_nbits >> 1) & 0xf) << 8)
           | (imm8 & 0xff);
}

static inline uint32_t arm_cmp(uint8_t rn, uint8_t rm) {
    return (0xe << 28)
        | (arm_cmp_op << 21)
        | (1 << 20)      
        | (rn << 16)
        | (rm & 0xf);
}

static inline uint32_t arm_cmp_imm8(uint8_t rn, uint8_t imm) {
    return (0xe << 28)
        | (1 << 25)
        | (arm_cmp_op << 21)
        | (1 << 20)     
        | (rn << 16)
        | (imm & 0xff);
}

static inline uint32_t arm_ldr_imm(uint8_t rd, uint8_t rn, uint16_t imm12) {
    assert(imm12 < 4096);
    return (0xe << 28)
        | (1 << 26)
        | (1 << 24)
        | (1 << 23)
        | (1 << 20)     
        | (rn << 16)
        | (rd << 12)
        | (imm12 & 0xfff);
}

static inline uint32_t arm_str_imm(uint8_t rd, uint8_t rn, uint16_t imm12) {
    assert(imm12 < 4096);
    return (0xe << 28)
        | (1 << 26)
        | (1 << 24)
        | (1 << 23)
        | (rn << 16)
        | (rd << 12)
        | (imm12 & 0xfff);
}

static inline uint32_t arm_sub(uint8_t rd, uint8_t rs1, uint8_t rs2) {
    return (0xe << 28)
        | (arm_sub_op << 21)
        | (rs1 << 16)
        | (rd << 12)
        | (rs2 & 0xf);
}

static inline uint32_t arm_sub_imm8(uint8_t rd, uint8_t rs1, uint8_t imm8) {
    return (0xe << 28)
        | (1 << 25)
        | (arm_sub_op << 21)
        | (rs1 << 16)
        | (rd << 12)
        | (imm8 & 0xff);
}

static inline uint32_t arm_rsb(uint8_t rd, uint8_t rs1, uint8_t rs2) {
    return (0xe << 28)
        | (arm_rsb_op << 21)
        | (rs1 << 16)
        | (rd << 12)
        | (rs2 & 0xf);
}

static inline uint32_t arm_rsb_imm8(uint8_t rd, uint8_t rs1, uint8_t imm8) {
    return (0xe << 28)
        | (1 << 25)
        | (arm_rsb_op << 21)
        | (rs1 << 16)
        | (rd << 12)
        | (imm8 & 0xff);
}

static inline uint32_t arm_and(uint8_t rd, uint8_t rs1, uint8_t rs2) {
    return (0xe << 28)
        | (arm_and_op << 21)
        | (rs1 << 16)
        | (rd << 12)
        | (rs2 & 0xf);
}

static inline uint32_t arm_and_imm8(uint8_t rd, uint8_t rs1, uint8_t imm8) {
    return (0xe << 28)
        | (1 << 25)
        | (arm_and_op << 21)
        | (rs1 << 16)
        | (rd << 12)
        | (imm8 & 0xff);
}

static inline uint32_t arm_eor(uint8_t rd, uint8_t rs1, uint8_t rs2) {
    return (0xe << 28)
        | (arm_eor_op << 21)
        | (rs1 << 16)
        | (rd << 12)
        | (rs2 & 0xf);
}

static inline uint32_t arm_eor_imm8(uint8_t rd, uint8_t rs1, uint8_t imm8) {
    return (0xe << 28)
        | (1 << 25)
        | (arm_eor_op << 21)
        | (rs1 << 16)
        | (rd << 12)
        | (imm8 & 0xff);
}

static inline uint32_t arm_mov(uint8_t rd, uint8_t rm) {
    return (0xe << 28)
        | (arm_mov_op << 21)
        | (rd << 12)
        | (rm & 0xf);
}

static inline uint32_t arm_mov_imm8(uint8_t rd, uint8_t imm8) {
    return (0xe << 28)
        | (1 << 25)
        | (arm_mov_op << 21)
        | (rd << 12)
        | (imm8 & 0xff);
}

static inline uint32_t arm_tst(uint8_t rn, uint8_t rm) {
    return (0xe << 28)
        | (arm_tst_op << 21)
        | (1 << 20)
        | (rn << 16)
        | (rm & 0xf);
}

static inline uint32_t arm_tst_imm8(uint8_t rn, uint8_t imm8) {
    return (0xe << 28)
        | (1 << 25)
        | (arm_tst_op << 21)
        | (1 << 20)
        | (rn << 16)
        | (imm8 & 0xff);
}

static inline uint32_t arm_teq(uint8_t rn, uint8_t rm) {
    return (0xe << 28)
        | (arm_teq_op << 21)
        | (1 << 20)
        | (rn << 16)
        | (rm & 0xf);
}

static inline uint32_t arm_teq_imm8(uint8_t rn, uint8_t imm8) {
    return (0xe << 28)
        | (1 << 25)
        | (arm_teq_op << 21)
        | (1 << 20)
        | (rn << 16)
        | (imm8 & 0xff);
}

static inline uint32_t arm_cmn(uint8_t rn, uint8_t rm) {
    return (0xe << 28)
        | (arm_cmn_op << 21)
        | (1 << 20)
        | (rn << 16)
        | (rm & 0xf);
}

static inline uint32_t arm_cmn_imm8(uint8_t rn, uint8_t imm8) {
    return (0xe << 28)
        | (1 << 25)
        | (arm_cmn_op << 21)
        | (1 << 20)
        | (rn << 16)
        | (imm8 & 0xff);
}

#endif
