/*
 * THIS FILE IS AUTO-GENERATED. DO NOT EDIT.
 * Compiled for x86-64 bit mode.
 *
 * You must include ncopcode_desc.h before this file.
 */

static const NaClOp g_Operands[846] = {
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
  /* 12 */ { RegREAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$rAXv" },
  /* 13 */ { I_Operand, NACL_OPFLAG(OpUse), "$Iz" },
  /* 14 */ { E_Operand, NACL_OPFLAG(OpUse), "$Eb" },
  /* 15 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gb" },
  /* 16 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ev" },
  /* 17 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gv" },
  /* 18 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gb" },
  /* 19 */ { E_Operand, NACL_OPFLAG(OpUse), "$Eb" },
  /* 20 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gv" },
  /* 21 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ev" },
  /* 22 */ { RegAL, NACL_OPFLAG(OpUse), "%al" },
  /* 23 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 24 */ { RegREAX, NACL_OPFLAG(OpUse), "$rAXv" },
  /* 25 */ { I_Operand, NACL_OPFLAG(OpUse), "$Iz" },
  /* 26 */ { RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rsp}" },
  /* 27 */ { G_OpcodeBase, NACL_OPFLAG(OpUse), "$r8v" },
  /* 28 */ { RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rsp}" },
  /* 29 */ { G_OpcodeBase, NACL_OPFLAG(OpUse), "$r8v" },
  /* 30 */ { RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rsp}" },
  /* 31 */ { G_OpcodeBase, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$r8v" },
  /* 32 */ { RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rsp}" },
  /* 33 */ { G_OpcodeBase, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$r8v" },
  /* 34 */ { G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$Gv" },
  /* 35 */ { Ev_Operand, NACL_OPFLAG(OpUse), "$Ed" },
  /* 36 */ { RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rsp}" },
  /* 37 */ { I_Operand, NACL_OPFLAG(OpUse), "$Iz" },
  /* 38 */ { G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$Gv" },
  /* 39 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ev" },
  /* 40 */ { I_Operand, NACL_OPFLAG(OpUse), "$Iz" },
  /* 41 */ { RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rsp}" },
  /* 42 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 43 */ { G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$Gv" },
  /* 44 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ev" },
  /* 45 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 46 */ { RegES_EDI, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$Yb}" },
  /* 47 */ { RegDX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%dx}" },
  /* 48 */ { RegES_EDI, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$Yzw}" },
  /* 49 */ { RegDX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%dx}" },
  /* 50 */ { RegES_EDI, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$Yzd}" },
  /* 51 */ { RegDX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%dx}" },
  /* 52 */ { RegDX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%dx}" },
  /* 53 */ { RegDS_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Xb}" },
  /* 54 */ { RegDX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%dx}" },
  /* 55 */ { RegES_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Xzw}" },
  /* 56 */ { RegDX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%dx}" },
  /* 57 */ { RegES_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Xzd}" },
  /* 58 */ { RegRIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rip}" },
  /* 59 */ { J_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandNear) | NACL_OPFLAG(OperandRelative), "$Jb" },
  /* 60 */ { RegRIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rip}" },
  /* 61 */ { J_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandNear) | NACL_OPFLAG(OperandRelative), "$Jb" },
  /* 62 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Eb" },
  /* 63 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 64 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Eb" },
  /* 65 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 66 */ { E_Operand, NACL_OPFLAG(OpUse), "$Eb" },
  /* 67 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 68 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$Ev" },
  /* 69 */ { I_Operand, NACL_OPFLAG(OpUse), "$Iz" },
  /* 70 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$Ev" },
  /* 71 */ { I_Operand, NACL_OPFLAG(OpUse), "$Iz" },
  /* 72 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$Ev" },
  /* 73 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 74 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$Ev" },
  /* 75 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 76 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Eb" },
  /* 77 */ { G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Gb" },
  /* 78 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$Ev" },
  /* 79 */ { G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$Gv" },
  /* 80 */ { E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Eb" },
  /* 81 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gb" },
  /* 82 */ { E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$Ev" },
  /* 83 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gv" },
  /* 84 */ { G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Gb" },
  /* 85 */ { E_Operand, NACL_OPFLAG(OpUse), "$Eb" },
  /* 86 */ { E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$Mw/Rv" },
  /* 87 */ { S_Operand, NACL_OPFLAG(OpUse), "$Sw" },
  /* 88 */ { G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OperandZeroExtends_v), "$Gv" },
  /* 89 */ { M_Operand, NACL_OPFLAG(OpAddress), "$M" },
  /* 90 */ { S_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Sw" },
  /* 91 */ { Ew_Operand, NACL_OPFLAG(OpUse), "$Ew" },
  /* 92 */ { RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rsp}" },
  /* 93 */ { E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Ev" },
  /* 94 */ { G_OpcodeBase, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$r8v" },
  /* 95 */ { RegREAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$rAXv" },
  /* 96 */ { G_OpcodeBase, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$r8v" },
  /* 97 */ { RegREAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$rAXv" },
  /* 98 */ { RegAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%ax}" },
  /* 99 */ { RegAL, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%al}" },
  /* 100 */ { RegEAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit) | NACL_OPFLAG(OperandSignExtends_v), "{%eax}" },
  /* 101 */ { RegAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%ax}" },
  /* 102 */ { RegRAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rax}" },
  /* 103 */ { RegEAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%eax}" },
  /* 104 */ { RegDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%dx}" },
  /* 105 */ { RegAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%ax}" },
  /* 106 */ { RegEDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%edx}" },
  /* 107 */ { RegEAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%eax}" },
  /* 108 */ { RegRDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rdx}" },
  /* 109 */ { RegRAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%rax}" },
  /* 110 */ { RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rsp}" },
  /* 111 */ { RegRFLAGS, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Fvw}" },
  /* 112 */ { RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rsp}" },
  /* 113 */ { RegRFLAGS, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Fvq}" },
  /* 114 */ { RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rsp}" },
  /* 115 */ { RegRFLAGS, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$Fvw}" },
  /* 116 */ { RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rsp}" },
  /* 117 */ { RegRFLAGS, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$Fvq}" },
  /* 118 */ { RegAH, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%ah}" },
  /* 119 */ { RegAH, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%ah}" },
  /* 120 */ { RegAL, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%al" },
  /* 121 */ { O_Operand, NACL_OPFLAG(OpUse), "$Ob" },
  /* 122 */ { RegREAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$rAXv" },
  /* 123 */ { O_Operand, NACL_OPFLAG(OpUse), "$Ov" },
  /* 124 */ { O_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Ob" },
  /* 125 */ { RegAL, NACL_OPFLAG(OpUse), "%al" },
  /* 126 */ { O_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$Ov" },
  /* 127 */ { RegREAX, NACL_OPFLAG(OpUse), "$rAXv" },
  /* 128 */ { RegES_EDI, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$Yb}" },
  /* 129 */ { RegDS_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Xb}" },
  /* 130 */ { RegES_EDI, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$Yvw}" },
  /* 131 */ { RegDS_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Xvw}" },
  /* 132 */ { RegES_EDI, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$Yvd}" },
  /* 133 */ { RegDS_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Xvd}" },
  /* 134 */ { RegES_EDI, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$Yvq}" },
  /* 135 */ { RegDS_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Xvq}" },
  /* 136 */ { RegDS_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Xb}" },
  /* 137 */ { RegES_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Yb}" },
  /* 138 */ { RegDS_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Xvw}" },
  /* 139 */ { RegES_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Yvw}" },
  /* 140 */ { RegDS_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Xvd}" },
  /* 141 */ { RegES_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Yvd}" },
  /* 142 */ { RegDS_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Xvq}" },
  /* 143 */ { RegES_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Yvq}" },
  /* 144 */ { RegES_EDI, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$Yb}" },
  /* 145 */ { RegAL, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%al}" },
  /* 146 */ { RegES_EDI, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$Yvw}" },
  /* 147 */ { RegAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$rAXvw}" },
  /* 148 */ { RegES_EDI, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$Yvd}" },
  /* 149 */ { RegEAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$rAXvd}" },
  /* 150 */ { RegES_EDI, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$Yvq}" },
  /* 151 */ { RegRAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$rAXvq}" },
  /* 152 */ { RegAL, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%al}" },
  /* 153 */ { RegDS_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Xb}" },
  /* 154 */ { RegAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$rAXvw}" },
  /* 155 */ { RegDS_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Xvw}" },
  /* 156 */ { RegEAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$rAXvd}" },
  /* 157 */ { RegDS_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Xvd}" },
  /* 158 */ { RegRAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$rAXvq}" },
  /* 159 */ { RegDS_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Xvq}" },
  /* 160 */ { RegAL, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%al}" },
  /* 161 */ { RegES_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Yb}" },
  /* 162 */ { RegAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$rAXvw}" },
  /* 163 */ { RegES_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Yvw}" },
  /* 164 */ { RegEAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$rAXvd}" },
  /* 165 */ { RegES_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Yvd}" },
  /* 166 */ { RegRAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$rAXvq}" },
  /* 167 */ { RegES_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Yvq}" },
  /* 168 */ { G_OpcodeBase, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$r8b" },
  /* 169 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 170 */ { G_OpcodeBase, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$r8b" },
  /* 171 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 172 */ { G_OpcodeBase, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$r8v" },
  /* 173 */ { I_Operand, NACL_OPFLAG(OpUse), "$Iv" },
  /* 174 */ { G_OpcodeBase, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$r8v" },
  /* 175 */ { I_Operand, NACL_OPFLAG(OpUse), "$Iv" },
  /* 176 */ { RegRIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rip}" },
  /* 177 */ { RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rsp}" },
  /* 178 */ { I_Operand, NACL_OPFLAG(OpUse), "$Iw" },
  /* 179 */ { E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Eb" },
  /* 180 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 181 */ { E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$Ev" },
  /* 182 */ { I_Operand, NACL_OPFLAG(OpUse), "$Iz" },
  /* 183 */ { RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rsp}" },
  /* 184 */ { RegRBP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rbp}" },
  /* 185 */ { I_Operand, NACL_OPFLAG(OpUse), "$Iw" },
  /* 186 */ { I2_Operand, NACL_OPFLAG(OpUse), "$I2b" },
  /* 187 */ { RegRSP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rsp}" },
  /* 188 */ { RegRBP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rbp}" },
  /* 189 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Eb" },
  /* 190 */ { Const_1, NACL_OPFLAG(OpUse), "1" },
  /* 191 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Eb" },
  /* 192 */ { Const_1, NACL_OPFLAG(OpUse), "1" },
  /* 193 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$Ev" },
  /* 194 */ { Const_1, NACL_OPFLAG(OpUse), "1" },
  /* 195 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$Ev" },
  /* 196 */ { Const_1, NACL_OPFLAG(OpUse), "1" },
  /* 197 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Eb" },
  /* 198 */ { RegCL, NACL_OPFLAG(OpUse), "%cl" },
  /* 199 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Eb" },
  /* 200 */ { RegCL, NACL_OPFLAG(OpUse), "%cl" },
  /* 201 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$Ev" },
  /* 202 */ { RegCL, NACL_OPFLAG(OpUse), "%cl" },
  /* 203 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$Ev" },
  /* 204 */ { RegCL, NACL_OPFLAG(OpUse), "%cl" },
  /* 205 */ { RegAL, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%al}" },
  /* 206 */ { RegDS_EBX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%DS_EBX}" },
  /* 207 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 208 */ { Mv_Operand, NACL_OPFLAG(OpUse), "$Md" },
  /* 209 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 210 */ { Mv_Operand, NACL_OPFLAG(OpUse), "$Md" },
  /* 211 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 212 */ { Mv_Operand, NACL_OPFLAG(OpUse), "$Md" },
  /* 213 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 214 */ { Mv_Operand, NACL_OPFLAG(OpUse), "$Md" },
  /* 215 */ { RegST0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 216 */ { Mv_Operand, NACL_OPFLAG(OpUse), "$Md" },
  /* 217 */ { Mv_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Md" },
  /* 218 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 219 */ { Mv_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Md" },
  /* 220 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 221 */ { M_Operand, NACL_OPFLAG(OpUse), "$M" },
  /* 222 */ { Mw_Operand, NACL_OPFLAG(OpUse), "$Mw" },
  /* 223 */ { M_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$M" },
  /* 224 */ { Mw_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Mw" },
  /* 225 */ { RegST0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 226 */ { M_Operand, NACL_OPFLAG(OpUse), "$M" },
  /* 227 */ { M_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$M" },
  /* 228 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 229 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 230 */ { Mo_Operand, NACL_OPFLAG(OpUse), "$Mq" },
  /* 231 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 232 */ { Mo_Operand, NACL_OPFLAG(OpUse), "$Mq" },
  /* 233 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 234 */ { Mo_Operand, NACL_OPFLAG(OpUse), "$Mq" },
  /* 235 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 236 */ { Mo_Operand, NACL_OPFLAG(OpUse), "$Mq" },
  /* 237 */ { RegST0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 238 */ { Mo_Operand, NACL_OPFLAG(OpUse), "$Mq" },
  /* 239 */ { Mo_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Mq" },
  /* 240 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 241 */ { Mo_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Mq" },
  /* 242 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 243 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 244 */ { Mw_Operand, NACL_OPFLAG(OpUse), "$Mw" },
  /* 245 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 246 */ { Mw_Operand, NACL_OPFLAG(OpUse), "$Mw" },
  /* 247 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 248 */ { Mw_Operand, NACL_OPFLAG(OpUse), "$Mw" },
  /* 249 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 250 */ { Mw_Operand, NACL_OPFLAG(OpUse), "$Mw" },
  /* 251 */ { RegST0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 252 */ { Mw_Operand, NACL_OPFLAG(OpUse), "$Mw" },
  /* 253 */ { Mw_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Mw" },
  /* 254 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 255 */ { Mw_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Mw" },
  /* 256 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 257 */ { RegRIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rip}" },
  /* 258 */ { RegECX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%ecx}" },
  /* 259 */ { J_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandNear) | NACL_OPFLAG(OperandRelative), "$Jb" },
  /* 260 */ { RegRIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rip}" },
  /* 261 */ { RegRCX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%rcx}" },
  /* 262 */ { J_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandNear) | NACL_OPFLAG(OperandRelative), "$Jb" },
  /* 263 */ { RegAL, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%al" },
  /* 264 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 265 */ { RegREAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$rAXv" },
  /* 266 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 267 */ { I_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Ib" },
  /* 268 */ { RegAL, NACL_OPFLAG(OpUse), "%al" },
  /* 269 */ { I_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Ib" },
  /* 270 */ { RegREAX, NACL_OPFLAG(OpUse), "$rAXv" },
  /* 271 */ { RegRIP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rip}" },
  /* 272 */ { RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rsp}" },
  /* 273 */ { J_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandNear) | NACL_OPFLAG(OperandRelative), "$Jzw" },
  /* 274 */ { RegRIP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rip}" },
  /* 275 */ { RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rsp}" },
  /* 276 */ { J_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandNear) | NACL_OPFLAG(OperandRelative), "$Jz" },
  /* 277 */ { RegRIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rip}" },
  /* 278 */ { J_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandNear) | NACL_OPFLAG(OperandRelative), "$Jz" },
  /* 279 */ { RegAL, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%al" },
  /* 280 */ { RegDX, NACL_OPFLAG(OpUse), "%dx" },
  /* 281 */ { RegREAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$rAXv" },
  /* 282 */ { RegDX, NACL_OPFLAG(OpUse), "%dx" },
  /* 283 */ { RegDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%dx" },
  /* 284 */ { RegAL, NACL_OPFLAG(OpUse), "%al" },
  /* 285 */ { RegDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%dx" },
  /* 286 */ { RegREAX, NACL_OPFLAG(OpUse), "$rAXv" },
  /* 287 */ { RegAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%ax}" },
  /* 288 */ { RegAL, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%al}" },
  /* 289 */ { E_Operand, NACL_OPFLAG(OpUse), "$Eb" },
  /* 290 */ { RegAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%ax}" },
  /* 291 */ { RegAL, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%al}" },
  /* 292 */ { E_Operand, NACL_OPFLAG(OpUse), "$Eb" },
  /* 293 */ { RegREDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%redx}" },
  /* 294 */ { RegREAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%reax}" },
  /* 295 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ev" },
  /* 296 */ { RegREDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%redx}" },
  /* 297 */ { RegREAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%reax}" },
  /* 298 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ev" },
  /* 299 */ { RegRIP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rip}" },
  /* 300 */ { RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rsp}" },
  /* 301 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandNear), "$Ev" },
  /* 302 */ { RegRIP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rip}" },
  /* 303 */ { RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rsp}" },
  /* 304 */ { M_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandFar), "$Mp" },
  /* 305 */ { RegRIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rip}" },
  /* 306 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandNear), "$Ev" },
  /* 307 */ { RegRIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rip}" },
  /* 308 */ { M_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandFar), "$Mp" },
  /* 309 */ { RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rsp}" },
  /* 310 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ev" },
  /* 311 */ { E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Mw/Rv" },
  /* 312 */ { E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Mw/Rv" },
  /* 313 */ { Ew_Operand, NACL_EMPTY_OPFLAGS, "$Ew" },
  /* 314 */ { Ew_Operand, NACL_EMPTY_OPFLAGS, "$Ew" },
  /* 315 */ { M_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Ms" },
  /* 316 */ { M_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Ms" },
  /* 317 */ { RegREAX, NACL_OPFLAG(OpUse), "%reax" },
  /* 318 */ { RegECX, NACL_OPFLAG(OpUse), "%ecx" },
  /* 319 */ { RegEDX, NACL_OPFLAG(OpUse), "%edx" },
  /* 320 */ { RegEAX, NACL_EMPTY_OPFLAGS, "%eax" },
  /* 321 */ { RegECX, NACL_EMPTY_OPFLAGS, "%ecx" },
  /* 322 */ { M_Operand, NACL_OPFLAG(OpUse), "$Ms" },
  /* 323 */ { M_Operand, NACL_OPFLAG(OpUse), "$Ms" },
  /* 324 */ { RegREAXa, NACL_OPFLAG(OpUse), "$rAXva" },
  /* 325 */ { RegREAXa, NACL_OPFLAG(OpUse), "$rAXva" },
  /* 326 */ { RegEIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eip}" },
  /* 327 */ { RegEAX, NACL_OPFLAG(OpUse), "%eax" },
  /* 328 */ { RegREAXa, NACL_OPFLAG(OpUse), "$rAXva" },
  /* 329 */ { RegECX, NACL_OPFLAG(OpUse), "%ecx" },
  /* 330 */ { Mb_Operand, NACL_OPFLAG(OpUse), "$Mb" },
  /* 331 */ { RegGS, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%gs}" },
  /* 332 */ { RegRDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rdx}" },
  /* 333 */ { RegRAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rax}" },
  /* 334 */ { RegRCX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rcx}" },
  /* 335 */ { G_Operand, NACL_EMPTY_OPFLAGS, "$Gv" },
  /* 336 */ { Ew_Operand, NACL_EMPTY_OPFLAGS, "$Ew" },
  /* 337 */ { G_Operand, NACL_EMPTY_OPFLAGS, "$Gv" },
  /* 338 */ { Ew_Operand, NACL_EMPTY_OPFLAGS, "$Ew" },
  /* 339 */ { RegRIP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rip}" },
  /* 340 */ { RegRCX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rcx}" },
  /* 341 */ { Mb_Operand, NACL_EMPTY_OPFLAGS, "$Mb" },
  /* 342 */ { Mb_Operand, NACL_EMPTY_OPFLAGS, "$Mb" },
  /* 343 */ { Mmx_G_Operand, NACL_EMPTY_OPFLAGS, "$Pq" },
  /* 344 */ { Mmx_E_Operand, NACL_EMPTY_OPFLAGS, "$Qq" },
  /* 345 */ { I_Operand, NACL_EMPTY_OPFLAGS, "$Ib" },
  /* 346 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vps" },
  /* 347 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wps" },
  /* 348 */ { Xmm_E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Wps" },
  /* 349 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vps" },
  /* 350 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vps" },
  /* 351 */ { Mo_Operand, NACL_OPFLAG(OpUse), "$Mq" },
  /* 352 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vps" },
  /* 353 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$VRq" },
  /* 354 */ { Mo_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Mq" },
  /* 355 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vps" },
  /* 356 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vps" },
  /* 357 */ { Xmm_Eo_Operand, NACL_OPFLAG(OpUse), "$Wq" },
  /* 358 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vps" },
  /* 359 */ { Xmm_Eo_Operand, NACL_OPFLAG(OpUse), "$Wq" },
  /* 360 */ { Eo_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Rd/q" },
  /* 361 */ { C_Operand, NACL_OPFLAG(OpUse), "$Cd/q" },
  /* 362 */ { Eo_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Rd/q" },
  /* 363 */ { D_Operand, NACL_OPFLAG(OpUse), "$Dd/q" },
  /* 364 */ { C_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Cd/q" },
  /* 365 */ { Eo_Operand, NACL_OPFLAG(OpUse), "$Rd/q" },
  /* 366 */ { D_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Dd/q" },
  /* 367 */ { Eo_Operand, NACL_OPFLAG(OpUse), "$Rd/q" },
  /* 368 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vps" },
  /* 369 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse), "$Qq" },
  /* 370 */ { Mdq_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Mdq" },
  /* 371 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vps" },
  /* 372 */ { Mmx_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Pq" },
  /* 373 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wps" },
  /* 374 */ { Mmx_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Pq" },
  /* 375 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wps" },
  /* 376 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vss" },
  /* 377 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wss" },
  /* 378 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vps" },
  /* 379 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wps" },
  /* 380 */ { RegEAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%eax}" },
  /* 381 */ { RegEDX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%edx}" },
  /* 382 */ { RegECX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%ecx}" },
  /* 383 */ { RegEAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eax}" },
  /* 384 */ { RegEDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%edx}" },
  /* 385 */ { RegEAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eax}" },
  /* 386 */ { RegEDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%edx}" },
  /* 387 */ { RegECX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%ecx}" },
  /* 388 */ { RegEAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eax}" },
  /* 389 */ { RegEDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%edx}" },
  /* 390 */ { RegECX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%ecx}" },
  /* 391 */ { RegEIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eip}" },
  /* 392 */ { RegESP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 393 */ { RegCS, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%cs}" },
  /* 394 */ { RegSS, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%ss}" },
  /* 395 */ { RegEIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eip}" },
  /* 396 */ { RegESP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 397 */ { RegCS, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%cs}" },
  /* 398 */ { RegSS, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%ss}" },
  /* 399 */ { RegEDX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%edx}" },
  /* 400 */ { RegECX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%ecx}" },
  /* 401 */ { Gv_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Gd" },
  /* 402 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$VRps" },
  /* 403 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vps" },
  /* 404 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wps" },
  /* 405 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vps" },
  /* 406 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wps" },
  /* 407 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vpd" },
  /* 408 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wps" },
  /* 409 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vps" },
  /* 410 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 411 */ { Mmx_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Pq" },
  /* 412 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse), "$Qq" },
  /* 413 */ { Mmx_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Pq" },
  /* 414 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse), "$Qq" },
  /* 415 */ { Mmx_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Pq" },
  /* 416 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse), "$Qd" },
  /* 417 */ { Mmx_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Pq" },
  /* 418 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse), "$Qd" },
  /* 419 */ { Mmx_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$Pq" },
  /* 420 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ed/q/d" },
  /* 421 */ { Mmx_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Pq" },
  /* 422 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ed/q/q" },
  /* 423 */ { Mmx_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Pq" },
  /* 424 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse), "$Qq" },
  /* 425 */ { Mmx_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Pq" },
  /* 426 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse), "$Qq" },
  /* 427 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 428 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$PRq" },
  /* 429 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 430 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$PRq" },
  /* 431 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 432 */ { E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$Ed/q/d" },
  /* 433 */ { Mmx_G_Operand, NACL_OPFLAG(OpUse), "$Pd/q/d" },
  /* 434 */ { E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Ed/q/q" },
  /* 435 */ { Mmx_G_Operand, NACL_OPFLAG(OpUse), "$Pd/q/q" },
  /* 436 */ { Mmx_E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Qq" },
  /* 437 */ { Mmx_G_Operand, NACL_OPFLAG(OpUse), "$Pq" },
  /* 438 */ { RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rsp}" },
  /* 439 */ { RegFS, NACL_OPFLAG(OpUse), "%fs" },
  /* 440 */ { RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rsp}" },
  /* 441 */ { RegFS, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%fs" },
  /* 442 */ { RegEBX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%ebx}" },
  /* 443 */ { RegEDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%edx}" },
  /* 444 */ { RegEAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eax}" },
  /* 445 */ { RegECX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%ecx}" },
  /* 446 */ { E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$Ev" },
  /* 447 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gv" },
  /* 448 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 449 */ { E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$Ev" },
  /* 450 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gv" },
  /* 451 */ { RegCL, NACL_OPFLAG(OpUse), "%cl" },
  /* 452 */ { RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rsp}" },
  /* 453 */ { RegGS, NACL_OPFLAG(OpUse), "%gs" },
  /* 454 */ { RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rsp}" },
  /* 455 */ { RegGS, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%gs" },
  /* 456 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$Ev" },
  /* 457 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gv" },
  /* 458 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 459 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$Ev" },
  /* 460 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gv" },
  /* 461 */ { RegCL, NACL_OPFLAG(OpUse), "%cl" },
  /* 462 */ { RegAL, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%al}" },
  /* 463 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Eb" },
  /* 464 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gb" },
  /* 465 */ { RegREAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$rAXv}" },
  /* 466 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$Ev" },
  /* 467 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gv" },
  /* 468 */ { Seg_G_Operand, NACL_OPFLAG(OpSet), "$SGz" },
  /* 469 */ { M_Operand, NACL_OPFLAG(OperandFar), "$Mp" },
  /* 470 */ { Seg_G_Operand, NACL_OPFLAG(OpSet), "$SGz" },
  /* 471 */ { M_Operand, NACL_OPFLAG(OperandFar), "$Mp" },
  /* 472 */ { G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$Gv" },
  /* 473 */ { Eb_Operand, NACL_OPFLAG(OpUse), "$Eb" },
  /* 474 */ { G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$Gv" },
  /* 475 */ { Ew_Operand, NACL_OPFLAG(OpUse), "$Ew" },
  /* 476 */ { G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$Gv" },
  /* 477 */ { Ew_Operand, NACL_OPFLAG(OpUse), "$Ew" },
  /* 478 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vps" },
  /* 479 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wps" },
  /* 480 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 481 */ { M_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Md/q" },
  /* 482 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gd/q" },
  /* 483 */ { Mmx_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Pq" },
  /* 484 */ { E_Operand, NACL_OPFLAG(OpUse), "$Rd/q/Mw" },
  /* 485 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 486 */ { Gv_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Gd" },
  /* 487 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse), "$PRq" },
  /* 488 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 489 */ { RegEDX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%edx}" },
  /* 490 */ { RegEAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eax}" },
  /* 491 */ { Mo_Operand, NACL_OPFLAG(OpUse), "$Mq" },
  /* 492 */ { RegRDX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rdx}" },
  /* 493 */ { RegEAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eax}" },
  /* 494 */ { Mdq_Operand, NACL_OPFLAG(OpUse), "$Mdq" },
  /* 495 */ { Mo_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Mq" },
  /* 496 */ { Mmx_G_Operand, NACL_OPFLAG(OpUse), "$Pq" },
  /* 497 */ { RegDS_EDI, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$Xvd}" },
  /* 498 */ { Mmx_G_Operand, NACL_OPFLAG(OpUse), "$Pq" },
  /* 499 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse), "$PRq" },
  /* 500 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vsd" },
  /* 501 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wsd" },
  /* 502 */ { Xmm_E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Wsd" },
  /* 503 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vsd" },
  /* 504 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vpd" },
  /* 505 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wsd" },
  /* 506 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vsd" },
  /* 507 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ed/q" },
  /* 508 */ { Mo_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Mq" },
  /* 509 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vsd" },
  /* 510 */ { G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Gd/q" },
  /* 511 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wsd" },
  /* 512 */ { G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Gd/q" },
  /* 513 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wsd" },
  /* 514 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vsd" },
  /* 515 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wsd" },
  /* 516 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vsd" },
  /* 517 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wsd" },
  /* 518 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vss" },
  /* 519 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wsd" },
  /* 520 */ { Xmm_Go_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vq" },
  /* 521 */ { Xmm_Eo_Operand, NACL_OPFLAG(OpUse), "$Wq" },
  /* 522 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 523 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 524 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$VRq" },
  /* 525 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 526 */ { I2_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 527 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 528 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$VRdq" },
  /* 529 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vsd" },
  /* 530 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wsd" },
  /* 531 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 532 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vpd" },
  /* 533 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wpd" },
  /* 534 */ { Mmx_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Pq" },
  /* 535 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$VRq" },
  /* 536 */ { Xmm_Go_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vq" },
  /* 537 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wpd" },
  /* 538 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 539 */ { Mdq_Operand, NACL_OPFLAG(OpUse), "$Mdq" },
  /* 540 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vss" },
  /* 541 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wss" },
  /* 542 */ { Xmm_E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Wss" },
  /* 543 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vss" },
  /* 544 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vss" },
  /* 545 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ed/q" },
  /* 546 */ { Mv_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Md" },
  /* 547 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vss" },
  /* 548 */ { G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Gd/q" },
  /* 549 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wss" },
  /* 550 */ { G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Gd/q" },
  /* 551 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wss" },
  /* 552 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vss" },
  /* 553 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wss" },
  /* 554 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vss" },
  /* 555 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wss" },
  /* 556 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vsd" },
  /* 557 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wss" },
  /* 558 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 559 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wps" },
  /* 560 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 561 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 562 */ { Xmm_E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Wdq" },
  /* 563 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 564 */ { G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Gv" },
  /* 565 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ev" },
  /* 566 */ { G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Gv" },
  /* 567 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ev" },
  /* 568 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vss" },
  /* 569 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wss" },
  /* 570 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 571 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 572 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse), "$PRq" },
  /* 573 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vpd" },
  /* 574 */ { Xmm_Eo_Operand, NACL_OPFLAG(OpUse), "$Wq" },
  /* 575 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vpd" },
  /* 576 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wpd" },
  /* 577 */ { Xmm_E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Wpd" },
  /* 578 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vpd" },
  /* 579 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vsd" },
  /* 580 */ { Mo_Operand, NACL_OPFLAG(OpUse), "$Mq" },
  /* 581 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vpd" },
  /* 582 */ { Xmm_Eo_Operand, NACL_OPFLAG(OpUse), "$Wq" },
  /* 583 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vpd" },
  /* 584 */ { Xmm_Eo_Operand, NACL_OPFLAG(OpUse), "$Wq" },
  /* 585 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vpd" },
  /* 586 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse), "$Qq" },
  /* 587 */ { Mdq_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Mdq" },
  /* 588 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vpd" },
  /* 589 */ { Mmx_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Pq" },
  /* 590 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wpd" },
  /* 591 */ { Mmx_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Pq" },
  /* 592 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wpd" },
  /* 593 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vsd" },
  /* 594 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wsd" },
  /* 595 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vpd" },
  /* 596 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wsd" },
  /* 597 */ { Gv_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Gd" },
  /* 598 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$VRpd" },
  /* 599 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vps" },
  /* 600 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wpd" },
  /* 601 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vps" },
  /* 602 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wpd" },
  /* 603 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 604 */ { Xmm_Eo_Operand, NACL_OPFLAG(OpUse), "$Wq" },
  /* 605 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 606 */ { Xmm_Eo_Operand, NACL_OPFLAG(OpUse), "$Wq" },
  /* 607 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 608 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 609 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 610 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 611 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$Vdq" },
  /* 612 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ed/q/d" },
  /* 613 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 614 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ed/q/q" },
  /* 615 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 616 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 617 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 618 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$VRdq" },
  /* 619 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 620 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$VRdq" },
  /* 621 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 622 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(AllowGOperandWithOpcodeInModRm), "$Vdq" },
  /* 623 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 624 */ { I2_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 625 */ { E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(OperandZeroExtends_v), "$Ed/q/d" },
  /* 626 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vd/q/d" },
  /* 627 */ { E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Ed/q/q" },
  /* 628 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vd/q/q" },
  /* 629 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vpd" },
  /* 630 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wpd" },
  /* 631 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 632 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 633 */ { E_Operand, NACL_OPFLAG(OpUse), "$Rd/q/Mw" },
  /* 634 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 635 */ { Gv_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Gd" },
  /* 636 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$VRdq" },
  /* 637 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 638 */ { Xmm_Eo_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Wq" },
  /* 639 */ { Xmm_Go_Operand, NACL_OPFLAG(OpUse), "$Vq" },
  /* 640 */ { Xmm_Go_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vq" },
  /* 641 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 642 */ { Mdq_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Mdq" },
  /* 643 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 644 */ { RegDS_EDI, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$Xvd}" },
  /* 645 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 646 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$VRdq" },
  /* 647 */ { G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Gv" },
  /* 648 */ { M_Operand, NACL_OPFLAG(OpUse), "$Mv" },
  /* 649 */ { M_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Mv" },
  /* 650 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gv" },
  /* 651 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 652 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 653 */ { RegXMM0, NACL_OPFLAG(OpUse), "%xmm0" },
  /* 654 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 655 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 656 */ { RegXMM0, NACL_OPFLAG(OpUse), "%xmm0" },
  /* 657 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 658 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 659 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 660 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Udq/Mq" },
  /* 661 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 662 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Udq/Md" },
  /* 663 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 664 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Udq/Mw" },
  /* 665 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 666 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Udq/Mw" },
  /* 667 */ { Go_Operand, NACL_OPFLAG(OpUse), "$Gq" },
  /* 668 */ { Mdq_Operand, NACL_OPFLAG(OpUse), "$Mdq" },
  /* 669 */ { Go_Operand, NACL_OPFLAG(OpUse), "$Gq" },
  /* 670 */ { Mdq_Operand, NACL_OPFLAG(OpUse), "$Mdq" },
  /* 671 */ { Gv_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Gd" },
  /* 672 */ { E_Operand, NACL_OPFLAG(OpUse), "$Eb" },
  /* 673 */ { Gv_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Gd" },
  /* 674 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ev" },
  /* 675 */ { Mmx_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Pq" },
  /* 676 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse), "$Qq" },
  /* 677 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 678 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vss" },
  /* 679 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wss" },
  /* 680 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 681 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vsd" },
  /* 682 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wsd" },
  /* 683 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 684 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 685 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 686 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 687 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 688 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 689 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 690 */ { Ev_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Rd/Mb" },
  /* 691 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 692 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 693 */ { Ev_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Rd/Mw" },
  /* 694 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 695 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 696 */ { E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Ed/q/d" },
  /* 697 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 698 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 699 */ { E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Ed/q/q" },
  /* 700 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 701 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 702 */ { Ev_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Ed" },
  /* 703 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 704 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 705 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 706 */ { E_Operand, NACL_OPFLAG(OpUse), "$Rd/q/Mb" },
  /* 707 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 708 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 709 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Udq/Md" },
  /* 710 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 711 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 712 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ed/q/d" },
  /* 713 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 714 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 715 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ed/q/q" },
  /* 716 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 717 */ { RegXMM0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%xmm0}" },
  /* 718 */ { RegREAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$rAXv}" },
  /* 719 */ { RegREDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$rDXv}" },
  /* 720 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 721 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 722 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 723 */ { RegRECX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$rCXv}" },
  /* 724 */ { RegREAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$rAXv}" },
  /* 725 */ { RegREDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$rDXv}" },
  /* 726 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 727 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 728 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 729 */ { RegXMM0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%xmm0}" },
  /* 730 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 731 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 732 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 733 */ { RegRECX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$rCXv}" },
  /* 734 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 735 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 736 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 737 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 738 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 739 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 740 */ { RegST1, NACL_OPFLAG(OpUse), "%st1" },
  /* 741 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 742 */ { RegST2, NACL_OPFLAG(OpUse), "%st2" },
  /* 743 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 744 */ { RegST3, NACL_OPFLAG(OpUse), "%st3" },
  /* 745 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 746 */ { RegST4, NACL_OPFLAG(OpUse), "%st4" },
  /* 747 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 748 */ { RegST5, NACL_OPFLAG(OpUse), "%st5" },
  /* 749 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 750 */ { RegST6, NACL_OPFLAG(OpUse), "%st6" },
  /* 751 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 752 */ { RegST7, NACL_OPFLAG(OpUse), "%st7" },
  /* 753 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 754 */ { RegST7, NACL_OPFLAG(OpUse), "%st7" },
  /* 755 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 756 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 757 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 758 */ { RegST1, NACL_OPFLAG(OpUse), "%st1" },
  /* 759 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 760 */ { RegST2, NACL_OPFLAG(OpUse), "%st2" },
  /* 761 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 762 */ { RegST3, NACL_OPFLAG(OpUse), "%st3" },
  /* 763 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 764 */ { RegST4, NACL_OPFLAG(OpUse), "%st4" },
  /* 765 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 766 */ { RegST5, NACL_OPFLAG(OpUse), "%st5" },
  /* 767 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 768 */ { RegST6, NACL_OPFLAG(OpUse), "%st6" },
  /* 769 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 770 */ { RegST7, NACL_OPFLAG(OpUse), "%st7" },
  /* 771 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 772 */ { RegST7, NACL_OPFLAG(OpUse), "%st7" },
  /* 773 */ { RegST0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 774 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 775 */ { RegST0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 776 */ { RegST1, NACL_OPFLAG(OpUse), "%st1" },
  /* 777 */ { RegST0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 778 */ { RegST2, NACL_OPFLAG(OpUse), "%st2" },
  /* 779 */ { RegST0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 780 */ { RegST3, NACL_OPFLAG(OpUse), "%st3" },
  /* 781 */ { RegST0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 782 */ { RegST4, NACL_OPFLAG(OpUse), "%st4" },
  /* 783 */ { RegST0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 784 */ { RegST5, NACL_OPFLAG(OpUse), "%st5" },
  /* 785 */ { RegST0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 786 */ { RegST6, NACL_OPFLAG(OpUse), "%st6" },
  /* 787 */ { RegST0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 788 */ { RegST7, NACL_OPFLAG(OpUse), "%st7" },
  /* 789 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 790 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 791 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 792 */ { RegST1, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st1" },
  /* 793 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 794 */ { RegST2, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st2" },
  /* 795 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 796 */ { RegST3, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st3" },
  /* 797 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 798 */ { RegST4, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st4" },
  /* 799 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 800 */ { RegST5, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st5" },
  /* 801 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 802 */ { RegST6, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st6" },
  /* 803 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 804 */ { RegST7, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st7" },
  /* 805 */ { RegST1, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st1" },
  /* 806 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 807 */ { RegST2, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st2" },
  /* 808 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 809 */ { RegST3, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st3" },
  /* 810 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 811 */ { RegST4, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st4" },
  /* 812 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 813 */ { RegST5, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st5" },
  /* 814 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 815 */ { RegST6, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st6" },
  /* 816 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 817 */ { RegST7, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st7" },
  /* 818 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 819 */ { RegST7, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st7" },
  /* 820 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 821 */ { RegST0, NACL_EMPTY_OPFLAGS, "%st0" },
  /* 822 */ { RegST1, NACL_EMPTY_OPFLAGS, "%st1" },
  /* 823 */ { RegST2, NACL_EMPTY_OPFLAGS, "%st2" },
  /* 824 */ { RegST3, NACL_EMPTY_OPFLAGS, "%st3" },
  /* 825 */ { RegST4, NACL_EMPTY_OPFLAGS, "%st4" },
  /* 826 */ { RegST5, NACL_EMPTY_OPFLAGS, "%st5" },
  /* 827 */ { RegST6, NACL_EMPTY_OPFLAGS, "%st6" },
  /* 828 */ { RegST7, NACL_EMPTY_OPFLAGS, "%st7" },
  /* 829 */ { RegST1, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st1" },
  /* 830 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 831 */ { RegST2, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st2" },
  /* 832 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 833 */ { RegST3, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st3" },
  /* 834 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 835 */ { RegST4, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st4" },
  /* 836 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 837 */ { RegST5, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st5" },
  /* 838 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 839 */ { RegST6, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st6" },
  /* 840 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 841 */ { RegST7, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st7" },
  /* 842 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 843 */ { RegST7, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st7" },
  /* 844 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 845 */ { RegAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%ax" },
};

static const NaClInst g_Opcodes[2253] = {
  /* 0 */
  { NoPrefix, 1, { 0x00, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstAdd,
    2, g_Operands + 0,
    NULL
  },
  /* 1 */
  { NoPrefix, 1, { 0x01, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstAdd,
    2, g_Operands + 2,
    NULL
  },
  /* 2 */
  { NoPrefix, 1, { 0x02, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstAdd,
    2, g_Operands + 4,
    NULL
  },
  /* 3 */
  { NoPrefix, 1, { 0x03, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstAdd,
    2, g_Operands + 6,
    NULL
  },
  /* 4 */
  { NoPrefix, 1, { 0x04, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable),
    InstAdd,
    2, g_Operands + 8,
    NULL
  },
  /* 5 */
  { NoPrefix, 1, { 0x05, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstAdd,
    2, g_Operands + 10,
    NULL
  },
  /* 6 */
  { NoPrefix, 1, { 0x06, 0x00, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 7 */
  { NoPrefix, 1, { 0x07, 0x00, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 8 */
  { NoPrefix, 1, { 0x08, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstOr,
    2, g_Operands + 0,
    NULL
  },
  /* 9 */
  { NoPrefix, 1, { 0x09, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstOr,
    2, g_Operands + 2,
    NULL
  },
  /* 10 */
  { NoPrefix, 1, { 0x0a, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstOr,
    2, g_Operands + 4,
    NULL
  },
  /* 11 */
  { NoPrefix, 1, { 0x0b, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstOr,
    2, g_Operands + 6,
    NULL
  },
  /* 12 */
  { NoPrefix, 1, { 0x0c, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable),
    InstOr,
    2, g_Operands + 8,
    NULL
  },
  /* 13 */
  { NoPrefix, 1, { 0x0d, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstOr,
    2, g_Operands + 12,
    NULL
  },
  /* 14 */
  { NoPrefix, 1, { 0x0e, 0x00, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 15 */
  { NoPrefix, 1, { 0x0f, 0x00, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 16 */
  { NoPrefix, 1, { 0x10, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstAdc,
    2, g_Operands + 0,
    NULL
  },
  /* 17 */
  { NoPrefix, 1, { 0x11, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstAdc,
    2, g_Operands + 2,
    NULL
  },
  /* 18 */
  { NoPrefix, 1, { 0x12, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstAdc,
    2, g_Operands + 4,
    NULL
  },
  /* 19 */
  { NoPrefix, 1, { 0x13, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstAdc,
    2, g_Operands + 6,
    NULL
  },
  /* 20 */
  { NoPrefix, 1, { 0x14, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable),
    InstAdc,
    2, g_Operands + 8,
    NULL
  },
  /* 21 */
  { NoPrefix, 1, { 0x15, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstAdc,
    2, g_Operands + 10,
    NULL
  },
  /* 22 */
  { NoPrefix, 1, { 0x16, 0x00, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 23 */
  { NoPrefix, 1, { 0x17, 0x00, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 24 */
  { NoPrefix, 1, { 0x18, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstSbb,
    2, g_Operands + 0,
    NULL
  },
  /* 25 */
  { NoPrefix, 1, { 0x19, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstSbb,
    2, g_Operands + 2,
    NULL
  },
  /* 26 */
  { NoPrefix, 1, { 0x1a, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstSbb,
    2, g_Operands + 4,
    NULL
  },
  /* 27 */
  { NoPrefix, 1, { 0x1b, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstSbb,
    2, g_Operands + 6,
    NULL
  },
  /* 28 */
  { NoPrefix, 1, { 0x1c, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable),
    InstSbb,
    2, g_Operands + 8,
    NULL
  },
  /* 29 */
  { NoPrefix, 1, { 0x1d, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstSbb,
    2, g_Operands + 10,
    NULL
  },
  /* 30 */
  { NoPrefix, 1, { 0x1e, 0x00, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 31 */
  { NoPrefix, 1, { 0x1f, 0x00, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 32 */
  { NoPrefix, 1, { 0x20, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstAnd,
    2, g_Operands + 0,
    NULL
  },
  /* 33 */
  { NoPrefix, 1, { 0x21, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstAnd,
    2, g_Operands + 2,
    NULL
  },
  /* 34 */
  { NoPrefix, 1, { 0x22, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstAnd,
    2, g_Operands + 4,
    NULL
  },
  /* 35 */
  { NoPrefix, 1, { 0x23, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstAnd,
    2, g_Operands + 6,
    NULL
  },
  /* 36 */
  { NoPrefix, 1, { 0x24, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable),
    InstAnd,
    2, g_Operands + 8,
    NULL
  },
  /* 37 */
  { NoPrefix, 1, { 0x25, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstAnd,
    2, g_Operands + 10,
    NULL
  },
  /* 38 */
  { NoPrefix, 1, { 0x26, 0x00, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 39 */
  { NoPrefix, 1, { 0x27, 0x00, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 40 */
  { NoPrefix, 1, { 0x28, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstSub,
    2, g_Operands + 0,
    NULL
  },
  /* 41 */
  { NoPrefix, 1, { 0x29, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstSub,
    2, g_Operands + 2,
    NULL
  },
  /* 42 */
  { NoPrefix, 1, { 0x2a, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstSub,
    2, g_Operands + 4,
    NULL
  },
  /* 43 */
  { NoPrefix, 1, { 0x2b, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstSub,
    2, g_Operands + 6,
    NULL
  },
  /* 44 */
  { NoPrefix, 1, { 0x2c, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable),
    InstSub,
    2, g_Operands + 8,
    NULL
  },
  /* 45 */
  { NoPrefix, 1, { 0x2d, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstSub,
    2, g_Operands + 10,
    NULL
  },
  /* 46 */
  { NoPrefix, 1, { 0x2e, 0x00, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 47 */
  { NoPrefix, 1, { 0x2f, 0x00, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 48 */
  { NoPrefix, 1, { 0x30, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstXor,
    2, g_Operands + 0,
    NULL
  },
  /* 49 */
  { NoPrefix, 1, { 0x31, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstXor,
    2, g_Operands + 2,
    NULL
  },
  /* 50 */
  { NoPrefix, 1, { 0x32, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstXor,
    2, g_Operands + 4,
    NULL
  },
  /* 51 */
  { NoPrefix, 1, { 0x33, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstXor,
    2, g_Operands + 6,
    NULL
  },
  /* 52 */
  { NoPrefix, 1, { 0x34, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable),
    InstXor,
    2, g_Operands + 8,
    NULL
  },
  /* 53 */
  { NoPrefix, 1, { 0x35, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstXor,
    2, g_Operands + 10,
    NULL
  },
  /* 54 */
  { NoPrefix, 1, { 0x36, 0x00, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 55 */
  { NoPrefix, 1, { 0x37, 0x00, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 56 */
  { NoPrefix, 1, { 0x38, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstCmp,
    2, g_Operands + 14,
    NULL
  },
  /* 57 */
  { NoPrefix, 1, { 0x39, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCmp,
    2, g_Operands + 16,
    NULL
  },
  /* 58 */
  { NoPrefix, 1, { 0x3a, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstCmp,
    2, g_Operands + 18,
    NULL
  },
  /* 59 */
  { NoPrefix, 1, { 0x3b, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCmp,
    2, g_Operands + 20,
    NULL
  },
  /* 60 */
  { NoPrefix, 1, { 0x3c, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b),
    InstCmp,
    2, g_Operands + 22,
    NULL
  },
  /* 61 */
  { NoPrefix, 1, { 0x3d, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCmp,
    2, g_Operands + 24,
    NULL
  },
  /* 62 */
  { NoPrefix, 1, { 0x3e, 0x00, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 63 */
  { NoPrefix, 1, { 0x3f, 0x00, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 64 */
  { NoPrefix, 1, { 0x50, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPush,
    2, g_Operands + 26,
    NULL
  },
  /* 65 */
  { NoPrefix, 1, { 0x51, 0x01, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPush,
    2, g_Operands + 28,
    NULL
  },
  /* 66 */
  { NoPrefix, 1, { 0x52, 0x02, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPush,
    2, g_Operands + 26,
    NULL
  },
  /* 67 */
  { NoPrefix, 1, { 0x53, 0x03, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPush,
    2, g_Operands + 26,
    NULL
  },
  /* 68 */
  { NoPrefix, 1, { 0x54, 0x04, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPush,
    2, g_Operands + 26,
    NULL
  },
  /* 69 */
  { NoPrefix, 1, { 0x55, 0x05, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPush,
    2, g_Operands + 26,
    NULL
  },
  /* 70 */
  { NoPrefix, 1, { 0x56, 0x06, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPush,
    2, g_Operands + 26,
    NULL
  },
  /* 71 */
  { NoPrefix, 1, { 0x57, 0x07, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPush,
    2, g_Operands + 26,
    NULL
  },
  /* 72 */
  { NoPrefix, 1, { 0x58, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPop,
    2, g_Operands + 30,
    NULL
  },
  /* 73 */
  { NoPrefix, 1, { 0x59, 0x01, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPop,
    2, g_Operands + 32,
    NULL
  },
  /* 74 */
  { NoPrefix, 1, { 0x5a, 0x02, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPop,
    2, g_Operands + 30,
    NULL
  },
  /* 75 */
  { NoPrefix, 1, { 0x5b, 0x03, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPop,
    2, g_Operands + 30,
    NULL
  },
  /* 76 */
  { NoPrefix, 1, { 0x5c, 0x04, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPop,
    2, g_Operands + 30,
    NULL
  },
  /* 77 */
  { NoPrefix, 1, { 0x5d, 0x05, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPop,
    2, g_Operands + 30,
    NULL
  },
  /* 78 */
  { NoPrefix, 1, { 0x5e, 0x06, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPop,
    2, g_Operands + 30,
    NULL
  },
  /* 79 */
  { NoPrefix, 1, { 0x5f, 0x07, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPop,
    2, g_Operands + 30,
    NULL
  },
  /* 80 */
  { NoPrefix, 1, { 0x60, 0x00, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 81 */
  { NoPrefix, 1, { 0x61, 0x00, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 82 */
  { NoPrefix, 1, { 0x62, 0x00, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 83 */
  { NoPrefix, 1, { 0x63, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(LongMode),
    InstMovsxd,
    2, g_Operands + 34,
    NULL
  },
  /* 84 */
  { NoPrefix, 1, { 0x64, 0x00, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 85 */
  { NoPrefix, 1, { 0x65, 0x00, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 86 */
  { NoPrefix, 1, { 0x66, 0x00, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 87 */
  { NoPrefix, 1, { 0x67, 0x00, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 88 */
  { NoPrefix, 1, { 0x68, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPush,
    2, g_Operands + 36,
    NULL
  },
  /* 89 */
  { NoPrefix, 1, { 0x69, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstImul,
    3, g_Operands + 38,
    NULL
  },
  /* 90 */
  { NoPrefix, 1, { 0x6a, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPush,
    2, g_Operands + 41,
    NULL
  },
  /* 91 */
  { NoPrefix, 1, { 0x6b, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstImul,
    3, g_Operands + 43,
    NULL
  },
  /* 92 */
  { NoPrefix, 1, { 0x6c, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(NaClIllegal),
    InstInsb,
    2, g_Operands + 46,
    NULL
  },
  /* 93 */
  { NoPrefix, 1, { 0x6d, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(NaClIllegal),
    InstInsw,
    2, g_Operands + 48,
    g_Opcodes + 94
  },
  /* 94 */
  { NoPrefix, 1, { 0x6d, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal),
    InstInsd,
    2, g_Operands + 50,
    NULL
  },
  /* 95 */
  { NoPrefix, 1, { 0x6e, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(NaClIllegal),
    InstOutsb,
    2, g_Operands + 52,
    NULL
  },
  /* 96 */
  { NoPrefix, 1, { 0x6f, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(NaClIllegal),
    InstOutsw,
    2, g_Operands + 54,
    g_Opcodes + 97
  },
  /* 97 */
  { NoPrefix, 1, { 0x6f, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal),
    InstOutsd,
    2, g_Operands + 56,
    NULL
  },
  /* 98 */
  { NoPrefix, 1, { 0x70, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstJo,
    2, g_Operands + 58,
    NULL
  },
  /* 99 */
  { NoPrefix, 1, { 0x71, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstJno,
    2, g_Operands + 60,
    NULL
  },
  /* 100 */
  { NoPrefix, 1, { 0x72, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstJb,
    2, g_Operands + 58,
    NULL
  },
  /* 101 */
  { NoPrefix, 1, { 0x73, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstJnb,
    2, g_Operands + 58,
    NULL
  },
  /* 102 */
  { NoPrefix, 1, { 0x74, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstJz,
    2, g_Operands + 58,
    NULL
  },
  /* 103 */
  { NoPrefix, 1, { 0x75, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstJnz,
    2, g_Operands + 58,
    NULL
  },
  /* 104 */
  { NoPrefix, 1, { 0x76, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstJbe,
    2, g_Operands + 58,
    NULL
  },
  /* 105 */
  { NoPrefix, 1, { 0x77, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstJnbe,
    2, g_Operands + 58,
    NULL
  },
  /* 106 */
  { NoPrefix, 1, { 0x78, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstJs,
    2, g_Operands + 58,
    NULL
  },
  /* 107 */
  { NoPrefix, 1, { 0x79, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstJns,
    2, g_Operands + 58,
    NULL
  },
  /* 108 */
  { NoPrefix, 1, { 0x7a, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstJp,
    2, g_Operands + 58,
    NULL
  },
  /* 109 */
  { NoPrefix, 1, { 0x7b, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstJnp,
    2, g_Operands + 58,
    NULL
  },
  /* 110 */
  { NoPrefix, 1, { 0x7c, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstJl,
    2, g_Operands + 58,
    NULL
  },
  /* 111 */
  { NoPrefix, 1, { 0x7d, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstJnl,
    2, g_Operands + 58,
    NULL
  },
  /* 112 */
  { NoPrefix, 1, { 0x7e, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstJle,
    2, g_Operands + 58,
    NULL
  },
  /* 113 */
  { NoPrefix, 1, { 0x7f, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstJnle,
    2, g_Operands + 58,
    NULL
  },
  /* 114 */
  { NoPrefix, 1, { 0x80, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstAdd,
    2, g_Operands + 62,
    g_Opcodes + 115
  },
  /* 115 */
  { NoPrefix, 1, { 0x80, 0x01, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstOr,
    2, g_Operands + 64,
    g_Opcodes + 116
  },
  /* 116 */
  { NoPrefix, 1, { 0x80, 0x02, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstAdc,
    2, g_Operands + 62,
    g_Opcodes + 117
  },
  /* 117 */
  { NoPrefix, 1, { 0x80, 0x03, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstSbb,
    2, g_Operands + 62,
    g_Opcodes + 118
  },
  /* 118 */
  { NoPrefix, 1, { 0x80, 0x04, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstAnd,
    2, g_Operands + 62,
    g_Opcodes + 119
  },
  /* 119 */
  { NoPrefix, 1, { 0x80, 0x05, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstSub,
    2, g_Operands + 62,
    g_Opcodes + 120
  },
  /* 120 */
  { NoPrefix, 1, { 0x80, 0x06, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstXor,
    2, g_Operands + 62,
    g_Opcodes + 121
  },
  /* 121 */
  { NoPrefix, 1, { 0x80, 0x07, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstCmp,
    2, g_Operands + 66,
    NULL
  },
  /* 122 */
  { NoPrefix, 1, { 0x81, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstAdd,
    2, g_Operands + 68,
    g_Opcodes + 123
  },
  /* 123 */
  { NoPrefix, 1, { 0x81, 0x01, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstOr,
    2, g_Operands + 70,
    g_Opcodes + 124
  },
  /* 124 */
  { NoPrefix, 1, { 0x81, 0x02, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstAdc,
    2, g_Operands + 68,
    g_Opcodes + 125
  },
  /* 125 */
  { NoPrefix, 1, { 0x81, 0x03, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstSbb,
    2, g_Operands + 68,
    g_Opcodes + 126
  },
  /* 126 */
  { NoPrefix, 1, { 0x81, 0x04, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstAnd,
    2, g_Operands + 68,
    g_Opcodes + 127
  },
  /* 127 */
  { NoPrefix, 1, { 0x81, 0x05, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstSub,
    2, g_Operands + 68,
    g_Opcodes + 128
  },
  /* 128 */
  { NoPrefix, 1, { 0x81, 0x06, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstXor,
    2, g_Operands + 68,
    g_Opcodes + 129
  },
  /* 129 */
  { NoPrefix, 1, { 0x81, 0x07, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCmp,
    2, g_Operands + 39,
    NULL
  },
  /* 130 */
  { NoPrefix, 1, { 0x82, 0x00, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    g_Opcodes + 131
  },
  /* 131 */
  { NoPrefix, 1, { 0x82, 0x01, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    g_Opcodes + 132
  },
  /* 132 */
  { NoPrefix, 1, { 0x82, 0x02, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    g_Opcodes + 133
  },
  /* 133 */
  { NoPrefix, 1, { 0x82, 0x03, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    g_Opcodes + 134
  },
  /* 134 */
  { NoPrefix, 1, { 0x82, 0x04, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    g_Opcodes + 135
  },
  /* 135 */
  { NoPrefix, 1, { 0x82, 0x05, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    g_Opcodes + 136
  },
  /* 136 */
  { NoPrefix, 1, { 0x82, 0x06, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    g_Opcodes + 137
  },
  /* 137 */
  { NoPrefix, 1, { 0x82, 0x07, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 138 */
  { NoPrefix, 1, { 0x83, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstAdd,
    2, g_Operands + 72,
    g_Opcodes + 139
  },
  /* 139 */
  { NoPrefix, 1, { 0x83, 0x01, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstOr,
    2, g_Operands + 74,
    g_Opcodes + 140
  },
  /* 140 */
  { NoPrefix, 1, { 0x83, 0x02, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstAdc,
    2, g_Operands + 72,
    g_Opcodes + 141
  },
  /* 141 */
  { NoPrefix, 1, { 0x83, 0x03, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstSbb,
    2, g_Operands + 72,
    g_Opcodes + 142
  },
  /* 142 */
  { NoPrefix, 1, { 0x83, 0x04, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstAnd,
    2, g_Operands + 72,
    g_Opcodes + 143
  },
  /* 143 */
  { NoPrefix, 1, { 0x83, 0x05, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstSub,
    2, g_Operands + 72,
    g_Opcodes + 144
  },
  /* 144 */
  { NoPrefix, 1, { 0x83, 0x06, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstXor,
    2, g_Operands + 72,
    g_Opcodes + 145
  },
  /* 145 */
  { NoPrefix, 1, { 0x83, 0x07, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCmp,
    2, g_Operands + 44,
    NULL
  },
  /* 146 */
  { NoPrefix, 1, { 0x84, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstTest,
    2, g_Operands + 14,
    NULL
  },
  /* 147 */
  { NoPrefix, 1, { 0x85, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstTest,
    2, g_Operands + 16,
    NULL
  },
  /* 148 */
  { NoPrefix, 1, { 0x86, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstXchg,
    2, g_Operands + 76,
    NULL
  },
  /* 149 */
  { NoPrefix, 1, { 0x87, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstXchg,
    2, g_Operands + 78,
    NULL
  },
  /* 150 */
  { NoPrefix, 1, { 0x88, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstMov,
    2, g_Operands + 80,
    NULL
  },
  /* 151 */
  { NoPrefix, 1, { 0x89, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstMov,
    2, g_Operands + 82,
    NULL
  },
  /* 152 */
  { NoPrefix, 1, { 0x8a, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstMov,
    2, g_Operands + 84,
    NULL
  },
  /* 153 */
  { NoPrefix, 1, { 0x8b, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstMov,
    2, g_Operands + 38,
    NULL
  },
  /* 154 */
  { NoPrefix, 1, { 0x8c, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(ModRmRegSOperand) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal),
    InstMov,
    2, g_Operands + 86,
    NULL
  },
  /* 155 */
  { NoPrefix, 1, { 0x8d, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstLea,
    2, g_Operands + 88,
    NULL
  },
  /* 156 */
  { NoPrefix, 1, { 0x8e, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(ModRmRegSOperand) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstMov,
    2, g_Operands + 90,
    NULL
  },
  /* 157 */
  { NoPrefix, 1, { 0x8f, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPop,
    2, g_Operands + 92,
    g_Opcodes + 158
  },
  /* 158 */
  { NoPrefix, 1, { 0x8f, 0x00, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 159 */
  { NoPrefix, 1, { 0x90, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstXchg,
    2, g_Operands + 94,
    NULL
  },
  /* 160 */
  { NoPrefix, 1, { 0x91, 0x01, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstXchg,
    2, g_Operands + 96,
    NULL
  },
  /* 161 */
  { NoPrefix, 1, { 0x92, 0x02, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstXchg,
    2, g_Operands + 94,
    NULL
  },
  /* 162 */
  { NoPrefix, 1, { 0x93, 0x03, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstXchg,
    2, g_Operands + 94,
    NULL
  },
  /* 163 */
  { NoPrefix, 1, { 0x94, 0x04, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstXchg,
    2, g_Operands + 94,
    NULL
  },
  /* 164 */
  { NoPrefix, 1, { 0x95, 0x05, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstXchg,
    2, g_Operands + 94,
    NULL
  },
  /* 165 */
  { NoPrefix, 1, { 0x96, 0x06, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstXchg,
    2, g_Operands + 94,
    NULL
  },
  /* 166 */
  { NoPrefix, 1, { 0x97, 0x07, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstXchg,
    2, g_Operands + 94,
    NULL
  },
  /* 167 */
  { NoPrefix, 1, { 0x98, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w),
    InstCbw,
    2, g_Operands + 98,
    g_Opcodes + 168
  },
  /* 168 */
  { NoPrefix, 1, { 0x98, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OperandSize_v),
    InstCwde,
    2, g_Operands + 100,
    g_Opcodes + 169
  },
  /* 169 */
  { NoPrefix, 1, { 0x98, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OperandSize_o) | NACL_IFLAG(LongMode),
    InstCdqe,
    2, g_Operands + 102,
    NULL
  },
  /* 170 */
  { NoPrefix, 1, { 0x99, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w),
    InstCwd,
    2, g_Operands + 104,
    g_Opcodes + 171
  },
  /* 171 */
  { NoPrefix, 1, { 0x99, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OperandSize_v),
    InstCdq,
    2, g_Operands + 106,
    g_Opcodes + 172
  },
  /* 172 */
  { NoPrefix, 1, { 0x99, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OperandSize_o) | NACL_IFLAG(LongMode),
    InstCqo,
    2, g_Operands + 108,
    NULL
  },
  /* 173 */
  { NoPrefix, 1, { 0x9a, 0x00, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 174 */
  { NoPrefix, 1, { 0x9b, 0x00, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFwait,
    0, g_Operands + 0,
    NULL
  },
  /* 175 */
  { NoPrefix, 1, { 0x9c, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(NaClIllegal),
    InstPushf,
    2, g_Operands + 110,
    g_Opcodes + 176
  },
  /* 176 */
  { NoPrefix, 1, { 0x9c, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(OperandSizeDefaultIs64) | NACL_IFLAG(LongMode),
    InstPushfq,
    2, g_Operands + 112,
    NULL
  },
  /* 177 */
  { NoPrefix, 1, { 0x9d, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(NaClIllegal),
    InstPopf,
    2, g_Operands + 114,
    g_Opcodes + 178
  },
  /* 178 */
  { NoPrefix, 1, { 0x9d, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(OperandSizeDefaultIs64) | NACL_IFLAG(LongMode),
    InstPopfq,
    2, g_Operands + 116,
    NULL
  },
  /* 179 */
  { NoPrefix, 1, { 0x9e, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstSahf,
    1, g_Operands + 118,
    NULL
  },
  /* 180 */
  { NoPrefix, 1, { 0x9f, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstLahf,
    1, g_Operands + 119,
    NULL
  },
  /* 181 */
  { NoPrefix, 1, { 0xa0, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_Addr) | NACL_IFLAG(OperandSize_b),
    InstMov,
    2, g_Operands + 120,
    NULL
  },
  /* 182 */
  { NoPrefix, 1, { 0xa1, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_Addr) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstMov,
    2, g_Operands + 122,
    NULL
  },
  /* 183 */
  { NoPrefix, 1, { 0xa2, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_Addr) | NACL_IFLAG(OperandSize_b),
    InstMov,
    2, g_Operands + 124,
    NULL
  },
  /* 184 */
  { NoPrefix, 1, { 0xa3, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_Addr) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstMov,
    2, g_Operands + 126,
    NULL
  },
  /* 185 */
  { NoPrefix, 1, { 0xa4, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_b),
    InstMovsb,
    2, g_Operands + 128,
    NULL
  },
  /* 186 */
  { NoPrefix, 1, { 0xa5, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w),
    InstMovsw,
    2, g_Operands + 130,
    g_Opcodes + 187
  },
  /* 187 */
  { NoPrefix, 1, { 0xa5, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_v),
    InstMovsd,
    2, g_Operands + 132,
    g_Opcodes + 188
  },
  /* 188 */
  { NoPrefix, 1, { 0xa5, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(LongMode),
    InstMovsq,
    2, g_Operands + 134,
    NULL
  },
  /* 189 */
  { NoPrefix, 1, { 0xa6, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(OperandSize_b),
    InstCmpsb,
    2, g_Operands + 136,
    NULL
  },
  /* 190 */
  { NoPrefix, 1, { 0xa7, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w),
    InstCmpsw,
    2, g_Operands + 138,
    g_Opcodes + 191
  },
  /* 191 */
  { NoPrefix, 1, { 0xa7, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(OperandSize_v),
    InstCmpsd,
    2, g_Operands + 140,
    g_Opcodes + 192
  },
  /* 192 */
  { NoPrefix, 1, { 0xa7, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(LongMode),
    InstCmpsq,
    2, g_Operands + 142,
    NULL
  },
  /* 193 */
  { NoPrefix, 1, { 0xa8, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b),
    InstTest,
    2, g_Operands + 22,
    NULL
  },
  /* 194 */
  { NoPrefix, 1, { 0xa9, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstTest,
    2, g_Operands + 24,
    NULL
  },
  /* 195 */
  { NoPrefix, 1, { 0xaa, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_b),
    InstStosb,
    2, g_Operands + 144,
    NULL
  },
  /* 196 */
  { NoPrefix, 1, { 0xab, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w),
    InstStosw,
    2, g_Operands + 146,
    g_Opcodes + 197
  },
  /* 197 */
  { NoPrefix, 1, { 0xab, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_v),
    InstStosd,
    2, g_Operands + 148,
    g_Opcodes + 198
  },
  /* 198 */
  { NoPrefix, 1, { 0xab, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(LongMode),
    InstStosq,
    2, g_Operands + 150,
    NULL
  },
  /* 199 */
  { NoPrefix, 1, { 0xac, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_b),
    InstLodsb,
    2, g_Operands + 152,
    NULL
  },
  /* 200 */
  { NoPrefix, 1, { 0xad, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w),
    InstLodsw,
    2, g_Operands + 154,
    g_Opcodes + 201
  },
  /* 201 */
  { NoPrefix, 1, { 0xad, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_v),
    InstLodsd,
    2, g_Operands + 156,
    g_Opcodes + 202
  },
  /* 202 */
  { NoPrefix, 1, { 0xad, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(LongMode),
    InstLodsq,
    2, g_Operands + 158,
    NULL
  },
  /* 203 */
  { NoPrefix, 1, { 0xae, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(OperandSize_b),
    InstScasb,
    2, g_Operands + 160,
    NULL
  },
  /* 204 */
  { NoPrefix, 1, { 0xaf, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w),
    InstScasw,
    2, g_Operands + 162,
    g_Opcodes + 205
  },
  /* 205 */
  { NoPrefix, 1, { 0xaf, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(OperandSize_v),
    InstScasd,
    2, g_Operands + 164,
    g_Opcodes + 206
  },
  /* 206 */
  { NoPrefix, 1, { 0xaf, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(LongMode),
    InstScasq,
    2, g_Operands + 166,
    NULL
  },
  /* 207 */
  { NoPrefix, 1, { 0xb0, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_b),
    InstMov,
    2, g_Operands + 168,
    NULL
  },
  /* 208 */
  { NoPrefix, 1, { 0xb1, 0x01, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_b),
    InstMov,
    2, g_Operands + 170,
    NULL
  },
  /* 209 */
  { NoPrefix, 1, { 0xb2, 0x02, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_b),
    InstMov,
    2, g_Operands + 168,
    NULL
  },
  /* 210 */
  { NoPrefix, 1, { 0xb3, 0x03, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_b),
    InstMov,
    2, g_Operands + 168,
    NULL
  },
  /* 211 */
  { NoPrefix, 1, { 0xb4, 0x04, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_b),
    InstMov,
    2, g_Operands + 168,
    NULL
  },
  /* 212 */
  { NoPrefix, 1, { 0xb5, 0x05, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_b),
    InstMov,
    2, g_Operands + 168,
    NULL
  },
  /* 213 */
  { NoPrefix, 1, { 0xb6, 0x06, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_b),
    InstMov,
    2, g_Operands + 168,
    NULL
  },
  /* 214 */
  { NoPrefix, 1, { 0xb7, 0x07, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_b),
    InstMov,
    2, g_Operands + 168,
    NULL
  },
  /* 215 */
  { NoPrefix, 1, { 0xb8, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstMov,
    2, g_Operands + 172,
    NULL
  },
  /* 216 */
  { NoPrefix, 1, { 0xb9, 0x01, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstMov,
    2, g_Operands + 174,
    NULL
  },
  /* 217 */
  { NoPrefix, 1, { 0xba, 0x02, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstMov,
    2, g_Operands + 172,
    NULL
  },
  /* 218 */
  { NoPrefix, 1, { 0xbb, 0x03, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstMov,
    2, g_Operands + 172,
    NULL
  },
  /* 219 */
  { NoPrefix, 1, { 0xbc, 0x04, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstMov,
    2, g_Operands + 172,
    NULL
  },
  /* 220 */
  { NoPrefix, 1, { 0xbd, 0x05, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstMov,
    2, g_Operands + 172,
    NULL
  },
  /* 221 */
  { NoPrefix, 1, { 0xbe, 0x06, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstMov,
    2, g_Operands + 172,
    NULL
  },
  /* 222 */
  { NoPrefix, 1, { 0xbf, 0x07, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstMov,
    2, g_Operands + 172,
    NULL
  },
  /* 223 */
  { NoPrefix, 1, { 0xc0, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstRol,
    2, g_Operands + 62,
    g_Opcodes + 224
  },
  /* 224 */
  { NoPrefix, 1, { 0xc0, 0x01, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstRor,
    2, g_Operands + 62,
    g_Opcodes + 225
  },
  /* 225 */
  { NoPrefix, 1, { 0xc0, 0x02, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstRcl,
    2, g_Operands + 62,
    g_Opcodes + 226
  },
  /* 226 */
  { NoPrefix, 1, { 0xc0, 0x03, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstRcr,
    2, g_Operands + 62,
    g_Opcodes + 227
  },
  /* 227 */
  { NoPrefix, 1, { 0xc0, 0x04, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstShl,
    2, g_Operands + 62,
    g_Opcodes + 228
  },
  /* 228 */
  { NoPrefix, 1, { 0xc0, 0x05, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstShr,
    2, g_Operands + 62,
    g_Opcodes + 229
  },
  /* 229 */
  { NoPrefix, 1, { 0xc0, 0x06, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstShl,
    2, g_Operands + 62,
    g_Opcodes + 230
  },
  /* 230 */
  { NoPrefix, 1, { 0xc0, 0x07, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstSar,
    2, g_Operands + 62,
    NULL
  },
  /* 231 */
  { NoPrefix, 1, { 0xc1, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstRol,
    2, g_Operands + 72,
    g_Opcodes + 232
  },
  /* 232 */
  { NoPrefix, 1, { 0xc1, 0x01, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstRor,
    2, g_Operands + 72,
    g_Opcodes + 233
  },
  /* 233 */
  { NoPrefix, 1, { 0xc1, 0x02, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstRcl,
    2, g_Operands + 72,
    g_Opcodes + 234
  },
  /* 234 */
  { NoPrefix, 1, { 0xc1, 0x03, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstRcr,
    2, g_Operands + 72,
    g_Opcodes + 235
  },
  /* 235 */
  { NoPrefix, 1, { 0xc1, 0x04, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstShl,
    2, g_Operands + 72,
    g_Opcodes + 236
  },
  /* 236 */
  { NoPrefix, 1, { 0xc1, 0x05, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstShr,
    2, g_Operands + 72,
    g_Opcodes + 237
  },
  /* 237 */
  { NoPrefix, 1, { 0xc1, 0x06, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstShl,
    2, g_Operands + 72,
    g_Opcodes + 238
  },
  /* 238 */
  { NoPrefix, 1, { 0xc1, 0x07, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstSar,
    2, g_Operands + 72,
    NULL
  },
  /* 239 */
  { NoPrefix, 1, { 0xc2, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_w) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstRet,
    3, g_Operands + 176,
    NULL
  },
  /* 240 */
  { NoPrefix, 1, { 0xc3, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(NaClIllegal) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstRet,
    2, g_Operands + 176,
    NULL
  },
  /* 241 */
  { NoPrefix, 1, { 0xc4, 0x00, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 242 */
  { NoPrefix, 1, { 0xc5, 0x00, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 243 */
  { NoPrefix, 1, { 0xc6, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstMov,
    2, g_Operands + 179,
    g_Opcodes + 244
  },
  /* 244 */
  { NoPrefix, 1, { 0xc6, 0x00, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 245 */
  { NoPrefix, 1, { 0xc7, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstMov,
    2, g_Operands + 181,
    g_Opcodes + 246
  },
  /* 246 */
  { NoPrefix, 1, { 0xc7, 0x00, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 247 */
  { NoPrefix, 1, { 0xc8, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_w) | NACL_IFLAG(OpcodeHasImmed2_b) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstEnter,
    4, g_Operands + 183,
    NULL
  },
  /* 248 */
  { NoPrefix, 1, { 0xc9, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(NaClIllegal) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstLeave,
    2, g_Operands + 187,
    NULL
  },
  /* 249 */
  { NoPrefix, 1, { 0xca, 0x00, 0x00, 0x00 }, NACLi_RETURN,
    NACL_IFLAG(OpcodeHasImmed_w) | NACL_IFLAG(NaClIllegal),
    InstRet,
    3, g_Operands + 176,
    NULL
  },
  /* 250 */
  { NoPrefix, 1, { 0xcb, 0x00, 0x00, 0x00 }, NACLi_RETURN,
    NACL_IFLAG(NaClIllegal),
    InstRet,
    2, g_Operands + 176,
    NULL
  },
  /* 251 */
  { NoPrefix, 1, { 0xcc, 0x00, 0x00, 0x00 }, NACLi_SYSTEM,
    NACL_IFLAG(NaClIllegal),
    InstInt3,
    0, g_Operands + 0,
    NULL
  },
  /* 252 */
  { NoPrefix, 1, { 0xcd, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(NaClIllegal),
    InstInt,
    1, g_Operands + 9,
    NULL
  },
  /* 253 */
  { NoPrefix, 1, { 0xce, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstInto,
    0, g_Operands + 0,
    NULL
  },
  /* 254 */
  { NoPrefix, 1, { 0xcf, 0x00, 0x00, 0x00 }, NACLi_SYSTEM,
    NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstIretd,
    2, g_Operands + 176,
    g_Opcodes + 255
  },
  /* 255 */
  { NoPrefix, 1, { 0xcf, 0x00, 0x00, 0x00 }, NACLi_SYSTEM,
    NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(LongMode),
    InstIretq,
    2, g_Operands + 176,
    g_Opcodes + 256
  },
  /* 256 */
  { NoPrefix, 1, { 0xcf, 0x00, 0x00, 0x00 }, NACLi_SYSTEM,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(NaClIllegal),
    InstIret,
    2, g_Operands + 176,
    NULL
  },
  /* 257 */
  { NoPrefix, 1, { 0xd0, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstRol,
    2, g_Operands + 189,
    g_Opcodes + 258
  },
  /* 258 */
  { NoPrefix, 1, { 0xd0, 0x01, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstRor,
    2, g_Operands + 191,
    g_Opcodes + 259
  },
  /* 259 */
  { NoPrefix, 1, { 0xd0, 0x02, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstRcl,
    2, g_Operands + 189,
    g_Opcodes + 260
  },
  /* 260 */
  { NoPrefix, 1, { 0xd0, 0x03, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstRcr,
    2, g_Operands + 189,
    g_Opcodes + 261
  },
  /* 261 */
  { NoPrefix, 1, { 0xd0, 0x04, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstShl,
    2, g_Operands + 189,
    g_Opcodes + 262
  },
  /* 262 */
  { NoPrefix, 1, { 0xd0, 0x05, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstShr,
    2, g_Operands + 189,
    g_Opcodes + 263
  },
  /* 263 */
  { NoPrefix, 1, { 0xd0, 0x06, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstShl,
    2, g_Operands + 189,
    g_Opcodes + 264
  },
  /* 264 */
  { NoPrefix, 1, { 0xd0, 0x07, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSar,
    2, g_Operands + 189,
    NULL
  },
  /* 265 */
  { NoPrefix, 1, { 0xd1, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstRol,
    2, g_Operands + 193,
    g_Opcodes + 266
  },
  /* 266 */
  { NoPrefix, 1, { 0xd1, 0x01, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstRor,
    2, g_Operands + 195,
    g_Opcodes + 267
  },
  /* 267 */
  { NoPrefix, 1, { 0xd1, 0x02, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstRcl,
    2, g_Operands + 193,
    g_Opcodes + 268
  },
  /* 268 */
  { NoPrefix, 1, { 0xd1, 0x03, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstRcr,
    2, g_Operands + 193,
    g_Opcodes + 269
  },
  /* 269 */
  { NoPrefix, 1, { 0xd1, 0x04, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstShl,
    2, g_Operands + 193,
    g_Opcodes + 270
  },
  /* 270 */
  { NoPrefix, 1, { 0xd1, 0x05, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstShr,
    2, g_Operands + 193,
    g_Opcodes + 271
  },
  /* 271 */
  { NoPrefix, 1, { 0xd1, 0x06, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstShl,
    2, g_Operands + 193,
    g_Opcodes + 272
  },
  /* 272 */
  { NoPrefix, 1, { 0xd1, 0x07, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstSar,
    2, g_Operands + 193,
    NULL
  },
  /* 273 */
  { NoPrefix, 1, { 0xd2, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstRol,
    2, g_Operands + 197,
    g_Opcodes + 274
  },
  /* 274 */
  { NoPrefix, 1, { 0xd2, 0x01, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstRor,
    2, g_Operands + 199,
    g_Opcodes + 275
  },
  /* 275 */
  { NoPrefix, 1, { 0xd2, 0x02, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstRcl,
    2, g_Operands + 197,
    g_Opcodes + 276
  },
  /* 276 */
  { NoPrefix, 1, { 0xd2, 0x03, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstRcr,
    2, g_Operands + 197,
    g_Opcodes + 277
  },
  /* 277 */
  { NoPrefix, 1, { 0xd2, 0x04, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstShl,
    2, g_Operands + 197,
    g_Opcodes + 278
  },
  /* 278 */
  { NoPrefix, 1, { 0xd2, 0x05, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstShr,
    2, g_Operands + 197,
    g_Opcodes + 279
  },
  /* 279 */
  { NoPrefix, 1, { 0xd2, 0x06, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstShl,
    2, g_Operands + 197,
    g_Opcodes + 280
  },
  /* 280 */
  { NoPrefix, 1, { 0xd2, 0x07, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSar,
    2, g_Operands + 197,
    NULL
  },
  /* 281 */
  { NoPrefix, 1, { 0xd3, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstRol,
    2, g_Operands + 201,
    g_Opcodes + 282
  },
  /* 282 */
  { NoPrefix, 1, { 0xd3, 0x01, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstRor,
    2, g_Operands + 203,
    g_Opcodes + 283
  },
  /* 283 */
  { NoPrefix, 1, { 0xd3, 0x02, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstRcl,
    2, g_Operands + 201,
    g_Opcodes + 284
  },
  /* 284 */
  { NoPrefix, 1, { 0xd3, 0x03, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstRcr,
    2, g_Operands + 201,
    g_Opcodes + 285
  },
  /* 285 */
  { NoPrefix, 1, { 0xd3, 0x04, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstShl,
    2, g_Operands + 201,
    g_Opcodes + 286
  },
  /* 286 */
  { NoPrefix, 1, { 0xd3, 0x05, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstShr,
    2, g_Operands + 201,
    g_Opcodes + 287
  },
  /* 287 */
  { NoPrefix, 1, { 0xd3, 0x06, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstShl,
    2, g_Operands + 201,
    g_Opcodes + 288
  },
  /* 288 */
  { NoPrefix, 1, { 0xd3, 0x07, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstSar,
    2, g_Operands + 201,
    NULL
  },
  /* 289 */
  { NoPrefix, 1, { 0xd4, 0x00, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 290 */
  { NoPrefix, 1, { 0xd5, 0x00, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 291 */
  { NoPrefix, 1, { 0xd6, 0x00, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 292 */
  { NoPrefix, 1, { 0xd7, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstXlat,
    2, g_Operands + 205,
    NULL
  },
  /* 293 */
  { NoPrefix, 1, { 0xd8, 0x00, 0x00, 0x00 }, NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFadd,
    2, g_Operands + 207,
    g_Opcodes + 294
  },
  /* 294 */
  { NoPrefix, 1, { 0xd8, 0x01, 0x00, 0x00 }, NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFmul,
    2, g_Operands + 209,
    g_Opcodes + 295
  },
  /* 295 */
  { NoPrefix, 1, { 0xd8, 0x02, 0x00, 0x00 }, NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFcom,
    2, g_Operands + 211,
    g_Opcodes + 296
  },
  /* 296 */
  { NoPrefix, 1, { 0xd8, 0x03, 0x00, 0x00 }, NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFcomp,
    2, g_Operands + 213,
    g_Opcodes + 297
  },
  /* 297 */
  { NoPrefix, 1, { 0xd8, 0x04, 0x00, 0x00 }, NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFsub,
    2, g_Operands + 207,
    g_Opcodes + 298
  },
  /* 298 */
  { NoPrefix, 1, { 0xd8, 0x05, 0x00, 0x00 }, NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFsubr,
    2, g_Operands + 207,
    g_Opcodes + 299
  },
  /* 299 */
  { NoPrefix, 1, { 0xd8, 0x06, 0x00, 0x00 }, NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFdiv,
    2, g_Operands + 207,
    g_Opcodes + 300
  },
  /* 300 */
  { NoPrefix, 1, { 0xd8, 0x07, 0x00, 0x00 }, NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFdivr,
    2, g_Operands + 207,
    NULL
  },
  /* 301 */
  { NoPrefix, 1, { 0xd9, 0x00, 0x00, 0x00 }, NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFld,
    2, g_Operands + 215,
    g_Opcodes + 302
  },
  /* 302 */
  { NoPrefix, 1, { 0xd9, 0x01, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    g_Opcodes + 303
  },
  /* 303 */
  { NoPrefix, 1, { 0xd9, 0x02, 0x00, 0x00 }, NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFst,
    2, g_Operands + 217,
    g_Opcodes + 304
  },
  /* 304 */
  { NoPrefix, 1, { 0xd9, 0x03, 0x00, 0x00 }, NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFstp,
    2, g_Operands + 219,
    g_Opcodes + 305
  },
  /* 305 */
  { NoPrefix, 1, { 0xd9, 0x04, 0x00, 0x00 }, NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16),
    InstFldenv,
    1, g_Operands + 221,
    g_Opcodes + 306
  },
  /* 306 */
  { NoPrefix, 1, { 0xd9, 0x05, 0x00, 0x00 }, NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFldcw,
    1, g_Operands + 222,
    g_Opcodes + 307
  },
  /* 307 */
  { NoPrefix, 1, { 0xd9, 0x06, 0x00, 0x00 }, NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16),
    InstFnstenv,
    1, g_Operands + 223,
    g_Opcodes + 308
  },
  /* 308 */
  { NoPrefix, 1, { 0xd9, 0x07, 0x00, 0x00 }, NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFnstcw,
    1, g_Operands + 224,
    NULL
  },
  /* 309 */
  { NoPrefix, 1, { 0xda, 0x00, 0x00, 0x00 }, NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFiadd,
    2, g_Operands + 207,
    g_Opcodes + 310
  },
  /* 310 */
  { NoPrefix, 1, { 0xda, 0x01, 0x00, 0x00 }, NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFimul,
    2, g_Operands + 207,
    g_Opcodes + 311
  },
  /* 311 */
  { NoPrefix, 1, { 0xda, 0x02, 0x00, 0x00 }, NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFicom,
    2, g_Operands + 207,
    g_Opcodes + 312
  },
  /* 312 */
  { NoPrefix, 1, { 0xda, 0x03, 0x00, 0x00 }, NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFicomp,
    2, g_Operands + 207,
    g_Opcodes + 313
  },
  /* 313 */
  { NoPrefix, 1, { 0xda, 0x04, 0x00, 0x00 }, NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFisub,
    2, g_Operands + 207,
    g_Opcodes + 314
  },
  /* 314 */
  { NoPrefix, 1, { 0xda, 0x05, 0x00, 0x00 }, NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFisubr,
    2, g_Operands + 207,
    g_Opcodes + 315
  },
  /* 315 */
  { NoPrefix, 1, { 0xda, 0x06, 0x00, 0x00 }, NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFidiv,
    2, g_Operands + 207,
    g_Opcodes + 316
  },
  /* 316 */
  { NoPrefix, 1, { 0xda, 0x07, 0x00, 0x00 }, NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFidivr,
    2, g_Operands + 207,
    NULL
  },
  /* 317 */
  { NoPrefix, 1, { 0xdb, 0x00, 0x00, 0x00 }, NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFild,
    2, g_Operands + 215,
    g_Opcodes + 318
  },
  /* 318 */
  { NoPrefix, 1, { 0xdb, 0x01, 0x00, 0x00 }, NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFisttp,
    2, g_Operands + 217,
    g_Opcodes + 319
  },
  /* 319 */
  { NoPrefix, 1, { 0xdb, 0x02, 0x00, 0x00 }, NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFist,
    2, g_Operands + 217,
    g_Opcodes + 320
  },
  /* 320 */
  { NoPrefix, 1, { 0xdb, 0x03, 0x00, 0x00 }, NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFistp,
    2, g_Operands + 217,
    g_Opcodes + 321
  },
  /* 321 */
  { NoPrefix, 1, { 0xdb, 0x04, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    g_Opcodes + 322
  },
  /* 322 */
  { NoPrefix, 1, { 0xdb, 0x05, 0x00, 0x00 }, NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16),
    InstFld,
    2, g_Operands + 225,
    g_Opcodes + 323
  },
  /* 323 */
  { NoPrefix, 1, { 0xdb, 0x06, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    g_Opcodes + 324
  },
  /* 324 */
  { NoPrefix, 1, { 0xdb, 0x07, 0x00, 0x00 }, NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16),
    InstFstp,
    2, g_Operands + 227,
    NULL
  },
  /* 325 */
  { NoPrefix, 1, { 0xdc, 0x00, 0x00, 0x00 }, NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFadd,
    2, g_Operands + 229,
    g_Opcodes + 326
  },
  /* 326 */
  { NoPrefix, 1, { 0xdc, 0x01, 0x00, 0x00 }, NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFmul,
    2, g_Operands + 231,
    g_Opcodes + 327
  },
  /* 327 */
  { NoPrefix, 1, { 0xdc, 0x02, 0x00, 0x00 }, NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFcom,
    2, g_Operands + 233,
    g_Opcodes + 328
  },
  /* 328 */
  { NoPrefix, 1, { 0xdc, 0x03, 0x00, 0x00 }, NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFcomp,
    2, g_Operands + 235,
    g_Opcodes + 329
  },
  /* 329 */
  { NoPrefix, 1, { 0xdc, 0x04, 0x00, 0x00 }, NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFsub,
    2, g_Operands + 229,
    g_Opcodes + 330
  },
  /* 330 */
  { NoPrefix, 1, { 0xdc, 0x05, 0x00, 0x00 }, NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFsubr,
    2, g_Operands + 229,
    g_Opcodes + 331
  },
  /* 331 */
  { NoPrefix, 1, { 0xdc, 0x06, 0x00, 0x00 }, NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFdiv,
    2, g_Operands + 229,
    g_Opcodes + 332
  },
  /* 332 */
  { NoPrefix, 1, { 0xdc, 0x07, 0x00, 0x00 }, NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFdivr,
    2, g_Operands + 229,
    NULL
  },
  /* 333 */
  { NoPrefix, 1, { 0xdd, 0x00, 0x00, 0x00 }, NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFld,
    2, g_Operands + 237,
    g_Opcodes + 334
  },
  /* 334 */
  { NoPrefix, 1, { 0xdd, 0x01, 0x00, 0x00 }, NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFisttp,
    2, g_Operands + 239,
    g_Opcodes + 335
  },
  /* 335 */
  { NoPrefix, 1, { 0xdd, 0x02, 0x00, 0x00 }, NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFst,
    2, g_Operands + 241,
    g_Opcodes + 336
  },
  /* 336 */
  { NoPrefix, 1, { 0xdd, 0x03, 0x00, 0x00 }, NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFstp,
    2, g_Operands + 239,
    g_Opcodes + 337
  },
  /* 337 */
  { NoPrefix, 1, { 0xdd, 0x04, 0x00, 0x00 }, NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16),
    InstFrstor,
    1, g_Operands + 221,
    g_Opcodes + 338
  },
  /* 338 */
  { NoPrefix, 1, { 0xdd, 0x05, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    g_Opcodes + 339
  },
  /* 339 */
  { NoPrefix, 1, { 0xdd, 0x06, 0x00, 0x00 }, NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16),
    InstFnsave,
    1, g_Operands + 223,
    g_Opcodes + 340
  },
  /* 340 */
  { NoPrefix, 1, { 0xdd, 0x07, 0x00, 0x00 }, NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFnstsw,
    1, g_Operands + 224,
    NULL
  },
  /* 341 */
  { NoPrefix, 1, { 0xde, 0x00, 0x00, 0x00 }, NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFiadd,
    2, g_Operands + 243,
    g_Opcodes + 342
  },
  /* 342 */
  { NoPrefix, 1, { 0xde, 0x01, 0x00, 0x00 }, NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFimul,
    2, g_Operands + 245,
    g_Opcodes + 343
  },
  /* 343 */
  { NoPrefix, 1, { 0xde, 0x02, 0x00, 0x00 }, NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFicom,
    2, g_Operands + 247,
    g_Opcodes + 344
  },
  /* 344 */
  { NoPrefix, 1, { 0xde, 0x03, 0x00, 0x00 }, NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFicomp,
    2, g_Operands + 249,
    g_Opcodes + 345
  },
  /* 345 */
  { NoPrefix, 1, { 0xde, 0x04, 0x00, 0x00 }, NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFisub,
    2, g_Operands + 243,
    g_Opcodes + 346
  },
  /* 346 */
  { NoPrefix, 1, { 0xde, 0x05, 0x00, 0x00 }, NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFisubr,
    2, g_Operands + 243,
    g_Opcodes + 347
  },
  /* 347 */
  { NoPrefix, 1, { 0xde, 0x06, 0x00, 0x00 }, NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFidiv,
    2, g_Operands + 243,
    g_Opcodes + 348
  },
  /* 348 */
  { NoPrefix, 1, { 0xde, 0x07, 0x00, 0x00 }, NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFidivr,
    2, g_Operands + 243,
    NULL
  },
  /* 349 */
  { NoPrefix, 1, { 0xdf, 0x00, 0x00, 0x00 }, NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFild,
    2, g_Operands + 251,
    g_Opcodes + 350
  },
  /* 350 */
  { NoPrefix, 1, { 0xdf, 0x01, 0x00, 0x00 }, NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFisttp,
    2, g_Operands + 253,
    g_Opcodes + 351
  },
  /* 351 */
  { NoPrefix, 1, { 0xdf, 0x02, 0x00, 0x00 }, NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFist,
    2, g_Operands + 255,
    g_Opcodes + 352
  },
  /* 352 */
  { NoPrefix, 1, { 0xdf, 0x03, 0x00, 0x00 }, NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFistp,
    2, g_Operands + 253,
    g_Opcodes + 353
  },
  /* 353 */
  { NoPrefix, 1, { 0xdf, 0x04, 0x00, 0x00 }, NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16),
    InstFbld,
    2, g_Operands + 225,
    g_Opcodes + 354
  },
  /* 354 */
  { NoPrefix, 1, { 0xdf, 0x05, 0x00, 0x00 }, NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16),
    InstFild,
    2, g_Operands + 225,
    g_Opcodes + 355
  },
  /* 355 */
  { NoPrefix, 1, { 0xdf, 0x06, 0x00, 0x00 }, NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16),
    InstFbstp,
    2, g_Operands + 227,
    g_Opcodes + 356
  },
  /* 356 */
  { NoPrefix, 1, { 0xdf, 0x07, 0x00, 0x00 }, NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16),
    InstFistp,
    2, g_Operands + 227,
    NULL
  },
  /* 357 */
  { NoPrefix, 1, { 0xe0, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstLoopne,
    2, g_Operands + 58,
    NULL
  },
  /* 358 */
  { NoPrefix, 1, { 0xe1, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstLoope,
    2, g_Operands + 58,
    NULL
  },
  /* 359 */
  { NoPrefix, 1, { 0xe2, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstLoop,
    2, g_Operands + 58,
    NULL
  },
  /* 360 */
  { NoPrefix, 1, { 0xe3, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(AddressSize_v),
    InstJecxz,
    3, g_Operands + 257,
    g_Opcodes + 361
  },
  /* 361 */
  { NoPrefix, 1, { 0xe3, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(AddressSize_o),
    InstJrcxz,
    3, g_Operands + 260,
    NULL
  },
  /* 362 */
  { NoPrefix, 1, { 0xe4, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(NaClIllegal),
    InstIn,
    2, g_Operands + 263,
    NULL
  },
  /* 363 */
  { NoPrefix, 1, { 0xe5, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal),
    InstIn,
    2, g_Operands + 265,
    NULL
  },
  /* 364 */
  { NoPrefix, 1, { 0xe6, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(NaClIllegal),
    InstOut,
    2, g_Operands + 267,
    NULL
  },
  /* 365 */
  { NoPrefix, 1, { 0xe7, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal),
    InstOut,
    2, g_Operands + 269,
    NULL
  },
  /* 366 */
  { NoPrefix, 1, { 0xe8, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(NaClIllegal),
    InstCall,
    3, g_Operands + 271,
    g_Opcodes + 367
  },
  /* 367 */
  { NoPrefix, 1, { 0xe8, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstCall,
    3, g_Operands + 274,
    NULL
  },
  /* 368 */
  { NoPrefix, 1, { 0xe9, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstJmp,
    2, g_Operands + 277,
    NULL
  },
  /* 369 */
  { NoPrefix, 1, { 0xeb, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstJmp,
    2, g_Operands + 58,
    NULL
  },
  /* 370 */
  { NoPrefix, 1, { 0xec, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstIn,
    2, g_Operands + 279,
    NULL
  },
  /* 371 */
  { NoPrefix, 1, { 0xed, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal),
    InstIn,
    2, g_Operands + 281,
    NULL
  },
  /* 372 */
  { NoPrefix, 1, { 0xee, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstOut,
    2, g_Operands + 283,
    NULL
  },
  /* 373 */
  { NoPrefix, 1, { 0xef, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal),
    InstOut,
    2, g_Operands + 285,
    NULL
  },
  /* 374 */
  { NoPrefix, 1, { 0xf0, 0x00, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 375 */
  { NoPrefix, 1, { 0xf1, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstInt1,
    0, g_Operands + 0,
    NULL
  },
  /* 376 */
  { NoPrefix, 1, { 0xf2, 0x00, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 377 */
  { NoPrefix, 1, { 0xf3, 0x00, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 378 */
  { NoPrefix, 1, { 0xf4, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstHlt,
    0, g_Operands + 0,
    NULL
  },
  /* 379 */
  { NoPrefix, 1, { 0xf5, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstCmc,
    0, g_Operands + 0,
    NULL
  },
  /* 380 */
  { NoPrefix, 1, { 0xf6, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstTest,
    2, g_Operands + 66,
    g_Opcodes + 381
  },
  /* 381 */
  { NoPrefix, 1, { 0xf6, 0x01, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstTest,
    2, g_Operands + 66,
    g_Opcodes + 382
  },
  /* 382 */
  { NoPrefix, 1, { 0xf6, 0x02, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstNot,
    1, g_Operands + 0,
    g_Opcodes + 383
  },
  /* 383 */
  { NoPrefix, 1, { 0xf6, 0x03, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstNeg,
    1, g_Operands + 0,
    g_Opcodes + 384
  },
  /* 384 */
  { NoPrefix, 1, { 0xf6, 0x04, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstMul,
    3, g_Operands + 287,
    g_Opcodes + 385
  },
  /* 385 */
  { NoPrefix, 1, { 0xf6, 0x05, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstImul,
    3, g_Operands + 290,
    g_Opcodes + 386
  },
  /* 386 */
  { NoPrefix, 1, { 0xf6, 0x06, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstDiv,
    3, g_Operands + 287,
    g_Opcodes + 387
  },
  /* 387 */
  { NoPrefix, 1, { 0xf6, 0x07, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstIdiv,
    3, g_Operands + 287,
    NULL
  },
  /* 388 */
  { NoPrefix, 1, { 0xf7, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstTest,
    2, g_Operands + 39,
    g_Opcodes + 389
  },
  /* 389 */
  { NoPrefix, 1, { 0xf7, 0x01, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstTest,
    2, g_Operands + 39,
    g_Opcodes + 390
  },
  /* 390 */
  { NoPrefix, 1, { 0xf7, 0x02, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstNot,
    1, g_Operands + 2,
    g_Opcodes + 391
  },
  /* 391 */
  { NoPrefix, 1, { 0xf7, 0x03, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstNeg,
    1, g_Operands + 2,
    g_Opcodes + 392
  },
  /* 392 */
  { NoPrefix, 1, { 0xf7, 0x04, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstMul,
    3, g_Operands + 293,
    g_Opcodes + 393
  },
  /* 393 */
  { NoPrefix, 1, { 0xf7, 0x05, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstImul,
    3, g_Operands + 296,
    g_Opcodes + 394
  },
  /* 394 */
  { NoPrefix, 1, { 0xf7, 0x06, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstDiv,
    3, g_Operands + 293,
    g_Opcodes + 395
  },
  /* 395 */
  { NoPrefix, 1, { 0xf7, 0x07, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstIdiv,
    3, g_Operands + 293,
    NULL
  },
  /* 396 */
  { NoPrefix, 1, { 0xf8, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstClc,
    0, g_Operands + 0,
    NULL
  },
  /* 397 */
  { NoPrefix, 1, { 0xf9, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstStc,
    0, g_Operands + 0,
    NULL
  },
  /* 398 */
  { NoPrefix, 1, { 0xfa, 0x00, 0x00, 0x00 }, NACLi_SYSTEM,
    NACL_IFLAG(NaClIllegal),
    InstCli,
    0, g_Operands + 0,
    NULL
  },
  /* 399 */
  { NoPrefix, 1, { 0xfb, 0x00, 0x00, 0x00 }, NACLi_SYSTEM,
    NACL_IFLAG(NaClIllegal),
    InstSti,
    0, g_Operands + 0,
    NULL
  },
  /* 400 */
  { NoPrefix, 1, { 0xfc, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstCld,
    0, g_Operands + 0,
    NULL
  },
  /* 401 */
  { NoPrefix, 1, { 0xfd, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstStd,
    0, g_Operands + 0,
    NULL
  },
  /* 402 */
  { NoPrefix, 1, { 0xfe, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstInc,
    1, g_Operands + 0,
    g_Opcodes + 403
  },
  /* 403 */
  { NoPrefix, 1, { 0xfe, 0x01, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstDec,
    1, g_Operands + 0,
    g_Opcodes + 404
  },
  /* 404 */
  { NoPrefix, 1, { 0xfe, 0x02, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    g_Opcodes + 405
  },
  /* 405 */
  { NoPrefix, 1, { 0xfe, 0x03, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    g_Opcodes + 406
  },
  /* 406 */
  { NoPrefix, 1, { 0xfe, 0x04, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    g_Opcodes + 407
  },
  /* 407 */
  { NoPrefix, 1, { 0xfe, 0x05, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    g_Opcodes + 408
  },
  /* 408 */
  { NoPrefix, 1, { 0xfe, 0x06, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    g_Opcodes + 409
  },
  /* 409 */
  { NoPrefix, 1, { 0xfe, 0x07, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 410 */
  { NoPrefix, 1, { 0xff, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstInc,
    1, g_Operands + 2,
    g_Opcodes + 411
  },
  /* 411 */
  { NoPrefix, 1, { 0xff, 0x01, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstDec,
    1, g_Operands + 2,
    g_Opcodes + 412
  },
  /* 412 */
  { NoPrefix, 1, { 0xff, 0x02, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstCall,
    3, g_Operands + 299,
    g_Opcodes + 413
  },
  /* 413 */
  { NoPrefix, 1, { 0xff, 0x03, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstCall,
    3, g_Operands + 302,
    g_Opcodes + 414
  },
  /* 414 */
  { NoPrefix, 1, { 0xff, 0x04, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstJmp,
    2, g_Operands + 305,
    g_Opcodes + 415
  },
  /* 415 */
  { NoPrefix, 1, { 0xff, 0x05, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstJmp,
    2, g_Operands + 307,
    g_Opcodes + 416
  },
  /* 416 */
  { NoPrefix, 1, { 0xff, 0x06, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPush,
    2, g_Operands + 309,
    g_Opcodes + 417
  },
  /* 417 */
  { NoPrefix, 1, { 0xff, 0x07, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 418 */
  { Prefix0F, 2, { 0x0f, 0x00, 0x00, 0x00 }, NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal),
    InstSldt,
    1, g_Operands + 311,
    g_Opcodes + 419
  },
  /* 419 */
  { Prefix0F, 2, { 0x0f, 0x00, 0x01, 0x00 }, NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal),
    InstStr,
    1, g_Operands + 312,
    g_Opcodes + 420
  },
  /* 420 */
  { Prefix0F, 2, { 0x0f, 0x00, 0x02, 0x00 }, NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstLldt,
    1, g_Operands + 91,
    g_Opcodes + 421
  },
  /* 421 */
  { Prefix0F, 2, { 0x0f, 0x00, 0x03, 0x00 }, NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstLtr,
    1, g_Operands + 91,
    g_Opcodes + 422
  },
  /* 422 */
  { Prefix0F, 2, { 0x0f, 0x00, 0x04, 0x00 }, NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstVerr,
    1, g_Operands + 313,
    g_Opcodes + 423
  },
  /* 423 */
  { Prefix0F, 2, { 0x0f, 0x00, 0x05, 0x00 }, NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstVerw,
    1, g_Operands + 314,
    g_Opcodes + 424
  },
  /* 424 */
  { Prefix0F, 2, { 0x0f, 0x00, 0x06, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    g_Opcodes + 425
  },
  /* 425 */
  { Prefix0F, 2, { 0x0f, 0x00, 0x07, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 426 */
  { Prefix0F, 2, { 0x0f, 0x01, 0x00, 0x00 }, NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstSgdt,
    1, g_Operands + 315,
    g_Opcodes + 427
  },
  /* 427 */
  { Prefix0F, 2, { 0x0f, 0x01, 0x01, 0x00 }, NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstSidt,
    1, g_Operands + 316,
    g_Opcodes + 428
  },
  /* 428 */
  { Prefix0F, 2, { 0x0f, 0x01, 0x01, 0x00 }, NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstMonitor,
    3, g_Operands + 317,
    g_Opcodes + 429
  },
  /* 429 */
  { Prefix0F, 2, { 0x0f, 0x01, 0x01, 0x01 }, NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstMwait,
    2, g_Operands + 320,
    g_Opcodes + 430
  },
  /* 430 */
  { Prefix0F, 2, { 0x0f, 0x01, 0x01, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    g_Opcodes + 431
  },
  /* 431 */
  { Prefix0F, 2, { 0x0f, 0x01, 0x02, 0x00 }, NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstLgdt,
    1, g_Operands + 322,
    g_Opcodes + 432
  },
  /* 432 */
  { Prefix0F, 2, { 0x0f, 0x01, 0x03, 0x00 }, NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstLidt,
    1, g_Operands + 323,
    g_Opcodes + 433
  },
  /* 433 */
  { Prefix0F, 2, { 0x0f, 0x01, 0x03, 0x00 }, NACLi_SVM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstVmrun,
    1, g_Operands + 324,
    g_Opcodes + 434
  },
  /* 434 */
  { Prefix0F, 2, { 0x0f, 0x01, 0x03, 0x01 }, NACLi_SVM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstVmmcall,
    0, g_Operands + 0,
    g_Opcodes + 435
  },
  /* 435 */
  { Prefix0F, 2, { 0x0f, 0x01, 0x03, 0x02 }, NACLi_SVM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstVmload,
    1, g_Operands + 325,
    g_Opcodes + 436
  },
  /* 436 */
  { Prefix0F, 2, { 0x0f, 0x01, 0x03, 0x03 }, NACLi_SVM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstVmsave,
    1, g_Operands + 324,
    g_Opcodes + 437
  },
  /* 437 */
  { Prefix0F, 2, { 0x0f, 0x01, 0x03, 0x04 }, NACLi_SVM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstStgi,
    0, g_Operands + 0,
    g_Opcodes + 438
  },
  /* 438 */
  { Prefix0F, 2, { 0x0f, 0x01, 0x03, 0x05 }, NACLi_SVM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstClgi,
    0, g_Operands + 0,
    g_Opcodes + 439
  },
  /* 439 */
  { Prefix0F, 2, { 0x0f, 0x01, 0x03, 0x06 }, NACLi_SVM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstSkinit,
    2, g_Operands + 326,
    g_Opcodes + 440
  },
  /* 440 */
  { Prefix0F, 2, { 0x0f, 0x01, 0x03, 0x07 }, NACLi_SVM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvlpga,
    2, g_Operands + 328,
    g_Opcodes + 441
  },
  /* 441 */
  { Prefix0F, 2, { 0x0f, 0x01, 0x04, 0x00 }, NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal),
    InstSmsw,
    1, g_Operands + 86,
    g_Opcodes + 442
  },
  /* 442 */
  { Prefix0F, 2, { 0x0f, 0x01, 0x05, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    g_Opcodes + 443
  },
  /* 443 */
  { Prefix0F, 2, { 0x0f, 0x01, 0x06, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstLmsw,
    1, g_Operands + 91,
    g_Opcodes + 444
  },
  /* 444 */
  { Prefix0F, 2, { 0x0f, 0x01, 0x07, 0x00 }, NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvlpg,
    1, g_Operands + 330,
    g_Opcodes + 445
  },
  /* 445 */
  { Prefix0F, 2, { 0x0f, 0x01, 0x07, 0x00 }, NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(LongMode),
    InstSwapgs,
    1, g_Operands + 331,
    g_Opcodes + 446
  },
  /* 446 */
  { Prefix0F, 2, { 0x0f, 0x01, 0x07, 0x01 }, NACLi_RDTSCP,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstRdtscp,
    3, g_Operands + 332,
    g_Opcodes + 447
  },
  /* 447 */
  { Prefix0F, 2, { 0x0f, 0x01, 0x07, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 448 */
  { Prefix0F, 2, { 0x0f, 0x02, 0x00, 0x00 }, NACLi_SYSTEM,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal),
    InstLar,
    2, g_Operands + 335,
    NULL
  },
  /* 449 */
  { Prefix0F, 2, { 0x0f, 0x03, 0x00, 0x00 }, NACLi_SYSTEM,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal),
    InstLsl,
    2, g_Operands + 337,
    NULL
  },
  /* 450 */
  { Prefix0F, 2, { 0x0f, 0x04, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 451 */
  { Prefix0F, 2, { 0x0f, 0x05, 0x00, 0x00 }, NACLi_SYSCALL,
    NACL_IFLAG(NaClIllegal),
    InstSyscall,
    2, g_Operands + 339,
    NULL
  },
  /* 452 */
  { Prefix0F, 2, { 0x0f, 0x06, 0x00, 0x00 }, NACLi_SYSTEM,
    NACL_IFLAG(NaClIllegal),
    InstClts,
    0, g_Operands + 0,
    NULL
  },
  /* 453 */
  { Prefix0F, 2, { 0x0f, 0x07, 0x00, 0x00 }, NACLi_SYSTEM,
    NACL_IFLAG(NaClIllegal),
    InstSysret,
    2, g_Operands + 260,
    NULL
  },
  /* 454 */
  { Prefix0F, 2, { 0x0f, 0x08, 0x00, 0x00 }, NACLi_SYSTEM,
    NACL_IFLAG(NaClIllegal),
    InstInvd,
    0, g_Operands + 0,
    NULL
  },
  /* 455 */
  { Prefix0F, 2, { 0x0f, 0x09, 0x00, 0x00 }, NACLi_SYSTEM,
    NACL_IFLAG(NaClIllegal),
    InstWbinvd,
    0, g_Operands + 0,
    NULL
  },
  /* 456 */
  { Prefix0F, 2, { 0x0f, 0x0a, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 457 */
  { Prefix0F, 2, { 0x0f, 0x0b, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstUd2,
    0, g_Operands + 0,
    NULL
  },
  /* 458 */
  { Prefix0F, 2, { 0x0f, 0x0c, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 459 */
  { Prefix0F, 2, { 0x0f, 0x0d, 0x00, 0x00 }, NACLi_3DNOW,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstPrefetch_exclusive,
    1, g_Operands + 341,
    g_Opcodes + 460
  },
  /* 460 */
  { Prefix0F, 2, { 0x0f, 0x0d, 0x01, 0x00 }, NACLi_3DNOW,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstPrefetch_modified,
    1, g_Operands + 342,
    g_Opcodes + 461
  },
  /* 461 */
  { Prefix0F, 2, { 0x0f, 0x0d, 0x02, 0x00 }, NACLi_3DNOW,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstPrefetch_reserved,
    1, g_Operands + 341,
    g_Opcodes + 462
  },
  /* 462 */
  { Prefix0F, 2, { 0x0f, 0x0d, 0x03, 0x00 }, NACLi_3DNOW,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstPrefetch_modified,
    1, g_Operands + 341,
    g_Opcodes + 463
  },
  /* 463 */
  { Prefix0F, 2, { 0x0f, 0x0d, 0x04, 0x00 }, NACLi_3DNOW,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstPrefetch_reserved,
    1, g_Operands + 341,
    g_Opcodes + 464
  },
  /* 464 */
  { Prefix0F, 2, { 0x0f, 0x0d, 0x05, 0x00 }, NACLi_3DNOW,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstPrefetch_reserved,
    1, g_Operands + 341,
    g_Opcodes + 465
  },
  /* 465 */
  { Prefix0F, 2, { 0x0f, 0x0d, 0x06, 0x00 }, NACLi_3DNOW,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstPrefetch_reserved,
    1, g_Operands + 341,
    g_Opcodes + 466
  },
  /* 466 */
  { Prefix0F, 2, { 0x0f, 0x0d, 0x07, 0x00 }, NACLi_3DNOW,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstPrefetch_reserved,
    1, g_Operands + 341,
    NULL
  },
  /* 467 */
  { Prefix0F, 2, { 0x0f, 0x0e, 0x00, 0x00 }, NACLi_3DNOW,
    NACL_EMPTY_IFLAGS,
    InstFemms,
    0, g_Operands + 0,
    NULL
  },
  /* 468 */
  { Prefix0F, 2, { 0x0f, 0x0f, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(Opcode0F0F) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    3, g_Operands + 343,
    NULL
  },
  /* 469 */
  { Prefix0F, 2, { 0x0f, 0x10, 0x00, 0x00 }, NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstMovups,
    2, g_Operands + 346,
    NULL
  },
  /* 470 */
  { Prefix0F, 2, { 0x0f, 0x11, 0x00, 0x00 }, NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstMovups,
    2, g_Operands + 348,
    NULL
  },
  /* 471 */
  { Prefix0F, 2, { 0x0f, 0x12, 0x00, 0x00 }, NACLi_SSE,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMovlps,
    2, g_Operands + 350,
    g_Opcodes + 472
  },
  /* 472 */
  { Prefix0F, 2, { 0x0f, 0x12, 0x00, 0x00 }, NACLi_SSE,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMovhlps,
    2, g_Operands + 352,
    NULL
  },
  /* 473 */
  { Prefix0F, 2, { 0x0f, 0x13, 0x00, 0x00 }, NACLi_SSE,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMovlps,
    2, g_Operands + 354,
    NULL
  },
  /* 474 */
  { Prefix0F, 2, { 0x0f, 0x14, 0x00, 0x00 }, NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstUnpcklps,
    2, g_Operands + 356,
    NULL
  },
  /* 475 */
  { Prefix0F, 2, { 0x0f, 0x15, 0x00, 0x00 }, NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstUnpckhps,
    2, g_Operands + 358,
    NULL
  },
  /* 476 */
  { Prefix0F, 2, { 0x0f, 0x16, 0x00, 0x00 }, NACLi_SSE,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMovhps,
    2, g_Operands + 350,
    g_Opcodes + 477
  },
  /* 477 */
  { Prefix0F, 2, { 0x0f, 0x16, 0x00, 0x00 }, NACLi_SSE,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMovlhps,
    2, g_Operands + 352,
    NULL
  },
  /* 478 */
  { Prefix0F, 2, { 0x0f, 0x17, 0x00, 0x00 }, NACLi_SSE,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMovhps,
    2, g_Operands + 354,
    NULL
  },
  /* 479 */
  { Prefix0F, 2, { 0x0f, 0x18, 0x00, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstPrefetchnta,
    1, g_Operands + 341,
    g_Opcodes + 480
  },
  /* 480 */
  { Prefix0F, 2, { 0x0f, 0x18, 0x01, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstPrefetcht0,
    1, g_Operands + 341,
    g_Opcodes + 481
  },
  /* 481 */
  { Prefix0F, 2, { 0x0f, 0x18, 0x02, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstPrefetcht1,
    1, g_Operands + 341,
    g_Opcodes + 482
  },
  /* 482 */
  { Prefix0F, 2, { 0x0f, 0x18, 0x03, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstPrefetcht2,
    1, g_Operands + 341,
    g_Opcodes + 483
  },
  /* 483 */
  { Prefix0F, 2, { 0x0f, 0x18, 0x04, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm),
    InstNop,
    0, g_Operands + 0,
    g_Opcodes + 484
  },
  /* 484 */
  { Prefix0F, 2, { 0x0f, 0x18, 0x05, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm),
    InstNop,
    0, g_Operands + 0,
    g_Opcodes + 485
  },
  /* 485 */
  { Prefix0F, 2, { 0x0f, 0x18, 0x06, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm),
    InstNop,
    0, g_Operands + 0,
    g_Opcodes + 486
  },
  /* 486 */
  { Prefix0F, 2, { 0x0f, 0x18, 0x07, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm),
    InstNop,
    0, g_Operands + 0,
    NULL
  },
  /* 487 */
  { Prefix0F, 2, { 0x0f, 0x19, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstNop,
    0, g_Operands + 0,
    NULL
  },
  /* 488 */
  { Prefix0F, 2, { 0x0f, 0x1a, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstNop,
    0, g_Operands + 0,
    NULL
  },
  /* 489 */
  { Prefix0F, 2, { 0x0f, 0x1b, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstNop,
    0, g_Operands + 0,
    NULL
  },
  /* 490 */
  { Prefix0F, 2, { 0x0f, 0x1c, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstNop,
    0, g_Operands + 0,
    NULL
  },
  /* 491 */
  { Prefix0F, 2, { 0x0f, 0x1d, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstNop,
    0, g_Operands + 0,
    NULL
  },
  /* 492 */
  { Prefix0F, 2, { 0x0f, 0x1e, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstNop,
    0, g_Operands + 0,
    NULL
  },
  /* 493 */
  { Prefix0F, 2, { 0x0f, 0x1f, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm),
    InstNop,
    0, g_Operands + 0,
    g_Opcodes + 494
  },
  /* 494 */
  { Prefix0F, 2, { 0x0f, 0x1f, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstNop,
    0, g_Operands + 0,
    NULL
  },
  /* 495 */
  { Prefix0F, 2, { 0x0f, 0x20, 0x00, 0x00 }, NACLi_SYSTEM,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstMov,
    2, g_Operands + 360,
    NULL
  },
  /* 496 */
  { Prefix0F, 2, { 0x0f, 0x21, 0x00, 0x00 }, NACLi_SYSTEM,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstMov,
    2, g_Operands + 362,
    NULL
  },
  /* 497 */
  { Prefix0F, 2, { 0x0f, 0x22, 0x00, 0x00 }, NACLi_SYSTEM,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstMov,
    2, g_Operands + 364,
    NULL
  },
  /* 498 */
  { Prefix0F, 2, { 0x0f, 0x23, 0x00, 0x00 }, NACLi_SYSTEM,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstMov,
    2, g_Operands + 366,
    NULL
  },
  /* 499 */
  { Prefix0F, 2, { 0x0f, 0x24, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 500 */
  { Prefix0F, 2, { 0x0f, 0x25, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 501 */
  { Prefix0F, 2, { 0x0f, 0x26, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 502 */
  { Prefix0F, 2, { 0x0f, 0x27, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 503 */
  { Prefix0F, 2, { 0x0f, 0x28, 0x00, 0x00 }, NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstMovaps,
    2, g_Operands + 346,
    NULL
  },
  /* 504 */
  { Prefix0F, 2, { 0x0f, 0x29, 0x00, 0x00 }, NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstMovaps,
    2, g_Operands + 348,
    NULL
  },
  /* 505 */
  { Prefix0F, 2, { 0x0f, 0x2a, 0x00, 0x00 }, NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstCvtpi2ps,
    2, g_Operands + 368,
    NULL
  },
  /* 506 */
  { Prefix0F, 2, { 0x0f, 0x2b, 0x00, 0x00 }, NACLi_SSE,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMovntps,
    2, g_Operands + 370,
    NULL
  },
  /* 507 */
  { Prefix0F, 2, { 0x0f, 0x2c, 0x00, 0x00 }, NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstCvttps2pi,
    2, g_Operands + 372,
    NULL
  },
  /* 508 */
  { Prefix0F, 2, { 0x0f, 0x2d, 0x00, 0x00 }, NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstCvtps2pi,
    2, g_Operands + 374,
    NULL
  },
  /* 509 */
  { Prefix0F, 2, { 0x0f, 0x2e, 0x00, 0x00 }, NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstUcomiss,
    2, g_Operands + 376,
    NULL
  },
  /* 510 */
  { Prefix0F, 2, { 0x0f, 0x2f, 0x00, 0x00 }, NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstComiss,
    2, g_Operands + 378,
    NULL
  },
  /* 511 */
  { Prefix0F, 2, { 0x0f, 0x30, 0x00, 0x00 }, NACLi_RDMSR,
    NACL_IFLAG(NaClIllegal),
    InstWrmsr,
    3, g_Operands + 380,
    NULL
  },
  /* 512 */
  { Prefix0F, 2, { 0x0f, 0x31, 0x00, 0x00 }, NACLi_RDTSC,
    NACL_EMPTY_IFLAGS,
    InstRdtsc,
    2, g_Operands + 383,
    NULL
  },
  /* 513 */
  { Prefix0F, 2, { 0x0f, 0x32, 0x00, 0x00 }, NACLi_RDMSR,
    NACL_IFLAG(NaClIllegal),
    InstRdmsr,
    3, g_Operands + 385,
    NULL
  },
  /* 514 */
  { Prefix0F, 2, { 0x0f, 0x33, 0x00, 0x00 }, NACLi_SYSTEM,
    NACL_IFLAG(NaClIllegal),
    InstRdpmc,
    3, g_Operands + 388,
    NULL
  },
  /* 515 */
  { Prefix0F, 2, { 0x0f, 0x34, 0x00, 0x00 }, NACLi_SYSENTER,
    NACL_IFLAG(NaClIllegal),
    InstSysenter,
    4, g_Operands + 391,
    NULL
  },
  /* 516 */
  { Prefix0F, 2, { 0x0f, 0x35, 0x00, 0x00 }, NACLi_SYSENTER,
    NACL_IFLAG(NaClIllegal),
    InstSysexit,
    6, g_Operands + 395,
    NULL
  },
  /* 517 */
  { Prefix0F, 2, { 0x0f, 0x36, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 518 */
  { Prefix0F, 2, { 0x0f, 0x37, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 519 */
  { Prefix0F, 2, { 0x0f, 0x38, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 520 */
  { Prefix0F, 2, { 0x0f, 0x39, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 521 */
  { Prefix0F, 2, { 0x0f, 0x3a, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 522 */
  { Prefix0F, 2, { 0x0f, 0x3b, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 523 */
  { Prefix0F, 2, { 0x0f, 0x3c, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 524 */
  { Prefix0F, 2, { 0x0f, 0x3d, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 525 */
  { Prefix0F, 2, { 0x0f, 0x3e, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 526 */
  { Prefix0F, 2, { 0x0f, 0x3f, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 527 */
  { Prefix0F, 2, { 0x0f, 0x40, 0x00, 0x00 }, NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCmovo,
    2, g_Operands + 38,
    NULL
  },
  /* 528 */
  { Prefix0F, 2, { 0x0f, 0x41, 0x00, 0x00 }, NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCmovno,
    2, g_Operands + 38,
    NULL
  },
  /* 529 */
  { Prefix0F, 2, { 0x0f, 0x42, 0x00, 0x00 }, NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCmovb,
    2, g_Operands + 38,
    NULL
  },
  /* 530 */
  { Prefix0F, 2, { 0x0f, 0x43, 0x00, 0x00 }, NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCmovnb,
    2, g_Operands + 38,
    NULL
  },
  /* 531 */
  { Prefix0F, 2, { 0x0f, 0x44, 0x00, 0x00 }, NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCmovz,
    2, g_Operands + 38,
    NULL
  },
  /* 532 */
  { Prefix0F, 2, { 0x0f, 0x45, 0x00, 0x00 }, NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCmovnz,
    2, g_Operands + 38,
    NULL
  },
  /* 533 */
  { Prefix0F, 2, { 0x0f, 0x46, 0x00, 0x00 }, NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCmovbe,
    2, g_Operands + 38,
    NULL
  },
  /* 534 */
  { Prefix0F, 2, { 0x0f, 0x47, 0x00, 0x00 }, NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCmovnbe,
    2, g_Operands + 38,
    NULL
  },
  /* 535 */
  { Prefix0F, 2, { 0x0f, 0x48, 0x00, 0x00 }, NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCmovs,
    2, g_Operands + 38,
    NULL
  },
  /* 536 */
  { Prefix0F, 2, { 0x0f, 0x49, 0x00, 0x00 }, NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCmovns,
    2, g_Operands + 38,
    NULL
  },
  /* 537 */
  { Prefix0F, 2, { 0x0f, 0x4a, 0x00, 0x00 }, NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCmovp,
    2, g_Operands + 38,
    NULL
  },
  /* 538 */
  { Prefix0F, 2, { 0x0f, 0x4b, 0x00, 0x00 }, NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCmovnp,
    2, g_Operands + 38,
    NULL
  },
  /* 539 */
  { Prefix0F, 2, { 0x0f, 0x4c, 0x00, 0x00 }, NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCmovl,
    2, g_Operands + 38,
    NULL
  },
  /* 540 */
  { Prefix0F, 2, { 0x0f, 0x4d, 0x00, 0x00 }, NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCmovnl,
    2, g_Operands + 38,
    NULL
  },
  /* 541 */
  { Prefix0F, 2, { 0x0f, 0x4e, 0x00, 0x00 }, NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCmovle,
    2, g_Operands + 38,
    NULL
  },
  /* 542 */
  { Prefix0F, 2, { 0x0f, 0x4f, 0x00, 0x00 }, NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCmovnle,
    2, g_Operands + 38,
    NULL
  },
  /* 543 */
  { Prefix0F, 2, { 0x0f, 0x50, 0x00, 0x00 }, NACLi_SSE,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMovmskps,
    2, g_Operands + 401,
    NULL
  },
  /* 544 */
  { Prefix0F, 2, { 0x0f, 0x51, 0x00, 0x00 }, NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstSqrtps,
    2, g_Operands + 346,
    NULL
  },
  /* 545 */
  { Prefix0F, 2, { 0x0f, 0x52, 0x00, 0x00 }, NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstRsqrtps,
    2, g_Operands + 346,
    NULL
  },
  /* 546 */
  { Prefix0F, 2, { 0x0f, 0x53, 0x00, 0x00 }, NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstRcpps,
    2, g_Operands + 346,
    NULL
  },
  /* 547 */
  { Prefix0F, 2, { 0x0f, 0x54, 0x00, 0x00 }, NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstAndps,
    2, g_Operands + 403,
    NULL
  },
  /* 548 */
  { Prefix0F, 2, { 0x0f, 0x55, 0x00, 0x00 }, NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstAndnps,
    2, g_Operands + 405,
    NULL
  },
  /* 549 */
  { Prefix0F, 2, { 0x0f, 0x56, 0x00, 0x00 }, NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstOrps,
    2, g_Operands + 403,
    NULL
  },
  /* 550 */
  { Prefix0F, 2, { 0x0f, 0x57, 0x00, 0x00 }, NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstXorps,
    2, g_Operands + 403,
    NULL
  },
  /* 551 */
  { Prefix0F, 2, { 0x0f, 0x58, 0x00, 0x00 }, NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstAddps,
    2, g_Operands + 403,
    NULL
  },
  /* 552 */
  { Prefix0F, 2, { 0x0f, 0x59, 0x00, 0x00 }, NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstMulps,
    2, g_Operands + 403,
    NULL
  },
  /* 553 */
  { Prefix0F, 2, { 0x0f, 0x5a, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm),
    InstCvtps2pd,
    2, g_Operands + 407,
    NULL
  },
  /* 554 */
  { Prefix0F, 2, { 0x0f, 0x5b, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm),
    InstCvtdq2ps,
    2, g_Operands + 409,
    NULL
  },
  /* 555 */
  { Prefix0F, 2, { 0x0f, 0x5c, 0x00, 0x00 }, NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstSubps,
    2, g_Operands + 403,
    NULL
  },
  /* 556 */
  { Prefix0F, 2, { 0x0f, 0x5d, 0x00, 0x00 }, NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstMinps,
    2, g_Operands + 403,
    NULL
  },
  /* 557 */
  { Prefix0F, 2, { 0x0f, 0x5e, 0x00, 0x00 }, NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstDivps,
    2, g_Operands + 403,
    NULL
  },
  /* 558 */
  { Prefix0F, 2, { 0x0f, 0x5f, 0x00, 0x00 }, NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstMaxps,
    2, g_Operands + 403,
    NULL
  },
  /* 559 */
  { Prefix0F, 2, { 0x0f, 0x60, 0x00, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPunpcklbw,
    2, g_Operands + 411,
    NULL
  },
  /* 560 */
  { Prefix0F, 2, { 0x0f, 0x61, 0x00, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPunpcklwd,
    2, g_Operands + 413,
    NULL
  },
  /* 561 */
  { Prefix0F, 2, { 0x0f, 0x62, 0x00, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPunpckldq,
    2, g_Operands + 411,
    NULL
  },
  /* 562 */
  { Prefix0F, 2, { 0x0f, 0x63, 0x00, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPacksswb,
    2, g_Operands + 411,
    NULL
  },
  /* 563 */
  { Prefix0F, 2, { 0x0f, 0x64, 0x00, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPcmpgtb,
    2, g_Operands + 411,
    NULL
  },
  /* 564 */
  { Prefix0F, 2, { 0x0f, 0x65, 0x00, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPcmpgtw,
    2, g_Operands + 411,
    NULL
  },
  /* 565 */
  { Prefix0F, 2, { 0x0f, 0x66, 0x00, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPcmpgtd,
    2, g_Operands + 411,
    NULL
  },
  /* 566 */
  { Prefix0F, 2, { 0x0f, 0x67, 0x00, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPackuswb,
    2, g_Operands + 411,
    NULL
  },
  /* 567 */
  { Prefix0F, 2, { 0x0f, 0x68, 0x00, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPunpckhbw,
    2, g_Operands + 415,
    NULL
  },
  /* 568 */
  { Prefix0F, 2, { 0x0f, 0x69, 0x00, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPunpckhwd,
    2, g_Operands + 417,
    NULL
  },
  /* 569 */
  { Prefix0F, 2, { 0x0f, 0x6a, 0x00, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPunpckhdq,
    2, g_Operands + 415,
    NULL
  },
  /* 570 */
  { Prefix0F, 2, { 0x0f, 0x6b, 0x00, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPackssdw,
    2, g_Operands + 411,
    NULL
  },
  /* 571 */
  { Prefix0F, 2, { 0x0f, 0x6c, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 572 */
  { Prefix0F, 2, { 0x0f, 0x6d, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 573 */
  { Prefix0F, 2, { 0x0f, 0x6e, 0x00, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstMovd,
    2, g_Operands + 419,
    g_Opcodes + 574
  },
  /* 574 */
  { Prefix0F, 2, { 0x0f, 0x6e, 0x00, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_o),
    InstMovq,
    2, g_Operands + 421,
    NULL
  },
  /* 575 */
  { Prefix0F, 2, { 0x0f, 0x6f, 0x00, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstMovq,
    2, g_Operands + 423,
    NULL
  },
  /* 576 */
  { Prefix0F, 2, { 0x0f, 0x70, 0x00, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstPshufw,
    3, g_Operands + 425,
    NULL
  },
  /* 577 */
  { Prefix0F, 2, { 0x0f, 0x71, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    g_Opcodes + 578
  },
  /* 578 */
  { Prefix0F, 2, { 0x0f, 0x71, 0x01, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    g_Opcodes + 579
  },
  /* 579 */
  { Prefix0F, 2, { 0x0f, 0x71, 0x02, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstPsrlw,
    2, g_Operands + 428,
    g_Opcodes + 580
  },
  /* 580 */
  { Prefix0F, 2, { 0x0f, 0x71, 0x03, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    g_Opcodes + 581
  },
  /* 581 */
  { Prefix0F, 2, { 0x0f, 0x71, 0x04, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstPsraw,
    2, g_Operands + 430,
    g_Opcodes + 582
  },
  /* 582 */
  { Prefix0F, 2, { 0x0f, 0x71, 0x05, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    g_Opcodes + 583
  },
  /* 583 */
  { Prefix0F, 2, { 0x0f, 0x71, 0x06, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstPsllw,
    2, g_Operands + 428,
    g_Opcodes + 584
  },
  /* 584 */
  { Prefix0F, 2, { 0x0f, 0x71, 0x07, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 585 */
  { Prefix0F, 2, { 0x0f, 0x72, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    g_Opcodes + 586
  },
  /* 586 */
  { Prefix0F, 2, { 0x0f, 0x72, 0x01, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    g_Opcodes + 587
  },
  /* 587 */
  { Prefix0F, 2, { 0x0f, 0x72, 0x02, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstPsrld,
    2, g_Operands + 428,
    g_Opcodes + 588
  },
  /* 588 */
  { Prefix0F, 2, { 0x0f, 0x72, 0x03, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    g_Opcodes + 589
  },
  /* 589 */
  { Prefix0F, 2, { 0x0f, 0x72, 0x04, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstPsrad,
    2, g_Operands + 428,
    g_Opcodes + 590
  },
  /* 590 */
  { Prefix0F, 2, { 0x0f, 0x72, 0x05, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    g_Opcodes + 591
  },
  /* 591 */
  { Prefix0F, 2, { 0x0f, 0x72, 0x06, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstPslld,
    2, g_Operands + 428,
    g_Opcodes + 592
  },
  /* 592 */
  { Prefix0F, 2, { 0x0f, 0x72, 0x07, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 593 */
  { Prefix0F, 2, { 0x0f, 0x73, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    g_Opcodes + 594
  },
  /* 594 */
  { Prefix0F, 2, { 0x0f, 0x73, 0x01, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    g_Opcodes + 595
  },
  /* 595 */
  { Prefix0F, 2, { 0x0f, 0x73, 0x02, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstPsrlq,
    2, g_Operands + 428,
    g_Opcodes + 596
  },
  /* 596 */
  { Prefix0F, 2, { 0x0f, 0x73, 0x03, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    g_Opcodes + 597
  },
  /* 597 */
  { Prefix0F, 2, { 0x0f, 0x73, 0x04, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    g_Opcodes + 598
  },
  /* 598 */
  { Prefix0F, 2, { 0x0f, 0x73, 0x05, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    g_Opcodes + 599
  },
  /* 599 */
  { Prefix0F, 2, { 0x0f, 0x73, 0x06, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstPsllq,
    2, g_Operands + 428,
    g_Opcodes + 600
  },
  /* 600 */
  { Prefix0F, 2, { 0x0f, 0x73, 0x07, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 601 */
  { Prefix0F, 2, { 0x0f, 0x74, 0x00, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPcmpeqb,
    2, g_Operands + 411,
    NULL
  },
  /* 602 */
  { Prefix0F, 2, { 0x0f, 0x75, 0x00, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPcmpeqw,
    2, g_Operands + 411,
    NULL
  },
  /* 603 */
  { Prefix0F, 2, { 0x0f, 0x76, 0x00, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPcmpeqd,
    2, g_Operands + 411,
    NULL
  },
  /* 604 */
  { Prefix0F, 2, { 0x0f, 0x77, 0x00, 0x00 }, NACLi_MMX,
    NACL_EMPTY_IFLAGS,
    InstEmms,
    0, g_Operands + 0,
    NULL
  },
  /* 605 */
  { Prefix0F, 2, { 0x0f, 0x78, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 606 */
  { Prefix0F, 2, { 0x0f, 0x79, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 607 */
  { Prefix0F, 2, { 0x0f, 0x7a, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 608 */
  { Prefix0F, 2, { 0x0f, 0x7b, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 609 */
  { Prefix0F, 2, { 0x0f, 0x7c, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 610 */
  { Prefix0F, 2, { 0x0f, 0x7d, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 611 */
  { Prefix0F, 2, { 0x0f, 0x7e, 0x00, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstMovd,
    2, g_Operands + 432,
    g_Opcodes + 612
  },
  /* 612 */
  { Prefix0F, 2, { 0x0f, 0x7e, 0x00, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_o),
    InstMovq,
    2, g_Operands + 434,
    NULL
  },
  /* 613 */
  { Prefix0F, 2, { 0x0f, 0x7f, 0x00, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstMovq,
    2, g_Operands + 436,
    NULL
  },
  /* 614 */
  { Prefix0F, 2, { 0x0f, 0x80, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstJo,
    2, g_Operands + 277,
    NULL
  },
  /* 615 */
  { Prefix0F, 2, { 0x0f, 0x81, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstJno,
    2, g_Operands + 277,
    NULL
  },
  /* 616 */
  { Prefix0F, 2, { 0x0f, 0x82, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstJb,
    2, g_Operands + 277,
    NULL
  },
  /* 617 */
  { Prefix0F, 2, { 0x0f, 0x83, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstJnb,
    2, g_Operands + 277,
    NULL
  },
  /* 618 */
  { Prefix0F, 2, { 0x0f, 0x84, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstJz,
    2, g_Operands + 277,
    NULL
  },
  /* 619 */
  { Prefix0F, 2, { 0x0f, 0x85, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstJnz,
    2, g_Operands + 277,
    NULL
  },
  /* 620 */
  { Prefix0F, 2, { 0x0f, 0x86, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstJbe,
    2, g_Operands + 277,
    NULL
  },
  /* 621 */
  { Prefix0F, 2, { 0x0f, 0x87, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstJnbe,
    2, g_Operands + 277,
    NULL
  },
  /* 622 */
  { Prefix0F, 2, { 0x0f, 0x88, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstJs,
    2, g_Operands + 277,
    NULL
  },
  /* 623 */
  { Prefix0F, 2, { 0x0f, 0x89, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstJns,
    2, g_Operands + 277,
    NULL
  },
  /* 624 */
  { Prefix0F, 2, { 0x0f, 0x8a, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstJp,
    2, g_Operands + 277,
    NULL
  },
  /* 625 */
  { Prefix0F, 2, { 0x0f, 0x8b, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstJnp,
    2, g_Operands + 277,
    NULL
  },
  /* 626 */
  { Prefix0F, 2, { 0x0f, 0x8c, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstJl,
    2, g_Operands + 277,
    NULL
  },
  /* 627 */
  { Prefix0F, 2, { 0x0f, 0x8d, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstJnl,
    2, g_Operands + 277,
    NULL
  },
  /* 628 */
  { Prefix0F, 2, { 0x0f, 0x8e, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstJle,
    2, g_Operands + 277,
    NULL
  },
  /* 629 */
  { Prefix0F, 2, { 0x0f, 0x8f, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstJnle,
    2, g_Operands + 277,
    NULL
  },
  /* 630 */
  { Prefix0F, 2, { 0x0f, 0x90, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSeto,
    1, g_Operands + 80,
    NULL
  },
  /* 631 */
  { Prefix0F, 2, { 0x0f, 0x91, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetno,
    1, g_Operands + 80,
    NULL
  },
  /* 632 */
  { Prefix0F, 2, { 0x0f, 0x92, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetb,
    1, g_Operands + 80,
    NULL
  },
  /* 633 */
  { Prefix0F, 2, { 0x0f, 0x93, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetnb,
    1, g_Operands + 80,
    NULL
  },
  /* 634 */
  { Prefix0F, 2, { 0x0f, 0x94, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetz,
    1, g_Operands + 80,
    NULL
  },
  /* 635 */
  { Prefix0F, 2, { 0x0f, 0x95, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetnz,
    1, g_Operands + 80,
    NULL
  },
  /* 636 */
  { Prefix0F, 2, { 0x0f, 0x96, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetbe,
    1, g_Operands + 80,
    NULL
  },
  /* 637 */
  { Prefix0F, 2, { 0x0f, 0x97, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetnbe,
    1, g_Operands + 80,
    NULL
  },
  /* 638 */
  { Prefix0F, 2, { 0x0f, 0x98, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSets,
    1, g_Operands + 80,
    NULL
  },
  /* 639 */
  { Prefix0F, 2, { 0x0f, 0x99, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetns,
    1, g_Operands + 80,
    NULL
  },
  /* 640 */
  { Prefix0F, 2, { 0x0f, 0x9a, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetp,
    1, g_Operands + 80,
    NULL
  },
  /* 641 */
  { Prefix0F, 2, { 0x0f, 0x9b, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetnp,
    1, g_Operands + 80,
    NULL
  },
  /* 642 */
  { Prefix0F, 2, { 0x0f, 0x9c, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetl,
    1, g_Operands + 80,
    NULL
  },
  /* 643 */
  { Prefix0F, 2, { 0x0f, 0x9d, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetnl,
    1, g_Operands + 80,
    NULL
  },
  /* 644 */
  { Prefix0F, 2, { 0x0f, 0x9e, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetle,
    1, g_Operands + 80,
    NULL
  },
  /* 645 */
  { Prefix0F, 2, { 0x0f, 0x9f, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetnle,
    1, g_Operands + 80,
    NULL
  },
  /* 646 */
  { Prefix0F, 2, { 0x0f, 0xa0, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(NaClIllegal) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPush,
    2, g_Operands + 438,
    NULL
  },
  /* 647 */
  { Prefix0F, 2, { 0x0f, 0xa1, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(NaClIllegal) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPop,
    2, g_Operands + 440,
    NULL
  },
  /* 648 */
  { Prefix0F, 2, { 0x0f, 0xa2, 0x00, 0x00 }, NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstCpuid,
    4, g_Operands + 442,
    NULL
  },
  /* 649 */
  { Prefix0F, 2, { 0x0f, 0xa3, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal),
    InstBt,
    2, g_Operands + 16,
    NULL
  },
  /* 650 */
  { Prefix0F, 2, { 0x0f, 0xa4, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstShld,
    3, g_Operands + 446,
    NULL
  },
  /* 651 */
  { Prefix0F, 2, { 0x0f, 0xa5, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstShld,
    3, g_Operands + 449,
    NULL
  },
  /* 652 */
  { Prefix0F, 2, { 0x0f, 0xa6, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 653 */
  { Prefix0F, 2, { 0x0f, 0xa7, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 654 */
  { Prefix0F, 2, { 0x0f, 0xa8, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(NaClIllegal) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPush,
    2, g_Operands + 452,
    NULL
  },
  /* 655 */
  { Prefix0F, 2, { 0x0f, 0xa9, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(NaClIllegal) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPop,
    2, g_Operands + 454,
    NULL
  },
  /* 656 */
  { Prefix0F, 2, { 0x0f, 0xaa, 0x00, 0x00 }, NACLi_SYSTEM,
    NACL_IFLAG(NaClIllegal),
    InstRsm,
    0, g_Operands + 0,
    NULL
  },
  /* 657 */
  { Prefix0F, 2, { 0x0f, 0xab, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal),
    InstBts,
    2, g_Operands + 2,
    NULL
  },
  /* 658 */
  { Prefix0F, 2, { 0x0f, 0xac, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstShrd,
    3, g_Operands + 456,
    NULL
  },
  /* 659 */
  { Prefix0F, 2, { 0x0f, 0xad, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstShrd,
    3, g_Operands + 459,
    NULL
  },
  /* 660 */
  { Prefix0F, 2, { 0x0f, 0xae, 0x00, 0x00 }, NACLi_FXSAVE,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(NaClIllegal),
    InstFxsave,
    1, g_Operands + 223,
    g_Opcodes + 661
  },
  /* 661 */
  { Prefix0F, 2, { 0x0f, 0xae, 0x01, 0x00 }, NACLi_FXSAVE,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(NaClIllegal),
    InstFxrstor,
    1, g_Operands + 221,
    g_Opcodes + 662
  },
  /* 662 */
  { Prefix0F, 2, { 0x0f, 0xae, 0x02, 0x00 }, NACLi_SSE,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstLdmxcsr,
    1, g_Operands + 208,
    g_Opcodes + 663
  },
  /* 663 */
  { Prefix0F, 2, { 0x0f, 0xae, 0x03, 0x00 }, NACLi_SSE,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstStmxcsr,
    1, g_Operands + 217,
    g_Opcodes + 664
  },
  /* 664 */
  { Prefix0F, 2, { 0x0f, 0xae, 0x04, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    g_Opcodes + 665
  },
  /* 665 */
  { Prefix0F, 2, { 0x0f, 0xae, 0x05, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstLfence,
    0, g_Operands + 0,
    g_Opcodes + 666
  },
  /* 666 */
  { Prefix0F, 2, { 0x0f, 0xae, 0x05, 0x01 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstLfence,
    0, g_Operands + 0,
    g_Opcodes + 667
  },
  /* 667 */
  { Prefix0F, 2, { 0x0f, 0xae, 0x05, 0x02 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstLfence,
    0, g_Operands + 0,
    g_Opcodes + 668
  },
  /* 668 */
  { Prefix0F, 2, { 0x0f, 0xae, 0x05, 0x03 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstLfence,
    0, g_Operands + 0,
    g_Opcodes + 669
  },
  /* 669 */
  { Prefix0F, 2, { 0x0f, 0xae, 0x05, 0x04 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstLfence,
    0, g_Operands + 0,
    g_Opcodes + 670
  },
  /* 670 */
  { Prefix0F, 2, { 0x0f, 0xae, 0x05, 0x05 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstLfence,
    0, g_Operands + 0,
    g_Opcodes + 671
  },
  /* 671 */
  { Prefix0F, 2, { 0x0f, 0xae, 0x05, 0x06 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstLfence,
    0, g_Operands + 0,
    g_Opcodes + 672
  },
  /* 672 */
  { Prefix0F, 2, { 0x0f, 0xae, 0x05, 0x07 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstLfence,
    0, g_Operands + 0,
    g_Opcodes + 673
  },
  /* 673 */
  { Prefix0F, 2, { 0x0f, 0xae, 0x06, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMfence,
    0, g_Operands + 0,
    g_Opcodes + 674
  },
  /* 674 */
  { Prefix0F, 2, { 0x0f, 0xae, 0x06, 0x01 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMfence,
    0, g_Operands + 0,
    g_Opcodes + 675
  },
  /* 675 */
  { Prefix0F, 2, { 0x0f, 0xae, 0x06, 0x02 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMfence,
    0, g_Operands + 0,
    g_Opcodes + 676
  },
  /* 676 */
  { Prefix0F, 2, { 0x0f, 0xae, 0x06, 0x03 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMfence,
    0, g_Operands + 0,
    g_Opcodes + 677
  },
  /* 677 */
  { Prefix0F, 2, { 0x0f, 0xae, 0x06, 0x04 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMfence,
    0, g_Operands + 0,
    g_Opcodes + 678
  },
  /* 678 */
  { Prefix0F, 2, { 0x0f, 0xae, 0x06, 0x05 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMfence,
    0, g_Operands + 0,
    g_Opcodes + 679
  },
  /* 679 */
  { Prefix0F, 2, { 0x0f, 0xae, 0x06, 0x06 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMfence,
    0, g_Operands + 0,
    g_Opcodes + 680
  },
  /* 680 */
  { Prefix0F, 2, { 0x0f, 0xae, 0x06, 0x07 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMfence,
    0, g_Operands + 0,
    g_Opcodes + 681
  },
  /* 681 */
  { Prefix0F, 2, { 0x0f, 0xae, 0x07, 0x00 }, NACLi_SFENCE_CLFLUSH,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstSfence,
    0, g_Operands + 0,
    g_Opcodes + 682
  },
  /* 682 */
  { Prefix0F, 2, { 0x0f, 0xae, 0x07, 0x01 }, NACLi_SFENCE_CLFLUSH,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstSfence,
    0, g_Operands + 0,
    g_Opcodes + 683
  },
  /* 683 */
  { Prefix0F, 2, { 0x0f, 0xae, 0x07, 0x02 }, NACLi_SFENCE_CLFLUSH,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstSfence,
    0, g_Operands + 0,
    g_Opcodes + 684
  },
  /* 684 */
  { Prefix0F, 2, { 0x0f, 0xae, 0x07, 0x03 }, NACLi_SFENCE_CLFLUSH,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstSfence,
    0, g_Operands + 0,
    g_Opcodes + 685
  },
  /* 685 */
  { Prefix0F, 2, { 0x0f, 0xae, 0x07, 0x04 }, NACLi_SFENCE_CLFLUSH,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstSfence,
    0, g_Operands + 0,
    g_Opcodes + 686
  },
  /* 686 */
  { Prefix0F, 2, { 0x0f, 0xae, 0x07, 0x05 }, NACLi_SFENCE_CLFLUSH,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstSfence,
    0, g_Operands + 0,
    g_Opcodes + 687
  },
  /* 687 */
  { Prefix0F, 2, { 0x0f, 0xae, 0x07, 0x06 }, NACLi_SFENCE_CLFLUSH,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstSfence,
    0, g_Operands + 0,
    g_Opcodes + 688
  },
  /* 688 */
  { Prefix0F, 2, { 0x0f, 0xae, 0x07, 0x07 }, NACLi_SFENCE_CLFLUSH,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstSfence,
    0, g_Operands + 0,
    g_Opcodes + 689
  },
  /* 689 */
  { Prefix0F, 2, { 0x0f, 0xae, 0x07, 0x00 }, NACLi_SFENCE_CLFLUSH,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstClflush,
    1, g_Operands + 330,
    NULL
  },
  /* 690 */
  { Prefix0F, 2, { 0x0f, 0xaf, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstImul,
    2, g_Operands + 6,
    NULL
  },
  /* 691 */
  { Prefix0F, 2, { 0x0f, 0xb0, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstCmpxchg,
    3, g_Operands + 462,
    NULL
  },
  /* 692 */
  { Prefix0F, 2, { 0x0f, 0xb1, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCmpxchg,
    3, g_Operands + 465,
    NULL
  },
  /* 693 */
  { Prefix0F, 2, { 0x0f, 0xb2, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal),
    InstLss,
    2, g_Operands + 468,
    NULL
  },
  /* 694 */
  { Prefix0F, 2, { 0x0f, 0xb3, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal),
    InstBtr,
    2, g_Operands + 2,
    NULL
  },
  /* 695 */
  { Prefix0F, 2, { 0x0f, 0xb4, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal),
    InstLfs,
    2, g_Operands + 470,
    NULL
  },
  /* 696 */
  { Prefix0F, 2, { 0x0f, 0xb5, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal),
    InstLgs,
    2, g_Operands + 468,
    NULL
  },
  /* 697 */
  { Prefix0F, 2, { 0x0f, 0xb6, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstMovzx,
    2, g_Operands + 472,
    NULL
  },
  /* 698 */
  { Prefix0F, 2, { 0x0f, 0xb7, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstMovzx,
    2, g_Operands + 474,
    NULL
  },
  /* 699 */
  { Prefix0F, 2, { 0x0f, 0xb8, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 700 */
  { Prefix0F, 2, { 0x0f, 0xb9, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 701 */
  { Prefix0F, 2, { 0x0f, 0xba, 0x04, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstBt,
    2, g_Operands + 44,
    g_Opcodes + 702
  },
  /* 702 */
  { Prefix0F, 2, { 0x0f, 0xba, 0x05, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstBts,
    2, g_Operands + 72,
    g_Opcodes + 703
  },
  /* 703 */
  { Prefix0F, 2, { 0x0f, 0xba, 0x06, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstBtr,
    2, g_Operands + 72,
    g_Opcodes + 704
  },
  /* 704 */
  { Prefix0F, 2, { 0x0f, 0xba, 0x07, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstBtc,
    2, g_Operands + 72,
    g_Opcodes + 705
  },
  /* 705 */
  { Prefix0F, 2, { 0x0f, 0xba, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 706 */
  { Prefix0F, 2, { 0x0f, 0xbb, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal),
    InstBtc,
    2, g_Operands + 2,
    NULL
  },
  /* 707 */
  { Prefix0F, 2, { 0x0f, 0xbc, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstBsf,
    2, g_Operands + 38,
    NULL
  },
  /* 708 */
  { Prefix0F, 2, { 0x0f, 0xbd, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstBsr,
    2, g_Operands + 38,
    NULL
  },
  /* 709 */
  { Prefix0F, 2, { 0x0f, 0xbe, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstMovsx,
    2, g_Operands + 472,
    NULL
  },
  /* 710 */
  { Prefix0F, 2, { 0x0f, 0xbf, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstMovsx,
    2, g_Operands + 476,
    NULL
  },
  /* 711 */
  { Prefix0F, 2, { 0x0f, 0xc0, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstXadd,
    2, g_Operands + 76,
    NULL
  },
  /* 712 */
  { Prefix0F, 2, { 0x0f, 0xc1, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstXadd,
    2, g_Operands + 78,
    NULL
  },
  /* 713 */
  { Prefix0F, 2, { 0x0f, 0xc2, 0x00, 0x00 }, NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstCmpps,
    3, g_Operands + 478,
    NULL
  },
  /* 714 */
  { Prefix0F, 2, { 0x0f, 0xc3, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstMovnti,
    2, g_Operands + 481,
    NULL
  },
  /* 715 */
  { Prefix0F, 2, { 0x0f, 0xc4, 0x00, 0x00 }, NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstPinsrw,
    3, g_Operands + 483,
    NULL
  },
  /* 716 */
  { Prefix0F, 2, { 0x0f, 0xc5, 0x00, 0x00 }, NACLi_SSE41,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstPextrw,
    3, g_Operands + 486,
    NULL
  },
  /* 717 */
  { Prefix0F, 2, { 0x0f, 0xc6, 0x00, 0x00 }, NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstShufps,
    3, g_Operands + 478,
    NULL
  },
  /* 718 */
  { Prefix0F, 2, { 0x0f, 0xc7, 0x01, 0x00 }, NACLi_CMPXCHG8B,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_v),
    InstCmpxchg8b,
    3, g_Operands + 489,
    g_Opcodes + 719
  },
  /* 719 */
  { Prefix0F, 2, { 0x0f, 0xc7, 0x01, 0x00 }, NACLi_CMPXCHG16B,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_o),
    InstCmpxchg16b,
    3, g_Operands + 492,
    g_Opcodes + 720
  },
  /* 720 */
  { Prefix0F, 2, { 0x0f, 0xc7, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 721 */
  { Prefix0F, 2, { 0x0f, 0xc8, 0x00, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstBswap,
    1, g_Operands + 94,
    NULL
  },
  /* 722 */
  { Prefix0F, 2, { 0x0f, 0xc9, 0x01, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstBswap,
    1, g_Operands + 94,
    NULL
  },
  /* 723 */
  { Prefix0F, 2, { 0x0f, 0xca, 0x02, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstBswap,
    1, g_Operands + 94,
    NULL
  },
  /* 724 */
  { Prefix0F, 2, { 0x0f, 0xcb, 0x03, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstBswap,
    1, g_Operands + 94,
    NULL
  },
  /* 725 */
  { Prefix0F, 2, { 0x0f, 0xcc, 0x04, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstBswap,
    1, g_Operands + 94,
    NULL
  },
  /* 726 */
  { Prefix0F, 2, { 0x0f, 0xcd, 0x05, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstBswap,
    1, g_Operands + 94,
    NULL
  },
  /* 727 */
  { Prefix0F, 2, { 0x0f, 0xce, 0x06, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstBswap,
    1, g_Operands + 94,
    NULL
  },
  /* 728 */
  { Prefix0F, 2, { 0x0f, 0xcf, 0x07, 0x00 }, NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstBswap,
    1, g_Operands + 94,
    NULL
  },
  /* 729 */
  { Prefix0F, 2, { 0x0f, 0xd0, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 730 */
  { Prefix0F, 2, { 0x0f, 0xd1, 0x00, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsrlw,
    2, g_Operands + 411,
    NULL
  },
  /* 731 */
  { Prefix0F, 2, { 0x0f, 0xd2, 0x00, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsrld,
    2, g_Operands + 411,
    NULL
  },
  /* 732 */
  { Prefix0F, 2, { 0x0f, 0xd3, 0x00, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsrlq,
    2, g_Operands + 411,
    NULL
  },
  /* 733 */
  { Prefix0F, 2, { 0x0f, 0xd4, 0x00, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPaddq,
    2, g_Operands + 411,
    NULL
  },
  /* 734 */
  { Prefix0F, 2, { 0x0f, 0xd5, 0x00, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPmullw,
    2, g_Operands + 411,
    NULL
  },
  /* 735 */
  { Prefix0F, 2, { 0x0f, 0xd6, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 736 */
  { Prefix0F, 2, { 0x0f, 0xd7, 0x00, 0x00 }, NACLi_MMX,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstPmovmskb,
    2, g_Operands + 486,
    NULL
  },
  /* 737 */
  { Prefix0F, 2, { 0x0f, 0xd8, 0x00, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsubusb,
    2, g_Operands + 411,
    NULL
  },
  /* 738 */
  { Prefix0F, 2, { 0x0f, 0xd9, 0x00, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsubusw,
    2, g_Operands + 411,
    NULL
  },
  /* 739 */
  { Prefix0F, 2, { 0x0f, 0xda, 0x00, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPminub,
    2, g_Operands + 411,
    NULL
  },
  /* 740 */
  { Prefix0F, 2, { 0x0f, 0xdb, 0x00, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPand,
    2, g_Operands + 411,
    NULL
  },
  /* 741 */
  { Prefix0F, 2, { 0x0f, 0xdc, 0x00, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPaddusb,
    2, g_Operands + 411,
    NULL
  },
  /* 742 */
  { Prefix0F, 2, { 0x0f, 0xdd, 0x00, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPaddusw,
    2, g_Operands + 411,
    NULL
  },
  /* 743 */
  { Prefix0F, 2, { 0x0f, 0xde, 0x00, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPmaxub,
    2, g_Operands + 411,
    NULL
  },
  /* 744 */
  { Prefix0F, 2, { 0x0f, 0xdf, 0x00, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPandn,
    2, g_Operands + 411,
    NULL
  },
  /* 745 */
  { Prefix0F, 2, { 0x0f, 0xe0, 0x00, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPavgb,
    2, g_Operands + 411,
    NULL
  },
  /* 746 */
  { Prefix0F, 2, { 0x0f, 0xe1, 0x00, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsraw,
    2, g_Operands + 411,
    NULL
  },
  /* 747 */
  { Prefix0F, 2, { 0x0f, 0xe2, 0x00, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsrad,
    2, g_Operands + 411,
    NULL
  },
  /* 748 */
  { Prefix0F, 2, { 0x0f, 0xe3, 0x00, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPavgw,
    2, g_Operands + 411,
    NULL
  },
  /* 749 */
  { Prefix0F, 2, { 0x0f, 0xe4, 0x00, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPmulhuw,
    2, g_Operands + 411,
    NULL
  },
  /* 750 */
  { Prefix0F, 2, { 0x0f, 0xe5, 0x00, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPmulhw,
    2, g_Operands + 411,
    NULL
  },
  /* 751 */
  { Prefix0F, 2, { 0x0f, 0xe6, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 752 */
  { Prefix0F, 2, { 0x0f, 0xe7, 0x00, 0x00 }, NACLi_MMX,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMovntq,
    2, g_Operands + 495,
    NULL
  },
  /* 753 */
  { Prefix0F, 2, { 0x0f, 0xe8, 0x00, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsubsb,
    2, g_Operands + 411,
    NULL
  },
  /* 754 */
  { Prefix0F, 2, { 0x0f, 0xe9, 0x00, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsubsw,
    2, g_Operands + 411,
    NULL
  },
  /* 755 */
  { Prefix0F, 2, { 0x0f, 0xea, 0x00, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPminsw,
    2, g_Operands + 411,
    NULL
  },
  /* 756 */
  { Prefix0F, 2, { 0x0f, 0xeb, 0x00, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPor,
    2, g_Operands + 411,
    NULL
  },
  /* 757 */
  { Prefix0F, 2, { 0x0f, 0xec, 0x00, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPaddsb,
    2, g_Operands + 411,
    NULL
  },
  /* 758 */
  { Prefix0F, 2, { 0x0f, 0xed, 0x00, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPaddsw,
    2, g_Operands + 411,
    NULL
  },
  /* 759 */
  { Prefix0F, 2, { 0x0f, 0xee, 0x00, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPmaxsw,
    2, g_Operands + 411,
    NULL
  },
  /* 760 */
  { Prefix0F, 2, { 0x0f, 0xef, 0x00, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPxor,
    2, g_Operands + 411,
    NULL
  },
  /* 761 */
  { Prefix0F, 2, { 0x0f, 0xf0, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 762 */
  { Prefix0F, 2, { 0x0f, 0xf1, 0x00, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsllw,
    2, g_Operands + 411,
    NULL
  },
  /* 763 */
  { Prefix0F, 2, { 0x0f, 0xf2, 0x00, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPslld,
    2, g_Operands + 411,
    NULL
  },
  /* 764 */
  { Prefix0F, 2, { 0x0f, 0xf3, 0x00, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsllq,
    2, g_Operands + 411,
    NULL
  },
  /* 765 */
  { Prefix0F, 2, { 0x0f, 0xf4, 0x00, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPmuludq,
    2, g_Operands + 411,
    NULL
  },
  /* 766 */
  { Prefix0F, 2, { 0x0f, 0xf5, 0x00, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPmaddwd,
    2, g_Operands + 411,
    NULL
  },
  /* 767 */
  { Prefix0F, 2, { 0x0f, 0xf6, 0x00, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsadbw,
    2, g_Operands + 411,
    NULL
  },
  /* 768 */
  { Prefix0F, 2, { 0x0f, 0xf7, 0x00, 0x00 }, NACLi_MMX,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_v),
    InstMaskmovq,
    3, g_Operands + 497,
    NULL
  },
  /* 769 */
  { Prefix0F, 2, { 0x0f, 0xf8, 0x00, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsubb,
    2, g_Operands + 411,
    NULL
  },
  /* 770 */
  { Prefix0F, 2, { 0x0f, 0xf9, 0x00, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsubw,
    2, g_Operands + 411,
    NULL
  },
  /* 771 */
  { Prefix0F, 2, { 0x0f, 0xfa, 0x00, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsubd,
    2, g_Operands + 411,
    NULL
  },
  /* 772 */
  { Prefix0F, 2, { 0x0f, 0xfb, 0x00, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsubq,
    2, g_Operands + 411,
    NULL
  },
  /* 773 */
  { Prefix0F, 2, { 0x0f, 0xfc, 0x00, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPaddb,
    2, g_Operands + 411,
    NULL
  },
  /* 774 */
  { Prefix0F, 2, { 0x0f, 0xfd, 0x00, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPaddw,
    2, g_Operands + 411,
    NULL
  },
  /* 775 */
  { Prefix0F, 2, { 0x0f, 0xfe, 0x00, 0x00 }, NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPaddd,
    2, g_Operands + 411,
    NULL
  },
  /* 776 */
  { Prefix0F, 2, { 0x0f, 0xff, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 777 */
  { PrefixF20F, 2, { 0x0f, 0x10, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstMovsd,
    2, g_Operands + 500,
    NULL
  },
  /* 778 */
  { PrefixF20F, 2, { 0x0f, 0x11, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstMovsd,
    2, g_Operands + 502,
    NULL
  },
  /* 779 */
  { PrefixF20F, 2, { 0x0f, 0x12, 0x00, 0x00 }, NACLi_SSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstMovddup,
    2, g_Operands + 504,
    NULL
  },
  /* 780 */
  { PrefixF20F, 2, { 0x0f, 0x13, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 781 */
  { PrefixF20F, 2, { 0x0f, 0x14, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 782 */
  { PrefixF20F, 2, { 0x0f, 0x15, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 783 */
  { PrefixF20F, 2, { 0x0f, 0x16, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 784 */
  { PrefixF20F, 2, { 0x0f, 0x17, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 785 */
  { PrefixF20F, 2, { 0x0f, 0x28, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 786 */
  { PrefixF20F, 2, { 0x0f, 0x29, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 787 */
  { PrefixF20F, 2, { 0x0f, 0x2a, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCvtsi2sd,
    2, g_Operands + 506,
    NULL
  },
  /* 788 */
  { PrefixF20F, 2, { 0x0f, 0x2b, 0x00, 0x00 }, NACLi_SSE4A,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstMovntsd,
    2, g_Operands + 508,
    NULL
  },
  /* 789 */
  { PrefixF20F, 2, { 0x0f, 0x2c, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCvttsd2si,
    2, g_Operands + 510,
    NULL
  },
  /* 790 */
  { PrefixF20F, 2, { 0x0f, 0x2d, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCvtsd2si,
    2, g_Operands + 512,
    NULL
  },
  /* 791 */
  { PrefixF20F, 2, { 0x0f, 0x2e, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 792 */
  { PrefixF20F, 2, { 0x0f, 0x2f, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 793 */
  { PrefixF20F, 2, { 0x0f, 0x50, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 794 */
  { PrefixF20F, 2, { 0x0f, 0x51, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstSqrtsd,
    2, g_Operands + 500,
    NULL
  },
  /* 795 */
  { PrefixF20F, 2, { 0x0f, 0x52, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 796 */
  { PrefixF20F, 2, { 0x0f, 0x53, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 797 */
  { PrefixF20F, 2, { 0x0f, 0x54, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 798 */
  { PrefixF20F, 2, { 0x0f, 0x55, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 799 */
  { PrefixF20F, 2, { 0x0f, 0x56, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 800 */
  { PrefixF20F, 2, { 0x0f, 0x57, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 801 */
  { PrefixF20F, 2, { 0x0f, 0x58, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstAddsd,
    2, g_Operands + 514,
    NULL
  },
  /* 802 */
  { PrefixF20F, 2, { 0x0f, 0x59, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstMulsd,
    2, g_Operands + 516,
    NULL
  },
  /* 803 */
  { PrefixF20F, 2, { 0x0f, 0x5a, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstCvtsd2ss,
    2, g_Operands + 518,
    NULL
  },
  /* 804 */
  { PrefixF20F, 2, { 0x0f, 0x5b, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 805 */
  { PrefixF20F, 2, { 0x0f, 0x5c, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstSubsd,
    2, g_Operands + 514,
    NULL
  },
  /* 806 */
  { PrefixF20F, 2, { 0x0f, 0x5d, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstMinsd,
    2, g_Operands + 514,
    NULL
  },
  /* 807 */
  { PrefixF20F, 2, { 0x0f, 0x5e, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstDivsd,
    2, g_Operands + 514,
    NULL
  },
  /* 808 */
  { PrefixF20F, 2, { 0x0f, 0x5f, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstMaxsd,
    2, g_Operands + 514,
    NULL
  },
  /* 809 */
  { PrefixF20F, 2, { 0x0f, 0x60, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 810 */
  { PrefixF20F, 2, { 0x0f, 0x61, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 811 */
  { PrefixF20F, 2, { 0x0f, 0x62, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 812 */
  { PrefixF20F, 2, { 0x0f, 0x63, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 813 */
  { PrefixF20F, 2, { 0x0f, 0x64, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 814 */
  { PrefixF20F, 2, { 0x0f, 0x65, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 815 */
  { PrefixF20F, 2, { 0x0f, 0x66, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 816 */
  { PrefixF20F, 2, { 0x0f, 0x67, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 817 */
  { PrefixF20F, 2, { 0x0f, 0x68, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 818 */
  { PrefixF20F, 2, { 0x0f, 0x69, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 819 */
  { PrefixF20F, 2, { 0x0f, 0x6a, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 820 */
  { PrefixF20F, 2, { 0x0f, 0x6b, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 821 */
  { PrefixF20F, 2, { 0x0f, 0x6c, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 822 */
  { PrefixF20F, 2, { 0x0f, 0x6d, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 823 */
  { PrefixF20F, 2, { 0x0f, 0x6e, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 824 */
  { PrefixF20F, 2, { 0x0f, 0x6f, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 825 */
  { PrefixF20F, 2, { 0x0f, 0x70, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsRepne),
    InstPshuflw,
    3, g_Operands + 520,
    NULL
  },
  /* 826 */
  { PrefixF20F, 2, { 0x0f, 0x71, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 827 */
  { PrefixF20F, 2, { 0x0f, 0x72, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 828 */
  { PrefixF20F, 2, { 0x0f, 0x73, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 829 */
  { PrefixF20F, 2, { 0x0f, 0x74, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 830 */
  { PrefixF20F, 2, { 0x0f, 0x75, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 831 */
  { PrefixF20F, 2, { 0x0f, 0x76, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 832 */
  { PrefixF20F, 2, { 0x0f, 0x77, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 833 */
  { PrefixF20F, 2, { 0x0f, 0x78, 0x00, 0x00 }, NACLi_SSE4A,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeHasImmed2_b) | NACL_IFLAG(OpcodeAllowsRepne),
    InstInsertq,
    4, g_Operands + 523,
    NULL
  },
  /* 834 */
  { PrefixF20F, 2, { 0x0f, 0x79, 0x00, 0x00 }, NACLi_SSE4A,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstInsertq,
    2, g_Operands + 527,
    NULL
  },
  /* 835 */
  { PrefixF20F, 2, { 0x0f, 0x7a, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 836 */
  { PrefixF20F, 2, { 0x0f, 0x7b, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 837 */
  { PrefixF20F, 2, { 0x0f, 0x7c, 0x00, 0x00 }, NACLi_SSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstHaddps,
    2, g_Operands + 403,
    NULL
  },
  /* 838 */
  { PrefixF20F, 2, { 0x0f, 0x7d, 0x00, 0x00 }, NACLi_SSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstHsubps,
    2, g_Operands + 403,
    NULL
  },
  /* 839 */
  { PrefixF20F, 2, { 0x0f, 0x7e, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 840 */
  { PrefixF20F, 2, { 0x0f, 0x7f, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 841 */
  { PrefixF20F, 2, { 0x0f, 0xae, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 842 */
  { PrefixF20F, 2, { 0x0f, 0xb8, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 843 */
  { PrefixF20F, 2, { 0x0f, 0xb9, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 844 */
  { PrefixF20F, 2, { 0x0f, 0xba, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 845 */
  { PrefixF20F, 2, { 0x0f, 0xbb, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 846 */
  { PrefixF20F, 2, { 0x0f, 0xbc, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 847 */
  { PrefixF20F, 2, { 0x0f, 0xbd, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 848 */
  { PrefixF20F, 2, { 0x0f, 0xbe, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 849 */
  { PrefixF20F, 2, { 0x0f, 0xbf, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 850 */
  { PrefixF20F, 2, { 0x0f, 0xc2, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsRepne),
    InstCmpsd_xmm,
    3, g_Operands + 529,
    NULL
  },
  /* 851 */
  { PrefixF20F, 2, { 0x0f, 0xc3, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 852 */
  { PrefixF20F, 2, { 0x0f, 0xc4, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 853 */
  { PrefixF20F, 2, { 0x0f, 0xc5, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 854 */
  { PrefixF20F, 2, { 0x0f, 0xc6, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 855 */
  { PrefixF20F, 2, { 0x0f, 0xd0, 0x00, 0x00 }, NACLi_SSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstAddsubps,
    2, g_Operands + 532,
    NULL
  },
  /* 856 */
  { PrefixF20F, 2, { 0x0f, 0xd1, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 857 */
  { PrefixF20F, 2, { 0x0f, 0xd2, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 858 */
  { PrefixF20F, 2, { 0x0f, 0xd3, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 859 */
  { PrefixF20F, 2, { 0x0f, 0xd4, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 860 */
  { PrefixF20F, 2, { 0x0f, 0xd5, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 861 */
  { PrefixF20F, 2, { 0x0f, 0xd6, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstMovdq2q,
    2, g_Operands + 534,
    NULL
  },
  /* 862 */
  { PrefixF20F, 2, { 0x0f, 0xd7, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 863 */
  { PrefixF20F, 2, { 0x0f, 0xd8, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 864 */
  { PrefixF20F, 2, { 0x0f, 0xd9, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 865 */
  { PrefixF20F, 2, { 0x0f, 0xda, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 866 */
  { PrefixF20F, 2, { 0x0f, 0xdb, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 867 */
  { PrefixF20F, 2, { 0x0f, 0xdc, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 868 */
  { PrefixF20F, 2, { 0x0f, 0xdd, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 869 */
  { PrefixF20F, 2, { 0x0f, 0xde, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 870 */
  { PrefixF20F, 2, { 0x0f, 0xdf, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 871 */
  { PrefixF20F, 2, { 0x0f, 0xe0, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 872 */
  { PrefixF20F, 2, { 0x0f, 0xe1, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 873 */
  { PrefixF20F, 2, { 0x0f, 0xe2, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 874 */
  { PrefixF20F, 2, { 0x0f, 0xe3, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 875 */
  { PrefixF20F, 2, { 0x0f, 0xe4, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 876 */
  { PrefixF20F, 2, { 0x0f, 0xe5, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 877 */
  { PrefixF20F, 2, { 0x0f, 0xe6, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstCvtpd2dq,
    2, g_Operands + 536,
    NULL
  },
  /* 878 */
  { PrefixF20F, 2, { 0x0f, 0xe7, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 879 */
  { PrefixF20F, 2, { 0x0f, 0xe8, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 880 */
  { PrefixF20F, 2, { 0x0f, 0xe9, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 881 */
  { PrefixF20F, 2, { 0x0f, 0xea, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 882 */
  { PrefixF20F, 2, { 0x0f, 0xeb, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 883 */
  { PrefixF20F, 2, { 0x0f, 0xec, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 884 */
  { PrefixF20F, 2, { 0x0f, 0xed, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 885 */
  { PrefixF20F, 2, { 0x0f, 0xee, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 886 */
  { PrefixF20F, 2, { 0x0f, 0xef, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 887 */
  { PrefixF20F, 2, { 0x0f, 0xf0, 0x00, 0x00 }, NACLi_SSE3,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstLddqu,
    2, g_Operands + 538,
    NULL
  },
  /* 888 */
  { PrefixF20F, 2, { 0x0f, 0xf1, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 889 */
  { PrefixF20F, 2, { 0x0f, 0xf2, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 890 */
  { PrefixF20F, 2, { 0x0f, 0xf3, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 891 */
  { PrefixF20F, 2, { 0x0f, 0xf4, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 892 */
  { PrefixF20F, 2, { 0x0f, 0xf5, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 893 */
  { PrefixF20F, 2, { 0x0f, 0xf6, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 894 */
  { PrefixF20F, 2, { 0x0f, 0xf7, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 895 */
  { PrefixF20F, 2, { 0x0f, 0xf8, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 896 */
  { PrefixF20F, 2, { 0x0f, 0xf9, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 897 */
  { PrefixF20F, 2, { 0x0f, 0xfa, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 898 */
  { PrefixF20F, 2, { 0x0f, 0xfb, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 899 */
  { PrefixF20F, 2, { 0x0f, 0xfc, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 900 */
  { PrefixF20F, 2, { 0x0f, 0xfd, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 901 */
  { PrefixF20F, 2, { 0x0f, 0xfe, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 902 */
  { PrefixF20F, 2, { 0x0f, 0xff, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 903 */
  { PrefixF30F, 2, { 0x0f, 0x10, 0x00, 0x00 }, NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMovss,
    2, g_Operands + 540,
    NULL
  },
  /* 904 */
  { PrefixF30F, 2, { 0x0f, 0x11, 0x00, 0x00 }, NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMovss,
    2, g_Operands + 542,
    NULL
  },
  /* 905 */
  { PrefixF30F, 2, { 0x0f, 0x12, 0x00, 0x00 }, NACLi_SSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMovsldup,
    2, g_Operands + 346,
    NULL
  },
  /* 906 */
  { PrefixF30F, 2, { 0x0f, 0x13, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 907 */
  { PrefixF30F, 2, { 0x0f, 0x14, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 908 */
  { PrefixF30F, 2, { 0x0f, 0x15, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 909 */
  { PrefixF30F, 2, { 0x0f, 0x16, 0x00, 0x00 }, NACLi_SSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMovshdup,
    2, g_Operands + 346,
    NULL
  },
  /* 910 */
  { PrefixF30F, 2, { 0x0f, 0x17, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 911 */
  { PrefixF30F, 2, { 0x0f, 0x28, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 912 */
  { PrefixF30F, 2, { 0x0f, 0x29, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 913 */
  { PrefixF30F, 2, { 0x0f, 0x2a, 0x00, 0x00 }, NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCvtsi2ss,
    2, g_Operands + 544,
    NULL
  },
  /* 914 */
  { PrefixF30F, 2, { 0x0f, 0x2b, 0x00, 0x00 }, NACLi_SSE4A,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMovntss,
    2, g_Operands + 546,
    NULL
  },
  /* 915 */
  { PrefixF30F, 2, { 0x0f, 0x2c, 0x00, 0x00 }, NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCvttss2si,
    2, g_Operands + 548,
    NULL
  },
  /* 916 */
  { PrefixF30F, 2, { 0x0f, 0x2d, 0x00, 0x00 }, NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCvtss2si,
    2, g_Operands + 550,
    NULL
  },
  /* 917 */
  { PrefixF30F, 2, { 0x0f, 0x2e, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 918 */
  { PrefixF30F, 2, { 0x0f, 0x2f, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 919 */
  { PrefixF30F, 2, { 0x0f, 0x50, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 920 */
  { PrefixF30F, 2, { 0x0f, 0x51, 0x00, 0x00 }, NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstSqrtss,
    2, g_Operands + 346,
    NULL
  },
  /* 921 */
  { PrefixF30F, 2, { 0x0f, 0x52, 0x00, 0x00 }, NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstRsqrtss,
    2, g_Operands + 540,
    NULL
  },
  /* 922 */
  { PrefixF30F, 2, { 0x0f, 0x53, 0x00, 0x00 }, NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstRcpss,
    2, g_Operands + 540,
    NULL
  },
  /* 923 */
  { PrefixF30F, 2, { 0x0f, 0x54, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 924 */
  { PrefixF30F, 2, { 0x0f, 0x55, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 925 */
  { PrefixF30F, 2, { 0x0f, 0x56, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 926 */
  { PrefixF30F, 2, { 0x0f, 0x57, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 927 */
  { PrefixF30F, 2, { 0x0f, 0x58, 0x00, 0x00 }, NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstAddss,
    2, g_Operands + 552,
    NULL
  },
  /* 928 */
  { PrefixF30F, 2, { 0x0f, 0x59, 0x00, 0x00 }, NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMulss,
    2, g_Operands + 554,
    NULL
  },
  /* 929 */
  { PrefixF30F, 2, { 0x0f, 0x5a, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstCvtss2sd,
    2, g_Operands + 556,
    NULL
  },
  /* 930 */
  { PrefixF30F, 2, { 0x0f, 0x5b, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstCvttps2dq,
    2, g_Operands + 558,
    NULL
  },
  /* 931 */
  { PrefixF30F, 2, { 0x0f, 0x5c, 0x00, 0x00 }, NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstSubss,
    2, g_Operands + 552,
    NULL
  },
  /* 932 */
  { PrefixF30F, 2, { 0x0f, 0x5d, 0x00, 0x00 }, NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMinss,
    2, g_Operands + 552,
    NULL
  },
  /* 933 */
  { PrefixF30F, 2, { 0x0f, 0x5e, 0x00, 0x00 }, NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstDivss,
    2, g_Operands + 552,
    NULL
  },
  /* 934 */
  { PrefixF30F, 2, { 0x0f, 0x5f, 0x00, 0x00 }, NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMaxss,
    2, g_Operands + 552,
    NULL
  },
  /* 935 */
  { PrefixF30F, 2, { 0x0f, 0x60, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 936 */
  { PrefixF30F, 2, { 0x0f, 0x61, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 937 */
  { PrefixF30F, 2, { 0x0f, 0x62, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 938 */
  { PrefixF30F, 2, { 0x0f, 0x63, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 939 */
  { PrefixF30F, 2, { 0x0f, 0x64, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 940 */
  { PrefixF30F, 2, { 0x0f, 0x65, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 941 */
  { PrefixF30F, 2, { 0x0f, 0x66, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 942 */
  { PrefixF30F, 2, { 0x0f, 0x67, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 943 */
  { PrefixF30F, 2, { 0x0f, 0x68, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 944 */
  { PrefixF30F, 2, { 0x0f, 0x69, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 945 */
  { PrefixF30F, 2, { 0x0f, 0x6a, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 946 */
  { PrefixF30F, 2, { 0x0f, 0x6b, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 947 */
  { PrefixF30F, 2, { 0x0f, 0x6c, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 948 */
  { PrefixF30F, 2, { 0x0f, 0x6d, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 949 */
  { PrefixF30F, 2, { 0x0f, 0x6e, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 950 */
  { PrefixF30F, 2, { 0x0f, 0x6f, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMovdqu,
    2, g_Operands + 560,
    NULL
  },
  /* 951 */
  { PrefixF30F, 2, { 0x0f, 0x70, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsRep),
    InstPshufhw,
    3, g_Operands + 520,
    NULL
  },
  /* 952 */
  { PrefixF30F, 2, { 0x0f, 0x71, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 953 */
  { PrefixF30F, 2, { 0x0f, 0x72, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 954 */
  { PrefixF30F, 2, { 0x0f, 0x73, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 955 */
  { PrefixF30F, 2, { 0x0f, 0x74, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 956 */
  { PrefixF30F, 2, { 0x0f, 0x75, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 957 */
  { PrefixF30F, 2, { 0x0f, 0x76, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 958 */
  { PrefixF30F, 2, { 0x0f, 0x77, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 959 */
  { PrefixF30F, 2, { 0x0f, 0x78, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 960 */
  { PrefixF30F, 2, { 0x0f, 0x79, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 961 */
  { PrefixF30F, 2, { 0x0f, 0x7a, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 962 */
  { PrefixF30F, 2, { 0x0f, 0x7b, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 963 */
  { PrefixF30F, 2, { 0x0f, 0x7c, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 964 */
  { PrefixF30F, 2, { 0x0f, 0x7d, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 965 */
  { PrefixF30F, 2, { 0x0f, 0x7e, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMovq,
    2, g_Operands + 520,
    NULL
  },
  /* 966 */
  { PrefixF30F, 2, { 0x0f, 0x7f, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMovdqu,
    2, g_Operands + 562,
    NULL
  },
  /* 967 */
  { PrefixF30F, 2, { 0x0f, 0xb8, 0x00, 0x00 }, NACLi_POPCNT,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstPopcnt,
    2, g_Operands + 564,
    NULL
  },
  /* 968 */
  { PrefixF30F, 2, { 0x0f, 0xb9, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 969 */
  { PrefixF30F, 2, { 0x0f, 0xba, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 970 */
  { PrefixF30F, 2, { 0x0f, 0xbb, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 971 */
  { PrefixF30F, 2, { 0x0f, 0xbc, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 972 */
  { PrefixF30F, 2, { 0x0f, 0xbd, 0x00, 0x00 }, NACLi_LZCNT,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstLzcnt,
    2, g_Operands + 566,
    NULL
  },
  /* 973 */
  { PrefixF30F, 2, { 0x0f, 0xbe, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 974 */
  { PrefixF30F, 2, { 0x0f, 0xbf, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 975 */
  { PrefixF30F, 2, { 0x0f, 0xc2, 0x00, 0x00 }, NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsRep),
    InstCmpss,
    3, g_Operands + 568,
    NULL
  },
  /* 976 */
  { PrefixF30F, 2, { 0x0f, 0xc3, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 977 */
  { PrefixF30F, 2, { 0x0f, 0xc4, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 978 */
  { PrefixF30F, 2, { 0x0f, 0xc5, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 979 */
  { PrefixF30F, 2, { 0x0f, 0xc6, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 980 */
  { PrefixF30F, 2, { 0x0f, 0xd0, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 981 */
  { PrefixF30F, 2, { 0x0f, 0xd1, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 982 */
  { PrefixF30F, 2, { 0x0f, 0xd2, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 983 */
  { PrefixF30F, 2, { 0x0f, 0xd3, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 984 */
  { PrefixF30F, 2, { 0x0f, 0xd4, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 985 */
  { PrefixF30F, 2, { 0x0f, 0xd5, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 986 */
  { PrefixF30F, 2, { 0x0f, 0xd6, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMovq2dq,
    2, g_Operands + 571,
    NULL
  },
  /* 987 */
  { PrefixF30F, 2, { 0x0f, 0xd7, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 988 */
  { PrefixF30F, 2, { 0x0f, 0xd8, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 989 */
  { PrefixF30F, 2, { 0x0f, 0xd9, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 990 */
  { PrefixF30F, 2, { 0x0f, 0xda, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 991 */
  { PrefixF30F, 2, { 0x0f, 0xdb, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 992 */
  { PrefixF30F, 2, { 0x0f, 0xdc, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 993 */
  { PrefixF30F, 2, { 0x0f, 0xdd, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 994 */
  { PrefixF30F, 2, { 0x0f, 0xde, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 995 */
  { PrefixF30F, 2, { 0x0f, 0xdf, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 996 */
  { PrefixF30F, 2, { 0x0f, 0xe0, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 997 */
  { PrefixF30F, 2, { 0x0f, 0xe1, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 998 */
  { PrefixF30F, 2, { 0x0f, 0xe2, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 999 */
  { PrefixF30F, 2, { 0x0f, 0xe3, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1000 */
  { PrefixF30F, 2, { 0x0f, 0xe4, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1001 */
  { PrefixF30F, 2, { 0x0f, 0xe5, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1002 */
  { PrefixF30F, 2, { 0x0f, 0xe6, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstCvtdq2pd,
    2, g_Operands + 573,
    NULL
  },
  /* 1003 */
  { PrefixF30F, 2, { 0x0f, 0xe7, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1004 */
  { PrefixF30F, 2, { 0x0f, 0xe8, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1005 */
  { PrefixF30F, 2, { 0x0f, 0xe9, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1006 */
  { PrefixF30F, 2, { 0x0f, 0xea, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1007 */
  { PrefixF30F, 2, { 0x0f, 0xeb, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1008 */
  { PrefixF30F, 2, { 0x0f, 0xec, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1009 */
  { PrefixF30F, 2, { 0x0f, 0xed, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1010 */
  { PrefixF30F, 2, { 0x0f, 0xee, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1011 */
  { PrefixF30F, 2, { 0x0f, 0xef, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1012 */
  { PrefixF30F, 2, { 0x0f, 0xf0, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1013 */
  { PrefixF30F, 2, { 0x0f, 0xf1, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1014 */
  { PrefixF30F, 2, { 0x0f, 0xf2, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1015 */
  { PrefixF30F, 2, { 0x0f, 0xf3, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1016 */
  { PrefixF30F, 2, { 0x0f, 0xf4, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1017 */
  { PrefixF30F, 2, { 0x0f, 0xf5, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1018 */
  { PrefixF30F, 2, { 0x0f, 0xf6, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1019 */
  { PrefixF30F, 2, { 0x0f, 0xf7, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1020 */
  { PrefixF30F, 2, { 0x0f, 0xf8, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1021 */
  { PrefixF30F, 2, { 0x0f, 0xf9, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1022 */
  { PrefixF30F, 2, { 0x0f, 0xfa, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1023 */
  { PrefixF30F, 2, { 0x0f, 0xfb, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1024 */
  { PrefixF30F, 2, { 0x0f, 0xfc, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1025 */
  { PrefixF30F, 2, { 0x0f, 0xfd, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1026 */
  { PrefixF30F, 2, { 0x0f, 0xfe, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1027 */
  { PrefixF30F, 2, { 0x0f, 0xff, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1028 */
  { Prefix660F, 2, { 0x0f, 0x10, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovupd,
    2, g_Operands + 575,
    NULL
  },
  /* 1029 */
  { Prefix660F, 2, { 0x0f, 0x11, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovupd,
    2, g_Operands + 577,
    NULL
  },
  /* 1030 */
  { Prefix660F, 2, { 0x0f, 0x12, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovlpd,
    2, g_Operands + 579,
    NULL
  },
  /* 1031 */
  { Prefix660F, 2, { 0x0f, 0x13, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovlpd,
    2, g_Operands + 508,
    NULL
  },
  /* 1032 */
  { Prefix660F, 2, { 0x0f, 0x14, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstUnpcklpd,
    2, g_Operands + 581,
    NULL
  },
  /* 1033 */
  { Prefix660F, 2, { 0x0f, 0x15, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstUnpckhpd,
    2, g_Operands + 583,
    NULL
  },
  /* 1034 */
  { Prefix660F, 2, { 0x0f, 0x16, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovhpd,
    2, g_Operands + 579,
    NULL
  },
  /* 1035 */
  { Prefix660F, 2, { 0x0f, 0x17, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovhpd,
    2, g_Operands + 508,
    NULL
  },
  /* 1036 */
  { Prefix660F, 2, { 0x0f, 0x28, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovapd,
    2, g_Operands + 575,
    NULL
  },
  /* 1037 */
  { Prefix660F, 2, { 0x0f, 0x29, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovapd,
    2, g_Operands + 577,
    NULL
  },
  /* 1038 */
  { Prefix660F, 2, { 0x0f, 0x2a, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstCvtpi2pd,
    2, g_Operands + 585,
    NULL
  },
  /* 1039 */
  { Prefix660F, 2, { 0x0f, 0x2b, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovntpd,
    2, g_Operands + 587,
    NULL
  },
  /* 1040 */
  { Prefix660F, 2, { 0x0f, 0x2c, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstCvttpd2pi,
    2, g_Operands + 589,
    NULL
  },
  /* 1041 */
  { Prefix660F, 2, { 0x0f, 0x2d, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstCvtpd2pi,
    2, g_Operands + 591,
    NULL
  },
  /* 1042 */
  { Prefix660F, 2, { 0x0f, 0x2e, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstUcomisd,
    2, g_Operands + 593,
    NULL
  },
  /* 1043 */
  { Prefix660F, 2, { 0x0f, 0x2f, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstComisd,
    2, g_Operands + 595,
    NULL
  },
  /* 1044 */
  { Prefix660F, 2, { 0x0f, 0x50, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovmskpd,
    2, g_Operands + 597,
    NULL
  },
  /* 1045 */
  { Prefix660F, 2, { 0x0f, 0x51, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstSqrtpd,
    2, g_Operands + 599,
    NULL
  },
  /* 1046 */
  { Prefix660F, 2, { 0x0f, 0x52, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1047 */
  { Prefix660F, 2, { 0x0f, 0x53, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1048 */
  { Prefix660F, 2, { 0x0f, 0x54, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstAndpd,
    2, g_Operands + 532,
    NULL
  },
  /* 1049 */
  { Prefix660F, 2, { 0x0f, 0x55, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstAndnpd,
    2, g_Operands + 532,
    NULL
  },
  /* 1050 */
  { Prefix660F, 2, { 0x0f, 0x56, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstOrpd,
    2, g_Operands + 532,
    NULL
  },
  /* 1051 */
  { Prefix660F, 2, { 0x0f, 0x57, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstXorpd,
    2, g_Operands + 532,
    NULL
  },
  /* 1052 */
  { Prefix660F, 2, { 0x0f, 0x58, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstAddpd,
    2, g_Operands + 532,
    NULL
  },
  /* 1053 */
  { Prefix660F, 2, { 0x0f, 0x59, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMulpd,
    2, g_Operands + 532,
    NULL
  },
  /* 1054 */
  { Prefix660F, 2, { 0x0f, 0x5a, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstCvtpd2ps,
    2, g_Operands + 601,
    NULL
  },
  /* 1055 */
  { Prefix660F, 2, { 0x0f, 0x5b, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstCvtps2dq,
    2, g_Operands + 558,
    NULL
  },
  /* 1056 */
  { Prefix660F, 2, { 0x0f, 0x5c, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstSubpd,
    2, g_Operands + 532,
    NULL
  },
  /* 1057 */
  { Prefix660F, 2, { 0x0f, 0x5d, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMinpd,
    2, g_Operands + 532,
    NULL
  },
  /* 1058 */
  { Prefix660F, 2, { 0x0f, 0x5e, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstDivpd,
    2, g_Operands + 532,
    NULL
  },
  /* 1059 */
  { Prefix660F, 2, { 0x0f, 0x5f, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMaxpd,
    2, g_Operands + 532,
    NULL
  },
  /* 1060 */
  { Prefix660F, 2, { 0x0f, 0x60, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPunpcklbw,
    2, g_Operands + 603,
    NULL
  },
  /* 1061 */
  { Prefix660F, 2, { 0x0f, 0x61, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPunpcklwd,
    2, g_Operands + 605,
    NULL
  },
  /* 1062 */
  { Prefix660F, 2, { 0x0f, 0x62, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPunpckldq,
    2, g_Operands + 603,
    NULL
  },
  /* 1063 */
  { Prefix660F, 2, { 0x0f, 0x63, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPacksswb,
    2, g_Operands + 607,
    NULL
  },
  /* 1064 */
  { Prefix660F, 2, { 0x0f, 0x64, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPcmpgtb,
    2, g_Operands + 609,
    NULL
  },
  /* 1065 */
  { Prefix660F, 2, { 0x0f, 0x65, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPcmpgtw,
    2, g_Operands + 607,
    NULL
  },
  /* 1066 */
  { Prefix660F, 2, { 0x0f, 0x66, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPcmpgtd,
    2, g_Operands + 607,
    NULL
  },
  /* 1067 */
  { Prefix660F, 2, { 0x0f, 0x67, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPackuswb,
    2, g_Operands + 607,
    NULL
  },
  /* 1068 */
  { Prefix660F, 2, { 0x0f, 0x68, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPunpckhbw,
    2, g_Operands + 603,
    NULL
  },
  /* 1069 */
  { Prefix660F, 2, { 0x0f, 0x69, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPunpckhwd,
    2, g_Operands + 603,
    NULL
  },
  /* 1070 */
  { Prefix660F, 2, { 0x0f, 0x6a, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPunpckhdq,
    2, g_Operands + 603,
    NULL
  },
  /* 1071 */
  { Prefix660F, 2, { 0x0f, 0x6b, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPackssdw,
    2, g_Operands + 607,
    NULL
  },
  /* 1072 */
  { Prefix660F, 2, { 0x0f, 0x6c, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPunpcklqdq,
    2, g_Operands + 603,
    NULL
  },
  /* 1073 */
  { Prefix660F, 2, { 0x0f, 0x6d, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPunpckhqdq,
    2, g_Operands + 603,
    NULL
  },
  /* 1074 */
  { Prefix660F, 2, { 0x0f, 0x6e, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstMovd,
    2, g_Operands + 611,
    g_Opcodes + 1075
  },
  /* 1075 */
  { Prefix660F, 2, { 0x0f, 0x6e, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_o),
    InstMovq,
    2, g_Operands + 613,
    NULL
  },
  /* 1076 */
  { Prefix660F, 2, { 0x0f, 0x6f, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovdqa,
    2, g_Operands + 560,
    NULL
  },
  /* 1077 */
  { Prefix660F, 2, { 0x0f, 0x70, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPshufd,
    3, g_Operands + 615,
    NULL
  },
  /* 1078 */
  { Prefix660F, 2, { 0x0f, 0x71, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    g_Opcodes + 1079
  },
  /* 1079 */
  { Prefix660F, 2, { 0x0f, 0x71, 0x01, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    g_Opcodes + 1080
  },
  /* 1080 */
  { Prefix660F, 2, { 0x0f, 0x71, 0x02, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsrlw,
    2, g_Operands + 618,
    g_Opcodes + 1081
  },
  /* 1081 */
  { Prefix660F, 2, { 0x0f, 0x71, 0x03, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    g_Opcodes + 1082
  },
  /* 1082 */
  { Prefix660F, 2, { 0x0f, 0x71, 0x04, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsraw,
    2, g_Operands + 620,
    g_Opcodes + 1083
  },
  /* 1083 */
  { Prefix660F, 2, { 0x0f, 0x71, 0x05, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    g_Opcodes + 1084
  },
  /* 1084 */
  { Prefix660F, 2, { 0x0f, 0x71, 0x06, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsllw,
    2, g_Operands + 618,
    g_Opcodes + 1085
  },
  /* 1085 */
  { Prefix660F, 2, { 0x0f, 0x71, 0x07, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1086 */
  { Prefix660F, 2, { 0x0f, 0x72, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    g_Opcodes + 1087
  },
  /* 1087 */
  { Prefix660F, 2, { 0x0f, 0x72, 0x01, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    g_Opcodes + 1088
  },
  /* 1088 */
  { Prefix660F, 2, { 0x0f, 0x72, 0x02, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsrld,
    2, g_Operands + 618,
    g_Opcodes + 1089
  },
  /* 1089 */
  { Prefix660F, 2, { 0x0f, 0x72, 0x03, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    g_Opcodes + 1090
  },
  /* 1090 */
  { Prefix660F, 2, { 0x0f, 0x72, 0x04, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsrad,
    2, g_Operands + 618,
    g_Opcodes + 1091
  },
  /* 1091 */
  { Prefix660F, 2, { 0x0f, 0x72, 0x05, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    g_Opcodes + 1092
  },
  /* 1092 */
  { Prefix660F, 2, { 0x0f, 0x72, 0x06, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPslld,
    2, g_Operands + 618,
    g_Opcodes + 1093
  },
  /* 1093 */
  { Prefix660F, 2, { 0x0f, 0x72, 0x07, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1094 */
  { Prefix660F, 2, { 0x0f, 0x73, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    g_Opcodes + 1095
  },
  /* 1095 */
  { Prefix660F, 2, { 0x0f, 0x73, 0x01, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    g_Opcodes + 1096
  },
  /* 1096 */
  { Prefix660F, 2, { 0x0f, 0x73, 0x02, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsrlq,
    2, g_Operands + 618,
    g_Opcodes + 1097
  },
  /* 1097 */
  { Prefix660F, 2, { 0x0f, 0x73, 0x03, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsrldq,
    2, g_Operands + 618,
    g_Opcodes + 1098
  },
  /* 1098 */
  { Prefix660F, 2, { 0x0f, 0x73, 0x04, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    g_Opcodes + 1099
  },
  /* 1099 */
  { Prefix660F, 2, { 0x0f, 0x73, 0x05, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    g_Opcodes + 1100
  },
  /* 1100 */
  { Prefix660F, 2, { 0x0f, 0x73, 0x06, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsllq,
    2, g_Operands + 618,
    g_Opcodes + 1101
  },
  /* 1101 */
  { Prefix660F, 2, { 0x0f, 0x73, 0x07, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPslldq,
    2, g_Operands + 618,
    NULL
  },
  /* 1102 */
  { Prefix660F, 2, { 0x0f, 0x74, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPcmpeqb,
    2, g_Operands + 607,
    NULL
  },
  /* 1103 */
  { Prefix660F, 2, { 0x0f, 0x75, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPcmpeqw,
    2, g_Operands + 607,
    NULL
  },
  /* 1104 */
  { Prefix660F, 2, { 0x0f, 0x76, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPcmpeqd,
    2, g_Operands + 607,
    NULL
  },
  /* 1105 */
  { Prefix660F, 2, { 0x0f, 0x77, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1106 */
  { Prefix660F, 2, { 0x0f, 0x78, 0x00, 0x00 }, NACLi_SSE4A,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeHasImmed2_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstExtrq,
    3, g_Operands + 622,
    g_Opcodes + 1107
  },
  /* 1107 */
  { Prefix660F, 2, { 0x0f, 0x78, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1108 */
  { Prefix660F, 2, { 0x0f, 0x79, 0x00, 0x00 }, NACLi_SSE4A,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstExtrq,
    2, g_Operands + 527,
    NULL
  },
  /* 1109 */
  { Prefix660F, 2, { 0x0f, 0x7a, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1110 */
  { Prefix660F, 2, { 0x0f, 0x7b, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1111 */
  { Prefix660F, 2, { 0x0f, 0x7c, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstHaddpd,
    2, g_Operands + 532,
    NULL
  },
  /* 1112 */
  { Prefix660F, 2, { 0x0f, 0x7d, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstHsubpd,
    2, g_Operands + 532,
    NULL
  },
  /* 1113 */
  { Prefix660F, 2, { 0x0f, 0x7e, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstMovd,
    2, g_Operands + 625,
    g_Opcodes + 1114
  },
  /* 1114 */
  { Prefix660F, 2, { 0x0f, 0x7e, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_o),
    InstMovq,
    2, g_Operands + 627,
    NULL
  },
  /* 1115 */
  { Prefix660F, 2, { 0x0f, 0x7f, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovdqa,
    2, g_Operands + 562,
    NULL
  },
  /* 1116 */
  { Prefix660F, 2, { 0x0f, 0xae, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1117 */
  { Prefix660F, 2, { 0x0f, 0xc2, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstCmppd,
    3, g_Operands + 629,
    NULL
  },
  /* 1118 */
  { Prefix660F, 2, { 0x0f, 0xc3, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1119 */
  { Prefix660F, 2, { 0x0f, 0xc4, 0x00, 0x00 }, NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstPinsrw,
    3, g_Operands + 632,
    NULL
  },
  /* 1120 */
  { Prefix660F, 2, { 0x0f, 0xc5, 0x00, 0x00 }, NACLi_SSE41,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPextrw,
    3, g_Operands + 635,
    NULL
  },
  /* 1121 */
  { Prefix660F, 2, { 0x0f, 0xc6, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstShufpd,
    3, g_Operands + 629,
    NULL
  },
  /* 1122 */
  { Prefix660F, 2, { 0x0f, 0xd0, 0x00, 0x00 }, NACLi_SSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstAddsubpd,
    2, g_Operands + 532,
    NULL
  },
  /* 1123 */
  { Prefix660F, 2, { 0x0f, 0xd1, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsrlw,
    2, g_Operands + 607,
    NULL
  },
  /* 1124 */
  { Prefix660F, 2, { 0x0f, 0xd2, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsrld,
    2, g_Operands + 607,
    NULL
  },
  /* 1125 */
  { Prefix660F, 2, { 0x0f, 0xd3, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsrlq,
    2, g_Operands + 607,
    NULL
  },
  /* 1126 */
  { Prefix660F, 2, { 0x0f, 0xd4, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPaddq,
    2, g_Operands + 607,
    NULL
  },
  /* 1127 */
  { Prefix660F, 2, { 0x0f, 0xd5, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmullw,
    2, g_Operands + 607,
    NULL
  },
  /* 1128 */
  { Prefix660F, 2, { 0x0f, 0xd6, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovq,
    2, g_Operands + 638,
    NULL
  },
  /* 1129 */
  { Prefix660F, 2, { 0x0f, 0xd7, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovmskb,
    2, g_Operands + 635,
    NULL
  },
  /* 1130 */
  { Prefix660F, 2, { 0x0f, 0xd8, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsubusb,
    2, g_Operands + 607,
    NULL
  },
  /* 1131 */
  { Prefix660F, 2, { 0x0f, 0xd9, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsubusw,
    2, g_Operands + 607,
    NULL
  },
  /* 1132 */
  { Prefix660F, 2, { 0x0f, 0xda, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPminub,
    2, g_Operands + 607,
    NULL
  },
  /* 1133 */
  { Prefix660F, 2, { 0x0f, 0xdb, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPand,
    2, g_Operands + 607,
    NULL
  },
  /* 1134 */
  { Prefix660F, 2, { 0x0f, 0xdc, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPaddusb,
    2, g_Operands + 607,
    NULL
  },
  /* 1135 */
  { Prefix660F, 2, { 0x0f, 0xdd, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPaddusw,
    2, g_Operands + 607,
    NULL
  },
  /* 1136 */
  { Prefix660F, 2, { 0x0f, 0xde, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmaxub,
    2, g_Operands + 607,
    NULL
  },
  /* 1137 */
  { Prefix660F, 2, { 0x0f, 0xdf, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPandn,
    2, g_Operands + 607,
    NULL
  },
  /* 1138 */
  { Prefix660F, 2, { 0x0f, 0xe0, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPavgb,
    2, g_Operands + 607,
    NULL
  },
  /* 1139 */
  { Prefix660F, 2, { 0x0f, 0xe1, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsraw,
    2, g_Operands + 607,
    NULL
  },
  /* 1140 */
  { Prefix660F, 2, { 0x0f, 0xe2, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsrad,
    2, g_Operands + 607,
    NULL
  },
  /* 1141 */
  { Prefix660F, 2, { 0x0f, 0xe3, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPavgw,
    2, g_Operands + 607,
    NULL
  },
  /* 1142 */
  { Prefix660F, 2, { 0x0f, 0xe4, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmulhuw,
    2, g_Operands + 607,
    NULL
  },
  /* 1143 */
  { Prefix660F, 2, { 0x0f, 0xe5, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmulhw,
    2, g_Operands + 607,
    NULL
  },
  /* 1144 */
  { Prefix660F, 2, { 0x0f, 0xe6, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstCvttpd2dq,
    2, g_Operands + 640,
    NULL
  },
  /* 1145 */
  { Prefix660F, 2, { 0x0f, 0xe7, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovntdq,
    2, g_Operands + 642,
    NULL
  },
  /* 1146 */
  { Prefix660F, 2, { 0x0f, 0xe8, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsubsb,
    2, g_Operands + 607,
    NULL
  },
  /* 1147 */
  { Prefix660F, 2, { 0x0f, 0xe9, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsubsw,
    2, g_Operands + 607,
    NULL
  },
  /* 1148 */
  { Prefix660F, 2, { 0x0f, 0xea, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPminsw,
    2, g_Operands + 607,
    NULL
  },
  /* 1149 */
  { Prefix660F, 2, { 0x0f, 0xeb, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPor,
    2, g_Operands + 607,
    NULL
  },
  /* 1150 */
  { Prefix660F, 2, { 0x0f, 0xec, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPaddsb,
    2, g_Operands + 607,
    NULL
  },
  /* 1151 */
  { Prefix660F, 2, { 0x0f, 0xed, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPaddsw,
    2, g_Operands + 607,
    NULL
  },
  /* 1152 */
  { Prefix660F, 2, { 0x0f, 0xee, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmaxsw,
    2, g_Operands + 607,
    NULL
  },
  /* 1153 */
  { Prefix660F, 2, { 0x0f, 0xef, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPxor,
    2, g_Operands + 607,
    NULL
  },
  /* 1154 */
  { Prefix660F, 2, { 0x0f, 0xf0, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1155 */
  { Prefix660F, 2, { 0x0f, 0xf1, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsllw,
    2, g_Operands + 607,
    NULL
  },
  /* 1156 */
  { Prefix660F, 2, { 0x0f, 0xf2, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPslld,
    2, g_Operands + 607,
    NULL
  },
  /* 1157 */
  { Prefix660F, 2, { 0x0f, 0xf3, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsllq,
    2, g_Operands + 607,
    NULL
  },
  /* 1158 */
  { Prefix660F, 2, { 0x0f, 0xf4, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmuludq,
    2, g_Operands + 607,
    NULL
  },
  /* 1159 */
  { Prefix660F, 2, { 0x0f, 0xf5, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmaddwd,
    2, g_Operands + 607,
    NULL
  },
  /* 1160 */
  { Prefix660F, 2, { 0x0f, 0xf6, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsadbw,
    2, g_Operands + 607,
    NULL
  },
  /* 1161 */
  { Prefix660F, 2, { 0x0f, 0xf7, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstMaskmovdqu,
    3, g_Operands + 644,
    NULL
  },
  /* 1162 */
  { Prefix660F, 2, { 0x0f, 0xf8, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsubb,
    2, g_Operands + 607,
    NULL
  },
  /* 1163 */
  { Prefix660F, 2, { 0x0f, 0xf9, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsubw,
    2, g_Operands + 607,
    NULL
  },
  /* 1164 */
  { Prefix660F, 2, { 0x0f, 0xfa, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsubd,
    2, g_Operands + 607,
    NULL
  },
  /* 1165 */
  { Prefix660F, 2, { 0x0f, 0xfb, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsubq,
    2, g_Operands + 607,
    NULL
  },
  /* 1166 */
  { Prefix660F, 2, { 0x0f, 0xfc, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPaddb,
    2, g_Operands + 607,
    NULL
  },
  /* 1167 */
  { Prefix660F, 2, { 0x0f, 0xfd, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPaddw,
    2, g_Operands + 607,
    NULL
  },
  /* 1168 */
  { Prefix660F, 2, { 0x0f, 0xfe, 0x00, 0x00 }, NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPaddd,
    2, g_Operands + 607,
    NULL
  },
  /* 1169 */
  { Prefix660F, 2, { 0x0f, 0xff, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1170 */
  { Prefix0F0F, 3, { 0x0f, 0x0f, 0x0c, 0x00 }, NACLi_E3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPi2fw,
    2, g_Operands + 423,
    NULL
  },
  /* 1171 */
  { Prefix0F0F, 3, { 0x0f, 0x0f, 0x0d, 0x00 }, NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPi2fd,
    2, g_Operands + 423,
    NULL
  },
  /* 1172 */
  { Prefix0F0F, 3, { 0x0f, 0x0f, 0x1c, 0x00 }, NACLi_E3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPf2iw,
    2, g_Operands + 423,
    NULL
  },
  /* 1173 */
  { Prefix0F0F, 3, { 0x0f, 0x0f, 0x1d, 0x00 }, NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPf2id,
    2, g_Operands + 423,
    NULL
  },
  /* 1174 */
  { Prefix0F0F, 3, { 0x0f, 0x0f, 0x8a, 0x00 }, NACLi_E3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfnacc,
    2, g_Operands + 411,
    NULL
  },
  /* 1175 */
  { Prefix0F0F, 3, { 0x0f, 0x0f, 0x8e, 0x00 }, NACLi_E3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfpnacc,
    2, g_Operands + 411,
    NULL
  },
  /* 1176 */
  { Prefix0F0F, 3, { 0x0f, 0x0f, 0x90, 0x00 }, NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfcmpge,
    2, g_Operands + 411,
    NULL
  },
  /* 1177 */
  { Prefix0F0F, 3, { 0x0f, 0x0f, 0x94, 0x00 }, NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfmin,
    2, g_Operands + 411,
    NULL
  },
  /* 1178 */
  { Prefix0F0F, 3, { 0x0f, 0x0f, 0x96, 0x00 }, NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfrcp,
    2, g_Operands + 423,
    NULL
  },
  /* 1179 */
  { Prefix0F0F, 3, { 0x0f, 0x0f, 0x97, 0x00 }, NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfrsqrt,
    2, g_Operands + 423,
    NULL
  },
  /* 1180 */
  { Prefix0F0F, 3, { 0x0f, 0x0f, 0x9a, 0x00 }, NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfsub,
    2, g_Operands + 411,
    NULL
  },
  /* 1181 */
  { Prefix0F0F, 3, { 0x0f, 0x0f, 0x9e, 0x00 }, NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfadd,
    2, g_Operands + 411,
    NULL
  },
  /* 1182 */
  { Prefix0F0F, 3, { 0x0f, 0x0f, 0xa0, 0x00 }, NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfcmpgt,
    2, g_Operands + 411,
    NULL
  },
  /* 1183 */
  { Prefix0F0F, 3, { 0x0f, 0x0f, 0xa4, 0x00 }, NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfmax,
    2, g_Operands + 411,
    NULL
  },
  /* 1184 */
  { Prefix0F0F, 3, { 0x0f, 0x0f, 0xa6, 0x00 }, NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfrcpit1,
    2, g_Operands + 411,
    NULL
  },
  /* 1185 */
  { Prefix0F0F, 3, { 0x0f, 0x0f, 0xa7, 0x00 }, NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfrsqit1,
    2, g_Operands + 411,
    NULL
  },
  /* 1186 */
  { Prefix0F0F, 3, { 0x0f, 0x0f, 0xaa, 0x00 }, NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfsubr,
    2, g_Operands + 411,
    NULL
  },
  /* 1187 */
  { Prefix0F0F, 3, { 0x0f, 0x0f, 0xae, 0x00 }, NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfacc,
    2, g_Operands + 411,
    NULL
  },
  /* 1188 */
  { Prefix0F0F, 3, { 0x0f, 0x0f, 0xb0, 0x00 }, NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfcmpeq,
    2, g_Operands + 411,
    NULL
  },
  /* 1189 */
  { Prefix0F0F, 3, { 0x0f, 0x0f, 0xb4, 0x00 }, NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfmul,
    2, g_Operands + 411,
    NULL
  },
  /* 1190 */
  { Prefix0F0F, 3, { 0x0f, 0x0f, 0xb6, 0x00 }, NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfrcpit2,
    2, g_Operands + 411,
    NULL
  },
  /* 1191 */
  { Prefix0F0F, 3, { 0x0f, 0x0f, 0xb7, 0x00 }, NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPmulhrw,
    2, g_Operands + 411,
    NULL
  },
  /* 1192 */
  { Prefix0F0F, 3, { 0x0f, 0x0f, 0xbb, 0x00 }, NACLi_E3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPswapd,
    2, g_Operands + 423,
    NULL
  },
  /* 1193 */
  { Prefix0F0F, 3, { 0x0f, 0x0f, 0xbf, 0x00 }, NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPavgusb,
    2, g_Operands + 411,
    NULL
  },
  /* 1194 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x00, 0x00 }, NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPshufb,
    2, g_Operands + 411,
    NULL
  },
  /* 1195 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x01, 0x00 }, NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPhaddw,
    2, g_Operands + 411,
    NULL
  },
  /* 1196 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x02, 0x00 }, NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPhaddd,
    2, g_Operands + 411,
    NULL
  },
  /* 1197 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x03, 0x00 }, NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPhaddsw,
    2, g_Operands + 411,
    NULL
  },
  /* 1198 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x04, 0x00 }, NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPmaddubsw,
    2, g_Operands + 411,
    NULL
  },
  /* 1199 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x05, 0x00 }, NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPhsubw,
    2, g_Operands + 411,
    NULL
  },
  /* 1200 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x06, 0x00 }, NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPhsubd,
    2, g_Operands + 411,
    NULL
  },
  /* 1201 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x07, 0x00 }, NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPhsubsw,
    2, g_Operands + 411,
    NULL
  },
  /* 1202 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x08, 0x00 }, NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsignb,
    2, g_Operands + 411,
    NULL
  },
  /* 1203 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x09, 0x00 }, NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsignw,
    2, g_Operands + 411,
    NULL
  },
  /* 1204 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x0a, 0x00 }, NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsignd,
    2, g_Operands + 411,
    NULL
  },
  /* 1205 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x0b, 0x00 }, NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPmulhrsw,
    2, g_Operands + 411,
    NULL
  },
  /* 1206 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x0c, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1207 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x0d, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1208 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x0e, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1209 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x0f, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1210 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x10, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1211 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x11, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1212 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x12, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1213 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x13, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1214 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x14, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1215 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x15, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1216 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x16, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1217 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x17, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1218 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x18, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1219 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x19, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1220 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x1a, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1221 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x1b, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1222 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x1c, 0x00 }, NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPabsb,
    2, g_Operands + 423,
    NULL
  },
  /* 1223 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x1d, 0x00 }, NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPabsw,
    2, g_Operands + 423,
    NULL
  },
  /* 1224 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x1e, 0x00 }, NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPabsd,
    2, g_Operands + 423,
    NULL
  },
  /* 1225 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x1f, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1226 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x20, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1227 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x21, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1228 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x22, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1229 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x23, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1230 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x24, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1231 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x25, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1232 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x26, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1233 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x27, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1234 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x28, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1235 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x29, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1236 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x2a, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1237 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x2b, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1238 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x2c, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1239 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x2d, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1240 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x2e, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1241 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x2f, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1242 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x30, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1243 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x31, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1244 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x32, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1245 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x33, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1246 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x34, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1247 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x35, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1248 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x36, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1249 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x37, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1250 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x38, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1251 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x39, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1252 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x3a, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1253 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x3b, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1254 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x3c, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1255 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x3d, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1256 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x3e, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1257 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x3f, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1258 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x40, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1259 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x41, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1260 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x42, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1261 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x43, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1262 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x44, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1263 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x45, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1264 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x46, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1265 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x47, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1266 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x48, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1267 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x49, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1268 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x4a, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1269 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x4b, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1270 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x4c, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1271 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x4d, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1272 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x4e, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1273 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x4f, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1274 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x50, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1275 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x51, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1276 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x52, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1277 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x53, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1278 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x54, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1279 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x55, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1280 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x56, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1281 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x57, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1282 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x58, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1283 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x59, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1284 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x5a, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1285 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x5b, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1286 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x5c, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1287 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x5d, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1288 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x5e, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1289 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x5f, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1290 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x60, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1291 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x61, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1292 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x62, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1293 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x63, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1294 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x64, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1295 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x65, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1296 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x66, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1297 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x67, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1298 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x68, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1299 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x69, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1300 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x6a, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1301 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x6b, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1302 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x6c, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1303 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x6d, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1304 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x6e, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1305 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x6f, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1306 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x70, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1307 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x71, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1308 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x72, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1309 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x73, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1310 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x74, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1311 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x75, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1312 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x76, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1313 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x77, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1314 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x78, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1315 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x79, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1316 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x7a, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1317 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x7b, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1318 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x7c, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1319 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x7d, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1320 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x7e, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1321 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x7f, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1322 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x80, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1323 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x81, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1324 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x82, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1325 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x83, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1326 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x84, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1327 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x85, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1328 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x86, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1329 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x87, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1330 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x88, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1331 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x89, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1332 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x8a, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1333 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x8b, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1334 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x8c, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1335 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x8d, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1336 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x8e, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1337 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x8f, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1338 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x90, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1339 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x91, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1340 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x92, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1341 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x93, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1342 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x94, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1343 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x95, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1344 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x96, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1345 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x97, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1346 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x98, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1347 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x99, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1348 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x9a, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1349 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x9b, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1350 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x9c, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1351 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x9d, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1352 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x9e, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1353 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0x9f, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1354 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xa0, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1355 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xa1, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1356 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xa2, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1357 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xa3, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1358 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xa4, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1359 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xa5, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1360 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xa6, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1361 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xa7, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1362 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xa8, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1363 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xa9, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1364 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xaa, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1365 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xab, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1366 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xac, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1367 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xad, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1368 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xae, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1369 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xaf, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1370 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xb0, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1371 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xb1, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1372 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xb2, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1373 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xb3, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1374 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xb4, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1375 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xb5, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1376 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xb6, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1377 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xb7, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1378 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xb8, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1379 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xb9, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1380 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xba, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1381 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xbb, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1382 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xbc, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1383 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xbd, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1384 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xbe, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1385 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xbf, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1386 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xc0, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1387 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xc1, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1388 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xc2, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1389 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xc3, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1390 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xc4, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1391 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xc5, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1392 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xc6, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1393 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xc7, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1394 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xc8, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1395 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xc9, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1396 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xca, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1397 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xcb, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1398 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xcc, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1399 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xcd, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1400 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xce, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1401 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xcf, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1402 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xd0, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1403 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xd1, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1404 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xd2, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1405 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xd3, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1406 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xd4, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1407 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xd5, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1408 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xd6, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1409 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xd7, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1410 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xd8, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1411 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xd9, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1412 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xda, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1413 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xdb, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1414 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xdc, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1415 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xdd, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1416 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xde, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1417 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xdf, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1418 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xe0, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1419 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xe1, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1420 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xe2, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1421 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xe3, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1422 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xe4, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1423 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xe5, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1424 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xe6, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1425 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xe7, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1426 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xe8, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1427 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xe9, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1428 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xea, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1429 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xeb, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1430 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xec, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1431 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xed, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1432 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xee, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1433 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xef, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1434 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xf0, 0x00 }, NACLi_MOVBE,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstMovbe,
    2, g_Operands + 647,
    NULL
  },
  /* 1435 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xf1, 0x00 }, NACLi_MOVBE,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstMovbe,
    2, g_Operands + 649,
    NULL
  },
  /* 1436 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xf2, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1437 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xf3, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1438 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xf4, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1439 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xf5, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1440 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xf6, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1441 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xf7, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1442 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xf8, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1443 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xf9, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1444 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xfa, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1445 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xfb, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1446 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xfc, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1447 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xfd, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1448 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xfe, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1449 */
  { Prefix0F38, 3, { 0x0f, 0x38, 0xff, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1450 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x00, 0x00 }, NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPshufb,
    2, g_Operands + 607,
    NULL
  },
  /* 1451 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x01, 0x00 }, NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPhaddw,
    2, g_Operands + 607,
    NULL
  },
  /* 1452 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x02, 0x00 }, NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPhaddd,
    2, g_Operands + 607,
    NULL
  },
  /* 1453 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x03, 0x00 }, NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPhaddsw,
    2, g_Operands + 607,
    NULL
  },
  /* 1454 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x04, 0x00 }, NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmaddubsw,
    2, g_Operands + 607,
    NULL
  },
  /* 1455 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x05, 0x00 }, NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPhsubw,
    2, g_Operands + 607,
    NULL
  },
  /* 1456 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x06, 0x00 }, NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPhsubd,
    2, g_Operands + 607,
    NULL
  },
  /* 1457 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x07, 0x00 }, NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPhsubsw,
    2, g_Operands + 607,
    NULL
  },
  /* 1458 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x08, 0x00 }, NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsignb,
    2, g_Operands + 607,
    NULL
  },
  /* 1459 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x09, 0x00 }, NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsignw,
    2, g_Operands + 607,
    NULL
  },
  /* 1460 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x0a, 0x00 }, NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsignd,
    2, g_Operands + 607,
    NULL
  },
  /* 1461 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x0b, 0x00 }, NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmulhrsw,
    2, g_Operands + 607,
    NULL
  },
  /* 1462 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x0c, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1463 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x0d, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1464 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x0e, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1465 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x0f, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1466 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x10, 0x00 }, NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPblendvb,
    3, g_Operands + 651,
    NULL
  },
  /* 1467 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x11, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1468 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x12, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1469 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x13, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1470 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x14, 0x00 }, NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstBlendvps,
    3, g_Operands + 654,
    NULL
  },
  /* 1471 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x15, 0x00 }, NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstBlendvpd,
    3, g_Operands + 651,
    NULL
  },
  /* 1472 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x16, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1473 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x17, 0x00 }, NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPtest,
    2, g_Operands + 657,
    NULL
  },
  /* 1474 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x18, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1475 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x19, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1476 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x1a, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1477 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x1b, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1478 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x1c, 0x00 }, NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPabsb,
    2, g_Operands + 560,
    NULL
  },
  /* 1479 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x1d, 0x00 }, NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPabsw,
    2, g_Operands + 560,
    NULL
  },
  /* 1480 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x1e, 0x00 }, NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPabsd,
    2, g_Operands + 560,
    NULL
  },
  /* 1481 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x1f, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1482 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x20, 0x00 }, NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovsxbw,
    2, g_Operands + 659,
    NULL
  },
  /* 1483 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x21, 0x00 }, NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovsxbd,
    2, g_Operands + 661,
    NULL
  },
  /* 1484 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x22, 0x00 }, NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovsxbq,
    2, g_Operands + 663,
    NULL
  },
  /* 1485 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x23, 0x00 }, NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovsxwd,
    2, g_Operands + 659,
    NULL
  },
  /* 1486 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x24, 0x00 }, NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovsxwq,
    2, g_Operands + 661,
    NULL
  },
  /* 1487 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x25, 0x00 }, NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovsxdq,
    2, g_Operands + 659,
    NULL
  },
  /* 1488 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x26, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1489 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x27, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1490 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x28, 0x00 }, NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmuldq,
    2, g_Operands + 607,
    NULL
  },
  /* 1491 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x29, 0x00 }, NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPcmpeqq,
    2, g_Operands + 607,
    NULL
  },
  /* 1492 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x2a, 0x00 }, NACLi_SSE41,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovntdqa,
    2, g_Operands + 538,
    NULL
  },
  /* 1493 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x2b, 0x00 }, NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPackusdw,
    2, g_Operands + 607,
    NULL
  },
  /* 1494 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x2c, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1495 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x2d, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1496 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x2e, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1497 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x2f, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1498 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x30, 0x00 }, NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovzxbw,
    2, g_Operands + 659,
    NULL
  },
  /* 1499 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x31, 0x00 }, NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovzxbd,
    2, g_Operands + 661,
    NULL
  },
  /* 1500 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x32, 0x00 }, NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovzxbq,
    2, g_Operands + 665,
    NULL
  },
  /* 1501 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x33, 0x00 }, NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovzxwd,
    2, g_Operands + 659,
    NULL
  },
  /* 1502 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x34, 0x00 }, NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovzxwq,
    2, g_Operands + 661,
    NULL
  },
  /* 1503 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x35, 0x00 }, NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovzxdq,
    2, g_Operands + 659,
    NULL
  },
  /* 1504 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x36, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1505 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x37, 0x00 }, NACLi_SSE42,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPcmpgtq,
    2, g_Operands + 607,
    NULL
  },
  /* 1506 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x38, 0x00 }, NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPminsb,
    2, g_Operands + 607,
    NULL
  },
  /* 1507 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x39, 0x00 }, NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPminsd,
    2, g_Operands + 607,
    NULL
  },
  /* 1508 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x3a, 0x00 }, NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPminuw,
    2, g_Operands + 607,
    NULL
  },
  /* 1509 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x3b, 0x00 }, NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPminud,
    2, g_Operands + 607,
    NULL
  },
  /* 1510 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x3c, 0x00 }, NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmaxsb,
    2, g_Operands + 607,
    NULL
  },
  /* 1511 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x3d, 0x00 }, NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmaxsd,
    2, g_Operands + 607,
    NULL
  },
  /* 1512 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x3e, 0x00 }, NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmaxuw,
    2, g_Operands + 607,
    NULL
  },
  /* 1513 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x3f, 0x00 }, NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmaxud,
    2, g_Operands + 607,
    NULL
  },
  /* 1514 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x40, 0x00 }, NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmulld,
    2, g_Operands + 607,
    NULL
  },
  /* 1515 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x41, 0x00 }, NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPhminposuw,
    2, g_Operands + 607,
    NULL
  },
  /* 1516 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x42, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1517 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x43, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1518 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x44, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1519 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x45, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1520 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x46, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1521 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x47, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1522 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x48, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1523 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x49, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1524 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x4a, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1525 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x4b, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1526 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x4c, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1527 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x4d, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1528 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x4e, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1529 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x4f, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1530 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x50, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1531 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x51, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1532 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x52, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1533 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x53, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1534 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x54, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1535 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x55, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1536 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x56, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1537 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x57, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1538 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x58, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1539 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x59, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1540 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x5a, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1541 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x5b, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1542 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x5c, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1543 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x5d, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1544 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x5e, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1545 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x5f, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1546 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x60, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1547 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x61, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1548 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x62, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1549 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x63, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1550 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x64, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1551 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x65, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1552 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x66, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1553 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x67, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1554 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x68, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1555 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x69, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1556 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x6a, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1557 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x6b, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1558 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x6c, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1559 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x6d, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1560 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x6e, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1561 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x6f, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1562 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x70, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1563 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x71, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1564 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x72, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1565 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x73, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1566 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x74, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1567 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x75, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1568 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x76, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1569 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x77, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1570 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x78, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1571 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x79, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1572 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x7a, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1573 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x7b, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1574 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x7c, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1575 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x7d, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1576 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x7e, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1577 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x7f, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1578 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x80, 0x00 }, NACLi_VMX,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvept,
    2, g_Operands + 667,
    NULL
  },
  /* 1579 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x81, 0x00 }, NACLi_VMX,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvvpid,
    2, g_Operands + 669,
    NULL
  },
  /* 1580 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x82, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1581 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x83, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1582 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x84, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1583 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x85, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1584 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x86, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1585 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x87, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1586 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x88, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1587 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x89, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1588 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x8a, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1589 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x8b, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1590 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x8c, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1591 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x8d, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1592 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x8e, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1593 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x8f, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1594 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x90, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1595 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x91, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1596 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x92, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1597 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x93, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1598 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x94, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1599 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x95, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1600 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x96, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1601 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x97, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1602 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x98, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1603 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x99, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1604 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x9a, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1605 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x9b, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1606 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x9c, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1607 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x9d, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1608 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x9e, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1609 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0x9f, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1610 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xa0, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1611 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xa1, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1612 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xa2, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1613 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xa3, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1614 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xa4, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1615 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xa5, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1616 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xa6, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1617 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xa7, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1618 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xa8, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1619 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xa9, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1620 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xaa, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1621 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xab, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1622 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xac, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1623 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xad, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1624 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xae, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1625 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xaf, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1626 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xb0, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1627 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xb1, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1628 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xb2, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1629 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xb3, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1630 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xb4, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1631 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xb5, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1632 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xb6, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1633 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xb7, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1634 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xb8, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1635 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xb9, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1636 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xba, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1637 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xbb, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1638 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xbc, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1639 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xbd, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1640 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xbe, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1641 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xbf, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1642 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xc0, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1643 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xc1, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1644 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xc2, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1645 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xc3, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1646 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xc4, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1647 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xc5, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1648 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xc6, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1649 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xc7, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1650 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xc8, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1651 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xc9, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1652 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xca, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1653 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xcb, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1654 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xcc, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1655 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xcd, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1656 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xce, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1657 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xcf, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1658 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xd0, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1659 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xd1, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1660 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xd2, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1661 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xd3, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1662 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xd4, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1663 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xd5, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1664 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xd6, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1665 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xd7, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1666 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xd8, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1667 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xd9, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1668 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xda, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1669 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xdb, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1670 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xdc, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1671 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xdd, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1672 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xde, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1673 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xdf, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1674 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xe0, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1675 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xe1, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1676 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xe2, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1677 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xe3, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1678 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xe4, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1679 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xe5, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1680 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xe6, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1681 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xe7, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1682 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xe8, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1683 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xe9, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1684 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xea, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1685 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xeb, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1686 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xec, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1687 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xed, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1688 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xee, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1689 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xef, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1690 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xf2, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1691 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xf3, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1692 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xf4, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1693 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xf5, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1694 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xf6, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1695 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xf7, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1696 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xf8, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1697 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xf9, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1698 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xfa, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1699 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xfb, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1700 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xfc, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1701 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xfd, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1702 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xfe, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1703 */
  { Prefix660F38, 3, { 0x0f, 0x38, 0xff, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1704 */
  { PrefixF20F38, 3, { 0x0f, 0x38, 0xf0, 0x00 }, NACLi_SSE42,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(OperandSize_b),
    InstCrc32,
    2, g_Operands + 671,
    NULL
  },
  /* 1705 */
  { PrefixF20F38, 3, { 0x0f, 0x38, 0xf1, 0x00 }, NACLi_SSE42,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCrc32,
    2, g_Operands + 673,
    NULL
  },
  /* 1706 */
  { Prefix0F3A, 3, { 0x0f, 0x3a, 0x0f, 0x00 }, NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstPalignr,
    3, g_Operands + 675,
    NULL
  },
  /* 1707 */
  { Prefix660F3A, 3, { 0x0f, 0x3a, 0x08, 0x00 }, NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstRoundps,
    3, g_Operands + 615,
    NULL
  },
  /* 1708 */
  { Prefix660F3A, 3, { 0x0f, 0x3a, 0x09, 0x00 }, NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstRoundpd,
    3, g_Operands + 615,
    NULL
  },
  /* 1709 */
  { Prefix660F3A, 3, { 0x0f, 0x3a, 0x0a, 0x00 }, NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstRoundss,
    3, g_Operands + 678,
    NULL
  },
  /* 1710 */
  { Prefix660F3A, 3, { 0x0f, 0x3a, 0x0b, 0x00 }, NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstRoundsd,
    3, g_Operands + 681,
    NULL
  },
  /* 1711 */
  { Prefix660F3A, 3, { 0x0f, 0x3a, 0x0c, 0x00 }, NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstBlendps,
    3, g_Operands + 684,
    NULL
  },
  /* 1712 */
  { Prefix660F3A, 3, { 0x0f, 0x3a, 0x0d, 0x00 }, NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstBlendpd,
    3, g_Operands + 687,
    NULL
  },
  /* 1713 */
  { Prefix660F3A, 3, { 0x0f, 0x3a, 0x0e, 0x00 }, NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPblendw,
    3, g_Operands + 684,
    NULL
  },
  /* 1714 */
  { Prefix660F3A, 3, { 0x0f, 0x3a, 0x0f, 0x00 }, NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPalignr,
    3, g_Operands + 684,
    NULL
  },
  /* 1715 */
  { Prefix660F3A, 3, { 0x0f, 0x3a, 0x14, 0x00 }, NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPextrb,
    3, g_Operands + 690,
    NULL
  },
  /* 1716 */
  { Prefix660F3A, 3, { 0x0f, 0x3a, 0x15, 0x00 }, NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPextrw,
    3, g_Operands + 693,
    NULL
  },
  /* 1717 */
  { Prefix660F3A, 3, { 0x0f, 0x3a, 0x16, 0x00 }, NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstPextrd,
    3, g_Operands + 696,
    g_Opcodes + 1718
  },
  /* 1718 */
  { Prefix660F3A, 3, { 0x0f, 0x3a, 0x16, 0x00 }, NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_o),
    InstPextrq,
    3, g_Operands + 699,
    NULL
  },
  /* 1719 */
  { Prefix660F3A, 3, { 0x0f, 0x3a, 0x17, 0x00 }, NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstExtractps,
    3, g_Operands + 702,
    NULL
  },
  /* 1720 */
  { Prefix660F3A, 3, { 0x0f, 0x3a, 0x20, 0x00 }, NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstPinsrb,
    3, g_Operands + 705,
    NULL
  },
  /* 1721 */
  { Prefix660F3A, 3, { 0x0f, 0x3a, 0x21, 0x00 }, NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstInsertps,
    3, g_Operands + 708,
    NULL
  },
  /* 1722 */
  { Prefix660F3A, 3, { 0x0f, 0x3a, 0x22, 0x00 }, NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstPinsrd,
    3, g_Operands + 711,
    g_Opcodes + 1723
  },
  /* 1723 */
  { Prefix660F3A, 3, { 0x0f, 0x3a, 0x22, 0x00 }, NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_o),
    InstPinsrq,
    3, g_Operands + 714,
    NULL
  },
  /* 1724 */
  { Prefix660F3A, 3, { 0x0f, 0x3a, 0x40, 0x00 }, NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstDpps,
    3, g_Operands + 684,
    NULL
  },
  /* 1725 */
  { Prefix660F3A, 3, { 0x0f, 0x3a, 0x41, 0x00 }, NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstDppd,
    3, g_Operands + 684,
    NULL
  },
  /* 1726 */
  { Prefix660F3A, 3, { 0x0f, 0x3a, 0x42, 0x00 }, NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMpsadbw,
    3, g_Operands + 684,
    NULL
  },
  /* 1727 */
  { Prefix660F3A, 3, { 0x0f, 0x3a, 0x60, 0x00 }, NACLi_SSE42,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstPcmpestrm,
    6, g_Operands + 717,
    NULL
  },
  /* 1728 */
  { Prefix660F3A, 3, { 0x0f, 0x3a, 0x61, 0x00 }, NACLi_SSE42,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstPcmpestri,
    6, g_Operands + 723,
    NULL
  },
  /* 1729 */
  { Prefix660F3A, 3, { 0x0f, 0x3a, 0x62, 0x00 }, NACLi_SSE42,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPcmpistrm,
    4, g_Operands + 729,
    NULL
  },
  /* 1730 */
  { Prefix660F3A, 3, { 0x0f, 0x3a, 0x63, 0x00 }, NACLi_SSE42,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstPcmpistri,
    4, g_Operands + 733,
    NULL
  },
  /* 1731 */
  { PrefixD8, 2, { 0xd8, 0xc0, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd,
    2, g_Operands + 737,
    NULL
  },
  /* 1732 */
  { PrefixD8, 2, { 0xd8, 0xc1, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd,
    2, g_Operands + 739,
    NULL
  },
  /* 1733 */
  { PrefixD8, 2, { 0xd8, 0xc2, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd,
    2, g_Operands + 741,
    NULL
  },
  /* 1734 */
  { PrefixD8, 2, { 0xd8, 0xc3, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd,
    2, g_Operands + 743,
    NULL
  },
  /* 1735 */
  { PrefixD8, 2, { 0xd8, 0xc4, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd,
    2, g_Operands + 745,
    NULL
  },
  /* 1736 */
  { PrefixD8, 2, { 0xd8, 0xc5, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd,
    2, g_Operands + 747,
    NULL
  },
  /* 1737 */
  { PrefixD8, 2, { 0xd8, 0xc6, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd,
    2, g_Operands + 749,
    NULL
  },
  /* 1738 */
  { PrefixD8, 2, { 0xd8, 0xc7, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd,
    2, g_Operands + 751,
    NULL
  },
  /* 1739 */
  { PrefixD8, 2, { 0xd8, 0xc8, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul,
    2, g_Operands + 737,
    NULL
  },
  /* 1740 */
  { PrefixD8, 2, { 0xd8, 0xc9, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul,
    2, g_Operands + 739,
    NULL
  },
  /* 1741 */
  { PrefixD8, 2, { 0xd8, 0xca, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul,
    2, g_Operands + 741,
    NULL
  },
  /* 1742 */
  { PrefixD8, 2, { 0xd8, 0xcb, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul,
    2, g_Operands + 743,
    NULL
  },
  /* 1743 */
  { PrefixD8, 2, { 0xd8, 0xcc, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul,
    2, g_Operands + 745,
    NULL
  },
  /* 1744 */
  { PrefixD8, 2, { 0xd8, 0xcd, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul,
    2, g_Operands + 747,
    NULL
  },
  /* 1745 */
  { PrefixD8, 2, { 0xd8, 0xce, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul,
    2, g_Operands + 749,
    NULL
  },
  /* 1746 */
  { PrefixD8, 2, { 0xd8, 0xcf, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul,
    2, g_Operands + 753,
    NULL
  },
  /* 1747 */
  { PrefixD8, 2, { 0xd8, 0xd0, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcom,
    2, g_Operands + 755,
    NULL
  },
  /* 1748 */
  { PrefixD8, 2, { 0xd8, 0xd1, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcom,
    2, g_Operands + 757,
    NULL
  },
  /* 1749 */
  { PrefixD8, 2, { 0xd8, 0xd2, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcom,
    2, g_Operands + 759,
    NULL
  },
  /* 1750 */
  { PrefixD8, 2, { 0xd8, 0xd3, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcom,
    2, g_Operands + 761,
    NULL
  },
  /* 1751 */
  { PrefixD8, 2, { 0xd8, 0xd4, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcom,
    2, g_Operands + 763,
    NULL
  },
  /* 1752 */
  { PrefixD8, 2, { 0xd8, 0xd5, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcom,
    2, g_Operands + 765,
    NULL
  },
  /* 1753 */
  { PrefixD8, 2, { 0xd8, 0xd6, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcom,
    2, g_Operands + 767,
    NULL
  },
  /* 1754 */
  { PrefixD8, 2, { 0xd8, 0xd7, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcom,
    2, g_Operands + 769,
    NULL
  },
  /* 1755 */
  { PrefixD8, 2, { 0xd8, 0xd8, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomp,
    2, g_Operands + 755,
    NULL
  },
  /* 1756 */
  { PrefixD8, 2, { 0xd8, 0xd9, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomp,
    2, g_Operands + 757,
    NULL
  },
  /* 1757 */
  { PrefixD8, 2, { 0xd8, 0xda, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomp,
    2, g_Operands + 759,
    NULL
  },
  /* 1758 */
  { PrefixD8, 2, { 0xd8, 0xdb, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomp,
    2, g_Operands + 761,
    NULL
  },
  /* 1759 */
  { PrefixD8, 2, { 0xd8, 0xdc, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomp,
    2, g_Operands + 763,
    NULL
  },
  /* 1760 */
  { PrefixD8, 2, { 0xd8, 0xdd, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomp,
    2, g_Operands + 765,
    NULL
  },
  /* 1761 */
  { PrefixD8, 2, { 0xd8, 0xde, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomp,
    2, g_Operands + 767,
    NULL
  },
  /* 1762 */
  { PrefixD8, 2, { 0xd8, 0xdf, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomp,
    2, g_Operands + 771,
    NULL
  },
  /* 1763 */
  { PrefixD8, 2, { 0xd8, 0xe0, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub,
    2, g_Operands + 737,
    NULL
  },
  /* 1764 */
  { PrefixD8, 2, { 0xd8, 0xe1, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub,
    2, g_Operands + 739,
    NULL
  },
  /* 1765 */
  { PrefixD8, 2, { 0xd8, 0xe2, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub,
    2, g_Operands + 741,
    NULL
  },
  /* 1766 */
  { PrefixD8, 2, { 0xd8, 0xe3, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub,
    2, g_Operands + 743,
    NULL
  },
  /* 1767 */
  { PrefixD8, 2, { 0xd8, 0xe4, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub,
    2, g_Operands + 745,
    NULL
  },
  /* 1768 */
  { PrefixD8, 2, { 0xd8, 0xe5, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub,
    2, g_Operands + 747,
    NULL
  },
  /* 1769 */
  { PrefixD8, 2, { 0xd8, 0xe6, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub,
    2, g_Operands + 749,
    NULL
  },
  /* 1770 */
  { PrefixD8, 2, { 0xd8, 0xe7, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub,
    2, g_Operands + 751,
    NULL
  },
  /* 1771 */
  { PrefixD8, 2, { 0xd8, 0xe8, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr,
    2, g_Operands + 737,
    NULL
  },
  /* 1772 */
  { PrefixD8, 2, { 0xd8, 0xe9, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr,
    2, g_Operands + 739,
    NULL
  },
  /* 1773 */
  { PrefixD8, 2, { 0xd8, 0xea, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr,
    2, g_Operands + 741,
    NULL
  },
  /* 1774 */
  { PrefixD8, 2, { 0xd8, 0xeb, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr,
    2, g_Operands + 743,
    NULL
  },
  /* 1775 */
  { PrefixD8, 2, { 0xd8, 0xec, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr,
    2, g_Operands + 745,
    NULL
  },
  /* 1776 */
  { PrefixD8, 2, { 0xd8, 0xed, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr,
    2, g_Operands + 747,
    NULL
  },
  /* 1777 */
  { PrefixD8, 2, { 0xd8, 0xee, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr,
    2, g_Operands + 749,
    NULL
  },
  /* 1778 */
  { PrefixD8, 2, { 0xd8, 0xef, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr,
    2, g_Operands + 751,
    NULL
  },
  /* 1779 */
  { PrefixD8, 2, { 0xd8, 0xf0, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv,
    2, g_Operands + 737,
    NULL
  },
  /* 1780 */
  { PrefixD8, 2, { 0xd8, 0xf1, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv,
    2, g_Operands + 739,
    NULL
  },
  /* 1781 */
  { PrefixD8, 2, { 0xd8, 0xf2, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv,
    2, g_Operands + 741,
    NULL
  },
  /* 1782 */
  { PrefixD8, 2, { 0xd8, 0xf3, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv,
    2, g_Operands + 743,
    NULL
  },
  /* 1783 */
  { PrefixD8, 2, { 0xd8, 0xf4, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv,
    2, g_Operands + 745,
    NULL
  },
  /* 1784 */
  { PrefixD8, 2, { 0xd8, 0xf5, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv,
    2, g_Operands + 747,
    NULL
  },
  /* 1785 */
  { PrefixD8, 2, { 0xd8, 0xf6, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv,
    2, g_Operands + 749,
    NULL
  },
  /* 1786 */
  { PrefixD8, 2, { 0xd8, 0xf7, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv,
    2, g_Operands + 751,
    NULL
  },
  /* 1787 */
  { PrefixD8, 2, { 0xd8, 0xf8, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr,
    2, g_Operands + 737,
    NULL
  },
  /* 1788 */
  { PrefixD8, 2, { 0xd8, 0xf9, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr,
    2, g_Operands + 739,
    NULL
  },
  /* 1789 */
  { PrefixD8, 2, { 0xd8, 0xfa, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr,
    2, g_Operands + 741,
    NULL
  },
  /* 1790 */
  { PrefixD8, 2, { 0xd8, 0xfb, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr,
    2, g_Operands + 743,
    NULL
  },
  /* 1791 */
  { PrefixD8, 2, { 0xd8, 0xfc, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr,
    2, g_Operands + 745,
    NULL
  },
  /* 1792 */
  { PrefixD8, 2, { 0xd8, 0xfd, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr,
    2, g_Operands + 747,
    NULL
  },
  /* 1793 */
  { PrefixD8, 2, { 0xd8, 0xfe, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr,
    2, g_Operands + 749,
    NULL
  },
  /* 1794 */
  { PrefixD8, 2, { 0xd8, 0xff, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr,
    2, g_Operands + 751,
    NULL
  },
  /* 1795 */
  { PrefixD9, 2, { 0xd9, 0xc0, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFld,
    2, g_Operands + 773,
    NULL
  },
  /* 1796 */
  { PrefixD9, 2, { 0xd9, 0xc1, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFld,
    2, g_Operands + 775,
    NULL
  },
  /* 1797 */
  { PrefixD9, 2, { 0xd9, 0xc2, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFld,
    2, g_Operands + 777,
    NULL
  },
  /* 1798 */
  { PrefixD9, 2, { 0xd9, 0xc3, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFld,
    2, g_Operands + 779,
    NULL
  },
  /* 1799 */
  { PrefixD9, 2, { 0xd9, 0xc4, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFld,
    2, g_Operands + 781,
    NULL
  },
  /* 1800 */
  { PrefixD9, 2, { 0xd9, 0xc5, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFld,
    2, g_Operands + 783,
    NULL
  },
  /* 1801 */
  { PrefixD9, 2, { 0xd9, 0xc6, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFld,
    2, g_Operands + 785,
    NULL
  },
  /* 1802 */
  { PrefixD9, 2, { 0xd9, 0xc7, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFld,
    2, g_Operands + 787,
    NULL
  },
  /* 1803 */
  { PrefixD9, 2, { 0xd9, 0xc8, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFxch,
    2, g_Operands + 789,
    NULL
  },
  /* 1804 */
  { PrefixD9, 2, { 0xd9, 0xc9, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFxch,
    2, g_Operands + 791,
    NULL
  },
  /* 1805 */
  { PrefixD9, 2, { 0xd9, 0xca, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFxch,
    2, g_Operands + 793,
    NULL
  },
  /* 1806 */
  { PrefixD9, 2, { 0xd9, 0xcb, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFxch,
    2, g_Operands + 795,
    NULL
  },
  /* 1807 */
  { PrefixD9, 2, { 0xd9, 0xcc, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFxch,
    2, g_Operands + 797,
    NULL
  },
  /* 1808 */
  { PrefixD9, 2, { 0xd9, 0xcd, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFxch,
    2, g_Operands + 799,
    NULL
  },
  /* 1809 */
  { PrefixD9, 2, { 0xd9, 0xce, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFxch,
    2, g_Operands + 801,
    NULL
  },
  /* 1810 */
  { PrefixD9, 2, { 0xd9, 0xcf, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFxch,
    2, g_Operands + 803,
    NULL
  },
  /* 1811 */
  { PrefixD9, 2, { 0xd9, 0xd0, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFnop,
    0, g_Operands + 0,
    NULL
  },
  /* 1812 */
  { PrefixD9, 2, { 0xd9, 0xd1, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1813 */
  { PrefixD9, 2, { 0xd9, 0xd2, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1814 */
  { PrefixD9, 2, { 0xd9, 0xd3, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1815 */
  { PrefixD9, 2, { 0xd9, 0xd4, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1816 */
  { PrefixD9, 2, { 0xd9, 0xd5, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1817 */
  { PrefixD9, 2, { 0xd9, 0xd6, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1818 */
  { PrefixD9, 2, { 0xd9, 0xd7, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1819 */
  { PrefixD9, 2, { 0xd9, 0xd8, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1820 */
  { PrefixD9, 2, { 0xd9, 0xd9, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1821 */
  { PrefixD9, 2, { 0xd9, 0xda, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1822 */
  { PrefixD9, 2, { 0xd9, 0xdb, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1823 */
  { PrefixD9, 2, { 0xd9, 0xdc, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1824 */
  { PrefixD9, 2, { 0xd9, 0xdd, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1825 */
  { PrefixD9, 2, { 0xd9, 0xde, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1826 */
  { PrefixD9, 2, { 0xd9, 0xdf, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1827 */
  { PrefixD9, 2, { 0xd9, 0xe0, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFchs,
    1, g_Operands + 207,
    NULL
  },
  /* 1828 */
  { PrefixD9, 2, { 0xd9, 0xe1, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFabs,
    1, g_Operands + 207,
    NULL
  },
  /* 1829 */
  { PrefixD9, 2, { 0xd9, 0xe2, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1830 */
  { PrefixD9, 2, { 0xd9, 0xe3, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1831 */
  { PrefixD9, 2, { 0xd9, 0xe4, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFtst,
    1, g_Operands + 211,
    NULL
  },
  /* 1832 */
  { PrefixD9, 2, { 0xd9, 0xe5, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFxam,
    1, g_Operands + 211,
    NULL
  },
  /* 1833 */
  { PrefixD9, 2, { 0xd9, 0xe6, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1834 */
  { PrefixD9, 2, { 0xd9, 0xe7, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1835 */
  { PrefixD9, 2, { 0xd9, 0xe8, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFld1,
    1, g_Operands + 207,
    NULL
  },
  /* 1836 */
  { PrefixD9, 2, { 0xd9, 0xe9, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFldl2t,
    1, g_Operands + 207,
    NULL
  },
  /* 1837 */
  { PrefixD9, 2, { 0xd9, 0xea, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFldl2e,
    1, g_Operands + 207,
    NULL
  },
  /* 1838 */
  { PrefixD9, 2, { 0xd9, 0xeb, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFldpi,
    1, g_Operands + 207,
    NULL
  },
  /* 1839 */
  { PrefixD9, 2, { 0xd9, 0xec, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFldlg2,
    1, g_Operands + 207,
    NULL
  },
  /* 1840 */
  { PrefixD9, 2, { 0xd9, 0xed, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFldln2,
    1, g_Operands + 207,
    NULL
  },
  /* 1841 */
  { PrefixD9, 2, { 0xd9, 0xee, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFldz,
    1, g_Operands + 207,
    NULL
  },
  /* 1842 */
  { PrefixD9, 2, { 0xd9, 0xef, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1843 */
  { PrefixD9, 2, { 0xd9, 0xf0, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstF2xm1,
    1, g_Operands + 207,
    NULL
  },
  /* 1844 */
  { PrefixD9, 2, { 0xd9, 0xf1, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFyl2x,
    2, g_Operands + 739,
    NULL
  },
  /* 1845 */
  { PrefixD9, 2, { 0xd9, 0xf2, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFptan,
    2, g_Operands + 775,
    NULL
  },
  /* 1846 */
  { PrefixD9, 2, { 0xd9, 0xf3, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFpatan,
    2, g_Operands + 739,
    NULL
  },
  /* 1847 */
  { PrefixD9, 2, { 0xd9, 0xf4, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFxtract,
    2, g_Operands + 775,
    NULL
  },
  /* 1848 */
  { PrefixD9, 2, { 0xd9, 0xf5, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFprem1,
    2, g_Operands + 739,
    NULL
  },
  /* 1849 */
  { PrefixD9, 2, { 0xd9, 0xf6, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdecstp,
    0, g_Operands + 0,
    NULL
  },
  /* 1850 */
  { PrefixD9, 2, { 0xd9, 0xf7, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFincstp,
    0, g_Operands + 0,
    NULL
  },
  /* 1851 */
  { PrefixD9, 2, { 0xd9, 0xf8, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFprem,
    2, g_Operands + 739,
    NULL
  },
  /* 1852 */
  { PrefixD9, 2, { 0xd9, 0xf9, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFyl2xp1,
    2, g_Operands + 739,
    NULL
  },
  /* 1853 */
  { PrefixD9, 2, { 0xd9, 0xfa, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsqrt,
    1, g_Operands + 207,
    NULL
  },
  /* 1854 */
  { PrefixD9, 2, { 0xd9, 0xfb, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsincos,
    2, g_Operands + 775,
    NULL
  },
  /* 1855 */
  { PrefixD9, 2, { 0xd9, 0xfc, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFrndint,
    1, g_Operands + 207,
    NULL
  },
  /* 1856 */
  { PrefixD9, 2, { 0xd9, 0xfd, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFscale,
    2, g_Operands + 739,
    NULL
  },
  /* 1857 */
  { PrefixD9, 2, { 0xd9, 0xfe, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsin,
    1, g_Operands + 207,
    NULL
  },
  /* 1858 */
  { PrefixD9, 2, { 0xd9, 0xff, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcos,
    1, g_Operands + 207,
    NULL
  },
  /* 1859 */
  { PrefixDA, 2, { 0xda, 0xc0, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovb,
    2, g_Operands + 737,
    NULL
  },
  /* 1860 */
  { PrefixDA, 2, { 0xda, 0xc1, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovb,
    2, g_Operands + 739,
    NULL
  },
  /* 1861 */
  { PrefixDA, 2, { 0xda, 0xc2, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovb,
    2, g_Operands + 741,
    NULL
  },
  /* 1862 */
  { PrefixDA, 2, { 0xda, 0xc3, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovb,
    2, g_Operands + 743,
    NULL
  },
  /* 1863 */
  { PrefixDA, 2, { 0xda, 0xc4, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovb,
    2, g_Operands + 745,
    NULL
  },
  /* 1864 */
  { PrefixDA, 2, { 0xda, 0xc5, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovb,
    2, g_Operands + 747,
    NULL
  },
  /* 1865 */
  { PrefixDA, 2, { 0xda, 0xc6, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovb,
    2, g_Operands + 749,
    NULL
  },
  /* 1866 */
  { PrefixDA, 2, { 0xda, 0xc7, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovb,
    2, g_Operands + 751,
    NULL
  },
  /* 1867 */
  { PrefixDA, 2, { 0xda, 0xc8, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmove,
    2, g_Operands + 737,
    NULL
  },
  /* 1868 */
  { PrefixDA, 2, { 0xda, 0xc9, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmove,
    2, g_Operands + 739,
    NULL
  },
  /* 1869 */
  { PrefixDA, 2, { 0xda, 0xca, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmove,
    2, g_Operands + 741,
    NULL
  },
  /* 1870 */
  { PrefixDA, 2, { 0xda, 0xcb, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmove,
    2, g_Operands + 743,
    NULL
  },
  /* 1871 */
  { PrefixDA, 2, { 0xda, 0xcc, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmove,
    2, g_Operands + 745,
    NULL
  },
  /* 1872 */
  { PrefixDA, 2, { 0xda, 0xcd, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmove,
    2, g_Operands + 747,
    NULL
  },
  /* 1873 */
  { PrefixDA, 2, { 0xda, 0xce, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmove,
    2, g_Operands + 749,
    NULL
  },
  /* 1874 */
  { PrefixDA, 2, { 0xda, 0xcf, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmove,
    2, g_Operands + 751,
    NULL
  },
  /* 1875 */
  { PrefixDA, 2, { 0xda, 0xd0, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovbe,
    2, g_Operands + 737,
    NULL
  },
  /* 1876 */
  { PrefixDA, 2, { 0xda, 0xd1, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovbe,
    2, g_Operands + 739,
    NULL
  },
  /* 1877 */
  { PrefixDA, 2, { 0xda, 0xd2, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovbe,
    2, g_Operands + 741,
    NULL
  },
  /* 1878 */
  { PrefixDA, 2, { 0xda, 0xd3, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovbe,
    2, g_Operands + 743,
    NULL
  },
  /* 1879 */
  { PrefixDA, 2, { 0xda, 0xd4, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovbe,
    2, g_Operands + 745,
    NULL
  },
  /* 1880 */
  { PrefixDA, 2, { 0xda, 0xd5, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovbe,
    2, g_Operands + 747,
    NULL
  },
  /* 1881 */
  { PrefixDA, 2, { 0xda, 0xd6, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovbe,
    2, g_Operands + 749,
    NULL
  },
  /* 1882 */
  { PrefixDA, 2, { 0xda, 0xd7, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovbe,
    2, g_Operands + 751,
    NULL
  },
  /* 1883 */
  { PrefixDA, 2, { 0xda, 0xd8, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovu,
    2, g_Operands + 737,
    NULL
  },
  /* 1884 */
  { PrefixDA, 2, { 0xda, 0xd9, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovu,
    2, g_Operands + 739,
    NULL
  },
  /* 1885 */
  { PrefixDA, 2, { 0xda, 0xda, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovu,
    2, g_Operands + 741,
    NULL
  },
  /* 1886 */
  { PrefixDA, 2, { 0xda, 0xdb, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovu,
    2, g_Operands + 743,
    NULL
  },
  /* 1887 */
  { PrefixDA, 2, { 0xda, 0xdc, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovu,
    2, g_Operands + 745,
    NULL
  },
  /* 1888 */
  { PrefixDA, 2, { 0xda, 0xdd, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovu,
    2, g_Operands + 747,
    NULL
  },
  /* 1889 */
  { PrefixDA, 2, { 0xda, 0xde, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovu,
    2, g_Operands + 749,
    NULL
  },
  /* 1890 */
  { PrefixDA, 2, { 0xda, 0xdf, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovu,
    2, g_Operands + 751,
    NULL
  },
  /* 1891 */
  { PrefixDA, 2, { 0xda, 0xe0, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1892 */
  { PrefixDA, 2, { 0xda, 0xe1, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1893 */
  { PrefixDA, 2, { 0xda, 0xe2, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1894 */
  { PrefixDA, 2, { 0xda, 0xe3, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1895 */
  { PrefixDA, 2, { 0xda, 0xe4, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1896 */
  { PrefixDA, 2, { 0xda, 0xe5, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1897 */
  { PrefixDA, 2, { 0xda, 0xe6, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1898 */
  { PrefixDA, 2, { 0xda, 0xe7, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1899 */
  { PrefixDA, 2, { 0xda, 0xe8, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1900 */
  { PrefixDA, 2, { 0xda, 0xe9, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucompp,
    2, g_Operands + 757,
    NULL
  },
  /* 1901 */
  { PrefixDA, 2, { 0xda, 0xea, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1902 */
  { PrefixDA, 2, { 0xda, 0xeb, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1903 */
  { PrefixDA, 2, { 0xda, 0xec, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1904 */
  { PrefixDA, 2, { 0xda, 0xed, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1905 */
  { PrefixDA, 2, { 0xda, 0xee, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1906 */
  { PrefixDA, 2, { 0xda, 0xef, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1907 */
  { PrefixDA, 2, { 0xda, 0xf0, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1908 */
  { PrefixDA, 2, { 0xda, 0xf1, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1909 */
  { PrefixDA, 2, { 0xda, 0xf2, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1910 */
  { PrefixDA, 2, { 0xda, 0xf3, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1911 */
  { PrefixDA, 2, { 0xda, 0xf4, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1912 */
  { PrefixDA, 2, { 0xda, 0xf5, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1913 */
  { PrefixDA, 2, { 0xda, 0xf6, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1914 */
  { PrefixDA, 2, { 0xda, 0xf7, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1915 */
  { PrefixDA, 2, { 0xda, 0xf8, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1916 */
  { PrefixDA, 2, { 0xda, 0xf9, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1917 */
  { PrefixDA, 2, { 0xda, 0xfa, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1918 */
  { PrefixDA, 2, { 0xda, 0xfb, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1919 */
  { PrefixDA, 2, { 0xda, 0xfc, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1920 */
  { PrefixDA, 2, { 0xda, 0xfd, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1921 */
  { PrefixDA, 2, { 0xda, 0xfe, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1922 */
  { PrefixDA, 2, { 0xda, 0xff, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1923 */
  { PrefixDB, 2, { 0xdb, 0xc0, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnb,
    2, g_Operands + 737,
    NULL
  },
  /* 1924 */
  { PrefixDB, 2, { 0xdb, 0xc1, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnb,
    2, g_Operands + 739,
    NULL
  },
  /* 1925 */
  { PrefixDB, 2, { 0xdb, 0xc2, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnb,
    2, g_Operands + 741,
    NULL
  },
  /* 1926 */
  { PrefixDB, 2, { 0xdb, 0xc3, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnb,
    2, g_Operands + 743,
    NULL
  },
  /* 1927 */
  { PrefixDB, 2, { 0xdb, 0xc4, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnb,
    2, g_Operands + 745,
    NULL
  },
  /* 1928 */
  { PrefixDB, 2, { 0xdb, 0xc5, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnb,
    2, g_Operands + 747,
    NULL
  },
  /* 1929 */
  { PrefixDB, 2, { 0xdb, 0xc6, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnb,
    2, g_Operands + 749,
    NULL
  },
  /* 1930 */
  { PrefixDB, 2, { 0xdb, 0xc7, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnb,
    2, g_Operands + 751,
    NULL
  },
  /* 1931 */
  { PrefixDB, 2, { 0xdb, 0xc8, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovne,
    2, g_Operands + 737,
    NULL
  },
  /* 1932 */
  { PrefixDB, 2, { 0xdb, 0xc9, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovne,
    2, g_Operands + 739,
    NULL
  },
  /* 1933 */
  { PrefixDB, 2, { 0xdb, 0xca, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovne,
    2, g_Operands + 741,
    NULL
  },
  /* 1934 */
  { PrefixDB, 2, { 0xdb, 0xcb, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovne,
    2, g_Operands + 743,
    NULL
  },
  /* 1935 */
  { PrefixDB, 2, { 0xdb, 0xcc, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovne,
    2, g_Operands + 745,
    NULL
  },
  /* 1936 */
  { PrefixDB, 2, { 0xdb, 0xcd, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovne,
    2, g_Operands + 747,
    NULL
  },
  /* 1937 */
  { PrefixDB, 2, { 0xdb, 0xce, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovne,
    2, g_Operands + 749,
    NULL
  },
  /* 1938 */
  { PrefixDB, 2, { 0xdb, 0xcf, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovne,
    2, g_Operands + 751,
    NULL
  },
  /* 1939 */
  { PrefixDB, 2, { 0xdb, 0xd0, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnbe,
    2, g_Operands + 737,
    NULL
  },
  /* 1940 */
  { PrefixDB, 2, { 0xdb, 0xd1, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnbe,
    2, g_Operands + 739,
    NULL
  },
  /* 1941 */
  { PrefixDB, 2, { 0xdb, 0xd2, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnbe,
    2, g_Operands + 741,
    NULL
  },
  /* 1942 */
  { PrefixDB, 2, { 0xdb, 0xd3, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnbe,
    2, g_Operands + 743,
    NULL
  },
  /* 1943 */
  { PrefixDB, 2, { 0xdb, 0xd4, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnbe,
    2, g_Operands + 745,
    NULL
  },
  /* 1944 */
  { PrefixDB, 2, { 0xdb, 0xd5, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnbe,
    2, g_Operands + 747,
    NULL
  },
  /* 1945 */
  { PrefixDB, 2, { 0xdb, 0xd6, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnbe,
    2, g_Operands + 749,
    NULL
  },
  /* 1946 */
  { PrefixDB, 2, { 0xdb, 0xd7, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnbe,
    2, g_Operands + 751,
    NULL
  },
  /* 1947 */
  { PrefixDB, 2, { 0xdb, 0xd8, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnu,
    2, g_Operands + 737,
    NULL
  },
  /* 1948 */
  { PrefixDB, 2, { 0xdb, 0xd9, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnu,
    2, g_Operands + 739,
    NULL
  },
  /* 1949 */
  { PrefixDB, 2, { 0xdb, 0xda, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnu,
    2, g_Operands + 741,
    NULL
  },
  /* 1950 */
  { PrefixDB, 2, { 0xdb, 0xdb, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnu,
    2, g_Operands + 743,
    NULL
  },
  /* 1951 */
  { PrefixDB, 2, { 0xdb, 0xdc, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnu,
    2, g_Operands + 745,
    NULL
  },
  /* 1952 */
  { PrefixDB, 2, { 0xdb, 0xdd, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnu,
    2, g_Operands + 747,
    NULL
  },
  /* 1953 */
  { PrefixDB, 2, { 0xdb, 0xde, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnu,
    2, g_Operands + 749,
    NULL
  },
  /* 1954 */
  { PrefixDB, 2, { 0xdb, 0xdf, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnu,
    2, g_Operands + 751,
    NULL
  },
  /* 1955 */
  { PrefixDB, 2, { 0xdb, 0xe0, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1956 */
  { PrefixDB, 2, { 0xdb, 0xe1, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1957 */
  { PrefixDB, 2, { 0xdb, 0xe2, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFnclex,
    0, g_Operands + 0,
    NULL
  },
  /* 1958 */
  { PrefixDB, 2, { 0xdb, 0xe3, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFninit,
    0, g_Operands + 0,
    NULL
  },
  /* 1959 */
  { PrefixDB, 2, { 0xdb, 0xe4, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1960 */
  { PrefixDB, 2, { 0xdb, 0xe5, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1961 */
  { PrefixDB, 2, { 0xdb, 0xe6, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1962 */
  { PrefixDB, 2, { 0xdb, 0xe7, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1963 */
  { PrefixDB, 2, { 0xdb, 0xe8, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomi,
    2, g_Operands + 755,
    NULL
  },
  /* 1964 */
  { PrefixDB, 2, { 0xdb, 0xe9, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomi,
    2, g_Operands + 757,
    NULL
  },
  /* 1965 */
  { PrefixDB, 2, { 0xdb, 0xea, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomi,
    2, g_Operands + 759,
    NULL
  },
  /* 1966 */
  { PrefixDB, 2, { 0xdb, 0xeb, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomi,
    2, g_Operands + 761,
    NULL
  },
  /* 1967 */
  { PrefixDB, 2, { 0xdb, 0xec, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomi,
    2, g_Operands + 763,
    NULL
  },
  /* 1968 */
  { PrefixDB, 2, { 0xdb, 0xed, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomi,
    2, g_Operands + 765,
    NULL
  },
  /* 1969 */
  { PrefixDB, 2, { 0xdb, 0xee, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomi,
    2, g_Operands + 767,
    NULL
  },
  /* 1970 */
  { PrefixDB, 2, { 0xdb, 0xef, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomi,
    2, g_Operands + 769,
    NULL
  },
  /* 1971 */
  { PrefixDB, 2, { 0xdb, 0xf0, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomi,
    2, g_Operands + 755,
    NULL
  },
  /* 1972 */
  { PrefixDB, 2, { 0xdb, 0xf1, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomi,
    2, g_Operands + 757,
    NULL
  },
  /* 1973 */
  { PrefixDB, 2, { 0xdb, 0xf2, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomi,
    2, g_Operands + 759,
    NULL
  },
  /* 1974 */
  { PrefixDB, 2, { 0xdb, 0xf3, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomi,
    2, g_Operands + 761,
    NULL
  },
  /* 1975 */
  { PrefixDB, 2, { 0xdb, 0xf4, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomi,
    2, g_Operands + 763,
    NULL
  },
  /* 1976 */
  { PrefixDB, 2, { 0xdb, 0xf5, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomi,
    2, g_Operands + 765,
    NULL
  },
  /* 1977 */
  { PrefixDB, 2, { 0xdb, 0xf6, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomi,
    2, g_Operands + 767,
    NULL
  },
  /* 1978 */
  { PrefixDB, 2, { 0xdb, 0xf7, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomi,
    2, g_Operands + 769,
    NULL
  },
  /* 1979 */
  { PrefixDC, 2, { 0xdc, 0xc0, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd,
    2, g_Operands + 737,
    NULL
  },
  /* 1980 */
  { PrefixDC, 2, { 0xdc, 0xc1, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd,
    2, g_Operands + 805,
    NULL
  },
  /* 1981 */
  { PrefixDC, 2, { 0xdc, 0xc2, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd,
    2, g_Operands + 807,
    NULL
  },
  /* 1982 */
  { PrefixDC, 2, { 0xdc, 0xc3, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd,
    2, g_Operands + 809,
    NULL
  },
  /* 1983 */
  { PrefixDC, 2, { 0xdc, 0xc4, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd,
    2, g_Operands + 811,
    NULL
  },
  /* 1984 */
  { PrefixDC, 2, { 0xdc, 0xc5, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd,
    2, g_Operands + 813,
    NULL
  },
  /* 1985 */
  { PrefixDC, 2, { 0xdc, 0xc6, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd,
    2, g_Operands + 815,
    NULL
  },
  /* 1986 */
  { PrefixDC, 2, { 0xdc, 0xc7, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd,
    2, g_Operands + 817,
    NULL
  },
  /* 1987 */
  { PrefixDC, 2, { 0xdc, 0xc8, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul,
    2, g_Operands + 737,
    NULL
  },
  /* 1988 */
  { PrefixDC, 2, { 0xdc, 0xc9, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul,
    2, g_Operands + 805,
    NULL
  },
  /* 1989 */
  { PrefixDC, 2, { 0xdc, 0xca, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul,
    2, g_Operands + 807,
    NULL
  },
  /* 1990 */
  { PrefixDC, 2, { 0xdc, 0xcb, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul,
    2, g_Operands + 809,
    NULL
  },
  /* 1991 */
  { PrefixDC, 2, { 0xdc, 0xcc, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul,
    2, g_Operands + 811,
    NULL
  },
  /* 1992 */
  { PrefixDC, 2, { 0xdc, 0xcd, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul,
    2, g_Operands + 813,
    NULL
  },
  /* 1993 */
  { PrefixDC, 2, { 0xdc, 0xce, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul,
    2, g_Operands + 815,
    NULL
  },
  /* 1994 */
  { PrefixDC, 2, { 0xdc, 0xcf, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul,
    2, g_Operands + 819,
    NULL
  },
  /* 1995 */
  { PrefixDC, 2, { 0xdc, 0xd0, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1996 */
  { PrefixDC, 2, { 0xdc, 0xd1, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1997 */
  { PrefixDC, 2, { 0xdc, 0xd2, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1998 */
  { PrefixDC, 2, { 0xdc, 0xd3, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 1999 */
  { PrefixDC, 2, { 0xdc, 0xd4, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2000 */
  { PrefixDC, 2, { 0xdc, 0xd5, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2001 */
  { PrefixDC, 2, { 0xdc, 0xd6, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2002 */
  { PrefixDC, 2, { 0xdc, 0xd7, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2003 */
  { PrefixDC, 2, { 0xdc, 0xd8, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2004 */
  { PrefixDC, 2, { 0xdc, 0xd9, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2005 */
  { PrefixDC, 2, { 0xdc, 0xda, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2006 */
  { PrefixDC, 2, { 0xdc, 0xdb, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2007 */
  { PrefixDC, 2, { 0xdc, 0xdc, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2008 */
  { PrefixDC, 2, { 0xdc, 0xdd, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2009 */
  { PrefixDC, 2, { 0xdc, 0xde, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2010 */
  { PrefixDC, 2, { 0xdc, 0xdf, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2011 */
  { PrefixDC, 2, { 0xdc, 0xe0, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr,
    2, g_Operands + 737,
    NULL
  },
  /* 2012 */
  { PrefixDC, 2, { 0xdc, 0xe1, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr,
    2, g_Operands + 805,
    NULL
  },
  /* 2013 */
  { PrefixDC, 2, { 0xdc, 0xe2, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr,
    2, g_Operands + 807,
    NULL
  },
  /* 2014 */
  { PrefixDC, 2, { 0xdc, 0xe3, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr,
    2, g_Operands + 809,
    NULL
  },
  /* 2015 */
  { PrefixDC, 2, { 0xdc, 0xe4, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr,
    2, g_Operands + 811,
    NULL
  },
  /* 2016 */
  { PrefixDC, 2, { 0xdc, 0xe5, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr,
    2, g_Operands + 813,
    NULL
  },
  /* 2017 */
  { PrefixDC, 2, { 0xdc, 0xe6, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr,
    2, g_Operands + 815,
    NULL
  },
  /* 2018 */
  { PrefixDC, 2, { 0xdc, 0xe7, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr,
    2, g_Operands + 817,
    NULL
  },
  /* 2019 */
  { PrefixDC, 2, { 0xdc, 0xe8, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub,
    2, g_Operands + 737,
    NULL
  },
  /* 2020 */
  { PrefixDC, 2, { 0xdc, 0xe9, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub,
    2, g_Operands + 805,
    NULL
  },
  /* 2021 */
  { PrefixDC, 2, { 0xdc, 0xea, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub,
    2, g_Operands + 807,
    NULL
  },
  /* 2022 */
  { PrefixDC, 2, { 0xdc, 0xeb, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub,
    2, g_Operands + 809,
    NULL
  },
  /* 2023 */
  { PrefixDC, 2, { 0xdc, 0xec, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub,
    2, g_Operands + 811,
    NULL
  },
  /* 2024 */
  { PrefixDC, 2, { 0xdc, 0xed, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub,
    2, g_Operands + 813,
    NULL
  },
  /* 2025 */
  { PrefixDC, 2, { 0xdc, 0xee, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub,
    2, g_Operands + 815,
    NULL
  },
  /* 2026 */
  { PrefixDC, 2, { 0xdc, 0xef, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub,
    2, g_Operands + 817,
    NULL
  },
  /* 2027 */
  { PrefixDC, 2, { 0xdc, 0xf0, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr,
    2, g_Operands + 737,
    NULL
  },
  /* 2028 */
  { PrefixDC, 2, { 0xdc, 0xf1, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr,
    2, g_Operands + 805,
    NULL
  },
  /* 2029 */
  { PrefixDC, 2, { 0xdc, 0xf2, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr,
    2, g_Operands + 807,
    NULL
  },
  /* 2030 */
  { PrefixDC, 2, { 0xdc, 0xf3, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr,
    2, g_Operands + 809,
    NULL
  },
  /* 2031 */
  { PrefixDC, 2, { 0xdc, 0xf4, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr,
    2, g_Operands + 811,
    NULL
  },
  /* 2032 */
  { PrefixDC, 2, { 0xdc, 0xf5, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr,
    2, g_Operands + 813,
    NULL
  },
  /* 2033 */
  { PrefixDC, 2, { 0xdc, 0xf6, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr,
    2, g_Operands + 815,
    NULL
  },
  /* 2034 */
  { PrefixDC, 2, { 0xdc, 0xf7, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr,
    2, g_Operands + 817,
    NULL
  },
  /* 2035 */
  { PrefixDC, 2, { 0xdc, 0xf8, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv,
    2, g_Operands + 737,
    NULL
  },
  /* 2036 */
  { PrefixDC, 2, { 0xdc, 0xf9, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv,
    2, g_Operands + 805,
    NULL
  },
  /* 2037 */
  { PrefixDC, 2, { 0xdc, 0xfa, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv,
    2, g_Operands + 807,
    NULL
  },
  /* 2038 */
  { PrefixDC, 2, { 0xdc, 0xfb, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv,
    2, g_Operands + 809,
    NULL
  },
  /* 2039 */
  { PrefixDC, 2, { 0xdc, 0xfc, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv,
    2, g_Operands + 811,
    NULL
  },
  /* 2040 */
  { PrefixDC, 2, { 0xdc, 0xfd, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv,
    2, g_Operands + 813,
    NULL
  },
  /* 2041 */
  { PrefixDC, 2, { 0xdc, 0xfe, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv,
    2, g_Operands + 815,
    NULL
  },
  /* 2042 */
  { PrefixDC, 2, { 0xdc, 0xff, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv,
    2, g_Operands + 817,
    NULL
  },
  /* 2043 */
  { PrefixDD, 2, { 0xdd, 0xc0, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFfree,
    1, g_Operands + 821,
    NULL
  },
  /* 2044 */
  { PrefixDD, 2, { 0xdd, 0xc1, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFfree,
    1, g_Operands + 822,
    NULL
  },
  /* 2045 */
  { PrefixDD, 2, { 0xdd, 0xc2, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFfree,
    1, g_Operands + 823,
    NULL
  },
  /* 2046 */
  { PrefixDD, 2, { 0xdd, 0xc3, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFfree,
    1, g_Operands + 824,
    NULL
  },
  /* 2047 */
  { PrefixDD, 2, { 0xdd, 0xc4, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFfree,
    1, g_Operands + 825,
    NULL
  },
  /* 2048 */
  { PrefixDD, 2, { 0xdd, 0xc5, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFfree,
    1, g_Operands + 826,
    NULL
  },
  /* 2049 */
  { PrefixDD, 2, { 0xdd, 0xc6, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFfree,
    1, g_Operands + 827,
    NULL
  },
  /* 2050 */
  { PrefixDD, 2, { 0xdd, 0xc7, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFfree,
    1, g_Operands + 828,
    NULL
  },
  /* 2051 */
  { PrefixDD, 2, { 0xdd, 0xc8, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2052 */
  { PrefixDD, 2, { 0xdd, 0xc9, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2053 */
  { PrefixDD, 2, { 0xdd, 0xca, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2054 */
  { PrefixDD, 2, { 0xdd, 0xcb, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2055 */
  { PrefixDD, 2, { 0xdd, 0xcc, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2056 */
  { PrefixDD, 2, { 0xdd, 0xcd, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2057 */
  { PrefixDD, 2, { 0xdd, 0xce, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2058 */
  { PrefixDD, 2, { 0xdd, 0xcf, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2059 */
  { PrefixDD, 2, { 0xdd, 0xd0, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFst,
    2, g_Operands + 773,
    NULL
  },
  /* 2060 */
  { PrefixDD, 2, { 0xdd, 0xd1, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFst,
    2, g_Operands + 829,
    NULL
  },
  /* 2061 */
  { PrefixDD, 2, { 0xdd, 0xd2, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFst,
    2, g_Operands + 831,
    NULL
  },
  /* 2062 */
  { PrefixDD, 2, { 0xdd, 0xd3, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFst,
    2, g_Operands + 833,
    NULL
  },
  /* 2063 */
  { PrefixDD, 2, { 0xdd, 0xd4, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFst,
    2, g_Operands + 835,
    NULL
  },
  /* 2064 */
  { PrefixDD, 2, { 0xdd, 0xd5, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFst,
    2, g_Operands + 837,
    NULL
  },
  /* 2065 */
  { PrefixDD, 2, { 0xdd, 0xd6, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFst,
    2, g_Operands + 839,
    NULL
  },
  /* 2066 */
  { PrefixDD, 2, { 0xdd, 0xd7, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFst,
    2, g_Operands + 841,
    NULL
  },
  /* 2067 */
  { PrefixDD, 2, { 0xdd, 0xd8, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFstp,
    2, g_Operands + 773,
    NULL
  },
  /* 2068 */
  { PrefixDD, 2, { 0xdd, 0xd9, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFstp,
    2, g_Operands + 829,
    NULL
  },
  /* 2069 */
  { PrefixDD, 2, { 0xdd, 0xda, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFstp,
    2, g_Operands + 831,
    NULL
  },
  /* 2070 */
  { PrefixDD, 2, { 0xdd, 0xdb, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFstp,
    2, g_Operands + 833,
    NULL
  },
  /* 2071 */
  { PrefixDD, 2, { 0xdd, 0xdc, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFstp,
    2, g_Operands + 835,
    NULL
  },
  /* 2072 */
  { PrefixDD, 2, { 0xdd, 0xdd, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFstp,
    2, g_Operands + 837,
    NULL
  },
  /* 2073 */
  { PrefixDD, 2, { 0xdd, 0xde, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFstp,
    2, g_Operands + 839,
    NULL
  },
  /* 2074 */
  { PrefixDD, 2, { 0xdd, 0xdf, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFstp,
    2, g_Operands + 843,
    NULL
  },
  /* 2075 */
  { PrefixDD, 2, { 0xdd, 0xe0, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucom,
    2, g_Operands + 755,
    NULL
  },
  /* 2076 */
  { PrefixDD, 2, { 0xdd, 0xe1, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucom,
    2, g_Operands + 757,
    NULL
  },
  /* 2077 */
  { PrefixDD, 2, { 0xdd, 0xe2, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucom,
    2, g_Operands + 759,
    NULL
  },
  /* 2078 */
  { PrefixDD, 2, { 0xdd, 0xe3, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucom,
    2, g_Operands + 761,
    NULL
  },
  /* 2079 */
  { PrefixDD, 2, { 0xdd, 0xe4, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucom,
    2, g_Operands + 763,
    NULL
  },
  /* 2080 */
  { PrefixDD, 2, { 0xdd, 0xe5, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucom,
    2, g_Operands + 765,
    NULL
  },
  /* 2081 */
  { PrefixDD, 2, { 0xdd, 0xe6, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucom,
    2, g_Operands + 767,
    NULL
  },
  /* 2082 */
  { PrefixDD, 2, { 0xdd, 0xe7, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucom,
    2, g_Operands + 769,
    NULL
  },
  /* 2083 */
  { PrefixDD, 2, { 0xdd, 0xe8, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomp,
    2, g_Operands + 755,
    NULL
  },
  /* 2084 */
  { PrefixDD, 2, { 0xdd, 0xe9, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomp,
    2, g_Operands + 757,
    NULL
  },
  /* 2085 */
  { PrefixDD, 2, { 0xdd, 0xea, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomp,
    2, g_Operands + 759,
    NULL
  },
  /* 2086 */
  { PrefixDD, 2, { 0xdd, 0xeb, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomp,
    2, g_Operands + 761,
    NULL
  },
  /* 2087 */
  { PrefixDD, 2, { 0xdd, 0xec, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomp,
    2, g_Operands + 763,
    NULL
  },
  /* 2088 */
  { PrefixDD, 2, { 0xdd, 0xed, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomp,
    2, g_Operands + 765,
    NULL
  },
  /* 2089 */
  { PrefixDD, 2, { 0xdd, 0xee, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomp,
    2, g_Operands + 767,
    NULL
  },
  /* 2090 */
  { PrefixDD, 2, { 0xdd, 0xef, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomp,
    2, g_Operands + 769,
    NULL
  },
  /* 2091 */
  { PrefixDD, 2, { 0xdd, 0xf0, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2092 */
  { PrefixDD, 2, { 0xdd, 0xf1, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2093 */
  { PrefixDD, 2, { 0xdd, 0xf2, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2094 */
  { PrefixDD, 2, { 0xdd, 0xf3, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2095 */
  { PrefixDD, 2, { 0xdd, 0xf4, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2096 */
  { PrefixDD, 2, { 0xdd, 0xf5, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2097 */
  { PrefixDD, 2, { 0xdd, 0xf6, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2098 */
  { PrefixDD, 2, { 0xdd, 0xf7, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2099 */
  { PrefixDD, 2, { 0xdd, 0xf8, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2100 */
  { PrefixDD, 2, { 0xdd, 0xf9, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2101 */
  { PrefixDD, 2, { 0xdd, 0xfa, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2102 */
  { PrefixDD, 2, { 0xdd, 0xfb, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2103 */
  { PrefixDD, 2, { 0xdd, 0xfc, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2104 */
  { PrefixDD, 2, { 0xdd, 0xfd, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2105 */
  { PrefixDD, 2, { 0xdd, 0xfe, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2106 */
  { PrefixDD, 2, { 0xdd, 0xff, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2107 */
  { PrefixDE, 2, { 0xde, 0xc0, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFaddp,
    2, g_Operands + 737,
    NULL
  },
  /* 2108 */
  { PrefixDE, 2, { 0xde, 0xc1, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFaddp,
    2, g_Operands + 805,
    NULL
  },
  /* 2109 */
  { PrefixDE, 2, { 0xde, 0xc2, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFaddp,
    2, g_Operands + 807,
    NULL
  },
  /* 2110 */
  { PrefixDE, 2, { 0xde, 0xc3, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFaddp,
    2, g_Operands + 809,
    NULL
  },
  /* 2111 */
  { PrefixDE, 2, { 0xde, 0xc4, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFaddp,
    2, g_Operands + 811,
    NULL
  },
  /* 2112 */
  { PrefixDE, 2, { 0xde, 0xc5, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFaddp,
    2, g_Operands + 813,
    NULL
  },
  /* 2113 */
  { PrefixDE, 2, { 0xde, 0xc6, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFaddp,
    2, g_Operands + 815,
    NULL
  },
  /* 2114 */
  { PrefixDE, 2, { 0xde, 0xc7, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFaddp,
    2, g_Operands + 817,
    NULL
  },
  /* 2115 */
  { PrefixDE, 2, { 0xde, 0xc8, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmulp,
    2, g_Operands + 737,
    NULL
  },
  /* 2116 */
  { PrefixDE, 2, { 0xde, 0xc9, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmulp,
    2, g_Operands + 805,
    NULL
  },
  /* 2117 */
  { PrefixDE, 2, { 0xde, 0xca, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmulp,
    2, g_Operands + 807,
    NULL
  },
  /* 2118 */
  { PrefixDE, 2, { 0xde, 0xcb, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmulp,
    2, g_Operands + 809,
    NULL
  },
  /* 2119 */
  { PrefixDE, 2, { 0xde, 0xcc, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmulp,
    2, g_Operands + 811,
    NULL
  },
  /* 2120 */
  { PrefixDE, 2, { 0xde, 0xcd, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmulp,
    2, g_Operands + 813,
    NULL
  },
  /* 2121 */
  { PrefixDE, 2, { 0xde, 0xce, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmulp,
    2, g_Operands + 815,
    NULL
  },
  /* 2122 */
  { PrefixDE, 2, { 0xde, 0xcf, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmulp,
    2, g_Operands + 817,
    NULL
  },
  /* 2123 */
  { PrefixDE, 2, { 0xde, 0xd0, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2124 */
  { PrefixDE, 2, { 0xde, 0xd1, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2125 */
  { PrefixDE, 2, { 0xde, 0xd2, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2126 */
  { PrefixDE, 2, { 0xde, 0xd3, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2127 */
  { PrefixDE, 2, { 0xde, 0xd4, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2128 */
  { PrefixDE, 2, { 0xde, 0xd5, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2129 */
  { PrefixDE, 2, { 0xde, 0xd6, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2130 */
  { PrefixDE, 2, { 0xde, 0xd7, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2131 */
  { PrefixDE, 2, { 0xde, 0xd8, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2132 */
  { PrefixDE, 2, { 0xde, 0xd9, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcompp,
    2, g_Operands + 757,
    NULL
  },
  /* 2133 */
  { PrefixDE, 2, { 0xde, 0xda, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2134 */
  { PrefixDE, 2, { 0xde, 0xdb, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2135 */
  { PrefixDE, 2, { 0xde, 0xdc, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2136 */
  { PrefixDE, 2, { 0xde, 0xdd, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2137 */
  { PrefixDE, 2, { 0xde, 0xde, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2138 */
  { PrefixDE, 2, { 0xde, 0xdf, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2139 */
  { PrefixDE, 2, { 0xde, 0xe0, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubrp,
    2, g_Operands + 737,
    NULL
  },
  /* 2140 */
  { PrefixDE, 2, { 0xde, 0xe1, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubrp,
    2, g_Operands + 805,
    NULL
  },
  /* 2141 */
  { PrefixDE, 2, { 0xde, 0xe2, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubrp,
    2, g_Operands + 807,
    NULL
  },
  /* 2142 */
  { PrefixDE, 2, { 0xde, 0xe3, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubrp,
    2, g_Operands + 809,
    NULL
  },
  /* 2143 */
  { PrefixDE, 2, { 0xde, 0xe4, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubrp,
    2, g_Operands + 811,
    NULL
  },
  /* 2144 */
  { PrefixDE, 2, { 0xde, 0xe5, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubrp,
    2, g_Operands + 813,
    NULL
  },
  /* 2145 */
  { PrefixDE, 2, { 0xde, 0xe6, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubrp,
    2, g_Operands + 815,
    NULL
  },
  /* 2146 */
  { PrefixDE, 2, { 0xde, 0xe7, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubrp,
    2, g_Operands + 817,
    NULL
  },
  /* 2147 */
  { PrefixDE, 2, { 0xde, 0xe8, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubp,
    2, g_Operands + 737,
    NULL
  },
  /* 2148 */
  { PrefixDE, 2, { 0xde, 0xe9, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubp,
    2, g_Operands + 805,
    NULL
  },
  /* 2149 */
  { PrefixDE, 2, { 0xde, 0xea, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubp,
    2, g_Operands + 807,
    NULL
  },
  /* 2150 */
  { PrefixDE, 2, { 0xde, 0xeb, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubp,
    2, g_Operands + 809,
    NULL
  },
  /* 2151 */
  { PrefixDE, 2, { 0xde, 0xec, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubp,
    2, g_Operands + 811,
    NULL
  },
  /* 2152 */
  { PrefixDE, 2, { 0xde, 0xed, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubp,
    2, g_Operands + 813,
    NULL
  },
  /* 2153 */
  { PrefixDE, 2, { 0xde, 0xee, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubp,
    2, g_Operands + 815,
    NULL
  },
  /* 2154 */
  { PrefixDE, 2, { 0xde, 0xef, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubp,
    2, g_Operands + 817,
    NULL
  },
  /* 2155 */
  { PrefixDE, 2, { 0xde, 0xf0, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivrp,
    2, g_Operands + 737,
    NULL
  },
  /* 2156 */
  { PrefixDE, 2, { 0xde, 0xf1, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivrp,
    2, g_Operands + 805,
    NULL
  },
  /* 2157 */
  { PrefixDE, 2, { 0xde, 0xf2, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivrp,
    2, g_Operands + 807,
    NULL
  },
  /* 2158 */
  { PrefixDE, 2, { 0xde, 0xf3, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivrp,
    2, g_Operands + 809,
    NULL
  },
  /* 2159 */
  { PrefixDE, 2, { 0xde, 0xf4, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivrp,
    2, g_Operands + 811,
    NULL
  },
  /* 2160 */
  { PrefixDE, 2, { 0xde, 0xf5, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivrp,
    2, g_Operands + 813,
    NULL
  },
  /* 2161 */
  { PrefixDE, 2, { 0xde, 0xf6, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivrp,
    2, g_Operands + 815,
    NULL
  },
  /* 2162 */
  { PrefixDE, 2, { 0xde, 0xf7, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivrp,
    2, g_Operands + 817,
    NULL
  },
  /* 2163 */
  { PrefixDE, 2, { 0xde, 0xf8, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivp,
    2, g_Operands + 737,
    NULL
  },
  /* 2164 */
  { PrefixDE, 2, { 0xde, 0xf9, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivp,
    2, g_Operands + 805,
    NULL
  },
  /* 2165 */
  { PrefixDE, 2, { 0xde, 0xfa, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivp,
    2, g_Operands + 807,
    NULL
  },
  /* 2166 */
  { PrefixDE, 2, { 0xde, 0xfb, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivp,
    2, g_Operands + 809,
    NULL
  },
  /* 2167 */
  { PrefixDE, 2, { 0xde, 0xfc, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivp,
    2, g_Operands + 811,
    NULL
  },
  /* 2168 */
  { PrefixDE, 2, { 0xde, 0xfd, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivp,
    2, g_Operands + 813,
    NULL
  },
  /* 2169 */
  { PrefixDE, 2, { 0xde, 0xfe, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivp,
    2, g_Operands + 815,
    NULL
  },
  /* 2170 */
  { PrefixDE, 2, { 0xde, 0xff, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivp,
    2, g_Operands + 817,
    NULL
  },
  /* 2171 */
  { PrefixDF, 2, { 0xdf, 0xc0, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2172 */
  { PrefixDF, 2, { 0xdf, 0xc1, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2173 */
  { PrefixDF, 2, { 0xdf, 0xc2, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2174 */
  { PrefixDF, 2, { 0xdf, 0xc3, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2175 */
  { PrefixDF, 2, { 0xdf, 0xc4, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2176 */
  { PrefixDF, 2, { 0xdf, 0xc5, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2177 */
  { PrefixDF, 2, { 0xdf, 0xc6, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2178 */
  { PrefixDF, 2, { 0xdf, 0xc7, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2179 */
  { PrefixDF, 2, { 0xdf, 0xc8, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2180 */
  { PrefixDF, 2, { 0xdf, 0xc9, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2181 */
  { PrefixDF, 2, { 0xdf, 0xca, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2182 */
  { PrefixDF, 2, { 0xdf, 0xcb, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2183 */
  { PrefixDF, 2, { 0xdf, 0xcc, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2184 */
  { PrefixDF, 2, { 0xdf, 0xcd, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2185 */
  { PrefixDF, 2, { 0xdf, 0xce, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2186 */
  { PrefixDF, 2, { 0xdf, 0xcf, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2187 */
  { PrefixDF, 2, { 0xdf, 0xd0, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2188 */
  { PrefixDF, 2, { 0xdf, 0xd1, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2189 */
  { PrefixDF, 2, { 0xdf, 0xd2, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2190 */
  { PrefixDF, 2, { 0xdf, 0xd3, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2191 */
  { PrefixDF, 2, { 0xdf, 0xd4, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2192 */
  { PrefixDF, 2, { 0xdf, 0xd5, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2193 */
  { PrefixDF, 2, { 0xdf, 0xd6, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2194 */
  { PrefixDF, 2, { 0xdf, 0xd7, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2195 */
  { PrefixDF, 2, { 0xdf, 0xd8, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2196 */
  { PrefixDF, 2, { 0xdf, 0xd9, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2197 */
  { PrefixDF, 2, { 0xdf, 0xda, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2198 */
  { PrefixDF, 2, { 0xdf, 0xdb, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2199 */
  { PrefixDF, 2, { 0xdf, 0xdc, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2200 */
  { PrefixDF, 2, { 0xdf, 0xdd, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2201 */
  { PrefixDF, 2, { 0xdf, 0xde, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2202 */
  { PrefixDF, 2, { 0xdf, 0xdf, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2203 */
  { PrefixDF, 2, { 0xdf, 0xe0, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFnstsw,
    1, g_Operands + 845,
    NULL
  },
  /* 2204 */
  { PrefixDF, 2, { 0xdf, 0xe1, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2205 */
  { PrefixDF, 2, { 0xdf, 0xe2, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2206 */
  { PrefixDF, 2, { 0xdf, 0xe3, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2207 */
  { PrefixDF, 2, { 0xdf, 0xe4, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2208 */
  { PrefixDF, 2, { 0xdf, 0xe5, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2209 */
  { PrefixDF, 2, { 0xdf, 0xe6, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2210 */
  { PrefixDF, 2, { 0xdf, 0xe7, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2211 */
  { PrefixDF, 2, { 0xdf, 0xe8, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomip,
    2, g_Operands + 755,
    NULL
  },
  /* 2212 */
  { PrefixDF, 2, { 0xdf, 0xe9, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomip,
    2, g_Operands + 757,
    NULL
  },
  /* 2213 */
  { PrefixDF, 2, { 0xdf, 0xea, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomip,
    2, g_Operands + 759,
    NULL
  },
  /* 2214 */
  { PrefixDF, 2, { 0xdf, 0xeb, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomip,
    2, g_Operands + 761,
    NULL
  },
  /* 2215 */
  { PrefixDF, 2, { 0xdf, 0xec, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomip,
    2, g_Operands + 763,
    NULL
  },
  /* 2216 */
  { PrefixDF, 2, { 0xdf, 0xed, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomip,
    2, g_Operands + 765,
    NULL
  },
  /* 2217 */
  { PrefixDF, 2, { 0xdf, 0xee, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomip,
    2, g_Operands + 767,
    NULL
  },
  /* 2218 */
  { PrefixDF, 2, { 0xdf, 0xef, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomip,
    2, g_Operands + 769,
    NULL
  },
  /* 2219 */
  { PrefixDF, 2, { 0xdf, 0xf0, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomip,
    2, g_Operands + 755,
    NULL
  },
  /* 2220 */
  { PrefixDF, 2, { 0xdf, 0xf1, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomip,
    2, g_Operands + 757,
    NULL
  },
  /* 2221 */
  { PrefixDF, 2, { 0xdf, 0xf2, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomip,
    2, g_Operands + 759,
    NULL
  },
  /* 2222 */
  { PrefixDF, 2, { 0xdf, 0xf3, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomip,
    2, g_Operands + 761,
    NULL
  },
  /* 2223 */
  { PrefixDF, 2, { 0xdf, 0xf4, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomip,
    2, g_Operands + 763,
    NULL
  },
  /* 2224 */
  { PrefixDF, 2, { 0xdf, 0xf5, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomip,
    2, g_Operands + 765,
    NULL
  },
  /* 2225 */
  { PrefixDF, 2, { 0xdf, 0xf6, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomip,
    2, g_Operands + 767,
    NULL
  },
  /* 2226 */
  { PrefixDF, 2, { 0xdf, 0xf7, 0x00, 0x00 }, NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomip,
    2, g_Operands + 769,
    NULL
  },
  /* 2227 */
  { PrefixDF, 2, { 0xdf, 0xf8, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2228 */
  { PrefixDF, 2, { 0xdf, 0xf9, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2229 */
  { PrefixDF, 2, { 0xdf, 0xfa, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2230 */
  { PrefixDF, 2, { 0xdf, 0xfb, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2231 */
  { PrefixDF, 2, { 0xdf, 0xfc, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2232 */
  { PrefixDF, 2, { 0xdf, 0xfd, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2233 */
  { PrefixDF, 2, { 0xdf, 0xfe, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2234 */
  { PrefixDF, 2, { 0xdf, 0xff, 0x00, 0x00 }, NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid,
    0, g_Operands + 0,
    NULL
  },
  /* 2235 */
  { NoPrefix, 1, { 0x0b, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstUd2,
    0, g_Operands + 0,
    NULL
  },
  /* 2236 */
  { NoPrefix, 1, { 0x1f, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstNop,
    0, g_Operands + 0,
    NULL
  },
  /* 2237 */
  { NoPrefix, 1, { 0x1f, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstNop,
    0, g_Operands + 0,
    NULL
  },
  /* 2238 */
  { NoPrefix, 1, { 0x1f, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstNop,
    0, g_Operands + 0,
    NULL
  },
  /* 2239 */
  { NoPrefix, 1, { 0x1f, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstNop,
    0, g_Operands + 0,
    NULL
  },
  /* 2240 */
  { NoPrefix, 1, { 0x1f, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstNop,
    0, g_Operands + 0,
    NULL
  },
  /* 2241 */
  { NoPrefix, 1, { 0x1f, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstNop,
    0, g_Operands + 0,
    NULL
  },
  /* 2242 */
  { NoPrefix, 1, { 0x1f, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstNop,
    0, g_Operands + 0,
    NULL
  },
  /* 2243 */
  { NoPrefix, 1, { 0x1f, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstNop,
    0, g_Operands + 0,
    NULL
  },
  /* 2244 */
  { NoPrefix, 1, { 0x1f, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstNop,
    0, g_Operands + 0,
    NULL
  },
  /* 2245 */
  { NoPrefix, 1, { 0x1f, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstNop,
    0, g_Operands + 0,
    NULL
  },
  /* 2246 */
  { NoPrefix, 1, { 0x1f, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstNop,
    0, g_Operands + 0,
    NULL
  },
  /* 2247 */
  { NoPrefix, 1, { 0x1f, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstNop,
    0, g_Operands + 0,
    NULL
  },
  /* 2248 */
  { NoPrefix, 1, { 0x1f, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstNop,
    0, g_Operands + 0,
    NULL
  },
  /* 2249 */
  { NoPrefix, 1, { 0x90, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstNop,
    0, g_Operands + 0,
    NULL
  },
  /* 2250 */
  { NoPrefix, 1, { 0x90, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstNop,
    0, g_Operands + 0,
    NULL
  },
  /* 2251 */
  { NoPrefix, 1, { 0x90, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstNop,
    0, g_Operands + 0,
    NULL
  },
  /* 2252 */
  { NoPrefix, 1, { 0x90, 0x00, 0x00, 0x00 }, NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstPause,
    0, g_Operands + 0,
    NULL
  },
};

static const NaClInst g_Undefined_Opcode = 
  /* 0 */
  { NoPrefix, 1, { 0x00, 0x00, 0x00, 0x00 }, NACLi_INVALID,
    NACL_EMPTY_IFLAGS,
    InstInvalid,
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
