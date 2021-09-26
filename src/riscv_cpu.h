/*
riscv_cpu.h - RISC-V CPU Definitions
Copyright (C) 2021  LekKit <github.com/LekKit>
                    cerg2010cerg2010 <github.com/cerg2010cerg2010>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef RISCV_CPU_H
#define RISCV_CPU_H

#include "rvvm.h"
#include "riscv_hart.h"
#include "riscv_csr.h"
#include "mem_ops.h"
#include "assert.h"

void riscv_install_opcode_R(rvvm_hart_t* vm, uint32_t opcode, riscv_inst_t func);
void riscv_install_opcode_UJ(rvvm_hart_t* vm, uint32_t opcode, riscv_inst_t func);
void riscv_install_opcode_ISB(rvvm_hart_t* vm, uint32_t opcode, riscv_inst_t func);
void riscv_install_opcode_C(rvvm_hart_t* vm, uint32_t opcode, riscv_inst_c_t func);

void riscv_decoder_init_rv64(rvvm_hart_t* vm);
void riscv_decoder_init_rv32(rvvm_hart_t* vm);
void riscv_decoder_enable_fpu(rvvm_hart_t* vm, bool enable);
void riscv_run_till_event(rvvm_hart_t* vm);

void riscv32i_init(rvvm_hart_t* vm);
void riscv32c_init(rvvm_hart_t* vm);
void riscv32m_init(rvvm_hart_t* vm);
void riscv32a_init(rvvm_hart_t* vm);

void riscv64i_init(rvvm_hart_t* vm);
void riscv64c_init(rvvm_hart_t* vm);
void riscv64m_init(rvvm_hart_t* vm);
void riscv64a_init(rvvm_hart_t* vm);

void riscv32f_enable(rvvm_hart_t* vm, bool enable);
void riscv32d_enable(rvvm_hart_t* vm, bool enable);

void riscv64f_enable(rvvm_hart_t* vm, bool enable);
void riscv64d_enable(rvvm_hart_t* vm, bool enable);

void riscv_illegal_insn(rvvm_hart_t* vm, const uint32_t instruction);
void riscv_c_illegal_insn(rvvm_hart_t* vm, const uint16_t instruction);

// Private CPU implementation definitions
#ifdef RISCV_CPU_SOURCE

#ifdef RV64
    typedef uint64_t xlen_t;
    typedef int64_t sxlen_t;
    typedef uint64_t xaddr_t;
    #define SHAMT_BITS 6
    #define DIV_OVERFLOW_RS1 ((sxlen_t)0x8000000000000000ULL)
    #define riscv_i_init riscv64i_init
    #define riscv_c_init riscv64c_init
    #define riscv_m_init riscv64m_init
    #define riscv_a_init riscv64a_init
    #define riscv_f_enable riscv64f_enable
    #define riscv_d_enable riscv64d_enable
#else
    typedef uint32_t xlen_t;
    typedef int32_t sxlen_t;
    typedef uint32_t xaddr_t;
    #define SHAMT_BITS 5
    #define DIV_OVERFLOW_RS1 ((sxlen_t)0x80000000U)
    #define riscv_i_init riscv32i_init
    #define riscv_c_init riscv32c_init
    #define riscv_m_init riscv32m_init
    #define riscv_a_init riscv32a_init
    #define riscv_f_enable riscv32f_enable
    #define riscv_d_enable riscv32d_enable
#endif

static inline xlen_t riscv_read_register(rvvm_hart_t *vm, regid_t reg)
{
    return vm->registers[reg];
}

static inline sxlen_t riscv_read_register_s(rvvm_hart_t *vm, regid_t reg)
{
    return vm->registers[reg];
}

static inline void riscv_write_register(rvvm_hart_t *vm, regid_t reg, xlen_t data)
{
    vm->registers[reg] = data;
}

#ifdef USE_FPU
static inline float fpu_read_register32(rvvm_hart_t *vm, regid_t reg)
{
    assert(reg < FPU_REGISTERS_MAX);
    return read_float_nanbox(&vm->fpu_registers[reg]);
}

static inline void fpu_write_register32(rvvm_hart_t *vm, regid_t reg, float val)
{
    assert(reg < FPU_REGISTERS_MAX);
    // NOTE: for performance reasons/smaller JIT footprint, maybe
    // we should hardcode the FPU state to dirty?
    fpu_set_fs(vm, FS_DIRTY);
    write_float_nanbox(&vm->fpu_registers[reg], val);
}

static inline double fpu_read_register64(rvvm_hart_t *vm, regid_t reg)
{
    assert(reg < FPU_REGISTERS_MAX);
    return vm->fpu_registers[reg];
}

static inline void fpu_write_register64(rvvm_hart_t *vm, regid_t reg, double val)
{
    assert(reg < FPU_REGISTERS_MAX);
    fpu_set_fs(vm, FS_DIRTY);
    vm->fpu_registers[reg] = val;
}
#endif

/* translate compressed register encoding into normal */
static inline regid_t riscv_c_reg(regid_t reg)
{
    //NOTE: register index is hard limited to 8, since encoding is 3 bits
    return REGISTER_X8 + reg;
}

/*
 * For normal 32-bit length instructions, identifier is func7[25] func3[14:12] opcode[6:2]
 * For compressed 16-bit length instructions, identifier is func3[15:13] opcode[1:0]
 *
 * This is tricky for non-R type instructions since there's no func3 or func7,
 * so we will simply smudge function pointers for those all over the jumptable.
 */

/*
 * RVI Base ISA
 */

// U/J type instructions
#define RVI_LUI          0xD
#define RVI_AUIPC        0x5
#define RVI_JAL          0x1B
// R-type instructions
#define RVI_SLLI         0x24
#define RVI_SRLI_SRAI    0xA4
#define RVI_ADD_SUB      0xC
#define RVI_SLL          0x2C
#define RVI_SLT          0x4C
#define RVI_SLTU         0x6C
#define RVI_XOR          0x8C
#define RVI_SRL_SRA      0xAC
#define RVI_OR           0xCC
#define RVI_AND          0xEC
// I/S/B type instructions
#define RVI_JALR         0x19
#define RVI_BEQ          0x18
#define RVI_BNE          0x38
#define RVI_BLT          0x98
#define RVI_BGE          0xB8
#define RVI_BLTU         0xD8
#define RVI_BGEU         0xF8
#define RVI_LB           0x0
#define RVI_LH           0x20
#define RVI_LW           0x40
#define RVI_LBU          0x80
#define RVI_LHU          0xA0
#define RVI_SB           0x8
#define RVI_SH           0x28
#define RVI_SW           0x48
#define RVI_ADDI         0x4
#define RVI_SLTI         0x44
#define RVI_SLTIU        0x64
#define RVI_XORI         0x84
#define RVI_ORI          0xC4
#define RVI_ANDI         0xE4

/*
 * RV64I-only instructions
 */

// R-type instructions
#define RV64I_ADDIW       0x6
#define RV64I_SLLIW       0x26
#define RV64I_SRLIW_SRAIW 0xA6
#define RV64I_ADDW_SUBW   0xE
#define RV64I_SLLW        0x2E
#define RV64I_SRLW_SRAW   0xAE
// I/S/B type instructions
#define RV64I_LWU         0xC0
#define RV64I_LD          0x60
#define RV64I_SD          0x68

/*
 * RVC Compressed instructions
 */

/* opcode 0 */
#define RVC_ADDI4SPN     0x0
#define RVC_FLD          0x4
#define RVC_LW           0x8
#define RVC_FLW          0xC  // only exists on RV32!
#define RVC_RESERVED1    0x10
#define RVC_FSD          0x14
#define RVC_SW           0x18
#define RVC_FSW          0x1C // only exists on RV32!
#define RV64C_SD         0x1C // Replaces FSW on RV64
#define RV64C_LD         0xC  // Replaces FLW on RV64
/* opcode 1 */
#define RVC_ADDI         0x1  // this is also NOP when rs/rd == 0
#define RVC_JAL          0x5  // only exists on RV32!
#define RVC_LI           0x9
#define RVC_ADDI16SP_LUI 0xD  // this is ADDI16SP when rd == 2 or LUI (rd!=0)
#define RVC_ALOPS1       0x11 // a lot of operations packed tightly, idk about performance
#define RVC_J            0x15
#define RVC_BEQZ         0x19
#define RVC_BNEZ         0x1D
#define RV64C_ADDIW      0x5  // Replaces JAL on RV64
/* opcode 2 */
#define RVC_SLLI         0x2
#define RVC_FLDSP        0x6
#define RVC_LWSP         0xA
#define RVC_FLWSP        0xE  // only exists on RV32!
#define RVC_ALOPS2       0x12 // same as RVC_ALOPS1
#define RVC_FSDSP        0x16
#define RVC_SWSP         0x1A
#define RVC_FSWSP        0x1E // only exists on RV32!
#define RV64C_LDSP       0xE  // Replaces FLWSP on RV64
#define RV64C_SDSP       0x1E // Replaces FSWSP on RV64

/*
 * RVM Math instructions
 */
// R-type instructions
#define RVM_MUL          0x10C
#define RVM_MULH         0x12C
#define RVM_MULHSU       0x14C
#define RVM_MULHU        0x16C
#define RVM_DIV          0x18C
#define RVM_DIVU         0x1AC
#define RVM_REM          0x1CC
#define RVM_REMU         0x1EC

/*
 * RV64M-only instructions
 */

// R-type instructions
#define RV64M_MULW       0x10E
#define RV64M_DIVW       0x18E
#define RV64M_DIVUW      0x1AE
#define RV64M_REMW       0x1CE
#define RV64M_REMUW      0x1EE

/*
 * RVA, RV64A Atomic instructions
 */

// I/S/B type instructions
#define RVA_ATOMIC_W     0x4B
#define RV64A_ATOMIC_D   0x6B

/*
 * RV32F instructions
 */
#define RVF_FLW          0x41 /* ISB */
#define RVF_FSW          0x49 /* ISB */
#define RVF_FMADD        0x10 /* R + funct3 */
#define RVF_FMSUB        0x11 /* R + funct3 */
#define RVF_FNMSUB       0x12 /* R + funct3 */
#define RVF_FNMADD       0x13 /* R + funct3 */
#define RVF_OTHER        0x14 /* R + funct3 + funct7 a bunch */

/*
 * RV32D instructions
 */
#define RVD_FLW          0x61 /* ISB */
#define RVD_FSW          0x69 /* ISB */
#define RVD_FMADD        0x110 /* R + funct3 */
#define RVD_FMSUB        0x111 /* R + funct3 */
#define RVD_FNMSUB       0x112 /* R + funct3 */
#define RVD_FNMADD       0x113 /* R + funct3 */
/* except FCVT.S.D */
#define RVD_OTHER        0x114 /* R + funct3 + funct7 a bunch */

#endif
#endif