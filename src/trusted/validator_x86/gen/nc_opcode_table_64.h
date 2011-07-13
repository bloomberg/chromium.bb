/*
 * THIS FILE IS AUTO-GENERATED. DO NOT EDIT.
 * Compiled for x86-64 bit mode.
 *
 * You must include ncopcode_desc.h before this file.
 */

static const NaClOp g_Operands[727] = {
  /* 0 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Eb" },
  /* 1 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gb" },
  /* 2 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$Ev" },
  /* 3 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gv" },
  /* 4 */ { G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Gb" },
  /* 5 */ { E_Operand, NACL_OPFLAG(OpUse), "$Eb" },
  /* 6 */ { G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$Gv" },
  /* 7 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ev" },
  /* 8 */ { RegAL, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%al" },
  /* 9 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 10 */ { RegREAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$rAXv" },
  /* 11 */ { I_Operand, NACL_OPFLAG(OpUse), "$Iz" },
  /* 12 */ { E_Operand, NACL_OPFLAG(OpUse), "$Eb" },
  /* 13 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gb" },
  /* 14 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ev" },
  /* 15 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gv" },
  /* 16 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gb" },
  /* 17 */ { E_Operand, NACL_OPFLAG(OpUse), "$Eb" },
  /* 18 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gv" },
  /* 19 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ev" },
  /* 20 */ { RegAL, NACL_OPFLAG(OpUse), "%al" },
  /* 21 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 22 */ { RegREAX, NACL_OPFLAG(OpUse), "$rAXv" },
  /* 23 */ { I_Operand, NACL_OPFLAG(OpUse), "$Iz" },
  /* 24 */ { RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rsp}" },
  /* 25 */ { G_OpcodeBase, NACL_OPFLAG(OpUse), "$r8v" },
  /* 26 */ { RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rsp}" },
  /* 27 */ { G_OpcodeBase, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$r8v" },
  /* 28 */ { G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$Gv" },
  /* 29 */ { Ev_Operand, NACL_OPFLAG(OpUse), "$Ed" },
  /* 30 */ { RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rsp}" },
  /* 31 */ { I_Operand, NACL_OPFLAG(OpUse), "$Iz" },
  /* 32 */ { G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$Gv" },
  /* 33 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ev" },
  /* 34 */ { I_Operand, NACL_OPFLAG(OpUse), "$Iz" },
  /* 35 */ { RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rsp}" },
  /* 36 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 37 */ { G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$Gv" },
  /* 38 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ev" },
  /* 39 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 40 */ { RegES_EDI, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$Yb}" },
  /* 41 */ { RegDX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%dx}" },
  /* 42 */ { RegES_EDI, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$Yzw}" },
  /* 43 */ { RegDX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%dx}" },
  /* 44 */ { RegES_EDI, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$Yzd}" },
  /* 45 */ { RegDX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%dx}" },
  /* 46 */ { RegDX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%dx}" },
  /* 47 */ { RegDS_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Xb}" },
  /* 48 */ { RegDX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%dx}" },
  /* 49 */ { RegES_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Xzw}" },
  /* 50 */ { RegDX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%dx}" },
  /* 51 */ { RegES_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Xzd}" },
  /* 52 */ { RegRIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rip}" },
  /* 53 */ { J_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandNear) | NACL_OPFLAG(OperandRelative), "$Jb" },
  /* 54 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Eb" },
  /* 55 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 56 */ { E_Operand, NACL_OPFLAG(OpUse), "$Eb" },
  /* 57 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 58 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$Ev" },
  /* 59 */ { I_Operand, NACL_OPFLAG(OpUse), "$Iz" },
  /* 60 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$Ev" },
  /* 61 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 62 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Eb" },
  /* 63 */ { G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Gb" },
  /* 64 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$Ev" },
  /* 65 */ { G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$Gv" },
  /* 66 */ { E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Eb" },
  /* 67 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gb" },
  /* 68 */ { E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$Ev" },
  /* 69 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gv" },
  /* 70 */ { G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Gb" },
  /* 71 */ { E_Operand, NACL_OPFLAG(OpUse), "$Eb" },
  /* 72 */ { E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$Mw/Rv" },
  /* 73 */ { S_Operand, NACL_OPFLAG(OpUse), "$Sw" },
  /* 74 */ { G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OperandZeroExtends_v), "$Gv" },
  /* 75 */ { M_Operand, NACL_OPFLAG(OpAddress), "$M" },
  /* 76 */ { S_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Sw" },
  /* 77 */ { Ew_Operand, NACL_OPFLAG(OpUse), "$Ew" },
  /* 78 */ { RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rsp}" },
  /* 79 */ { E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Ev" },
  /* 80 */ { G_OpcodeBase, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$r8v" },
  /* 81 */ { RegREAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$rAXv" },
  /* 82 */ { RegAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%ax}" },
  /* 83 */ { RegAL, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%al}" },
  /* 84 */ { RegEAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit) | NACL_OPFLAG(OperandSignExtends_v), "{%eax}" },
  /* 85 */ { RegAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%ax}" },
  /* 86 */ { RegRAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rax}" },
  /* 87 */ { RegEAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%eax}" },
  /* 88 */ { RegDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%dx}" },
  /* 89 */ { RegAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%ax}" },
  /* 90 */ { RegEDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%edx}" },
  /* 91 */ { RegEAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%eax}" },
  /* 92 */ { RegRDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rdx}" },
  /* 93 */ { RegRAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%rax}" },
  /* 94 */ { RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rsp}" },
  /* 95 */ { RegRFLAGS, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Fvw}" },
  /* 96 */ { RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rsp}" },
  /* 97 */ { RegRFLAGS, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Fvq}" },
  /* 98 */ { RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rsp}" },
  /* 99 */ { RegRFLAGS, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$Fvw}" },
  /* 100 */ { RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rsp}" },
  /* 101 */ { RegRFLAGS, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$Fvq}" },
  /* 102 */ { RegAH, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%ah}" },
  /* 103 */ { RegAH, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%ah}" },
  /* 104 */ { RegAL, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%al" },
  /* 105 */ { O_Operand, NACL_OPFLAG(OpUse), "$Ob" },
  /* 106 */ { RegREAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$rAXv" },
  /* 107 */ { O_Operand, NACL_OPFLAG(OpUse), "$Ov" },
  /* 108 */ { O_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Ob" },
  /* 109 */ { RegAL, NACL_OPFLAG(OpUse), "%al" },
  /* 110 */ { O_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$Ov" },
  /* 111 */ { RegREAX, NACL_OPFLAG(OpUse), "$rAXv" },
  /* 112 */ { RegES_EDI, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$Yb}" },
  /* 113 */ { RegDS_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Xb}" },
  /* 114 */ { RegES_EDI, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$Yvw}" },
  /* 115 */ { RegDS_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Xvw}" },
  /* 116 */ { RegES_EDI, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$Yvd}" },
  /* 117 */ { RegDS_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Xvd}" },
  /* 118 */ { RegES_EDI, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$Yvq}" },
  /* 119 */ { RegDS_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Xvq}" },
  /* 120 */ { RegDS_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Xb}" },
  /* 121 */ { RegES_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Yb}" },
  /* 122 */ { RegDS_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Xvw}" },
  /* 123 */ { RegES_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Yvw}" },
  /* 124 */ { RegDS_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Xvd}" },
  /* 125 */ { RegES_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Yvd}" },
  /* 126 */ { RegDS_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Xvq}" },
  /* 127 */ { RegES_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Yvq}" },
  /* 128 */ { RegES_EDI, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$Yb}" },
  /* 129 */ { RegAL, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%al}" },
  /* 130 */ { RegES_EDI, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$Yvw}" },
  /* 131 */ { RegAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$rAXvw}" },
  /* 132 */ { RegES_EDI, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$Yvd}" },
  /* 133 */ { RegEAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$rAXvd}" },
  /* 134 */ { RegES_EDI, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$Yvq}" },
  /* 135 */ { RegRAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$rAXvq}" },
  /* 136 */ { RegAL, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%al}" },
  /* 137 */ { RegDS_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Xb}" },
  /* 138 */ { RegAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$rAXvw}" },
  /* 139 */ { RegDS_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Xvw}" },
  /* 140 */ { RegEAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$rAXvd}" },
  /* 141 */ { RegDS_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Xvd}" },
  /* 142 */ { RegRAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$rAXvq}" },
  /* 143 */ { RegDS_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Xvq}" },
  /* 144 */ { RegAL, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%al}" },
  /* 145 */ { RegES_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Yb}" },
  /* 146 */ { RegAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$rAXvw}" },
  /* 147 */ { RegES_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Yvw}" },
  /* 148 */ { RegEAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$rAXvd}" },
  /* 149 */ { RegES_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Yvd}" },
  /* 150 */ { RegRAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$rAXvq}" },
  /* 151 */ { RegES_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Yvq}" },
  /* 152 */ { G_OpcodeBase, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$r8b" },
  /* 153 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 154 */ { G_OpcodeBase, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$r8v" },
  /* 155 */ { I_Operand, NACL_OPFLAG(OpUse), "$Iv" },
  /* 156 */ { RegRIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rip}" },
  /* 157 */ { RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rsp}" },
  /* 158 */ { I_Operand, NACL_OPFLAG(OpUse), "$Iw" },
  /* 159 */ { E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Eb" },
  /* 160 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 161 */ { E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$Ev" },
  /* 162 */ { I_Operand, NACL_OPFLAG(OpUse), "$Iz" },
  /* 163 */ { RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rsp}" },
  /* 164 */ { RegRBP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rbp}" },
  /* 165 */ { I_Operand, NACL_OPFLAG(OpUse), "$Iw" },
  /* 166 */ { I2_Operand, NACL_OPFLAG(OpUse), "$I2b" },
  /* 167 */ { RegRSP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rsp}" },
  /* 168 */ { RegRBP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rbp}" },
  /* 169 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Eb" },
  /* 170 */ { Const_1, NACL_OPFLAG(OpUse), "1" },
  /* 171 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$Ev" },
  /* 172 */ { Const_1, NACL_OPFLAG(OpUse), "1" },
  /* 173 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Eb" },
  /* 174 */ { RegCL, NACL_OPFLAG(OpUse), "%cl" },
  /* 175 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$Ev" },
  /* 176 */ { RegCL, NACL_OPFLAG(OpUse), "%cl" },
  /* 177 */ { RegAL, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%al}" },
  /* 178 */ { RegDS_EBX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%DS_EBX}" },
  /* 179 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 180 */ { Mv_Operand, NACL_OPFLAG(OpUse), "$Md" },
  /* 181 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 182 */ { Mv_Operand, NACL_OPFLAG(OpUse), "$Md" },
  /* 183 */ { RegST0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 184 */ { Mv_Operand, NACL_OPFLAG(OpUse), "$Md" },
  /* 185 */ { Mv_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Md" },
  /* 186 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 187 */ { M_Operand, NACL_OPFLAG(OpUse), "$M" },
  /* 188 */ { Mw_Operand, NACL_OPFLAG(OpUse), "$Mw" },
  /* 189 */ { M_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$M" },
  /* 190 */ { Mw_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Mw" },
  /* 191 */ { RegST0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 192 */ { M_Operand, NACL_OPFLAG(OpUse), "$M" },
  /* 193 */ { M_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$M" },
  /* 194 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 195 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 196 */ { Mo_Operand, NACL_OPFLAG(OpUse), "$Mq" },
  /* 197 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 198 */ { Mo_Operand, NACL_OPFLAG(OpUse), "$Mq" },
  /* 199 */ { RegST0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 200 */ { Mo_Operand, NACL_OPFLAG(OpUse), "$Mq" },
  /* 201 */ { Mo_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Mq" },
  /* 202 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 203 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 204 */ { Mw_Operand, NACL_OPFLAG(OpUse), "$Mw" },
  /* 205 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 206 */ { Mw_Operand, NACL_OPFLAG(OpUse), "$Mw" },
  /* 207 */ { RegST0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 208 */ { Mw_Operand, NACL_OPFLAG(OpUse), "$Mw" },
  /* 209 */ { Mw_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Mw" },
  /* 210 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 211 */ { RegRIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rip}" },
  /* 212 */ { RegECX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%ecx}" },
  /* 213 */ { J_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandNear) | NACL_OPFLAG(OperandRelative), "$Jb" },
  /* 214 */ { RegRIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rip}" },
  /* 215 */ { RegRCX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%rcx}" },
  /* 216 */ { J_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandNear) | NACL_OPFLAG(OperandRelative), "$Jb" },
  /* 217 */ { RegAL, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%al" },
  /* 218 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 219 */ { RegREAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$rAXv" },
  /* 220 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 221 */ { I_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Ib" },
  /* 222 */ { RegAL, NACL_OPFLAG(OpUse), "%al" },
  /* 223 */ { I_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Ib" },
  /* 224 */ { RegREAX, NACL_OPFLAG(OpUse), "$rAXv" },
  /* 225 */ { RegRIP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rip}" },
  /* 226 */ { RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rsp}" },
  /* 227 */ { J_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandNear) | NACL_OPFLAG(OperandRelative), "$Jzw" },
  /* 228 */ { RegRIP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rip}" },
  /* 229 */ { RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rsp}" },
  /* 230 */ { J_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandNear) | NACL_OPFLAG(OperandRelative), "$Jz" },
  /* 231 */ { RegRIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rip}" },
  /* 232 */ { J_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandNear) | NACL_OPFLAG(OperandRelative), "$Jz" },
  /* 233 */ { RegAL, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%al" },
  /* 234 */ { RegDX, NACL_OPFLAG(OpUse), "%dx" },
  /* 235 */ { RegREAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$rAXv" },
  /* 236 */ { RegDX, NACL_OPFLAG(OpUse), "%dx" },
  /* 237 */ { RegDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%dx" },
  /* 238 */ { RegAL, NACL_OPFLAG(OpUse), "%al" },
  /* 239 */ { RegDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%dx" },
  /* 240 */ { RegREAX, NACL_OPFLAG(OpUse), "$rAXv" },
  /* 241 */ { RegAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%ax}" },
  /* 242 */ { RegAL, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%al}" },
  /* 243 */ { E_Operand, NACL_OPFLAG(OpUse), "$Eb" },
  /* 244 */ { RegREDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%redx}" },
  /* 245 */ { RegREAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%reax}" },
  /* 246 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ev" },
  /* 247 */ { RegRIP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rip}" },
  /* 248 */ { RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rsp}" },
  /* 249 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandNear), "$Ev" },
  /* 250 */ { RegRIP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rip}" },
  /* 251 */ { RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rsp}" },
  /* 252 */ { M_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandFar), "$Mp" },
  /* 253 */ { RegRIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rip}" },
  /* 254 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandNear), "$Ev" },
  /* 255 */ { RegRIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rip}" },
  /* 256 */ { M_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandFar), "$Mp" },
  /* 257 */ { RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rsp}" },
  /* 258 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ev" },
  /* 259 */ { E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Mw/Rv" },
  /* 260 */ { Ew_Operand, NACL_EMPTY_OPFLAGS, "$Ew" },
  /* 261 */ { M_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Ms" },
  /* 262 */ { RegREAX, NACL_OPFLAG(OpUse), "%reax" },
  /* 263 */ { RegECX, NACL_OPFLAG(OpUse), "%ecx" },
  /* 264 */ { RegEDX, NACL_OPFLAG(OpUse), "%edx" },
  /* 265 */ { RegEAX, NACL_EMPTY_OPFLAGS, "%eax" },
  /* 266 */ { RegECX, NACL_EMPTY_OPFLAGS, "%ecx" },
  /* 267 */ { M_Operand, NACL_OPFLAG(OpUse), "$Ms" },
  /* 268 */ { RegREAXa, NACL_OPFLAG(OpUse), "$rAXva" },
  /* 269 */ { RegEIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eip}" },
  /* 270 */ { RegEAX, NACL_OPFLAG(OpUse), "%eax" },
  /* 271 */ { RegREAXa, NACL_OPFLAG(OpUse), "$rAXva" },
  /* 272 */ { RegECX, NACL_OPFLAG(OpUse), "%ecx" },
  /* 273 */ { Mb_Operand, NACL_OPFLAG(OpUse), "$Mb" },
  /* 274 */ { RegGS, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%gs}" },
  /* 275 */ { RegRDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rdx}" },
  /* 276 */ { RegRAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rax}" },
  /* 277 */ { RegRCX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rcx}" },
  /* 278 */ { G_Operand, NACL_EMPTY_OPFLAGS, "$Gv" },
  /* 279 */ { Ew_Operand, NACL_EMPTY_OPFLAGS, "$Ew" },
  /* 280 */ { RegRIP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rip}" },
  /* 281 */ { RegRCX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rcx}" },
  /* 282 */ { Mb_Operand, NACL_EMPTY_OPFLAGS, "$Mb" },
  /* 283 */ { Mmx_G_Operand, NACL_EMPTY_OPFLAGS, "$Pq" },
  /* 284 */ { Mmx_E_Operand, NACL_EMPTY_OPFLAGS, "$Qq" },
  /* 285 */ { I_Operand, NACL_EMPTY_OPFLAGS, "$Ib" },
  /* 286 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vps" },
  /* 287 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wps" },
  /* 288 */ { Xmm_E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Wps" },
  /* 289 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vps" },
  /* 290 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vps" },
  /* 291 */ { Mo_Operand, NACL_OPFLAG(OpUse), "$Mq" },
  /* 292 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vps" },
  /* 293 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$VRq" },
  /* 294 */ { Mo_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Mq" },
  /* 295 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vps" },
  /* 296 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vps" },
  /* 297 */ { Xmm_Eo_Operand, NACL_OPFLAG(OpUse), "$Wq" },
  /* 298 */ { Eo_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Rd/q" },
  /* 299 */ { C_Operand, NACL_OPFLAG(OpUse), "$Cd/q" },
  /* 300 */ { Eo_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Rd/q" },
  /* 301 */ { D_Operand, NACL_OPFLAG(OpUse), "$Dd/q" },
  /* 302 */ { C_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Cd/q" },
  /* 303 */ { Eo_Operand, NACL_OPFLAG(OpUse), "$Rd/q" },
  /* 304 */ { D_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Dd/q" },
  /* 305 */ { Eo_Operand, NACL_OPFLAG(OpUse), "$Rd/q" },
  /* 306 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vps" },
  /* 307 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse), "$Qq" },
  /* 308 */ { Mdq_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Mdq" },
  /* 309 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vps" },
  /* 310 */ { Mmx_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Pq" },
  /* 311 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wps" },
  /* 312 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vss" },
  /* 313 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wss" },
  /* 314 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vps" },
  /* 315 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wps" },
  /* 316 */ { RegEAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%eax}" },
  /* 317 */ { RegEDX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%edx}" },
  /* 318 */ { RegECX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%ecx}" },
  /* 319 */ { RegEAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eax}" },
  /* 320 */ { RegEDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%edx}" },
  /* 321 */ { RegEAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eax}" },
  /* 322 */ { RegEDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%edx}" },
  /* 323 */ { RegECX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%ecx}" },
  /* 324 */ { RegEIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eip}" },
  /* 325 */ { RegESP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 326 */ { RegCS, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%cs}" },
  /* 327 */ { RegSS, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%ss}" },
  /* 328 */ { RegEIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eip}" },
  /* 329 */ { RegESP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 330 */ { RegCS, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%cs}" },
  /* 331 */ { RegSS, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%ss}" },
  /* 332 */ { RegEDX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%edx}" },
  /* 333 */ { RegECX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%ecx}" },
  /* 334 */ { Gv_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Gd" },
  /* 335 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$VRps" },
  /* 336 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vps" },
  /* 337 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wps" },
  /* 338 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vpd" },
  /* 339 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wps" },
  /* 340 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vps" },
  /* 341 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 342 */ { Mmx_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Pq" },
  /* 343 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse), "$Qq" },
  /* 344 */ { Mmx_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Pq" },
  /* 345 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse), "$Qd" },
  /* 346 */ { Mmx_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$Pq" },
  /* 347 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ed/q/d" },
  /* 348 */ { Mmx_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Pq" },
  /* 349 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ed/q/q" },
  /* 350 */ { Mmx_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Pq" },
  /* 351 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse), "$Qq" },
  /* 352 */ { Mmx_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Pq" },
  /* 353 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse), "$Qq" },
  /* 354 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 355 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$PRq" },
  /* 356 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 357 */ { E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$Ed/q/d" },
  /* 358 */ { Mmx_G_Operand, NACL_OPFLAG(OpUse), "$Pd/q/d" },
  /* 359 */ { E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Ed/q/q" },
  /* 360 */ { Mmx_G_Operand, NACL_OPFLAG(OpUse), "$Pd/q/q" },
  /* 361 */ { Mmx_E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Qq" },
  /* 362 */ { Mmx_G_Operand, NACL_OPFLAG(OpUse), "$Pq" },
  /* 363 */ { RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rsp}" },
  /* 364 */ { RegFS, NACL_OPFLAG(OpUse), "%fs" },
  /* 365 */ { RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rsp}" },
  /* 366 */ { RegFS, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%fs" },
  /* 367 */ { RegEBX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%ebx}" },
  /* 368 */ { RegEDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%edx}" },
  /* 369 */ { RegEAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eax}" },
  /* 370 */ { RegECX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%ecx}" },
  /* 371 */ { E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$Ev" },
  /* 372 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gv" },
  /* 373 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 374 */ { E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$Ev" },
  /* 375 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gv" },
  /* 376 */ { RegCL, NACL_OPFLAG(OpUse), "%cl" },
  /* 377 */ { RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rsp}" },
  /* 378 */ { RegGS, NACL_OPFLAG(OpUse), "%gs" },
  /* 379 */ { RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rsp}" },
  /* 380 */ { RegGS, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%gs" },
  /* 381 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$Ev" },
  /* 382 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gv" },
  /* 383 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 384 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$Ev" },
  /* 385 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gv" },
  /* 386 */ { RegCL, NACL_OPFLAG(OpUse), "%cl" },
  /* 387 */ { RegAL, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%al}" },
  /* 388 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Eb" },
  /* 389 */ { G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Gb" },
  /* 390 */ { RegREAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$rAXv}" },
  /* 391 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$Ev" },
  /* 392 */ { G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OperandZeroExtends_v), "$Gv" },
  /* 393 */ { Seg_G_Operand, NACL_OPFLAG(OpSet), "$SGz" },
  /* 394 */ { M_Operand, NACL_OPFLAG(OperandFar), "$Mp" },
  /* 395 */ { G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$Gv" },
  /* 396 */ { Eb_Operand, NACL_OPFLAG(OpUse), "$Eb" },
  /* 397 */ { G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$Gv" },
  /* 398 */ { Ew_Operand, NACL_OPFLAG(OpUse), "$Ew" },
  /* 399 */ { G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Gv" },
  /* 400 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ev" },
  /* 401 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vps" },
  /* 402 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wps" },
  /* 403 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 404 */ { M_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Md/q" },
  /* 405 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gd/q" },
  /* 406 */ { Mmx_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Pq" },
  /* 407 */ { E_Operand, NACL_OPFLAG(OpUse), "$Rd/q/Mw" },
  /* 408 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 409 */ { Gv_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Gd" },
  /* 410 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse), "$PRq" },
  /* 411 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 412 */ { RegEDX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%edx}" },
  /* 413 */ { RegEAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eax}" },
  /* 414 */ { Mo_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Mq" },
  /* 415 */ { RegRDX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rdx}" },
  /* 416 */ { RegEAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eax}" },
  /* 417 */ { Mdq_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Mdq" },
  /* 418 */ { Mo_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Mq" },
  /* 419 */ { Mmx_G_Operand, NACL_OPFLAG(OpUse), "$Pq" },
  /* 420 */ { RegDS_EDI, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$Xvd}" },
  /* 421 */ { Mmx_G_Operand, NACL_OPFLAG(OpUse), "$Pq" },
  /* 422 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse), "$PRq" },
  /* 423 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vsd" },
  /* 424 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wsd" },
  /* 425 */ { Xmm_E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Wsd" },
  /* 426 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vsd" },
  /* 427 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vpd" },
  /* 428 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wsd" },
  /* 429 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vsd" },
  /* 430 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ed/q" },
  /* 431 */ { Mo_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Mq" },
  /* 432 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vsd" },
  /* 433 */ { G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Gd/q" },
  /* 434 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wsd" },
  /* 435 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vsd" },
  /* 436 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wsd" },
  /* 437 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vss" },
  /* 438 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wsd" },
  /* 439 */ { Xmm_Go_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vq" },
  /* 440 */ { Xmm_Eo_Operand, NACL_OPFLAG(OpUse), "$Wq" },
  /* 441 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 442 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 443 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$VRq" },
  /* 444 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 445 */ { I2_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 446 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 447 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$VRdq" },
  /* 448 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vsd" },
  /* 449 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wsd" },
  /* 450 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 451 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vpd" },
  /* 452 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wpd" },
  /* 453 */ { Mmx_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Pq" },
  /* 454 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$VRq" },
  /* 455 */ { Xmm_Go_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vq" },
  /* 456 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wpd" },
  /* 457 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 458 */ { Mdq_Operand, NACL_OPFLAG(OpUse), "$Mdq" },
  /* 459 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vss" },
  /* 460 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wss" },
  /* 461 */ { Xmm_E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Wss" },
  /* 462 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vss" },
  /* 463 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vss" },
  /* 464 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ed/q" },
  /* 465 */ { Mv_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Md" },
  /* 466 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vss" },
  /* 467 */ { G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Gd/q" },
  /* 468 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wss" },
  /* 469 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vss" },
  /* 470 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wss" },
  /* 471 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vsd" },
  /* 472 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wss" },
  /* 473 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 474 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wps" },
  /* 475 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 476 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 477 */ { Xmm_E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Wdq" },
  /* 478 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 479 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vss" },
  /* 480 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wss" },
  /* 481 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 482 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 483 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse), "$PRq" },
  /* 484 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vpd" },
  /* 485 */ { Xmm_Eo_Operand, NACL_OPFLAG(OpUse), "$Wq" },
  /* 486 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vpd" },
  /* 487 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wpd" },
  /* 488 */ { Xmm_E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Wpd" },
  /* 489 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vpd" },
  /* 490 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vsd" },
  /* 491 */ { Mo_Operand, NACL_OPFLAG(OpUse), "$Mq" },
  /* 492 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vpd" },
  /* 493 */ { Xmm_Eo_Operand, NACL_OPFLAG(OpUse), "$Wq" },
  /* 494 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vpd" },
  /* 495 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse), "$Qq" },
  /* 496 */ { Mdq_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Mdq" },
  /* 497 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vpd" },
  /* 498 */ { Mmx_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Pq" },
  /* 499 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wpd" },
  /* 500 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vsd" },
  /* 501 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wsd" },
  /* 502 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vpd" },
  /* 503 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wsd" },
  /* 504 */ { Gv_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Gd" },
  /* 505 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$VRpd" },
  /* 506 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vps" },
  /* 507 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wpd" },
  /* 508 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 509 */ { Xmm_Eo_Operand, NACL_OPFLAG(OpUse), "$Wq" },
  /* 510 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 511 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 512 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$Vdq" },
  /* 513 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ed/q/d" },
  /* 514 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 515 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ed/q/q" },
  /* 516 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 517 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 518 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 519 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$VRdq" },
  /* 520 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 521 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(AllowGOperandWithOpcodeInModRm), "$Vdq" },
  /* 522 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 523 */ { I2_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 524 */ { E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$Ed/q/d" },
  /* 525 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vd/q/d" },
  /* 526 */ { E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Ed/q/q" },
  /* 527 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vd/q/q" },
  /* 528 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vpd" },
  /* 529 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wpd" },
  /* 530 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 531 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 532 */ { E_Operand, NACL_OPFLAG(OpUse), "$Rd/q/Mw" },
  /* 533 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 534 */ { Gv_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Gd" },
  /* 535 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$VRdq" },
  /* 536 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 537 */ { Xmm_Eo_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Wq" },
  /* 538 */ { Xmm_Go_Operand, NACL_OPFLAG(OpUse), "$Vq" },
  /* 539 */ { Xmm_Go_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vq" },
  /* 540 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 541 */ { Mdq_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Mdq" },
  /* 542 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 543 */ { RegDS_EDI, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$Xvd}" },
  /* 544 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 545 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$VRdq" },
  /* 546 */ { G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Gv" },
  /* 547 */ { M_Operand, NACL_OPFLAG(OpUse), "$Mv" },
  /* 548 */ { M_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Mv" },
  /* 549 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gv" },
  /* 550 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 551 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 552 */ { RegXMM0, NACL_OPFLAG(OpUse), "%xmm0" },
  /* 553 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 554 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 555 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 556 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Udq/Mq" },
  /* 557 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 558 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Udq/Md" },
  /* 559 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 560 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Udq/Mw" },
  /* 561 */ { Go_Operand, NACL_OPFLAG(OpUse), "$Gq" },
  /* 562 */ { Mdq_Operand, NACL_OPFLAG(OpUse), "$Mdq" },
  /* 563 */ { Gv_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Gd" },
  /* 564 */ { E_Operand, NACL_OPFLAG(OpUse), "$Eb" },
  /* 565 */ { Gv_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Gd" },
  /* 566 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ev" },
  /* 567 */ { Mmx_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Pq" },
  /* 568 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse), "$Qq" },
  /* 569 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 570 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vss" },
  /* 571 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wss" },
  /* 572 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 573 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vsd" },
  /* 574 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wsd" },
  /* 575 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 576 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 577 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 578 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 579 */ { Ev_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Rd/Mb" },
  /* 580 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 581 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 582 */ { Ev_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Rd/Mw" },
  /* 583 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 584 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 585 */ { E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Ed/q/d" },
  /* 586 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 587 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 588 */ { E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Ed/q/q" },
  /* 589 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 590 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 591 */ { Ev_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Ed" },
  /* 592 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 593 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 594 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 595 */ { E_Operand, NACL_OPFLAG(OpUse), "$Rd/q/Mb" },
  /* 596 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 597 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 598 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Udq/Md" },
  /* 599 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 600 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 601 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ed/q/d" },
  /* 602 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 603 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 604 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ed/q/q" },
  /* 605 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 606 */ { RegXMM0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%xmm0}" },
  /* 607 */ { RegREAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$rAXv}" },
  /* 608 */ { RegREDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$rDXv}" },
  /* 609 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 610 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 611 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 612 */ { RegRECX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$rCXv}" },
  /* 613 */ { RegREAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$rAXv}" },
  /* 614 */ { RegREDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$rDXv}" },
  /* 615 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 616 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 617 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 618 */ { RegXMM0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%xmm0}" },
  /* 619 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 620 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 621 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 622 */ { RegRECX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$rCXv}" },
  /* 623 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 624 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 625 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 626 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 627 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 628 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 629 */ { RegST1, NACL_OPFLAG(OpUse), "%st1" },
  /* 630 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 631 */ { RegST2, NACL_OPFLAG(OpUse), "%st2" },
  /* 632 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 633 */ { RegST3, NACL_OPFLAG(OpUse), "%st3" },
  /* 634 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 635 */ { RegST4, NACL_OPFLAG(OpUse), "%st4" },
  /* 636 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 637 */ { RegST5, NACL_OPFLAG(OpUse), "%st5" },
  /* 638 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 639 */ { RegST6, NACL_OPFLAG(OpUse), "%st6" },
  /* 640 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 641 */ { RegST7, NACL_OPFLAG(OpUse), "%st7" },
  /* 642 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 643 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 644 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 645 */ { RegST1, NACL_OPFLAG(OpUse), "%st1" },
  /* 646 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 647 */ { RegST2, NACL_OPFLAG(OpUse), "%st2" },
  /* 648 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 649 */ { RegST3, NACL_OPFLAG(OpUse), "%st3" },
  /* 650 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 651 */ { RegST4, NACL_OPFLAG(OpUse), "%st4" },
  /* 652 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 653 */ { RegST5, NACL_OPFLAG(OpUse), "%st5" },
  /* 654 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 655 */ { RegST6, NACL_OPFLAG(OpUse), "%st6" },
  /* 656 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 657 */ { RegST7, NACL_OPFLAG(OpUse), "%st7" },
  /* 658 */ { RegST0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 659 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 660 */ { RegST0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 661 */ { RegST1, NACL_OPFLAG(OpUse), "%st1" },
  /* 662 */ { RegST0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 663 */ { RegST2, NACL_OPFLAG(OpUse), "%st2" },
  /* 664 */ { RegST0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 665 */ { RegST3, NACL_OPFLAG(OpUse), "%st3" },
  /* 666 */ { RegST0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 667 */ { RegST4, NACL_OPFLAG(OpUse), "%st4" },
  /* 668 */ { RegST0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 669 */ { RegST5, NACL_OPFLAG(OpUse), "%st5" },
  /* 670 */ { RegST0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 671 */ { RegST6, NACL_OPFLAG(OpUse), "%st6" },
  /* 672 */ { RegST0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 673 */ { RegST7, NACL_OPFLAG(OpUse), "%st7" },
  /* 674 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 675 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 676 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 677 */ { RegST1, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st1" },
  /* 678 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 679 */ { RegST2, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st2" },
  /* 680 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 681 */ { RegST3, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st3" },
  /* 682 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 683 */ { RegST4, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st4" },
  /* 684 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 685 */ { RegST5, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st5" },
  /* 686 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 687 */ { RegST6, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st6" },
  /* 688 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 689 */ { RegST7, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st7" },
  /* 690 */ { RegST1, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st1" },
  /* 691 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 692 */ { RegST2, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st2" },
  /* 693 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 694 */ { RegST3, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st3" },
  /* 695 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 696 */ { RegST4, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st4" },
  /* 697 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 698 */ { RegST5, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st5" },
  /* 699 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 700 */ { RegST6, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st6" },
  /* 701 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 702 */ { RegST7, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st7" },
  /* 703 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 704 */ { RegST0, NACL_EMPTY_OPFLAGS, "%st0" },
  /* 705 */ { RegST1, NACL_EMPTY_OPFLAGS, "%st1" },
  /* 706 */ { RegST2, NACL_EMPTY_OPFLAGS, "%st2" },
  /* 707 */ { RegST3, NACL_EMPTY_OPFLAGS, "%st3" },
  /* 708 */ { RegST4, NACL_EMPTY_OPFLAGS, "%st4" },
  /* 709 */ { RegST5, NACL_EMPTY_OPFLAGS, "%st5" },
  /* 710 */ { RegST6, NACL_EMPTY_OPFLAGS, "%st6" },
  /* 711 */ { RegST7, NACL_EMPTY_OPFLAGS, "%st7" },
  /* 712 */ { RegST1, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st1" },
  /* 713 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 714 */ { RegST2, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st2" },
  /* 715 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 716 */ { RegST3, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st3" },
  /* 717 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 718 */ { RegST4, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st4" },
  /* 719 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 720 */ { RegST5, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st5" },
  /* 721 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 722 */ { RegST6, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st6" },
  /* 723 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 724 */ { RegST7, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st7" },
  /* 725 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 726 */ { RegAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%ax" },
};

static const NaClInst g_Opcodes[2253] = {
  /* 0 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstAdd, 0x00,
    2, g_Operands + 0,
    NULL
  },
  /* 1 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstAdd, 0x00,
    2, g_Operands + 2,
    NULL
  },
  /* 2 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstAdd, 0x00,
    2, g_Operands + 4,
    NULL
  },
  /* 3 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstAdd, 0x00,
    2, g_Operands + 6,
    NULL
  },
  /* 4 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable),
    InstAdd, 0x00,
    2, g_Operands + 8,
    NULL
  },
  /* 5 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstAdd, 0x00,
    2, g_Operands + 10,
    NULL
  },
  /* 6 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 7 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 8 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstOr, 0x00,
    2, g_Operands + 0,
    NULL
  },
  /* 9 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstOr, 0x00,
    2, g_Operands + 2,
    NULL
  },
  /* 10 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstOr, 0x00,
    2, g_Operands + 4,
    NULL
  },
  /* 11 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstOr, 0x00,
    2, g_Operands + 6,
    NULL
  },
  /* 12 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable),
    InstOr, 0x00,
    2, g_Operands + 8,
    NULL
  },
  /* 13 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstOr, 0x00,
    2, g_Operands + 10,
    NULL
  },
  /* 14 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 15 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 16 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstAdc, 0x00,
    2, g_Operands + 0,
    NULL
  },
  /* 17 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstAdc, 0x00,
    2, g_Operands + 2,
    NULL
  },
  /* 18 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstAdc, 0x00,
    2, g_Operands + 4,
    NULL
  },
  /* 19 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstAdc, 0x00,
    2, g_Operands + 6,
    NULL
  },
  /* 20 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable),
    InstAdc, 0x00,
    2, g_Operands + 8,
    NULL
  },
  /* 21 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstAdc, 0x00,
    2, g_Operands + 10,
    NULL
  },
  /* 22 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 23 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 24 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstSbb, 0x00,
    2, g_Operands + 0,
    NULL
  },
  /* 25 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstSbb, 0x00,
    2, g_Operands + 2,
    NULL
  },
  /* 26 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstSbb, 0x00,
    2, g_Operands + 4,
    NULL
  },
  /* 27 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstSbb, 0x00,
    2, g_Operands + 6,
    NULL
  },
  /* 28 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable),
    InstSbb, 0x00,
    2, g_Operands + 8,
    NULL
  },
  /* 29 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstSbb, 0x00,
    2, g_Operands + 10,
    NULL
  },
  /* 30 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 31 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 32 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstAnd, 0x00,
    2, g_Operands + 0,
    NULL
  },
  /* 33 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstAnd, 0x00,
    2, g_Operands + 2,
    NULL
  },
  /* 34 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstAnd, 0x00,
    2, g_Operands + 4,
    NULL
  },
  /* 35 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstAnd, 0x00,
    2, g_Operands + 6,
    NULL
  },
  /* 36 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable),
    InstAnd, 0x00,
    2, g_Operands + 8,
    NULL
  },
  /* 37 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstAnd, 0x00,
    2, g_Operands + 10,
    NULL
  },
  /* 38 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 39 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 40 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstSub, 0x00,
    2, g_Operands + 0,
    NULL
  },
  /* 41 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstSub, 0x00,
    2, g_Operands + 2,
    NULL
  },
  /* 42 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstSub, 0x00,
    2, g_Operands + 4,
    NULL
  },
  /* 43 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstSub, 0x00,
    2, g_Operands + 6,
    NULL
  },
  /* 44 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable),
    InstSub, 0x00,
    2, g_Operands + 8,
    NULL
  },
  /* 45 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstSub, 0x00,
    2, g_Operands + 10,
    NULL
  },
  /* 46 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 47 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 48 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstXor, 0x00,
    2, g_Operands + 0,
    NULL
  },
  /* 49 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstXor, 0x00,
    2, g_Operands + 2,
    NULL
  },
  /* 50 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstXor, 0x00,
    2, g_Operands + 4,
    NULL
  },
  /* 51 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstXor, 0x00,
    2, g_Operands + 6,
    NULL
  },
  /* 52 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable),
    InstXor, 0x00,
    2, g_Operands + 8,
    NULL
  },
  /* 53 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstXor, 0x00,
    2, g_Operands + 10,
    NULL
  },
  /* 54 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 55 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 56 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstCmp, 0x00,
    2, g_Operands + 12,
    NULL
  },
  /* 57 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCmp, 0x00,
    2, g_Operands + 14,
    NULL
  },
  /* 58 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstCmp, 0x00,
    2, g_Operands + 16,
    NULL
  },
  /* 59 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCmp, 0x00,
    2, g_Operands + 18,
    NULL
  },
  /* 60 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b),
    InstCmp, 0x00,
    2, g_Operands + 20,
    NULL
  },
  /* 61 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCmp, 0x00,
    2, g_Operands + 22,
    NULL
  },
  /* 62 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 63 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 64 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPush, 0x00,
    2, g_Operands + 24,
    NULL
  },
  /* 65 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPush, 0x01,
    2, g_Operands + 24,
    NULL
  },
  /* 66 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPush, 0x02,
    2, g_Operands + 24,
    NULL
  },
  /* 67 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPush, 0x03,
    2, g_Operands + 24,
    NULL
  },
  /* 68 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPush, 0x04,
    2, g_Operands + 24,
    NULL
  },
  /* 69 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPush, 0x05,
    2, g_Operands + 24,
    NULL
  },
  /* 70 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPush, 0x06,
    2, g_Operands + 24,
    NULL
  },
  /* 71 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPush, 0x07,
    2, g_Operands + 24,
    NULL
  },
  /* 72 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPop, 0x00,
    2, g_Operands + 26,
    NULL
  },
  /* 73 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPop, 0x01,
    2, g_Operands + 26,
    NULL
  },
  /* 74 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPop, 0x02,
    2, g_Operands + 26,
    NULL
  },
  /* 75 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPop, 0x03,
    2, g_Operands + 26,
    NULL
  },
  /* 76 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPop, 0x04,
    2, g_Operands + 26,
    NULL
  },
  /* 77 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPop, 0x05,
    2, g_Operands + 26,
    NULL
  },
  /* 78 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPop, 0x06,
    2, g_Operands + 26,
    NULL
  },
  /* 79 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPop, 0x07,
    2, g_Operands + 26,
    NULL
  },
  /* 80 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 81 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 82 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 83 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(LongMode),
    InstMovsxd, 0x00,
    2, g_Operands + 28,
    NULL
  },
  /* 84 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 85 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 86 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 87 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 88 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPush, 0x00,
    2, g_Operands + 30,
    NULL
  },
  /* 89 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstImul, 0x00,
    3, g_Operands + 32,
    NULL
  },
  /* 90 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPush, 0x00,
    2, g_Operands + 35,
    NULL
  },
  /* 91 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstImul, 0x00,
    3, g_Operands + 37,
    NULL
  },
  /* 92 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(NaClIllegal),
    InstInsb, 0x00,
    2, g_Operands + 40,
    NULL
  },
  /* 93 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(NaClIllegal),
    InstInsw, 0x00,
    2, g_Operands + 42,
    g_Opcodes + 94
  },
  /* 94 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal),
    InstInsd, 0x00,
    2, g_Operands + 44,
    NULL
  },
  /* 95 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(NaClIllegal),
    InstOutsb, 0x00,
    2, g_Operands + 46,
    NULL
  },
  /* 96 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(NaClIllegal),
    InstOutsw, 0x00,
    2, g_Operands + 48,
    g_Opcodes + 97
  },
  /* 97 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal),
    InstOutsd, 0x00,
    2, g_Operands + 50,
    NULL
  },
  /* 98 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump),
    InstJo, 0x00,
    2, g_Operands + 52,
    NULL
  },
  /* 99 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump),
    InstJno, 0x00,
    2, g_Operands + 52,
    NULL
  },
  /* 100 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump),
    InstJb, 0x00,
    2, g_Operands + 52,
    NULL
  },
  /* 101 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump),
    InstJnb, 0x00,
    2, g_Operands + 52,
    NULL
  },
  /* 102 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump),
    InstJz, 0x00,
    2, g_Operands + 52,
    NULL
  },
  /* 103 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump),
    InstJnz, 0x00,
    2, g_Operands + 52,
    NULL
  },
  /* 104 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump),
    InstJbe, 0x00,
    2, g_Operands + 52,
    NULL
  },
  /* 105 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump),
    InstJnbe, 0x00,
    2, g_Operands + 52,
    NULL
  },
  /* 106 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump),
    InstJs, 0x00,
    2, g_Operands + 52,
    NULL
  },
  /* 107 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump),
    InstJns, 0x00,
    2, g_Operands + 52,
    NULL
  },
  /* 108 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump),
    InstJp, 0x00,
    2, g_Operands + 52,
    NULL
  },
  /* 109 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump),
    InstJnp, 0x00,
    2, g_Operands + 52,
    NULL
  },
  /* 110 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump),
    InstJl, 0x00,
    2, g_Operands + 52,
    NULL
  },
  /* 111 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump),
    InstJnl, 0x00,
    2, g_Operands + 52,
    NULL
  },
  /* 112 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump),
    InstJle, 0x00,
    2, g_Operands + 52,
    NULL
  },
  /* 113 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump),
    InstJnle, 0x00,
    2, g_Operands + 52,
    NULL
  },
  /* 114 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstAdd, 0x00,
    2, g_Operands + 54,
    g_Opcodes + 115
  },
  /* 115 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstOr, 0x01,
    2, g_Operands + 54,
    g_Opcodes + 116
  },
  /* 116 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstAdc, 0x02,
    2, g_Operands + 54,
    g_Opcodes + 117
  },
  /* 117 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstSbb, 0x03,
    2, g_Operands + 54,
    g_Opcodes + 118
  },
  /* 118 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstAnd, 0x04,
    2, g_Operands + 54,
    g_Opcodes + 119
  },
  /* 119 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstSub, 0x05,
    2, g_Operands + 54,
    g_Opcodes + 120
  },
  /* 120 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstXor, 0x06,
    2, g_Operands + 54,
    g_Opcodes + 121
  },
  /* 121 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstCmp, 0x07,
    2, g_Operands + 56,
    NULL
  },
  /* 122 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstAdd, 0x00,
    2, g_Operands + 58,
    g_Opcodes + 123
  },
  /* 123 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstOr, 0x01,
    2, g_Operands + 58,
    g_Opcodes + 124
  },
  /* 124 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstAdc, 0x02,
    2, g_Operands + 58,
    g_Opcodes + 125
  },
  /* 125 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstSbb, 0x03,
    2, g_Operands + 58,
    g_Opcodes + 126
  },
  /* 126 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstAnd, 0x04,
    2, g_Operands + 58,
    g_Opcodes + 127
  },
  /* 127 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstSub, 0x05,
    2, g_Operands + 58,
    g_Opcodes + 128
  },
  /* 128 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstXor, 0x06,
    2, g_Operands + 58,
    g_Opcodes + 129
  },
  /* 129 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCmp, 0x07,
    2, g_Operands + 33,
    NULL
  },
  /* 130 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    g_Opcodes + 131
  },
  /* 131 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x01,
    0, g_Operands + 0,
    g_Opcodes + 132
  },
  /* 132 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x02,
    0, g_Operands + 0,
    g_Opcodes + 133
  },
  /* 133 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x03,
    0, g_Operands + 0,
    g_Opcodes + 134
  },
  /* 134 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x04,
    0, g_Operands + 0,
    g_Opcodes + 135
  },
  /* 135 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x05,
    0, g_Operands + 0,
    g_Opcodes + 136
  },
  /* 136 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x06,
    0, g_Operands + 0,
    g_Opcodes + 137
  },
  /* 137 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x07,
    0, g_Operands + 0,
    NULL
  },
  /* 138 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstAdd, 0x00,
    2, g_Operands + 60,
    g_Opcodes + 139
  },
  /* 139 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstOr, 0x01,
    2, g_Operands + 60,
    g_Opcodes + 140
  },
  /* 140 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstAdc, 0x02,
    2, g_Operands + 60,
    g_Opcodes + 141
  },
  /* 141 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstSbb, 0x03,
    2, g_Operands + 60,
    g_Opcodes + 142
  },
  /* 142 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstAnd, 0x04,
    2, g_Operands + 60,
    g_Opcodes + 143
  },
  /* 143 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstSub, 0x05,
    2, g_Operands + 60,
    g_Opcodes + 144
  },
  /* 144 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstXor, 0x06,
    2, g_Operands + 60,
    g_Opcodes + 145
  },
  /* 145 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCmp, 0x07,
    2, g_Operands + 38,
    NULL
  },
  /* 146 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstTest, 0x00,
    2, g_Operands + 12,
    NULL
  },
  /* 147 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstTest, 0x00,
    2, g_Operands + 14,
    NULL
  },
  /* 148 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstXchg, 0x00,
    2, g_Operands + 62,
    NULL
  },
  /* 149 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstXchg, 0x00,
    2, g_Operands + 64,
    NULL
  },
  /* 150 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x00,
    2, g_Operands + 66,
    NULL
  },
  /* 151 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstMov, 0x00,
    2, g_Operands + 68,
    NULL
  },
  /* 152 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x00,
    2, g_Operands + 70,
    NULL
  },
  /* 153 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstMov, 0x00,
    2, g_Operands + 32,
    NULL
  },
  /* 154 */
  { NACLi_386,
    NACL_IFLAG(ModRmRegSOperand) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal),
    InstMov, 0x00,
    2, g_Operands + 72,
    NULL
  },
  /* 155 */
  { NACLi_386,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstLea, 0x00,
    2, g_Operands + 74,
    NULL
  },
  /* 156 */
  { NACLi_386,
    NACL_IFLAG(ModRmRegSOperand) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstMov, 0x00,
    2, g_Operands + 76,
    NULL
  },
  /* 157 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPop, 0x00,
    2, g_Operands + 78,
    g_Opcodes + 158
  },
  /* 158 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 159 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstXchg, 0x00,
    2, g_Operands + 80,
    NULL
  },
  /* 160 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstXchg, 0x01,
    2, g_Operands + 80,
    NULL
  },
  /* 161 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstXchg, 0x02,
    2, g_Operands + 80,
    NULL
  },
  /* 162 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstXchg, 0x03,
    2, g_Operands + 80,
    NULL
  },
  /* 163 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstXchg, 0x04,
    2, g_Operands + 80,
    NULL
  },
  /* 164 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstXchg, 0x05,
    2, g_Operands + 80,
    NULL
  },
  /* 165 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstXchg, 0x06,
    2, g_Operands + 80,
    NULL
  },
  /* 166 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstXchg, 0x07,
    2, g_Operands + 80,
    NULL
  },
  /* 167 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w),
    InstCbw, 0x00,
    2, g_Operands + 82,
    g_Opcodes + 168
  },
  /* 168 */
  { NACLi_386,
    NACL_IFLAG(OperandSize_v),
    InstCwde, 0x00,
    2, g_Operands + 84,
    g_Opcodes + 169
  },
  /* 169 */
  { NACLi_386,
    NACL_IFLAG(OperandSize_o) | NACL_IFLAG(LongMode),
    InstCdqe, 0x00,
    2, g_Operands + 86,
    NULL
  },
  /* 170 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w),
    InstCwd, 0x00,
    2, g_Operands + 88,
    g_Opcodes + 171
  },
  /* 171 */
  { NACLi_386,
    NACL_IFLAG(OperandSize_v),
    InstCdq, 0x00,
    2, g_Operands + 90,
    g_Opcodes + 172
  },
  /* 172 */
  { NACLi_386,
    NACL_IFLAG(OperandSize_o) | NACL_IFLAG(LongMode),
    InstCqo, 0x00,
    2, g_Operands + 92,
    NULL
  },
  /* 173 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 174 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFwait, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 175 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(NaClIllegal),
    InstPushf, 0x00,
    2, g_Operands + 94,
    g_Opcodes + 176
  },
  /* 176 */
  { NACLi_386,
    NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(OperandSizeDefaultIs64) | NACL_IFLAG(LongMode),
    InstPushfq, 0x00,
    2, g_Operands + 96,
    NULL
  },
  /* 177 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(NaClIllegal),
    InstPopf, 0x00,
    2, g_Operands + 98,
    g_Opcodes + 178
  },
  /* 178 */
  { NACLi_386,
    NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(OperandSizeDefaultIs64) | NACL_IFLAG(LongMode),
    InstPopfq, 0x00,
    2, g_Operands + 100,
    NULL
  },
  /* 179 */
  { NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstSahf, 0x00,
    1, g_Operands + 102,
    NULL
  },
  /* 180 */
  { NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstLahf, 0x00,
    1, g_Operands + 103,
    NULL
  },
  /* 181 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_Addr) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x00,
    2, g_Operands + 104,
    NULL
  },
  /* 182 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_Addr) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstMov, 0x00,
    2, g_Operands + 106,
    NULL
  },
  /* 183 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_Addr) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x00,
    2, g_Operands + 108,
    NULL
  },
  /* 184 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_Addr) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstMov, 0x00,
    2, g_Operands + 110,
    NULL
  },
  /* 185 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_b),
    InstMovsb, 0x00,
    2, g_Operands + 112,
    NULL
  },
  /* 186 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w),
    InstMovsw, 0x00,
    2, g_Operands + 114,
    g_Opcodes + 187
  },
  /* 187 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_v),
    InstMovsd, 0x00,
    2, g_Operands + 116,
    g_Opcodes + 188
  },
  /* 188 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(LongMode),
    InstMovsq, 0x00,
    2, g_Operands + 118,
    NULL
  },
  /* 189 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(OperandSize_b),
    InstCmpsb, 0x00,
    2, g_Operands + 120,
    NULL
  },
  /* 190 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w),
    InstCmpsw, 0x00,
    2, g_Operands + 122,
    g_Opcodes + 191
  },
  /* 191 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(OperandSize_v),
    InstCmpsd, 0x00,
    2, g_Operands + 124,
    g_Opcodes + 192
  },
  /* 192 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(LongMode),
    InstCmpsq, 0x00,
    2, g_Operands + 126,
    NULL
  },
  /* 193 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b),
    InstTest, 0x00,
    2, g_Operands + 20,
    NULL
  },
  /* 194 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstTest, 0x00,
    2, g_Operands + 22,
    NULL
  },
  /* 195 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_b),
    InstStosb, 0x00,
    2, g_Operands + 128,
    NULL
  },
  /* 196 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w),
    InstStosw, 0x00,
    2, g_Operands + 130,
    g_Opcodes + 197
  },
  /* 197 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_v),
    InstStosd, 0x00,
    2, g_Operands + 132,
    g_Opcodes + 198
  },
  /* 198 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(LongMode),
    InstStosq, 0x00,
    2, g_Operands + 134,
    NULL
  },
  /* 199 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_b),
    InstLodsb, 0x00,
    2, g_Operands + 136,
    NULL
  },
  /* 200 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w),
    InstLodsw, 0x00,
    2, g_Operands + 138,
    g_Opcodes + 201
  },
  /* 201 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_v),
    InstLodsd, 0x00,
    2, g_Operands + 140,
    g_Opcodes + 202
  },
  /* 202 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(LongMode),
    InstLodsq, 0x00,
    2, g_Operands + 142,
    NULL
  },
  /* 203 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(OperandSize_b),
    InstScasb, 0x00,
    2, g_Operands + 144,
    NULL
  },
  /* 204 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w),
    InstScasw, 0x00,
    2, g_Operands + 146,
    g_Opcodes + 205
  },
  /* 205 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(OperandSize_v),
    InstScasd, 0x00,
    2, g_Operands + 148,
    g_Opcodes + 206
  },
  /* 206 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(LongMode),
    InstScasq, 0x00,
    2, g_Operands + 150,
    NULL
  },
  /* 207 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x00,
    2, g_Operands + 152,
    NULL
  },
  /* 208 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x01,
    2, g_Operands + 152,
    NULL
  },
  /* 209 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x02,
    2, g_Operands + 152,
    NULL
  },
  /* 210 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x03,
    2, g_Operands + 152,
    NULL
  },
  /* 211 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x04,
    2, g_Operands + 152,
    NULL
  },
  /* 212 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x05,
    2, g_Operands + 152,
    NULL
  },
  /* 213 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x06,
    2, g_Operands + 152,
    NULL
  },
  /* 214 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x07,
    2, g_Operands + 152,
    NULL
  },
  /* 215 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstMov, 0x00,
    2, g_Operands + 154,
    NULL
  },
  /* 216 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstMov, 0x01,
    2, g_Operands + 154,
    NULL
  },
  /* 217 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstMov, 0x02,
    2, g_Operands + 154,
    NULL
  },
  /* 218 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstMov, 0x03,
    2, g_Operands + 154,
    NULL
  },
  /* 219 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstMov, 0x04,
    2, g_Operands + 154,
    NULL
  },
  /* 220 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstMov, 0x05,
    2, g_Operands + 154,
    NULL
  },
  /* 221 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstMov, 0x06,
    2, g_Operands + 154,
    NULL
  },
  /* 222 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstMov, 0x07,
    2, g_Operands + 154,
    NULL
  },
  /* 223 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstRol, 0x00,
    2, g_Operands + 54,
    g_Opcodes + 224
  },
  /* 224 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstRor, 0x01,
    2, g_Operands + 54,
    g_Opcodes + 225
  },
  /* 225 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstRcl, 0x02,
    2, g_Operands + 54,
    g_Opcodes + 226
  },
  /* 226 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstRcr, 0x03,
    2, g_Operands + 54,
    g_Opcodes + 227
  },
  /* 227 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstShl, 0x04,
    2, g_Operands + 54,
    g_Opcodes + 228
  },
  /* 228 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstShr, 0x05,
    2, g_Operands + 54,
    g_Opcodes + 229
  },
  /* 229 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstShl, 0x06,
    2, g_Operands + 54,
    g_Opcodes + 230
  },
  /* 230 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstSar, 0x07,
    2, g_Operands + 54,
    NULL
  },
  /* 231 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstRol, 0x00,
    2, g_Operands + 60,
    g_Opcodes + 232
  },
  /* 232 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstRor, 0x01,
    2, g_Operands + 60,
    g_Opcodes + 233
  },
  /* 233 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstRcl, 0x02,
    2, g_Operands + 60,
    g_Opcodes + 234
  },
  /* 234 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstRcr, 0x03,
    2, g_Operands + 60,
    g_Opcodes + 235
  },
  /* 235 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstShl, 0x04,
    2, g_Operands + 60,
    g_Opcodes + 236
  },
  /* 236 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstShr, 0x05,
    2, g_Operands + 60,
    g_Opcodes + 237
  },
  /* 237 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstShl, 0x06,
    2, g_Operands + 60,
    g_Opcodes + 238
  },
  /* 238 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstSar, 0x07,
    2, g_Operands + 60,
    NULL
  },
  /* 239 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_w) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstRet, 0x00,
    3, g_Operands + 156,
    NULL
  },
  /* 240 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstRet, 0x00,
    2, g_Operands + 156,
    NULL
  },
  /* 241 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 242 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 243 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x00,
    2, g_Operands + 159,
    g_Opcodes + 244
  },
  /* 244 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 245 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstMov, 0x00,
    2, g_Operands + 161,
    g_Opcodes + 246
  },
  /* 246 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 247 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_w) | NACL_IFLAG(OpcodeHasImmed2_b) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstEnter, 0x00,
    4, g_Operands + 163,
    NULL
  },
  /* 248 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstLeave, 0x00,
    2, g_Operands + 167,
    NULL
  },
  /* 249 */
  { NACLi_RETURN,
    NACL_IFLAG(OpcodeHasImmed_w) | NACL_IFLAG(NaClIllegal),
    InstRet, 0x00,
    3, g_Operands + 156,
    NULL
  },
  /* 250 */
  { NACLi_RETURN,
    NACL_IFLAG(NaClIllegal),
    InstRet, 0x00,
    2, g_Operands + 156,
    NULL
  },
  /* 251 */
  { NACLi_SYSTEM,
    NACL_IFLAG(NaClIllegal),
    InstInt3, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 252 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(NaClIllegal),
    InstInt, 0x00,
    1, g_Operands + 9,
    NULL
  },
  /* 253 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstInto, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 254 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstIretd, 0x00,
    2, g_Operands + 156,
    g_Opcodes + 255
  },
  /* 255 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(LongMode),
    InstIretq, 0x00,
    2, g_Operands + 156,
    g_Opcodes + 256
  },
  /* 256 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(NaClIllegal),
    InstIret, 0x00,
    2, g_Operands + 156,
    NULL
  },
  /* 257 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstRol, 0x00,
    2, g_Operands + 169,
    g_Opcodes + 258
  },
  /* 258 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstRor, 0x01,
    2, g_Operands + 169,
    g_Opcodes + 259
  },
  /* 259 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstRcl, 0x02,
    2, g_Operands + 169,
    g_Opcodes + 260
  },
  /* 260 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstRcr, 0x03,
    2, g_Operands + 169,
    g_Opcodes + 261
  },
  /* 261 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstShl, 0x04,
    2, g_Operands + 169,
    g_Opcodes + 262
  },
  /* 262 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstShr, 0x05,
    2, g_Operands + 169,
    g_Opcodes + 263
  },
  /* 263 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstShl, 0x06,
    2, g_Operands + 169,
    g_Opcodes + 264
  },
  /* 264 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSar, 0x07,
    2, g_Operands + 169,
    NULL
  },
  /* 265 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstRol, 0x00,
    2, g_Operands + 171,
    g_Opcodes + 266
  },
  /* 266 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstRor, 0x01,
    2, g_Operands + 171,
    g_Opcodes + 267
  },
  /* 267 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstRcl, 0x02,
    2, g_Operands + 171,
    g_Opcodes + 268
  },
  /* 268 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstRcr, 0x03,
    2, g_Operands + 171,
    g_Opcodes + 269
  },
  /* 269 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstShl, 0x04,
    2, g_Operands + 171,
    g_Opcodes + 270
  },
  /* 270 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstShr, 0x05,
    2, g_Operands + 171,
    g_Opcodes + 271
  },
  /* 271 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstShl, 0x06,
    2, g_Operands + 171,
    g_Opcodes + 272
  },
  /* 272 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstSar, 0x07,
    2, g_Operands + 171,
    NULL
  },
  /* 273 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstRol, 0x00,
    2, g_Operands + 173,
    g_Opcodes + 274
  },
  /* 274 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstRor, 0x01,
    2, g_Operands + 173,
    g_Opcodes + 275
  },
  /* 275 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstRcl, 0x02,
    2, g_Operands + 173,
    g_Opcodes + 276
  },
  /* 276 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstRcr, 0x03,
    2, g_Operands + 173,
    g_Opcodes + 277
  },
  /* 277 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstShl, 0x04,
    2, g_Operands + 173,
    g_Opcodes + 278
  },
  /* 278 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstShr, 0x05,
    2, g_Operands + 173,
    g_Opcodes + 279
  },
  /* 279 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstShl, 0x06,
    2, g_Operands + 173,
    g_Opcodes + 280
  },
  /* 280 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSar, 0x07,
    2, g_Operands + 173,
    NULL
  },
  /* 281 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstRol, 0x00,
    2, g_Operands + 175,
    g_Opcodes + 282
  },
  /* 282 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstRor, 0x01,
    2, g_Operands + 175,
    g_Opcodes + 283
  },
  /* 283 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstRcl, 0x02,
    2, g_Operands + 175,
    g_Opcodes + 284
  },
  /* 284 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstRcr, 0x03,
    2, g_Operands + 175,
    g_Opcodes + 285
  },
  /* 285 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstShl, 0x04,
    2, g_Operands + 175,
    g_Opcodes + 286
  },
  /* 286 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstShr, 0x05,
    2, g_Operands + 175,
    g_Opcodes + 287
  },
  /* 287 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstShl, 0x06,
    2, g_Operands + 175,
    g_Opcodes + 288
  },
  /* 288 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstSar, 0x07,
    2, g_Operands + 175,
    NULL
  },
  /* 289 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 290 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 291 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 292 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstXlat, 0x00,
    2, g_Operands + 177,
    NULL
  },
  /* 293 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFadd, 0x00,
    2, g_Operands + 179,
    g_Opcodes + 294
  },
  /* 294 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFmul, 0x01,
    2, g_Operands + 179,
    g_Opcodes + 295
  },
  /* 295 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFcom, 0x02,
    2, g_Operands + 181,
    g_Opcodes + 296
  },
  /* 296 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFcomp, 0x03,
    2, g_Operands + 181,
    g_Opcodes + 297
  },
  /* 297 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFsub, 0x04,
    2, g_Operands + 179,
    g_Opcodes + 298
  },
  /* 298 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFsubr, 0x05,
    2, g_Operands + 179,
    g_Opcodes + 299
  },
  /* 299 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFdiv, 0x06,
    2, g_Operands + 179,
    g_Opcodes + 300
  },
  /* 300 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFdivr, 0x07,
    2, g_Operands + 179,
    NULL
  },
  /* 301 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFld, 0x00,
    2, g_Operands + 183,
    g_Opcodes + 302
  },
  /* 302 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x01,
    0, g_Operands + 0,
    g_Opcodes + 303
  },
  /* 303 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFst, 0x02,
    2, g_Operands + 185,
    g_Opcodes + 304
  },
  /* 304 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFstp, 0x03,
    2, g_Operands + 185,
    g_Opcodes + 305
  },
  /* 305 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16),
    InstFldenv, 0x04,
    1, g_Operands + 187,
    g_Opcodes + 306
  },
  /* 306 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFldcw, 0x05,
    1, g_Operands + 188,
    g_Opcodes + 307
  },
  /* 307 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16),
    InstFnstenv, 0x06,
    1, g_Operands + 189,
    g_Opcodes + 308
  },
  /* 308 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFnstcw, 0x07,
    1, g_Operands + 190,
    NULL
  },
  /* 309 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFiadd, 0x00,
    2, g_Operands + 179,
    g_Opcodes + 310
  },
  /* 310 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFimul, 0x01,
    2, g_Operands + 179,
    g_Opcodes + 311
  },
  /* 311 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFicom, 0x02,
    2, g_Operands + 179,
    g_Opcodes + 312
  },
  /* 312 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFicomp, 0x03,
    2, g_Operands + 179,
    g_Opcodes + 313
  },
  /* 313 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFisub, 0x04,
    2, g_Operands + 179,
    g_Opcodes + 314
  },
  /* 314 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFisubr, 0x05,
    2, g_Operands + 179,
    g_Opcodes + 315
  },
  /* 315 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFidiv, 0x06,
    2, g_Operands + 179,
    g_Opcodes + 316
  },
  /* 316 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFidivr, 0x07,
    2, g_Operands + 179,
    NULL
  },
  /* 317 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFild, 0x00,
    2, g_Operands + 183,
    g_Opcodes + 318
  },
  /* 318 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFisttp, 0x01,
    2, g_Operands + 185,
    g_Opcodes + 319
  },
  /* 319 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFist, 0x02,
    2, g_Operands + 185,
    g_Opcodes + 320
  },
  /* 320 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFistp, 0x03,
    2, g_Operands + 185,
    g_Opcodes + 321
  },
  /* 321 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x04,
    0, g_Operands + 0,
    g_Opcodes + 322
  },
  /* 322 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16),
    InstFld, 0x05,
    2, g_Operands + 191,
    g_Opcodes + 323
  },
  /* 323 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x06,
    0, g_Operands + 0,
    g_Opcodes + 324
  },
  /* 324 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16),
    InstFstp, 0x07,
    2, g_Operands + 193,
    NULL
  },
  /* 325 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFadd, 0x00,
    2, g_Operands + 195,
    g_Opcodes + 326
  },
  /* 326 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFmul, 0x01,
    2, g_Operands + 195,
    g_Opcodes + 327
  },
  /* 327 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFcom, 0x02,
    2, g_Operands + 197,
    g_Opcodes + 328
  },
  /* 328 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFcomp, 0x03,
    2, g_Operands + 197,
    g_Opcodes + 329
  },
  /* 329 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFsub, 0x04,
    2, g_Operands + 195,
    g_Opcodes + 330
  },
  /* 330 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFsubr, 0x05,
    2, g_Operands + 195,
    g_Opcodes + 331
  },
  /* 331 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFdiv, 0x06,
    2, g_Operands + 195,
    g_Opcodes + 332
  },
  /* 332 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFdivr, 0x07,
    2, g_Operands + 195,
    NULL
  },
  /* 333 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFld, 0x00,
    2, g_Operands + 199,
    g_Opcodes + 334
  },
  /* 334 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFisttp, 0x01,
    2, g_Operands + 201,
    g_Opcodes + 335
  },
  /* 335 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFst, 0x02,
    2, g_Operands + 201,
    g_Opcodes + 336
  },
  /* 336 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFstp, 0x03,
    2, g_Operands + 201,
    g_Opcodes + 337
  },
  /* 337 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16),
    InstFrstor, 0x04,
    1, g_Operands + 187,
    g_Opcodes + 338
  },
  /* 338 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x05,
    0, g_Operands + 0,
    g_Opcodes + 339
  },
  /* 339 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16),
    InstFnsave, 0x06,
    1, g_Operands + 189,
    g_Opcodes + 340
  },
  /* 340 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFnstsw, 0x07,
    1, g_Operands + 190,
    NULL
  },
  /* 341 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFiadd, 0x00,
    2, g_Operands + 203,
    g_Opcodes + 342
  },
  /* 342 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFimul, 0x01,
    2, g_Operands + 203,
    g_Opcodes + 343
  },
  /* 343 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFicom, 0x02,
    2, g_Operands + 205,
    g_Opcodes + 344
  },
  /* 344 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFicomp, 0x03,
    2, g_Operands + 205,
    g_Opcodes + 345
  },
  /* 345 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFisub, 0x04,
    2, g_Operands + 203,
    g_Opcodes + 346
  },
  /* 346 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFisubr, 0x05,
    2, g_Operands + 203,
    g_Opcodes + 347
  },
  /* 347 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFidiv, 0x06,
    2, g_Operands + 203,
    g_Opcodes + 348
  },
  /* 348 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFidivr, 0x07,
    2, g_Operands + 203,
    NULL
  },
  /* 349 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFild, 0x00,
    2, g_Operands + 207,
    g_Opcodes + 350
  },
  /* 350 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFisttp, 0x01,
    2, g_Operands + 209,
    g_Opcodes + 351
  },
  /* 351 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFist, 0x02,
    2, g_Operands + 209,
    g_Opcodes + 352
  },
  /* 352 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFistp, 0x03,
    2, g_Operands + 209,
    g_Opcodes + 353
  },
  /* 353 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16),
    InstFbld, 0x04,
    2, g_Operands + 191,
    g_Opcodes + 354
  },
  /* 354 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16),
    InstFild, 0x05,
    2, g_Operands + 191,
    g_Opcodes + 355
  },
  /* 355 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16),
    InstFbstp, 0x06,
    2, g_Operands + 193,
    g_Opcodes + 356
  },
  /* 356 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16),
    InstFistp, 0x07,
    2, g_Operands + 193,
    NULL
  },
  /* 357 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump),
    InstLoopne, 0x00,
    2, g_Operands + 52,
    NULL
  },
  /* 358 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump),
    InstLoope, 0x00,
    2, g_Operands + 52,
    NULL
  },
  /* 359 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(JumpInstruction),
    InstLoop, 0x00,
    2, g_Operands + 52,
    NULL
  },
  /* 360 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(AddressSize_v) | NACL_IFLAG(ConditionalJump),
    InstJecxz, 0x00,
    3, g_Operands + 211,
    g_Opcodes + 361
  },
  /* 361 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(AddressSize_o) | NACL_IFLAG(ConditionalJump),
    InstJrcxz, 0x00,
    3, g_Operands + 214,
    NULL
  },
  /* 362 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(NaClIllegal),
    InstIn, 0x00,
    2, g_Operands + 217,
    NULL
  },
  /* 363 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal),
    InstIn, 0x00,
    2, g_Operands + 219,
    NULL
  },
  /* 364 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(NaClIllegal),
    InstOut, 0x00,
    2, g_Operands + 221,
    NULL
  },
  /* 365 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal),
    InstOut, 0x00,
    2, g_Operands + 223,
    NULL
  },
  /* 366 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(JumpInstruction),
    InstCall, 0x00,
    3, g_Operands + 225,
    g_Opcodes + 367
  },
  /* 367 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64) | NACL_IFLAG(JumpInstruction),
    InstCall, 0x00,
    3, g_Operands + 228,
    NULL
  },
  /* 368 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64) | NACL_IFLAG(JumpInstruction),
    InstJmp, 0x00,
    2, g_Operands + 231,
    NULL
  },
  /* 369 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(JumpInstruction),
    InstJmp, 0x00,
    2, g_Operands + 52,
    NULL
  },
  /* 370 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstIn, 0x00,
    2, g_Operands + 233,
    NULL
  },
  /* 371 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal),
    InstIn, 0x00,
    2, g_Operands + 235,
    NULL
  },
  /* 372 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstOut, 0x00,
    2, g_Operands + 237,
    NULL
  },
  /* 373 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal),
    InstOut, 0x00,
    2, g_Operands + 239,
    NULL
  },
  /* 374 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 375 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstInt1, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 376 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 377 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 378 */
  { NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstHlt, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 379 */
  { NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstCmc, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 380 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstTest, 0x00,
    2, g_Operands + 56,
    g_Opcodes + 381
  },
  /* 381 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstTest, 0x01,
    2, g_Operands + 56,
    g_Opcodes + 382
  },
  /* 382 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstNot, 0x02,
    1, g_Operands + 0,
    g_Opcodes + 383
  },
  /* 383 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstNeg, 0x03,
    1, g_Operands + 0,
    g_Opcodes + 384
  },
  /* 384 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstMul, 0x04,
    3, g_Operands + 241,
    g_Opcodes + 385
  },
  /* 385 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstImul, 0x05,
    3, g_Operands + 241,
    g_Opcodes + 386
  },
  /* 386 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstDiv, 0x06,
    3, g_Operands + 241,
    g_Opcodes + 387
  },
  /* 387 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstIdiv, 0x07,
    3, g_Operands + 241,
    NULL
  },
  /* 388 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstTest, 0x00,
    2, g_Operands + 33,
    g_Opcodes + 389
  },
  /* 389 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstTest, 0x01,
    2, g_Operands + 33,
    g_Opcodes + 390
  },
  /* 390 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstNot, 0x02,
    1, g_Operands + 2,
    g_Opcodes + 391
  },
  /* 391 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstNeg, 0x03,
    1, g_Operands + 2,
    g_Opcodes + 392
  },
  /* 392 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstMul, 0x04,
    3, g_Operands + 244,
    g_Opcodes + 393
  },
  /* 393 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstImul, 0x05,
    3, g_Operands + 244,
    g_Opcodes + 394
  },
  /* 394 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstDiv, 0x06,
    3, g_Operands + 244,
    g_Opcodes + 395
  },
  /* 395 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstIdiv, 0x07,
    3, g_Operands + 244,
    NULL
  },
  /* 396 */
  { NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstClc, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 397 */
  { NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstStc, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 398 */
  { NACLi_SYSTEM,
    NACL_IFLAG(NaClIllegal),
    InstCli, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 399 */
  { NACLi_SYSTEM,
    NACL_IFLAG(NaClIllegal),
    InstSti, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 400 */
  { NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstCld, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 401 */
  { NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstStd, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 402 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstInc, 0x00,
    1, g_Operands + 0,
    g_Opcodes + 403
  },
  /* 403 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstDec, 0x01,
    1, g_Operands + 0,
    g_Opcodes + 404
  },
  /* 404 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x02,
    0, g_Operands + 0,
    g_Opcodes + 405
  },
  /* 405 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x03,
    0, g_Operands + 0,
    g_Opcodes + 406
  },
  /* 406 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x04,
    0, g_Operands + 0,
    g_Opcodes + 407
  },
  /* 407 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x05,
    0, g_Operands + 0,
    g_Opcodes + 408
  },
  /* 408 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x06,
    0, g_Operands + 0,
    g_Opcodes + 409
  },
  /* 409 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x07,
    0, g_Operands + 0,
    NULL
  },
  /* 410 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstInc, 0x00,
    1, g_Operands + 2,
    g_Opcodes + 411
  },
  /* 411 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstDec, 0x01,
    1, g_Operands + 2,
    g_Opcodes + 412
  },
  /* 412 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64) | NACL_IFLAG(JumpInstruction),
    InstCall, 0x02,
    3, g_Operands + 247,
    g_Opcodes + 413
  },
  /* 413 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(JumpInstruction),
    InstCall, 0x03,
    3, g_Operands + 250,
    g_Opcodes + 414
  },
  /* 414 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64) | NACL_IFLAG(JumpInstruction),
    InstJmp, 0x04,
    2, g_Operands + 253,
    g_Opcodes + 415
  },
  /* 415 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(JumpInstruction),
    InstJmp, 0x05,
    2, g_Operands + 255,
    g_Opcodes + 416
  },
  /* 416 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPush, 0x06,
    2, g_Operands + 257,
    g_Opcodes + 417
  },
  /* 417 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x07,
    0, g_Operands + 0,
    NULL
  },
  /* 418 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal),
    InstSldt, 0x00,
    1, g_Operands + 259,
    g_Opcodes + 419
  },
  /* 419 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal),
    InstStr, 0x01,
    1, g_Operands + 259,
    g_Opcodes + 420
  },
  /* 420 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstLldt, 0x02,
    1, g_Operands + 77,
    g_Opcodes + 421
  },
  /* 421 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstLtr, 0x03,
    1, g_Operands + 77,
    g_Opcodes + 422
  },
  /* 422 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstVerr, 0x04,
    1, g_Operands + 260,
    g_Opcodes + 423
  },
  /* 423 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstVerw, 0x05,
    1, g_Operands + 260,
    g_Opcodes + 424
  },
  /* 424 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x06,
    0, g_Operands + 0,
    g_Opcodes + 425
  },
  /* 425 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x07,
    0, g_Operands + 0,
    NULL
  },
  /* 426 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstSgdt, 0x00,
    1, g_Operands + 261,
    g_Opcodes + 427
  },
  /* 427 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstSidt, 0x01,
    1, g_Operands + 261,
    g_Opcodes + 428
  },
  /* 428 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstMonitor, 0x01,
    3, g_Operands + 262,
    g_Opcodes + 429
  },
  /* 429 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstMwait, 0x11,
    2, g_Operands + 265,
    g_Opcodes + 430
  },
  /* 430 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x01,
    0, g_Operands + 0,
    g_Opcodes + 431
  },
  /* 431 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstLgdt, 0x02,
    1, g_Operands + 267,
    g_Opcodes + 432
  },
  /* 432 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstLidt, 0x03,
    1, g_Operands + 267,
    g_Opcodes + 433
  },
  /* 433 */
  { NACLi_SVM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstVmrun, 0x03,
    1, g_Operands + 268,
    g_Opcodes + 434
  },
  /* 434 */
  { NACLi_SVM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstVmmcall, 0x13,
    0, g_Operands + 0,
    g_Opcodes + 435
  },
  /* 435 */
  { NACLi_SVM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstVmload, 0x23,
    1, g_Operands + 268,
    g_Opcodes + 436
  },
  /* 436 */
  { NACLi_SVM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstVmsave, 0x33,
    1, g_Operands + 268,
    g_Opcodes + 437
  },
  /* 437 */
  { NACLi_SVM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstStgi, 0x43,
    0, g_Operands + 0,
    g_Opcodes + 438
  },
  /* 438 */
  { NACLi_SVM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstClgi, 0x53,
    0, g_Operands + 0,
    g_Opcodes + 439
  },
  /* 439 */
  { NACLi_SVM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstSkinit, 0x63,
    2, g_Operands + 269,
    g_Opcodes + 440
  },
  /* 440 */
  { NACLi_SVM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvlpga, 0x73,
    2, g_Operands + 271,
    g_Opcodes + 441
  },
  /* 441 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal),
    InstSmsw, 0x04,
    1, g_Operands + 72,
    g_Opcodes + 442
  },
  /* 442 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x05,
    0, g_Operands + 0,
    g_Opcodes + 443
  },
  /* 443 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstLmsw, 0x06,
    1, g_Operands + 77,
    g_Opcodes + 444
  },
  /* 444 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvlpg, 0x07,
    1, g_Operands + 273,
    g_Opcodes + 445
  },
  /* 445 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(LongMode),
    InstSwapgs, 0x07,
    1, g_Operands + 274,
    g_Opcodes + 446
  },
  /* 446 */
  { NACLi_RDTSCP,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstRdtscp, 0x17,
    3, g_Operands + 275,
    g_Opcodes + 447
  },
  /* 447 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x07,
    0, g_Operands + 0,
    NULL
  },
  /* 448 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal),
    InstLar, 0x00,
    2, g_Operands + 278,
    NULL
  },
  /* 449 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal),
    InstLsl, 0x00,
    2, g_Operands + 278,
    NULL
  },
  /* 450 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 451 */
  { NACLi_SYSCALL,
    NACL_IFLAG(NaClIllegal),
    InstSyscall, 0x00,
    2, g_Operands + 280,
    NULL
  },
  /* 452 */
  { NACLi_SYSTEM,
    NACL_IFLAG(NaClIllegal),
    InstClts, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 453 */
  { NACLi_SYSTEM,
    NACL_IFLAG(NaClIllegal),
    InstSysret, 0x00,
    2, g_Operands + 214,
    NULL
  },
  /* 454 */
  { NACLi_SYSTEM,
    NACL_IFLAG(NaClIllegal),
    InstInvd, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 455 */
  { NACLi_SYSTEM,
    NACL_IFLAG(NaClIllegal),
    InstWbinvd, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 456 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 457 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstUd2, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 458 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 459 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstPrefetch_exclusive, 0x00,
    1, g_Operands + 282,
    g_Opcodes + 460
  },
  /* 460 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstPrefetch_modified, 0x01,
    1, g_Operands + 282,
    g_Opcodes + 461
  },
  /* 461 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstPrefetch_reserved, 0x02,
    1, g_Operands + 282,
    g_Opcodes + 462
  },
  /* 462 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstPrefetch_modified, 0x03,
    1, g_Operands + 282,
    g_Opcodes + 463
  },
  /* 463 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstPrefetch_reserved, 0x04,
    1, g_Operands + 282,
    g_Opcodes + 464
  },
  /* 464 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstPrefetch_reserved, 0x05,
    1, g_Operands + 282,
    g_Opcodes + 465
  },
  /* 465 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstPrefetch_reserved, 0x06,
    1, g_Operands + 282,
    g_Opcodes + 466
  },
  /* 466 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstPrefetch_reserved, 0x07,
    1, g_Operands + 282,
    NULL
  },
  /* 467 */
  { NACLi_3DNOW,
    NACL_EMPTY_IFLAGS,
    InstFemms, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 468 */
  { NACLi_INVALID,
    NACL_IFLAG(Opcode0F0F) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    3, g_Operands + 283,
    NULL
  },
  /* 469 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstMovups, 0x00,
    2, g_Operands + 286,
    NULL
  },
  /* 470 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstMovups, 0x00,
    2, g_Operands + 288,
    NULL
  },
  /* 471 */
  { NACLi_SSE,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMovlps, 0x00,
    2, g_Operands + 290,
    g_Opcodes + 472
  },
  /* 472 */
  { NACLi_SSE,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMovhlps, 0x00,
    2, g_Operands + 292,
    NULL
  },
  /* 473 */
  { NACLi_SSE,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMovlps, 0x00,
    2, g_Operands + 294,
    NULL
  },
  /* 474 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstUnpcklps, 0x00,
    2, g_Operands + 296,
    NULL
  },
  /* 475 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstUnpckhps, 0x00,
    2, g_Operands + 296,
    NULL
  },
  /* 476 */
  { NACLi_SSE,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMovhps, 0x00,
    2, g_Operands + 290,
    g_Opcodes + 477
  },
  /* 477 */
  { NACLi_SSE,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMovlhps, 0x00,
    2, g_Operands + 292,
    NULL
  },
  /* 478 */
  { NACLi_SSE,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMovhps, 0x00,
    2, g_Operands + 294,
    NULL
  },
  /* 479 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstPrefetchnta, 0x00,
    1, g_Operands + 282,
    g_Opcodes + 480
  },
  /* 480 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstPrefetcht0, 0x01,
    1, g_Operands + 282,
    g_Opcodes + 481
  },
  /* 481 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstPrefetcht1, 0x02,
    1, g_Operands + 282,
    g_Opcodes + 482
  },
  /* 482 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstPrefetcht2, 0x03,
    1, g_Operands + 282,
    g_Opcodes + 483
  },
  /* 483 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm),
    InstNop, 0x04,
    0, g_Operands + 0,
    g_Opcodes + 484
  },
  /* 484 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm),
    InstNop, 0x05,
    0, g_Operands + 0,
    g_Opcodes + 485
  },
  /* 485 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm),
    InstNop, 0x06,
    0, g_Operands + 0,
    g_Opcodes + 486
  },
  /* 486 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm),
    InstNop, 0x07,
    0, g_Operands + 0,
    NULL
  },
  /* 487 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstNop, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 488 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstNop, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 489 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstNop, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 490 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstNop, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 491 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstNop, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 492 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstNop, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 493 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm),
    InstNop, 0x00,
    0, g_Operands + 0,
    g_Opcodes + 494
  },
  /* 494 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstNop, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 495 */
  { NACLi_SYSTEM,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstMov, 0x00,
    2, g_Operands + 298,
    NULL
  },
  /* 496 */
  { NACLi_SYSTEM,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstMov, 0x00,
    2, g_Operands + 300,
    NULL
  },
  /* 497 */
  { NACLi_SYSTEM,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstMov, 0x00,
    2, g_Operands + 302,
    NULL
  },
  /* 498 */
  { NACLi_SYSTEM,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstMov, 0x00,
    2, g_Operands + 304,
    NULL
  },
  /* 499 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 500 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 501 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 502 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 503 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstMovaps, 0x00,
    2, g_Operands + 286,
    NULL
  },
  /* 504 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstMovaps, 0x00,
    2, g_Operands + 288,
    NULL
  },
  /* 505 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstCvtpi2ps, 0x00,
    2, g_Operands + 306,
    NULL
  },
  /* 506 */
  { NACLi_SSE,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMovntps, 0x00,
    2, g_Operands + 308,
    NULL
  },
  /* 507 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstCvttps2pi, 0x00,
    2, g_Operands + 310,
    NULL
  },
  /* 508 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstCvtps2pi, 0x00,
    2, g_Operands + 310,
    NULL
  },
  /* 509 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstUcomiss, 0x00,
    2, g_Operands + 312,
    NULL
  },
  /* 510 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstComiss, 0x00,
    2, g_Operands + 314,
    NULL
  },
  /* 511 */
  { NACLi_RDMSR,
    NACL_IFLAG(NaClIllegal),
    InstWrmsr, 0x00,
    3, g_Operands + 316,
    NULL
  },
  /* 512 */
  { NACLi_RDTSC,
    NACL_EMPTY_IFLAGS,
    InstRdtsc, 0x00,
    2, g_Operands + 319,
    NULL
  },
  /* 513 */
  { NACLi_RDMSR,
    NACL_IFLAG(NaClIllegal),
    InstRdmsr, 0x00,
    3, g_Operands + 321,
    NULL
  },
  /* 514 */
  { NACLi_SYSTEM,
    NACL_IFLAG(NaClIllegal),
    InstRdpmc, 0x00,
    3, g_Operands + 321,
    NULL
  },
  /* 515 */
  { NACLi_SYSENTER,
    NACL_IFLAG(NaClIllegal),
    InstSysenter, 0x00,
    4, g_Operands + 324,
    NULL
  },
  /* 516 */
  { NACLi_SYSENTER,
    NACL_IFLAG(NaClIllegal),
    InstSysexit, 0x00,
    6, g_Operands + 328,
    NULL
  },
  /* 517 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 518 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 519 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 520 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 521 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 522 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 523 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 524 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 525 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 526 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 527 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCmovo, 0x00,
    2, g_Operands + 32,
    NULL
  },
  /* 528 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCmovno, 0x00,
    2, g_Operands + 32,
    NULL
  },
  /* 529 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCmovb, 0x00,
    2, g_Operands + 32,
    NULL
  },
  /* 530 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCmovnb, 0x00,
    2, g_Operands + 32,
    NULL
  },
  /* 531 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCmovz, 0x00,
    2, g_Operands + 32,
    NULL
  },
  /* 532 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCmovnz, 0x00,
    2, g_Operands + 32,
    NULL
  },
  /* 533 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCmovbe, 0x00,
    2, g_Operands + 32,
    NULL
  },
  /* 534 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCmovnbe, 0x00,
    2, g_Operands + 32,
    NULL
  },
  /* 535 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCmovs, 0x00,
    2, g_Operands + 32,
    NULL
  },
  /* 536 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCmovns, 0x00,
    2, g_Operands + 32,
    NULL
  },
  /* 537 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCmovp, 0x00,
    2, g_Operands + 32,
    NULL
  },
  /* 538 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCmovnp, 0x00,
    2, g_Operands + 32,
    NULL
  },
  /* 539 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCmovl, 0x00,
    2, g_Operands + 32,
    NULL
  },
  /* 540 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCmovnl, 0x00,
    2, g_Operands + 32,
    NULL
  },
  /* 541 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCmovle, 0x00,
    2, g_Operands + 32,
    NULL
  },
  /* 542 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCmovnle, 0x00,
    2, g_Operands + 32,
    NULL
  },
  /* 543 */
  { NACLi_SSE,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMovmskps, 0x00,
    2, g_Operands + 334,
    NULL
  },
  /* 544 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstSqrtps, 0x00,
    2, g_Operands + 286,
    NULL
  },
  /* 545 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstRsqrtps, 0x00,
    2, g_Operands + 286,
    NULL
  },
  /* 546 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstRcpps, 0x00,
    2, g_Operands + 286,
    NULL
  },
  /* 547 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstAndps, 0x00,
    2, g_Operands + 336,
    NULL
  },
  /* 548 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstAndnps, 0x00,
    2, g_Operands + 336,
    NULL
  },
  /* 549 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstOrps, 0x00,
    2, g_Operands + 336,
    NULL
  },
  /* 550 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstXorps, 0x00,
    2, g_Operands + 336,
    NULL
  },
  /* 551 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstAddps, 0x00,
    2, g_Operands + 336,
    NULL
  },
  /* 552 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstMulps, 0x00,
    2, g_Operands + 336,
    NULL
  },
  /* 553 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm),
    InstCvtps2pd, 0x00,
    2, g_Operands + 338,
    NULL
  },
  /* 554 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm),
    InstCvtdq2ps, 0x00,
    2, g_Operands + 340,
    NULL
  },
  /* 555 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstSubps, 0x00,
    2, g_Operands + 336,
    NULL
  },
  /* 556 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstMinps, 0x00,
    2, g_Operands + 336,
    NULL
  },
  /* 557 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstDivps, 0x00,
    2, g_Operands + 336,
    NULL
  },
  /* 558 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstMaxps, 0x00,
    2, g_Operands + 336,
    NULL
  },
  /* 559 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPunpcklbw, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 560 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPunpcklwd, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 561 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPunpckldq, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 562 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPacksswb, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 563 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPcmpgtb, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 564 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPcmpgtw, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 565 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPcmpgtd, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 566 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPackuswb, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 567 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPunpckhbw, 0x00,
    2, g_Operands + 344,
    NULL
  },
  /* 568 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPunpckhwd, 0x00,
    2, g_Operands + 344,
    NULL
  },
  /* 569 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPunpckhdq, 0x00,
    2, g_Operands + 344,
    NULL
  },
  /* 570 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPackssdw, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 571 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 572 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 573 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstMovd, 0x00,
    2, g_Operands + 346,
    g_Opcodes + 574
  },
  /* 574 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_o),
    InstMovq, 0x00,
    2, g_Operands + 348,
    NULL
  },
  /* 575 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstMovq, 0x00,
    2, g_Operands + 350,
    NULL
  },
  /* 576 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstPshufw, 0x00,
    3, g_Operands + 352,
    NULL
  },
  /* 577 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    g_Opcodes + 578
  },
  /* 578 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x01,
    0, g_Operands + 0,
    g_Opcodes + 579
  },
  /* 579 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstPsrlw, 0x02,
    2, g_Operands + 355,
    g_Opcodes + 580
  },
  /* 580 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x03,
    0, g_Operands + 0,
    g_Opcodes + 581
  },
  /* 581 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstPsraw, 0x04,
    2, g_Operands + 355,
    g_Opcodes + 582
  },
  /* 582 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x05,
    0, g_Operands + 0,
    g_Opcodes + 583
  },
  /* 583 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstPsllw, 0x06,
    2, g_Operands + 355,
    g_Opcodes + 584
  },
  /* 584 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x07,
    0, g_Operands + 0,
    NULL
  },
  /* 585 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    g_Opcodes + 586
  },
  /* 586 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x01,
    0, g_Operands + 0,
    g_Opcodes + 587
  },
  /* 587 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstPsrld, 0x02,
    2, g_Operands + 355,
    g_Opcodes + 588
  },
  /* 588 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x03,
    0, g_Operands + 0,
    g_Opcodes + 589
  },
  /* 589 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstPsrad, 0x04,
    2, g_Operands + 355,
    g_Opcodes + 590
  },
  /* 590 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x05,
    0, g_Operands + 0,
    g_Opcodes + 591
  },
  /* 591 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstPslld, 0x06,
    2, g_Operands + 355,
    g_Opcodes + 592
  },
  /* 592 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x07,
    0, g_Operands + 0,
    NULL
  },
  /* 593 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    g_Opcodes + 594
  },
  /* 594 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x01,
    0, g_Operands + 0,
    g_Opcodes + 595
  },
  /* 595 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstPsrlq, 0x02,
    2, g_Operands + 355,
    g_Opcodes + 596
  },
  /* 596 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x03,
    0, g_Operands + 0,
    g_Opcodes + 597
  },
  /* 597 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x04,
    0, g_Operands + 0,
    g_Opcodes + 598
  },
  /* 598 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x05,
    0, g_Operands + 0,
    g_Opcodes + 599
  },
  /* 599 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstPsllq, 0x06,
    2, g_Operands + 355,
    g_Opcodes + 600
  },
  /* 600 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x07,
    0, g_Operands + 0,
    NULL
  },
  /* 601 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPcmpeqb, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 602 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPcmpeqw, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 603 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPcmpeqd, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 604 */
  { NACLi_MMX,
    NACL_EMPTY_IFLAGS,
    InstEmms, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 605 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 606 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 607 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 608 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 609 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 610 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 611 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstMovd, 0x00,
    2, g_Operands + 357,
    g_Opcodes + 612
  },
  /* 612 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_o),
    InstMovq, 0x00,
    2, g_Operands + 359,
    NULL
  },
  /* 613 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstMovq, 0x00,
    2, g_Operands + 361,
    NULL
  },
  /* 614 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64) | NACL_IFLAG(ConditionalJump),
    InstJo, 0x00,
    2, g_Operands + 231,
    NULL
  },
  /* 615 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64) | NACL_IFLAG(ConditionalJump),
    InstJno, 0x00,
    2, g_Operands + 231,
    NULL
  },
  /* 616 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64) | NACL_IFLAG(ConditionalJump),
    InstJb, 0x00,
    2, g_Operands + 231,
    NULL
  },
  /* 617 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64) | NACL_IFLAG(ConditionalJump),
    InstJnb, 0x00,
    2, g_Operands + 231,
    NULL
  },
  /* 618 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64) | NACL_IFLAG(ConditionalJump),
    InstJz, 0x00,
    2, g_Operands + 231,
    NULL
  },
  /* 619 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64) | NACL_IFLAG(ConditionalJump),
    InstJnz, 0x00,
    2, g_Operands + 231,
    NULL
  },
  /* 620 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64) | NACL_IFLAG(ConditionalJump),
    InstJbe, 0x00,
    2, g_Operands + 231,
    NULL
  },
  /* 621 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64) | NACL_IFLAG(ConditionalJump),
    InstJnbe, 0x00,
    2, g_Operands + 231,
    NULL
  },
  /* 622 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64) | NACL_IFLAG(ConditionalJump),
    InstJs, 0x00,
    2, g_Operands + 231,
    NULL
  },
  /* 623 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64) | NACL_IFLAG(ConditionalJump),
    InstJns, 0x00,
    2, g_Operands + 231,
    NULL
  },
  /* 624 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64) | NACL_IFLAG(ConditionalJump),
    InstJp, 0x00,
    2, g_Operands + 231,
    NULL
  },
  /* 625 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64) | NACL_IFLAG(ConditionalJump),
    InstJnp, 0x00,
    2, g_Operands + 231,
    NULL
  },
  /* 626 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64) | NACL_IFLAG(ConditionalJump),
    InstJl, 0x00,
    2, g_Operands + 231,
    NULL
  },
  /* 627 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64) | NACL_IFLAG(ConditionalJump),
    InstJnl, 0x00,
    2, g_Operands + 231,
    NULL
  },
  /* 628 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64) | NACL_IFLAG(ConditionalJump),
    InstJle, 0x00,
    2, g_Operands + 231,
    NULL
  },
  /* 629 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64) | NACL_IFLAG(ConditionalJump),
    InstJnle, 0x00,
    2, g_Operands + 231,
    NULL
  },
  /* 630 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSeto, 0x00,
    1, g_Operands + 66,
    NULL
  },
  /* 631 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetno, 0x00,
    1, g_Operands + 66,
    NULL
  },
  /* 632 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetb, 0x00,
    1, g_Operands + 66,
    NULL
  },
  /* 633 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetnb, 0x00,
    1, g_Operands + 66,
    NULL
  },
  /* 634 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetz, 0x00,
    1, g_Operands + 66,
    NULL
  },
  /* 635 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetnz, 0x00,
    1, g_Operands + 66,
    NULL
  },
  /* 636 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetbe, 0x00,
    1, g_Operands + 66,
    NULL
  },
  /* 637 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetnbe, 0x00,
    1, g_Operands + 66,
    NULL
  },
  /* 638 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSets, 0x00,
    1, g_Operands + 66,
    NULL
  },
  /* 639 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetns, 0x00,
    1, g_Operands + 66,
    NULL
  },
  /* 640 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetp, 0x00,
    1, g_Operands + 66,
    NULL
  },
  /* 641 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetnp, 0x00,
    1, g_Operands + 66,
    NULL
  },
  /* 642 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetl, 0x00,
    1, g_Operands + 66,
    NULL
  },
  /* 643 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetnl, 0x00,
    1, g_Operands + 66,
    NULL
  },
  /* 644 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetle, 0x00,
    1, g_Operands + 66,
    NULL
  },
  /* 645 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetnle, 0x00,
    1, g_Operands + 66,
    NULL
  },
  /* 646 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPush, 0x00,
    2, g_Operands + 363,
    NULL
  },
  /* 647 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPop, 0x00,
    2, g_Operands + 365,
    NULL
  },
  /* 648 */
  { NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstCpuid, 0x00,
    4, g_Operands + 367,
    NULL
  },
  /* 649 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal),
    InstBt, 0x00,
    2, g_Operands + 14,
    NULL
  },
  /* 650 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstShld, 0x00,
    3, g_Operands + 371,
    NULL
  },
  /* 651 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstShld, 0x00,
    3, g_Operands + 374,
    NULL
  },
  /* 652 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 653 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 654 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPush, 0x00,
    2, g_Operands + 377,
    NULL
  },
  /* 655 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPop, 0x00,
    2, g_Operands + 379,
    NULL
  },
  /* 656 */
  { NACLi_SYSTEM,
    NACL_IFLAG(NaClIllegal),
    InstRsm, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 657 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal),
    InstBts, 0x00,
    2, g_Operands + 2,
    NULL
  },
  /* 658 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstShrd, 0x00,
    3, g_Operands + 381,
    NULL
  },
  /* 659 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstShrd, 0x00,
    3, g_Operands + 384,
    NULL
  },
  /* 660 */
  { NACLi_FXSAVE,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(NaClIllegal),
    InstFxsave, 0x00,
    1, g_Operands + 189,
    g_Opcodes + 661
  },
  /* 661 */
  { NACLi_FXSAVE,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(NaClIllegal),
    InstFxrstor, 0x01,
    1, g_Operands + 187,
    g_Opcodes + 662
  },
  /* 662 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstLdmxcsr, 0x02,
    1, g_Operands + 180,
    g_Opcodes + 663
  },
  /* 663 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstStmxcsr, 0x03,
    1, g_Operands + 185,
    g_Opcodes + 664
  },
  /* 664 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x04,
    0, g_Operands + 0,
    g_Opcodes + 665
  },
  /* 665 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstLfence, 0x05,
    0, g_Operands + 0,
    g_Opcodes + 666
  },
  /* 666 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstLfence, 0x15,
    0, g_Operands + 0,
    g_Opcodes + 667
  },
  /* 667 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstLfence, 0x25,
    0, g_Operands + 0,
    g_Opcodes + 668
  },
  /* 668 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstLfence, 0x35,
    0, g_Operands + 0,
    g_Opcodes + 669
  },
  /* 669 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstLfence, 0x45,
    0, g_Operands + 0,
    g_Opcodes + 670
  },
  /* 670 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstLfence, 0x55,
    0, g_Operands + 0,
    g_Opcodes + 671
  },
  /* 671 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstLfence, 0x65,
    0, g_Operands + 0,
    g_Opcodes + 672
  },
  /* 672 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstLfence, 0x75,
    0, g_Operands + 0,
    g_Opcodes + 673
  },
  /* 673 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMfence, 0x06,
    0, g_Operands + 0,
    g_Opcodes + 674
  },
  /* 674 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMfence, 0x16,
    0, g_Operands + 0,
    g_Opcodes + 675
  },
  /* 675 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMfence, 0x26,
    0, g_Operands + 0,
    g_Opcodes + 676
  },
  /* 676 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMfence, 0x36,
    0, g_Operands + 0,
    g_Opcodes + 677
  },
  /* 677 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMfence, 0x46,
    0, g_Operands + 0,
    g_Opcodes + 678
  },
  /* 678 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMfence, 0x56,
    0, g_Operands + 0,
    g_Opcodes + 679
  },
  /* 679 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMfence, 0x66,
    0, g_Operands + 0,
    g_Opcodes + 680
  },
  /* 680 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMfence, 0x76,
    0, g_Operands + 0,
    g_Opcodes + 681
  },
  /* 681 */
  { NACLi_SFENCE_CLFLUSH,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstSfence, 0x07,
    0, g_Operands + 0,
    g_Opcodes + 682
  },
  /* 682 */
  { NACLi_SFENCE_CLFLUSH,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstSfence, 0x17,
    0, g_Operands + 0,
    g_Opcodes + 683
  },
  /* 683 */
  { NACLi_SFENCE_CLFLUSH,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstSfence, 0x27,
    0, g_Operands + 0,
    g_Opcodes + 684
  },
  /* 684 */
  { NACLi_SFENCE_CLFLUSH,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstSfence, 0x37,
    0, g_Operands + 0,
    g_Opcodes + 685
  },
  /* 685 */
  { NACLi_SFENCE_CLFLUSH,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstSfence, 0x47,
    0, g_Operands + 0,
    g_Opcodes + 686
  },
  /* 686 */
  { NACLi_SFENCE_CLFLUSH,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstSfence, 0x57,
    0, g_Operands + 0,
    g_Opcodes + 687
  },
  /* 687 */
  { NACLi_SFENCE_CLFLUSH,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstSfence, 0x67,
    0, g_Operands + 0,
    g_Opcodes + 688
  },
  /* 688 */
  { NACLi_SFENCE_CLFLUSH,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstSfence, 0x77,
    0, g_Operands + 0,
    g_Opcodes + 689
  },
  /* 689 */
  { NACLi_SFENCE_CLFLUSH,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstClflush, 0x07,
    1, g_Operands + 273,
    NULL
  },
  /* 690 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstImul, 0x00,
    2, g_Operands + 6,
    NULL
  },
  /* 691 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstCmpxchg, 0x00,
    3, g_Operands + 387,
    NULL
  },
  /* 692 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCmpxchg, 0x00,
    3, g_Operands + 390,
    NULL
  },
  /* 693 */
  { NACLi_386,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal),
    InstLss, 0x00,
    2, g_Operands + 393,
    NULL
  },
  /* 694 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal),
    InstBtr, 0x00,
    2, g_Operands + 2,
    NULL
  },
  /* 695 */
  { NACLi_386,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal),
    InstLfs, 0x00,
    2, g_Operands + 393,
    NULL
  },
  /* 696 */
  { NACLi_386,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal),
    InstLgs, 0x00,
    2, g_Operands + 393,
    NULL
  },
  /* 697 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstMovzx, 0x00,
    2, g_Operands + 395,
    NULL
  },
  /* 698 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstMovzx, 0x00,
    2, g_Operands + 397,
    NULL
  },
  /* 699 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 700 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 701 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstBt, 0x04,
    2, g_Operands + 38,
    g_Opcodes + 702
  },
  /* 702 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstBts, 0x05,
    2, g_Operands + 60,
    g_Opcodes + 703
  },
  /* 703 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstBtr, 0x06,
    2, g_Operands + 60,
    g_Opcodes + 704
  },
  /* 704 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstBtc, 0x07,
    2, g_Operands + 60,
    g_Opcodes + 705
  },
  /* 705 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 706 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal),
    InstBtc, 0x00,
    2, g_Operands + 2,
    NULL
  },
  /* 707 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstBsf, 0x00,
    2, g_Operands + 399,
    NULL
  },
  /* 708 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstBsr, 0x00,
    2, g_Operands + 399,
    NULL
  },
  /* 709 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstMovsx, 0x00,
    2, g_Operands + 395,
    NULL
  },
  /* 710 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstMovsx, 0x00,
    2, g_Operands + 397,
    NULL
  },
  /* 711 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstXadd, 0x00,
    2, g_Operands + 62,
    NULL
  },
  /* 712 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstXadd, 0x00,
    2, g_Operands + 64,
    NULL
  },
  /* 713 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstCmpps, 0x00,
    3, g_Operands + 401,
    NULL
  },
  /* 714 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstMovnti, 0x00,
    2, g_Operands + 404,
    NULL
  },
  /* 715 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstPinsrw, 0x00,
    3, g_Operands + 406,
    NULL
  },
  /* 716 */
  { NACLi_SSE41,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstPextrw, 0x00,
    3, g_Operands + 409,
    NULL
  },
  /* 717 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstShufps, 0x00,
    3, g_Operands + 401,
    NULL
  },
  /* 718 */
  { NACLi_CMPXCHG8B,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_v),
    InstCmpxchg8b, 0x01,
    3, g_Operands + 412,
    g_Opcodes + 719
  },
  /* 719 */
  { NACLi_CMPXCHG16B,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_o),
    InstCmpxchg16b, 0x01,
    3, g_Operands + 415,
    g_Opcodes + 720
  },
  /* 720 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 721 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstBswap, 0x00,
    1, g_Operands + 80,
    NULL
  },
  /* 722 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstBswap, 0x01,
    1, g_Operands + 80,
    NULL
  },
  /* 723 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstBswap, 0x02,
    1, g_Operands + 80,
    NULL
  },
  /* 724 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstBswap, 0x03,
    1, g_Operands + 80,
    NULL
  },
  /* 725 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstBswap, 0x04,
    1, g_Operands + 80,
    NULL
  },
  /* 726 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstBswap, 0x05,
    1, g_Operands + 80,
    NULL
  },
  /* 727 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstBswap, 0x06,
    1, g_Operands + 80,
    NULL
  },
  /* 728 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstBswap, 0x07,
    1, g_Operands + 80,
    NULL
  },
  /* 729 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 730 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsrlw, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 731 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsrld, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 732 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsrlq, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 733 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPaddq, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 734 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPmullw, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 735 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 736 */
  { NACLi_MMX,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstPmovmskb, 0x00,
    2, g_Operands + 409,
    NULL
  },
  /* 737 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsubusb, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 738 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsubusw, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 739 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPminub, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 740 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPand, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 741 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPaddusb, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 742 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPaddusw, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 743 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPmaxub, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 744 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPandn, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 745 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPavgb, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 746 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsraw, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 747 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsrad, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 748 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPavgw, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 749 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPmulhuw, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 750 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPmulhw, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 751 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 752 */
  { NACLi_MMX,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMovntq, 0x00,
    2, g_Operands + 418,
    NULL
  },
  /* 753 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsubsb, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 754 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsubsw, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 755 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPminsw, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 756 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPor, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 757 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPaddsb, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 758 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPaddsw, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 759 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPmaxsw, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 760 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPxor, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 761 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 762 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsllw, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 763 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPslld, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 764 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsllq, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 765 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPmuludq, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 766 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPmaddwd, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 767 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsadbw, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 768 */
  { NACLi_MMX,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_v),
    InstMaskmovq, 0x00,
    3, g_Operands + 420,
    NULL
  },
  /* 769 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsubb, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 770 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsubw, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 771 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsubd, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 772 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsubq, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 773 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPaddb, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 774 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPaddw, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 775 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPaddd, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 776 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 777 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstMovsd, 0x00,
    2, g_Operands + 423,
    NULL
  },
  /* 778 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstMovsd, 0x00,
    2, g_Operands + 425,
    NULL
  },
  /* 779 */
  { NACLi_SSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstMovddup, 0x00,
    2, g_Operands + 427,
    NULL
  },
  /* 780 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 781 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 782 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 783 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 784 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 785 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 786 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 787 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCvtsi2sd, 0x00,
    2, g_Operands + 429,
    NULL
  },
  /* 788 */
  { NACLi_SSE4A,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstMovntsd, 0x00,
    2, g_Operands + 431,
    NULL
  },
  /* 789 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCvttsd2si, 0x00,
    2, g_Operands + 433,
    NULL
  },
  /* 790 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCvtsd2si, 0x00,
    2, g_Operands + 433,
    NULL
  },
  /* 791 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 792 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 793 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 794 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstSqrtsd, 0x00,
    2, g_Operands + 423,
    NULL
  },
  /* 795 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 796 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 797 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 798 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 799 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 800 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 801 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstAddsd, 0x00,
    2, g_Operands + 435,
    NULL
  },
  /* 802 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstMulsd, 0x00,
    2, g_Operands + 435,
    NULL
  },
  /* 803 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstCvtsd2ss, 0x00,
    2, g_Operands + 437,
    NULL
  },
  /* 804 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 805 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstSubsd, 0x00,
    2, g_Operands + 435,
    NULL
  },
  /* 806 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstMinsd, 0x00,
    2, g_Operands + 435,
    NULL
  },
  /* 807 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstDivsd, 0x00,
    2, g_Operands + 435,
    NULL
  },
  /* 808 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstMaxsd, 0x00,
    2, g_Operands + 435,
    NULL
  },
  /* 809 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 810 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 811 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 812 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 813 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 814 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 815 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 816 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 817 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 818 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 819 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 820 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 821 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 822 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 823 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 824 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 825 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsRepne),
    InstPshuflw, 0x00,
    3, g_Operands + 439,
    NULL
  },
  /* 826 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 827 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 828 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 829 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 830 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 831 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 832 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 833 */
  { NACLi_SSE4A,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeHasImmed2_b) | NACL_IFLAG(OpcodeAllowsRepne),
    InstInsertq, 0x00,
    4, g_Operands + 442,
    NULL
  },
  /* 834 */
  { NACLi_SSE4A,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstInsertq, 0x00,
    2, g_Operands + 446,
    NULL
  },
  /* 835 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 836 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 837 */
  { NACLi_SSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstHaddps, 0x00,
    2, g_Operands + 336,
    NULL
  },
  /* 838 */
  { NACLi_SSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstHsubps, 0x00,
    2, g_Operands + 336,
    NULL
  },
  /* 839 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 840 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 841 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 842 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 843 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 844 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 845 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 846 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 847 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 848 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 849 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 850 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsRepne),
    InstCmpsd_xmm, 0x00,
    3, g_Operands + 448,
    NULL
  },
  /* 851 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 852 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 853 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 854 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 855 */
  { NACLi_SSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstAddsubps, 0x00,
    2, g_Operands + 451,
    NULL
  },
  /* 856 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 857 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 858 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 859 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 860 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 861 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstMovdq2q, 0x00,
    2, g_Operands + 453,
    NULL
  },
  /* 862 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 863 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 864 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 865 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 866 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 867 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 868 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 869 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 870 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 871 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 872 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 873 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 874 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 875 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 876 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 877 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstCvtpd2dq, 0x00,
    2, g_Operands + 455,
    NULL
  },
  /* 878 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 879 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 880 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 881 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 882 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 883 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 884 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 885 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 886 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 887 */
  { NACLi_SSE3,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstLddqu, 0x00,
    2, g_Operands + 457,
    NULL
  },
  /* 888 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 889 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 890 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 891 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 892 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 893 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 894 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 895 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 896 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 897 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 898 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 899 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 900 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 901 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 902 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 903 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMovss, 0x00,
    2, g_Operands + 459,
    NULL
  },
  /* 904 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMovss, 0x00,
    2, g_Operands + 461,
    NULL
  },
  /* 905 */
  { NACLi_SSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMovsldup, 0x00,
    2, g_Operands + 286,
    NULL
  },
  /* 906 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 907 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 908 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 909 */
  { NACLi_SSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMovshdup, 0x00,
    2, g_Operands + 286,
    NULL
  },
  /* 910 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 911 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 912 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 913 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCvtsi2ss, 0x00,
    2, g_Operands + 463,
    NULL
  },
  /* 914 */
  { NACLi_SSE4A,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMovntss, 0x00,
    2, g_Operands + 465,
    NULL
  },
  /* 915 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCvttss2si, 0x00,
    2, g_Operands + 467,
    NULL
  },
  /* 916 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCvtss2si, 0x00,
    2, g_Operands + 467,
    NULL
  },
  /* 917 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 918 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 919 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 920 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstSqrtss, 0x00,
    2, g_Operands + 286,
    NULL
  },
  /* 921 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstRsqrtss, 0x00,
    2, g_Operands + 459,
    NULL
  },
  /* 922 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstRcpss, 0x00,
    2, g_Operands + 459,
    NULL
  },
  /* 923 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 924 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 925 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 926 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 927 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstAddss, 0x00,
    2, g_Operands + 469,
    NULL
  },
  /* 928 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMulss, 0x00,
    2, g_Operands + 469,
    NULL
  },
  /* 929 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstCvtss2sd, 0x00,
    2, g_Operands + 471,
    NULL
  },
  /* 930 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstCvttps2dq, 0x00,
    2, g_Operands + 473,
    NULL
  },
  /* 931 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstSubss, 0x00,
    2, g_Operands + 469,
    NULL
  },
  /* 932 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMinss, 0x00,
    2, g_Operands + 469,
    NULL
  },
  /* 933 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstDivss, 0x00,
    2, g_Operands + 469,
    NULL
  },
  /* 934 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMaxss, 0x00,
    2, g_Operands + 469,
    NULL
  },
  /* 935 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 936 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 937 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 938 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 939 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 940 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 941 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 942 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 943 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 944 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 945 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 946 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 947 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 948 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 949 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 950 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMovdqu, 0x00,
    2, g_Operands + 475,
    NULL
  },
  /* 951 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsRep),
    InstPshufhw, 0x00,
    3, g_Operands + 439,
    NULL
  },
  /* 952 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 953 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 954 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 955 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 956 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 957 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 958 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 959 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 960 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 961 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 962 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 963 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 964 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 965 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMovq, 0x00,
    2, g_Operands + 439,
    NULL
  },
  /* 966 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMovdqu, 0x00,
    2, g_Operands + 477,
    NULL
  },
  /* 967 */
  { NACLi_POPCNT,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstPopcnt, 0x00,
    2, g_Operands + 399,
    NULL
  },
  /* 968 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 969 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 970 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 971 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 972 */
  { NACLi_LZCNT,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstLzcnt, 0x00,
    2, g_Operands + 399,
    NULL
  },
  /* 973 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 974 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 975 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsRep),
    InstCmpss, 0x00,
    3, g_Operands + 479,
    NULL
  },
  /* 976 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 977 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 978 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 979 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 980 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 981 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 982 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 983 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 984 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 985 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 986 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMovq2dq, 0x00,
    2, g_Operands + 482,
    NULL
  },
  /* 987 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 988 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 989 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 990 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 991 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 992 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 993 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 994 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 995 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 996 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 997 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 998 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 999 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1000 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1001 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1002 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstCvtdq2pd, 0x00,
    2, g_Operands + 484,
    NULL
  },
  /* 1003 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1004 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1005 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1006 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1007 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1008 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1009 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1010 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1011 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1012 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1013 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1014 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1015 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1016 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1017 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1018 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1019 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1020 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1021 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1022 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1023 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1024 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1025 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1026 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1027 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1028 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovupd, 0x00,
    2, g_Operands + 486,
    NULL
  },
  /* 1029 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovupd, 0x00,
    2, g_Operands + 488,
    NULL
  },
  /* 1030 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovlpd, 0x00,
    2, g_Operands + 490,
    NULL
  },
  /* 1031 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovlpd, 0x00,
    2, g_Operands + 431,
    NULL
  },
  /* 1032 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstUnpcklpd, 0x00,
    2, g_Operands + 492,
    NULL
  },
  /* 1033 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstUnpckhpd, 0x00,
    2, g_Operands + 492,
    NULL
  },
  /* 1034 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovhpd, 0x00,
    2, g_Operands + 490,
    NULL
  },
  /* 1035 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovhpd, 0x00,
    2, g_Operands + 431,
    NULL
  },
  /* 1036 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovapd, 0x00,
    2, g_Operands + 486,
    NULL
  },
  /* 1037 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovapd, 0x00,
    2, g_Operands + 488,
    NULL
  },
  /* 1038 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstCvtpi2pd, 0x00,
    2, g_Operands + 494,
    NULL
  },
  /* 1039 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovntpd, 0x00,
    2, g_Operands + 496,
    NULL
  },
  /* 1040 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstCvttpd2pi, 0x00,
    2, g_Operands + 498,
    NULL
  },
  /* 1041 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstCvtpd2pi, 0x00,
    2, g_Operands + 498,
    NULL
  },
  /* 1042 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstUcomisd, 0x00,
    2, g_Operands + 500,
    NULL
  },
  /* 1043 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstComisd, 0x00,
    2, g_Operands + 502,
    NULL
  },
  /* 1044 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovmskpd, 0x00,
    2, g_Operands + 504,
    NULL
  },
  /* 1045 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstSqrtpd, 0x00,
    2, g_Operands + 506,
    NULL
  },
  /* 1046 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1047 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1048 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstAndpd, 0x00,
    2, g_Operands + 451,
    NULL
  },
  /* 1049 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstAndnpd, 0x00,
    2, g_Operands + 451,
    NULL
  },
  /* 1050 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstOrpd, 0x00,
    2, g_Operands + 451,
    NULL
  },
  /* 1051 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstXorpd, 0x00,
    2, g_Operands + 451,
    NULL
  },
  /* 1052 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstAddpd, 0x00,
    2, g_Operands + 451,
    NULL
  },
  /* 1053 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMulpd, 0x00,
    2, g_Operands + 451,
    NULL
  },
  /* 1054 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstCvtpd2ps, 0x00,
    2, g_Operands + 506,
    NULL
  },
  /* 1055 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstCvtps2dq, 0x00,
    2, g_Operands + 473,
    NULL
  },
  /* 1056 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstSubpd, 0x00,
    2, g_Operands + 451,
    NULL
  },
  /* 1057 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMinpd, 0x00,
    2, g_Operands + 451,
    NULL
  },
  /* 1058 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstDivpd, 0x00,
    2, g_Operands + 451,
    NULL
  },
  /* 1059 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMaxpd, 0x00,
    2, g_Operands + 451,
    NULL
  },
  /* 1060 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPunpcklbw, 0x00,
    2, g_Operands + 508,
    NULL
  },
  /* 1061 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPunpcklwd, 0x00,
    2, g_Operands + 508,
    NULL
  },
  /* 1062 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPunpckldq, 0x00,
    2, g_Operands + 508,
    NULL
  },
  /* 1063 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPacksswb, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1064 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPcmpgtb, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1065 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPcmpgtw, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1066 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPcmpgtd, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1067 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPackuswb, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1068 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPunpckhbw, 0x00,
    2, g_Operands + 508,
    NULL
  },
  /* 1069 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPunpckhwd, 0x00,
    2, g_Operands + 508,
    NULL
  },
  /* 1070 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPunpckhdq, 0x00,
    2, g_Operands + 508,
    NULL
  },
  /* 1071 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPackssdw, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1072 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPunpcklqdq, 0x00,
    2, g_Operands + 508,
    NULL
  },
  /* 1073 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPunpckhqdq, 0x00,
    2, g_Operands + 508,
    NULL
  },
  /* 1074 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstMovd, 0x00,
    2, g_Operands + 512,
    g_Opcodes + 1075
  },
  /* 1075 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_o),
    InstMovq, 0x00,
    2, g_Operands + 514,
    NULL
  },
  /* 1076 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovdqa, 0x00,
    2, g_Operands + 475,
    NULL
  },
  /* 1077 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPshufd, 0x00,
    3, g_Operands + 516,
    NULL
  },
  /* 1078 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    g_Opcodes + 1079
  },
  /* 1079 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x01,
    0, g_Operands + 0,
    g_Opcodes + 1080
  },
  /* 1080 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsrlw, 0x02,
    2, g_Operands + 519,
    g_Opcodes + 1081
  },
  /* 1081 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x03,
    0, g_Operands + 0,
    g_Opcodes + 1082
  },
  /* 1082 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsraw, 0x04,
    2, g_Operands + 519,
    g_Opcodes + 1083
  },
  /* 1083 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x05,
    0, g_Operands + 0,
    g_Opcodes + 1084
  },
  /* 1084 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsllw, 0x06,
    2, g_Operands + 519,
    g_Opcodes + 1085
  },
  /* 1085 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x07,
    0, g_Operands + 0,
    NULL
  },
  /* 1086 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    g_Opcodes + 1087
  },
  /* 1087 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x01,
    0, g_Operands + 0,
    g_Opcodes + 1088
  },
  /* 1088 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsrld, 0x02,
    2, g_Operands + 519,
    g_Opcodes + 1089
  },
  /* 1089 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x03,
    0, g_Operands + 0,
    g_Opcodes + 1090
  },
  /* 1090 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsrad, 0x04,
    2, g_Operands + 519,
    g_Opcodes + 1091
  },
  /* 1091 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x05,
    0, g_Operands + 0,
    g_Opcodes + 1092
  },
  /* 1092 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPslld, 0x06,
    2, g_Operands + 519,
    g_Opcodes + 1093
  },
  /* 1093 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x07,
    0, g_Operands + 0,
    NULL
  },
  /* 1094 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    g_Opcodes + 1095
  },
  /* 1095 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x01,
    0, g_Operands + 0,
    g_Opcodes + 1096
  },
  /* 1096 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsrlq, 0x02,
    2, g_Operands + 519,
    g_Opcodes + 1097
  },
  /* 1097 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsrldq, 0x03,
    2, g_Operands + 519,
    g_Opcodes + 1098
  },
  /* 1098 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x04,
    0, g_Operands + 0,
    g_Opcodes + 1099
  },
  /* 1099 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x05,
    0, g_Operands + 0,
    g_Opcodes + 1100
  },
  /* 1100 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsllq, 0x06,
    2, g_Operands + 519,
    g_Opcodes + 1101
  },
  /* 1101 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPslldq, 0x07,
    2, g_Operands + 519,
    NULL
  },
  /* 1102 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPcmpeqb, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1103 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPcmpeqw, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1104 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPcmpeqd, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1105 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1106 */
  { NACLi_SSE4A,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeHasImmed2_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstExtrq, 0x00,
    3, g_Operands + 521,
    g_Opcodes + 1107
  },
  /* 1107 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1108 */
  { NACLi_SSE4A,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstExtrq, 0x00,
    2, g_Operands + 446,
    NULL
  },
  /* 1109 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1110 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1111 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstHaddpd, 0x00,
    2, g_Operands + 451,
    NULL
  },
  /* 1112 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstHsubpd, 0x00,
    2, g_Operands + 451,
    NULL
  },
  /* 1113 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstMovd, 0x00,
    2, g_Operands + 524,
    g_Opcodes + 1114
  },
  /* 1114 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_o),
    InstMovq, 0x00,
    2, g_Operands + 526,
    NULL
  },
  /* 1115 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovdqa, 0x00,
    2, g_Operands + 477,
    NULL
  },
  /* 1116 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1117 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstCmppd, 0x00,
    3, g_Operands + 528,
    NULL
  },
  /* 1118 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1119 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstPinsrw, 0x00,
    3, g_Operands + 531,
    NULL
  },
  /* 1120 */
  { NACLi_SSE41,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPextrw, 0x00,
    3, g_Operands + 534,
    NULL
  },
  /* 1121 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstShufpd, 0x00,
    3, g_Operands + 528,
    NULL
  },
  /* 1122 */
  { NACLi_SSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstAddsubpd, 0x00,
    2, g_Operands + 451,
    NULL
  },
  /* 1123 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsrlw, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1124 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsrld, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1125 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsrlq, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1126 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPaddq, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1127 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmullw, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1128 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovq, 0x00,
    2, g_Operands + 537,
    NULL
  },
  /* 1129 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovmskb, 0x00,
    2, g_Operands + 534,
    NULL
  },
  /* 1130 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsubusb, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1131 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsubusw, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1132 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPminub, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1133 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPand, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1134 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPaddusb, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1135 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPaddusw, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1136 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmaxub, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1137 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPandn, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1138 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPavgb, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1139 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsraw, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1140 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsrad, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1141 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPavgw, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1142 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmulhuw, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1143 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmulhw, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1144 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstCvttpd2dq, 0x00,
    2, g_Operands + 539,
    NULL
  },
  /* 1145 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovntdq, 0x00,
    2, g_Operands + 541,
    NULL
  },
  /* 1146 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsubsb, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1147 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsubsw, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1148 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPminsw, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1149 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPor, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1150 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPaddsb, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1151 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPaddsw, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1152 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmaxsw, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1153 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPxor, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1154 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1155 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsllw, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1156 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPslld, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1157 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsllq, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1158 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmuludq, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1159 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmaddwd, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1160 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsadbw, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1161 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstMaskmovdqu, 0x00,
    3, g_Operands + 543,
    NULL
  },
  /* 1162 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsubb, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1163 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsubw, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1164 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsubd, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1165 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsubq, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1166 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPaddb, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1167 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPaddw, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1168 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPaddd, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1169 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1170 */
  { NACLi_E3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPi2fw, 0x00,
    2, g_Operands + 350,
    NULL
  },
  /* 1171 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPi2fd, 0x00,
    2, g_Operands + 350,
    NULL
  },
  /* 1172 */
  { NACLi_E3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPf2iw, 0x00,
    2, g_Operands + 350,
    NULL
  },
  /* 1173 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPf2id, 0x00,
    2, g_Operands + 350,
    NULL
  },
  /* 1174 */
  { NACLi_E3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfnacc, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 1175 */
  { NACLi_E3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfpnacc, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 1176 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfcmpge, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 1177 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfmin, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 1178 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfrcp, 0x00,
    2, g_Operands + 350,
    NULL
  },
  /* 1179 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfrsqrt, 0x00,
    2, g_Operands + 350,
    NULL
  },
  /* 1180 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfsub, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 1181 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfadd, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 1182 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfcmpgt, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 1183 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfmax, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 1184 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfrcpit1, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 1185 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfrsqit1, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 1186 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfsubr, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 1187 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfacc, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 1188 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfcmpeq, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 1189 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfmul, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 1190 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfrcpit2, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 1191 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPmulhrw, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 1192 */
  { NACLi_E3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPswapd, 0x00,
    2, g_Operands + 350,
    NULL
  },
  /* 1193 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPavgusb, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 1194 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPshufb, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 1195 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPhaddw, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 1196 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPhaddd, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 1197 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPhaddsw, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 1198 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPmaddubsw, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 1199 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPhsubw, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 1200 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPhsubd, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 1201 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPhsubsw, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 1202 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsignb, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 1203 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsignw, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 1204 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsignd, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 1205 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPmulhrsw, 0x00,
    2, g_Operands + 342,
    NULL
  },
  /* 1206 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1207 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1208 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1209 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1210 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1211 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1212 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1213 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1214 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1215 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1216 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1217 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1218 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1219 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1220 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1221 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1222 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPabsb, 0x00,
    2, g_Operands + 350,
    NULL
  },
  /* 1223 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPabsw, 0x00,
    2, g_Operands + 350,
    NULL
  },
  /* 1224 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPabsd, 0x00,
    2, g_Operands + 350,
    NULL
  },
  /* 1225 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1226 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1227 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1228 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1229 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1230 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1231 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1232 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1233 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1234 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1235 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1236 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1237 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1238 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1239 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1240 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1241 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1242 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1243 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1244 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1245 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1246 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1247 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1248 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1249 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1250 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1251 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1252 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1253 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1254 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1255 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1256 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1257 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1258 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1259 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1260 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1261 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1262 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1263 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1264 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1265 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1266 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1267 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1268 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1269 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1270 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1271 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1272 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1273 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1274 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1275 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1276 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1277 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1278 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1279 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1280 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1281 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1282 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1283 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1284 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1285 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1286 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1287 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1288 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1289 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1290 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1291 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1292 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1293 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1294 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1295 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1296 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1297 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1298 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1299 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1300 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1301 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1302 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1303 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1304 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1305 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1306 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1307 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1308 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1309 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1310 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1311 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1312 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1313 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1314 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1315 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1316 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1317 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1318 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1319 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1320 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1321 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1322 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1323 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1324 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1325 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1326 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1327 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1328 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1329 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1330 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1331 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1332 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1333 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1334 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1335 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1336 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1337 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1338 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1339 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1340 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1341 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1342 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1343 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1344 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1345 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1346 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1347 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1348 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1349 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1350 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1351 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1352 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1353 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1354 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1355 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1356 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1357 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1358 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1359 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1360 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1361 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1362 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1363 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1364 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1365 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1366 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1367 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1368 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1369 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1370 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1371 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1372 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1373 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1374 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1375 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1376 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1377 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1378 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1379 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1380 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1381 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1382 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1383 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1384 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1385 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1386 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1387 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1388 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1389 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1390 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1391 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1392 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1393 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1394 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1395 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1396 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1397 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1398 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1399 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1400 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1401 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1402 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1403 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1404 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1405 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1406 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1407 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1408 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1409 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1410 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1411 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1412 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1413 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1414 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1415 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1416 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1417 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1418 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1419 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1420 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1421 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1422 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1423 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1424 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1425 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1426 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1427 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1428 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1429 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1430 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1431 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1432 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1433 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1434 */
  { NACLi_MOVBE,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstMovbe, 0x00,
    2, g_Operands + 546,
    NULL
  },
  /* 1435 */
  { NACLi_MOVBE,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstMovbe, 0x00,
    2, g_Operands + 548,
    NULL
  },
  /* 1436 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1437 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1438 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1439 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1440 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1441 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1442 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1443 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1444 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1445 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1446 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1447 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1448 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1449 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1450 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPshufb, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1451 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPhaddw, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1452 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPhaddd, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1453 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPhaddsw, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1454 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmaddubsw, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1455 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPhsubw, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1456 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPhsubd, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1457 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPhsubsw, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1458 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsignb, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1459 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsignw, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1460 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsignd, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1461 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmulhrsw, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1462 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1463 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1464 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1465 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1466 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPblendvb, 0x00,
    3, g_Operands + 550,
    NULL
  },
  /* 1467 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1468 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1469 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1470 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstBlendvps, 0x00,
    3, g_Operands + 550,
    NULL
  },
  /* 1471 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstBlendvpd, 0x00,
    3, g_Operands + 550,
    NULL
  },
  /* 1472 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1473 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPtest, 0x00,
    2, g_Operands + 553,
    NULL
  },
  /* 1474 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1475 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1476 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1477 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1478 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPabsb, 0x00,
    2, g_Operands + 475,
    NULL
  },
  /* 1479 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPabsw, 0x00,
    2, g_Operands + 475,
    NULL
  },
  /* 1480 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPabsd, 0x00,
    2, g_Operands + 475,
    NULL
  },
  /* 1481 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1482 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovsxbw, 0x00,
    2, g_Operands + 555,
    NULL
  },
  /* 1483 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovsxbd, 0x00,
    2, g_Operands + 557,
    NULL
  },
  /* 1484 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovsxbq, 0x00,
    2, g_Operands + 559,
    NULL
  },
  /* 1485 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovsxwd, 0x00,
    2, g_Operands + 555,
    NULL
  },
  /* 1486 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovsxwq, 0x00,
    2, g_Operands + 557,
    NULL
  },
  /* 1487 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovsxdq, 0x00,
    2, g_Operands + 555,
    NULL
  },
  /* 1488 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1489 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1490 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmuldq, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1491 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPcmpeqq, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1492 */
  { NACLi_SSE41,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovntdqa, 0x00,
    2, g_Operands + 457,
    NULL
  },
  /* 1493 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPackusdw, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1494 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1495 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1496 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1497 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1498 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovzxbw, 0x00,
    2, g_Operands + 555,
    NULL
  },
  /* 1499 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovzxbd, 0x00,
    2, g_Operands + 557,
    NULL
  },
  /* 1500 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovzxbq, 0x00,
    2, g_Operands + 559,
    NULL
  },
  /* 1501 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovzxwd, 0x00,
    2, g_Operands + 555,
    NULL
  },
  /* 1502 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovzxwq, 0x00,
    2, g_Operands + 557,
    NULL
  },
  /* 1503 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovzxdq, 0x00,
    2, g_Operands + 555,
    NULL
  },
  /* 1504 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1505 */
  { NACLi_SSE42,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPcmpgtq, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1506 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPminsb, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1507 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPminsd, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1508 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPminuw, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1509 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPminud, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1510 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmaxsb, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1511 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmaxsd, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1512 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmaxuw, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1513 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmaxud, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1514 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmulld, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1515 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPhminposuw, 0x00,
    2, g_Operands + 510,
    NULL
  },
  /* 1516 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1517 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1518 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1519 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1520 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1521 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1522 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1523 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1524 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1525 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1526 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1527 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1528 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1529 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1530 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1531 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1532 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1533 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1534 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1535 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1536 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1537 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1538 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1539 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1540 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1541 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1542 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1543 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1544 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1545 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1546 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1547 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1548 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1549 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1550 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1551 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1552 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1553 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1554 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1555 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1556 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1557 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1558 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1559 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1560 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1561 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1562 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1563 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1564 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1565 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1566 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1567 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1568 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1569 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1570 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1571 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1572 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1573 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1574 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1575 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1576 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1577 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1578 */
  { NACLi_VMX,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvept, 0x00,
    2, g_Operands + 561,
    NULL
  },
  /* 1579 */
  { NACLi_VMX,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvvpid, 0x00,
    2, g_Operands + 561,
    NULL
  },
  /* 1580 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1581 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1582 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1583 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1584 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1585 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1586 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1587 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1588 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1589 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1590 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1591 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1592 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1593 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1594 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1595 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1596 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1597 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1598 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1599 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1600 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1601 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1602 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1603 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1604 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1605 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1606 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1607 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1608 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1609 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1610 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1611 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1612 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1613 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1614 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1615 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1616 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1617 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1618 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1619 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1620 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1621 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1622 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1623 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1624 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1625 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1626 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1627 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1628 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1629 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1630 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1631 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1632 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1633 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1634 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1635 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1636 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1637 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1638 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1639 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1640 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1641 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1642 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1643 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1644 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1645 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1646 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1647 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1648 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1649 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1650 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1651 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1652 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1653 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1654 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1655 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1656 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1657 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1658 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1659 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1660 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1661 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1662 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1663 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1664 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1665 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1666 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1667 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1668 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1669 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1670 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1671 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1672 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1673 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1674 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1675 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1676 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1677 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1678 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1679 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1680 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1681 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1682 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1683 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1684 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1685 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1686 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1687 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1688 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1689 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1690 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1691 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1692 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1693 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1694 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1695 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1696 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1697 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1698 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1699 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1700 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1701 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1702 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1703 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1704 */
  { NACLi_SSE42,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(OperandSize_b),
    InstCrc32, 0x00,
    2, g_Operands + 563,
    NULL
  },
  /* 1705 */
  { NACLi_SSE42,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCrc32, 0x00,
    2, g_Operands + 565,
    NULL
  },
  /* 1706 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstPalignr, 0x00,
    3, g_Operands + 567,
    NULL
  },
  /* 1707 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstRoundps, 0x00,
    3, g_Operands + 516,
    NULL
  },
  /* 1708 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstRoundpd, 0x00,
    3, g_Operands + 516,
    NULL
  },
  /* 1709 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstRoundss, 0x00,
    3, g_Operands + 570,
    NULL
  },
  /* 1710 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstRoundsd, 0x00,
    3, g_Operands + 573,
    NULL
  },
  /* 1711 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstBlendps, 0x00,
    3, g_Operands + 576,
    NULL
  },
  /* 1712 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstBlendpd, 0x00,
    3, g_Operands + 576,
    NULL
  },
  /* 1713 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPblendw, 0x00,
    3, g_Operands + 576,
    NULL
  },
  /* 1714 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPalignr, 0x00,
    3, g_Operands + 576,
    NULL
  },
  /* 1715 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPextrb, 0x00,
    3, g_Operands + 579,
    NULL
  },
  /* 1716 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPextrw, 0x00,
    3, g_Operands + 582,
    NULL
  },
  /* 1717 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstPextrd, 0x00,
    3, g_Operands + 585,
    g_Opcodes + 1718
  },
  /* 1718 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_o),
    InstPextrq, 0x00,
    3, g_Operands + 588,
    NULL
  },
  /* 1719 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstExtractps, 0x00,
    3, g_Operands + 591,
    NULL
  },
  /* 1720 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstPinsrb, 0x00,
    3, g_Operands + 594,
    NULL
  },
  /* 1721 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstInsertps, 0x00,
    3, g_Operands + 597,
    NULL
  },
  /* 1722 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstPinsrd, 0x00,
    3, g_Operands + 600,
    g_Opcodes + 1723
  },
  /* 1723 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_o),
    InstPinsrq, 0x00,
    3, g_Operands + 603,
    NULL
  },
  /* 1724 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstDpps, 0x00,
    3, g_Operands + 576,
    NULL
  },
  /* 1725 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstDppd, 0x00,
    3, g_Operands + 576,
    NULL
  },
  /* 1726 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMpsadbw, 0x00,
    3, g_Operands + 576,
    NULL
  },
  /* 1727 */
  { NACLi_SSE42,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstPcmpestrm, 0x00,
    6, g_Operands + 606,
    NULL
  },
  /* 1728 */
  { NACLi_SSE42,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstPcmpestri, 0x00,
    6, g_Operands + 612,
    NULL
  },
  /* 1729 */
  { NACLi_SSE42,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPcmpistrm, 0x00,
    4, g_Operands + 618,
    NULL
  },
  /* 1730 */
  { NACLi_SSE42,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstPcmpistri, 0x00,
    4, g_Operands + 622,
    NULL
  },
  /* 1731 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00,
    2, g_Operands + 626,
    NULL
  },
  /* 1732 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00,
    2, g_Operands + 628,
    NULL
  },
  /* 1733 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00,
    2, g_Operands + 630,
    NULL
  },
  /* 1734 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00,
    2, g_Operands + 632,
    NULL
  },
  /* 1735 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00,
    2, g_Operands + 634,
    NULL
  },
  /* 1736 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00,
    2, g_Operands + 636,
    NULL
  },
  /* 1737 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00,
    2, g_Operands + 638,
    NULL
  },
  /* 1738 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00,
    2, g_Operands + 640,
    NULL
  },
  /* 1739 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00,
    2, g_Operands + 626,
    NULL
  },
  /* 1740 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00,
    2, g_Operands + 628,
    NULL
  },
  /* 1741 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00,
    2, g_Operands + 630,
    NULL
  },
  /* 1742 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00,
    2, g_Operands + 632,
    NULL
  },
  /* 1743 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00,
    2, g_Operands + 634,
    NULL
  },
  /* 1744 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00,
    2, g_Operands + 636,
    NULL
  },
  /* 1745 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00,
    2, g_Operands + 638,
    NULL
  },
  /* 1746 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00,
    2, g_Operands + 640,
    NULL
  },
  /* 1747 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcom, 0x00,
    2, g_Operands + 642,
    NULL
  },
  /* 1748 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcom, 0x00,
    2, g_Operands + 644,
    NULL
  },
  /* 1749 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcom, 0x00,
    2, g_Operands + 646,
    NULL
  },
  /* 1750 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcom, 0x00,
    2, g_Operands + 648,
    NULL
  },
  /* 1751 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcom, 0x00,
    2, g_Operands + 650,
    NULL
  },
  /* 1752 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcom, 0x00,
    2, g_Operands + 652,
    NULL
  },
  /* 1753 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcom, 0x00,
    2, g_Operands + 654,
    NULL
  },
  /* 1754 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcom, 0x00,
    2, g_Operands + 656,
    NULL
  },
  /* 1755 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomp, 0x00,
    2, g_Operands + 642,
    NULL
  },
  /* 1756 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomp, 0x00,
    2, g_Operands + 644,
    NULL
  },
  /* 1757 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomp, 0x00,
    2, g_Operands + 646,
    NULL
  },
  /* 1758 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomp, 0x00,
    2, g_Operands + 648,
    NULL
  },
  /* 1759 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomp, 0x00,
    2, g_Operands + 650,
    NULL
  },
  /* 1760 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomp, 0x00,
    2, g_Operands + 652,
    NULL
  },
  /* 1761 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomp, 0x00,
    2, g_Operands + 654,
    NULL
  },
  /* 1762 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomp, 0x00,
    2, g_Operands + 656,
    NULL
  },
  /* 1763 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00,
    2, g_Operands + 626,
    NULL
  },
  /* 1764 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00,
    2, g_Operands + 628,
    NULL
  },
  /* 1765 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00,
    2, g_Operands + 630,
    NULL
  },
  /* 1766 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00,
    2, g_Operands + 632,
    NULL
  },
  /* 1767 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00,
    2, g_Operands + 634,
    NULL
  },
  /* 1768 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00,
    2, g_Operands + 636,
    NULL
  },
  /* 1769 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00,
    2, g_Operands + 638,
    NULL
  },
  /* 1770 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00,
    2, g_Operands + 640,
    NULL
  },
  /* 1771 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00,
    2, g_Operands + 626,
    NULL
  },
  /* 1772 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00,
    2, g_Operands + 628,
    NULL
  },
  /* 1773 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00,
    2, g_Operands + 630,
    NULL
  },
  /* 1774 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00,
    2, g_Operands + 632,
    NULL
  },
  /* 1775 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00,
    2, g_Operands + 634,
    NULL
  },
  /* 1776 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00,
    2, g_Operands + 636,
    NULL
  },
  /* 1777 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00,
    2, g_Operands + 638,
    NULL
  },
  /* 1778 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00,
    2, g_Operands + 640,
    NULL
  },
  /* 1779 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00,
    2, g_Operands + 626,
    NULL
  },
  /* 1780 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00,
    2, g_Operands + 628,
    NULL
  },
  /* 1781 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00,
    2, g_Operands + 630,
    NULL
  },
  /* 1782 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00,
    2, g_Operands + 632,
    NULL
  },
  /* 1783 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00,
    2, g_Operands + 634,
    NULL
  },
  /* 1784 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00,
    2, g_Operands + 636,
    NULL
  },
  /* 1785 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00,
    2, g_Operands + 638,
    NULL
  },
  /* 1786 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00,
    2, g_Operands + 640,
    NULL
  },
  /* 1787 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00,
    2, g_Operands + 626,
    NULL
  },
  /* 1788 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00,
    2, g_Operands + 628,
    NULL
  },
  /* 1789 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00,
    2, g_Operands + 630,
    NULL
  },
  /* 1790 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00,
    2, g_Operands + 632,
    NULL
  },
  /* 1791 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00,
    2, g_Operands + 634,
    NULL
  },
  /* 1792 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00,
    2, g_Operands + 636,
    NULL
  },
  /* 1793 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00,
    2, g_Operands + 638,
    NULL
  },
  /* 1794 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00,
    2, g_Operands + 640,
    NULL
  },
  /* 1795 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFld, 0x00,
    2, g_Operands + 658,
    NULL
  },
  /* 1796 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFld, 0x00,
    2, g_Operands + 660,
    NULL
  },
  /* 1797 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFld, 0x00,
    2, g_Operands + 662,
    NULL
  },
  /* 1798 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFld, 0x00,
    2, g_Operands + 664,
    NULL
  },
  /* 1799 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFld, 0x00,
    2, g_Operands + 666,
    NULL
  },
  /* 1800 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFld, 0x00,
    2, g_Operands + 668,
    NULL
  },
  /* 1801 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFld, 0x00,
    2, g_Operands + 670,
    NULL
  },
  /* 1802 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFld, 0x00,
    2, g_Operands + 672,
    NULL
  },
  /* 1803 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFxch, 0x00,
    2, g_Operands + 674,
    NULL
  },
  /* 1804 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFxch, 0x00,
    2, g_Operands + 676,
    NULL
  },
  /* 1805 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFxch, 0x00,
    2, g_Operands + 678,
    NULL
  },
  /* 1806 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFxch, 0x00,
    2, g_Operands + 680,
    NULL
  },
  /* 1807 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFxch, 0x00,
    2, g_Operands + 682,
    NULL
  },
  /* 1808 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFxch, 0x00,
    2, g_Operands + 684,
    NULL
  },
  /* 1809 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFxch, 0x00,
    2, g_Operands + 686,
    NULL
  },
  /* 1810 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFxch, 0x00,
    2, g_Operands + 688,
    NULL
  },
  /* 1811 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFnop, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1812 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1813 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1814 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1815 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1816 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1817 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1818 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1819 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1820 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1821 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1822 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1823 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1824 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1825 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1826 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1827 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFchs, 0x00,
    1, g_Operands + 179,
    NULL
  },
  /* 1828 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFabs, 0x00,
    1, g_Operands + 179,
    NULL
  },
  /* 1829 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1830 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1831 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFtst, 0x00,
    1, g_Operands + 181,
    NULL
  },
  /* 1832 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFxam, 0x00,
    1, g_Operands + 181,
    NULL
  },
  /* 1833 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1834 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1835 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFld1, 0x00,
    1, g_Operands + 179,
    NULL
  },
  /* 1836 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFldl2t, 0x00,
    1, g_Operands + 179,
    NULL
  },
  /* 1837 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFldl2e, 0x00,
    1, g_Operands + 179,
    NULL
  },
  /* 1838 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFldpi, 0x00,
    1, g_Operands + 179,
    NULL
  },
  /* 1839 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFldlg2, 0x00,
    1, g_Operands + 179,
    NULL
  },
  /* 1840 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFldln2, 0x00,
    1, g_Operands + 179,
    NULL
  },
  /* 1841 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFldz, 0x00,
    1, g_Operands + 179,
    NULL
  },
  /* 1842 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1843 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstF2xm1, 0x00,
    1, g_Operands + 179,
    NULL
  },
  /* 1844 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFyl2x, 0x00,
    2, g_Operands + 628,
    NULL
  },
  /* 1845 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFptan, 0x00,
    2, g_Operands + 660,
    NULL
  },
  /* 1846 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFpatan, 0x00,
    2, g_Operands + 628,
    NULL
  },
  /* 1847 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFxtract, 0x00,
    2, g_Operands + 660,
    NULL
  },
  /* 1848 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFprem1, 0x00,
    2, g_Operands + 628,
    NULL
  },
  /* 1849 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdecstp, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1850 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFincstp, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1851 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFprem, 0x00,
    2, g_Operands + 628,
    NULL
  },
  /* 1852 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFyl2xp1, 0x00,
    2, g_Operands + 628,
    NULL
  },
  /* 1853 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsqrt, 0x00,
    1, g_Operands + 179,
    NULL
  },
  /* 1854 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsincos, 0x00,
    2, g_Operands + 660,
    NULL
  },
  /* 1855 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFrndint, 0x00,
    1, g_Operands + 179,
    NULL
  },
  /* 1856 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFscale, 0x00,
    2, g_Operands + 628,
    NULL
  },
  /* 1857 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsin, 0x00,
    1, g_Operands + 179,
    NULL
  },
  /* 1858 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcos, 0x00,
    1, g_Operands + 179,
    NULL
  },
  /* 1859 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovb, 0x00,
    2, g_Operands + 626,
    NULL
  },
  /* 1860 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovb, 0x00,
    2, g_Operands + 628,
    NULL
  },
  /* 1861 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovb, 0x00,
    2, g_Operands + 630,
    NULL
  },
  /* 1862 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovb, 0x00,
    2, g_Operands + 632,
    NULL
  },
  /* 1863 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovb, 0x00,
    2, g_Operands + 634,
    NULL
  },
  /* 1864 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovb, 0x00,
    2, g_Operands + 636,
    NULL
  },
  /* 1865 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovb, 0x00,
    2, g_Operands + 638,
    NULL
  },
  /* 1866 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovb, 0x00,
    2, g_Operands + 640,
    NULL
  },
  /* 1867 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmove, 0x00,
    2, g_Operands + 626,
    NULL
  },
  /* 1868 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmove, 0x00,
    2, g_Operands + 628,
    NULL
  },
  /* 1869 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmove, 0x00,
    2, g_Operands + 630,
    NULL
  },
  /* 1870 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmove, 0x00,
    2, g_Operands + 632,
    NULL
  },
  /* 1871 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmove, 0x00,
    2, g_Operands + 634,
    NULL
  },
  /* 1872 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmove, 0x00,
    2, g_Operands + 636,
    NULL
  },
  /* 1873 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmove, 0x00,
    2, g_Operands + 638,
    NULL
  },
  /* 1874 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmove, 0x00,
    2, g_Operands + 640,
    NULL
  },
  /* 1875 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovbe, 0x00,
    2, g_Operands + 626,
    NULL
  },
  /* 1876 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovbe, 0x00,
    2, g_Operands + 628,
    NULL
  },
  /* 1877 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovbe, 0x00,
    2, g_Operands + 630,
    NULL
  },
  /* 1878 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovbe, 0x00,
    2, g_Operands + 632,
    NULL
  },
  /* 1879 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovbe, 0x00,
    2, g_Operands + 634,
    NULL
  },
  /* 1880 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovbe, 0x00,
    2, g_Operands + 636,
    NULL
  },
  /* 1881 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovbe, 0x00,
    2, g_Operands + 638,
    NULL
  },
  /* 1882 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovbe, 0x00,
    2, g_Operands + 640,
    NULL
  },
  /* 1883 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovu, 0x00,
    2, g_Operands + 626,
    NULL
  },
  /* 1884 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovu, 0x00,
    2, g_Operands + 628,
    NULL
  },
  /* 1885 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovu, 0x00,
    2, g_Operands + 630,
    NULL
  },
  /* 1886 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovu, 0x00,
    2, g_Operands + 632,
    NULL
  },
  /* 1887 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovu, 0x00,
    2, g_Operands + 634,
    NULL
  },
  /* 1888 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovu, 0x00,
    2, g_Operands + 636,
    NULL
  },
  /* 1889 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovu, 0x00,
    2, g_Operands + 638,
    NULL
  },
  /* 1890 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovu, 0x00,
    2, g_Operands + 640,
    NULL
  },
  /* 1891 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1892 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1893 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1894 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1895 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1896 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1897 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1898 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1899 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1900 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucompp, 0x00,
    2, g_Operands + 644,
    NULL
  },
  /* 1901 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1902 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1903 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1904 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1905 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1906 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1907 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1908 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1909 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1910 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1911 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1912 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1913 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1914 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1915 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1916 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1917 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1918 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1919 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1920 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1921 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1922 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1923 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnb, 0x00,
    2, g_Operands + 626,
    NULL
  },
  /* 1924 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnb, 0x00,
    2, g_Operands + 628,
    NULL
  },
  /* 1925 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnb, 0x00,
    2, g_Operands + 630,
    NULL
  },
  /* 1926 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnb, 0x00,
    2, g_Operands + 632,
    NULL
  },
  /* 1927 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnb, 0x00,
    2, g_Operands + 634,
    NULL
  },
  /* 1928 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnb, 0x00,
    2, g_Operands + 636,
    NULL
  },
  /* 1929 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnb, 0x00,
    2, g_Operands + 638,
    NULL
  },
  /* 1930 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnb, 0x00,
    2, g_Operands + 640,
    NULL
  },
  /* 1931 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovne, 0x00,
    2, g_Operands + 626,
    NULL
  },
  /* 1932 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovne, 0x00,
    2, g_Operands + 628,
    NULL
  },
  /* 1933 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovne, 0x00,
    2, g_Operands + 630,
    NULL
  },
  /* 1934 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovne, 0x00,
    2, g_Operands + 632,
    NULL
  },
  /* 1935 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovne, 0x00,
    2, g_Operands + 634,
    NULL
  },
  /* 1936 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovne, 0x00,
    2, g_Operands + 636,
    NULL
  },
  /* 1937 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovne, 0x00,
    2, g_Operands + 638,
    NULL
  },
  /* 1938 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovne, 0x00,
    2, g_Operands + 640,
    NULL
  },
  /* 1939 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnbe, 0x00,
    2, g_Operands + 626,
    NULL
  },
  /* 1940 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnbe, 0x00,
    2, g_Operands + 628,
    NULL
  },
  /* 1941 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnbe, 0x00,
    2, g_Operands + 630,
    NULL
  },
  /* 1942 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnbe, 0x00,
    2, g_Operands + 632,
    NULL
  },
  /* 1943 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnbe, 0x00,
    2, g_Operands + 634,
    NULL
  },
  /* 1944 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnbe, 0x00,
    2, g_Operands + 636,
    NULL
  },
  /* 1945 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnbe, 0x00,
    2, g_Operands + 638,
    NULL
  },
  /* 1946 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnbe, 0x00,
    2, g_Operands + 640,
    NULL
  },
  /* 1947 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnu, 0x00,
    2, g_Operands + 626,
    NULL
  },
  /* 1948 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnu, 0x00,
    2, g_Operands + 628,
    NULL
  },
  /* 1949 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnu, 0x00,
    2, g_Operands + 630,
    NULL
  },
  /* 1950 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnu, 0x00,
    2, g_Operands + 632,
    NULL
  },
  /* 1951 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnu, 0x00,
    2, g_Operands + 634,
    NULL
  },
  /* 1952 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnu, 0x00,
    2, g_Operands + 636,
    NULL
  },
  /* 1953 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnu, 0x00,
    2, g_Operands + 638,
    NULL
  },
  /* 1954 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnu, 0x00,
    2, g_Operands + 640,
    NULL
  },
  /* 1955 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1956 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1957 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFnclex, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1958 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFninit, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1959 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1960 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1961 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1962 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1963 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomi, 0x00,
    2, g_Operands + 642,
    NULL
  },
  /* 1964 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomi, 0x00,
    2, g_Operands + 644,
    NULL
  },
  /* 1965 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomi, 0x00,
    2, g_Operands + 646,
    NULL
  },
  /* 1966 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomi, 0x00,
    2, g_Operands + 648,
    NULL
  },
  /* 1967 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomi, 0x00,
    2, g_Operands + 650,
    NULL
  },
  /* 1968 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomi, 0x00,
    2, g_Operands + 652,
    NULL
  },
  /* 1969 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomi, 0x00,
    2, g_Operands + 654,
    NULL
  },
  /* 1970 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomi, 0x00,
    2, g_Operands + 656,
    NULL
  },
  /* 1971 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomi, 0x00,
    2, g_Operands + 642,
    NULL
  },
  /* 1972 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomi, 0x00,
    2, g_Operands + 644,
    NULL
  },
  /* 1973 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomi, 0x00,
    2, g_Operands + 646,
    NULL
  },
  /* 1974 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomi, 0x00,
    2, g_Operands + 648,
    NULL
  },
  /* 1975 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomi, 0x00,
    2, g_Operands + 650,
    NULL
  },
  /* 1976 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomi, 0x00,
    2, g_Operands + 652,
    NULL
  },
  /* 1977 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomi, 0x00,
    2, g_Operands + 654,
    NULL
  },
  /* 1978 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomi, 0x00,
    2, g_Operands + 656,
    NULL
  },
  /* 1979 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00,
    2, g_Operands + 626,
    NULL
  },
  /* 1980 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00,
    2, g_Operands + 690,
    NULL
  },
  /* 1981 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00,
    2, g_Operands + 692,
    NULL
  },
  /* 1982 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00,
    2, g_Operands + 694,
    NULL
  },
  /* 1983 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00,
    2, g_Operands + 696,
    NULL
  },
  /* 1984 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00,
    2, g_Operands + 698,
    NULL
  },
  /* 1985 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00,
    2, g_Operands + 700,
    NULL
  },
  /* 1986 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00,
    2, g_Operands + 702,
    NULL
  },
  /* 1987 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00,
    2, g_Operands + 626,
    NULL
  },
  /* 1988 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00,
    2, g_Operands + 690,
    NULL
  },
  /* 1989 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00,
    2, g_Operands + 692,
    NULL
  },
  /* 1990 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00,
    2, g_Operands + 694,
    NULL
  },
  /* 1991 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00,
    2, g_Operands + 696,
    NULL
  },
  /* 1992 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00,
    2, g_Operands + 698,
    NULL
  },
  /* 1993 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00,
    2, g_Operands + 700,
    NULL
  },
  /* 1994 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00,
    2, g_Operands + 702,
    NULL
  },
  /* 1995 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1996 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1997 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1998 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1999 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2000 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2001 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2002 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2003 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2004 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2005 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2006 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2007 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2008 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2009 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2010 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2011 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00,
    2, g_Operands + 626,
    NULL
  },
  /* 2012 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00,
    2, g_Operands + 690,
    NULL
  },
  /* 2013 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00,
    2, g_Operands + 692,
    NULL
  },
  /* 2014 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00,
    2, g_Operands + 694,
    NULL
  },
  /* 2015 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00,
    2, g_Operands + 696,
    NULL
  },
  /* 2016 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00,
    2, g_Operands + 698,
    NULL
  },
  /* 2017 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00,
    2, g_Operands + 700,
    NULL
  },
  /* 2018 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00,
    2, g_Operands + 702,
    NULL
  },
  /* 2019 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00,
    2, g_Operands + 626,
    NULL
  },
  /* 2020 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00,
    2, g_Operands + 690,
    NULL
  },
  /* 2021 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00,
    2, g_Operands + 692,
    NULL
  },
  /* 2022 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00,
    2, g_Operands + 694,
    NULL
  },
  /* 2023 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00,
    2, g_Operands + 696,
    NULL
  },
  /* 2024 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00,
    2, g_Operands + 698,
    NULL
  },
  /* 2025 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00,
    2, g_Operands + 700,
    NULL
  },
  /* 2026 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00,
    2, g_Operands + 702,
    NULL
  },
  /* 2027 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00,
    2, g_Operands + 626,
    NULL
  },
  /* 2028 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00,
    2, g_Operands + 690,
    NULL
  },
  /* 2029 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00,
    2, g_Operands + 692,
    NULL
  },
  /* 2030 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00,
    2, g_Operands + 694,
    NULL
  },
  /* 2031 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00,
    2, g_Operands + 696,
    NULL
  },
  /* 2032 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00,
    2, g_Operands + 698,
    NULL
  },
  /* 2033 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00,
    2, g_Operands + 700,
    NULL
  },
  /* 2034 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00,
    2, g_Operands + 702,
    NULL
  },
  /* 2035 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00,
    2, g_Operands + 626,
    NULL
  },
  /* 2036 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00,
    2, g_Operands + 690,
    NULL
  },
  /* 2037 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00,
    2, g_Operands + 692,
    NULL
  },
  /* 2038 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00,
    2, g_Operands + 694,
    NULL
  },
  /* 2039 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00,
    2, g_Operands + 696,
    NULL
  },
  /* 2040 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00,
    2, g_Operands + 698,
    NULL
  },
  /* 2041 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00,
    2, g_Operands + 700,
    NULL
  },
  /* 2042 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00,
    2, g_Operands + 702,
    NULL
  },
  /* 2043 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFfree, 0x00,
    1, g_Operands + 704,
    NULL
  },
  /* 2044 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFfree, 0x00,
    1, g_Operands + 705,
    NULL
  },
  /* 2045 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFfree, 0x00,
    1, g_Operands + 706,
    NULL
  },
  /* 2046 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFfree, 0x00,
    1, g_Operands + 707,
    NULL
  },
  /* 2047 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFfree, 0x00,
    1, g_Operands + 708,
    NULL
  },
  /* 2048 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFfree, 0x00,
    1, g_Operands + 709,
    NULL
  },
  /* 2049 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFfree, 0x00,
    1, g_Operands + 710,
    NULL
  },
  /* 2050 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFfree, 0x00,
    1, g_Operands + 711,
    NULL
  },
  /* 2051 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2052 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2053 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2054 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2055 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2056 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2057 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2058 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2059 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFst, 0x00,
    2, g_Operands + 658,
    NULL
  },
  /* 2060 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFst, 0x00,
    2, g_Operands + 712,
    NULL
  },
  /* 2061 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFst, 0x00,
    2, g_Operands + 714,
    NULL
  },
  /* 2062 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFst, 0x00,
    2, g_Operands + 716,
    NULL
  },
  /* 2063 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFst, 0x00,
    2, g_Operands + 718,
    NULL
  },
  /* 2064 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFst, 0x00,
    2, g_Operands + 720,
    NULL
  },
  /* 2065 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFst, 0x00,
    2, g_Operands + 722,
    NULL
  },
  /* 2066 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFst, 0x00,
    2, g_Operands + 724,
    NULL
  },
  /* 2067 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFstp, 0x00,
    2, g_Operands + 658,
    NULL
  },
  /* 2068 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFstp, 0x00,
    2, g_Operands + 712,
    NULL
  },
  /* 2069 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFstp, 0x00,
    2, g_Operands + 714,
    NULL
  },
  /* 2070 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFstp, 0x00,
    2, g_Operands + 716,
    NULL
  },
  /* 2071 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFstp, 0x00,
    2, g_Operands + 718,
    NULL
  },
  /* 2072 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFstp, 0x00,
    2, g_Operands + 720,
    NULL
  },
  /* 2073 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFstp, 0x00,
    2, g_Operands + 722,
    NULL
  },
  /* 2074 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFstp, 0x00,
    2, g_Operands + 724,
    NULL
  },
  /* 2075 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucom, 0x00,
    2, g_Operands + 642,
    NULL
  },
  /* 2076 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucom, 0x00,
    2, g_Operands + 644,
    NULL
  },
  /* 2077 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucom, 0x00,
    2, g_Operands + 646,
    NULL
  },
  /* 2078 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucom, 0x00,
    2, g_Operands + 648,
    NULL
  },
  /* 2079 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucom, 0x00,
    2, g_Operands + 650,
    NULL
  },
  /* 2080 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucom, 0x00,
    2, g_Operands + 652,
    NULL
  },
  /* 2081 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucom, 0x00,
    2, g_Operands + 654,
    NULL
  },
  /* 2082 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucom, 0x00,
    2, g_Operands + 656,
    NULL
  },
  /* 2083 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomp, 0x00,
    2, g_Operands + 642,
    NULL
  },
  /* 2084 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomp, 0x00,
    2, g_Operands + 644,
    NULL
  },
  /* 2085 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomp, 0x00,
    2, g_Operands + 646,
    NULL
  },
  /* 2086 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomp, 0x00,
    2, g_Operands + 648,
    NULL
  },
  /* 2087 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomp, 0x00,
    2, g_Operands + 650,
    NULL
  },
  /* 2088 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomp, 0x00,
    2, g_Operands + 652,
    NULL
  },
  /* 2089 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomp, 0x00,
    2, g_Operands + 654,
    NULL
  },
  /* 2090 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomp, 0x00,
    2, g_Operands + 656,
    NULL
  },
  /* 2091 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2092 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2093 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2094 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2095 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2096 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2097 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2098 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2099 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2100 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2101 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2102 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2103 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2104 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2105 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2106 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2107 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFaddp, 0x00,
    2, g_Operands + 626,
    NULL
  },
  /* 2108 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFaddp, 0x00,
    2, g_Operands + 690,
    NULL
  },
  /* 2109 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFaddp, 0x00,
    2, g_Operands + 692,
    NULL
  },
  /* 2110 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFaddp, 0x00,
    2, g_Operands + 694,
    NULL
  },
  /* 2111 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFaddp, 0x00,
    2, g_Operands + 696,
    NULL
  },
  /* 2112 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFaddp, 0x00,
    2, g_Operands + 698,
    NULL
  },
  /* 2113 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFaddp, 0x00,
    2, g_Operands + 700,
    NULL
  },
  /* 2114 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFaddp, 0x00,
    2, g_Operands + 702,
    NULL
  },
  /* 2115 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmulp, 0x00,
    2, g_Operands + 626,
    NULL
  },
  /* 2116 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmulp, 0x00,
    2, g_Operands + 690,
    NULL
  },
  /* 2117 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmulp, 0x00,
    2, g_Operands + 692,
    NULL
  },
  /* 2118 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmulp, 0x00,
    2, g_Operands + 694,
    NULL
  },
  /* 2119 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmulp, 0x00,
    2, g_Operands + 696,
    NULL
  },
  /* 2120 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmulp, 0x00,
    2, g_Operands + 698,
    NULL
  },
  /* 2121 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmulp, 0x00,
    2, g_Operands + 700,
    NULL
  },
  /* 2122 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmulp, 0x00,
    2, g_Operands + 702,
    NULL
  },
  /* 2123 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2124 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2125 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2126 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2127 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2128 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2129 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2130 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2131 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2132 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcompp, 0x00,
    2, g_Operands + 644,
    NULL
  },
  /* 2133 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2134 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2135 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2136 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2137 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2138 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2139 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubrp, 0x00,
    2, g_Operands + 626,
    NULL
  },
  /* 2140 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubrp, 0x00,
    2, g_Operands + 690,
    NULL
  },
  /* 2141 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubrp, 0x00,
    2, g_Operands + 692,
    NULL
  },
  /* 2142 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubrp, 0x00,
    2, g_Operands + 694,
    NULL
  },
  /* 2143 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubrp, 0x00,
    2, g_Operands + 696,
    NULL
  },
  /* 2144 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubrp, 0x00,
    2, g_Operands + 698,
    NULL
  },
  /* 2145 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubrp, 0x00,
    2, g_Operands + 700,
    NULL
  },
  /* 2146 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubrp, 0x00,
    2, g_Operands + 702,
    NULL
  },
  /* 2147 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubp, 0x00,
    2, g_Operands + 626,
    NULL
  },
  /* 2148 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubp, 0x00,
    2, g_Operands + 690,
    NULL
  },
  /* 2149 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubp, 0x00,
    2, g_Operands + 692,
    NULL
  },
  /* 2150 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubp, 0x00,
    2, g_Operands + 694,
    NULL
  },
  /* 2151 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubp, 0x00,
    2, g_Operands + 696,
    NULL
  },
  /* 2152 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubp, 0x00,
    2, g_Operands + 698,
    NULL
  },
  /* 2153 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubp, 0x00,
    2, g_Operands + 700,
    NULL
  },
  /* 2154 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubp, 0x00,
    2, g_Operands + 702,
    NULL
  },
  /* 2155 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivrp, 0x00,
    2, g_Operands + 626,
    NULL
  },
  /* 2156 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivrp, 0x00,
    2, g_Operands + 690,
    NULL
  },
  /* 2157 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivrp, 0x00,
    2, g_Operands + 692,
    NULL
  },
  /* 2158 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivrp, 0x00,
    2, g_Operands + 694,
    NULL
  },
  /* 2159 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivrp, 0x00,
    2, g_Operands + 696,
    NULL
  },
  /* 2160 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivrp, 0x00,
    2, g_Operands + 698,
    NULL
  },
  /* 2161 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivrp, 0x00,
    2, g_Operands + 700,
    NULL
  },
  /* 2162 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivrp, 0x00,
    2, g_Operands + 702,
    NULL
  },
  /* 2163 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivp, 0x00,
    2, g_Operands + 626,
    NULL
  },
  /* 2164 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivp, 0x00,
    2, g_Operands + 690,
    NULL
  },
  /* 2165 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivp, 0x00,
    2, g_Operands + 692,
    NULL
  },
  /* 2166 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivp, 0x00,
    2, g_Operands + 694,
    NULL
  },
  /* 2167 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivp, 0x00,
    2, g_Operands + 696,
    NULL
  },
  /* 2168 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivp, 0x00,
    2, g_Operands + 698,
    NULL
  },
  /* 2169 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivp, 0x00,
    2, g_Operands + 700,
    NULL
  },
  /* 2170 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivp, 0x00,
    2, g_Operands + 702,
    NULL
  },
  /* 2171 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2172 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2173 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2174 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2175 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2176 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2177 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2178 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2179 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2180 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2181 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2182 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2183 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2184 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2185 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2186 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2187 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2188 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2189 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2190 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2191 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2192 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2193 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2194 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2195 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2196 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2197 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2198 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2199 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2200 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2201 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2202 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2203 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFnstsw, 0x00,
    1, g_Operands + 726,
    NULL
  },
  /* 2204 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2205 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2206 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2207 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2208 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2209 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2210 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2211 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomip, 0x00,
    2, g_Operands + 642,
    NULL
  },
  /* 2212 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomip, 0x00,
    2, g_Operands + 644,
    NULL
  },
  /* 2213 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomip, 0x00,
    2, g_Operands + 646,
    NULL
  },
  /* 2214 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomip, 0x00,
    2, g_Operands + 648,
    NULL
  },
  /* 2215 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomip, 0x00,
    2, g_Operands + 650,
    NULL
  },
  /* 2216 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomip, 0x00,
    2, g_Operands + 652,
    NULL
  },
  /* 2217 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomip, 0x00,
    2, g_Operands + 654,
    NULL
  },
  /* 2218 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomip, 0x00,
    2, g_Operands + 656,
    NULL
  },
  /* 2219 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomip, 0x00,
    2, g_Operands + 642,
    NULL
  },
  /* 2220 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomip, 0x00,
    2, g_Operands + 644,
    NULL
  },
  /* 2221 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomip, 0x00,
    2, g_Operands + 646,
    NULL
  },
  /* 2222 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomip, 0x00,
    2, g_Operands + 648,
    NULL
  },
  /* 2223 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomip, 0x00,
    2, g_Operands + 650,
    NULL
  },
  /* 2224 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomip, 0x00,
    2, g_Operands + 652,
    NULL
  },
  /* 2225 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomip, 0x00,
    2, g_Operands + 654,
    NULL
  },
  /* 2226 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomip, 0x00,
    2, g_Operands + 656,
    NULL
  },
  /* 2227 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2228 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2229 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2230 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2231 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2232 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2233 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2234 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2235 */
  { NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstUd2, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2236 */
  { NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstNop, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2237 */
  { NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstNop, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2238 */
  { NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstNop, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2239 */
  { NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstNop, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2240 */
  { NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstNop, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2241 */
  { NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstNop, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2242 */
  { NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstNop, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2243 */
  { NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstNop, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2244 */
  { NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstNop, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2245 */
  { NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstNop, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2246 */
  { NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstNop, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2247 */
  { NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstNop, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2248 */
  { NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstNop, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2249 */
  { NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstNop, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2250 */
  { NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstNop, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2251 */
  { NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstNop, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 2252 */
  { NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstPause, 0x00,
    0, g_Operands + 0,
    NULL
  },
};

static const NaClInst g_Undefined_Opcode = 
  /* 0 */
  { NACLi_INVALID,
    NACL_EMPTY_IFLAGS,
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  };
static const NaClInst* g_OpcodeTable[NaClInstPrefixEnumSize][NCDTABLESIZE] = {
/* NoPrefix */
{
  /* 00 */ g_Opcodes + 0  ,
  /* 01 */ g_Opcodes + 1  ,
  /* 02 */ g_Opcodes + 2  ,
  /* 03 */ g_Opcodes + 3  ,
  /* 04 */ g_Opcodes + 4  ,
  /* 05 */ g_Opcodes + 5  ,
  /* 06 */ g_Opcodes + 6  ,
  /* 07 */ g_Opcodes + 7  ,
  /* 08 */ g_Opcodes + 8  ,
  /* 09 */ g_Opcodes + 9  ,
  /* 0a */ g_Opcodes + 10  ,
  /* 0b */ g_Opcodes + 11  ,
  /* 0c */ g_Opcodes + 12  ,
  /* 0d */ g_Opcodes + 13  ,
  /* 0e */ g_Opcodes + 14  ,
  /* 0f */ g_Opcodes + 15  ,
  /* 10 */ g_Opcodes + 16  ,
  /* 11 */ g_Opcodes + 17  ,
  /* 12 */ g_Opcodes + 18  ,
  /* 13 */ g_Opcodes + 19  ,
  /* 14 */ g_Opcodes + 20  ,
  /* 15 */ g_Opcodes + 21  ,
  /* 16 */ g_Opcodes + 22  ,
  /* 17 */ g_Opcodes + 23  ,
  /* 18 */ g_Opcodes + 24  ,
  /* 19 */ g_Opcodes + 25  ,
  /* 1a */ g_Opcodes + 26  ,
  /* 1b */ g_Opcodes + 27  ,
  /* 1c */ g_Opcodes + 28  ,
  /* 1d */ g_Opcodes + 29  ,
  /* 1e */ g_Opcodes + 30  ,
  /* 1f */ g_Opcodes + 31  ,
  /* 20 */ g_Opcodes + 32  ,
  /* 21 */ g_Opcodes + 33  ,
  /* 22 */ g_Opcodes + 34  ,
  /* 23 */ g_Opcodes + 35  ,
  /* 24 */ g_Opcodes + 36  ,
  /* 25 */ g_Opcodes + 37  ,
  /* 26 */ g_Opcodes + 38  ,
  /* 27 */ g_Opcodes + 39  ,
  /* 28 */ g_Opcodes + 40  ,
  /* 29 */ g_Opcodes + 41  ,
  /* 2a */ g_Opcodes + 42  ,
  /* 2b */ g_Opcodes + 43  ,
  /* 2c */ g_Opcodes + 44  ,
  /* 2d */ g_Opcodes + 45  ,
  /* 2e */ g_Opcodes + 46  ,
  /* 2f */ g_Opcodes + 47  ,
  /* 30 */ g_Opcodes + 48  ,
  /* 31 */ g_Opcodes + 49  ,
  /* 32 */ g_Opcodes + 50  ,
  /* 33 */ g_Opcodes + 51  ,
  /* 34 */ g_Opcodes + 52  ,
  /* 35 */ g_Opcodes + 53  ,
  /* 36 */ g_Opcodes + 54  ,
  /* 37 */ g_Opcodes + 55  ,
  /* 38 */ g_Opcodes + 56  ,
  /* 39 */ g_Opcodes + 57  ,
  /* 3a */ g_Opcodes + 58  ,
  /* 3b */ g_Opcodes + 59  ,
  /* 3c */ g_Opcodes + 60  ,
  /* 3d */ g_Opcodes + 61  ,
  /* 3e */ g_Opcodes + 62  ,
  /* 3f */ g_Opcodes + 63  ,
  /* 40 */ NULL  ,
  /* 41 */ NULL  ,
  /* 42 */ NULL  ,
  /* 43 */ NULL  ,
  /* 44 */ NULL  ,
  /* 45 */ NULL  ,
  /* 46 */ NULL  ,
  /* 47 */ NULL  ,
  /* 48 */ NULL  ,
  /* 49 */ NULL  ,
  /* 4a */ NULL  ,
  /* 4b */ NULL  ,
  /* 4c */ NULL  ,
  /* 4d */ NULL  ,
  /* 4e */ NULL  ,
  /* 4f */ NULL  ,
  /* 50 */ g_Opcodes + 64  ,
  /* 51 */ g_Opcodes + 65  ,
  /* 52 */ g_Opcodes + 66  ,
  /* 53 */ g_Opcodes + 67  ,
  /* 54 */ g_Opcodes + 68  ,
  /* 55 */ g_Opcodes + 69  ,
  /* 56 */ g_Opcodes + 70  ,
  /* 57 */ g_Opcodes + 71  ,
  /* 58 */ g_Opcodes + 72  ,
  /* 59 */ g_Opcodes + 73  ,
  /* 5a */ g_Opcodes + 74  ,
  /* 5b */ g_Opcodes + 75  ,
  /* 5c */ g_Opcodes + 76  ,
  /* 5d */ g_Opcodes + 77  ,
  /* 5e */ g_Opcodes + 78  ,
  /* 5f */ g_Opcodes + 79  ,
  /* 60 */ g_Opcodes + 80  ,
  /* 61 */ g_Opcodes + 81  ,
  /* 62 */ g_Opcodes + 82  ,
  /* 63 */ g_Opcodes + 83  ,
  /* 64 */ g_Opcodes + 84  ,
  /* 65 */ g_Opcodes + 85  ,
  /* 66 */ g_Opcodes + 86  ,
  /* 67 */ g_Opcodes + 87  ,
  /* 68 */ g_Opcodes + 88  ,
  /* 69 */ g_Opcodes + 89  ,
  /* 6a */ g_Opcodes + 90  ,
  /* 6b */ g_Opcodes + 91  ,
  /* 6c */ g_Opcodes + 92  ,
  /* 6d */ g_Opcodes + 93  ,
  /* 6e */ g_Opcodes + 95  ,
  /* 6f */ g_Opcodes + 96  ,
  /* 70 */ g_Opcodes + 98  ,
  /* 71 */ g_Opcodes + 99  ,
  /* 72 */ g_Opcodes + 100  ,
  /* 73 */ g_Opcodes + 101  ,
  /* 74 */ g_Opcodes + 102  ,
  /* 75 */ g_Opcodes + 103  ,
  /* 76 */ g_Opcodes + 104  ,
  /* 77 */ g_Opcodes + 105  ,
  /* 78 */ g_Opcodes + 106  ,
  /* 79 */ g_Opcodes + 107  ,
  /* 7a */ g_Opcodes + 108  ,
  /* 7b */ g_Opcodes + 109  ,
  /* 7c */ g_Opcodes + 110  ,
  /* 7d */ g_Opcodes + 111  ,
  /* 7e */ g_Opcodes + 112  ,
  /* 7f */ g_Opcodes + 113  ,
  /* 80 */ g_Opcodes + 114  ,
  /* 81 */ g_Opcodes + 122  ,
  /* 82 */ g_Opcodes + 130  ,
  /* 83 */ g_Opcodes + 138  ,
  /* 84 */ g_Opcodes + 146  ,
  /* 85 */ g_Opcodes + 147  ,
  /* 86 */ g_Opcodes + 148  ,
  /* 87 */ g_Opcodes + 149  ,
  /* 88 */ g_Opcodes + 150  ,
  /* 89 */ g_Opcodes + 151  ,
  /* 8a */ g_Opcodes + 152  ,
  /* 8b */ g_Opcodes + 153  ,
  /* 8c */ g_Opcodes + 154  ,
  /* 8d */ g_Opcodes + 155  ,
  /* 8e */ g_Opcodes + 156  ,
  /* 8f */ g_Opcodes + 157  ,
  /* 90 */ g_Opcodes + 159  ,
  /* 91 */ g_Opcodes + 160  ,
  /* 92 */ g_Opcodes + 161  ,
  /* 93 */ g_Opcodes + 162  ,
  /* 94 */ g_Opcodes + 163  ,
  /* 95 */ g_Opcodes + 164  ,
  /* 96 */ g_Opcodes + 165  ,
  /* 97 */ g_Opcodes + 166  ,
  /* 98 */ g_Opcodes + 167  ,
  /* 99 */ g_Opcodes + 170  ,
  /* 9a */ g_Opcodes + 173  ,
  /* 9b */ g_Opcodes + 174  ,
  /* 9c */ g_Opcodes + 175  ,
  /* 9d */ g_Opcodes + 177  ,
  /* 9e */ g_Opcodes + 179  ,
  /* 9f */ g_Opcodes + 180  ,
  /* a0 */ g_Opcodes + 181  ,
  /* a1 */ g_Opcodes + 182  ,
  /* a2 */ g_Opcodes + 183  ,
  /* a3 */ g_Opcodes + 184  ,
  /* a4 */ g_Opcodes + 185  ,
  /* a5 */ g_Opcodes + 186  ,
  /* a6 */ g_Opcodes + 189  ,
  /* a7 */ g_Opcodes + 190  ,
  /* a8 */ g_Opcodes + 193  ,
  /* a9 */ g_Opcodes + 194  ,
  /* aa */ g_Opcodes + 195  ,
  /* ab */ g_Opcodes + 196  ,
  /* ac */ g_Opcodes + 199  ,
  /* ad */ g_Opcodes + 200  ,
  /* ae */ g_Opcodes + 203  ,
  /* af */ g_Opcodes + 204  ,
  /* b0 */ g_Opcodes + 207  ,
  /* b1 */ g_Opcodes + 208  ,
  /* b2 */ g_Opcodes + 209  ,
  /* b3 */ g_Opcodes + 210  ,
  /* b4 */ g_Opcodes + 211  ,
  /* b5 */ g_Opcodes + 212  ,
  /* b6 */ g_Opcodes + 213  ,
  /* b7 */ g_Opcodes + 214  ,
  /* b8 */ g_Opcodes + 215  ,
  /* b9 */ g_Opcodes + 216  ,
  /* ba */ g_Opcodes + 217  ,
  /* bb */ g_Opcodes + 218  ,
  /* bc */ g_Opcodes + 219  ,
  /* bd */ g_Opcodes + 220  ,
  /* be */ g_Opcodes + 221  ,
  /* bf */ g_Opcodes + 222  ,
  /* c0 */ g_Opcodes + 223  ,
  /* c1 */ g_Opcodes + 231  ,
  /* c2 */ g_Opcodes + 239  ,
  /* c3 */ g_Opcodes + 240  ,
  /* c4 */ g_Opcodes + 241  ,
  /* c5 */ g_Opcodes + 242  ,
  /* c6 */ g_Opcodes + 243  ,
  /* c7 */ g_Opcodes + 245  ,
  /* c8 */ g_Opcodes + 247  ,
  /* c9 */ g_Opcodes + 248  ,
  /* ca */ g_Opcodes + 249  ,
  /* cb */ g_Opcodes + 250  ,
  /* cc */ g_Opcodes + 251  ,
  /* cd */ g_Opcodes + 252  ,
  /* ce */ g_Opcodes + 253  ,
  /* cf */ g_Opcodes + 254  ,
  /* d0 */ g_Opcodes + 257  ,
  /* d1 */ g_Opcodes + 265  ,
  /* d2 */ g_Opcodes + 273  ,
  /* d3 */ g_Opcodes + 281  ,
  /* d4 */ g_Opcodes + 289  ,
  /* d5 */ g_Opcodes + 290  ,
  /* d6 */ g_Opcodes + 291  ,
  /* d7 */ g_Opcodes + 292  ,
  /* d8 */ g_Opcodes + 293  ,
  /* d9 */ g_Opcodes + 301  ,
  /* da */ g_Opcodes + 309  ,
  /* db */ g_Opcodes + 317  ,
  /* dc */ g_Opcodes + 325  ,
  /* dd */ g_Opcodes + 333  ,
  /* de */ g_Opcodes + 341  ,
  /* df */ g_Opcodes + 349  ,
  /* e0 */ g_Opcodes + 357  ,
  /* e1 */ g_Opcodes + 358  ,
  /* e2 */ g_Opcodes + 359  ,
  /* e3 */ g_Opcodes + 360  ,
  /* e4 */ g_Opcodes + 362  ,
  /* e5 */ g_Opcodes + 363  ,
  /* e6 */ g_Opcodes + 364  ,
  /* e7 */ g_Opcodes + 365  ,
  /* e8 */ g_Opcodes + 366  ,
  /* e9 */ g_Opcodes + 368  ,
  /* ea */ NULL  ,
  /* eb */ g_Opcodes + 369  ,
  /* ec */ g_Opcodes + 370  ,
  /* ed */ g_Opcodes + 371  ,
  /* ee */ g_Opcodes + 372  ,
  /* ef */ g_Opcodes + 373  ,
  /* f0 */ g_Opcodes + 374  ,
  /* f1 */ g_Opcodes + 375  ,
  /* f2 */ g_Opcodes + 376  ,
  /* f3 */ g_Opcodes + 377  ,
  /* f4 */ g_Opcodes + 378  ,
  /* f5 */ g_Opcodes + 379  ,
  /* f6 */ g_Opcodes + 380  ,
  /* f7 */ g_Opcodes + 388  ,
  /* f8 */ g_Opcodes + 396  ,
  /* f9 */ g_Opcodes + 397  ,
  /* fa */ g_Opcodes + 398  ,
  /* fb */ g_Opcodes + 399  ,
  /* fc */ g_Opcodes + 400  ,
  /* fd */ g_Opcodes + 401  ,
  /* fe */ g_Opcodes + 402  ,
  /* ff */ g_Opcodes + 410  ,
},
/* Prefix0F */
{
  /* 00 */ g_Opcodes + 418  ,
  /* 01 */ g_Opcodes + 426  ,
  /* 02 */ g_Opcodes + 448  ,
  /* 03 */ g_Opcodes + 449  ,
  /* 04 */ g_Opcodes + 450  ,
  /* 05 */ g_Opcodes + 451  ,
  /* 06 */ g_Opcodes + 452  ,
  /* 07 */ g_Opcodes + 453  ,
  /* 08 */ g_Opcodes + 454  ,
  /* 09 */ g_Opcodes + 455  ,
  /* 0a */ g_Opcodes + 456  ,
  /* 0b */ g_Opcodes + 457  ,
  /* 0c */ g_Opcodes + 458  ,
  /* 0d */ g_Opcodes + 459  ,
  /* 0e */ g_Opcodes + 467  ,
  /* 0f */ g_Opcodes + 468  ,
  /* 10 */ g_Opcodes + 469  ,
  /* 11 */ g_Opcodes + 470  ,
  /* 12 */ g_Opcodes + 471  ,
  /* 13 */ g_Opcodes + 473  ,
  /* 14 */ g_Opcodes + 474  ,
  /* 15 */ g_Opcodes + 475  ,
  /* 16 */ g_Opcodes + 476  ,
  /* 17 */ g_Opcodes + 478  ,
  /* 18 */ g_Opcodes + 479  ,
  /* 19 */ g_Opcodes + 487  ,
  /* 1a */ g_Opcodes + 488  ,
  /* 1b */ g_Opcodes + 489  ,
  /* 1c */ g_Opcodes + 490  ,
  /* 1d */ g_Opcodes + 491  ,
  /* 1e */ g_Opcodes + 492  ,
  /* 1f */ g_Opcodes + 493  ,
  /* 20 */ g_Opcodes + 495  ,
  /* 21 */ g_Opcodes + 496  ,
  /* 22 */ g_Opcodes + 497  ,
  /* 23 */ g_Opcodes + 498  ,
  /* 24 */ g_Opcodes + 499  ,
  /* 25 */ g_Opcodes + 500  ,
  /* 26 */ g_Opcodes + 501  ,
  /* 27 */ g_Opcodes + 502  ,
  /* 28 */ g_Opcodes + 503  ,
  /* 29 */ g_Opcodes + 504  ,
  /* 2a */ g_Opcodes + 505  ,
  /* 2b */ g_Opcodes + 506  ,
  /* 2c */ g_Opcodes + 507  ,
  /* 2d */ g_Opcodes + 508  ,
  /* 2e */ g_Opcodes + 509  ,
  /* 2f */ g_Opcodes + 510  ,
  /* 30 */ g_Opcodes + 511  ,
  /* 31 */ g_Opcodes + 512  ,
  /* 32 */ g_Opcodes + 513  ,
  /* 33 */ g_Opcodes + 514  ,
  /* 34 */ g_Opcodes + 515  ,
  /* 35 */ g_Opcodes + 516  ,
  /* 36 */ g_Opcodes + 517  ,
  /* 37 */ g_Opcodes + 518  ,
  /* 38 */ g_Opcodes + 519  ,
  /* 39 */ g_Opcodes + 520  ,
  /* 3a */ g_Opcodes + 521  ,
  /* 3b */ g_Opcodes + 522  ,
  /* 3c */ g_Opcodes + 523  ,
  /* 3d */ g_Opcodes + 524  ,
  /* 3e */ g_Opcodes + 525  ,
  /* 3f */ g_Opcodes + 526  ,
  /* 40 */ g_Opcodes + 527  ,
  /* 41 */ g_Opcodes + 528  ,
  /* 42 */ g_Opcodes + 529  ,
  /* 43 */ g_Opcodes + 530  ,
  /* 44 */ g_Opcodes + 531  ,
  /* 45 */ g_Opcodes + 532  ,
  /* 46 */ g_Opcodes + 533  ,
  /* 47 */ g_Opcodes + 534  ,
  /* 48 */ g_Opcodes + 535  ,
  /* 49 */ g_Opcodes + 536  ,
  /* 4a */ g_Opcodes + 537  ,
  /* 4b */ g_Opcodes + 538  ,
  /* 4c */ g_Opcodes + 539  ,
  /* 4d */ g_Opcodes + 540  ,
  /* 4e */ g_Opcodes + 541  ,
  /* 4f */ g_Opcodes + 542  ,
  /* 50 */ g_Opcodes + 543  ,
  /* 51 */ g_Opcodes + 544  ,
  /* 52 */ g_Opcodes + 545  ,
  /* 53 */ g_Opcodes + 546  ,
  /* 54 */ g_Opcodes + 547  ,
  /* 55 */ g_Opcodes + 548  ,
  /* 56 */ g_Opcodes + 549  ,
  /* 57 */ g_Opcodes + 550  ,
  /* 58 */ g_Opcodes + 551  ,
  /* 59 */ g_Opcodes + 552  ,
  /* 5a */ g_Opcodes + 553  ,
  /* 5b */ g_Opcodes + 554  ,
  /* 5c */ g_Opcodes + 555  ,
  /* 5d */ g_Opcodes + 556  ,
  /* 5e */ g_Opcodes + 557  ,
  /* 5f */ g_Opcodes + 558  ,
  /* 60 */ g_Opcodes + 559  ,
  /* 61 */ g_Opcodes + 560  ,
  /* 62 */ g_Opcodes + 561  ,
  /* 63 */ g_Opcodes + 562  ,
  /* 64 */ g_Opcodes + 563  ,
  /* 65 */ g_Opcodes + 564  ,
  /* 66 */ g_Opcodes + 565  ,
  /* 67 */ g_Opcodes + 566  ,
  /* 68 */ g_Opcodes + 567  ,
  /* 69 */ g_Opcodes + 568  ,
  /* 6a */ g_Opcodes + 569  ,
  /* 6b */ g_Opcodes + 570  ,
  /* 6c */ g_Opcodes + 571  ,
  /* 6d */ g_Opcodes + 572  ,
  /* 6e */ g_Opcodes + 573  ,
  /* 6f */ g_Opcodes + 575  ,
  /* 70 */ g_Opcodes + 576  ,
  /* 71 */ g_Opcodes + 577  ,
  /* 72 */ g_Opcodes + 585  ,
  /* 73 */ g_Opcodes + 593  ,
  /* 74 */ g_Opcodes + 601  ,
  /* 75 */ g_Opcodes + 602  ,
  /* 76 */ g_Opcodes + 603  ,
  /* 77 */ g_Opcodes + 604  ,
  /* 78 */ g_Opcodes + 605  ,
  /* 79 */ g_Opcodes + 606  ,
  /* 7a */ g_Opcodes + 607  ,
  /* 7b */ g_Opcodes + 608  ,
  /* 7c */ g_Opcodes + 609  ,
  /* 7d */ g_Opcodes + 610  ,
  /* 7e */ g_Opcodes + 611  ,
  /* 7f */ g_Opcodes + 613  ,
  /* 80 */ g_Opcodes + 614  ,
  /* 81 */ g_Opcodes + 615  ,
  /* 82 */ g_Opcodes + 616  ,
  /* 83 */ g_Opcodes + 617  ,
  /* 84 */ g_Opcodes + 618  ,
  /* 85 */ g_Opcodes + 619  ,
  /* 86 */ g_Opcodes + 620  ,
  /* 87 */ g_Opcodes + 621  ,
  /* 88 */ g_Opcodes + 622  ,
  /* 89 */ g_Opcodes + 623  ,
  /* 8a */ g_Opcodes + 624  ,
  /* 8b */ g_Opcodes + 625  ,
  /* 8c */ g_Opcodes + 626  ,
  /* 8d */ g_Opcodes + 627  ,
  /* 8e */ g_Opcodes + 628  ,
  /* 8f */ g_Opcodes + 629  ,
  /* 90 */ g_Opcodes + 630  ,
  /* 91 */ g_Opcodes + 631  ,
  /* 92 */ g_Opcodes + 632  ,
  /* 93 */ g_Opcodes + 633  ,
  /* 94 */ g_Opcodes + 634  ,
  /* 95 */ g_Opcodes + 635  ,
  /* 96 */ g_Opcodes + 636  ,
  /* 97 */ g_Opcodes + 637  ,
  /* 98 */ g_Opcodes + 638  ,
  /* 99 */ g_Opcodes + 639  ,
  /* 9a */ g_Opcodes + 640  ,
  /* 9b */ g_Opcodes + 641  ,
  /* 9c */ g_Opcodes + 642  ,
  /* 9d */ g_Opcodes + 643  ,
  /* 9e */ g_Opcodes + 644  ,
  /* 9f */ g_Opcodes + 645  ,
  /* a0 */ g_Opcodes + 646  ,
  /* a1 */ g_Opcodes + 647  ,
  /* a2 */ g_Opcodes + 648  ,
  /* a3 */ g_Opcodes + 649  ,
  /* a4 */ g_Opcodes + 650  ,
  /* a5 */ g_Opcodes + 651  ,
  /* a6 */ g_Opcodes + 652  ,
  /* a7 */ g_Opcodes + 653  ,
  /* a8 */ g_Opcodes + 654  ,
  /* a9 */ g_Opcodes + 655  ,
  /* aa */ g_Opcodes + 656  ,
  /* ab */ g_Opcodes + 657  ,
  /* ac */ g_Opcodes + 658  ,
  /* ad */ g_Opcodes + 659  ,
  /* ae */ g_Opcodes + 660  ,
  /* af */ g_Opcodes + 690  ,
  /* b0 */ g_Opcodes + 691  ,
  /* b1 */ g_Opcodes + 692  ,
  /* b2 */ g_Opcodes + 693  ,
  /* b3 */ g_Opcodes + 694  ,
  /* b4 */ g_Opcodes + 695  ,
  /* b5 */ g_Opcodes + 696  ,
  /* b6 */ g_Opcodes + 697  ,
  /* b7 */ g_Opcodes + 698  ,
  /* b8 */ g_Opcodes + 699  ,
  /* b9 */ g_Opcodes + 700  ,
  /* ba */ g_Opcodes + 701  ,
  /* bb */ g_Opcodes + 706  ,
  /* bc */ g_Opcodes + 707  ,
  /* bd */ g_Opcodes + 708  ,
  /* be */ g_Opcodes + 709  ,
  /* bf */ g_Opcodes + 710  ,
  /* c0 */ g_Opcodes + 711  ,
  /* c1 */ g_Opcodes + 712  ,
  /* c2 */ g_Opcodes + 713  ,
  /* c3 */ g_Opcodes + 714  ,
  /* c4 */ g_Opcodes + 715  ,
  /* c5 */ g_Opcodes + 716  ,
  /* c6 */ g_Opcodes + 717  ,
  /* c7 */ g_Opcodes + 718  ,
  /* c8 */ g_Opcodes + 721  ,
  /* c9 */ g_Opcodes + 722  ,
  /* ca */ g_Opcodes + 723  ,
  /* cb */ g_Opcodes + 724  ,
  /* cc */ g_Opcodes + 725  ,
  /* cd */ g_Opcodes + 726  ,
  /* ce */ g_Opcodes + 727  ,
  /* cf */ g_Opcodes + 728  ,
  /* d0 */ g_Opcodes + 729  ,
  /* d1 */ g_Opcodes + 730  ,
  /* d2 */ g_Opcodes + 731  ,
  /* d3 */ g_Opcodes + 732  ,
  /* d4 */ g_Opcodes + 733  ,
  /* d5 */ g_Opcodes + 734  ,
  /* d6 */ g_Opcodes + 735  ,
  /* d7 */ g_Opcodes + 736  ,
  /* d8 */ g_Opcodes + 737  ,
  /* d9 */ g_Opcodes + 738  ,
  /* da */ g_Opcodes + 739  ,
  /* db */ g_Opcodes + 740  ,
  /* dc */ g_Opcodes + 741  ,
  /* dd */ g_Opcodes + 742  ,
  /* de */ g_Opcodes + 743  ,
  /* df */ g_Opcodes + 744  ,
  /* e0 */ g_Opcodes + 745  ,
  /* e1 */ g_Opcodes + 746  ,
  /* e2 */ g_Opcodes + 747  ,
  /* e3 */ g_Opcodes + 748  ,
  /* e4 */ g_Opcodes + 749  ,
  /* e5 */ g_Opcodes + 750  ,
  /* e6 */ g_Opcodes + 751  ,
  /* e7 */ g_Opcodes + 752  ,
  /* e8 */ g_Opcodes + 753  ,
  /* e9 */ g_Opcodes + 754  ,
  /* ea */ g_Opcodes + 755  ,
  /* eb */ g_Opcodes + 756  ,
  /* ec */ g_Opcodes + 757  ,
  /* ed */ g_Opcodes + 758  ,
  /* ee */ g_Opcodes + 759  ,
  /* ef */ g_Opcodes + 760  ,
  /* f0 */ g_Opcodes + 761  ,
  /* f1 */ g_Opcodes + 762  ,
  /* f2 */ g_Opcodes + 763  ,
  /* f3 */ g_Opcodes + 764  ,
  /* f4 */ g_Opcodes + 765  ,
  /* f5 */ g_Opcodes + 766  ,
  /* f6 */ g_Opcodes + 767  ,
  /* f7 */ g_Opcodes + 768  ,
  /* f8 */ g_Opcodes + 769  ,
  /* f9 */ g_Opcodes + 770  ,
  /* fa */ g_Opcodes + 771  ,
  /* fb */ g_Opcodes + 772  ,
  /* fc */ g_Opcodes + 773  ,
  /* fd */ g_Opcodes + 774  ,
  /* fe */ g_Opcodes + 775  ,
  /* ff */ g_Opcodes + 776  ,
},
/* PrefixF20F */
{
  /* 00 */ NULL  ,
  /* 01 */ NULL  ,
  /* 02 */ NULL  ,
  /* 03 */ NULL  ,
  /* 04 */ NULL  ,
  /* 05 */ NULL  ,
  /* 06 */ NULL  ,
  /* 07 */ NULL  ,
  /* 08 */ NULL  ,
  /* 09 */ NULL  ,
  /* 0a */ NULL  ,
  /* 0b */ NULL  ,
  /* 0c */ NULL  ,
  /* 0d */ NULL  ,
  /* 0e */ NULL  ,
  /* 0f */ NULL  ,
  /* 10 */ g_Opcodes + 777  ,
  /* 11 */ g_Opcodes + 778  ,
  /* 12 */ g_Opcodes + 779  ,
  /* 13 */ g_Opcodes + 780  ,
  /* 14 */ g_Opcodes + 781  ,
  /* 15 */ g_Opcodes + 782  ,
  /* 16 */ g_Opcodes + 783  ,
  /* 17 */ g_Opcodes + 784  ,
  /* 18 */ NULL  ,
  /* 19 */ NULL  ,
  /* 1a */ NULL  ,
  /* 1b */ NULL  ,
  /* 1c */ NULL  ,
  /* 1d */ NULL  ,
  /* 1e */ NULL  ,
  /* 1f */ NULL  ,
  /* 20 */ NULL  ,
  /* 21 */ NULL  ,
  /* 22 */ NULL  ,
  /* 23 */ NULL  ,
  /* 24 */ NULL  ,
  /* 25 */ NULL  ,
  /* 26 */ NULL  ,
  /* 27 */ NULL  ,
  /* 28 */ g_Opcodes + 785  ,
  /* 29 */ g_Opcodes + 786  ,
  /* 2a */ g_Opcodes + 787  ,
  /* 2b */ g_Opcodes + 788  ,
  /* 2c */ g_Opcodes + 789  ,
  /* 2d */ g_Opcodes + 790  ,
  /* 2e */ g_Opcodes + 791  ,
  /* 2f */ g_Opcodes + 792  ,
  /* 30 */ NULL  ,
  /* 31 */ NULL  ,
  /* 32 */ NULL  ,
  /* 33 */ NULL  ,
  /* 34 */ NULL  ,
  /* 35 */ NULL  ,
  /* 36 */ NULL  ,
  /* 37 */ NULL  ,
  /* 38 */ NULL  ,
  /* 39 */ NULL  ,
  /* 3a */ NULL  ,
  /* 3b */ NULL  ,
  /* 3c */ NULL  ,
  /* 3d */ NULL  ,
  /* 3e */ NULL  ,
  /* 3f */ NULL  ,
  /* 40 */ NULL  ,
  /* 41 */ NULL  ,
  /* 42 */ NULL  ,
  /* 43 */ NULL  ,
  /* 44 */ NULL  ,
  /* 45 */ NULL  ,
  /* 46 */ NULL  ,
  /* 47 */ NULL  ,
  /* 48 */ NULL  ,
  /* 49 */ NULL  ,
  /* 4a */ NULL  ,
  /* 4b */ NULL  ,
  /* 4c */ NULL  ,
  /* 4d */ NULL  ,
  /* 4e */ NULL  ,
  /* 4f */ NULL  ,
  /* 50 */ g_Opcodes + 793  ,
  /* 51 */ g_Opcodes + 794  ,
  /* 52 */ g_Opcodes + 795  ,
  /* 53 */ g_Opcodes + 796  ,
  /* 54 */ g_Opcodes + 797  ,
  /* 55 */ g_Opcodes + 798  ,
  /* 56 */ g_Opcodes + 799  ,
  /* 57 */ g_Opcodes + 800  ,
  /* 58 */ g_Opcodes + 801  ,
  /* 59 */ g_Opcodes + 802  ,
  /* 5a */ g_Opcodes + 803  ,
  /* 5b */ g_Opcodes + 804  ,
  /* 5c */ g_Opcodes + 805  ,
  /* 5d */ g_Opcodes + 806  ,
  /* 5e */ g_Opcodes + 807  ,
  /* 5f */ g_Opcodes + 808  ,
  /* 60 */ g_Opcodes + 809  ,
  /* 61 */ g_Opcodes + 810  ,
  /* 62 */ g_Opcodes + 811  ,
  /* 63 */ g_Opcodes + 812  ,
  /* 64 */ g_Opcodes + 813  ,
  /* 65 */ g_Opcodes + 814  ,
  /* 66 */ g_Opcodes + 815  ,
  /* 67 */ g_Opcodes + 816  ,
  /* 68 */ g_Opcodes + 817  ,
  /* 69 */ g_Opcodes + 818  ,
  /* 6a */ g_Opcodes + 819  ,
  /* 6b */ g_Opcodes + 820  ,
  /* 6c */ g_Opcodes + 821  ,
  /* 6d */ g_Opcodes + 822  ,
  /* 6e */ g_Opcodes + 823  ,
  /* 6f */ g_Opcodes + 824  ,
  /* 70 */ g_Opcodes + 825  ,
  /* 71 */ g_Opcodes + 826  ,
  /* 72 */ g_Opcodes + 827  ,
  /* 73 */ g_Opcodes + 828  ,
  /* 74 */ g_Opcodes + 829  ,
  /* 75 */ g_Opcodes + 830  ,
  /* 76 */ g_Opcodes + 831  ,
  /* 77 */ g_Opcodes + 832  ,
  /* 78 */ g_Opcodes + 833  ,
  /* 79 */ g_Opcodes + 834  ,
  /* 7a */ g_Opcodes + 835  ,
  /* 7b */ g_Opcodes + 836  ,
  /* 7c */ g_Opcodes + 837  ,
  /* 7d */ g_Opcodes + 838  ,
  /* 7e */ g_Opcodes + 839  ,
  /* 7f */ g_Opcodes + 840  ,
  /* 80 */ NULL  ,
  /* 81 */ NULL  ,
  /* 82 */ NULL  ,
  /* 83 */ NULL  ,
  /* 84 */ NULL  ,
  /* 85 */ NULL  ,
  /* 86 */ NULL  ,
  /* 87 */ NULL  ,
  /* 88 */ NULL  ,
  /* 89 */ NULL  ,
  /* 8a */ NULL  ,
  /* 8b */ NULL  ,
  /* 8c */ NULL  ,
  /* 8d */ NULL  ,
  /* 8e */ NULL  ,
  /* 8f */ NULL  ,
  /* 90 */ NULL  ,
  /* 91 */ NULL  ,
  /* 92 */ NULL  ,
  /* 93 */ NULL  ,
  /* 94 */ NULL  ,
  /* 95 */ NULL  ,
  /* 96 */ NULL  ,
  /* 97 */ NULL  ,
  /* 98 */ NULL  ,
  /* 99 */ NULL  ,
  /* 9a */ NULL  ,
  /* 9b */ NULL  ,
  /* 9c */ NULL  ,
  /* 9d */ NULL  ,
  /* 9e */ NULL  ,
  /* 9f */ NULL  ,
  /* a0 */ NULL  ,
  /* a1 */ NULL  ,
  /* a2 */ NULL  ,
  /* a3 */ NULL  ,
  /* a4 */ NULL  ,
  /* a5 */ NULL  ,
  /* a6 */ NULL  ,
  /* a7 */ NULL  ,
  /* a8 */ NULL  ,
  /* a9 */ NULL  ,
  /* aa */ NULL  ,
  /* ab */ NULL  ,
  /* ac */ NULL  ,
  /* ad */ NULL  ,
  /* ae */ g_Opcodes + 841  ,
  /* af */ NULL  ,
  /* b0 */ NULL  ,
  /* b1 */ NULL  ,
  /* b2 */ NULL  ,
  /* b3 */ NULL  ,
  /* b4 */ NULL  ,
  /* b5 */ NULL  ,
  /* b6 */ NULL  ,
  /* b7 */ NULL  ,
  /* b8 */ g_Opcodes + 842  ,
  /* b9 */ g_Opcodes + 843  ,
  /* ba */ g_Opcodes + 844  ,
  /* bb */ g_Opcodes + 845  ,
  /* bc */ g_Opcodes + 846  ,
  /* bd */ g_Opcodes + 847  ,
  /* be */ g_Opcodes + 848  ,
  /* bf */ g_Opcodes + 849  ,
  /* c0 */ NULL  ,
  /* c1 */ NULL  ,
  /* c2 */ g_Opcodes + 850  ,
  /* c3 */ g_Opcodes + 851  ,
  /* c4 */ g_Opcodes + 852  ,
  /* c5 */ g_Opcodes + 853  ,
  /* c6 */ g_Opcodes + 854  ,
  /* c7 */ NULL  ,
  /* c8 */ NULL  ,
  /* c9 */ NULL  ,
  /* ca */ NULL  ,
  /* cb */ NULL  ,
  /* cc */ NULL  ,
  /* cd */ NULL  ,
  /* ce */ NULL  ,
  /* cf */ NULL  ,
  /* d0 */ g_Opcodes + 855  ,
  /* d1 */ g_Opcodes + 856  ,
  /* d2 */ g_Opcodes + 857  ,
  /* d3 */ g_Opcodes + 858  ,
  /* d4 */ g_Opcodes + 859  ,
  /* d5 */ g_Opcodes + 860  ,
  /* d6 */ g_Opcodes + 861  ,
  /* d7 */ g_Opcodes + 862  ,
  /* d8 */ g_Opcodes + 863  ,
  /* d9 */ g_Opcodes + 864  ,
  /* da */ g_Opcodes + 865  ,
  /* db */ g_Opcodes + 866  ,
  /* dc */ g_Opcodes + 867  ,
  /* dd */ g_Opcodes + 868  ,
  /* de */ g_Opcodes + 869  ,
  /* df */ g_Opcodes + 870  ,
  /* e0 */ g_Opcodes + 871  ,
  /* e1 */ g_Opcodes + 872  ,
  /* e2 */ g_Opcodes + 873  ,
  /* e3 */ g_Opcodes + 874  ,
  /* e4 */ g_Opcodes + 875  ,
  /* e5 */ g_Opcodes + 876  ,
  /* e6 */ g_Opcodes + 877  ,
  /* e7 */ g_Opcodes + 878  ,
  /* e8 */ g_Opcodes + 879  ,
  /* e9 */ g_Opcodes + 880  ,
  /* ea */ g_Opcodes + 881  ,
  /* eb */ g_Opcodes + 882  ,
  /* ec */ g_Opcodes + 883  ,
  /* ed */ g_Opcodes + 884  ,
  /* ee */ g_Opcodes + 885  ,
  /* ef */ g_Opcodes + 886  ,
  /* f0 */ g_Opcodes + 887  ,
  /* f1 */ g_Opcodes + 888  ,
  /* f2 */ g_Opcodes + 889  ,
  /* f3 */ g_Opcodes + 890  ,
  /* f4 */ g_Opcodes + 891  ,
  /* f5 */ g_Opcodes + 892  ,
  /* f6 */ g_Opcodes + 893  ,
  /* f7 */ g_Opcodes + 894  ,
  /* f8 */ g_Opcodes + 895  ,
  /* f9 */ g_Opcodes + 896  ,
  /* fa */ g_Opcodes + 897  ,
  /* fb */ g_Opcodes + 898  ,
  /* fc */ g_Opcodes + 899  ,
  /* fd */ g_Opcodes + 900  ,
  /* fe */ g_Opcodes + 901  ,
  /* ff */ g_Opcodes + 902  ,
},
/* PrefixF30F */
{
  /* 00 */ NULL  ,
  /* 01 */ NULL  ,
  /* 02 */ NULL  ,
  /* 03 */ NULL  ,
  /* 04 */ NULL  ,
  /* 05 */ NULL  ,
  /* 06 */ NULL  ,
  /* 07 */ NULL  ,
  /* 08 */ NULL  ,
  /* 09 */ NULL  ,
  /* 0a */ NULL  ,
  /* 0b */ NULL  ,
  /* 0c */ NULL  ,
  /* 0d */ NULL  ,
  /* 0e */ NULL  ,
  /* 0f */ NULL  ,
  /* 10 */ g_Opcodes + 903  ,
  /* 11 */ g_Opcodes + 904  ,
  /* 12 */ g_Opcodes + 905  ,
  /* 13 */ g_Opcodes + 906  ,
  /* 14 */ g_Opcodes + 907  ,
  /* 15 */ g_Opcodes + 908  ,
  /* 16 */ g_Opcodes + 909  ,
  /* 17 */ g_Opcodes + 910  ,
  /* 18 */ NULL  ,
  /* 19 */ NULL  ,
  /* 1a */ NULL  ,
  /* 1b */ NULL  ,
  /* 1c */ NULL  ,
  /* 1d */ NULL  ,
  /* 1e */ NULL  ,
  /* 1f */ NULL  ,
  /* 20 */ NULL  ,
  /* 21 */ NULL  ,
  /* 22 */ NULL  ,
  /* 23 */ NULL  ,
  /* 24 */ NULL  ,
  /* 25 */ NULL  ,
  /* 26 */ NULL  ,
  /* 27 */ NULL  ,
  /* 28 */ g_Opcodes + 911  ,
  /* 29 */ g_Opcodes + 912  ,
  /* 2a */ g_Opcodes + 913  ,
  /* 2b */ g_Opcodes + 914  ,
  /* 2c */ g_Opcodes + 915  ,
  /* 2d */ g_Opcodes + 916  ,
  /* 2e */ g_Opcodes + 917  ,
  /* 2f */ g_Opcodes + 918  ,
  /* 30 */ NULL  ,
  /* 31 */ NULL  ,
  /* 32 */ NULL  ,
  /* 33 */ NULL  ,
  /* 34 */ NULL  ,
  /* 35 */ NULL  ,
  /* 36 */ NULL  ,
  /* 37 */ NULL  ,
  /* 38 */ NULL  ,
  /* 39 */ NULL  ,
  /* 3a */ NULL  ,
  /* 3b */ NULL  ,
  /* 3c */ NULL  ,
  /* 3d */ NULL  ,
  /* 3e */ NULL  ,
  /* 3f */ NULL  ,
  /* 40 */ NULL  ,
  /* 41 */ NULL  ,
  /* 42 */ NULL  ,
  /* 43 */ NULL  ,
  /* 44 */ NULL  ,
  /* 45 */ NULL  ,
  /* 46 */ NULL  ,
  /* 47 */ NULL  ,
  /* 48 */ NULL  ,
  /* 49 */ NULL  ,
  /* 4a */ NULL  ,
  /* 4b */ NULL  ,
  /* 4c */ NULL  ,
  /* 4d */ NULL  ,
  /* 4e */ NULL  ,
  /* 4f */ NULL  ,
  /* 50 */ g_Opcodes + 919  ,
  /* 51 */ g_Opcodes + 920  ,
  /* 52 */ g_Opcodes + 921  ,
  /* 53 */ g_Opcodes + 922  ,
  /* 54 */ g_Opcodes + 923  ,
  /* 55 */ g_Opcodes + 924  ,
  /* 56 */ g_Opcodes + 925  ,
  /* 57 */ g_Opcodes + 926  ,
  /* 58 */ g_Opcodes + 927  ,
  /* 59 */ g_Opcodes + 928  ,
  /* 5a */ g_Opcodes + 929  ,
  /* 5b */ g_Opcodes + 930  ,
  /* 5c */ g_Opcodes + 931  ,
  /* 5d */ g_Opcodes + 932  ,
  /* 5e */ g_Opcodes + 933  ,
  /* 5f */ g_Opcodes + 934  ,
  /* 60 */ g_Opcodes + 935  ,
  /* 61 */ g_Opcodes + 936  ,
  /* 62 */ g_Opcodes + 937  ,
  /* 63 */ g_Opcodes + 938  ,
  /* 64 */ g_Opcodes + 939  ,
  /* 65 */ g_Opcodes + 940  ,
  /* 66 */ g_Opcodes + 941  ,
  /* 67 */ g_Opcodes + 942  ,
  /* 68 */ g_Opcodes + 943  ,
  /* 69 */ g_Opcodes + 944  ,
  /* 6a */ g_Opcodes + 945  ,
  /* 6b */ g_Opcodes + 946  ,
  /* 6c */ g_Opcodes + 947  ,
  /* 6d */ g_Opcodes + 948  ,
  /* 6e */ g_Opcodes + 949  ,
  /* 6f */ g_Opcodes + 950  ,
  /* 70 */ g_Opcodes + 951  ,
  /* 71 */ g_Opcodes + 952  ,
  /* 72 */ g_Opcodes + 953  ,
  /* 73 */ g_Opcodes + 954  ,
  /* 74 */ g_Opcodes + 955  ,
  /* 75 */ g_Opcodes + 956  ,
  /* 76 */ g_Opcodes + 957  ,
  /* 77 */ g_Opcodes + 958  ,
  /* 78 */ g_Opcodes + 959  ,
  /* 79 */ g_Opcodes + 960  ,
  /* 7a */ g_Opcodes + 961  ,
  /* 7b */ g_Opcodes + 962  ,
  /* 7c */ g_Opcodes + 963  ,
  /* 7d */ g_Opcodes + 964  ,
  /* 7e */ g_Opcodes + 965  ,
  /* 7f */ g_Opcodes + 966  ,
  /* 80 */ NULL  ,
  /* 81 */ NULL  ,
  /* 82 */ NULL  ,
  /* 83 */ NULL  ,
  /* 84 */ NULL  ,
  /* 85 */ NULL  ,
  /* 86 */ NULL  ,
  /* 87 */ NULL  ,
  /* 88 */ NULL  ,
  /* 89 */ NULL  ,
  /* 8a */ NULL  ,
  /* 8b */ NULL  ,
  /* 8c */ NULL  ,
  /* 8d */ NULL  ,
  /* 8e */ NULL  ,
  /* 8f */ NULL  ,
  /* 90 */ NULL  ,
  /* 91 */ NULL  ,
  /* 92 */ NULL  ,
  /* 93 */ NULL  ,
  /* 94 */ NULL  ,
  /* 95 */ NULL  ,
  /* 96 */ NULL  ,
  /* 97 */ NULL  ,
  /* 98 */ NULL  ,
  /* 99 */ NULL  ,
  /* 9a */ NULL  ,
  /* 9b */ NULL  ,
  /* 9c */ NULL  ,
  /* 9d */ NULL  ,
  /* 9e */ NULL  ,
  /* 9f */ NULL  ,
  /* a0 */ NULL  ,
  /* a1 */ NULL  ,
  /* a2 */ NULL  ,
  /* a3 */ NULL  ,
  /* a4 */ NULL  ,
  /* a5 */ NULL  ,
  /* a6 */ NULL  ,
  /* a7 */ NULL  ,
  /* a8 */ NULL  ,
  /* a9 */ NULL  ,
  /* aa */ NULL  ,
  /* ab */ NULL  ,
  /* ac */ NULL  ,
  /* ad */ NULL  ,
  /* ae */ NULL  ,
  /* af */ NULL  ,
  /* b0 */ NULL  ,
  /* b1 */ NULL  ,
  /* b2 */ NULL  ,
  /* b3 */ NULL  ,
  /* b4 */ NULL  ,
  /* b5 */ NULL  ,
  /* b6 */ NULL  ,
  /* b7 */ NULL  ,
  /* b8 */ g_Opcodes + 967  ,
  /* b9 */ g_Opcodes + 968  ,
  /* ba */ g_Opcodes + 969  ,
  /* bb */ g_Opcodes + 970  ,
  /* bc */ g_Opcodes + 971  ,
  /* bd */ g_Opcodes + 972  ,
  /* be */ g_Opcodes + 973  ,
  /* bf */ g_Opcodes + 974  ,
  /* c0 */ NULL  ,
  /* c1 */ NULL  ,
  /* c2 */ g_Opcodes + 975  ,
  /* c3 */ g_Opcodes + 976  ,
  /* c4 */ g_Opcodes + 977  ,
  /* c5 */ g_Opcodes + 978  ,
  /* c6 */ g_Opcodes + 979  ,
  /* c7 */ NULL  ,
  /* c8 */ NULL  ,
  /* c9 */ NULL  ,
  /* ca */ NULL  ,
  /* cb */ NULL  ,
  /* cc */ NULL  ,
  /* cd */ NULL  ,
  /* ce */ NULL  ,
  /* cf */ NULL  ,
  /* d0 */ g_Opcodes + 980  ,
  /* d1 */ g_Opcodes + 981  ,
  /* d2 */ g_Opcodes + 982  ,
  /* d3 */ g_Opcodes + 983  ,
  /* d4 */ g_Opcodes + 984  ,
  /* d5 */ g_Opcodes + 985  ,
  /* d6 */ g_Opcodes + 986  ,
  /* d7 */ g_Opcodes + 987  ,
  /* d8 */ g_Opcodes + 988  ,
  /* d9 */ g_Opcodes + 989  ,
  /* da */ g_Opcodes + 990  ,
  /* db */ g_Opcodes + 991  ,
  /* dc */ g_Opcodes + 992  ,
  /* dd */ g_Opcodes + 993  ,
  /* de */ g_Opcodes + 994  ,
  /* df */ g_Opcodes + 995  ,
  /* e0 */ g_Opcodes + 996  ,
  /* e1 */ g_Opcodes + 997  ,
  /* e2 */ g_Opcodes + 998  ,
  /* e3 */ g_Opcodes + 999  ,
  /* e4 */ g_Opcodes + 1000  ,
  /* e5 */ g_Opcodes + 1001  ,
  /* e6 */ g_Opcodes + 1002  ,
  /* e7 */ g_Opcodes + 1003  ,
  /* e8 */ g_Opcodes + 1004  ,
  /* e9 */ g_Opcodes + 1005  ,
  /* ea */ g_Opcodes + 1006  ,
  /* eb */ g_Opcodes + 1007  ,
  /* ec */ g_Opcodes + 1008  ,
  /* ed */ g_Opcodes + 1009  ,
  /* ee */ g_Opcodes + 1010  ,
  /* ef */ g_Opcodes + 1011  ,
  /* f0 */ g_Opcodes + 1012  ,
  /* f1 */ g_Opcodes + 1013  ,
  /* f2 */ g_Opcodes + 1014  ,
  /* f3 */ g_Opcodes + 1015  ,
  /* f4 */ g_Opcodes + 1016  ,
  /* f5 */ g_Opcodes + 1017  ,
  /* f6 */ g_Opcodes + 1018  ,
  /* f7 */ g_Opcodes + 1019  ,
  /* f8 */ g_Opcodes + 1020  ,
  /* f9 */ g_Opcodes + 1021  ,
  /* fa */ g_Opcodes + 1022  ,
  /* fb */ g_Opcodes + 1023  ,
  /* fc */ g_Opcodes + 1024  ,
  /* fd */ g_Opcodes + 1025  ,
  /* fe */ g_Opcodes + 1026  ,
  /* ff */ g_Opcodes + 1027  ,
},
/* Prefix660F */
{
  /* 00 */ NULL  ,
  /* 01 */ NULL  ,
  /* 02 */ NULL  ,
  /* 03 */ NULL  ,
  /* 04 */ NULL  ,
  /* 05 */ NULL  ,
  /* 06 */ NULL  ,
  /* 07 */ NULL  ,
  /* 08 */ NULL  ,
  /* 09 */ NULL  ,
  /* 0a */ NULL  ,
  /* 0b */ NULL  ,
  /* 0c */ NULL  ,
  /* 0d */ NULL  ,
  /* 0e */ NULL  ,
  /* 0f */ NULL  ,
  /* 10 */ g_Opcodes + 1028  ,
  /* 11 */ g_Opcodes + 1029  ,
  /* 12 */ g_Opcodes + 1030  ,
  /* 13 */ g_Opcodes + 1031  ,
  /* 14 */ g_Opcodes + 1032  ,
  /* 15 */ g_Opcodes + 1033  ,
  /* 16 */ g_Opcodes + 1034  ,
  /* 17 */ g_Opcodes + 1035  ,
  /* 18 */ NULL  ,
  /* 19 */ NULL  ,
  /* 1a */ NULL  ,
  /* 1b */ NULL  ,
  /* 1c */ NULL  ,
  /* 1d */ NULL  ,
  /* 1e */ NULL  ,
  /* 1f */ NULL  ,
  /* 20 */ NULL  ,
  /* 21 */ NULL  ,
  /* 22 */ NULL  ,
  /* 23 */ NULL  ,
  /* 24 */ NULL  ,
  /* 25 */ NULL  ,
  /* 26 */ NULL  ,
  /* 27 */ NULL  ,
  /* 28 */ g_Opcodes + 1036  ,
  /* 29 */ g_Opcodes + 1037  ,
  /* 2a */ g_Opcodes + 1038  ,
  /* 2b */ g_Opcodes + 1039  ,
  /* 2c */ g_Opcodes + 1040  ,
  /* 2d */ g_Opcodes + 1041  ,
  /* 2e */ g_Opcodes + 1042  ,
  /* 2f */ g_Opcodes + 1043  ,
  /* 30 */ NULL  ,
  /* 31 */ NULL  ,
  /* 32 */ NULL  ,
  /* 33 */ NULL  ,
  /* 34 */ NULL  ,
  /* 35 */ NULL  ,
  /* 36 */ NULL  ,
  /* 37 */ NULL  ,
  /* 38 */ NULL  ,
  /* 39 */ NULL  ,
  /* 3a */ NULL  ,
  /* 3b */ NULL  ,
  /* 3c */ NULL  ,
  /* 3d */ NULL  ,
  /* 3e */ NULL  ,
  /* 3f */ NULL  ,
  /* 40 */ NULL  ,
  /* 41 */ NULL  ,
  /* 42 */ NULL  ,
  /* 43 */ NULL  ,
  /* 44 */ NULL  ,
  /* 45 */ NULL  ,
  /* 46 */ NULL  ,
  /* 47 */ NULL  ,
  /* 48 */ NULL  ,
  /* 49 */ NULL  ,
  /* 4a */ NULL  ,
  /* 4b */ NULL  ,
  /* 4c */ NULL  ,
  /* 4d */ NULL  ,
  /* 4e */ NULL  ,
  /* 4f */ NULL  ,
  /* 50 */ g_Opcodes + 1044  ,
  /* 51 */ g_Opcodes + 1045  ,
  /* 52 */ g_Opcodes + 1046  ,
  /* 53 */ g_Opcodes + 1047  ,
  /* 54 */ g_Opcodes + 1048  ,
  /* 55 */ g_Opcodes + 1049  ,
  /* 56 */ g_Opcodes + 1050  ,
  /* 57 */ g_Opcodes + 1051  ,
  /* 58 */ g_Opcodes + 1052  ,
  /* 59 */ g_Opcodes + 1053  ,
  /* 5a */ g_Opcodes + 1054  ,
  /* 5b */ g_Opcodes + 1055  ,
  /* 5c */ g_Opcodes + 1056  ,
  /* 5d */ g_Opcodes + 1057  ,
  /* 5e */ g_Opcodes + 1058  ,
  /* 5f */ g_Opcodes + 1059  ,
  /* 60 */ g_Opcodes + 1060  ,
  /* 61 */ g_Opcodes + 1061  ,
  /* 62 */ g_Opcodes + 1062  ,
  /* 63 */ g_Opcodes + 1063  ,
  /* 64 */ g_Opcodes + 1064  ,
  /* 65 */ g_Opcodes + 1065  ,
  /* 66 */ g_Opcodes + 1066  ,
  /* 67 */ g_Opcodes + 1067  ,
  /* 68 */ g_Opcodes + 1068  ,
  /* 69 */ g_Opcodes + 1069  ,
  /* 6a */ g_Opcodes + 1070  ,
  /* 6b */ g_Opcodes + 1071  ,
  /* 6c */ g_Opcodes + 1072  ,
  /* 6d */ g_Opcodes + 1073  ,
  /* 6e */ g_Opcodes + 1074  ,
  /* 6f */ g_Opcodes + 1076  ,
  /* 70 */ g_Opcodes + 1077  ,
  /* 71 */ g_Opcodes + 1078  ,
  /* 72 */ g_Opcodes + 1086  ,
  /* 73 */ g_Opcodes + 1094  ,
  /* 74 */ g_Opcodes + 1102  ,
  /* 75 */ g_Opcodes + 1103  ,
  /* 76 */ g_Opcodes + 1104  ,
  /* 77 */ g_Opcodes + 1105  ,
  /* 78 */ g_Opcodes + 1106  ,
  /* 79 */ g_Opcodes + 1108  ,
  /* 7a */ g_Opcodes + 1109  ,
  /* 7b */ g_Opcodes + 1110  ,
  /* 7c */ g_Opcodes + 1111  ,
  /* 7d */ g_Opcodes + 1112  ,
  /* 7e */ g_Opcodes + 1113  ,
  /* 7f */ g_Opcodes + 1115  ,
  /* 80 */ NULL  ,
  /* 81 */ NULL  ,
  /* 82 */ NULL  ,
  /* 83 */ NULL  ,
  /* 84 */ NULL  ,
  /* 85 */ NULL  ,
  /* 86 */ NULL  ,
  /* 87 */ NULL  ,
  /* 88 */ NULL  ,
  /* 89 */ NULL  ,
  /* 8a */ NULL  ,
  /* 8b */ NULL  ,
  /* 8c */ NULL  ,
  /* 8d */ NULL  ,
  /* 8e */ NULL  ,
  /* 8f */ NULL  ,
  /* 90 */ NULL  ,
  /* 91 */ NULL  ,
  /* 92 */ NULL  ,
  /* 93 */ NULL  ,
  /* 94 */ NULL  ,
  /* 95 */ NULL  ,
  /* 96 */ NULL  ,
  /* 97 */ NULL  ,
  /* 98 */ NULL  ,
  /* 99 */ NULL  ,
  /* 9a */ NULL  ,
  /* 9b */ NULL  ,
  /* 9c */ NULL  ,
  /* 9d */ NULL  ,
  /* 9e */ NULL  ,
  /* 9f */ NULL  ,
  /* a0 */ NULL  ,
  /* a1 */ NULL  ,
  /* a2 */ NULL  ,
  /* a3 */ NULL  ,
  /* a4 */ NULL  ,
  /* a5 */ NULL  ,
  /* a6 */ NULL  ,
  /* a7 */ NULL  ,
  /* a8 */ NULL  ,
  /* a9 */ NULL  ,
  /* aa */ NULL  ,
  /* ab */ NULL  ,
  /* ac */ NULL  ,
  /* ad */ NULL  ,
  /* ae */ g_Opcodes + 1116  ,
  /* af */ NULL  ,
  /* b0 */ NULL  ,
  /* b1 */ NULL  ,
  /* b2 */ NULL  ,
  /* b3 */ NULL  ,
  /* b4 */ NULL  ,
  /* b5 */ NULL  ,
  /* b6 */ NULL  ,
  /* b7 */ NULL  ,
  /* b8 */ NULL  ,
  /* b9 */ NULL  ,
  /* ba */ NULL  ,
  /* bb */ NULL  ,
  /* bc */ NULL  ,
  /* bd */ NULL  ,
  /* be */ NULL  ,
  /* bf */ NULL  ,
  /* c0 */ NULL  ,
  /* c1 */ NULL  ,
  /* c2 */ g_Opcodes + 1117  ,
  /* c3 */ g_Opcodes + 1118  ,
  /* c4 */ g_Opcodes + 1119  ,
  /* c5 */ g_Opcodes + 1120  ,
  /* c6 */ g_Opcodes + 1121  ,
  /* c7 */ NULL  ,
  /* c8 */ NULL  ,
  /* c9 */ NULL  ,
  /* ca */ NULL  ,
  /* cb */ NULL  ,
  /* cc */ NULL  ,
  /* cd */ NULL  ,
  /* ce */ NULL  ,
  /* cf */ NULL  ,
  /* d0 */ g_Opcodes + 1122  ,
  /* d1 */ g_Opcodes + 1123  ,
  /* d2 */ g_Opcodes + 1124  ,
  /* d3 */ g_Opcodes + 1125  ,
  /* d4 */ g_Opcodes + 1126  ,
  /* d5 */ g_Opcodes + 1127  ,
  /* d6 */ g_Opcodes + 1128  ,
  /* d7 */ g_Opcodes + 1129  ,
  /* d8 */ g_Opcodes + 1130  ,
  /* d9 */ g_Opcodes + 1131  ,
  /* da */ g_Opcodes + 1132  ,
  /* db */ g_Opcodes + 1133  ,
  /* dc */ g_Opcodes + 1134  ,
  /* dd */ g_Opcodes + 1135  ,
  /* de */ g_Opcodes + 1136  ,
  /* df */ g_Opcodes + 1137  ,
  /* e0 */ g_Opcodes + 1138  ,
  /* e1 */ g_Opcodes + 1139  ,
  /* e2 */ g_Opcodes + 1140  ,
  /* e3 */ g_Opcodes + 1141  ,
  /* e4 */ g_Opcodes + 1142  ,
  /* e5 */ g_Opcodes + 1143  ,
  /* e6 */ g_Opcodes + 1144  ,
  /* e7 */ g_Opcodes + 1145  ,
  /* e8 */ g_Opcodes + 1146  ,
  /* e9 */ g_Opcodes + 1147  ,
  /* ea */ g_Opcodes + 1148  ,
  /* eb */ g_Opcodes + 1149  ,
  /* ec */ g_Opcodes + 1150  ,
  /* ed */ g_Opcodes + 1151  ,
  /* ee */ g_Opcodes + 1152  ,
  /* ef */ g_Opcodes + 1153  ,
  /* f0 */ g_Opcodes + 1154  ,
  /* f1 */ g_Opcodes + 1155  ,
  /* f2 */ g_Opcodes + 1156  ,
  /* f3 */ g_Opcodes + 1157  ,
  /* f4 */ g_Opcodes + 1158  ,
  /* f5 */ g_Opcodes + 1159  ,
  /* f6 */ g_Opcodes + 1160  ,
  /* f7 */ g_Opcodes + 1161  ,
  /* f8 */ g_Opcodes + 1162  ,
  /* f9 */ g_Opcodes + 1163  ,
  /* fa */ g_Opcodes + 1164  ,
  /* fb */ g_Opcodes + 1165  ,
  /* fc */ g_Opcodes + 1166  ,
  /* fd */ g_Opcodes + 1167  ,
  /* fe */ g_Opcodes + 1168  ,
  /* ff */ g_Opcodes + 1169  ,
},
/* Prefix0F0F */
{
  /* 00 */ NULL  ,
  /* 01 */ NULL  ,
  /* 02 */ NULL  ,
  /* 03 */ NULL  ,
  /* 04 */ NULL  ,
  /* 05 */ NULL  ,
  /* 06 */ NULL  ,
  /* 07 */ NULL  ,
  /* 08 */ NULL  ,
  /* 09 */ NULL  ,
  /* 0a */ NULL  ,
  /* 0b */ NULL  ,
  /* 0c */ g_Opcodes + 1170  ,
  /* 0d */ g_Opcodes + 1171  ,
  /* 0e */ NULL  ,
  /* 0f */ NULL  ,
  /* 10 */ NULL  ,
  /* 11 */ NULL  ,
  /* 12 */ NULL  ,
  /* 13 */ NULL  ,
  /* 14 */ NULL  ,
  /* 15 */ NULL  ,
  /* 16 */ NULL  ,
  /* 17 */ NULL  ,
  /* 18 */ NULL  ,
  /* 19 */ NULL  ,
  /* 1a */ NULL  ,
  /* 1b */ NULL  ,
  /* 1c */ g_Opcodes + 1172  ,
  /* 1d */ g_Opcodes + 1173  ,
  /* 1e */ NULL  ,
  /* 1f */ NULL  ,
  /* 20 */ NULL  ,
  /* 21 */ NULL  ,
  /* 22 */ NULL  ,
  /* 23 */ NULL  ,
  /* 24 */ NULL  ,
  /* 25 */ NULL  ,
  /* 26 */ NULL  ,
  /* 27 */ NULL  ,
  /* 28 */ NULL  ,
  /* 29 */ NULL  ,
  /* 2a */ NULL  ,
  /* 2b */ NULL  ,
  /* 2c */ NULL  ,
  /* 2d */ NULL  ,
  /* 2e */ NULL  ,
  /* 2f */ NULL  ,
  /* 30 */ NULL  ,
  /* 31 */ NULL  ,
  /* 32 */ NULL  ,
  /* 33 */ NULL  ,
  /* 34 */ NULL  ,
  /* 35 */ NULL  ,
  /* 36 */ NULL  ,
  /* 37 */ NULL  ,
  /* 38 */ NULL  ,
  /* 39 */ NULL  ,
  /* 3a */ NULL  ,
  /* 3b */ NULL  ,
  /* 3c */ NULL  ,
  /* 3d */ NULL  ,
  /* 3e */ NULL  ,
  /* 3f */ NULL  ,
  /* 40 */ NULL  ,
  /* 41 */ NULL  ,
  /* 42 */ NULL  ,
  /* 43 */ NULL  ,
  /* 44 */ NULL  ,
  /* 45 */ NULL  ,
  /* 46 */ NULL  ,
  /* 47 */ NULL  ,
  /* 48 */ NULL  ,
  /* 49 */ NULL  ,
  /* 4a */ NULL  ,
  /* 4b */ NULL  ,
  /* 4c */ NULL  ,
  /* 4d */ NULL  ,
  /* 4e */ NULL  ,
  /* 4f */ NULL  ,
  /* 50 */ NULL  ,
  /* 51 */ NULL  ,
  /* 52 */ NULL  ,
  /* 53 */ NULL  ,
  /* 54 */ NULL  ,
  /* 55 */ NULL  ,
  /* 56 */ NULL  ,
  /* 57 */ NULL  ,
  /* 58 */ NULL  ,
  /* 59 */ NULL  ,
  /* 5a */ NULL  ,
  /* 5b */ NULL  ,
  /* 5c */ NULL  ,
  /* 5d */ NULL  ,
  /* 5e */ NULL  ,
  /* 5f */ NULL  ,
  /* 60 */ NULL  ,
  /* 61 */ NULL  ,
  /* 62 */ NULL  ,
  /* 63 */ NULL  ,
  /* 64 */ NULL  ,
  /* 65 */ NULL  ,
  /* 66 */ NULL  ,
  /* 67 */ NULL  ,
  /* 68 */ NULL  ,
  /* 69 */ NULL  ,
  /* 6a */ NULL  ,
  /* 6b */ NULL  ,
  /* 6c */ NULL  ,
  /* 6d */ NULL  ,
  /* 6e */ NULL  ,
  /* 6f */ NULL  ,
  /* 70 */ NULL  ,
  /* 71 */ NULL  ,
  /* 72 */ NULL  ,
  /* 73 */ NULL  ,
  /* 74 */ NULL  ,
  /* 75 */ NULL  ,
  /* 76 */ NULL  ,
  /* 77 */ NULL  ,
  /* 78 */ NULL  ,
  /* 79 */ NULL  ,
  /* 7a */ NULL  ,
  /* 7b */ NULL  ,
  /* 7c */ NULL  ,
  /* 7d */ NULL  ,
  /* 7e */ NULL  ,
  /* 7f */ NULL  ,
  /* 80 */ NULL  ,
  /* 81 */ NULL  ,
  /* 82 */ NULL  ,
  /* 83 */ NULL  ,
  /* 84 */ NULL  ,
  /* 85 */ NULL  ,
  /* 86 */ NULL  ,
  /* 87 */ NULL  ,
  /* 88 */ NULL  ,
  /* 89 */ NULL  ,
  /* 8a */ g_Opcodes + 1174  ,
  /* 8b */ NULL  ,
  /* 8c */ NULL  ,
  /* 8d */ NULL  ,
  /* 8e */ g_Opcodes + 1175  ,
  /* 8f */ NULL  ,
  /* 90 */ g_Opcodes + 1176  ,
  /* 91 */ NULL  ,
  /* 92 */ NULL  ,
  /* 93 */ NULL  ,
  /* 94 */ g_Opcodes + 1177  ,
  /* 95 */ NULL  ,
  /* 96 */ g_Opcodes + 1178  ,
  /* 97 */ g_Opcodes + 1179  ,
  /* 98 */ NULL  ,
  /* 99 */ NULL  ,
  /* 9a */ g_Opcodes + 1180  ,
  /* 9b */ NULL  ,
  /* 9c */ NULL  ,
  /* 9d */ NULL  ,
  /* 9e */ g_Opcodes + 1181  ,
  /* 9f */ NULL  ,
  /* a0 */ g_Opcodes + 1182  ,
  /* a1 */ NULL  ,
  /* a2 */ NULL  ,
  /* a3 */ NULL  ,
  /* a4 */ g_Opcodes + 1183  ,
  /* a5 */ NULL  ,
  /* a6 */ g_Opcodes + 1184  ,
  /* a7 */ g_Opcodes + 1185  ,
  /* a8 */ NULL  ,
  /* a9 */ NULL  ,
  /* aa */ g_Opcodes + 1186  ,
  /* ab */ NULL  ,
  /* ac */ NULL  ,
  /* ad */ NULL  ,
  /* ae */ g_Opcodes + 1187  ,
  /* af */ NULL  ,
  /* b0 */ g_Opcodes + 1188  ,
  /* b1 */ NULL  ,
  /* b2 */ NULL  ,
  /* b3 */ NULL  ,
  /* b4 */ g_Opcodes + 1189  ,
  /* b5 */ NULL  ,
  /* b6 */ g_Opcodes + 1190  ,
  /* b7 */ g_Opcodes + 1191  ,
  /* b8 */ NULL  ,
  /* b9 */ NULL  ,
  /* ba */ NULL  ,
  /* bb */ g_Opcodes + 1192  ,
  /* bc */ NULL  ,
  /* bd */ NULL  ,
  /* be */ NULL  ,
  /* bf */ g_Opcodes + 1193  ,
  /* c0 */ NULL  ,
  /* c1 */ NULL  ,
  /* c2 */ NULL  ,
  /* c3 */ NULL  ,
  /* c4 */ NULL  ,
  /* c5 */ NULL  ,
  /* c6 */ NULL  ,
  /* c7 */ NULL  ,
  /* c8 */ NULL  ,
  /* c9 */ NULL  ,
  /* ca */ NULL  ,
  /* cb */ NULL  ,
  /* cc */ NULL  ,
  /* cd */ NULL  ,
  /* ce */ NULL  ,
  /* cf */ NULL  ,
  /* d0 */ NULL  ,
  /* d1 */ NULL  ,
  /* d2 */ NULL  ,
  /* d3 */ NULL  ,
  /* d4 */ NULL  ,
  /* d5 */ NULL  ,
  /* d6 */ NULL  ,
  /* d7 */ NULL  ,
  /* d8 */ NULL  ,
  /* d9 */ NULL  ,
  /* da */ NULL  ,
  /* db */ NULL  ,
  /* dc */ NULL  ,
  /* dd */ NULL  ,
  /* de */ NULL  ,
  /* df */ NULL  ,
  /* e0 */ NULL  ,
  /* e1 */ NULL  ,
  /* e2 */ NULL  ,
  /* e3 */ NULL  ,
  /* e4 */ NULL  ,
  /* e5 */ NULL  ,
  /* e6 */ NULL  ,
  /* e7 */ NULL  ,
  /* e8 */ NULL  ,
  /* e9 */ NULL  ,
  /* ea */ NULL  ,
  /* eb */ NULL  ,
  /* ec */ NULL  ,
  /* ed */ NULL  ,
  /* ee */ NULL  ,
  /* ef */ NULL  ,
  /* f0 */ NULL  ,
  /* f1 */ NULL  ,
  /* f2 */ NULL  ,
  /* f3 */ NULL  ,
  /* f4 */ NULL  ,
  /* f5 */ NULL  ,
  /* f6 */ NULL  ,
  /* f7 */ NULL  ,
  /* f8 */ NULL  ,
  /* f9 */ NULL  ,
  /* fa */ NULL  ,
  /* fb */ NULL  ,
  /* fc */ NULL  ,
  /* fd */ NULL  ,
  /* fe */ NULL  ,
  /* ff */ NULL  ,
},
/* Prefix0F38 */
{
  /* 00 */ g_Opcodes + 1194  ,
  /* 01 */ g_Opcodes + 1195  ,
  /* 02 */ g_Opcodes + 1196  ,
  /* 03 */ g_Opcodes + 1197  ,
  /* 04 */ g_Opcodes + 1198  ,
  /* 05 */ g_Opcodes + 1199  ,
  /* 06 */ g_Opcodes + 1200  ,
  /* 07 */ g_Opcodes + 1201  ,
  /* 08 */ g_Opcodes + 1202  ,
  /* 09 */ g_Opcodes + 1203  ,
  /* 0a */ g_Opcodes + 1204  ,
  /* 0b */ g_Opcodes + 1205  ,
  /* 0c */ g_Opcodes + 1206  ,
  /* 0d */ g_Opcodes + 1207  ,
  /* 0e */ g_Opcodes + 1208  ,
  /* 0f */ g_Opcodes + 1209  ,
  /* 10 */ g_Opcodes + 1210  ,
  /* 11 */ g_Opcodes + 1211  ,
  /* 12 */ g_Opcodes + 1212  ,
  /* 13 */ g_Opcodes + 1213  ,
  /* 14 */ g_Opcodes + 1214  ,
  /* 15 */ g_Opcodes + 1215  ,
  /* 16 */ g_Opcodes + 1216  ,
  /* 17 */ g_Opcodes + 1217  ,
  /* 18 */ g_Opcodes + 1218  ,
  /* 19 */ g_Opcodes + 1219  ,
  /* 1a */ g_Opcodes + 1220  ,
  /* 1b */ g_Opcodes + 1221  ,
  /* 1c */ g_Opcodes + 1222  ,
  /* 1d */ g_Opcodes + 1223  ,
  /* 1e */ g_Opcodes + 1224  ,
  /* 1f */ g_Opcodes + 1225  ,
  /* 20 */ g_Opcodes + 1226  ,
  /* 21 */ g_Opcodes + 1227  ,
  /* 22 */ g_Opcodes + 1228  ,
  /* 23 */ g_Opcodes + 1229  ,
  /* 24 */ g_Opcodes + 1230  ,
  /* 25 */ g_Opcodes + 1231  ,
  /* 26 */ g_Opcodes + 1232  ,
  /* 27 */ g_Opcodes + 1233  ,
  /* 28 */ g_Opcodes + 1234  ,
  /* 29 */ g_Opcodes + 1235  ,
  /* 2a */ g_Opcodes + 1236  ,
  /* 2b */ g_Opcodes + 1237  ,
  /* 2c */ g_Opcodes + 1238  ,
  /* 2d */ g_Opcodes + 1239  ,
  /* 2e */ g_Opcodes + 1240  ,
  /* 2f */ g_Opcodes + 1241  ,
  /* 30 */ g_Opcodes + 1242  ,
  /* 31 */ g_Opcodes + 1243  ,
  /* 32 */ g_Opcodes + 1244  ,
  /* 33 */ g_Opcodes + 1245  ,
  /* 34 */ g_Opcodes + 1246  ,
  /* 35 */ g_Opcodes + 1247  ,
  /* 36 */ g_Opcodes + 1248  ,
  /* 37 */ g_Opcodes + 1249  ,
  /* 38 */ g_Opcodes + 1250  ,
  /* 39 */ g_Opcodes + 1251  ,
  /* 3a */ g_Opcodes + 1252  ,
  /* 3b */ g_Opcodes + 1253  ,
  /* 3c */ g_Opcodes + 1254  ,
  /* 3d */ g_Opcodes + 1255  ,
  /* 3e */ g_Opcodes + 1256  ,
  /* 3f */ g_Opcodes + 1257  ,
  /* 40 */ g_Opcodes + 1258  ,
  /* 41 */ g_Opcodes + 1259  ,
  /* 42 */ g_Opcodes + 1260  ,
  /* 43 */ g_Opcodes + 1261  ,
  /* 44 */ g_Opcodes + 1262  ,
  /* 45 */ g_Opcodes + 1263  ,
  /* 46 */ g_Opcodes + 1264  ,
  /* 47 */ g_Opcodes + 1265  ,
  /* 48 */ g_Opcodes + 1266  ,
  /* 49 */ g_Opcodes + 1267  ,
  /* 4a */ g_Opcodes + 1268  ,
  /* 4b */ g_Opcodes + 1269  ,
  /* 4c */ g_Opcodes + 1270  ,
  /* 4d */ g_Opcodes + 1271  ,
  /* 4e */ g_Opcodes + 1272  ,
  /* 4f */ g_Opcodes + 1273  ,
  /* 50 */ g_Opcodes + 1274  ,
  /* 51 */ g_Opcodes + 1275  ,
  /* 52 */ g_Opcodes + 1276  ,
  /* 53 */ g_Opcodes + 1277  ,
  /* 54 */ g_Opcodes + 1278  ,
  /* 55 */ g_Opcodes + 1279  ,
  /* 56 */ g_Opcodes + 1280  ,
  /* 57 */ g_Opcodes + 1281  ,
  /* 58 */ g_Opcodes + 1282  ,
  /* 59 */ g_Opcodes + 1283  ,
  /* 5a */ g_Opcodes + 1284  ,
  /* 5b */ g_Opcodes + 1285  ,
  /* 5c */ g_Opcodes + 1286  ,
  /* 5d */ g_Opcodes + 1287  ,
  /* 5e */ g_Opcodes + 1288  ,
  /* 5f */ g_Opcodes + 1289  ,
  /* 60 */ g_Opcodes + 1290  ,
  /* 61 */ g_Opcodes + 1291  ,
  /* 62 */ g_Opcodes + 1292  ,
  /* 63 */ g_Opcodes + 1293  ,
  /* 64 */ g_Opcodes + 1294  ,
  /* 65 */ g_Opcodes + 1295  ,
  /* 66 */ g_Opcodes + 1296  ,
  /* 67 */ g_Opcodes + 1297  ,
  /* 68 */ g_Opcodes + 1298  ,
  /* 69 */ g_Opcodes + 1299  ,
  /* 6a */ g_Opcodes + 1300  ,
  /* 6b */ g_Opcodes + 1301  ,
  /* 6c */ g_Opcodes + 1302  ,
  /* 6d */ g_Opcodes + 1303  ,
  /* 6e */ g_Opcodes + 1304  ,
  /* 6f */ g_Opcodes + 1305  ,
  /* 70 */ g_Opcodes + 1306  ,
  /* 71 */ g_Opcodes + 1307  ,
  /* 72 */ g_Opcodes + 1308  ,
  /* 73 */ g_Opcodes + 1309  ,
  /* 74 */ g_Opcodes + 1310  ,
  /* 75 */ g_Opcodes + 1311  ,
  /* 76 */ g_Opcodes + 1312  ,
  /* 77 */ g_Opcodes + 1313  ,
  /* 78 */ g_Opcodes + 1314  ,
  /* 79 */ g_Opcodes + 1315  ,
  /* 7a */ g_Opcodes + 1316  ,
  /* 7b */ g_Opcodes + 1317  ,
  /* 7c */ g_Opcodes + 1318  ,
  /* 7d */ g_Opcodes + 1319  ,
  /* 7e */ g_Opcodes + 1320  ,
  /* 7f */ g_Opcodes + 1321  ,
  /* 80 */ g_Opcodes + 1322  ,
  /* 81 */ g_Opcodes + 1323  ,
  /* 82 */ g_Opcodes + 1324  ,
  /* 83 */ g_Opcodes + 1325  ,
  /* 84 */ g_Opcodes + 1326  ,
  /* 85 */ g_Opcodes + 1327  ,
  /* 86 */ g_Opcodes + 1328  ,
  /* 87 */ g_Opcodes + 1329  ,
  /* 88 */ g_Opcodes + 1330  ,
  /* 89 */ g_Opcodes + 1331  ,
  /* 8a */ g_Opcodes + 1332  ,
  /* 8b */ g_Opcodes + 1333  ,
  /* 8c */ g_Opcodes + 1334  ,
  /* 8d */ g_Opcodes + 1335  ,
  /* 8e */ g_Opcodes + 1336  ,
  /* 8f */ g_Opcodes + 1337  ,
  /* 90 */ g_Opcodes + 1338  ,
  /* 91 */ g_Opcodes + 1339  ,
  /* 92 */ g_Opcodes + 1340  ,
  /* 93 */ g_Opcodes + 1341  ,
  /* 94 */ g_Opcodes + 1342  ,
  /* 95 */ g_Opcodes + 1343  ,
  /* 96 */ g_Opcodes + 1344  ,
  /* 97 */ g_Opcodes + 1345  ,
  /* 98 */ g_Opcodes + 1346  ,
  /* 99 */ g_Opcodes + 1347  ,
  /* 9a */ g_Opcodes + 1348  ,
  /* 9b */ g_Opcodes + 1349  ,
  /* 9c */ g_Opcodes + 1350  ,
  /* 9d */ g_Opcodes + 1351  ,
  /* 9e */ g_Opcodes + 1352  ,
  /* 9f */ g_Opcodes + 1353  ,
  /* a0 */ g_Opcodes + 1354  ,
  /* a1 */ g_Opcodes + 1355  ,
  /* a2 */ g_Opcodes + 1356  ,
  /* a3 */ g_Opcodes + 1357  ,
  /* a4 */ g_Opcodes + 1358  ,
  /* a5 */ g_Opcodes + 1359  ,
  /* a6 */ g_Opcodes + 1360  ,
  /* a7 */ g_Opcodes + 1361  ,
  /* a8 */ g_Opcodes + 1362  ,
  /* a9 */ g_Opcodes + 1363  ,
  /* aa */ g_Opcodes + 1364  ,
  /* ab */ g_Opcodes + 1365  ,
  /* ac */ g_Opcodes + 1366  ,
  /* ad */ g_Opcodes + 1367  ,
  /* ae */ g_Opcodes + 1368  ,
  /* af */ g_Opcodes + 1369  ,
  /* b0 */ g_Opcodes + 1370  ,
  /* b1 */ g_Opcodes + 1371  ,
  /* b2 */ g_Opcodes + 1372  ,
  /* b3 */ g_Opcodes + 1373  ,
  /* b4 */ g_Opcodes + 1374  ,
  /* b5 */ g_Opcodes + 1375  ,
  /* b6 */ g_Opcodes + 1376  ,
  /* b7 */ g_Opcodes + 1377  ,
  /* b8 */ g_Opcodes + 1378  ,
  /* b9 */ g_Opcodes + 1379  ,
  /* ba */ g_Opcodes + 1380  ,
  /* bb */ g_Opcodes + 1381  ,
  /* bc */ g_Opcodes + 1382  ,
  /* bd */ g_Opcodes + 1383  ,
  /* be */ g_Opcodes + 1384  ,
  /* bf */ g_Opcodes + 1385  ,
  /* c0 */ g_Opcodes + 1386  ,
  /* c1 */ g_Opcodes + 1387  ,
  /* c2 */ g_Opcodes + 1388  ,
  /* c3 */ g_Opcodes + 1389  ,
  /* c4 */ g_Opcodes + 1390  ,
  /* c5 */ g_Opcodes + 1391  ,
  /* c6 */ g_Opcodes + 1392  ,
  /* c7 */ g_Opcodes + 1393  ,
  /* c8 */ g_Opcodes + 1394  ,
  /* c9 */ g_Opcodes + 1395  ,
  /* ca */ g_Opcodes + 1396  ,
  /* cb */ g_Opcodes + 1397  ,
  /* cc */ g_Opcodes + 1398  ,
  /* cd */ g_Opcodes + 1399  ,
  /* ce */ g_Opcodes + 1400  ,
  /* cf */ g_Opcodes + 1401  ,
  /* d0 */ g_Opcodes + 1402  ,
  /* d1 */ g_Opcodes + 1403  ,
  /* d2 */ g_Opcodes + 1404  ,
  /* d3 */ g_Opcodes + 1405  ,
  /* d4 */ g_Opcodes + 1406  ,
  /* d5 */ g_Opcodes + 1407  ,
  /* d6 */ g_Opcodes + 1408  ,
  /* d7 */ g_Opcodes + 1409  ,
  /* d8 */ g_Opcodes + 1410  ,
  /* d9 */ g_Opcodes + 1411  ,
  /* da */ g_Opcodes + 1412  ,
  /* db */ g_Opcodes + 1413  ,
  /* dc */ g_Opcodes + 1414  ,
  /* dd */ g_Opcodes + 1415  ,
  /* de */ g_Opcodes + 1416  ,
  /* df */ g_Opcodes + 1417  ,
  /* e0 */ g_Opcodes + 1418  ,
  /* e1 */ g_Opcodes + 1419  ,
  /* e2 */ g_Opcodes + 1420  ,
  /* e3 */ g_Opcodes + 1421  ,
  /* e4 */ g_Opcodes + 1422  ,
  /* e5 */ g_Opcodes + 1423  ,
  /* e6 */ g_Opcodes + 1424  ,
  /* e7 */ g_Opcodes + 1425  ,
  /* e8 */ g_Opcodes + 1426  ,
  /* e9 */ g_Opcodes + 1427  ,
  /* ea */ g_Opcodes + 1428  ,
  /* eb */ g_Opcodes + 1429  ,
  /* ec */ g_Opcodes + 1430  ,
  /* ed */ g_Opcodes + 1431  ,
  /* ee */ g_Opcodes + 1432  ,
  /* ef */ g_Opcodes + 1433  ,
  /* f0 */ g_Opcodes + 1434  ,
  /* f1 */ g_Opcodes + 1435  ,
  /* f2 */ g_Opcodes + 1436  ,
  /* f3 */ g_Opcodes + 1437  ,
  /* f4 */ g_Opcodes + 1438  ,
  /* f5 */ g_Opcodes + 1439  ,
  /* f6 */ g_Opcodes + 1440  ,
  /* f7 */ g_Opcodes + 1441  ,
  /* f8 */ g_Opcodes + 1442  ,
  /* f9 */ g_Opcodes + 1443  ,
  /* fa */ g_Opcodes + 1444  ,
  /* fb */ g_Opcodes + 1445  ,
  /* fc */ g_Opcodes + 1446  ,
  /* fd */ g_Opcodes + 1447  ,
  /* fe */ g_Opcodes + 1448  ,
  /* ff */ g_Opcodes + 1449  ,
},
/* Prefix660F38 */
{
  /* 00 */ g_Opcodes + 1450  ,
  /* 01 */ g_Opcodes + 1451  ,
  /* 02 */ g_Opcodes + 1452  ,
  /* 03 */ g_Opcodes + 1453  ,
  /* 04 */ g_Opcodes + 1454  ,
  /* 05 */ g_Opcodes + 1455  ,
  /* 06 */ g_Opcodes + 1456  ,
  /* 07 */ g_Opcodes + 1457  ,
  /* 08 */ g_Opcodes + 1458  ,
  /* 09 */ g_Opcodes + 1459  ,
  /* 0a */ g_Opcodes + 1460  ,
  /* 0b */ g_Opcodes + 1461  ,
  /* 0c */ g_Opcodes + 1462  ,
  /* 0d */ g_Opcodes + 1463  ,
  /* 0e */ g_Opcodes + 1464  ,
  /* 0f */ g_Opcodes + 1465  ,
  /* 10 */ g_Opcodes + 1466  ,
  /* 11 */ g_Opcodes + 1467  ,
  /* 12 */ g_Opcodes + 1468  ,
  /* 13 */ g_Opcodes + 1469  ,
  /* 14 */ g_Opcodes + 1470  ,
  /* 15 */ g_Opcodes + 1471  ,
  /* 16 */ g_Opcodes + 1472  ,
  /* 17 */ g_Opcodes + 1473  ,
  /* 18 */ g_Opcodes + 1474  ,
  /* 19 */ g_Opcodes + 1475  ,
  /* 1a */ g_Opcodes + 1476  ,
  /* 1b */ g_Opcodes + 1477  ,
  /* 1c */ g_Opcodes + 1478  ,
  /* 1d */ g_Opcodes + 1479  ,
  /* 1e */ g_Opcodes + 1480  ,
  /* 1f */ g_Opcodes + 1481  ,
  /* 20 */ g_Opcodes + 1482  ,
  /* 21 */ g_Opcodes + 1483  ,
  /* 22 */ g_Opcodes + 1484  ,
  /* 23 */ g_Opcodes + 1485  ,
  /* 24 */ g_Opcodes + 1486  ,
  /* 25 */ g_Opcodes + 1487  ,
  /* 26 */ g_Opcodes + 1488  ,
  /* 27 */ g_Opcodes + 1489  ,
  /* 28 */ g_Opcodes + 1490  ,
  /* 29 */ g_Opcodes + 1491  ,
  /* 2a */ g_Opcodes + 1492  ,
  /* 2b */ g_Opcodes + 1493  ,
  /* 2c */ g_Opcodes + 1494  ,
  /* 2d */ g_Opcodes + 1495  ,
  /* 2e */ g_Opcodes + 1496  ,
  /* 2f */ g_Opcodes + 1497  ,
  /* 30 */ g_Opcodes + 1498  ,
  /* 31 */ g_Opcodes + 1499  ,
  /* 32 */ g_Opcodes + 1500  ,
  /* 33 */ g_Opcodes + 1501  ,
  /* 34 */ g_Opcodes + 1502  ,
  /* 35 */ g_Opcodes + 1503  ,
  /* 36 */ g_Opcodes + 1504  ,
  /* 37 */ g_Opcodes + 1505  ,
  /* 38 */ g_Opcodes + 1506  ,
  /* 39 */ g_Opcodes + 1507  ,
  /* 3a */ g_Opcodes + 1508  ,
  /* 3b */ g_Opcodes + 1509  ,
  /* 3c */ g_Opcodes + 1510  ,
  /* 3d */ g_Opcodes + 1511  ,
  /* 3e */ g_Opcodes + 1512  ,
  /* 3f */ g_Opcodes + 1513  ,
  /* 40 */ g_Opcodes + 1514  ,
  /* 41 */ g_Opcodes + 1515  ,
  /* 42 */ g_Opcodes + 1516  ,
  /* 43 */ g_Opcodes + 1517  ,
  /* 44 */ g_Opcodes + 1518  ,
  /* 45 */ g_Opcodes + 1519  ,
  /* 46 */ g_Opcodes + 1520  ,
  /* 47 */ g_Opcodes + 1521  ,
  /* 48 */ g_Opcodes + 1522  ,
  /* 49 */ g_Opcodes + 1523  ,
  /* 4a */ g_Opcodes + 1524  ,
  /* 4b */ g_Opcodes + 1525  ,
  /* 4c */ g_Opcodes + 1526  ,
  /* 4d */ g_Opcodes + 1527  ,
  /* 4e */ g_Opcodes + 1528  ,
  /* 4f */ g_Opcodes + 1529  ,
  /* 50 */ g_Opcodes + 1530  ,
  /* 51 */ g_Opcodes + 1531  ,
  /* 52 */ g_Opcodes + 1532  ,
  /* 53 */ g_Opcodes + 1533  ,
  /* 54 */ g_Opcodes + 1534  ,
  /* 55 */ g_Opcodes + 1535  ,
  /* 56 */ g_Opcodes + 1536  ,
  /* 57 */ g_Opcodes + 1537  ,
  /* 58 */ g_Opcodes + 1538  ,
  /* 59 */ g_Opcodes + 1539  ,
  /* 5a */ g_Opcodes + 1540  ,
  /* 5b */ g_Opcodes + 1541  ,
  /* 5c */ g_Opcodes + 1542  ,
  /* 5d */ g_Opcodes + 1543  ,
  /* 5e */ g_Opcodes + 1544  ,
  /* 5f */ g_Opcodes + 1545  ,
  /* 60 */ g_Opcodes + 1546  ,
  /* 61 */ g_Opcodes + 1547  ,
  /* 62 */ g_Opcodes + 1548  ,
  /* 63 */ g_Opcodes + 1549  ,
  /* 64 */ g_Opcodes + 1550  ,
  /* 65 */ g_Opcodes + 1551  ,
  /* 66 */ g_Opcodes + 1552  ,
  /* 67 */ g_Opcodes + 1553  ,
  /* 68 */ g_Opcodes + 1554  ,
  /* 69 */ g_Opcodes + 1555  ,
  /* 6a */ g_Opcodes + 1556  ,
  /* 6b */ g_Opcodes + 1557  ,
  /* 6c */ g_Opcodes + 1558  ,
  /* 6d */ g_Opcodes + 1559  ,
  /* 6e */ g_Opcodes + 1560  ,
  /* 6f */ g_Opcodes + 1561  ,
  /* 70 */ g_Opcodes + 1562  ,
  /* 71 */ g_Opcodes + 1563  ,
  /* 72 */ g_Opcodes + 1564  ,
  /* 73 */ g_Opcodes + 1565  ,
  /* 74 */ g_Opcodes + 1566  ,
  /* 75 */ g_Opcodes + 1567  ,
  /* 76 */ g_Opcodes + 1568  ,
  /* 77 */ g_Opcodes + 1569  ,
  /* 78 */ g_Opcodes + 1570  ,
  /* 79 */ g_Opcodes + 1571  ,
  /* 7a */ g_Opcodes + 1572  ,
  /* 7b */ g_Opcodes + 1573  ,
  /* 7c */ g_Opcodes + 1574  ,
  /* 7d */ g_Opcodes + 1575  ,
  /* 7e */ g_Opcodes + 1576  ,
  /* 7f */ g_Opcodes + 1577  ,
  /* 80 */ g_Opcodes + 1578  ,
  /* 81 */ g_Opcodes + 1579  ,
  /* 82 */ g_Opcodes + 1580  ,
  /* 83 */ g_Opcodes + 1581  ,
  /* 84 */ g_Opcodes + 1582  ,
  /* 85 */ g_Opcodes + 1583  ,
  /* 86 */ g_Opcodes + 1584  ,
  /* 87 */ g_Opcodes + 1585  ,
  /* 88 */ g_Opcodes + 1586  ,
  /* 89 */ g_Opcodes + 1587  ,
  /* 8a */ g_Opcodes + 1588  ,
  /* 8b */ g_Opcodes + 1589  ,
  /* 8c */ g_Opcodes + 1590  ,
  /* 8d */ g_Opcodes + 1591  ,
  /* 8e */ g_Opcodes + 1592  ,
  /* 8f */ g_Opcodes + 1593  ,
  /* 90 */ g_Opcodes + 1594  ,
  /* 91 */ g_Opcodes + 1595  ,
  /* 92 */ g_Opcodes + 1596  ,
  /* 93 */ g_Opcodes + 1597  ,
  /* 94 */ g_Opcodes + 1598  ,
  /* 95 */ g_Opcodes + 1599  ,
  /* 96 */ g_Opcodes + 1600  ,
  /* 97 */ g_Opcodes + 1601  ,
  /* 98 */ g_Opcodes + 1602  ,
  /* 99 */ g_Opcodes + 1603  ,
  /* 9a */ g_Opcodes + 1604  ,
  /* 9b */ g_Opcodes + 1605  ,
  /* 9c */ g_Opcodes + 1606  ,
  /* 9d */ g_Opcodes + 1607  ,
  /* 9e */ g_Opcodes + 1608  ,
  /* 9f */ g_Opcodes + 1609  ,
  /* a0 */ g_Opcodes + 1610  ,
  /* a1 */ g_Opcodes + 1611  ,
  /* a2 */ g_Opcodes + 1612  ,
  /* a3 */ g_Opcodes + 1613  ,
  /* a4 */ g_Opcodes + 1614  ,
  /* a5 */ g_Opcodes + 1615  ,
  /* a6 */ g_Opcodes + 1616  ,
  /* a7 */ g_Opcodes + 1617  ,
  /* a8 */ g_Opcodes + 1618  ,
  /* a9 */ g_Opcodes + 1619  ,
  /* aa */ g_Opcodes + 1620  ,
  /* ab */ g_Opcodes + 1621  ,
  /* ac */ g_Opcodes + 1622  ,
  /* ad */ g_Opcodes + 1623  ,
  /* ae */ g_Opcodes + 1624  ,
  /* af */ g_Opcodes + 1625  ,
  /* b0 */ g_Opcodes + 1626  ,
  /* b1 */ g_Opcodes + 1627  ,
  /* b2 */ g_Opcodes + 1628  ,
  /* b3 */ g_Opcodes + 1629  ,
  /* b4 */ g_Opcodes + 1630  ,
  /* b5 */ g_Opcodes + 1631  ,
  /* b6 */ g_Opcodes + 1632  ,
  /* b7 */ g_Opcodes + 1633  ,
  /* b8 */ g_Opcodes + 1634  ,
  /* b9 */ g_Opcodes + 1635  ,
  /* ba */ g_Opcodes + 1636  ,
  /* bb */ g_Opcodes + 1637  ,
  /* bc */ g_Opcodes + 1638  ,
  /* bd */ g_Opcodes + 1639  ,
  /* be */ g_Opcodes + 1640  ,
  /* bf */ g_Opcodes + 1641  ,
  /* c0 */ g_Opcodes + 1642  ,
  /* c1 */ g_Opcodes + 1643  ,
  /* c2 */ g_Opcodes + 1644  ,
  /* c3 */ g_Opcodes + 1645  ,
  /* c4 */ g_Opcodes + 1646  ,
  /* c5 */ g_Opcodes + 1647  ,
  /* c6 */ g_Opcodes + 1648  ,
  /* c7 */ g_Opcodes + 1649  ,
  /* c8 */ g_Opcodes + 1650  ,
  /* c9 */ g_Opcodes + 1651  ,
  /* ca */ g_Opcodes + 1652  ,
  /* cb */ g_Opcodes + 1653  ,
  /* cc */ g_Opcodes + 1654  ,
  /* cd */ g_Opcodes + 1655  ,
  /* ce */ g_Opcodes + 1656  ,
  /* cf */ g_Opcodes + 1657  ,
  /* d0 */ g_Opcodes + 1658  ,
  /* d1 */ g_Opcodes + 1659  ,
  /* d2 */ g_Opcodes + 1660  ,
  /* d3 */ g_Opcodes + 1661  ,
  /* d4 */ g_Opcodes + 1662  ,
  /* d5 */ g_Opcodes + 1663  ,
  /* d6 */ g_Opcodes + 1664  ,
  /* d7 */ g_Opcodes + 1665  ,
  /* d8 */ g_Opcodes + 1666  ,
  /* d9 */ g_Opcodes + 1667  ,
  /* da */ g_Opcodes + 1668  ,
  /* db */ g_Opcodes + 1669  ,
  /* dc */ g_Opcodes + 1670  ,
  /* dd */ g_Opcodes + 1671  ,
  /* de */ g_Opcodes + 1672  ,
  /* df */ g_Opcodes + 1673  ,
  /* e0 */ g_Opcodes + 1674  ,
  /* e1 */ g_Opcodes + 1675  ,
  /* e2 */ g_Opcodes + 1676  ,
  /* e3 */ g_Opcodes + 1677  ,
  /* e4 */ g_Opcodes + 1678  ,
  /* e5 */ g_Opcodes + 1679  ,
  /* e6 */ g_Opcodes + 1680  ,
  /* e7 */ g_Opcodes + 1681  ,
  /* e8 */ g_Opcodes + 1682  ,
  /* e9 */ g_Opcodes + 1683  ,
  /* ea */ g_Opcodes + 1684  ,
  /* eb */ g_Opcodes + 1685  ,
  /* ec */ g_Opcodes + 1686  ,
  /* ed */ g_Opcodes + 1687  ,
  /* ee */ g_Opcodes + 1688  ,
  /* ef */ g_Opcodes + 1689  ,
  /* f0 */ NULL  ,
  /* f1 */ NULL  ,
  /* f2 */ g_Opcodes + 1690  ,
  /* f3 */ g_Opcodes + 1691  ,
  /* f4 */ g_Opcodes + 1692  ,
  /* f5 */ g_Opcodes + 1693  ,
  /* f6 */ g_Opcodes + 1694  ,
  /* f7 */ g_Opcodes + 1695  ,
  /* f8 */ g_Opcodes + 1696  ,
  /* f9 */ g_Opcodes + 1697  ,
  /* fa */ g_Opcodes + 1698  ,
  /* fb */ g_Opcodes + 1699  ,
  /* fc */ g_Opcodes + 1700  ,
  /* fd */ g_Opcodes + 1701  ,
  /* fe */ g_Opcodes + 1702  ,
  /* ff */ g_Opcodes + 1703  ,
},
/* PrefixF20F38 */
{
  /* 00 */ NULL  ,
  /* 01 */ NULL  ,
  /* 02 */ NULL  ,
  /* 03 */ NULL  ,
  /* 04 */ NULL  ,
  /* 05 */ NULL  ,
  /* 06 */ NULL  ,
  /* 07 */ NULL  ,
  /* 08 */ NULL  ,
  /* 09 */ NULL  ,
  /* 0a */ NULL  ,
  /* 0b */ NULL  ,
  /* 0c */ NULL  ,
  /* 0d */ NULL  ,
  /* 0e */ NULL  ,
  /* 0f */ NULL  ,
  /* 10 */ NULL  ,
  /* 11 */ NULL  ,
  /* 12 */ NULL  ,
  /* 13 */ NULL  ,
  /* 14 */ NULL  ,
  /* 15 */ NULL  ,
  /* 16 */ NULL  ,
  /* 17 */ NULL  ,
  /* 18 */ NULL  ,
  /* 19 */ NULL  ,
  /* 1a */ NULL  ,
  /* 1b */ NULL  ,
  /* 1c */ NULL  ,
  /* 1d */ NULL  ,
  /* 1e */ NULL  ,
  /* 1f */ NULL  ,
  /* 20 */ NULL  ,
  /* 21 */ NULL  ,
  /* 22 */ NULL  ,
  /* 23 */ NULL  ,
  /* 24 */ NULL  ,
  /* 25 */ NULL  ,
  /* 26 */ NULL  ,
  /* 27 */ NULL  ,
  /* 28 */ NULL  ,
  /* 29 */ NULL  ,
  /* 2a */ NULL  ,
  /* 2b */ NULL  ,
  /* 2c */ NULL  ,
  /* 2d */ NULL  ,
  /* 2e */ NULL  ,
  /* 2f */ NULL  ,
  /* 30 */ NULL  ,
  /* 31 */ NULL  ,
  /* 32 */ NULL  ,
  /* 33 */ NULL  ,
  /* 34 */ NULL  ,
  /* 35 */ NULL  ,
  /* 36 */ NULL  ,
  /* 37 */ NULL  ,
  /* 38 */ NULL  ,
  /* 39 */ NULL  ,
  /* 3a */ NULL  ,
  /* 3b */ NULL  ,
  /* 3c */ NULL  ,
  /* 3d */ NULL  ,
  /* 3e */ NULL  ,
  /* 3f */ NULL  ,
  /* 40 */ NULL  ,
  /* 41 */ NULL  ,
  /* 42 */ NULL  ,
  /* 43 */ NULL  ,
  /* 44 */ NULL  ,
  /* 45 */ NULL  ,
  /* 46 */ NULL  ,
  /* 47 */ NULL  ,
  /* 48 */ NULL  ,
  /* 49 */ NULL  ,
  /* 4a */ NULL  ,
  /* 4b */ NULL  ,
  /* 4c */ NULL  ,
  /* 4d */ NULL  ,
  /* 4e */ NULL  ,
  /* 4f */ NULL  ,
  /* 50 */ NULL  ,
  /* 51 */ NULL  ,
  /* 52 */ NULL  ,
  /* 53 */ NULL  ,
  /* 54 */ NULL  ,
  /* 55 */ NULL  ,
  /* 56 */ NULL  ,
  /* 57 */ NULL  ,
  /* 58 */ NULL  ,
  /* 59 */ NULL  ,
  /* 5a */ NULL  ,
  /* 5b */ NULL  ,
  /* 5c */ NULL  ,
  /* 5d */ NULL  ,
  /* 5e */ NULL  ,
  /* 5f */ NULL  ,
  /* 60 */ NULL  ,
  /* 61 */ NULL  ,
  /* 62 */ NULL  ,
  /* 63 */ NULL  ,
  /* 64 */ NULL  ,
  /* 65 */ NULL  ,
  /* 66 */ NULL  ,
  /* 67 */ NULL  ,
  /* 68 */ NULL  ,
  /* 69 */ NULL  ,
  /* 6a */ NULL  ,
  /* 6b */ NULL  ,
  /* 6c */ NULL  ,
  /* 6d */ NULL  ,
  /* 6e */ NULL  ,
  /* 6f */ NULL  ,
  /* 70 */ NULL  ,
  /* 71 */ NULL  ,
  /* 72 */ NULL  ,
  /* 73 */ NULL  ,
  /* 74 */ NULL  ,
  /* 75 */ NULL  ,
  /* 76 */ NULL  ,
  /* 77 */ NULL  ,
  /* 78 */ NULL  ,
  /* 79 */ NULL  ,
  /* 7a */ NULL  ,
  /* 7b */ NULL  ,
  /* 7c */ NULL  ,
  /* 7d */ NULL  ,
  /* 7e */ NULL  ,
  /* 7f */ NULL  ,
  /* 80 */ NULL  ,
  /* 81 */ NULL  ,
  /* 82 */ NULL  ,
  /* 83 */ NULL  ,
  /* 84 */ NULL  ,
  /* 85 */ NULL  ,
  /* 86 */ NULL  ,
  /* 87 */ NULL  ,
  /* 88 */ NULL  ,
  /* 89 */ NULL  ,
  /* 8a */ NULL  ,
  /* 8b */ NULL  ,
  /* 8c */ NULL  ,
  /* 8d */ NULL  ,
  /* 8e */ NULL  ,
  /* 8f */ NULL  ,
  /* 90 */ NULL  ,
  /* 91 */ NULL  ,
  /* 92 */ NULL  ,
  /* 93 */ NULL  ,
  /* 94 */ NULL  ,
  /* 95 */ NULL  ,
  /* 96 */ NULL  ,
  /* 97 */ NULL  ,
  /* 98 */ NULL  ,
  /* 99 */ NULL  ,
  /* 9a */ NULL  ,
  /* 9b */ NULL  ,
  /* 9c */ NULL  ,
  /* 9d */ NULL  ,
  /* 9e */ NULL  ,
  /* 9f */ NULL  ,
  /* a0 */ NULL  ,
  /* a1 */ NULL  ,
  /* a2 */ NULL  ,
  /* a3 */ NULL  ,
  /* a4 */ NULL  ,
  /* a5 */ NULL  ,
  /* a6 */ NULL  ,
  /* a7 */ NULL  ,
  /* a8 */ NULL  ,
  /* a9 */ NULL  ,
  /* aa */ NULL  ,
  /* ab */ NULL  ,
  /* ac */ NULL  ,
  /* ad */ NULL  ,
  /* ae */ NULL  ,
  /* af */ NULL  ,
  /* b0 */ NULL  ,
  /* b1 */ NULL  ,
  /* b2 */ NULL  ,
  /* b3 */ NULL  ,
  /* b4 */ NULL  ,
  /* b5 */ NULL  ,
  /* b6 */ NULL  ,
  /* b7 */ NULL  ,
  /* b8 */ NULL  ,
  /* b9 */ NULL  ,
  /* ba */ NULL  ,
  /* bb */ NULL  ,
  /* bc */ NULL  ,
  /* bd */ NULL  ,
  /* be */ NULL  ,
  /* bf */ NULL  ,
  /* c0 */ NULL  ,
  /* c1 */ NULL  ,
  /* c2 */ NULL  ,
  /* c3 */ NULL  ,
  /* c4 */ NULL  ,
  /* c5 */ NULL  ,
  /* c6 */ NULL  ,
  /* c7 */ NULL  ,
  /* c8 */ NULL  ,
  /* c9 */ NULL  ,
  /* ca */ NULL  ,
  /* cb */ NULL  ,
  /* cc */ NULL  ,
  /* cd */ NULL  ,
  /* ce */ NULL  ,
  /* cf */ NULL  ,
  /* d0 */ NULL  ,
  /* d1 */ NULL  ,
  /* d2 */ NULL  ,
  /* d3 */ NULL  ,
  /* d4 */ NULL  ,
  /* d5 */ NULL  ,
  /* d6 */ NULL  ,
  /* d7 */ NULL  ,
  /* d8 */ NULL  ,
  /* d9 */ NULL  ,
  /* da */ NULL  ,
  /* db */ NULL  ,
  /* dc */ NULL  ,
  /* dd */ NULL  ,
  /* de */ NULL  ,
  /* df */ NULL  ,
  /* e0 */ NULL  ,
  /* e1 */ NULL  ,
  /* e2 */ NULL  ,
  /* e3 */ NULL  ,
  /* e4 */ NULL  ,
  /* e5 */ NULL  ,
  /* e6 */ NULL  ,
  /* e7 */ NULL  ,
  /* e8 */ NULL  ,
  /* e9 */ NULL  ,
  /* ea */ NULL  ,
  /* eb */ NULL  ,
  /* ec */ NULL  ,
  /* ed */ NULL  ,
  /* ee */ NULL  ,
  /* ef */ NULL  ,
  /* f0 */ g_Opcodes + 1704  ,
  /* f1 */ g_Opcodes + 1705  ,
  /* f2 */ NULL  ,
  /* f3 */ NULL  ,
  /* f4 */ NULL  ,
  /* f5 */ NULL  ,
  /* f6 */ NULL  ,
  /* f7 */ NULL  ,
  /* f8 */ NULL  ,
  /* f9 */ NULL  ,
  /* fa */ NULL  ,
  /* fb */ NULL  ,
  /* fc */ NULL  ,
  /* fd */ NULL  ,
  /* fe */ NULL  ,
  /* ff */ NULL  ,
},
/* Prefix0F3A */
{
  /* 00 */ NULL  ,
  /* 01 */ NULL  ,
  /* 02 */ NULL  ,
  /* 03 */ NULL  ,
  /* 04 */ NULL  ,
  /* 05 */ NULL  ,
  /* 06 */ NULL  ,
  /* 07 */ NULL  ,
  /* 08 */ NULL  ,
  /* 09 */ NULL  ,
  /* 0a */ NULL  ,
  /* 0b */ NULL  ,
  /* 0c */ NULL  ,
  /* 0d */ NULL  ,
  /* 0e */ NULL  ,
  /* 0f */ g_Opcodes + 1706  ,
  /* 10 */ NULL  ,
  /* 11 */ NULL  ,
  /* 12 */ NULL  ,
  /* 13 */ NULL  ,
  /* 14 */ NULL  ,
  /* 15 */ NULL  ,
  /* 16 */ NULL  ,
  /* 17 */ NULL  ,
  /* 18 */ NULL  ,
  /* 19 */ NULL  ,
  /* 1a */ NULL  ,
  /* 1b */ NULL  ,
  /* 1c */ NULL  ,
  /* 1d */ NULL  ,
  /* 1e */ NULL  ,
  /* 1f */ NULL  ,
  /* 20 */ NULL  ,
  /* 21 */ NULL  ,
  /* 22 */ NULL  ,
  /* 23 */ NULL  ,
  /* 24 */ NULL  ,
  /* 25 */ NULL  ,
  /* 26 */ NULL  ,
  /* 27 */ NULL  ,
  /* 28 */ NULL  ,
  /* 29 */ NULL  ,
  /* 2a */ NULL  ,
  /* 2b */ NULL  ,
  /* 2c */ NULL  ,
  /* 2d */ NULL  ,
  /* 2e */ NULL  ,
  /* 2f */ NULL  ,
  /* 30 */ NULL  ,
  /* 31 */ NULL  ,
  /* 32 */ NULL  ,
  /* 33 */ NULL  ,
  /* 34 */ NULL  ,
  /* 35 */ NULL  ,
  /* 36 */ NULL  ,
  /* 37 */ NULL  ,
  /* 38 */ NULL  ,
  /* 39 */ NULL  ,
  /* 3a */ NULL  ,
  /* 3b */ NULL  ,
  /* 3c */ NULL  ,
  /* 3d */ NULL  ,
  /* 3e */ NULL  ,
  /* 3f */ NULL  ,
  /* 40 */ NULL  ,
  /* 41 */ NULL  ,
  /* 42 */ NULL  ,
  /* 43 */ NULL  ,
  /* 44 */ NULL  ,
  /* 45 */ NULL  ,
  /* 46 */ NULL  ,
  /* 47 */ NULL  ,
  /* 48 */ NULL  ,
  /* 49 */ NULL  ,
  /* 4a */ NULL  ,
  /* 4b */ NULL  ,
  /* 4c */ NULL  ,
  /* 4d */ NULL  ,
  /* 4e */ NULL  ,
  /* 4f */ NULL  ,
  /* 50 */ NULL  ,
  /* 51 */ NULL  ,
  /* 52 */ NULL  ,
  /* 53 */ NULL  ,
  /* 54 */ NULL  ,
  /* 55 */ NULL  ,
  /* 56 */ NULL  ,
  /* 57 */ NULL  ,
  /* 58 */ NULL  ,
  /* 59 */ NULL  ,
  /* 5a */ NULL  ,
  /* 5b */ NULL  ,
  /* 5c */ NULL  ,
  /* 5d */ NULL  ,
  /* 5e */ NULL  ,
  /* 5f */ NULL  ,
  /* 60 */ NULL  ,
  /* 61 */ NULL  ,
  /* 62 */ NULL  ,
  /* 63 */ NULL  ,
  /* 64 */ NULL  ,
  /* 65 */ NULL  ,
  /* 66 */ NULL  ,
  /* 67 */ NULL  ,
  /* 68 */ NULL  ,
  /* 69 */ NULL  ,
  /* 6a */ NULL  ,
  /* 6b */ NULL  ,
  /* 6c */ NULL  ,
  /* 6d */ NULL  ,
  /* 6e */ NULL  ,
  /* 6f */ NULL  ,
  /* 70 */ NULL  ,
  /* 71 */ NULL  ,
  /* 72 */ NULL  ,
  /* 73 */ NULL  ,
  /* 74 */ NULL  ,
  /* 75 */ NULL  ,
  /* 76 */ NULL  ,
  /* 77 */ NULL  ,
  /* 78 */ NULL  ,
  /* 79 */ NULL  ,
  /* 7a */ NULL  ,
  /* 7b */ NULL  ,
  /* 7c */ NULL  ,
  /* 7d */ NULL  ,
  /* 7e */ NULL  ,
  /* 7f */ NULL  ,
  /* 80 */ NULL  ,
  /* 81 */ NULL  ,
  /* 82 */ NULL  ,
  /* 83 */ NULL  ,
  /* 84 */ NULL  ,
  /* 85 */ NULL  ,
  /* 86 */ NULL  ,
  /* 87 */ NULL  ,
  /* 88 */ NULL  ,
  /* 89 */ NULL  ,
  /* 8a */ NULL  ,
  /* 8b */ NULL  ,
  /* 8c */ NULL  ,
  /* 8d */ NULL  ,
  /* 8e */ NULL  ,
  /* 8f */ NULL  ,
  /* 90 */ NULL  ,
  /* 91 */ NULL  ,
  /* 92 */ NULL  ,
  /* 93 */ NULL  ,
  /* 94 */ NULL  ,
  /* 95 */ NULL  ,
  /* 96 */ NULL  ,
  /* 97 */ NULL  ,
  /* 98 */ NULL  ,
  /* 99 */ NULL  ,
  /* 9a */ NULL  ,
  /* 9b */ NULL  ,
  /* 9c */ NULL  ,
  /* 9d */ NULL  ,
  /* 9e */ NULL  ,
  /* 9f */ NULL  ,
  /* a0 */ NULL  ,
  /* a1 */ NULL  ,
  /* a2 */ NULL  ,
  /* a3 */ NULL  ,
  /* a4 */ NULL  ,
  /* a5 */ NULL  ,
  /* a6 */ NULL  ,
  /* a7 */ NULL  ,
  /* a8 */ NULL  ,
  /* a9 */ NULL  ,
  /* aa */ NULL  ,
  /* ab */ NULL  ,
  /* ac */ NULL  ,
  /* ad */ NULL  ,
  /* ae */ NULL  ,
  /* af */ NULL  ,
  /* b0 */ NULL  ,
  /* b1 */ NULL  ,
  /* b2 */ NULL  ,
  /* b3 */ NULL  ,
  /* b4 */ NULL  ,
  /* b5 */ NULL  ,
  /* b6 */ NULL  ,
  /* b7 */ NULL  ,
  /* b8 */ NULL  ,
  /* b9 */ NULL  ,
  /* ba */ NULL  ,
  /* bb */ NULL  ,
  /* bc */ NULL  ,
  /* bd */ NULL  ,
  /* be */ NULL  ,
  /* bf */ NULL  ,
  /* c0 */ NULL  ,
  /* c1 */ NULL  ,
  /* c2 */ NULL  ,
  /* c3 */ NULL  ,
  /* c4 */ NULL  ,
  /* c5 */ NULL  ,
  /* c6 */ NULL  ,
  /* c7 */ NULL  ,
  /* c8 */ NULL  ,
  /* c9 */ NULL  ,
  /* ca */ NULL  ,
  /* cb */ NULL  ,
  /* cc */ NULL  ,
  /* cd */ NULL  ,
  /* ce */ NULL  ,
  /* cf */ NULL  ,
  /* d0 */ NULL  ,
  /* d1 */ NULL  ,
  /* d2 */ NULL  ,
  /* d3 */ NULL  ,
  /* d4 */ NULL  ,
  /* d5 */ NULL  ,
  /* d6 */ NULL  ,
  /* d7 */ NULL  ,
  /* d8 */ NULL  ,
  /* d9 */ NULL  ,
  /* da */ NULL  ,
  /* db */ NULL  ,
  /* dc */ NULL  ,
  /* dd */ NULL  ,
  /* de */ NULL  ,
  /* df */ NULL  ,
  /* e0 */ NULL  ,
  /* e1 */ NULL  ,
  /* e2 */ NULL  ,
  /* e3 */ NULL  ,
  /* e4 */ NULL  ,
  /* e5 */ NULL  ,
  /* e6 */ NULL  ,
  /* e7 */ NULL  ,
  /* e8 */ NULL  ,
  /* e9 */ NULL  ,
  /* ea */ NULL  ,
  /* eb */ NULL  ,
  /* ec */ NULL  ,
  /* ed */ NULL  ,
  /* ee */ NULL  ,
  /* ef */ NULL  ,
  /* f0 */ NULL  ,
  /* f1 */ NULL  ,
  /* f2 */ NULL  ,
  /* f3 */ NULL  ,
  /* f4 */ NULL  ,
  /* f5 */ NULL  ,
  /* f6 */ NULL  ,
  /* f7 */ NULL  ,
  /* f8 */ NULL  ,
  /* f9 */ NULL  ,
  /* fa */ NULL  ,
  /* fb */ NULL  ,
  /* fc */ NULL  ,
  /* fd */ NULL  ,
  /* fe */ NULL  ,
  /* ff */ NULL  ,
},
/* Prefix660F3A */
{
  /* 00 */ NULL  ,
  /* 01 */ NULL  ,
  /* 02 */ NULL  ,
  /* 03 */ NULL  ,
  /* 04 */ NULL  ,
  /* 05 */ NULL  ,
  /* 06 */ NULL  ,
  /* 07 */ NULL  ,
  /* 08 */ g_Opcodes + 1707  ,
  /* 09 */ g_Opcodes + 1708  ,
  /* 0a */ g_Opcodes + 1709  ,
  /* 0b */ g_Opcodes + 1710  ,
  /* 0c */ g_Opcodes + 1711  ,
  /* 0d */ g_Opcodes + 1712  ,
  /* 0e */ g_Opcodes + 1713  ,
  /* 0f */ g_Opcodes + 1714  ,
  /* 10 */ NULL  ,
  /* 11 */ NULL  ,
  /* 12 */ NULL  ,
  /* 13 */ NULL  ,
  /* 14 */ g_Opcodes + 1715  ,
  /* 15 */ g_Opcodes + 1716  ,
  /* 16 */ g_Opcodes + 1717  ,
  /* 17 */ g_Opcodes + 1719  ,
  /* 18 */ NULL  ,
  /* 19 */ NULL  ,
  /* 1a */ NULL  ,
  /* 1b */ NULL  ,
  /* 1c */ NULL  ,
  /* 1d */ NULL  ,
  /* 1e */ NULL  ,
  /* 1f */ NULL  ,
  /* 20 */ g_Opcodes + 1720  ,
  /* 21 */ g_Opcodes + 1721  ,
  /* 22 */ g_Opcodes + 1722  ,
  /* 23 */ NULL  ,
  /* 24 */ NULL  ,
  /* 25 */ NULL  ,
  /* 26 */ NULL  ,
  /* 27 */ NULL  ,
  /* 28 */ NULL  ,
  /* 29 */ NULL  ,
  /* 2a */ NULL  ,
  /* 2b */ NULL  ,
  /* 2c */ NULL  ,
  /* 2d */ NULL  ,
  /* 2e */ NULL  ,
  /* 2f */ NULL  ,
  /* 30 */ NULL  ,
  /* 31 */ NULL  ,
  /* 32 */ NULL  ,
  /* 33 */ NULL  ,
  /* 34 */ NULL  ,
  /* 35 */ NULL  ,
  /* 36 */ NULL  ,
  /* 37 */ NULL  ,
  /* 38 */ NULL  ,
  /* 39 */ NULL  ,
  /* 3a */ NULL  ,
  /* 3b */ NULL  ,
  /* 3c */ NULL  ,
  /* 3d */ NULL  ,
  /* 3e */ NULL  ,
  /* 3f */ NULL  ,
  /* 40 */ g_Opcodes + 1724  ,
  /* 41 */ g_Opcodes + 1725  ,
  /* 42 */ g_Opcodes + 1726  ,
  /* 43 */ NULL  ,
  /* 44 */ NULL  ,
  /* 45 */ NULL  ,
  /* 46 */ NULL  ,
  /* 47 */ NULL  ,
  /* 48 */ NULL  ,
  /* 49 */ NULL  ,
  /* 4a */ NULL  ,
  /* 4b */ NULL  ,
  /* 4c */ NULL  ,
  /* 4d */ NULL  ,
  /* 4e */ NULL  ,
  /* 4f */ NULL  ,
  /* 50 */ NULL  ,
  /* 51 */ NULL  ,
  /* 52 */ NULL  ,
  /* 53 */ NULL  ,
  /* 54 */ NULL  ,
  /* 55 */ NULL  ,
  /* 56 */ NULL  ,
  /* 57 */ NULL  ,
  /* 58 */ NULL  ,
  /* 59 */ NULL  ,
  /* 5a */ NULL  ,
  /* 5b */ NULL  ,
  /* 5c */ NULL  ,
  /* 5d */ NULL  ,
  /* 5e */ NULL  ,
  /* 5f */ NULL  ,
  /* 60 */ g_Opcodes + 1727  ,
  /* 61 */ g_Opcodes + 1728  ,
  /* 62 */ g_Opcodes + 1729  ,
  /* 63 */ g_Opcodes + 1730  ,
  /* 64 */ NULL  ,
  /* 65 */ NULL  ,
  /* 66 */ NULL  ,
  /* 67 */ NULL  ,
  /* 68 */ NULL  ,
  /* 69 */ NULL  ,
  /* 6a */ NULL  ,
  /* 6b */ NULL  ,
  /* 6c */ NULL  ,
  /* 6d */ NULL  ,
  /* 6e */ NULL  ,
  /* 6f */ NULL  ,
  /* 70 */ NULL  ,
  /* 71 */ NULL  ,
  /* 72 */ NULL  ,
  /* 73 */ NULL  ,
  /* 74 */ NULL  ,
  /* 75 */ NULL  ,
  /* 76 */ NULL  ,
  /* 77 */ NULL  ,
  /* 78 */ NULL  ,
  /* 79 */ NULL  ,
  /* 7a */ NULL  ,
  /* 7b */ NULL  ,
  /* 7c */ NULL  ,
  /* 7d */ NULL  ,
  /* 7e */ NULL  ,
  /* 7f */ NULL  ,
  /* 80 */ NULL  ,
  /* 81 */ NULL  ,
  /* 82 */ NULL  ,
  /* 83 */ NULL  ,
  /* 84 */ NULL  ,
  /* 85 */ NULL  ,
  /* 86 */ NULL  ,
  /* 87 */ NULL  ,
  /* 88 */ NULL  ,
  /* 89 */ NULL  ,
  /* 8a */ NULL  ,
  /* 8b */ NULL  ,
  /* 8c */ NULL  ,
  /* 8d */ NULL  ,
  /* 8e */ NULL  ,
  /* 8f */ NULL  ,
  /* 90 */ NULL  ,
  /* 91 */ NULL  ,
  /* 92 */ NULL  ,
  /* 93 */ NULL  ,
  /* 94 */ NULL  ,
  /* 95 */ NULL  ,
  /* 96 */ NULL  ,
  /* 97 */ NULL  ,
  /* 98 */ NULL  ,
  /* 99 */ NULL  ,
  /* 9a */ NULL  ,
  /* 9b */ NULL  ,
  /* 9c */ NULL  ,
  /* 9d */ NULL  ,
  /* 9e */ NULL  ,
  /* 9f */ NULL  ,
  /* a0 */ NULL  ,
  /* a1 */ NULL  ,
  /* a2 */ NULL  ,
  /* a3 */ NULL  ,
  /* a4 */ NULL  ,
  /* a5 */ NULL  ,
  /* a6 */ NULL  ,
  /* a7 */ NULL  ,
  /* a8 */ NULL  ,
  /* a9 */ NULL  ,
  /* aa */ NULL  ,
  /* ab */ NULL  ,
  /* ac */ NULL  ,
  /* ad */ NULL  ,
  /* ae */ NULL  ,
  /* af */ NULL  ,
  /* b0 */ NULL  ,
  /* b1 */ NULL  ,
  /* b2 */ NULL  ,
  /* b3 */ NULL  ,
  /* b4 */ NULL  ,
  /* b5 */ NULL  ,
  /* b6 */ NULL  ,
  /* b7 */ NULL  ,
  /* b8 */ NULL  ,
  /* b9 */ NULL  ,
  /* ba */ NULL  ,
  /* bb */ NULL  ,
  /* bc */ NULL  ,
  /* bd */ NULL  ,
  /* be */ NULL  ,
  /* bf */ NULL  ,
  /* c0 */ NULL  ,
  /* c1 */ NULL  ,
  /* c2 */ NULL  ,
  /* c3 */ NULL  ,
  /* c4 */ NULL  ,
  /* c5 */ NULL  ,
  /* c6 */ NULL  ,
  /* c7 */ NULL  ,
  /* c8 */ NULL  ,
  /* c9 */ NULL  ,
  /* ca */ NULL  ,
  /* cb */ NULL  ,
  /* cc */ NULL  ,
  /* cd */ NULL  ,
  /* ce */ NULL  ,
  /* cf */ NULL  ,
  /* d0 */ NULL  ,
  /* d1 */ NULL  ,
  /* d2 */ NULL  ,
  /* d3 */ NULL  ,
  /* d4 */ NULL  ,
  /* d5 */ NULL  ,
  /* d6 */ NULL  ,
  /* d7 */ NULL  ,
  /* d8 */ NULL  ,
  /* d9 */ NULL  ,
  /* da */ NULL  ,
  /* db */ NULL  ,
  /* dc */ NULL  ,
  /* dd */ NULL  ,
  /* de */ NULL  ,
  /* df */ NULL  ,
  /* e0 */ NULL  ,
  /* e1 */ NULL  ,
  /* e2 */ NULL  ,
  /* e3 */ NULL  ,
  /* e4 */ NULL  ,
  /* e5 */ NULL  ,
  /* e6 */ NULL  ,
  /* e7 */ NULL  ,
  /* e8 */ NULL  ,
  /* e9 */ NULL  ,
  /* ea */ NULL  ,
  /* eb */ NULL  ,
  /* ec */ NULL  ,
  /* ed */ NULL  ,
  /* ee */ NULL  ,
  /* ef */ NULL  ,
  /* f0 */ NULL  ,
  /* f1 */ NULL  ,
  /* f2 */ NULL  ,
  /* f3 */ NULL  ,
  /* f4 */ NULL  ,
  /* f5 */ NULL  ,
  /* f6 */ NULL  ,
  /* f7 */ NULL  ,
  /* f8 */ NULL  ,
  /* f9 */ NULL  ,
  /* fa */ NULL  ,
  /* fb */ NULL  ,
  /* fc */ NULL  ,
  /* fd */ NULL  ,
  /* fe */ NULL  ,
  /* ff */ NULL  ,
},
/* PrefixD8 */
{
  /* 00 */ NULL  ,
  /* 01 */ NULL  ,
  /* 02 */ NULL  ,
  /* 03 */ NULL  ,
  /* 04 */ NULL  ,
  /* 05 */ NULL  ,
  /* 06 */ NULL  ,
  /* 07 */ NULL  ,
  /* 08 */ NULL  ,
  /* 09 */ NULL  ,
  /* 0a */ NULL  ,
  /* 0b */ NULL  ,
  /* 0c */ NULL  ,
  /* 0d */ NULL  ,
  /* 0e */ NULL  ,
  /* 0f */ NULL  ,
  /* 10 */ NULL  ,
  /* 11 */ NULL  ,
  /* 12 */ NULL  ,
  /* 13 */ NULL  ,
  /* 14 */ NULL  ,
  /* 15 */ NULL  ,
  /* 16 */ NULL  ,
  /* 17 */ NULL  ,
  /* 18 */ NULL  ,
  /* 19 */ NULL  ,
  /* 1a */ NULL  ,
  /* 1b */ NULL  ,
  /* 1c */ NULL  ,
  /* 1d */ NULL  ,
  /* 1e */ NULL  ,
  /* 1f */ NULL  ,
  /* 20 */ NULL  ,
  /* 21 */ NULL  ,
  /* 22 */ NULL  ,
  /* 23 */ NULL  ,
  /* 24 */ NULL  ,
  /* 25 */ NULL  ,
  /* 26 */ NULL  ,
  /* 27 */ NULL  ,
  /* 28 */ NULL  ,
  /* 29 */ NULL  ,
  /* 2a */ NULL  ,
  /* 2b */ NULL  ,
  /* 2c */ NULL  ,
  /* 2d */ NULL  ,
  /* 2e */ NULL  ,
  /* 2f */ NULL  ,
  /* 30 */ NULL  ,
  /* 31 */ NULL  ,
  /* 32 */ NULL  ,
  /* 33 */ NULL  ,
  /* 34 */ NULL  ,
  /* 35 */ NULL  ,
  /* 36 */ NULL  ,
  /* 37 */ NULL  ,
  /* 38 */ NULL  ,
  /* 39 */ NULL  ,
  /* 3a */ NULL  ,
  /* 3b */ NULL  ,
  /* 3c */ NULL  ,
  /* 3d */ NULL  ,
  /* 3e */ NULL  ,
  /* 3f */ NULL  ,
  /* 40 */ NULL  ,
  /* 41 */ NULL  ,
  /* 42 */ NULL  ,
  /* 43 */ NULL  ,
  /* 44 */ NULL  ,
  /* 45 */ NULL  ,
  /* 46 */ NULL  ,
  /* 47 */ NULL  ,
  /* 48 */ NULL  ,
  /* 49 */ NULL  ,
  /* 4a */ NULL  ,
  /* 4b */ NULL  ,
  /* 4c */ NULL  ,
  /* 4d */ NULL  ,
  /* 4e */ NULL  ,
  /* 4f */ NULL  ,
  /* 50 */ NULL  ,
  /* 51 */ NULL  ,
  /* 52 */ NULL  ,
  /* 53 */ NULL  ,
  /* 54 */ NULL  ,
  /* 55 */ NULL  ,
  /* 56 */ NULL  ,
  /* 57 */ NULL  ,
  /* 58 */ NULL  ,
  /* 59 */ NULL  ,
  /* 5a */ NULL  ,
  /* 5b */ NULL  ,
  /* 5c */ NULL  ,
  /* 5d */ NULL  ,
  /* 5e */ NULL  ,
  /* 5f */ NULL  ,
  /* 60 */ NULL  ,
  /* 61 */ NULL  ,
  /* 62 */ NULL  ,
  /* 63 */ NULL  ,
  /* 64 */ NULL  ,
  /* 65 */ NULL  ,
  /* 66 */ NULL  ,
  /* 67 */ NULL  ,
  /* 68 */ NULL  ,
  /* 69 */ NULL  ,
  /* 6a */ NULL  ,
  /* 6b */ NULL  ,
  /* 6c */ NULL  ,
  /* 6d */ NULL  ,
  /* 6e */ NULL  ,
  /* 6f */ NULL  ,
  /* 70 */ NULL  ,
  /* 71 */ NULL  ,
  /* 72 */ NULL  ,
  /* 73 */ NULL  ,
  /* 74 */ NULL  ,
  /* 75 */ NULL  ,
  /* 76 */ NULL  ,
  /* 77 */ NULL  ,
  /* 78 */ NULL  ,
  /* 79 */ NULL  ,
  /* 7a */ NULL  ,
  /* 7b */ NULL  ,
  /* 7c */ NULL  ,
  /* 7d */ NULL  ,
  /* 7e */ NULL  ,
  /* 7f */ NULL  ,
  /* 80 */ NULL  ,
  /* 81 */ NULL  ,
  /* 82 */ NULL  ,
  /* 83 */ NULL  ,
  /* 84 */ NULL  ,
  /* 85 */ NULL  ,
  /* 86 */ NULL  ,
  /* 87 */ NULL  ,
  /* 88 */ NULL  ,
  /* 89 */ NULL  ,
  /* 8a */ NULL  ,
  /* 8b */ NULL  ,
  /* 8c */ NULL  ,
  /* 8d */ NULL  ,
  /* 8e */ NULL  ,
  /* 8f */ NULL  ,
  /* 90 */ NULL  ,
  /* 91 */ NULL  ,
  /* 92 */ NULL  ,
  /* 93 */ NULL  ,
  /* 94 */ NULL  ,
  /* 95 */ NULL  ,
  /* 96 */ NULL  ,
  /* 97 */ NULL  ,
  /* 98 */ NULL  ,
  /* 99 */ NULL  ,
  /* 9a */ NULL  ,
  /* 9b */ NULL  ,
  /* 9c */ NULL  ,
  /* 9d */ NULL  ,
  /* 9e */ NULL  ,
  /* 9f */ NULL  ,
  /* a0 */ NULL  ,
  /* a1 */ NULL  ,
  /* a2 */ NULL  ,
  /* a3 */ NULL  ,
  /* a4 */ NULL  ,
  /* a5 */ NULL  ,
  /* a6 */ NULL  ,
  /* a7 */ NULL  ,
  /* a8 */ NULL  ,
  /* a9 */ NULL  ,
  /* aa */ NULL  ,
  /* ab */ NULL  ,
  /* ac */ NULL  ,
  /* ad */ NULL  ,
  /* ae */ NULL  ,
  /* af */ NULL  ,
  /* b0 */ NULL  ,
  /* b1 */ NULL  ,
  /* b2 */ NULL  ,
  /* b3 */ NULL  ,
  /* b4 */ NULL  ,
  /* b5 */ NULL  ,
  /* b6 */ NULL  ,
  /* b7 */ NULL  ,
  /* b8 */ NULL  ,
  /* b9 */ NULL  ,
  /* ba */ NULL  ,
  /* bb */ NULL  ,
  /* bc */ NULL  ,
  /* bd */ NULL  ,
  /* be */ NULL  ,
  /* bf */ NULL  ,
  /* c0 */ g_Opcodes + 1731  ,
  /* c1 */ g_Opcodes + 1732  ,
  /* c2 */ g_Opcodes + 1733  ,
  /* c3 */ g_Opcodes + 1734  ,
  /* c4 */ g_Opcodes + 1735  ,
  /* c5 */ g_Opcodes + 1736  ,
  /* c6 */ g_Opcodes + 1737  ,
  /* c7 */ g_Opcodes + 1738  ,
  /* c8 */ g_Opcodes + 1739  ,
  /* c9 */ g_Opcodes + 1740  ,
  /* ca */ g_Opcodes + 1741  ,
  /* cb */ g_Opcodes + 1742  ,
  /* cc */ g_Opcodes + 1743  ,
  /* cd */ g_Opcodes + 1744  ,
  /* ce */ g_Opcodes + 1745  ,
  /* cf */ g_Opcodes + 1746  ,
  /* d0 */ g_Opcodes + 1747  ,
  /* d1 */ g_Opcodes + 1748  ,
  /* d2 */ g_Opcodes + 1749  ,
  /* d3 */ g_Opcodes + 1750  ,
  /* d4 */ g_Opcodes + 1751  ,
  /* d5 */ g_Opcodes + 1752  ,
  /* d6 */ g_Opcodes + 1753  ,
  /* d7 */ g_Opcodes + 1754  ,
  /* d8 */ g_Opcodes + 1755  ,
  /* d9 */ g_Opcodes + 1756  ,
  /* da */ g_Opcodes + 1757  ,
  /* db */ g_Opcodes + 1758  ,
  /* dc */ g_Opcodes + 1759  ,
  /* dd */ g_Opcodes + 1760  ,
  /* de */ g_Opcodes + 1761  ,
  /* df */ g_Opcodes + 1762  ,
  /* e0 */ g_Opcodes + 1763  ,
  /* e1 */ g_Opcodes + 1764  ,
  /* e2 */ g_Opcodes + 1765  ,
  /* e3 */ g_Opcodes + 1766  ,
  /* e4 */ g_Opcodes + 1767  ,
  /* e5 */ g_Opcodes + 1768  ,
  /* e6 */ g_Opcodes + 1769  ,
  /* e7 */ g_Opcodes + 1770  ,
  /* e8 */ g_Opcodes + 1771  ,
  /* e9 */ g_Opcodes + 1772  ,
  /* ea */ g_Opcodes + 1773  ,
  /* eb */ g_Opcodes + 1774  ,
  /* ec */ g_Opcodes + 1775  ,
  /* ed */ g_Opcodes + 1776  ,
  /* ee */ g_Opcodes + 1777  ,
  /* ef */ g_Opcodes + 1778  ,
  /* f0 */ g_Opcodes + 1779  ,
  /* f1 */ g_Opcodes + 1780  ,
  /* f2 */ g_Opcodes + 1781  ,
  /* f3 */ g_Opcodes + 1782  ,
  /* f4 */ g_Opcodes + 1783  ,
  /* f5 */ g_Opcodes + 1784  ,
  /* f6 */ g_Opcodes + 1785  ,
  /* f7 */ g_Opcodes + 1786  ,
  /* f8 */ g_Opcodes + 1787  ,
  /* f9 */ g_Opcodes + 1788  ,
  /* fa */ g_Opcodes + 1789  ,
  /* fb */ g_Opcodes + 1790  ,
  /* fc */ g_Opcodes + 1791  ,
  /* fd */ g_Opcodes + 1792  ,
  /* fe */ g_Opcodes + 1793  ,
  /* ff */ g_Opcodes + 1794  ,
},
/* PrefixD9 */
{
  /* 00 */ NULL  ,
  /* 01 */ NULL  ,
  /* 02 */ NULL  ,
  /* 03 */ NULL  ,
  /* 04 */ NULL  ,
  /* 05 */ NULL  ,
  /* 06 */ NULL  ,
  /* 07 */ NULL  ,
  /* 08 */ NULL  ,
  /* 09 */ NULL  ,
  /* 0a */ NULL  ,
  /* 0b */ NULL  ,
  /* 0c */ NULL  ,
  /* 0d */ NULL  ,
  /* 0e */ NULL  ,
  /* 0f */ NULL  ,
  /* 10 */ NULL  ,
  /* 11 */ NULL  ,
  /* 12 */ NULL  ,
  /* 13 */ NULL  ,
  /* 14 */ NULL  ,
  /* 15 */ NULL  ,
  /* 16 */ NULL  ,
  /* 17 */ NULL  ,
  /* 18 */ NULL  ,
  /* 19 */ NULL  ,
  /* 1a */ NULL  ,
  /* 1b */ NULL  ,
  /* 1c */ NULL  ,
  /* 1d */ NULL  ,
  /* 1e */ NULL  ,
  /* 1f */ NULL  ,
  /* 20 */ NULL  ,
  /* 21 */ NULL  ,
  /* 22 */ NULL  ,
  /* 23 */ NULL  ,
  /* 24 */ NULL  ,
  /* 25 */ NULL  ,
  /* 26 */ NULL  ,
  /* 27 */ NULL  ,
  /* 28 */ NULL  ,
  /* 29 */ NULL  ,
  /* 2a */ NULL  ,
  /* 2b */ NULL  ,
  /* 2c */ NULL  ,
  /* 2d */ NULL  ,
  /* 2e */ NULL  ,
  /* 2f */ NULL  ,
  /* 30 */ NULL  ,
  /* 31 */ NULL  ,
  /* 32 */ NULL  ,
  /* 33 */ NULL  ,
  /* 34 */ NULL  ,
  /* 35 */ NULL  ,
  /* 36 */ NULL  ,
  /* 37 */ NULL  ,
  /* 38 */ NULL  ,
  /* 39 */ NULL  ,
  /* 3a */ NULL  ,
  /* 3b */ NULL  ,
  /* 3c */ NULL  ,
  /* 3d */ NULL  ,
  /* 3e */ NULL  ,
  /* 3f */ NULL  ,
  /* 40 */ NULL  ,
  /* 41 */ NULL  ,
  /* 42 */ NULL  ,
  /* 43 */ NULL  ,
  /* 44 */ NULL  ,
  /* 45 */ NULL  ,
  /* 46 */ NULL  ,
  /* 47 */ NULL  ,
  /* 48 */ NULL  ,
  /* 49 */ NULL  ,
  /* 4a */ NULL  ,
  /* 4b */ NULL  ,
  /* 4c */ NULL  ,
  /* 4d */ NULL  ,
  /* 4e */ NULL  ,
  /* 4f */ NULL  ,
  /* 50 */ NULL  ,
  /* 51 */ NULL  ,
  /* 52 */ NULL  ,
  /* 53 */ NULL  ,
  /* 54 */ NULL  ,
  /* 55 */ NULL  ,
  /* 56 */ NULL  ,
  /* 57 */ NULL  ,
  /* 58 */ NULL  ,
  /* 59 */ NULL  ,
  /* 5a */ NULL  ,
  /* 5b */ NULL  ,
  /* 5c */ NULL  ,
  /* 5d */ NULL  ,
  /* 5e */ NULL  ,
  /* 5f */ NULL  ,
  /* 60 */ NULL  ,
  /* 61 */ NULL  ,
  /* 62 */ NULL  ,
  /* 63 */ NULL  ,
  /* 64 */ NULL  ,
  /* 65 */ NULL  ,
  /* 66 */ NULL  ,
  /* 67 */ NULL  ,
  /* 68 */ NULL  ,
  /* 69 */ NULL  ,
  /* 6a */ NULL  ,
  /* 6b */ NULL  ,
  /* 6c */ NULL  ,
  /* 6d */ NULL  ,
  /* 6e */ NULL  ,
  /* 6f */ NULL  ,
  /* 70 */ NULL  ,
  /* 71 */ NULL  ,
  /* 72 */ NULL  ,
  /* 73 */ NULL  ,
  /* 74 */ NULL  ,
  /* 75 */ NULL  ,
  /* 76 */ NULL  ,
  /* 77 */ NULL  ,
  /* 78 */ NULL  ,
  /* 79 */ NULL  ,
  /* 7a */ NULL  ,
  /* 7b */ NULL  ,
  /* 7c */ NULL  ,
  /* 7d */ NULL  ,
  /* 7e */ NULL  ,
  /* 7f */ NULL  ,
  /* 80 */ NULL  ,
  /* 81 */ NULL  ,
  /* 82 */ NULL  ,
  /* 83 */ NULL  ,
  /* 84 */ NULL  ,
  /* 85 */ NULL  ,
  /* 86 */ NULL  ,
  /* 87 */ NULL  ,
  /* 88 */ NULL  ,
  /* 89 */ NULL  ,
  /* 8a */ NULL  ,
  /* 8b */ NULL  ,
  /* 8c */ NULL  ,
  /* 8d */ NULL  ,
  /* 8e */ NULL  ,
  /* 8f */ NULL  ,
  /* 90 */ NULL  ,
  /* 91 */ NULL  ,
  /* 92 */ NULL  ,
  /* 93 */ NULL  ,
  /* 94 */ NULL  ,
  /* 95 */ NULL  ,
  /* 96 */ NULL  ,
  /* 97 */ NULL  ,
  /* 98 */ NULL  ,
  /* 99 */ NULL  ,
  /* 9a */ NULL  ,
  /* 9b */ NULL  ,
  /* 9c */ NULL  ,
  /* 9d */ NULL  ,
  /* 9e */ NULL  ,
  /* 9f */ NULL  ,
  /* a0 */ NULL  ,
  /* a1 */ NULL  ,
  /* a2 */ NULL  ,
  /* a3 */ NULL  ,
  /* a4 */ NULL  ,
  /* a5 */ NULL  ,
  /* a6 */ NULL  ,
  /* a7 */ NULL  ,
  /* a8 */ NULL  ,
  /* a9 */ NULL  ,
  /* aa */ NULL  ,
  /* ab */ NULL  ,
  /* ac */ NULL  ,
  /* ad */ NULL  ,
  /* ae */ NULL  ,
  /* af */ NULL  ,
  /* b0 */ NULL  ,
  /* b1 */ NULL  ,
  /* b2 */ NULL  ,
  /* b3 */ NULL  ,
  /* b4 */ NULL  ,
  /* b5 */ NULL  ,
  /* b6 */ NULL  ,
  /* b7 */ NULL  ,
  /* b8 */ NULL  ,
  /* b9 */ NULL  ,
  /* ba */ NULL  ,
  /* bb */ NULL  ,
  /* bc */ NULL  ,
  /* bd */ NULL  ,
  /* be */ NULL  ,
  /* bf */ NULL  ,
  /* c0 */ g_Opcodes + 1795  ,
  /* c1 */ g_Opcodes + 1796  ,
  /* c2 */ g_Opcodes + 1797  ,
  /* c3 */ g_Opcodes + 1798  ,
  /* c4 */ g_Opcodes + 1799  ,
  /* c5 */ g_Opcodes + 1800  ,
  /* c6 */ g_Opcodes + 1801  ,
  /* c7 */ g_Opcodes + 1802  ,
  /* c8 */ g_Opcodes + 1803  ,
  /* c9 */ g_Opcodes + 1804  ,
  /* ca */ g_Opcodes + 1805  ,
  /* cb */ g_Opcodes + 1806  ,
  /* cc */ g_Opcodes + 1807  ,
  /* cd */ g_Opcodes + 1808  ,
  /* ce */ g_Opcodes + 1809  ,
  /* cf */ g_Opcodes + 1810  ,
  /* d0 */ g_Opcodes + 1811  ,
  /* d1 */ g_Opcodes + 1812  ,
  /* d2 */ g_Opcodes + 1813  ,
  /* d3 */ g_Opcodes + 1814  ,
  /* d4 */ g_Opcodes + 1815  ,
  /* d5 */ g_Opcodes + 1816  ,
  /* d6 */ g_Opcodes + 1817  ,
  /* d7 */ g_Opcodes + 1818  ,
  /* d8 */ g_Opcodes + 1819  ,
  /* d9 */ g_Opcodes + 1820  ,
  /* da */ g_Opcodes + 1821  ,
  /* db */ g_Opcodes + 1822  ,
  /* dc */ g_Opcodes + 1823  ,
  /* dd */ g_Opcodes + 1824  ,
  /* de */ g_Opcodes + 1825  ,
  /* df */ g_Opcodes + 1826  ,
  /* e0 */ g_Opcodes + 1827  ,
  /* e1 */ g_Opcodes + 1828  ,
  /* e2 */ g_Opcodes + 1829  ,
  /* e3 */ g_Opcodes + 1830  ,
  /* e4 */ g_Opcodes + 1831  ,
  /* e5 */ g_Opcodes + 1832  ,
  /* e6 */ g_Opcodes + 1833  ,
  /* e7 */ g_Opcodes + 1834  ,
  /* e8 */ g_Opcodes + 1835  ,
  /* e9 */ g_Opcodes + 1836  ,
  /* ea */ g_Opcodes + 1837  ,
  /* eb */ g_Opcodes + 1838  ,
  /* ec */ g_Opcodes + 1839  ,
  /* ed */ g_Opcodes + 1840  ,
  /* ee */ g_Opcodes + 1841  ,
  /* ef */ g_Opcodes + 1842  ,
  /* f0 */ g_Opcodes + 1843  ,
  /* f1 */ g_Opcodes + 1844  ,
  /* f2 */ g_Opcodes + 1845  ,
  /* f3 */ g_Opcodes + 1846  ,
  /* f4 */ g_Opcodes + 1847  ,
  /* f5 */ g_Opcodes + 1848  ,
  /* f6 */ g_Opcodes + 1849  ,
  /* f7 */ g_Opcodes + 1850  ,
  /* f8 */ g_Opcodes + 1851  ,
  /* f9 */ g_Opcodes + 1852  ,
  /* fa */ g_Opcodes + 1853  ,
  /* fb */ g_Opcodes + 1854  ,
  /* fc */ g_Opcodes + 1855  ,
  /* fd */ g_Opcodes + 1856  ,
  /* fe */ g_Opcodes + 1857  ,
  /* ff */ g_Opcodes + 1858  ,
},
/* PrefixDA */
{
  /* 00 */ NULL  ,
  /* 01 */ NULL  ,
  /* 02 */ NULL  ,
  /* 03 */ NULL  ,
  /* 04 */ NULL  ,
  /* 05 */ NULL  ,
  /* 06 */ NULL  ,
  /* 07 */ NULL  ,
  /* 08 */ NULL  ,
  /* 09 */ NULL  ,
  /* 0a */ NULL  ,
  /* 0b */ NULL  ,
  /* 0c */ NULL  ,
  /* 0d */ NULL  ,
  /* 0e */ NULL  ,
  /* 0f */ NULL  ,
  /* 10 */ NULL  ,
  /* 11 */ NULL  ,
  /* 12 */ NULL  ,
  /* 13 */ NULL  ,
  /* 14 */ NULL  ,
  /* 15 */ NULL  ,
  /* 16 */ NULL  ,
  /* 17 */ NULL  ,
  /* 18 */ NULL  ,
  /* 19 */ NULL  ,
  /* 1a */ NULL  ,
  /* 1b */ NULL  ,
  /* 1c */ NULL  ,
  /* 1d */ NULL  ,
  /* 1e */ NULL  ,
  /* 1f */ NULL  ,
  /* 20 */ NULL  ,
  /* 21 */ NULL  ,
  /* 22 */ NULL  ,
  /* 23 */ NULL  ,
  /* 24 */ NULL  ,
  /* 25 */ NULL  ,
  /* 26 */ NULL  ,
  /* 27 */ NULL  ,
  /* 28 */ NULL  ,
  /* 29 */ NULL  ,
  /* 2a */ NULL  ,
  /* 2b */ NULL  ,
  /* 2c */ NULL  ,
  /* 2d */ NULL  ,
  /* 2e */ NULL  ,
  /* 2f */ NULL  ,
  /* 30 */ NULL  ,
  /* 31 */ NULL  ,
  /* 32 */ NULL  ,
  /* 33 */ NULL  ,
  /* 34 */ NULL  ,
  /* 35 */ NULL  ,
  /* 36 */ NULL  ,
  /* 37 */ NULL  ,
  /* 38 */ NULL  ,
  /* 39 */ NULL  ,
  /* 3a */ NULL  ,
  /* 3b */ NULL  ,
  /* 3c */ NULL  ,
  /* 3d */ NULL  ,
  /* 3e */ NULL  ,
  /* 3f */ NULL  ,
  /* 40 */ NULL  ,
  /* 41 */ NULL  ,
  /* 42 */ NULL  ,
  /* 43 */ NULL  ,
  /* 44 */ NULL  ,
  /* 45 */ NULL  ,
  /* 46 */ NULL  ,
  /* 47 */ NULL  ,
  /* 48 */ NULL  ,
  /* 49 */ NULL  ,
  /* 4a */ NULL  ,
  /* 4b */ NULL  ,
  /* 4c */ NULL  ,
  /* 4d */ NULL  ,
  /* 4e */ NULL  ,
  /* 4f */ NULL  ,
  /* 50 */ NULL  ,
  /* 51 */ NULL  ,
  /* 52 */ NULL  ,
  /* 53 */ NULL  ,
  /* 54 */ NULL  ,
  /* 55 */ NULL  ,
  /* 56 */ NULL  ,
  /* 57 */ NULL  ,
  /* 58 */ NULL  ,
  /* 59 */ NULL  ,
  /* 5a */ NULL  ,
  /* 5b */ NULL  ,
  /* 5c */ NULL  ,
  /* 5d */ NULL  ,
  /* 5e */ NULL  ,
  /* 5f */ NULL  ,
  /* 60 */ NULL  ,
  /* 61 */ NULL  ,
  /* 62 */ NULL  ,
  /* 63 */ NULL  ,
  /* 64 */ NULL  ,
  /* 65 */ NULL  ,
  /* 66 */ NULL  ,
  /* 67 */ NULL  ,
  /* 68 */ NULL  ,
  /* 69 */ NULL  ,
  /* 6a */ NULL  ,
  /* 6b */ NULL  ,
  /* 6c */ NULL  ,
  /* 6d */ NULL  ,
  /* 6e */ NULL  ,
  /* 6f */ NULL  ,
  /* 70 */ NULL  ,
  /* 71 */ NULL  ,
  /* 72 */ NULL  ,
  /* 73 */ NULL  ,
  /* 74 */ NULL  ,
  /* 75 */ NULL  ,
  /* 76 */ NULL  ,
  /* 77 */ NULL  ,
  /* 78 */ NULL  ,
  /* 79 */ NULL  ,
  /* 7a */ NULL  ,
  /* 7b */ NULL  ,
  /* 7c */ NULL  ,
  /* 7d */ NULL  ,
  /* 7e */ NULL  ,
  /* 7f */ NULL  ,
  /* 80 */ NULL  ,
  /* 81 */ NULL  ,
  /* 82 */ NULL  ,
  /* 83 */ NULL  ,
  /* 84 */ NULL  ,
  /* 85 */ NULL  ,
  /* 86 */ NULL  ,
  /* 87 */ NULL  ,
  /* 88 */ NULL  ,
  /* 89 */ NULL  ,
  /* 8a */ NULL  ,
  /* 8b */ NULL  ,
  /* 8c */ NULL  ,
  /* 8d */ NULL  ,
  /* 8e */ NULL  ,
  /* 8f */ NULL  ,
  /* 90 */ NULL  ,
  /* 91 */ NULL  ,
  /* 92 */ NULL  ,
  /* 93 */ NULL  ,
  /* 94 */ NULL  ,
  /* 95 */ NULL  ,
  /* 96 */ NULL  ,
  /* 97 */ NULL  ,
  /* 98 */ NULL  ,
  /* 99 */ NULL  ,
  /* 9a */ NULL  ,
  /* 9b */ NULL  ,
  /* 9c */ NULL  ,
  /* 9d */ NULL  ,
  /* 9e */ NULL  ,
  /* 9f */ NULL  ,
  /* a0 */ NULL  ,
  /* a1 */ NULL  ,
  /* a2 */ NULL  ,
  /* a3 */ NULL  ,
  /* a4 */ NULL  ,
  /* a5 */ NULL  ,
  /* a6 */ NULL  ,
  /* a7 */ NULL  ,
  /* a8 */ NULL  ,
  /* a9 */ NULL  ,
  /* aa */ NULL  ,
  /* ab */ NULL  ,
  /* ac */ NULL  ,
  /* ad */ NULL  ,
  /* ae */ NULL  ,
  /* af */ NULL  ,
  /* b0 */ NULL  ,
  /* b1 */ NULL  ,
  /* b2 */ NULL  ,
  /* b3 */ NULL  ,
  /* b4 */ NULL  ,
  /* b5 */ NULL  ,
  /* b6 */ NULL  ,
  /* b7 */ NULL  ,
  /* b8 */ NULL  ,
  /* b9 */ NULL  ,
  /* ba */ NULL  ,
  /* bb */ NULL  ,
  /* bc */ NULL  ,
  /* bd */ NULL  ,
  /* be */ NULL  ,
  /* bf */ NULL  ,
  /* c0 */ g_Opcodes + 1859  ,
  /* c1 */ g_Opcodes + 1860  ,
  /* c2 */ g_Opcodes + 1861  ,
  /* c3 */ g_Opcodes + 1862  ,
  /* c4 */ g_Opcodes + 1863  ,
  /* c5 */ g_Opcodes + 1864  ,
  /* c6 */ g_Opcodes + 1865  ,
  /* c7 */ g_Opcodes + 1866  ,
  /* c8 */ g_Opcodes + 1867  ,
  /* c9 */ g_Opcodes + 1868  ,
  /* ca */ g_Opcodes + 1869  ,
  /* cb */ g_Opcodes + 1870  ,
  /* cc */ g_Opcodes + 1871  ,
  /* cd */ g_Opcodes + 1872  ,
  /* ce */ g_Opcodes + 1873  ,
  /* cf */ g_Opcodes + 1874  ,
  /* d0 */ g_Opcodes + 1875  ,
  /* d1 */ g_Opcodes + 1876  ,
  /* d2 */ g_Opcodes + 1877  ,
  /* d3 */ g_Opcodes + 1878  ,
  /* d4 */ g_Opcodes + 1879  ,
  /* d5 */ g_Opcodes + 1880  ,
  /* d6 */ g_Opcodes + 1881  ,
  /* d7 */ g_Opcodes + 1882  ,
  /* d8 */ g_Opcodes + 1883  ,
  /* d9 */ g_Opcodes + 1884  ,
  /* da */ g_Opcodes + 1885  ,
  /* db */ g_Opcodes + 1886  ,
  /* dc */ g_Opcodes + 1887  ,
  /* dd */ g_Opcodes + 1888  ,
  /* de */ g_Opcodes + 1889  ,
  /* df */ g_Opcodes + 1890  ,
  /* e0 */ g_Opcodes + 1891  ,
  /* e1 */ g_Opcodes + 1892  ,
  /* e2 */ g_Opcodes + 1893  ,
  /* e3 */ g_Opcodes + 1894  ,
  /* e4 */ g_Opcodes + 1895  ,
  /* e5 */ g_Opcodes + 1896  ,
  /* e6 */ g_Opcodes + 1897  ,
  /* e7 */ g_Opcodes + 1898  ,
  /* e8 */ g_Opcodes + 1899  ,
  /* e9 */ g_Opcodes + 1900  ,
  /* ea */ g_Opcodes + 1901  ,
  /* eb */ g_Opcodes + 1902  ,
  /* ec */ g_Opcodes + 1903  ,
  /* ed */ g_Opcodes + 1904  ,
  /* ee */ g_Opcodes + 1905  ,
  /* ef */ g_Opcodes + 1906  ,
  /* f0 */ g_Opcodes + 1907  ,
  /* f1 */ g_Opcodes + 1908  ,
  /* f2 */ g_Opcodes + 1909  ,
  /* f3 */ g_Opcodes + 1910  ,
  /* f4 */ g_Opcodes + 1911  ,
  /* f5 */ g_Opcodes + 1912  ,
  /* f6 */ g_Opcodes + 1913  ,
  /* f7 */ g_Opcodes + 1914  ,
  /* f8 */ g_Opcodes + 1915  ,
  /* f9 */ g_Opcodes + 1916  ,
  /* fa */ g_Opcodes + 1917  ,
  /* fb */ g_Opcodes + 1918  ,
  /* fc */ g_Opcodes + 1919  ,
  /* fd */ g_Opcodes + 1920  ,
  /* fe */ g_Opcodes + 1921  ,
  /* ff */ g_Opcodes + 1922  ,
},
/* PrefixDB */
{
  /* 00 */ NULL  ,
  /* 01 */ NULL  ,
  /* 02 */ NULL  ,
  /* 03 */ NULL  ,
  /* 04 */ NULL  ,
  /* 05 */ NULL  ,
  /* 06 */ NULL  ,
  /* 07 */ NULL  ,
  /* 08 */ NULL  ,
  /* 09 */ NULL  ,
  /* 0a */ NULL  ,
  /* 0b */ NULL  ,
  /* 0c */ NULL  ,
  /* 0d */ NULL  ,
  /* 0e */ NULL  ,
  /* 0f */ NULL  ,
  /* 10 */ NULL  ,
  /* 11 */ NULL  ,
  /* 12 */ NULL  ,
  /* 13 */ NULL  ,
  /* 14 */ NULL  ,
  /* 15 */ NULL  ,
  /* 16 */ NULL  ,
  /* 17 */ NULL  ,
  /* 18 */ NULL  ,
  /* 19 */ NULL  ,
  /* 1a */ NULL  ,
  /* 1b */ NULL  ,
  /* 1c */ NULL  ,
  /* 1d */ NULL  ,
  /* 1e */ NULL  ,
  /* 1f */ NULL  ,
  /* 20 */ NULL  ,
  /* 21 */ NULL  ,
  /* 22 */ NULL  ,
  /* 23 */ NULL  ,
  /* 24 */ NULL  ,
  /* 25 */ NULL  ,
  /* 26 */ NULL  ,
  /* 27 */ NULL  ,
  /* 28 */ NULL  ,
  /* 29 */ NULL  ,
  /* 2a */ NULL  ,
  /* 2b */ NULL  ,
  /* 2c */ NULL  ,
  /* 2d */ NULL  ,
  /* 2e */ NULL  ,
  /* 2f */ NULL  ,
  /* 30 */ NULL  ,
  /* 31 */ NULL  ,
  /* 32 */ NULL  ,
  /* 33 */ NULL  ,
  /* 34 */ NULL  ,
  /* 35 */ NULL  ,
  /* 36 */ NULL  ,
  /* 37 */ NULL  ,
  /* 38 */ NULL  ,
  /* 39 */ NULL  ,
  /* 3a */ NULL  ,
  /* 3b */ NULL  ,
  /* 3c */ NULL  ,
  /* 3d */ NULL  ,
  /* 3e */ NULL  ,
  /* 3f */ NULL  ,
  /* 40 */ NULL  ,
  /* 41 */ NULL  ,
  /* 42 */ NULL  ,
  /* 43 */ NULL  ,
  /* 44 */ NULL  ,
  /* 45 */ NULL  ,
  /* 46 */ NULL  ,
  /* 47 */ NULL  ,
  /* 48 */ NULL  ,
  /* 49 */ NULL  ,
  /* 4a */ NULL  ,
  /* 4b */ NULL  ,
  /* 4c */ NULL  ,
  /* 4d */ NULL  ,
  /* 4e */ NULL  ,
  /* 4f */ NULL  ,
  /* 50 */ NULL  ,
  /* 51 */ NULL  ,
  /* 52 */ NULL  ,
  /* 53 */ NULL  ,
  /* 54 */ NULL  ,
  /* 55 */ NULL  ,
  /* 56 */ NULL  ,
  /* 57 */ NULL  ,
  /* 58 */ NULL  ,
  /* 59 */ NULL  ,
  /* 5a */ NULL  ,
  /* 5b */ NULL  ,
  /* 5c */ NULL  ,
  /* 5d */ NULL  ,
  /* 5e */ NULL  ,
  /* 5f */ NULL  ,
  /* 60 */ NULL  ,
  /* 61 */ NULL  ,
  /* 62 */ NULL  ,
  /* 63 */ NULL  ,
  /* 64 */ NULL  ,
  /* 65 */ NULL  ,
  /* 66 */ NULL  ,
  /* 67 */ NULL  ,
  /* 68 */ NULL  ,
  /* 69 */ NULL  ,
  /* 6a */ NULL  ,
  /* 6b */ NULL  ,
  /* 6c */ NULL  ,
  /* 6d */ NULL  ,
  /* 6e */ NULL  ,
  /* 6f */ NULL  ,
  /* 70 */ NULL  ,
  /* 71 */ NULL  ,
  /* 72 */ NULL  ,
  /* 73 */ NULL  ,
  /* 74 */ NULL  ,
  /* 75 */ NULL  ,
  /* 76 */ NULL  ,
  /* 77 */ NULL  ,
  /* 78 */ NULL  ,
  /* 79 */ NULL  ,
  /* 7a */ NULL  ,
  /* 7b */ NULL  ,
  /* 7c */ NULL  ,
  /* 7d */ NULL  ,
  /* 7e */ NULL  ,
  /* 7f */ NULL  ,
  /* 80 */ NULL  ,
  /* 81 */ NULL  ,
  /* 82 */ NULL  ,
  /* 83 */ NULL  ,
  /* 84 */ NULL  ,
  /* 85 */ NULL  ,
  /* 86 */ NULL  ,
  /* 87 */ NULL  ,
  /* 88 */ NULL  ,
  /* 89 */ NULL  ,
  /* 8a */ NULL  ,
  /* 8b */ NULL  ,
  /* 8c */ NULL  ,
  /* 8d */ NULL  ,
  /* 8e */ NULL  ,
  /* 8f */ NULL  ,
  /* 90 */ NULL  ,
  /* 91 */ NULL  ,
  /* 92 */ NULL  ,
  /* 93 */ NULL  ,
  /* 94 */ NULL  ,
  /* 95 */ NULL  ,
  /* 96 */ NULL  ,
  /* 97 */ NULL  ,
  /* 98 */ NULL  ,
  /* 99 */ NULL  ,
  /* 9a */ NULL  ,
  /* 9b */ NULL  ,
  /* 9c */ NULL  ,
  /* 9d */ NULL  ,
  /* 9e */ NULL  ,
  /* 9f */ NULL  ,
  /* a0 */ NULL  ,
  /* a1 */ NULL  ,
  /* a2 */ NULL  ,
  /* a3 */ NULL  ,
  /* a4 */ NULL  ,
  /* a5 */ NULL  ,
  /* a6 */ NULL  ,
  /* a7 */ NULL  ,
  /* a8 */ NULL  ,
  /* a9 */ NULL  ,
  /* aa */ NULL  ,
  /* ab */ NULL  ,
  /* ac */ NULL  ,
  /* ad */ NULL  ,
  /* ae */ NULL  ,
  /* af */ NULL  ,
  /* b0 */ NULL  ,
  /* b1 */ NULL  ,
  /* b2 */ NULL  ,
  /* b3 */ NULL  ,
  /* b4 */ NULL  ,
  /* b5 */ NULL  ,
  /* b6 */ NULL  ,
  /* b7 */ NULL  ,
  /* b8 */ NULL  ,
  /* b9 */ NULL  ,
  /* ba */ NULL  ,
  /* bb */ NULL  ,
  /* bc */ NULL  ,
  /* bd */ NULL  ,
  /* be */ NULL  ,
  /* bf */ NULL  ,
  /* c0 */ g_Opcodes + 1923  ,
  /* c1 */ g_Opcodes + 1924  ,
  /* c2 */ g_Opcodes + 1925  ,
  /* c3 */ g_Opcodes + 1926  ,
  /* c4 */ g_Opcodes + 1927  ,
  /* c5 */ g_Opcodes + 1928  ,
  /* c6 */ g_Opcodes + 1929  ,
  /* c7 */ g_Opcodes + 1930  ,
  /* c8 */ g_Opcodes + 1931  ,
  /* c9 */ g_Opcodes + 1932  ,
  /* ca */ g_Opcodes + 1933  ,
  /* cb */ g_Opcodes + 1934  ,
  /* cc */ g_Opcodes + 1935  ,
  /* cd */ g_Opcodes + 1936  ,
  /* ce */ g_Opcodes + 1937  ,
  /* cf */ g_Opcodes + 1938  ,
  /* d0 */ g_Opcodes + 1939  ,
  /* d1 */ g_Opcodes + 1940  ,
  /* d2 */ g_Opcodes + 1941  ,
  /* d3 */ g_Opcodes + 1942  ,
  /* d4 */ g_Opcodes + 1943  ,
  /* d5 */ g_Opcodes + 1944  ,
  /* d6 */ g_Opcodes + 1945  ,
  /* d7 */ g_Opcodes + 1946  ,
  /* d8 */ g_Opcodes + 1947  ,
  /* d9 */ g_Opcodes + 1948  ,
  /* da */ g_Opcodes + 1949  ,
  /* db */ g_Opcodes + 1950  ,
  /* dc */ g_Opcodes + 1951  ,
  /* dd */ g_Opcodes + 1952  ,
  /* de */ g_Opcodes + 1953  ,
  /* df */ g_Opcodes + 1954  ,
  /* e0 */ g_Opcodes + 1955  ,
  /* e1 */ g_Opcodes + 1956  ,
  /* e2 */ g_Opcodes + 1957  ,
  /* e3 */ g_Opcodes + 1958  ,
  /* e4 */ g_Opcodes + 1959  ,
  /* e5 */ g_Opcodes + 1960  ,
  /* e6 */ g_Opcodes + 1961  ,
  /* e7 */ g_Opcodes + 1962  ,
  /* e8 */ g_Opcodes + 1963  ,
  /* e9 */ g_Opcodes + 1964  ,
  /* ea */ g_Opcodes + 1965  ,
  /* eb */ g_Opcodes + 1966  ,
  /* ec */ g_Opcodes + 1967  ,
  /* ed */ g_Opcodes + 1968  ,
  /* ee */ g_Opcodes + 1969  ,
  /* ef */ g_Opcodes + 1970  ,
  /* f0 */ g_Opcodes + 1971  ,
  /* f1 */ g_Opcodes + 1972  ,
  /* f2 */ g_Opcodes + 1973  ,
  /* f3 */ g_Opcodes + 1974  ,
  /* f4 */ g_Opcodes + 1975  ,
  /* f5 */ g_Opcodes + 1976  ,
  /* f6 */ g_Opcodes + 1977  ,
  /* f7 */ g_Opcodes + 1978  ,
  /* f8 */ NULL  ,
  /* f9 */ NULL  ,
  /* fa */ NULL  ,
  /* fb */ NULL  ,
  /* fc */ NULL  ,
  /* fd */ NULL  ,
  /* fe */ NULL  ,
  /* ff */ NULL  ,
},
/* PrefixDC */
{
  /* 00 */ NULL  ,
  /* 01 */ NULL  ,
  /* 02 */ NULL  ,
  /* 03 */ NULL  ,
  /* 04 */ NULL  ,
  /* 05 */ NULL  ,
  /* 06 */ NULL  ,
  /* 07 */ NULL  ,
  /* 08 */ NULL  ,
  /* 09 */ NULL  ,
  /* 0a */ NULL  ,
  /* 0b */ NULL  ,
  /* 0c */ NULL  ,
  /* 0d */ NULL  ,
  /* 0e */ NULL  ,
  /* 0f */ NULL  ,
  /* 10 */ NULL  ,
  /* 11 */ NULL  ,
  /* 12 */ NULL  ,
  /* 13 */ NULL  ,
  /* 14 */ NULL  ,
  /* 15 */ NULL  ,
  /* 16 */ NULL  ,
  /* 17 */ NULL  ,
  /* 18 */ NULL  ,
  /* 19 */ NULL  ,
  /* 1a */ NULL  ,
  /* 1b */ NULL  ,
  /* 1c */ NULL  ,
  /* 1d */ NULL  ,
  /* 1e */ NULL  ,
  /* 1f */ NULL  ,
  /* 20 */ NULL  ,
  /* 21 */ NULL  ,
  /* 22 */ NULL  ,
  /* 23 */ NULL  ,
  /* 24 */ NULL  ,
  /* 25 */ NULL  ,
  /* 26 */ NULL  ,
  /* 27 */ NULL  ,
  /* 28 */ NULL  ,
  /* 29 */ NULL  ,
  /* 2a */ NULL  ,
  /* 2b */ NULL  ,
  /* 2c */ NULL  ,
  /* 2d */ NULL  ,
  /* 2e */ NULL  ,
  /* 2f */ NULL  ,
  /* 30 */ NULL  ,
  /* 31 */ NULL  ,
  /* 32 */ NULL  ,
  /* 33 */ NULL  ,
  /* 34 */ NULL  ,
  /* 35 */ NULL  ,
  /* 36 */ NULL  ,
  /* 37 */ NULL  ,
  /* 38 */ NULL  ,
  /* 39 */ NULL  ,
  /* 3a */ NULL  ,
  /* 3b */ NULL  ,
  /* 3c */ NULL  ,
  /* 3d */ NULL  ,
  /* 3e */ NULL  ,
  /* 3f */ NULL  ,
  /* 40 */ NULL  ,
  /* 41 */ NULL  ,
  /* 42 */ NULL  ,
  /* 43 */ NULL  ,
  /* 44 */ NULL  ,
  /* 45 */ NULL  ,
  /* 46 */ NULL  ,
  /* 47 */ NULL  ,
  /* 48 */ NULL  ,
  /* 49 */ NULL  ,
  /* 4a */ NULL  ,
  /* 4b */ NULL  ,
  /* 4c */ NULL  ,
  /* 4d */ NULL  ,
  /* 4e */ NULL  ,
  /* 4f */ NULL  ,
  /* 50 */ NULL  ,
  /* 51 */ NULL  ,
  /* 52 */ NULL  ,
  /* 53 */ NULL  ,
  /* 54 */ NULL  ,
  /* 55 */ NULL  ,
  /* 56 */ NULL  ,
  /* 57 */ NULL  ,
  /* 58 */ NULL  ,
  /* 59 */ NULL  ,
  /* 5a */ NULL  ,
  /* 5b */ NULL  ,
  /* 5c */ NULL  ,
  /* 5d */ NULL  ,
  /* 5e */ NULL  ,
  /* 5f */ NULL  ,
  /* 60 */ NULL  ,
  /* 61 */ NULL  ,
  /* 62 */ NULL  ,
  /* 63 */ NULL  ,
  /* 64 */ NULL  ,
  /* 65 */ NULL  ,
  /* 66 */ NULL  ,
  /* 67 */ NULL  ,
  /* 68 */ NULL  ,
  /* 69 */ NULL  ,
  /* 6a */ NULL  ,
  /* 6b */ NULL  ,
  /* 6c */ NULL  ,
  /* 6d */ NULL  ,
  /* 6e */ NULL  ,
  /* 6f */ NULL  ,
  /* 70 */ NULL  ,
  /* 71 */ NULL  ,
  /* 72 */ NULL  ,
  /* 73 */ NULL  ,
  /* 74 */ NULL  ,
  /* 75 */ NULL  ,
  /* 76 */ NULL  ,
  /* 77 */ NULL  ,
  /* 78 */ NULL  ,
  /* 79 */ NULL  ,
  /* 7a */ NULL  ,
  /* 7b */ NULL  ,
  /* 7c */ NULL  ,
  /* 7d */ NULL  ,
  /* 7e */ NULL  ,
  /* 7f */ NULL  ,
  /* 80 */ NULL  ,
  /* 81 */ NULL  ,
  /* 82 */ NULL  ,
  /* 83 */ NULL  ,
  /* 84 */ NULL  ,
  /* 85 */ NULL  ,
  /* 86 */ NULL  ,
  /* 87 */ NULL  ,
  /* 88 */ NULL  ,
  /* 89 */ NULL  ,
  /* 8a */ NULL  ,
  /* 8b */ NULL  ,
  /* 8c */ NULL  ,
  /* 8d */ NULL  ,
  /* 8e */ NULL  ,
  /* 8f */ NULL  ,
  /* 90 */ NULL  ,
  /* 91 */ NULL  ,
  /* 92 */ NULL  ,
  /* 93 */ NULL  ,
  /* 94 */ NULL  ,
  /* 95 */ NULL  ,
  /* 96 */ NULL  ,
  /* 97 */ NULL  ,
  /* 98 */ NULL  ,
  /* 99 */ NULL  ,
  /* 9a */ NULL  ,
  /* 9b */ NULL  ,
  /* 9c */ NULL  ,
  /* 9d */ NULL  ,
  /* 9e */ NULL  ,
  /* 9f */ NULL  ,
  /* a0 */ NULL  ,
  /* a1 */ NULL  ,
  /* a2 */ NULL  ,
  /* a3 */ NULL  ,
  /* a4 */ NULL  ,
  /* a5 */ NULL  ,
  /* a6 */ NULL  ,
  /* a7 */ NULL  ,
  /* a8 */ NULL  ,
  /* a9 */ NULL  ,
  /* aa */ NULL  ,
  /* ab */ NULL  ,
  /* ac */ NULL  ,
  /* ad */ NULL  ,
  /* ae */ NULL  ,
  /* af */ NULL  ,
  /* b0 */ NULL  ,
  /* b1 */ NULL  ,
  /* b2 */ NULL  ,
  /* b3 */ NULL  ,
  /* b4 */ NULL  ,
  /* b5 */ NULL  ,
  /* b6 */ NULL  ,
  /* b7 */ NULL  ,
  /* b8 */ NULL  ,
  /* b9 */ NULL  ,
  /* ba */ NULL  ,
  /* bb */ NULL  ,
  /* bc */ NULL  ,
  /* bd */ NULL  ,
  /* be */ NULL  ,
  /* bf */ NULL  ,
  /* c0 */ g_Opcodes + 1979  ,
  /* c1 */ g_Opcodes + 1980  ,
  /* c2 */ g_Opcodes + 1981  ,
  /* c3 */ g_Opcodes + 1982  ,
  /* c4 */ g_Opcodes + 1983  ,
  /* c5 */ g_Opcodes + 1984  ,
  /* c6 */ g_Opcodes + 1985  ,
  /* c7 */ g_Opcodes + 1986  ,
  /* c8 */ g_Opcodes + 1987  ,
  /* c9 */ g_Opcodes + 1988  ,
  /* ca */ g_Opcodes + 1989  ,
  /* cb */ g_Opcodes + 1990  ,
  /* cc */ g_Opcodes + 1991  ,
  /* cd */ g_Opcodes + 1992  ,
  /* ce */ g_Opcodes + 1993  ,
  /* cf */ g_Opcodes + 1994  ,
  /* d0 */ g_Opcodes + 1995  ,
  /* d1 */ g_Opcodes + 1996  ,
  /* d2 */ g_Opcodes + 1997  ,
  /* d3 */ g_Opcodes + 1998  ,
  /* d4 */ g_Opcodes + 1999  ,
  /* d5 */ g_Opcodes + 2000  ,
  /* d6 */ g_Opcodes + 2001  ,
  /* d7 */ g_Opcodes + 2002  ,
  /* d8 */ g_Opcodes + 2003  ,
  /* d9 */ g_Opcodes + 2004  ,
  /* da */ g_Opcodes + 2005  ,
  /* db */ g_Opcodes + 2006  ,
  /* dc */ g_Opcodes + 2007  ,
  /* dd */ g_Opcodes + 2008  ,
  /* de */ g_Opcodes + 2009  ,
  /* df */ g_Opcodes + 2010  ,
  /* e0 */ g_Opcodes + 2011  ,
  /* e1 */ g_Opcodes + 2012  ,
  /* e2 */ g_Opcodes + 2013  ,
  /* e3 */ g_Opcodes + 2014  ,
  /* e4 */ g_Opcodes + 2015  ,
  /* e5 */ g_Opcodes + 2016  ,
  /* e6 */ g_Opcodes + 2017  ,
  /* e7 */ g_Opcodes + 2018  ,
  /* e8 */ g_Opcodes + 2019  ,
  /* e9 */ g_Opcodes + 2020  ,
  /* ea */ g_Opcodes + 2021  ,
  /* eb */ g_Opcodes + 2022  ,
  /* ec */ g_Opcodes + 2023  ,
  /* ed */ g_Opcodes + 2024  ,
  /* ee */ g_Opcodes + 2025  ,
  /* ef */ g_Opcodes + 2026  ,
  /* f0 */ g_Opcodes + 2027  ,
  /* f1 */ g_Opcodes + 2028  ,
  /* f2 */ g_Opcodes + 2029  ,
  /* f3 */ g_Opcodes + 2030  ,
  /* f4 */ g_Opcodes + 2031  ,
  /* f5 */ g_Opcodes + 2032  ,
  /* f6 */ g_Opcodes + 2033  ,
  /* f7 */ g_Opcodes + 2034  ,
  /* f8 */ g_Opcodes + 2035  ,
  /* f9 */ g_Opcodes + 2036  ,
  /* fa */ g_Opcodes + 2037  ,
  /* fb */ g_Opcodes + 2038  ,
  /* fc */ g_Opcodes + 2039  ,
  /* fd */ g_Opcodes + 2040  ,
  /* fe */ g_Opcodes + 2041  ,
  /* ff */ g_Opcodes + 2042  ,
},
/* PrefixDD */
{
  /* 00 */ NULL  ,
  /* 01 */ NULL  ,
  /* 02 */ NULL  ,
  /* 03 */ NULL  ,
  /* 04 */ NULL  ,
  /* 05 */ NULL  ,
  /* 06 */ NULL  ,
  /* 07 */ NULL  ,
  /* 08 */ NULL  ,
  /* 09 */ NULL  ,
  /* 0a */ NULL  ,
  /* 0b */ NULL  ,
  /* 0c */ NULL  ,
  /* 0d */ NULL  ,
  /* 0e */ NULL  ,
  /* 0f */ NULL  ,
  /* 10 */ NULL  ,
  /* 11 */ NULL  ,
  /* 12 */ NULL  ,
  /* 13 */ NULL  ,
  /* 14 */ NULL  ,
  /* 15 */ NULL  ,
  /* 16 */ NULL  ,
  /* 17 */ NULL  ,
  /* 18 */ NULL  ,
  /* 19 */ NULL  ,
  /* 1a */ NULL  ,
  /* 1b */ NULL  ,
  /* 1c */ NULL  ,
  /* 1d */ NULL  ,
  /* 1e */ NULL  ,
  /* 1f */ NULL  ,
  /* 20 */ NULL  ,
  /* 21 */ NULL  ,
  /* 22 */ NULL  ,
  /* 23 */ NULL  ,
  /* 24 */ NULL  ,
  /* 25 */ NULL  ,
  /* 26 */ NULL  ,
  /* 27 */ NULL  ,
  /* 28 */ NULL  ,
  /* 29 */ NULL  ,
  /* 2a */ NULL  ,
  /* 2b */ NULL  ,
  /* 2c */ NULL  ,
  /* 2d */ NULL  ,
  /* 2e */ NULL  ,
  /* 2f */ NULL  ,
  /* 30 */ NULL  ,
  /* 31 */ NULL  ,
  /* 32 */ NULL  ,
  /* 33 */ NULL  ,
  /* 34 */ NULL  ,
  /* 35 */ NULL  ,
  /* 36 */ NULL  ,
  /* 37 */ NULL  ,
  /* 38 */ NULL  ,
  /* 39 */ NULL  ,
  /* 3a */ NULL  ,
  /* 3b */ NULL  ,
  /* 3c */ NULL  ,
  /* 3d */ NULL  ,
  /* 3e */ NULL  ,
  /* 3f */ NULL  ,
  /* 40 */ NULL  ,
  /* 41 */ NULL  ,
  /* 42 */ NULL  ,
  /* 43 */ NULL  ,
  /* 44 */ NULL  ,
  /* 45 */ NULL  ,
  /* 46 */ NULL  ,
  /* 47 */ NULL  ,
  /* 48 */ NULL  ,
  /* 49 */ NULL  ,
  /* 4a */ NULL  ,
  /* 4b */ NULL  ,
  /* 4c */ NULL  ,
  /* 4d */ NULL  ,
  /* 4e */ NULL  ,
  /* 4f */ NULL  ,
  /* 50 */ NULL  ,
  /* 51 */ NULL  ,
  /* 52 */ NULL  ,
  /* 53 */ NULL  ,
  /* 54 */ NULL  ,
  /* 55 */ NULL  ,
  /* 56 */ NULL  ,
  /* 57 */ NULL  ,
  /* 58 */ NULL  ,
  /* 59 */ NULL  ,
  /* 5a */ NULL  ,
  /* 5b */ NULL  ,
  /* 5c */ NULL  ,
  /* 5d */ NULL  ,
  /* 5e */ NULL  ,
  /* 5f */ NULL  ,
  /* 60 */ NULL  ,
  /* 61 */ NULL  ,
  /* 62 */ NULL  ,
  /* 63 */ NULL  ,
  /* 64 */ NULL  ,
  /* 65 */ NULL  ,
  /* 66 */ NULL  ,
  /* 67 */ NULL  ,
  /* 68 */ NULL  ,
  /* 69 */ NULL  ,
  /* 6a */ NULL  ,
  /* 6b */ NULL  ,
  /* 6c */ NULL  ,
  /* 6d */ NULL  ,
  /* 6e */ NULL  ,
  /* 6f */ NULL  ,
  /* 70 */ NULL  ,
  /* 71 */ NULL  ,
  /* 72 */ NULL  ,
  /* 73 */ NULL  ,
  /* 74 */ NULL  ,
  /* 75 */ NULL  ,
  /* 76 */ NULL  ,
  /* 77 */ NULL  ,
  /* 78 */ NULL  ,
  /* 79 */ NULL  ,
  /* 7a */ NULL  ,
  /* 7b */ NULL  ,
  /* 7c */ NULL  ,
  /* 7d */ NULL  ,
  /* 7e */ NULL  ,
  /* 7f */ NULL  ,
  /* 80 */ NULL  ,
  /* 81 */ NULL  ,
  /* 82 */ NULL  ,
  /* 83 */ NULL  ,
  /* 84 */ NULL  ,
  /* 85 */ NULL  ,
  /* 86 */ NULL  ,
  /* 87 */ NULL  ,
  /* 88 */ NULL  ,
  /* 89 */ NULL  ,
  /* 8a */ NULL  ,
  /* 8b */ NULL  ,
  /* 8c */ NULL  ,
  /* 8d */ NULL  ,
  /* 8e */ NULL  ,
  /* 8f */ NULL  ,
  /* 90 */ NULL  ,
  /* 91 */ NULL  ,
  /* 92 */ NULL  ,
  /* 93 */ NULL  ,
  /* 94 */ NULL  ,
  /* 95 */ NULL  ,
  /* 96 */ NULL  ,
  /* 97 */ NULL  ,
  /* 98 */ NULL  ,
  /* 99 */ NULL  ,
  /* 9a */ NULL  ,
  /* 9b */ NULL  ,
  /* 9c */ NULL  ,
  /* 9d */ NULL  ,
  /* 9e */ NULL  ,
  /* 9f */ NULL  ,
  /* a0 */ NULL  ,
  /* a1 */ NULL  ,
  /* a2 */ NULL  ,
  /* a3 */ NULL  ,
  /* a4 */ NULL  ,
  /* a5 */ NULL  ,
  /* a6 */ NULL  ,
  /* a7 */ NULL  ,
  /* a8 */ NULL  ,
  /* a9 */ NULL  ,
  /* aa */ NULL  ,
  /* ab */ NULL  ,
  /* ac */ NULL  ,
  /* ad */ NULL  ,
  /* ae */ NULL  ,
  /* af */ NULL  ,
  /* b0 */ NULL  ,
  /* b1 */ NULL  ,
  /* b2 */ NULL  ,
  /* b3 */ NULL  ,
  /* b4 */ NULL  ,
  /* b5 */ NULL  ,
  /* b6 */ NULL  ,
  /* b7 */ NULL  ,
  /* b8 */ NULL  ,
  /* b9 */ NULL  ,
  /* ba */ NULL  ,
  /* bb */ NULL  ,
  /* bc */ NULL  ,
  /* bd */ NULL  ,
  /* be */ NULL  ,
  /* bf */ NULL  ,
  /* c0 */ g_Opcodes + 2043  ,
  /* c1 */ g_Opcodes + 2044  ,
  /* c2 */ g_Opcodes + 2045  ,
  /* c3 */ g_Opcodes + 2046  ,
  /* c4 */ g_Opcodes + 2047  ,
  /* c5 */ g_Opcodes + 2048  ,
  /* c6 */ g_Opcodes + 2049  ,
  /* c7 */ g_Opcodes + 2050  ,
  /* c8 */ g_Opcodes + 2051  ,
  /* c9 */ g_Opcodes + 2052  ,
  /* ca */ g_Opcodes + 2053  ,
  /* cb */ g_Opcodes + 2054  ,
  /* cc */ g_Opcodes + 2055  ,
  /* cd */ g_Opcodes + 2056  ,
  /* ce */ g_Opcodes + 2057  ,
  /* cf */ g_Opcodes + 2058  ,
  /* d0 */ g_Opcodes + 2059  ,
  /* d1 */ g_Opcodes + 2060  ,
  /* d2 */ g_Opcodes + 2061  ,
  /* d3 */ g_Opcodes + 2062  ,
  /* d4 */ g_Opcodes + 2063  ,
  /* d5 */ g_Opcodes + 2064  ,
  /* d6 */ g_Opcodes + 2065  ,
  /* d7 */ g_Opcodes + 2066  ,
  /* d8 */ g_Opcodes + 2067  ,
  /* d9 */ g_Opcodes + 2068  ,
  /* da */ g_Opcodes + 2069  ,
  /* db */ g_Opcodes + 2070  ,
  /* dc */ g_Opcodes + 2071  ,
  /* dd */ g_Opcodes + 2072  ,
  /* de */ g_Opcodes + 2073  ,
  /* df */ g_Opcodes + 2074  ,
  /* e0 */ g_Opcodes + 2075  ,
  /* e1 */ g_Opcodes + 2076  ,
  /* e2 */ g_Opcodes + 2077  ,
  /* e3 */ g_Opcodes + 2078  ,
  /* e4 */ g_Opcodes + 2079  ,
  /* e5 */ g_Opcodes + 2080  ,
  /* e6 */ g_Opcodes + 2081  ,
  /* e7 */ g_Opcodes + 2082  ,
  /* e8 */ g_Opcodes + 2083  ,
  /* e9 */ g_Opcodes + 2084  ,
  /* ea */ g_Opcodes + 2085  ,
  /* eb */ g_Opcodes + 2086  ,
  /* ec */ g_Opcodes + 2087  ,
  /* ed */ g_Opcodes + 2088  ,
  /* ee */ g_Opcodes + 2089  ,
  /* ef */ g_Opcodes + 2090  ,
  /* f0 */ g_Opcodes + 2091  ,
  /* f1 */ g_Opcodes + 2092  ,
  /* f2 */ g_Opcodes + 2093  ,
  /* f3 */ g_Opcodes + 2094  ,
  /* f4 */ g_Opcodes + 2095  ,
  /* f5 */ g_Opcodes + 2096  ,
  /* f6 */ g_Opcodes + 2097  ,
  /* f7 */ g_Opcodes + 2098  ,
  /* f8 */ g_Opcodes + 2099  ,
  /* f9 */ g_Opcodes + 2100  ,
  /* fa */ g_Opcodes + 2101  ,
  /* fb */ g_Opcodes + 2102  ,
  /* fc */ g_Opcodes + 2103  ,
  /* fd */ g_Opcodes + 2104  ,
  /* fe */ g_Opcodes + 2105  ,
  /* ff */ g_Opcodes + 2106  ,
},
/* PrefixDE */
{
  /* 00 */ NULL  ,
  /* 01 */ NULL  ,
  /* 02 */ NULL  ,
  /* 03 */ NULL  ,
  /* 04 */ NULL  ,
  /* 05 */ NULL  ,
  /* 06 */ NULL  ,
  /* 07 */ NULL  ,
  /* 08 */ NULL  ,
  /* 09 */ NULL  ,
  /* 0a */ NULL  ,
  /* 0b */ NULL  ,
  /* 0c */ NULL  ,
  /* 0d */ NULL  ,
  /* 0e */ NULL  ,
  /* 0f */ NULL  ,
  /* 10 */ NULL  ,
  /* 11 */ NULL  ,
  /* 12 */ NULL  ,
  /* 13 */ NULL  ,
  /* 14 */ NULL  ,
  /* 15 */ NULL  ,
  /* 16 */ NULL  ,
  /* 17 */ NULL  ,
  /* 18 */ NULL  ,
  /* 19 */ NULL  ,
  /* 1a */ NULL  ,
  /* 1b */ NULL  ,
  /* 1c */ NULL  ,
  /* 1d */ NULL  ,
  /* 1e */ NULL  ,
  /* 1f */ NULL  ,
  /* 20 */ NULL  ,
  /* 21 */ NULL  ,
  /* 22 */ NULL  ,
  /* 23 */ NULL  ,
  /* 24 */ NULL  ,
  /* 25 */ NULL  ,
  /* 26 */ NULL  ,
  /* 27 */ NULL  ,
  /* 28 */ NULL  ,
  /* 29 */ NULL  ,
  /* 2a */ NULL  ,
  /* 2b */ NULL  ,
  /* 2c */ NULL  ,
  /* 2d */ NULL  ,
  /* 2e */ NULL  ,
  /* 2f */ NULL  ,
  /* 30 */ NULL  ,
  /* 31 */ NULL  ,
  /* 32 */ NULL  ,
  /* 33 */ NULL  ,
  /* 34 */ NULL  ,
  /* 35 */ NULL  ,
  /* 36 */ NULL  ,
  /* 37 */ NULL  ,
  /* 38 */ NULL  ,
  /* 39 */ NULL  ,
  /* 3a */ NULL  ,
  /* 3b */ NULL  ,
  /* 3c */ NULL  ,
  /* 3d */ NULL  ,
  /* 3e */ NULL  ,
  /* 3f */ NULL  ,
  /* 40 */ NULL  ,
  /* 41 */ NULL  ,
  /* 42 */ NULL  ,
  /* 43 */ NULL  ,
  /* 44 */ NULL  ,
  /* 45 */ NULL  ,
  /* 46 */ NULL  ,
  /* 47 */ NULL  ,
  /* 48 */ NULL  ,
  /* 49 */ NULL  ,
  /* 4a */ NULL  ,
  /* 4b */ NULL  ,
  /* 4c */ NULL  ,
  /* 4d */ NULL  ,
  /* 4e */ NULL  ,
  /* 4f */ NULL  ,
  /* 50 */ NULL  ,
  /* 51 */ NULL  ,
  /* 52 */ NULL  ,
  /* 53 */ NULL  ,
  /* 54 */ NULL  ,
  /* 55 */ NULL  ,
  /* 56 */ NULL  ,
  /* 57 */ NULL  ,
  /* 58 */ NULL  ,
  /* 59 */ NULL  ,
  /* 5a */ NULL  ,
  /* 5b */ NULL  ,
  /* 5c */ NULL  ,
  /* 5d */ NULL  ,
  /* 5e */ NULL  ,
  /* 5f */ NULL  ,
  /* 60 */ NULL  ,
  /* 61 */ NULL  ,
  /* 62 */ NULL  ,
  /* 63 */ NULL  ,
  /* 64 */ NULL  ,
  /* 65 */ NULL  ,
  /* 66 */ NULL  ,
  /* 67 */ NULL  ,
  /* 68 */ NULL  ,
  /* 69 */ NULL  ,
  /* 6a */ NULL  ,
  /* 6b */ NULL  ,
  /* 6c */ NULL  ,
  /* 6d */ NULL  ,
  /* 6e */ NULL  ,
  /* 6f */ NULL  ,
  /* 70 */ NULL  ,
  /* 71 */ NULL  ,
  /* 72 */ NULL  ,
  /* 73 */ NULL  ,
  /* 74 */ NULL  ,
  /* 75 */ NULL  ,
  /* 76 */ NULL  ,
  /* 77 */ NULL  ,
  /* 78 */ NULL  ,
  /* 79 */ NULL  ,
  /* 7a */ NULL  ,
  /* 7b */ NULL  ,
  /* 7c */ NULL  ,
  /* 7d */ NULL  ,
  /* 7e */ NULL  ,
  /* 7f */ NULL  ,
  /* 80 */ NULL  ,
  /* 81 */ NULL  ,
  /* 82 */ NULL  ,
  /* 83 */ NULL  ,
  /* 84 */ NULL  ,
  /* 85 */ NULL  ,
  /* 86 */ NULL  ,
  /* 87 */ NULL  ,
  /* 88 */ NULL  ,
  /* 89 */ NULL  ,
  /* 8a */ NULL  ,
  /* 8b */ NULL  ,
  /* 8c */ NULL  ,
  /* 8d */ NULL  ,
  /* 8e */ NULL  ,
  /* 8f */ NULL  ,
  /* 90 */ NULL  ,
  /* 91 */ NULL  ,
  /* 92 */ NULL  ,
  /* 93 */ NULL  ,
  /* 94 */ NULL  ,
  /* 95 */ NULL  ,
  /* 96 */ NULL  ,
  /* 97 */ NULL  ,
  /* 98 */ NULL  ,
  /* 99 */ NULL  ,
  /* 9a */ NULL  ,
  /* 9b */ NULL  ,
  /* 9c */ NULL  ,
  /* 9d */ NULL  ,
  /* 9e */ NULL  ,
  /* 9f */ NULL  ,
  /* a0 */ NULL  ,
  /* a1 */ NULL  ,
  /* a2 */ NULL  ,
  /* a3 */ NULL  ,
  /* a4 */ NULL  ,
  /* a5 */ NULL  ,
  /* a6 */ NULL  ,
  /* a7 */ NULL  ,
  /* a8 */ NULL  ,
  /* a9 */ NULL  ,
  /* aa */ NULL  ,
  /* ab */ NULL  ,
  /* ac */ NULL  ,
  /* ad */ NULL  ,
  /* ae */ NULL  ,
  /* af */ NULL  ,
  /* b0 */ NULL  ,
  /* b1 */ NULL  ,
  /* b2 */ NULL  ,
  /* b3 */ NULL  ,
  /* b4 */ NULL  ,
  /* b5 */ NULL  ,
  /* b6 */ NULL  ,
  /* b7 */ NULL  ,
  /* b8 */ NULL  ,
  /* b9 */ NULL  ,
  /* ba */ NULL  ,
  /* bb */ NULL  ,
  /* bc */ NULL  ,
  /* bd */ NULL  ,
  /* be */ NULL  ,
  /* bf */ NULL  ,
  /* c0 */ g_Opcodes + 2107  ,
  /* c1 */ g_Opcodes + 2108  ,
  /* c2 */ g_Opcodes + 2109  ,
  /* c3 */ g_Opcodes + 2110  ,
  /* c4 */ g_Opcodes + 2111  ,
  /* c5 */ g_Opcodes + 2112  ,
  /* c6 */ g_Opcodes + 2113  ,
  /* c7 */ g_Opcodes + 2114  ,
  /* c8 */ g_Opcodes + 2115  ,
  /* c9 */ g_Opcodes + 2116  ,
  /* ca */ g_Opcodes + 2117  ,
  /* cb */ g_Opcodes + 2118  ,
  /* cc */ g_Opcodes + 2119  ,
  /* cd */ g_Opcodes + 2120  ,
  /* ce */ g_Opcodes + 2121  ,
  /* cf */ g_Opcodes + 2122  ,
  /* d0 */ g_Opcodes + 2123  ,
  /* d1 */ g_Opcodes + 2124  ,
  /* d2 */ g_Opcodes + 2125  ,
  /* d3 */ g_Opcodes + 2126  ,
  /* d4 */ g_Opcodes + 2127  ,
  /* d5 */ g_Opcodes + 2128  ,
  /* d6 */ g_Opcodes + 2129  ,
  /* d7 */ g_Opcodes + 2130  ,
  /* d8 */ g_Opcodes + 2131  ,
  /* d9 */ g_Opcodes + 2132  ,
  /* da */ g_Opcodes + 2133  ,
  /* db */ g_Opcodes + 2134  ,
  /* dc */ g_Opcodes + 2135  ,
  /* dd */ g_Opcodes + 2136  ,
  /* de */ g_Opcodes + 2137  ,
  /* df */ g_Opcodes + 2138  ,
  /* e0 */ g_Opcodes + 2139  ,
  /* e1 */ g_Opcodes + 2140  ,
  /* e2 */ g_Opcodes + 2141  ,
  /* e3 */ g_Opcodes + 2142  ,
  /* e4 */ g_Opcodes + 2143  ,
  /* e5 */ g_Opcodes + 2144  ,
  /* e6 */ g_Opcodes + 2145  ,
  /* e7 */ g_Opcodes + 2146  ,
  /* e8 */ g_Opcodes + 2147  ,
  /* e9 */ g_Opcodes + 2148  ,
  /* ea */ g_Opcodes + 2149  ,
  /* eb */ g_Opcodes + 2150  ,
  /* ec */ g_Opcodes + 2151  ,
  /* ed */ g_Opcodes + 2152  ,
  /* ee */ g_Opcodes + 2153  ,
  /* ef */ g_Opcodes + 2154  ,
  /* f0 */ g_Opcodes + 2155  ,
  /* f1 */ g_Opcodes + 2156  ,
  /* f2 */ g_Opcodes + 2157  ,
  /* f3 */ g_Opcodes + 2158  ,
  /* f4 */ g_Opcodes + 2159  ,
  /* f5 */ g_Opcodes + 2160  ,
  /* f6 */ g_Opcodes + 2161  ,
  /* f7 */ g_Opcodes + 2162  ,
  /* f8 */ g_Opcodes + 2163  ,
  /* f9 */ g_Opcodes + 2164  ,
  /* fa */ g_Opcodes + 2165  ,
  /* fb */ g_Opcodes + 2166  ,
  /* fc */ g_Opcodes + 2167  ,
  /* fd */ g_Opcodes + 2168  ,
  /* fe */ g_Opcodes + 2169  ,
  /* ff */ g_Opcodes + 2170  ,
},
/* PrefixDF */
{
  /* 00 */ NULL  ,
  /* 01 */ NULL  ,
  /* 02 */ NULL  ,
  /* 03 */ NULL  ,
  /* 04 */ NULL  ,
  /* 05 */ NULL  ,
  /* 06 */ NULL  ,
  /* 07 */ NULL  ,
  /* 08 */ NULL  ,
  /* 09 */ NULL  ,
  /* 0a */ NULL  ,
  /* 0b */ NULL  ,
  /* 0c */ NULL  ,
  /* 0d */ NULL  ,
  /* 0e */ NULL  ,
  /* 0f */ NULL  ,
  /* 10 */ NULL  ,
  /* 11 */ NULL  ,
  /* 12 */ NULL  ,
  /* 13 */ NULL  ,
  /* 14 */ NULL  ,
  /* 15 */ NULL  ,
  /* 16 */ NULL  ,
  /* 17 */ NULL  ,
  /* 18 */ NULL  ,
  /* 19 */ NULL  ,
  /* 1a */ NULL  ,
  /* 1b */ NULL  ,
  /* 1c */ NULL  ,
  /* 1d */ NULL  ,
  /* 1e */ NULL  ,
  /* 1f */ NULL  ,
  /* 20 */ NULL  ,
  /* 21 */ NULL  ,
  /* 22 */ NULL  ,
  /* 23 */ NULL  ,
  /* 24 */ NULL  ,
  /* 25 */ NULL  ,
  /* 26 */ NULL  ,
  /* 27 */ NULL  ,
  /* 28 */ NULL  ,
  /* 29 */ NULL  ,
  /* 2a */ NULL  ,
  /* 2b */ NULL  ,
  /* 2c */ NULL  ,
  /* 2d */ NULL  ,
  /* 2e */ NULL  ,
  /* 2f */ NULL  ,
  /* 30 */ NULL  ,
  /* 31 */ NULL  ,
  /* 32 */ NULL  ,
  /* 33 */ NULL  ,
  /* 34 */ NULL  ,
  /* 35 */ NULL  ,
  /* 36 */ NULL  ,
  /* 37 */ NULL  ,
  /* 38 */ NULL  ,
  /* 39 */ NULL  ,
  /* 3a */ NULL  ,
  /* 3b */ NULL  ,
  /* 3c */ NULL  ,
  /* 3d */ NULL  ,
  /* 3e */ NULL  ,
  /* 3f */ NULL  ,
  /* 40 */ NULL  ,
  /* 41 */ NULL  ,
  /* 42 */ NULL  ,
  /* 43 */ NULL  ,
  /* 44 */ NULL  ,
  /* 45 */ NULL  ,
  /* 46 */ NULL  ,
  /* 47 */ NULL  ,
  /* 48 */ NULL  ,
  /* 49 */ NULL  ,
  /* 4a */ NULL  ,
  /* 4b */ NULL  ,
  /* 4c */ NULL  ,
  /* 4d */ NULL  ,
  /* 4e */ NULL  ,
  /* 4f */ NULL  ,
  /* 50 */ NULL  ,
  /* 51 */ NULL  ,
  /* 52 */ NULL  ,
  /* 53 */ NULL  ,
  /* 54 */ NULL  ,
  /* 55 */ NULL  ,
  /* 56 */ NULL  ,
  /* 57 */ NULL  ,
  /* 58 */ NULL  ,
  /* 59 */ NULL  ,
  /* 5a */ NULL  ,
  /* 5b */ NULL  ,
  /* 5c */ NULL  ,
  /* 5d */ NULL  ,
  /* 5e */ NULL  ,
  /* 5f */ NULL  ,
  /* 60 */ NULL  ,
  /* 61 */ NULL  ,
  /* 62 */ NULL  ,
  /* 63 */ NULL  ,
  /* 64 */ NULL  ,
  /* 65 */ NULL  ,
  /* 66 */ NULL  ,
  /* 67 */ NULL  ,
  /* 68 */ NULL  ,
  /* 69 */ NULL  ,
  /* 6a */ NULL  ,
  /* 6b */ NULL  ,
  /* 6c */ NULL  ,
  /* 6d */ NULL  ,
  /* 6e */ NULL  ,
  /* 6f */ NULL  ,
  /* 70 */ NULL  ,
  /* 71 */ NULL  ,
  /* 72 */ NULL  ,
  /* 73 */ NULL  ,
  /* 74 */ NULL  ,
  /* 75 */ NULL  ,
  /* 76 */ NULL  ,
  /* 77 */ NULL  ,
  /* 78 */ NULL  ,
  /* 79 */ NULL  ,
  /* 7a */ NULL  ,
  /* 7b */ NULL  ,
  /* 7c */ NULL  ,
  /* 7d */ NULL  ,
  /* 7e */ NULL  ,
  /* 7f */ NULL  ,
  /* 80 */ NULL  ,
  /* 81 */ NULL  ,
  /* 82 */ NULL  ,
  /* 83 */ NULL  ,
  /* 84 */ NULL  ,
  /* 85 */ NULL  ,
  /* 86 */ NULL  ,
  /* 87 */ NULL  ,
  /* 88 */ NULL  ,
  /* 89 */ NULL  ,
  /* 8a */ NULL  ,
  /* 8b */ NULL  ,
  /* 8c */ NULL  ,
  /* 8d */ NULL  ,
  /* 8e */ NULL  ,
  /* 8f */ NULL  ,
  /* 90 */ NULL  ,
  /* 91 */ NULL  ,
  /* 92 */ NULL  ,
  /* 93 */ NULL  ,
  /* 94 */ NULL  ,
  /* 95 */ NULL  ,
  /* 96 */ NULL  ,
  /* 97 */ NULL  ,
  /* 98 */ NULL  ,
  /* 99 */ NULL  ,
  /* 9a */ NULL  ,
  /* 9b */ NULL  ,
  /* 9c */ NULL  ,
  /* 9d */ NULL  ,
  /* 9e */ NULL  ,
  /* 9f */ NULL  ,
  /* a0 */ NULL  ,
  /* a1 */ NULL  ,
  /* a2 */ NULL  ,
  /* a3 */ NULL  ,
  /* a4 */ NULL  ,
  /* a5 */ NULL  ,
  /* a6 */ NULL  ,
  /* a7 */ NULL  ,
  /* a8 */ NULL  ,
  /* a9 */ NULL  ,
  /* aa */ NULL  ,
  /* ab */ NULL  ,
  /* ac */ NULL  ,
  /* ad */ NULL  ,
  /* ae */ NULL  ,
  /* af */ NULL  ,
  /* b0 */ NULL  ,
  /* b1 */ NULL  ,
  /* b2 */ NULL  ,
  /* b3 */ NULL  ,
  /* b4 */ NULL  ,
  /* b5 */ NULL  ,
  /* b6 */ NULL  ,
  /* b7 */ NULL  ,
  /* b8 */ NULL  ,
  /* b9 */ NULL  ,
  /* ba */ NULL  ,
  /* bb */ NULL  ,
  /* bc */ NULL  ,
  /* bd */ NULL  ,
  /* be */ NULL  ,
  /* bf */ NULL  ,
  /* c0 */ g_Opcodes + 2171  ,
  /* c1 */ g_Opcodes + 2172  ,
  /* c2 */ g_Opcodes + 2173  ,
  /* c3 */ g_Opcodes + 2174  ,
  /* c4 */ g_Opcodes + 2175  ,
  /* c5 */ g_Opcodes + 2176  ,
  /* c6 */ g_Opcodes + 2177  ,
  /* c7 */ g_Opcodes + 2178  ,
  /* c8 */ g_Opcodes + 2179  ,
  /* c9 */ g_Opcodes + 2180  ,
  /* ca */ g_Opcodes + 2181  ,
  /* cb */ g_Opcodes + 2182  ,
  /* cc */ g_Opcodes + 2183  ,
  /* cd */ g_Opcodes + 2184  ,
  /* ce */ g_Opcodes + 2185  ,
  /* cf */ g_Opcodes + 2186  ,
  /* d0 */ g_Opcodes + 2187  ,
  /* d1 */ g_Opcodes + 2188  ,
  /* d2 */ g_Opcodes + 2189  ,
  /* d3 */ g_Opcodes + 2190  ,
  /* d4 */ g_Opcodes + 2191  ,
  /* d5 */ g_Opcodes + 2192  ,
  /* d6 */ g_Opcodes + 2193  ,
  /* d7 */ g_Opcodes + 2194  ,
  /* d8 */ g_Opcodes + 2195  ,
  /* d9 */ g_Opcodes + 2196  ,
  /* da */ g_Opcodes + 2197  ,
  /* db */ g_Opcodes + 2198  ,
  /* dc */ g_Opcodes + 2199  ,
  /* dd */ g_Opcodes + 2200  ,
  /* de */ g_Opcodes + 2201  ,
  /* df */ g_Opcodes + 2202  ,
  /* e0 */ g_Opcodes + 2203  ,
  /* e1 */ g_Opcodes + 2204  ,
  /* e2 */ g_Opcodes + 2205  ,
  /* e3 */ g_Opcodes + 2206  ,
  /* e4 */ g_Opcodes + 2207  ,
  /* e5 */ g_Opcodes + 2208  ,
  /* e6 */ g_Opcodes + 2209  ,
  /* e7 */ g_Opcodes + 2210  ,
  /* e8 */ g_Opcodes + 2211  ,
  /* e9 */ g_Opcodes + 2212  ,
  /* ea */ g_Opcodes + 2213  ,
  /* eb */ g_Opcodes + 2214  ,
  /* ec */ g_Opcodes + 2215  ,
  /* ed */ g_Opcodes + 2216  ,
  /* ee */ g_Opcodes + 2217  ,
  /* ef */ g_Opcodes + 2218  ,
  /* f0 */ g_Opcodes + 2219  ,
  /* f1 */ g_Opcodes + 2220  ,
  /* f2 */ g_Opcodes + 2221  ,
  /* f3 */ g_Opcodes + 2222  ,
  /* f4 */ g_Opcodes + 2223  ,
  /* f5 */ g_Opcodes + 2224  ,
  /* f6 */ g_Opcodes + 2225  ,
  /* f7 */ g_Opcodes + 2226  ,
  /* f8 */ g_Opcodes + 2227  ,
  /* f9 */ g_Opcodes + 2228  ,
  /* fa */ g_Opcodes + 2229  ,
  /* fb */ g_Opcodes + 2230  ,
  /* fc */ g_Opcodes + 2231  ,
  /* fd */ g_Opcodes + 2232  ,
  /* fe */ g_Opcodes + 2233  ,
  /* ff */ g_Opcodes + 2234  ,
},
};

static const uint32_t kNaClPrefixTable[NCDTABLESIZE] = {
  /* 0x00-0x0f */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
  /* 0x10-0x1f */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
  /* 0x20-0x2f */
  0, 0, 0, 0, 0, 0, kPrefixSEGES, 0, 0, 0, 0, 0, 0, 0, kPrefixSEGCS, 0, 
  /* 0x30-0x3f */
  0, 0, 0, 0, 0, 0, kPrefixSEGSS, 0, 0, 0, 0, 0, 0, 0, kPrefixSEGDS, 0, 
  /* 0x40-0x4f */
  kPrefixREX, kPrefixREX, kPrefixREX, kPrefixREX, kPrefixREX, kPrefixREX, kPrefixREX, kPrefixREX, kPrefixREX, kPrefixREX, kPrefixREX, kPrefixREX, kPrefixREX, kPrefixREX, kPrefixREX, kPrefixREX, 
  /* 0x50-0x5f */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
  /* 0x60-0x6f */
  0, 0, 0, 0, kPrefixSEGFS, kPrefixSEGGS, kPrefixDATA16, kPrefixADDR16, 0, 0, 0, 0, 0, 0, 0, 0, 
  /* 0x70-0x7f */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
  /* 0x80-0x8f */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
  /* 0x90-0x9f */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
  /* 0xa0-0xaf */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
  /* 0xb0-0xbf */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
  /* 0xc0-0xcf */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
  /* 0xd0-0xdf */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
  /* 0xe0-0xef */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
  /* 0xf0-0xff */
  kPrefixLOCK, 0, kPrefixREPNE, kPrefixREP, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
};

static const NaClInstNode g_OpcodeSeq[96] = {
  /* 0 */
  { 0x0f,
    NULL,
    g_OpcodeSeq + 1,
    g_OpcodeSeq + 20,
  },
  /* 1 */
  { 0x0b,
    g_Opcodes + 2235,
    NULL,
    g_OpcodeSeq + 2,
  },
  /* 2 */
  { 0x1f,
    NULL,
    g_OpcodeSeq + 3,
    NULL,
  },
  /* 3 */
  { 0x00,
    g_Opcodes + 2236,
    NULL,
    g_OpcodeSeq + 4,
  },
  /* 4 */
  { 0x40,
    NULL,
    g_OpcodeSeq + 5,
    g_OpcodeSeq + 6,
  },
  /* 5 */
  { 0x00,
    g_Opcodes + 2237,
    NULL,
    NULL,
  },
  /* 6 */
  { 0x44,
    NULL,
    g_OpcodeSeq + 7,
    g_OpcodeSeq + 9,
  },
  /* 7 */
  { 0x00,
    NULL,
    g_OpcodeSeq + 8,
    NULL,
  },
  /* 8 */
  { 0x00,
    g_Opcodes + 2238,
    NULL,
    NULL,
  },
  /* 9 */
  { 0x80,
    NULL,
    g_OpcodeSeq + 10,
    g_OpcodeSeq + 14,
  },
  /* 10 */
  { 0x00,
    NULL,
    g_OpcodeSeq + 11,
    NULL,
  },
  /* 11 */
  { 0x00,
    NULL,
    g_OpcodeSeq + 12,
    NULL,
  },
  /* 12 */
  { 0x00,
    NULL,
    g_OpcodeSeq + 13,
    NULL,
  },
  /* 13 */
  { 0x00,
    g_Opcodes + 2239,
    NULL,
    NULL,
  },
  /* 14 */
  { 0x84,
    NULL,
    g_OpcodeSeq + 15,
    NULL,
  },
  /* 15 */
  { 0x00,
    NULL,
    g_OpcodeSeq + 16,
    NULL,
  },
  /* 16 */
  { 0x00,
    NULL,
    g_OpcodeSeq + 17,
    NULL,
  },
  /* 17 */
  { 0x00,
    NULL,
    g_OpcodeSeq + 18,
    NULL,
  },
  /* 18 */
  { 0x00,
    NULL,
    g_OpcodeSeq + 19,
    NULL,
  },
  /* 19 */
  { 0x00,
    g_Opcodes + 2240,
    NULL,
    NULL,
  },
  /* 20 */
  { 0x66,
    NULL,
    g_OpcodeSeq + 21,
    g_OpcodeSeq + 93,
  },
  /* 21 */
  { 0x0f,
    NULL,
    g_OpcodeSeq + 22,
    g_OpcodeSeq + 32,
  },
  /* 22 */
  { 0x1f,
    NULL,
    g_OpcodeSeq + 23,
    NULL,
  },
  /* 23 */
  { 0x44,
    NULL,
    g_OpcodeSeq + 24,
    g_OpcodeSeq + 26,
  },
  /* 24 */
  { 0x00,
    NULL,
    g_OpcodeSeq + 25,
    NULL,
  },
  /* 25 */
  { 0x00,
    g_Opcodes + 2241,
    NULL,
    NULL,
  },
  /* 26 */
  { 0x84,
    NULL,
    g_OpcodeSeq + 27,
    NULL,
  },
  /* 27 */
  { 0x00,
    NULL,
    g_OpcodeSeq + 28,
    NULL,
  },
  /* 28 */
  { 0x00,
    NULL,
    g_OpcodeSeq + 29,
    NULL,
  },
  /* 29 */
  { 0x00,
    NULL,
    g_OpcodeSeq + 30,
    NULL,
  },
  /* 30 */
  { 0x00,
    NULL,
    g_OpcodeSeq + 31,
    NULL,
  },
  /* 31 */
  { 0x00,
    g_Opcodes + 2242,
    NULL,
    NULL,
  },
  /* 32 */
  { 0x2e,
    NULL,
    g_OpcodeSeq + 33,
    g_OpcodeSeq + 41,
  },
  /* 33 */
  { 0x0f,
    NULL,
    g_OpcodeSeq + 34,
    NULL,
  },
  /* 34 */
  { 0x1f,
    NULL,
    g_OpcodeSeq + 35,
    NULL,
  },
  /* 35 */
  { 0x84,
    NULL,
    g_OpcodeSeq + 36,
    NULL,
  },
  /* 36 */
  { 0x00,
    NULL,
    g_OpcodeSeq + 37,
    NULL,
  },
  /* 37 */
  { 0x00,
    NULL,
    g_OpcodeSeq + 38,
    NULL,
  },
  /* 38 */
  { 0x00,
    NULL,
    g_OpcodeSeq + 39,
    NULL,
  },
  /* 39 */
  { 0x00,
    NULL,
    g_OpcodeSeq + 40,
    NULL,
  },
  /* 40 */
  { 0x00,
    g_Opcodes + 2243,
    NULL,
    NULL,
  },
  /* 41 */
  { 0x66,
    NULL,
    g_OpcodeSeq + 42,
    g_OpcodeSeq + 92,
  },
  /* 42 */
  { 0x2e,
    NULL,
    g_OpcodeSeq + 43,
    g_OpcodeSeq + 51,
  },
  /* 43 */
  { 0x0f,
    NULL,
    g_OpcodeSeq + 44,
    NULL,
  },
  /* 44 */
  { 0x1f,
    NULL,
    g_OpcodeSeq + 45,
    NULL,
  },
  /* 45 */
  { 0x84,
    NULL,
    g_OpcodeSeq + 46,
    NULL,
  },
  /* 46 */
  { 0x00,
    NULL,
    g_OpcodeSeq + 47,
    NULL,
  },
  /* 47 */
  { 0x00,
    NULL,
    g_OpcodeSeq + 48,
    NULL,
  },
  /* 48 */
  { 0x00,
    NULL,
    g_OpcodeSeq + 49,
    NULL,
  },
  /* 49 */
  { 0x00,
    NULL,
    g_OpcodeSeq + 50,
    NULL,
  },
  /* 50 */
  { 0x00,
    g_Opcodes + 2244,
    NULL,
    NULL,
  },
  /* 51 */
  { 0x66,
    NULL,
    g_OpcodeSeq + 52,
    g_OpcodeSeq + 91,
  },
  /* 52 */
  { 0x2e,
    NULL,
    g_OpcodeSeq + 53,
    g_OpcodeSeq + 61,
  },
  /* 53 */
  { 0x0f,
    NULL,
    g_OpcodeSeq + 54,
    NULL,
  },
  /* 54 */
  { 0x1f,
    NULL,
    g_OpcodeSeq + 55,
    NULL,
  },
  /* 55 */
  { 0x84,
    NULL,
    g_OpcodeSeq + 56,
    NULL,
  },
  /* 56 */
  { 0x00,
    NULL,
    g_OpcodeSeq + 57,
    NULL,
  },
  /* 57 */
  { 0x00,
    NULL,
    g_OpcodeSeq + 58,
    NULL,
  },
  /* 58 */
  { 0x00,
    NULL,
    g_OpcodeSeq + 59,
    NULL,
  },
  /* 59 */
  { 0x00,
    NULL,
    g_OpcodeSeq + 60,
    NULL,
  },
  /* 60 */
  { 0x00,
    g_Opcodes + 2245,
    NULL,
    NULL,
  },
  /* 61 */
  { 0x66,
    NULL,
    g_OpcodeSeq + 62,
    NULL,
  },
  /* 62 */
  { 0x2e,
    NULL,
    g_OpcodeSeq + 63,
    g_OpcodeSeq + 71,
  },
  /* 63 */
  { 0x0f,
    NULL,
    g_OpcodeSeq + 64,
    NULL,
  },
  /* 64 */
  { 0x1f,
    NULL,
    g_OpcodeSeq + 65,
    NULL,
  },
  /* 65 */
  { 0x84,
    NULL,
    g_OpcodeSeq + 66,
    NULL,
  },
  /* 66 */
  { 0x00,
    NULL,
    g_OpcodeSeq + 67,
    NULL,
  },
  /* 67 */
  { 0x00,
    NULL,
    g_OpcodeSeq + 68,
    NULL,
  },
  /* 68 */
  { 0x00,
    NULL,
    g_OpcodeSeq + 69,
    NULL,
  },
  /* 69 */
  { 0x00,
    NULL,
    g_OpcodeSeq + 70,
    NULL,
  },
  /* 70 */
  { 0x00,
    g_Opcodes + 2246,
    NULL,
    NULL,
  },
  /* 71 */
  { 0x66,
    NULL,
    g_OpcodeSeq + 72,
    NULL,
  },
  /* 72 */
  { 0x2e,
    NULL,
    g_OpcodeSeq + 73,
    g_OpcodeSeq + 81,
  },
  /* 73 */
  { 0x0f,
    NULL,
    g_OpcodeSeq + 74,
    NULL,
  },
  /* 74 */
  { 0x1f,
    NULL,
    g_OpcodeSeq + 75,
    NULL,
  },
  /* 75 */
  { 0x84,
    NULL,
    g_OpcodeSeq + 76,
    NULL,
  },
  /* 76 */
  { 0x00,
    NULL,
    g_OpcodeSeq + 77,
    NULL,
  },
  /* 77 */
  { 0x00,
    NULL,
    g_OpcodeSeq + 78,
    NULL,
  },
  /* 78 */
  { 0x00,
    NULL,
    g_OpcodeSeq + 79,
    NULL,
  },
  /* 79 */
  { 0x00,
    NULL,
    g_OpcodeSeq + 80,
    NULL,
  },
  /* 80 */
  { 0x00,
    g_Opcodes + 2247,
    NULL,
    NULL,
  },
  /* 81 */
  { 0x66,
    NULL,
    g_OpcodeSeq + 82,
    NULL,
  },
  /* 82 */
  { 0x2e,
    NULL,
    g_OpcodeSeq + 83,
    NULL,
  },
  /* 83 */
  { 0x0f,
    NULL,
    g_OpcodeSeq + 84,
    NULL,
  },
  /* 84 */
  { 0x1f,
    NULL,
    g_OpcodeSeq + 85,
    NULL,
  },
  /* 85 */
  { 0x84,
    NULL,
    g_OpcodeSeq + 86,
    NULL,
  },
  /* 86 */
  { 0x00,
    NULL,
    g_OpcodeSeq + 87,
    NULL,
  },
  /* 87 */
  { 0x00,
    NULL,
    g_OpcodeSeq + 88,
    NULL,
  },
  /* 88 */
  { 0x00,
    NULL,
    g_OpcodeSeq + 89,
    NULL,
  },
  /* 89 */
  { 0x00,
    NULL,
    g_OpcodeSeq + 90,
    NULL,
  },
  /* 90 */
  { 0x00,
    g_Opcodes + 2248,
    NULL,
    NULL,
  },
  /* 91 */
  { 0x90,
    g_Opcodes + 2249,
    NULL,
    NULL,
  },
  /* 92 */
  { 0x90,
    g_Opcodes + 2250,
    NULL,
    NULL,
  },
  /* 93 */
  { 0x90,
    g_Opcodes + 2251,
    NULL,
    g_OpcodeSeq + 94,
  },
  /* 94 */
  { 0xf3,
    NULL,
    g_OpcodeSeq + 95,
    NULL,
  },
  /* 95 */
  { 0x90,
    g_Opcodes + 2252,
    NULL,
    NULL,
  },
};
