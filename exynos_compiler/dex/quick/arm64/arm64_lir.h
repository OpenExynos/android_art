/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ART_COMPILER_DEX_QUICK_ARM64_ARM64_LIR_H_
#define ART_COMPILER_DEX_QUICK_ARM64_ARM64_LIR_H_

#include "dex/compiler_enums.h"
#include "dex/reg_location.h"
#include "dex/reg_storage.h"

namespace art {

/*
 * Runtime register usage conventions.
 *
 * r0     : As in C/C++ w0 is 32-bit return register and x0 is 64-bit.
 * r0-r7  : Argument registers in both Dalvik and C/C++ conventions.
 *          However, for Dalvik->Dalvik calls we'll pass the target's Method*
 *          pointer in x0 as a hidden arg0. Otherwise used as codegen scratch
 *          registers.
 * r8-r15 : Caller save registers (used as temporary registers).
 * r16-r17: Also known as ip0-ip1, respectively. Used as scratch registers by
 *          the linker, by the trampolines and other stubs (the backend uses
 *          these as temporary registers).
 * r18    : (rxSELF) is reserved (pointer to thread-local storage).
 * r19-r29: Callee save registers (promotion targets).
 * r30    : (lr) is reserved (the link register).
 * rsp    : (sp) is reserved (the stack pointer).
 * rzr    : (zr) is reserved (the zero register).
 *
 * 18 core temps that codegen can use (r0-r17).
 * 10 core registers that can be used for promotion.
 *
 * Floating-point registers
 * v0-v31
 *
 * v0     : s0 is return register for singles (32-bit) and d0 for doubles (64-bit).
 *          This is analogous to the C/C++ (hard-float) calling convention.
 * v0-v7  : Floating-point argument registers in both Dalvik and C/C++ conventions.
 *          Also used as temporary and codegen scratch registers.
 *
 * v0-v7 and v16-v31 : trashed across C calls.
 * v8-v15 : bottom 64-bits preserved across C calls (d8-d15 are preserved).
 *
 * v16-v31: Used as codegen temp/scratch.
 * v8-v15 : Can be used for promotion.
 *
 * Calling convention (Hard-float)
 *     o On a call to a Dalvik method, pass target's Method* in x0
 *     o r1-r7, v0-v7 will be used for the first 7+8 arguments
 *     o Arguments which cannot be put in registers are placed in appropriate
 *       out slots by the caller.
 *     o Maintain a 16-byte stack alignment
 *
 *  Stack frame diagram (stack grows down, higher addresses at top):
 *
 * +--------------------------------------------+
 * | IN[ins-1]                                  |  {Note: resides in caller's frame}
 * |       .                                    |
 * | IN[0]                                      |
 * | caller's method ArtMethod*                 |  {Pointer sized reference}
 * +============================================+  {Note: start of callee's frame}
 * | spill region                               |  {variable sized - will include lr if non-leaf}
 * +--------------------------------------------+
 * |   ...filler word...                        |  {Note: used as 2nd word of V[locals-1] if long}
 * +--------------------------------------------+
 * | V[locals-1]                                |
 * | V[locals-2]                                |
 * |      .                                     |
 * |      .                                     |
 * | V[1]                                       |
 * | V[0]                                       |
 * +--------------------------------------------+
 * |   0 to 3 words padding                     |
 * +--------------------------------------------+
 * | OUT[outs-1]                                |
 * | OUT[outs-2]                                |
 * |       .                                    |
 * | OUT[0]                                     |
 * | current method ArtMethod*                  | <<== sp w/ 16-byte alignment
 * +============================================+
 */

// First FP callee save.
#define A64_FP_CALLEE_SAVE_BASE 8

// Temporary macros, used to mark code which wants to distinguish betweek zr/sp.
#define A64_REG_IS_SP(reg_num) ((reg_num) == rwsp || (reg_num) == rsp)
#define A64_REG_IS_ZR(reg_num) ((reg_num) == rwzr || (reg_num) == rxzr)
#define A64_REGSTORAGE_IS_SP_OR_ZR(rs) (((rs).GetRegNum() & 0x1f) == 0x1f)

enum A64ResourceEncodingPos {
  kA64GPReg0   = 0,
  kA64RegLR    = 30,
  kA64RegSP    = 31,
  kA64FPReg0   = 32,
  kA64RegEnd   = 64,
};

#define IS_SIGNED_IMM(size, value) \
  ((value) >= -(1 << ((size) - 1)) && (value) < (1 << ((size) - 1)))
#define IS_SIGNED_IMM7(value) IS_SIGNED_IMM(7, value)
#define IS_SIGNED_IMM9(value) IS_SIGNED_IMM(9, value)
#define IS_SIGNED_IMM12(value) IS_SIGNED_IMM(12, value)
#define IS_SIGNED_IMM14(value) IS_SIGNED_IMM(14, value)
#define IS_SIGNED_IMM19(value) IS_SIGNED_IMM(19, value)
#define IS_SIGNED_IMM21(value) IS_SIGNED_IMM(21, value)
#define IS_SIGNED_IMM26(value) IS_SIGNED_IMM(26, value)

// Quick macro used to define the registers.
#define A64_REGISTER_CODE_LIST(R) \
  R(0)  R(1)  R(2)  R(3)  R(4)  R(5)  R(6)  R(7) \
  R(8)  R(9)  R(10) R(11) R(12) R(13) R(14) R(15) \
  R(16) R(17) R(18) R(19) R(20) R(21) R(22) R(23) \
  R(24) R(25) R(26) R(27) R(28) R(29) R(30) R(31)

// Registers (integer) values.
enum A64NativeRegisterPool {  // private marker to avoid generate-operator-out.py from processing.
#  define A64_DEFINE_REGISTERS(nr) \
    rw##nr = RegStorage::k32BitSolo | RegStorage::kCoreRegister | nr, \
    rx##nr = RegStorage::k64BitSolo | RegStorage::kCoreRegister | nr, \
    rf##nr = RegStorage::k32BitSolo | RegStorage::kFloatingPoint | nr, \
    rd##nr = RegStorage::k64BitSolo | RegStorage::kFloatingPoint | nr, \
    rq##nr = RegStorage::k128BitSolo | RegStorage::kFloatingPoint | nr,
  A64_REGISTER_CODE_LIST(A64_DEFINE_REGISTERS)
#undef A64_DEFINE_REGISTERS

  rxzr = RegStorage::k64BitSolo | RegStorage::kCoreRegister | 0x3f,
  rwzr = RegStorage::k32BitSolo | RegStorage::kCoreRegister | 0x3f,
  rsp = rx31,
  rwsp = rw31,

  // Aliases which are not defined in "ARM Architecture Reference, register names".
  rxIP0 = rx16,
  rxIP1 = rx17,
  rxSELF = rx18,
  rxLR = rx30,
  /*
   * FIXME: It's a bit awkward to define both 32 and 64-bit views of these - we'll only ever use
   * the 64-bit view. However, for now we'll define a 32-bit view to keep these from being
   * allocated as 32-bit temp registers.
   */
  rwIP0 = rw16,
  rwIP1 = rw17,
  rwSELF = rw18,
  rwLR = rw30,
};

#define A64_DEFINE_REGSTORAGES(nr) \
  constexpr RegStorage rs_w##nr(RegStorage::kValid | rw##nr); \
  constexpr RegStorage rs_x##nr(RegStorage::kValid | rx##nr); \
  constexpr RegStorage rs_f##nr(RegStorage::kValid | rf##nr); \
  constexpr RegStorage rs_d##nr(RegStorage::kValid | rd##nr); \
  constexpr RegStorage rs_q##nr(RegStorage::kValid | rq##nr);
A64_REGISTER_CODE_LIST(A64_DEFINE_REGSTORAGES)
#undef A64_DEFINE_REGSTORAGES

constexpr RegStorage rs_xzr(RegStorage::kValid | rxzr);
constexpr RegStorage rs_wzr(RegStorage::kValid | rwzr);
constexpr RegStorage rs_xIP0(RegStorage::kValid | rxIP0);
constexpr RegStorage rs_wIP0(RegStorage::kValid | rwIP0);
constexpr RegStorage rs_xIP1(RegStorage::kValid | rxIP1);
constexpr RegStorage rs_wIP1(RegStorage::kValid | rwIP1);
// Reserved registers.
constexpr RegStorage rs_xSELF(RegStorage::kValid | rxSELF);
constexpr RegStorage rs_sp(RegStorage::kValid | rsp);
constexpr RegStorage rs_xLR(RegStorage::kValid | rxLR);
// TODO: eliminate the need for these.
constexpr RegStorage rs_wSELF(RegStorage::kValid | rwSELF);
constexpr RegStorage rs_wsp(RegStorage::kValid | rwsp);
constexpr RegStorage rs_wLR(RegStorage::kValid | rwLR);

// RegisterLocation templates return values (following the hard-float calling convention).
const RegLocation a64_loc_c_return =
    {kLocPhysReg, 0, 0, 0, 0, 0, 0, 0, 1, rs_w0, INVALID_SREG, INVALID_SREG};
const RegLocation a64_loc_c_return_ref =
    {kLocPhysReg, 0, 0, 0, 0, 0, 1, 0, 1, rs_x0, INVALID_SREG, INVALID_SREG};
const RegLocation a64_loc_c_return_wide =
    {kLocPhysReg, 1, 0, 0, 0, 0, 0, 0, 1, rs_x0, INVALID_SREG, INVALID_SREG};
const RegLocation a64_loc_c_return_float =
    {kLocPhysReg, 0, 0, 0, 1, 0, 0, 0, 1, rs_f0, INVALID_SREG, INVALID_SREG};
const RegLocation a64_loc_c_return_double =
    {kLocPhysReg, 1, 0, 0, 1, 0, 0, 0, 1, rs_d0, INVALID_SREG, INVALID_SREG};

/**
 * @brief Shift-type to be applied to a register via EncodeShift().
 */
enum A64ShiftEncodings {
  kA64Lsl = 0x0,
  kA64Lsr = 0x1,
  kA64Asr = 0x2,
  kA64Ror = 0x3
};

/**
 * @brief Extend-type to be applied to a register via EncodeExtend().
 */
enum A64RegExtEncodings {
  kA64Uxtb = 0x0,
  kA64Uxth = 0x1,
  kA64Uxtw = 0x2,
  kA64Uxtx = 0x3,
  kA64Sxtb = 0x4,
  kA64Sxth = 0x5,
  kA64Sxtw = 0x6,
  kA64Sxtx = 0x7
};

#define ENCODE_NO_SHIFT (EncodeShift(kA64Lsl, 0))
#define ENCODE_NO_EXTEND (EncodeExtend(kA64Uxtx, 0))
/*
 * The following enum defines the list of supported A64 instructions by the
 * assembler. Their corresponding EncodingMap positions will be defined in
 * assemble_arm64.cc.
 */
 #ifdef EXYNOS_ART_OPT
enum A64Opcode {
  kA64First = 0,
  kA64Adc3rrr = kA64First,  // adc [00011010000] rm[20-16] [000000] rn[9-5] rd[4-0].
  kA64Add4RRdT,      // add [s001000100] imm_12[21-10] rn[9-5] rd[4-0].
  kA64Add4rrro,      // add [00001011000] rm[20-16] imm_6[15-10] rn[9-5] rd[4-0].
  kA64Add4RRre,      // add [00001011001] rm[20-16] option[15-13] imm_3[12-10] rn[9-5] rd[4-0].
  kA64Adr2xd,        // adr [0] immlo[30-29] [10000] immhi[23-5] rd[4-0].
  kA64Adrp2xd,       // adrp [1] immlo[30-29] [10000] immhi[23-5] rd[4-0].
  kA64And3Rrl,       // and [00010010] N[22] imm_r[21-16] imm_s[15-10] rn[9-5] rd[4-0].
  kA64And4rrro,      // and [00001010] shift[23-22] [N=0] rm[20-16] imm_6[15-10] rn[9-5] rd[4-0].
  kA64Asr3rrd,       // asr [0001001100] immr[21-16] imms[15-10] rn[9-5] rd[4-0].
  kA64Asr3rrr,       // asr alias of "sbfm arg0, arg1, arg2, {#31/#63}".
  kA64B2ct,          // b.cond [01010100] imm_19[23-5] [0] cond[3-0].
  kA64Blr1x,         // blr [1101011000111111000000] rn[9-5] [00000].
  kA64Br1x,          // br  [1101011000011111000000] rn[9-5] [00000].
  kA64Bl1t,          // bl  [100101] imm26[25-0].
  kA64Brk1d,         // brk [11010100001] imm_16[20-5] [00000].
  kA64B1t,           // b   [00010100] offset_26[25-0].
  kA64Cbnz2rt,       // cbnz[00110101] imm_19[23-5] rt[4-0].
  kA64Cbz2rt,        // cbz [00110100] imm_19[23-5] rt[4-0].
  kA64Cmn3rro,       // cmn [s0101011] shift[23-22] [0] rm[20-16] imm_6[15-10] rn[9-5] [11111].
  kA64Cmn3Rre,       // cmn [s0101011001] rm[20-16] option[15-13] imm_3[12-10] rn[9-5] [11111].
  kA64Cmn3RdT,       // cmn [00110001] shift[23-22] imm_12[21-10] rn[9-5] [11111].
  kA64Cmp3rro,       // cmp [s1101011] shift[23-22] [0] rm[20-16] imm_6[15-10] rn[9-5] [11111].
  kA64Cmp3Rre,       // cmp [s1101011001] rm[20-16] option[15-13] imm_3[12-10] rn[9-5] [11111].
  kA64Cmp3RdT,       // cmp [01110001] shift[23-22] imm_12[21-10] rn[9-5] [11111].
  kA64Csel4rrrc,     // csel[s0011010100] rm[20-16] cond[15-12] [00] rn[9-5] rd[4-0].
  kA64Csinc4rrrc,    // csinc [s0011010100] rm[20-16] cond[15-12] [01] rn[9-5] rd[4-0].
  kA64Csinv4rrrc,    // csinv [s1011010100] rm[20-16] cond[15-12] [00] rn[9-5] rd[4-0].
  kA64Csneg4rrrc,    // csneg [s1011010100] rm[20-16] cond[15-12] [01] rn[9-5] rd[4-0].
  kA64Dmb1B,         // dmb [11010101000000110011] CRm[11-8] [10111111].
  kA64Eor3Rrl,       // eor [s10100100] N[22] imm_r[21-16] imm_s[15-10] rn[9-5] rd[4-0].
  kA64Eor4rrro,      // eor [s1001010] shift[23-22] [0] rm[20-16] imm_6[15-10] rn[9-5] rd[4-0].
  kA64Extr4rrrd,     // extr[s00100111N0] rm[20-16] imm_s[15-10] rn[9-5] rd[4-0].
  kA64Fabs2ff,       // fabs[000111100s100000110000] rn[9-5] rd[4-0].
  kA64Fadd3fff,      // fadd[000111100s1] rm[20-16] [001010] rn[9-5] rd[4-0].
  kA64Fcmp1f,        // fcmp[000111100s100000001000] rn[9-5] [01000].
  kA64Fcmp2ff,       // fcmp[000111100s1] rm[20-16] [001000] rn[9-5] [00000].
  kA64Fcvtzs2wf,     // fcvtzs [000111100s111000000000] rn[9-5] rd[4-0].
  kA64Fcvtzs2xf,     // fcvtzs [100111100s111000000000] rn[9-5] rd[4-0].
  kA64Fcvt2Ss,       // fcvt   [0001111000100010110000] rn[9-5] rd[4-0].
  kA64Fcvt2sS,       // fcvt   [0001111001100010010000] rn[9-5] rd[4-0].
  kA64Fcvtms2ws,     // fcvtms [0001111000110000000000] rn[9-5] rd[4-0].
  kA64Fcvtms2xS,     // fcvtms [1001111001110000000000] rn[9-5] rd[4-0].
  kA64Fdiv3fff,      // fdiv[000111100s1] rm[20-16] [000110] rn[9-5] rd[4-0].
  kA64Fmax3fff,      // fmax[000111100s1] rm[20-16] [010010] rn[9-5] rd[4-0].
  kA64Fmin3fff,      // fmin[000111100s1] rm[20-16] [010110] rn[9-5] rd[4-0].
  kA64Fmov2ff,       // fmov[000111100s100000010000] rn[9-5] rd[4-0].
  kA64Fmov2fI,       // fmov[000111100s1] imm_8[20-13] [10000000] rd[4-0].
  kA64Fmov2sw,       // fmov[0001111000100111000000] rn[9-5] rd[4-0].
  kA64Fmov2Sx,       // fmov[1001111001100111000000] rn[9-5] rd[4-0].
  kA64Fmov2ws,       // fmov[0001111001101110000000] rn[9-5] rd[4-0].
  kA64Fmov2xS,       // fmov[1001111001101111000000] rn[9-5] rd[4-0].
  kA64Fmul3fff,      // fmul[000111100s1] rm[20-16] [000010] rn[9-5] rd[4-0].
  kA64Fneg2ff,       // fneg[000111100s100001010000] rn[9-5] rd[4-0].
  kA64Frintp2ff,     // frintp [000111100s100100110000] rn[9-5] rd[4-0].
  kA64Frintm2ff,     // frintm [000111100s100101010000] rn[9-5] rd[4-0].
  kA64Frintn2ff,     // frintn [000111100s100100010000] rn[9-5] rd[4-0].
  kA64Frintz2ff,     // frintz [000111100s100101110000] rn[9-5] rd[4-0].
  kA64Fsqrt2ff,      // fsqrt[000111100s100001110000] rn[9-5] rd[4-0].
  kA64Fsub3fff,      // fsub[000111100s1] rm[20-16] [001110] rn[9-5] rd[4-0].
  kA64Ldrb3wXd,      // ldrb[0011100101] imm_12[21-10] rn[9-5] rt[4-0].
  kA64Ldrb3wXx,      // ldrb[00111000011] rm[20-16] [011] S[12] [10] rn[9-5] rt[4-0].
  kA64Ldrsb3rXd,     // ldrsb[001110011s] imm_12[21-10] rn[9-5] rt[4-0].
  kA64Ldrsb3rXx,     // ldrsb[0011 1000 1s1] rm[20-16] [011] S[12] [10] rn[9-5] rt[4-0].
  kA64Ldrh3wXF,      // ldrh[0111100101] imm_12[21-10] rn[9-5] rt[4-0].
  kA64Ldrh4wXxd,     // ldrh[01111000011] rm[20-16] [011] S[12] [10] rn[9-5] rt[4-0].
  kA64Ldrsh3rXF,     // ldrsh[011110011s] imm_12[21-10] rn[9-5] rt[4-0].
  kA64Ldrsh4rXxd,    // ldrsh[011110001s1] rm[20-16] [011] S[12] [10] rn[9-5] rt[4-0]
  kA64Ldr2fp,        // ldr [0s011100] imm_19[23-5] rt[4-0].
  kA64Ldr2rp,        // ldr [0s011000] imm_19[23-5] rt[4-0].
  kA64Ldr3fXD,       // ldr [1s11110100] imm_12[21-10] rn[9-5] rt[4-0].
  kA64Ldr3rXD,       // ldr [1s111000010] imm_9[20-12] [01] rn[9-5] rt[4-0].
  kA64Ldr4fXxG,      // ldr [1s111100011] rm[20-16] [011] S[12] [10] rn[9-5] rt[4-0].
  kA64Ldr4rXxG,      // ldr [1s111000011] rm[20-16] [011] S[12] [10] rn[9-5] rt[4-0].
  kA64LdrPost3rXd,   // ldr [1s111000010] imm_9[20-12] [01] rn[9-5] rt[4-0].
  kA64Ldp4ffXD,      // ldp [0s10110101] imm_7[21-15] rt2[14-10] rn[9-5] rt[4-0].
  kA64Ldp4rrXD,      // ldp [s010100101] imm_7[21-15] rt2[14-10] rn[9-5] rt[4-0].
  kA64LdpPost4rrXD,  // ldp [s010100011] imm_7[21-15] rt2[14-10] rn[9-5] rt[4-0].
  kA64Ldur3fXd,      // ldur[1s111100010] imm_9[20-12] [00] rn[9-5] rt[4-0].
  kA64Ldur3rXd,      // ldur[1s111000010] imm_9[20-12] [00] rn[9-5] rt[4-0].
  kA64Ldxr2rX,       // ldxr[1s00100001011111011111] rn[9-5] rt[4-0].
  kA64Ldaxr2rX,      // ldaxr[1s00100001011111111111] rn[9-5] rt[4-0].
  kA64Lsl3rrr,       // lsl [s0011010110] rm[20-16] [001000] rn[9-5] rd[4-0].
  kA64Lsr3rrd,       // lsr alias of "ubfm arg0, arg1, arg2, #{31/63}".
  kA64Lsr3rrr,       // lsr [s0011010110] rm[20-16] [001001] rn[9-5] rd[4-0].
  kA64Madd4rrrr,     // madd[s0011011000] rm[20-16] [0] ra[14-10] rn[9-5] rd[4-0].
  kA64Movk3rdM,      // mov [010100101] hw[22-21] imm_16[20-5] rd[4-0].
  kA64Movn3rdM,      // mov [000100101] hw[22-21] imm_16[20-5] rd[4-0].
  kA64Movz3rdM,      // mov [011100101] hw[22-21] imm_16[20-5] rd[4-0].
  kA64Mov2rr,        // mov [00101010000] rm[20-16] [000000] [11111] rd[4-0].
  kA64Mvn2rr,        // mov [00101010001] rm[20-16] [000000] [11111] rd[4-0].
  kA64Mul3rrr,       // mul [00011011000] rm[20-16] [011111] rn[9-5] rd[4-0].
  kA64Msub4rrrr,     // msub[s0011011000] rm[20-16] [1] ra[14-10] rn[9-5] rd[4-0].
  kA64Neg3rro,       // neg alias of "sub arg0, rzr, arg1, arg2".
  kA64Nop0,          // nop alias of "hint #0" [11010101000000110010000000011111].
  kA64Orr3Rrl,       // orr [s01100100] N[22] imm_r[21-16] imm_s[15-10] rn[9-5] rd[4-0].
  kA64Orr4rrro,      // orr [s0101010] shift[23-22] [0] rm[20-16] imm_6[15-10] rn[9-5] rd[4-0].
  kA64Ret,           // ret [11010110010111110000001111000000].
  kA64Rbit2rr,       // rbit [s101101011000000000000] rn[9-5] rd[4-0].
  kA64Rev2rr,        // rev [s10110101100000000001x] rn[9-5] rd[4-0].
  kA64Rev162rr,      // rev16[s101101011000000000001] rn[9-5] rd[4-0].
  kA64Ror3rrr,       // ror [s0011010110] rm[20-16] [001011] rn[9-5] rd[4-0].
  kA64Sbc3rrr,       // sbc [s0011010000] rm[20-16] [000000] rn[9-5] rd[4-0].
  kA64Sbfm4rrdd,     // sbfm[0001001100] imm_r[21-16] imm_s[15-10] rn[9-5] rd[4-0].
  kA64Scvtf2fw,      // scvtf  [000111100s100010000000] rn[9-5] rd[4-0].
  kA64Scvtf2fx,      // scvtf  [100111100s100010000000] rn[9-5] rd[4-0].
  kA64Sdiv3rrr,      // sdiv[s0011010110] rm[20-16] [000011] rn[9-5] rd[4-0].
  kA64Smull3xww,     // smull [10011011001] rm[20-16] [011111] rn[9-5] rd[4-0].
  kA64Smulh3xxx,     // smulh [10011011010] rm[20-16] [011111] rn[9-5] rd[4-0].
  kA64Stp4ffXD,      // stp [0s10110100] imm_7[21-15] rt2[14-10] rn[9-5] rt[4-0].
  kA64Stp4rrXD,      // stp [s010100100] imm_7[21-15] rt2[14-10] rn[9-5] rt[4-0].
  kA64StpPost4rrXD,  // stp [s010100010] imm_7[21-15] rt2[14-10] rn[9-5] rt[4-0].
  kA64StpPre4ffXD,   // stp [0s10110110] imm_7[21-15] rt2[14-10] rn[9-5] rt[4-0].
  kA64StpPre4rrXD,   // stp [s010100110] imm_7[21-15] rt2[14-10] rn[9-5] rt[4-0].
  kA64Str3fXD,       // str [1s11110100] imm_12[21-10] rn[9-5] rt[4-0].
  kA64Str4fXxG,      // str [1s111100001] rm[20-16] [011] S[12] [10] rn[9-5] rt[4-0].
  kA64Str3rXD,       // str [1s11100100] imm_12[21-10] rn[9-5] rt[4-0].
  kA64Str4rXxG,      // str [1s111000001] rm[20-16] option[15-13] S[12-12] [10] rn[9-5] rt[4-0].
  kA64Strb3wXd,      // strb[0011100100] imm_12[21-10] rn[9-5] rt[4-0].
  kA64Strb3wXx,      // strb[00111000001] rm[20-16] [011] S[12] [10] rn[9-5] rt[4-0].
  kA64Strh3wXF,      // strh[0111100100] imm_12[21-10] rn[9-5] rt[4-0].
  kA64Strh4wXxd,     // strh[01111000001] rm[20-16] [011] S[12] [10] rn[9-5] rt[4-0].
  kA64StrPost3rXd,   // str [1s111000000] imm_9[20-12] [01] rn[9-5] rt[4-0].
  kA64Stur3fXd,      // stur[1s111100000] imm_9[20-12] [00] rn[9-5] rt[4-0].
  kA64Stur3rXd,      // stur[1s111000000] imm_9[20-12] [00] rn[9-5] rt[4-0].
  kA64Stxr3wrX,      // stxr[11001000000] rs[20-16] [011111] rn[9-5] rt[4-0].
  kA64Stlxr3wrX,     // stlxr[11001000000] rs[20-16] [111111] rn[9-5] rt[4-0].
  kA64Sub4RRdT,      // sub [s101000100] imm_12[21-10] rn[9-5] rd[4-0].
  kA64Sub4rrro,      // sub [s1001011000] rm[20-16] imm_6[15-10] rn[9-5] rd[4-0].
  kA64Sub4RRre,      // sub [s1001011001] rm[20-16] option[15-13] imm_3[12-10] rn[9-5] rd[4-0].
  kA64Subs3rRd,      // subs[s111000100] imm_12[21-10] rn[9-5] rd[4-0].
  kA64Tst2rl,        // tst alias of "ands rzr, rn, #imm".
  kA64Tst3rro,       // tst alias of "ands rzr, arg1, arg2, arg3".
  kA64Tbnz3rht,      // tbnz imm_6_b5[31] [0110111] imm_6_b40[23-19] imm_14[18-5] rt[4-0].
  kA64Tbz3rht,       // tbz imm_6_b5[31] [0110110] imm_6_b40[23-19] imm_14[18-5] rt[4-0].
  kA64Ubfm4rrdd,     // ubfm[s10100110] N[22] imm_r[21-16] imm_s[15-10] rn[9-5] rd[4-0].
// EXYNOS_ART_OPT start
  kA64SimdDup64QX,   // dup 64-bit register value to entire Q register
  kA64SimdDup32QR,   // dup 32-bit register value to entire Q register
  kA64SimdDup16QR,   // dup 16-bit register value to entire Q register
  kA64SimdDup8QR,    // dup 8-bit register value to entire Q register
  kA64SimdDup32DR,   // dup 32-bit register value to entire D register
  kA64SimdDup16DR,   // dup 16-bit register value to entire D register
  kA64SimdDup8DR,    // dup 8-bit register value to entire D register
  kA64SimdVdup32QD0,
  kA64SimdVdup32QD1,
  kA64SimdVmov64QI,
  kA64SimdVmov32QI,
  kA64SimdVmov16QI,
  kA64SimdVmov8QI,
  kA64SimdVmov32DI,
  kA64SimdVmov16DI,
  kA64SimdVmov8DI,
  kA64SimdVmovQQ,
  kA64SimdVmovDD,
  kA64SimdVmovF64DD,
  kA64SimdVmovF32SS,
  kA64SimdMovD0X,
  kA64SimdMovD1X,
  kA64SimdMovS0W,
  kA64SimdMovS1W,
  kA64SimdMovS2W,
  kA64SimdMovS3W,
  kA64SimdMovH0W,
  kA64SimdMovH1W,
  kA64SimdMovH2W,
  kA64SimdMovH3W,
  kA64SimdMovH4W,
  kA64SimdMovH5W,
  kA64SimdMovH6W,
  kA64SimdMovH7W,
  kA64SimdMovB0W,
  kA64SimdMovB1W,
  kA64SimdMovB2W,
  kA64SimdMovB3W,
  kA64SimdMovB4W,
  kA64SimdMovB5W,
  kA64SimdMovB6W,
  kA64SimdMovB7W,
  kA64SimdMovB8W,
  kA64SimdMovB9W,
  kA64SimdMovB10W,
  kA64SimdMovB11W,
  kA64SimdMovB12W,
  kA64SimdMovB13W,
  kA64SimdMovB14W,
  kA64SimdMovB15W,
  kA64SimdMovXD0,
  kA64SimdMovXD1,
  kA64SimdMovWS0,
  kA64SimdMovWS1,
  kA64SimdMovDD0,
  kA64SimdMovDD1,
  kA64SimdMovSS0,
  kA64SimdMovSS1,
  kA64SimdMovSS2,
  kA64SimdMovSS3,
  kA64SimdSmovWB0,
  kA64SimdSmovWH0,
  kA64SimdSmovXB0,
  kA64SimdSmovXH0,
  kA64SimdSmovXS0,
  kA64SimdUmovWB0,
  kA64SimdUmovWH0,
  kA64SimdVextQQQI,
  kA64SimdVextDDDI,
  kA64SimdAdd64QQQ,  // Add 2 x 64-bit values in Q register (add v0.2d,v0.2d,v0.2d)
  kA64SimdAdd32QQQ,  // Add 4 x 32-bit values in Q register (add v0.4s,v0.4s,v0.4s)
  kA64SimdAdd16QQQ,  // Add 8 x 16-bit values in Q register (add v0.8h,v0.8h,v0.8h)
  kA64SimdAdd8QQQ,   // Add 16 x 8-bit values in Q register (add v0.16b,v0.16b,v0.16b)
  kA64SimdAdd32DDD,  // Add 2 x 32-bit values in D register (add v0.2s,v0.2s,v0.2s)
  kA64SimdAdd16DDD,  // Add 4 x 16-bit values in D register (add v0.4h,v0.4h,v0.4h)
  kA64SimdAdd8DDD,   // Add 8 x 8-bit values in D register  (add v0.8b,v0.8b,v0.8b)
  kA64SimdFAdd64QQQ, // FAdd 2 x 64-bit double-precision floating point values in Q register (fadd v0.2d,v0.2d,v0.2d)
  kA64SimdFAdd32QQQ, // FAdd 4 x 32-bit single-precision floating point values in Q register (fadd v0.4s,v0.4s,v0.4s)
  kA64SimdFAdd32DDD, // FAdd 2 x 32-bit single-precision floating point values in D register (fadd v0.2s,v0.2s,v0.2s)
  kA64SimdSaddlp2D4S,
  kA64SimdSaddlp4S8H,
  kA64SimdSaddlp8H16B,
  kA64SimdSaddlp1D2S,
  kA64SimdSaddlp2S4H,
  kA64SimdSaddlp4H8B,
  kA64SimdAddp64QQQ,
  kA64SimdAddp32QQQ,
  kA64SimdAddp16QQQ,
  kA64SimdAddp8QQQ,
  kA64SimdAddp32DDD,
  kA64SimdAddp16DDD,
  kA64SimdAddp8DDD,
  kA64SimdFaddp64QQQ,
  kA64SimdFaddp32QQQ,
  kA64SimdFaddp32DDD,
  kA64SimdMul32QQQ,  // Mul 4 x 32-bit values in Q register (mul v0.4s,v0.4s,v0.4s)
  kA64SimdMul16QQQ,  // Mul 8 x 16-bit values in Q register (mul v0.8h,v0.8h,v0.8h)
  kA64SimdMul8QQQ,   // Mul 16 x 8-bit values in Q register (mul v0.16b,v0.16b,v0.16b)
  kA64SimdMul32DDD,  // Mul 2 x 32-bit values in D register (mul v0.2s,v0.2s,v0.2s)
  kA64SimdMul16DDD,  // Mul 4 x 16-bit values in D register (mul v0.4h,v0.4h,v0.4h)
  kA64SimdMul8DDD,   // Mul 8 x 8-bit values in D register  (mul v0.8b,v0.8b,v0.8b)
  kA64SimdFMul64QQQ, // FMul 2 x 64-bit double-precision floating point values in Q register (fadd v0.2d,v0.2d,v0.2d)
  kA64SimdFMul32QQQ, // FMul 2 x 32-bit single-precision floating point values in D register (fadd v0.2s,v0.2s,v0.2s)
  kA64SimdFMul32DDD, // FMul 2 x 32-bit single-precision floating point values in D register (fadd v0.2s,v0.2s,v0.2s)
  kA64SimdVandQQQ,
  kA64SimdVandDDD,
// EXYNOS_ART_OPT end
  kA64Last,
  kA64NotWide = kA64First,  // 0 - Flag used to select the first instruction variant.
  kA64Wide = 0x1000         // Flag used to select the second instruction variant.
};
std::ostream& operator<<(std::ostream& os, const A64Opcode& rhs);
#else
enum A64Opcode {
  kA64First = 0,
  kA64Adc3rrr = kA64First,  // adc [00011010000] rm[20-16] [000000] rn[9-5] rd[4-0].
  kA64Add4RRdT,      // add [s001000100] imm_12[21-10] rn[9-5] rd[4-0].
  kA64Add4rrro,      // add [00001011000] rm[20-16] imm_6[15-10] rn[9-5] rd[4-0].
  kA64Add4RRre,      // add [00001011001] rm[20-16] option[15-13] imm_3[12-10] rn[9-5] rd[4-0].
  kA64Adr2xd,        // adr [0] immlo[30-29] [10000] immhi[23-5] rd[4-0].
  kA64Adrp2xd,       // adrp [1] immlo[30-29] [10000] immhi[23-5] rd[4-0].
  kA64And3Rrl,       // and [00010010] N[22] imm_r[21-16] imm_s[15-10] rn[9-5] rd[4-0].
  kA64And4rrro,      // and [00001010] shift[23-22] [N=0] rm[20-16] imm_6[15-10] rn[9-5] rd[4-0].
  kA64Asr3rrd,       // asr [0001001100] immr[21-16] imms[15-10] rn[9-5] rd[4-0].
  kA64Asr3rrr,       // asr alias of "sbfm arg0, arg1, arg2, {#31/#63}".
  kA64B2ct,          // b.cond [01010100] imm_19[23-5] [0] cond[3-0].
  kA64Blr1x,         // blr [1101011000111111000000] rn[9-5] [00000].
  kA64Br1x,          // br  [1101011000011111000000] rn[9-5] [00000].
  kA64Bl1t,          // bl  [100101] imm26[25-0].
  kA64Brk1d,         // brk [11010100001] imm_16[20-5] [00000].
  kA64B1t,           // b   [00010100] offset_26[25-0].
  kA64Cbnz2rt,       // cbnz[00110101] imm_19[23-5] rt[4-0].
  kA64Cbz2rt,        // cbz [00110100] imm_19[23-5] rt[4-0].
  kA64Cmn3rro,       // cmn [s0101011] shift[23-22] [0] rm[20-16] imm_6[15-10] rn[9-5] [11111].
  kA64Cmn3Rre,       // cmn [s0101011001] rm[20-16] option[15-13] imm_3[12-10] rn[9-5] [11111].
  kA64Cmn3RdT,       // cmn [00110001] shift[23-22] imm_12[21-10] rn[9-5] [11111].
  kA64Cmp3rro,       // cmp [s1101011] shift[23-22] [0] rm[20-16] imm_6[15-10] rn[9-5] [11111].
  kA64Cmp3Rre,       // cmp [s1101011001] rm[20-16] option[15-13] imm_3[12-10] rn[9-5] [11111].
  kA64Cmp3RdT,       // cmp [01110001] shift[23-22] imm_12[21-10] rn[9-5] [11111].
  kA64Csel4rrrc,     // csel[s0011010100] rm[20-16] cond[15-12] [00] rn[9-5] rd[4-0].
  kA64Csinc4rrrc,    // csinc [s0011010100] rm[20-16] cond[15-12] [01] rn[9-5] rd[4-0].
  kA64Csinv4rrrc,    // csinv [s1011010100] rm[20-16] cond[15-12] [00] rn[9-5] rd[4-0].
  kA64Csneg4rrrc,    // csneg [s1011010100] rm[20-16] cond[15-12] [01] rn[9-5] rd[4-0].
  kA64Dmb1B,         // dmb [11010101000000110011] CRm[11-8] [10111111].
  kA64Eor3Rrl,       // eor [s10100100] N[22] imm_r[21-16] imm_s[15-10] rn[9-5] rd[4-0].
  kA64Eor4rrro,      // eor [s1001010] shift[23-22] [0] rm[20-16] imm_6[15-10] rn[9-5] rd[4-0].
  kA64Extr4rrrd,     // extr[s00100111N0] rm[20-16] imm_s[15-10] rn[9-5] rd[4-0].
  kA64Fabs2ff,       // fabs[000111100s100000110000] rn[9-5] rd[4-0].
  kA64Fadd3fff,      // fadd[000111100s1] rm[20-16] [001010] rn[9-5] rd[4-0].
  kA64Fcmp1f,        // fcmp[000111100s100000001000] rn[9-5] [01000].
  kA64Fcmp2ff,       // fcmp[000111100s1] rm[20-16] [001000] rn[9-5] [00000].
  kA64Fcvtzs2wf,     // fcvtzs [000111100s111000000000] rn[9-5] rd[4-0].
  kA64Fcvtzs2xf,     // fcvtzs [100111100s111000000000] rn[9-5] rd[4-0].
  kA64Fcvt2Ss,       // fcvt   [0001111000100010110000] rn[9-5] rd[4-0].
  kA64Fcvt2sS,       // fcvt   [0001111001100010010000] rn[9-5] rd[4-0].
  kA64Fcvtms2ws,     // fcvtms [0001111000110000000000] rn[9-5] rd[4-0].
  kA64Fcvtms2xS,     // fcvtms [1001111001110000000000] rn[9-5] rd[4-0].
  kA64Fdiv3fff,      // fdiv[000111100s1] rm[20-16] [000110] rn[9-5] rd[4-0].
  kA64Fmax3fff,      // fmax[000111100s1] rm[20-16] [010010] rn[9-5] rd[4-0].
  kA64Fmin3fff,      // fmin[000111100s1] rm[20-16] [010110] rn[9-5] rd[4-0].
  kA64Fmov2ff,       // fmov[000111100s100000010000] rn[9-5] rd[4-0].
  kA64Fmov2fI,       // fmov[000111100s1] imm_8[20-13] [10000000] rd[4-0].
  kA64Fmov2sw,       // fmov[0001111000100111000000] rn[9-5] rd[4-0].
  kA64Fmov2Sx,       // fmov[1001111001100111000000] rn[9-5] rd[4-0].
  kA64Fmov2ws,       // fmov[0001111001101110000000] rn[9-5] rd[4-0].
  kA64Fmov2xS,       // fmov[1001111001101111000000] rn[9-5] rd[4-0].
  kA64Fmul3fff,      // fmul[000111100s1] rm[20-16] [000010] rn[9-5] rd[4-0].
  kA64Fneg2ff,       // fneg[000111100s100001010000] rn[9-5] rd[4-0].
  kA64Frintp2ff,     // frintp [000111100s100100110000] rn[9-5] rd[4-0].
  kA64Frintm2ff,     // frintm [000111100s100101010000] rn[9-5] rd[4-0].
  kA64Frintn2ff,     // frintn [000111100s100100010000] rn[9-5] rd[4-0].
  kA64Frintz2ff,     // frintz [000111100s100101110000] rn[9-5] rd[4-0].
  kA64Fsqrt2ff,      // fsqrt[000111100s100001110000] rn[9-5] rd[4-0].
  kA64Fsub3fff,      // fsub[000111100s1] rm[20-16] [001110] rn[9-5] rd[4-0].
  kA64Ldrb3wXd,      // ldrb[0011100101] imm_12[21-10] rn[9-5] rt[4-0].
  kA64Ldrb3wXx,      // ldrb[00111000011] rm[20-16] [011] S[12] [10] rn[9-5] rt[4-0].
  kA64Ldrsb3rXd,     // ldrsb[001110011s] imm_12[21-10] rn[9-5] rt[4-0].
  kA64Ldrsb3rXx,     // ldrsb[0011 1000 1s1] rm[20-16] [011] S[12] [10] rn[9-5] rt[4-0].
  kA64Ldrh3wXF,      // ldrh[0111100101] imm_12[21-10] rn[9-5] rt[4-0].
  kA64Ldrh4wXxd,     // ldrh[01111000011] rm[20-16] [011] S[12] [10] rn[9-5] rt[4-0].
  kA64Ldrsh3rXF,     // ldrsh[011110011s] imm_12[21-10] rn[9-5] rt[4-0].
  kA64Ldrsh4rXxd,    // ldrsh[011110001s1] rm[20-16] [011] S[12] [10] rn[9-5] rt[4-0]
  kA64Ldr2fp,        // ldr [0s011100] imm_19[23-5] rt[4-0].
  kA64Ldr2rp,        // ldr [0s011000] imm_19[23-5] rt[4-0].
  kA64Ldr3fXD,       // ldr [1s11110100] imm_12[21-10] rn[9-5] rt[4-0].
  kA64Ldr3rXD,       // ldr [1s111000010] imm_9[20-12] [01] rn[9-5] rt[4-0].
  kA64Ldr4fXxG,      // ldr [1s111100011] rm[20-16] [011] S[12] [10] rn[9-5] rt[4-0].
  kA64Ldr4rXxG,      // ldr [1s111000011] rm[20-16] [011] S[12] [10] rn[9-5] rt[4-0].
  kA64LdrPost3rXd,   // ldr [1s111000010] imm_9[20-12] [01] rn[9-5] rt[4-0].
  kA64Ldp4ffXD,      // ldp [0s10110101] imm_7[21-15] rt2[14-10] rn[9-5] rt[4-0].
  kA64Ldp4rrXD,      // ldp [s010100101] imm_7[21-15] rt2[14-10] rn[9-5] rt[4-0].
  kA64LdpPost4rrXD,  // ldp [s010100011] imm_7[21-15] rt2[14-10] rn[9-5] rt[4-0].
  kA64Ldur3fXd,      // ldur[1s111100010] imm_9[20-12] [00] rn[9-5] rt[4-0].
  kA64Ldur3rXd,      // ldur[1s111000010] imm_9[20-12] [00] rn[9-5] rt[4-0].
  kA64Ldxr2rX,       // ldxr[1s00100001011111011111] rn[9-5] rt[4-0].
  kA64Ldaxr2rX,      // ldaxr[1s00100001011111111111] rn[9-5] rt[4-0].
  kA64Lsl3rrr,       // lsl [s0011010110] rm[20-16] [001000] rn[9-5] rd[4-0].
  kA64Lsr3rrd,       // lsr alias of "ubfm arg0, arg1, arg2, #{31/63}".
  kA64Lsr3rrr,       // lsr [s0011010110] rm[20-16] [001001] rn[9-5] rd[4-0].
  kA64Madd4rrrr,     // madd[s0011011000] rm[20-16] [0] ra[14-10] rn[9-5] rd[4-0].
  kA64Movk3rdM,      // mov [010100101] hw[22-21] imm_16[20-5] rd[4-0].
  kA64Movn3rdM,      // mov [000100101] hw[22-21] imm_16[20-5] rd[4-0].
  kA64Movz3rdM,      // mov [011100101] hw[22-21] imm_16[20-5] rd[4-0].
  kA64Mov2rr,        // mov [00101010000] rm[20-16] [000000] [11111] rd[4-0].
  kA64Mvn2rr,        // mov [00101010001] rm[20-16] [000000] [11111] rd[4-0].
  kA64Mul3rrr,       // mul [00011011000] rm[20-16] [011111] rn[9-5] rd[4-0].
  kA64Msub4rrrr,     // msub[s0011011000] rm[20-16] [1] ra[14-10] rn[9-5] rd[4-0].
  kA64Neg3rro,       // neg alias of "sub arg0, rzr, arg1, arg2".
  kA64Nop0,          // nop alias of "hint #0" [11010101000000110010000000011111].
  kA64Orr3Rrl,       // orr [s01100100] N[22] imm_r[21-16] imm_s[15-10] rn[9-5] rd[4-0].
  kA64Orr4rrro,      // orr [s0101010] shift[23-22] [0] rm[20-16] imm_6[15-10] rn[9-5] rd[4-0].
  kA64Ret,           // ret [11010110010111110000001111000000].
  kA64Rbit2rr,       // rbit [s101101011000000000000] rn[9-5] rd[4-0].
  kA64Rev2rr,        // rev [s10110101100000000001x] rn[9-5] rd[4-0].
  kA64Rev162rr,      // rev16[s101101011000000000001] rn[9-5] rd[4-0].
  kA64Ror3rrr,       // ror [s0011010110] rm[20-16] [001011] rn[9-5] rd[4-0].
  kA64Sbc3rrr,       // sbc [s0011010000] rm[20-16] [000000] rn[9-5] rd[4-0].
  kA64Sbfm4rrdd,     // sbfm[0001001100] imm_r[21-16] imm_s[15-10] rn[9-5] rd[4-0].
  kA64Scvtf2fw,      // scvtf  [000111100s100010000000] rn[9-5] rd[4-0].
  kA64Scvtf2fx,      // scvtf  [100111100s100010000000] rn[9-5] rd[4-0].
  kA64Sdiv3rrr,      // sdiv[s0011010110] rm[20-16] [000011] rn[9-5] rd[4-0].
  kA64Smull3xww,     // smull [10011011001] rm[20-16] [011111] rn[9-5] rd[4-0].
  kA64Smulh3xxx,     // smulh [10011011010] rm[20-16] [011111] rn[9-5] rd[4-0].
  kA64Stp4ffXD,      // stp [0s10110100] imm_7[21-15] rt2[14-10] rn[9-5] rt[4-0].
  kA64Stp4rrXD,      // stp [s010100100] imm_7[21-15] rt2[14-10] rn[9-5] rt[4-0].
  kA64StpPost4rrXD,  // stp [s010100010] imm_7[21-15] rt2[14-10] rn[9-5] rt[4-0].
  kA64StpPre4ffXD,   // stp [0s10110110] imm_7[21-15] rt2[14-10] rn[9-5] rt[4-0].
  kA64StpPre4rrXD,   // stp [s010100110] imm_7[21-15] rt2[14-10] rn[9-5] rt[4-0].
  kA64Str3fXD,       // str [1s11110100] imm_12[21-10] rn[9-5] rt[4-0].
  kA64Str4fXxG,      // str [1s111100001] rm[20-16] [011] S[12] [10] rn[9-5] rt[4-0].
  kA64Str3rXD,       // str [1s11100100] imm_12[21-10] rn[9-5] rt[4-0].
  kA64Str4rXxG,      // str [1s111000001] rm[20-16] option[15-13] S[12-12] [10] rn[9-5] rt[4-0].
  kA64Strb3wXd,      // strb[0011100100] imm_12[21-10] rn[9-5] rt[4-0].
  kA64Strb3wXx,      // strb[00111000001] rm[20-16] [011] S[12] [10] rn[9-5] rt[4-0].
  kA64Strh3wXF,      // strh[0111100100] imm_12[21-10] rn[9-5] rt[4-0].
  kA64Strh4wXxd,     // strh[01111000001] rm[20-16] [011] S[12] [10] rn[9-5] rt[4-0].
  kA64StrPost3rXd,   // str [1s111000000] imm_9[20-12] [01] rn[9-5] rt[4-0].
  kA64Stur3fXd,      // stur[1s111100000] imm_9[20-12] [00] rn[9-5] rt[4-0].
  kA64Stur3rXd,      // stur[1s111000000] imm_9[20-12] [00] rn[9-5] rt[4-0].
  kA64Stxr3wrX,      // stxr[11001000000] rs[20-16] [011111] rn[9-5] rt[4-0].
  kA64Stlxr3wrX,     // stlxr[11001000000] rs[20-16] [111111] rn[9-5] rt[4-0].
  kA64Sub4RRdT,      // sub [s101000100] imm_12[21-10] rn[9-5] rd[4-0].
  kA64Sub4rrro,      // sub [s1001011000] rm[20-16] imm_6[15-10] rn[9-5] rd[4-0].
  kA64Sub4RRre,      // sub [s1001011001] rm[20-16] option[15-13] imm_3[12-10] rn[9-5] rd[4-0].
  kA64Subs3rRd,      // subs[s111000100] imm_12[21-10] rn[9-5] rd[4-0].
  kA64Tst2rl,        // tst alias of "ands rzr, rn, #imm".
  kA64Tst3rro,       // tst alias of "ands rzr, arg1, arg2, arg3".
  kA64Tbnz3rht,      // tbnz imm_6_b5[31] [0110111] imm_6_b40[23-19] imm_14[18-5] rt[4-0].
  kA64Tbz3rht,       // tbz imm_6_b5[31] [0110110] imm_6_b40[23-19] imm_14[18-5] rt[4-0].
  kA64Ubfm4rrdd,     // ubfm[s10100110] N[22] imm_r[21-16] imm_s[15-10] rn[9-5] rd[4-0].
  kA64Last,
  kA64NotWide = kA64First,  // 0 - Flag used to select the first instruction variant.
  kA64Wide = 0x1000         // Flag used to select the second instruction variant.
};
std::ostream& operator<<(std::ostream& os, const A64Opcode& rhs);
#endif
/*
 * The A64 instruction set provides two variants for many instructions. For example, "mov wN, wM"
 * and "mov xN, xM" or - for floating point instructions - "mov sN, sM" and "mov dN, dM".
 * It definitely makes sense to exploit this symmetries of the instruction set. We do this via the
 * WIDE, UNWIDE macros. For opcodes that allow it, the wide variant can be obtained by applying the
 * WIDE macro to the non-wide opcode. E.g. WIDE(kA64Sub4RRdT).
 */

// Return the wide and no-wide variants of the given opcode.
#define WIDE(op) ((A64Opcode)((op) | kA64Wide))
#define UNWIDE(op) ((A64Opcode)((op) & ~kA64Wide))

// Whether the given opcode is wide.
#define IS_WIDE(op) (((op) & kA64Wide) != 0)

enum A64OpDmbOptions {
  kSY = 0xf,
  kST = 0xe,
  kISH = 0xb,
  kISHST = 0xa,
  kISHLD = 0x9,
  kNSH = 0x7,
  kNSHST = 0x6
};

// Instruction assembly field_loc kind.
#ifdef EXYNOS_ART_OPT
enum A64EncodingKind {
  // All the formats below are encoded in the same way (as a kFmtBitBlt).
  // These are grouped together, for fast handling (e.g. "if (LIKELY(fmt <= kFmtBitBlt)) ...").
  kFmtRegW = 0,   // Word register (w) or wzr.
  kFmtRegX,       // Extended word register (x) or xzr.
  kFmtRegR,       // Register with same width as the instruction or zr.
  kFmtRegWOrSp,   // Word register (w) or wsp.
  kFmtRegXOrSp,   // Extended word register (x) or sp.
  kFmtRegROrSp,   // Register with same width as the instruction or sp.
  kFmtRegS,       // Single FP reg.
  kFmtRegD,       // Double FP reg.
  kFmtRegF,       // Single/double FP reg depending on the instruction width.
  kFmtBitBlt,     // Bit string using end/start.

  // Less likely formats.
  kFmtUnused,     // Unused field and marks end of formats.
  kFmtImm6Shift,  // Shift immediate, 6-bit at [31, 23..19].
  kFmtImm21,      // Sign-extended immediate using [23..5,30..29].
  kFmtShift,      // Register shift, 9-bit at [23..21, 15..10]..
  kFmtExtend,     // Register extend, 9-bit at [23..21, 15..10].
// EXYNOS_ART_OPT start
  kFmtSimdSd,    // SIMD  operand (Operand d)
  kFmtSimdSn,    // SIMD  (Operand n)
  kFmtSimdSm,    // SIMD  (Operand m)
  kFmtSimdI8M,   // SIMD vmov immedate operand, [18..16, 9..5] cmode : [15-12]
// EXYNOS_ART_OPT end
  kFmtSkip,       // Unused field, but continue to next.
};
std::ostream& operator<<(std::ostream& os, const A64EncodingKind & rhs);
#else
enum A64EncodingKind {
  // All the formats below are encoded in the same way (as a kFmtBitBlt).
  // These are grouped together, for fast handling (e.g. "if (LIKELY(fmt <= kFmtBitBlt)) ...").
  kFmtRegW = 0,   // Word register (w) or wzr.
  kFmtRegX,       // Extended word register (x) or xzr.
  kFmtRegR,       // Register with same width as the instruction or zr.
  kFmtRegWOrSp,   // Word register (w) or wsp.
  kFmtRegXOrSp,   // Extended word register (x) or sp.
  kFmtRegROrSp,   // Register with same width as the instruction or sp.
  kFmtRegS,       // Single FP reg.
  kFmtRegD,       // Double FP reg.
  kFmtRegF,       // Single/double FP reg depending on the instruction width.
  kFmtBitBlt,     // Bit string using end/start.

  // Less likely formats.
  kFmtUnused,     // Unused field and marks end of formats.
  kFmtImm6Shift,  // Shift immediate, 6-bit at [31, 23..19].
  kFmtImm21,      // Sign-extended immediate using [23..5,30..29].
  kFmtShift,      // Register shift, 9-bit at [23..21, 15..10]..
  kFmtExtend,     // Register extend, 9-bit at [23..21, 15..10].
  kFmtSkip,       // Unused field, but continue to next.
};
std::ostream& operator<<(std::ostream& os, const A64EncodingKind & rhs);
#endif

// Struct used to define the snippet positions for each A64 opcode.
struct A64EncodingMap {
  uint32_t wskeleton;
  uint32_t xskeleton;
  struct {
    A64EncodingKind kind;
    int end;         // end for kFmtBitBlt, 1-bit slice end for FP regs.
    int start;       // start for kFmtBitBlt, 4-bit slice end for FP regs.
  } field_loc[4];
  A64Opcode opcode;  // can be WIDE()-ned to indicate it has a wide variant.
  uint64_t flags;
  const char* name;
  const char* fmt;
  int size;          // Note: size is in bytes.
  FixupKind fixup;
};

}  // namespace art

#endif  // ART_COMPILER_DEX_QUICK_ARM64_ARM64_LIR_H_
