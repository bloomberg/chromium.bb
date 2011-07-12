/*
 * THIS FILE IS AUTO-GENERATED. DO NOT EDIT.
 * Compiled for x86-32 bit mode.
 *
 * You must include ncopcode_desc.h before this file.
 */

static const NaClOp g_Operands[724] = {
  /* 0 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Eb" },
  /* 1 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gb" },
  /* 2 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Ev" },
  /* 3 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gv" },
  /* 4 */ { G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Gb" },
  /* 5 */ { E_Operand, NACL_OPFLAG(OpUse), "$Eb" },
  /* 6 */ { G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Gv" },
  /* 7 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ev" },
  /* 8 */ { RegAL, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%al" },
  /* 9 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 10 */ { RegREAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$rAXv" },
  /* 11 */ { I_Operand, NACL_OPFLAG(OpUse), "$Iz" },
  /* 12 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 13 */ { RegES, NACL_OPFLAG(OpUse), "%es" },
  /* 14 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 15 */ { RegES, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%es" },
  /* 16 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 17 */ { RegCS, NACL_OPFLAG(OpUse), "%cs" },
  /* 18 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 19 */ { RegSS, NACL_OPFLAG(OpUse), "%ss" },
  /* 20 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 21 */ { RegSS, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%ss" },
  /* 22 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 23 */ { RegDS, NACL_OPFLAG(OpUse), "%ds" },
  /* 24 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 25 */ { RegDS, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%ds" },
  /* 26 */ { RegAL, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%al}" },
  /* 27 */ { E_Operand, NACL_OPFLAG(OpUse), "$Eb" },
  /* 28 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gb" },
  /* 29 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ev" },
  /* 30 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gv" },
  /* 31 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gb" },
  /* 32 */ { E_Operand, NACL_OPFLAG(OpUse), "$Eb" },
  /* 33 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gv" },
  /* 34 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ev" },
  /* 35 */ { RegAL, NACL_OPFLAG(OpUse), "%al" },
  /* 36 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 37 */ { RegREAX, NACL_OPFLAG(OpUse), "$rAXv" },
  /* 38 */ { I_Operand, NACL_OPFLAG(OpUse), "$Iz" },
  /* 39 */ { RegRECX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$rCXv" },
  /* 40 */ { RegREDX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$rDXv" },
  /* 41 */ { RegREBX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$rBXv" },
  /* 42 */ { RegRESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$rSPv" },
  /* 43 */ { RegREBP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$rBPv" },
  /* 44 */ { RegRESI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$rSIv" },
  /* 45 */ { RegREDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$rDIv" },
  /* 46 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 47 */ { G_OpcodeBase, NACL_OPFLAG(OpUse), "$r8v" },
  /* 48 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 49 */ { G_OpcodeBase, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$r8v" },
  /* 50 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 51 */ { RegGP7, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%gp7}" },
  /* 52 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 53 */ { RegGP7, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%gp7}" },
  /* 54 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gv" },
  /* 55 */ { M_Operand, NACL_OPFLAG(OpUse), "$Ma" },
  /* 56 */ { M_Operand, NACL_OPFLAG(OpUse), NULL },
  /* 57 */ { Ew_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Ew" },
  /* 58 */ { Gw_Operand, NACL_OPFLAG(OpUse), "$Gw" },
  /* 59 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 60 */ { I_Operand, NACL_OPFLAG(OpUse), "$Iz" },
  /* 61 */ { G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Gv" },
  /* 62 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ev" },
  /* 63 */ { I_Operand, NACL_OPFLAG(OpUse), "$Iz" },
  /* 64 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 65 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 66 */ { G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Gv" },
  /* 67 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ev" },
  /* 68 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 69 */ { RegES_EDI, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$Yb}" },
  /* 70 */ { RegDX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%dx}" },
  /* 71 */ { RegES_EDI, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$Yzw}" },
  /* 72 */ { RegDX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%dx}" },
  /* 73 */ { RegES_EDI, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$Yzd}" },
  /* 74 */ { RegDX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%dx}" },
  /* 75 */ { RegDX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%dx}" },
  /* 76 */ { RegDS_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Xb}" },
  /* 77 */ { RegDX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%dx}" },
  /* 78 */ { RegES_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Xzw}" },
  /* 79 */ { RegDX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%dx}" },
  /* 80 */ { RegES_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Xzd}" },
  /* 81 */ { RegEIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eip}" },
  /* 82 */ { J_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandNear) | NACL_OPFLAG(OperandRelative), "$Jb" },
  /* 83 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Eb" },
  /* 84 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 85 */ { E_Operand, NACL_OPFLAG(OpUse), "$Eb" },
  /* 86 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 87 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Ev" },
  /* 88 */ { I_Operand, NACL_OPFLAG(OpUse), "$Iz" },
  /* 89 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Ev" },
  /* 90 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 91 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Eb" },
  /* 92 */ { G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Gb" },
  /* 93 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Ev" },
  /* 94 */ { G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Gv" },
  /* 95 */ { E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Eb" },
  /* 96 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gb" },
  /* 97 */ { E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Ev" },
  /* 98 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gv" },
  /* 99 */ { G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Gb" },
  /* 100 */ { E_Operand, NACL_OPFLAG(OpUse), "$Eb" },
  /* 101 */ { E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Mw/Rv" },
  /* 102 */ { S_Operand, NACL_OPFLAG(OpUse), "$Sw" },
  /* 103 */ { G_Operand, NACL_OPFLAG(OpSet), "$Gv" },
  /* 104 */ { M_Operand, NACL_OPFLAG(OpAddress), "$M" },
  /* 105 */ { S_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Sw" },
  /* 106 */ { Ew_Operand, NACL_OPFLAG(OpUse), "$Ew" },
  /* 107 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 108 */ { E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Ev" },
  /* 109 */ { G_OpcodeBase, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$r8v" },
  /* 110 */ { RegREAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$rAXv" },
  /* 111 */ { RegAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%ax}" },
  /* 112 */ { RegAL, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%al}" },
  /* 113 */ { RegEAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit) | NACL_OPFLAG(OperandSignExtends_v), "{%eax}" },
  /* 114 */ { RegAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%ax}" },
  /* 115 */ { RegDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%dx}" },
  /* 116 */ { RegAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%ax}" },
  /* 117 */ { RegEDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%edx}" },
  /* 118 */ { RegEAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%eax}" },
  /* 119 */ { RegEIP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eip}" },
  /* 120 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 121 */ { J_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandFar) | NACL_OPFLAG(OperandRelative), "$Ad" },
  /* 122 */ { RegEIP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eip}" },
  /* 123 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 124 */ { J_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandFar) | NACL_OPFLAG(OperandRelative), "$Ap" },
  /* 125 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 126 */ { RegRFLAGS, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Fvw}" },
  /* 127 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 128 */ { RegRFLAGS, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Fvd}" },
  /* 129 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 130 */ { RegRFLAGS, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$Fvw}" },
  /* 131 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 132 */ { RegRFLAGS, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$Fvd}" },
  /* 133 */ { RegAH, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%ah}" },
  /* 134 */ { RegAH, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%ah}" },
  /* 135 */ { RegAL, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%al" },
  /* 136 */ { O_Operand, NACL_OPFLAG(OpUse), "$Ob" },
  /* 137 */ { RegREAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$rAXv" },
  /* 138 */ { O_Operand, NACL_OPFLAG(OpUse), "$Ov" },
  /* 139 */ { O_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Ob" },
  /* 140 */ { RegAL, NACL_OPFLAG(OpUse), "%al" },
  /* 141 */ { O_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Ov" },
  /* 142 */ { RegREAX, NACL_OPFLAG(OpUse), "$rAXv" },
  /* 143 */ { RegES_EDI, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$Yb}" },
  /* 144 */ { RegDS_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Xb}" },
  /* 145 */ { RegES_EDI, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$Yvw}" },
  /* 146 */ { RegDS_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Xvw}" },
  /* 147 */ { RegES_EDI, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$Yvd}" },
  /* 148 */ { RegDS_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Xvd}" },
  /* 149 */ { RegDS_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Xb}" },
  /* 150 */ { RegES_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Yb}" },
  /* 151 */ { RegDS_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Xvw}" },
  /* 152 */ { RegES_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Yvw}" },
  /* 153 */ { RegDS_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Xvd}" },
  /* 154 */ { RegES_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Yvd}" },
  /* 155 */ { RegES_EDI, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$Yb}" },
  /* 156 */ { RegAL, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%al}" },
  /* 157 */ { RegES_EDI, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$Yvw}" },
  /* 158 */ { RegAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$rAXvw}" },
  /* 159 */ { RegES_EDI, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$Yvd}" },
  /* 160 */ { RegEAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$rAXvd}" },
  /* 161 */ { RegAL, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%al}" },
  /* 162 */ { RegDS_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Xb}" },
  /* 163 */ { RegAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$rAXvw}" },
  /* 164 */ { RegDS_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Xvw}" },
  /* 165 */ { RegEAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$rAXvd}" },
  /* 166 */ { RegDS_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Xvd}" },
  /* 167 */ { RegAL, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%al}" },
  /* 168 */ { RegES_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Yb}" },
  /* 169 */ { RegAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$rAXvw}" },
  /* 170 */ { RegES_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Yvw}" },
  /* 171 */ { RegEAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$rAXvd}" },
  /* 172 */ { RegES_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Yvd}" },
  /* 173 */ { G_OpcodeBase, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$r8b" },
  /* 174 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 175 */ { G_OpcodeBase, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$r8v" },
  /* 176 */ { I_Operand, NACL_OPFLAG(OpUse), "$Iv" },
  /* 177 */ { RegEIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eip}" },
  /* 178 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 179 */ { I_Operand, NACL_OPFLAG(OpUse), "$Iw" },
  /* 180 */ { Seg_G_Operand, NACL_OPFLAG(OpSet), "$SGz" },
  /* 181 */ { M_Operand, NACL_OPFLAG(OperandFar), "$Mp" },
  /* 182 */ { E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Eb" },
  /* 183 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 184 */ { E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Ev" },
  /* 185 */ { I_Operand, NACL_OPFLAG(OpUse), "$Iz" },
  /* 186 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 187 */ { RegEBP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%ebp}" },
  /* 188 */ { I_Operand, NACL_OPFLAG(OpUse), "$Iw" },
  /* 189 */ { I2_Operand, NACL_OPFLAG(OpUse), "$I2b" },
  /* 190 */ { RegESP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 191 */ { RegEBP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%ebp}" },
  /* 192 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Eb" },
  /* 193 */ { Const_1, NACL_OPFLAG(OpUse), "1" },
  /* 194 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Ev" },
  /* 195 */ { Const_1, NACL_OPFLAG(OpUse), "1" },
  /* 196 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Eb" },
  /* 197 */ { RegCL, NACL_OPFLAG(OpUse), "%cl" },
  /* 198 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Ev" },
  /* 199 */ { RegCL, NACL_OPFLAG(OpUse), "%cl" },
  /* 200 */ { RegAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%ax}" },
  /* 201 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 202 */ { RegAL, NACL_OPFLAG(OpImplicit), "{%al}" },
  /* 203 */ { RegAL, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%al}" },
  /* 204 */ { RegDS_EBX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%DS_EBX}" },
  /* 205 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 206 */ { Mv_Operand, NACL_OPFLAG(OpUse), "$Md" },
  /* 207 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 208 */ { Mv_Operand, NACL_OPFLAG(OpUse), "$Md" },
  /* 209 */ { RegST0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 210 */ { Mv_Operand, NACL_OPFLAG(OpUse), "$Md" },
  /* 211 */ { Mv_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Md" },
  /* 212 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 213 */ { M_Operand, NACL_OPFLAG(OpUse), "$M" },
  /* 214 */ { Mw_Operand, NACL_OPFLAG(OpUse), "$Mw" },
  /* 215 */ { M_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$M" },
  /* 216 */ { Mw_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Mw" },
  /* 217 */ { RegST0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 218 */ { M_Operand, NACL_OPFLAG(OpUse), "$M" },
  /* 219 */ { M_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$M" },
  /* 220 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 221 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 222 */ { Mo_Operand, NACL_OPFLAG(OpUse), "$Mq" },
  /* 223 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 224 */ { Mo_Operand, NACL_OPFLAG(OpUse), "$Mq" },
  /* 225 */ { RegST0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 226 */ { Mo_Operand, NACL_OPFLAG(OpUse), "$Mq" },
  /* 227 */ { Mo_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Mq" },
  /* 228 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 229 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 230 */ { Mw_Operand, NACL_OPFLAG(OpUse), "$Mw" },
  /* 231 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 232 */ { Mw_Operand, NACL_OPFLAG(OpUse), "$Mw" },
  /* 233 */ { RegST0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 234 */ { Mw_Operand, NACL_OPFLAG(OpUse), "$Mw" },
  /* 235 */ { Mw_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Mw" },
  /* 236 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 237 */ { RegEIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eip}" },
  /* 238 */ { RegCX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%cx}" },
  /* 239 */ { J_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandNear) | NACL_OPFLAG(OperandRelative), "$Jb" },
  /* 240 */ { RegEIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eip}" },
  /* 241 */ { RegECX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%ecx}" },
  /* 242 */ { J_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandNear) | NACL_OPFLAG(OperandRelative), "$Jb" },
  /* 243 */ { RegAL, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%al" },
  /* 244 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 245 */ { RegREAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$rAXv" },
  /* 246 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 247 */ { I_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Ib" },
  /* 248 */ { RegAL, NACL_OPFLAG(OpUse), "%al" },
  /* 249 */ { I_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Ib" },
  /* 250 */ { RegREAX, NACL_OPFLAG(OpUse), "$rAXv" },
  /* 251 */ { RegEIP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eip}" },
  /* 252 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 253 */ { J_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandNear) | NACL_OPFLAG(OperandRelative), "$Jz" },
  /* 254 */ { RegEIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eip}" },
  /* 255 */ { J_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandNear) | NACL_OPFLAG(OperandRelative), "$Jz" },
  /* 256 */ { RegEIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eip}" },
  /* 257 */ { J_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandFar) | NACL_OPFLAG(OperandRelative), "$Ap" },
  /* 258 */ { RegAL, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%al" },
  /* 259 */ { RegDX, NACL_OPFLAG(OpUse), "%dx" },
  /* 260 */ { RegREAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$rAXv" },
  /* 261 */ { RegDX, NACL_OPFLAG(OpUse), "%dx" },
  /* 262 */ { RegDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%dx" },
  /* 263 */ { RegAL, NACL_OPFLAG(OpUse), "%al" },
  /* 264 */ { RegDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%dx" },
  /* 265 */ { RegREAX, NACL_OPFLAG(OpUse), "$rAXv" },
  /* 266 */ { RegAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%ax}" },
  /* 267 */ { RegAL, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%al}" },
  /* 268 */ { E_Operand, NACL_OPFLAG(OpUse), "$Eb" },
  /* 269 */ { RegREDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%redx}" },
  /* 270 */ { RegREAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%reax}" },
  /* 271 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ev" },
  /* 272 */ { RegEIP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eip}" },
  /* 273 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 274 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandNear), "$Ev" },
  /* 275 */ { RegEIP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eip}" },
  /* 276 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 277 */ { M_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandFar), "$Mp" },
  /* 278 */ { RegEIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eip}" },
  /* 279 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandNear), "$Ev" },
  /* 280 */ { RegEIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eip}" },
  /* 281 */ { M_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandFar), "$Mp" },
  /* 282 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 283 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ev" },
  /* 284 */ { Ew_Operand, NACL_EMPTY_OPFLAGS, "$Ew" },
  /* 285 */ { M_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Ms" },
  /* 286 */ { RegREAX, NACL_OPFLAG(OpUse), "%reax" },
  /* 287 */ { RegECX, NACL_OPFLAG(OpUse), "%ecx" },
  /* 288 */ { RegEDX, NACL_OPFLAG(OpUse), "%edx" },
  /* 289 */ { RegEAX, NACL_EMPTY_OPFLAGS, "%eax" },
  /* 290 */ { RegECX, NACL_EMPTY_OPFLAGS, "%ecx" },
  /* 291 */ { M_Operand, NACL_OPFLAG(OpUse), "$Ms" },
  /* 292 */ { RegREAXa, NACL_OPFLAG(OpUse), "$rAXva" },
  /* 293 */ { RegEIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eip}" },
  /* 294 */ { RegEAX, NACL_OPFLAG(OpUse), "%eax" },
  /* 295 */ { RegREAXa, NACL_OPFLAG(OpUse), "$rAXva" },
  /* 296 */ { RegECX, NACL_OPFLAG(OpUse), "%ecx" },
  /* 297 */ { Mb_Operand, NACL_OPFLAG(OpUse), "$Mb" },
  /* 298 */ { G_Operand, NACL_EMPTY_OPFLAGS, "$Gv" },
  /* 299 */ { Ew_Operand, NACL_EMPTY_OPFLAGS, "$Ew" },
  /* 300 */ { Mb_Operand, NACL_EMPTY_OPFLAGS, "$Mb" },
  /* 301 */ { Mmx_G_Operand, NACL_EMPTY_OPFLAGS, "$Pq" },
  /* 302 */ { Mmx_E_Operand, NACL_EMPTY_OPFLAGS, "$Qq" },
  /* 303 */ { I_Operand, NACL_EMPTY_OPFLAGS, "$Ib" },
  /* 304 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vps" },
  /* 305 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wps" },
  /* 306 */ { Xmm_E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Wps" },
  /* 307 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vps" },
  /* 308 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vps" },
  /* 309 */ { Mo_Operand, NACL_OPFLAG(OpUse), "$Mq" },
  /* 310 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vps" },
  /* 311 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$VRq" },
  /* 312 */ { Mo_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Mq" },
  /* 313 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vps" },
  /* 314 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vps" },
  /* 315 */ { Xmm_Eo_Operand, NACL_OPFLAG(OpUse), "$Wq" },
  /* 316 */ { Ev_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Rd/q" },
  /* 317 */ { C_Operand, NACL_OPFLAG(OpUse), "$Cd/q" },
  /* 318 */ { Ev_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Rd/q" },
  /* 319 */ { D_Operand, NACL_OPFLAG(OpUse), "$Dd/q" },
  /* 320 */ { C_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Cd/q" },
  /* 321 */ { Ev_Operand, NACL_OPFLAG(OpUse), "$Rd/q" },
  /* 322 */ { D_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Dd/q" },
  /* 323 */ { Ev_Operand, NACL_OPFLAG(OpUse), "$Rd/q" },
  /* 324 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vps" },
  /* 325 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse), "$Qq" },
  /* 326 */ { Mdq_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Mdq" },
  /* 327 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vps" },
  /* 328 */ { Mmx_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Pq" },
  /* 329 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wps" },
  /* 330 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vss" },
  /* 331 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wss" },
  /* 332 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vps" },
  /* 333 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wps" },
  /* 334 */ { RegEAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%eax}" },
  /* 335 */ { RegEDX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%edx}" },
  /* 336 */ { RegECX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%ecx}" },
  /* 337 */ { RegEAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eax}" },
  /* 338 */ { RegEDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%edx}" },
  /* 339 */ { RegEAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eax}" },
  /* 340 */ { RegEDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%edx}" },
  /* 341 */ { RegECX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%ecx}" },
  /* 342 */ { RegEIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eip}" },
  /* 343 */ { RegESP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 344 */ { RegCS, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%cs}" },
  /* 345 */ { RegSS, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%ss}" },
  /* 346 */ { RegEIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eip}" },
  /* 347 */ { RegESP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 348 */ { RegCS, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%cs}" },
  /* 349 */ { RegSS, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%ss}" },
  /* 350 */ { RegEDX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%edx}" },
  /* 351 */ { RegECX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%ecx}" },
  /* 352 */ { Gv_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Gd" },
  /* 353 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$VRps" },
  /* 354 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vps" },
  /* 355 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wps" },
  /* 356 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vpd" },
  /* 357 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wps" },
  /* 358 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vps" },
  /* 359 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 360 */ { Mmx_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Pq" },
  /* 361 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse), "$Qq" },
  /* 362 */ { Mmx_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Pq" },
  /* 363 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse), "$Qd" },
  /* 364 */ { Mmx_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Pq" },
  /* 365 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ed/q/d" },
  /* 366 */ { Mmx_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Pq" },
  /* 367 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse), "$Qq" },
  /* 368 */ { Mmx_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Pq" },
  /* 369 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse), "$Qq" },
  /* 370 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 371 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$PRq" },
  /* 372 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 373 */ { E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Ed/q/d" },
  /* 374 */ { Mmx_G_Operand, NACL_OPFLAG(OpUse), "$Pd/q/d" },
  /* 375 */ { Mmx_E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Qq" },
  /* 376 */ { Mmx_G_Operand, NACL_OPFLAG(OpUse), "$Pq" },
  /* 377 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 378 */ { RegFS, NACL_OPFLAG(OpUse), "%fs" },
  /* 379 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 380 */ { RegFS, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%fs" },
  /* 381 */ { RegEBX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%ebx}" },
  /* 382 */ { RegEDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%edx}" },
  /* 383 */ { RegEAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eax}" },
  /* 384 */ { RegECX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%ecx}" },
  /* 385 */ { E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Ev" },
  /* 386 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gv" },
  /* 387 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 388 */ { E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Ev" },
  /* 389 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gv" },
  /* 390 */ { RegCL, NACL_OPFLAG(OpUse), "%cl" },
  /* 391 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 392 */ { RegGS, NACL_OPFLAG(OpUse), "%gs" },
  /* 393 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 394 */ { RegGS, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%gs" },
  /* 395 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Ev" },
  /* 396 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gv" },
  /* 397 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 398 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Ev" },
  /* 399 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gv" },
  /* 400 */ { RegCL, NACL_OPFLAG(OpUse), "%cl" },
  /* 401 */ { RegAL, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%al}" },
  /* 402 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Eb" },
  /* 403 */ { G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Gb" },
  /* 404 */ { RegREAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$rAXv}" },
  /* 405 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Ev" },
  /* 406 */ { G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Gv" },
  /* 407 */ { G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Gv" },
  /* 408 */ { Eb_Operand, NACL_OPFLAG(OpUse), "$Eb" },
  /* 409 */ { G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Gv" },
  /* 410 */ { Ew_Operand, NACL_OPFLAG(OpUse), "$Ew" },
  /* 411 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vps" },
  /* 412 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wps" },
  /* 413 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 414 */ { M_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Md/q" },
  /* 415 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gd/q" },
  /* 416 */ { Mmx_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Pq" },
  /* 417 */ { E_Operand, NACL_OPFLAG(OpUse), "$Rd/q/Mw" },
  /* 418 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 419 */ { Gv_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Gd" },
  /* 420 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse), "$PRq" },
  /* 421 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 422 */ { RegEDX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%edx}" },
  /* 423 */ { RegEAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eax}" },
  /* 424 */ { Mo_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Mq" },
  /* 425 */ { Mo_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Mq" },
  /* 426 */ { Mmx_G_Operand, NACL_OPFLAG(OpUse), "$Pq" },
  /* 427 */ { RegDS_EDI, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$Xvd}" },
  /* 428 */ { Mmx_G_Operand, NACL_OPFLAG(OpUse), "$Pq" },
  /* 429 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse), "$PRq" },
  /* 430 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vsd" },
  /* 431 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wsd" },
  /* 432 */ { Xmm_E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Wsd" },
  /* 433 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vsd" },
  /* 434 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vpd" },
  /* 435 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wsd" },
  /* 436 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vsd" },
  /* 437 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ed/q" },
  /* 438 */ { Mo_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Mq" },
  /* 439 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vsd" },
  /* 440 */ { G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Gd/q" },
  /* 441 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wsd" },
  /* 442 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vsd" },
  /* 443 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wsd" },
  /* 444 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vss" },
  /* 445 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wsd" },
  /* 446 */ { Xmm_Go_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vq" },
  /* 447 */ { Xmm_Eo_Operand, NACL_OPFLAG(OpUse), "$Wq" },
  /* 448 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 449 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 450 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$VRq" },
  /* 451 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 452 */ { I2_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 453 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 454 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$VRdq" },
  /* 455 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vsd" },
  /* 456 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wsd" },
  /* 457 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 458 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vpd" },
  /* 459 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wpd" },
  /* 460 */ { Mmx_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Pq" },
  /* 461 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$VRq" },
  /* 462 */ { Xmm_Go_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vq" },
  /* 463 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wpd" },
  /* 464 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 465 */ { Mdq_Operand, NACL_OPFLAG(OpUse), "$Mdq" },
  /* 466 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vss" },
  /* 467 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wss" },
  /* 468 */ { Xmm_E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Wss" },
  /* 469 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vss" },
  /* 470 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vss" },
  /* 471 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ed/q" },
  /* 472 */ { Mv_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Md" },
  /* 473 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vss" },
  /* 474 */ { G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Gd/q" },
  /* 475 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wss" },
  /* 476 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vss" },
  /* 477 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wss" },
  /* 478 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vsd" },
  /* 479 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wss" },
  /* 480 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 481 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wps" },
  /* 482 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 483 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 484 */ { Xmm_E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Wdq" },
  /* 485 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 486 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vss" },
  /* 487 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wss" },
  /* 488 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 489 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 490 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse), "$PRq" },
  /* 491 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vpd" },
  /* 492 */ { Xmm_Eo_Operand, NACL_OPFLAG(OpUse), "$Wq" },
  /* 493 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vpd" },
  /* 494 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wpd" },
  /* 495 */ { Xmm_E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Wpd" },
  /* 496 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vpd" },
  /* 497 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vsd" },
  /* 498 */ { Mo_Operand, NACL_OPFLAG(OpUse), "$Mq" },
  /* 499 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vpd" },
  /* 500 */ { Xmm_Eo_Operand, NACL_OPFLAG(OpUse), "$Wq" },
  /* 501 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vpd" },
  /* 502 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse), "$Qq" },
  /* 503 */ { Mdq_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Mdq" },
  /* 504 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vpd" },
  /* 505 */ { Mmx_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Pq" },
  /* 506 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wpd" },
  /* 507 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vsd" },
  /* 508 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wsd" },
  /* 509 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vpd" },
  /* 510 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wsd" },
  /* 511 */ { Gv_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Gd" },
  /* 512 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$VRpd" },
  /* 513 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vps" },
  /* 514 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wpd" },
  /* 515 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 516 */ { Xmm_Eo_Operand, NACL_OPFLAG(OpUse), "$Wq" },
  /* 517 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 518 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 519 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 520 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ed/q/d" },
  /* 521 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 522 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 523 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 524 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$VRdq" },
  /* 525 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 526 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(AllowGOperandWithOpcodeInModRm), "$Vdq" },
  /* 527 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 528 */ { I2_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 529 */ { E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Ed/q/d" },
  /* 530 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vd/q/d" },
  /* 531 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vpd" },
  /* 532 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wpd" },
  /* 533 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 534 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 535 */ { E_Operand, NACL_OPFLAG(OpUse), "$Rd/q/Mw" },
  /* 536 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 537 */ { Gv_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Gd" },
  /* 538 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$VRdq" },
  /* 539 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 540 */ { Xmm_Eo_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Wq" },
  /* 541 */ { Xmm_Go_Operand, NACL_OPFLAG(OpUse), "$Vq" },
  /* 542 */ { Xmm_Go_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vq" },
  /* 543 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 544 */ { Mdq_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Mdq" },
  /* 545 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 546 */ { RegDS_EDI, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$Xvd}" },
  /* 547 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 548 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$VRdq" },
  /* 549 */ { G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Gv" },
  /* 550 */ { M_Operand, NACL_OPFLAG(OpUse), "$Mv" },
  /* 551 */ { M_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Mv" },
  /* 552 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gv" },
  /* 553 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 554 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 555 */ { RegXMM0, NACL_OPFLAG(OpUse), "%xmm0" },
  /* 556 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 557 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 558 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 559 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Udq/Mq" },
  /* 560 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 561 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Udq/Md" },
  /* 562 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 563 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Udq/Mw" },
  /* 564 */ { Gv_Operand, NACL_OPFLAG(OpUse), "$Gd" },
  /* 565 */ { Mdq_Operand, NACL_OPFLAG(OpUse), "$Mdq" },
  /* 566 */ { Gv_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Gd" },
  /* 567 */ { E_Operand, NACL_OPFLAG(OpUse), "$Eb" },
  /* 568 */ { Gv_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Gd" },
  /* 569 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ev" },
  /* 570 */ { Mmx_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Pq" },
  /* 571 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse), "$Qq" },
  /* 572 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 573 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vss" },
  /* 574 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wss" },
  /* 575 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 576 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vsd" },
  /* 577 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wsd" },
  /* 578 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 579 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 580 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 581 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 582 */ { Ev_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Rd/Mb" },
  /* 583 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 584 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 585 */ { Ev_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Rd/Mw" },
  /* 586 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 587 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 588 */ { E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Ed/q/d" },
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
  /* 603 */ { RegXMM0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%xmm0}" },
  /* 604 */ { RegREAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$rAXv}" },
  /* 605 */ { RegREDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$rDXv}" },
  /* 606 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 607 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 608 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 609 */ { RegRECX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$rCXv}" },
  /* 610 */ { RegREAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$rAXv}" },
  /* 611 */ { RegREDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$rDXv}" },
  /* 612 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 613 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 614 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 615 */ { RegXMM0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%xmm0}" },
  /* 616 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 617 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 618 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 619 */ { RegRECX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$rCXv}" },
  /* 620 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 621 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 622 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 623 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 624 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 625 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 626 */ { RegST1, NACL_OPFLAG(OpUse), "%st1" },
  /* 627 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 628 */ { RegST2, NACL_OPFLAG(OpUse), "%st2" },
  /* 629 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 630 */ { RegST3, NACL_OPFLAG(OpUse), "%st3" },
  /* 631 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 632 */ { RegST4, NACL_OPFLAG(OpUse), "%st4" },
  /* 633 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 634 */ { RegST5, NACL_OPFLAG(OpUse), "%st5" },
  /* 635 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 636 */ { RegST6, NACL_OPFLAG(OpUse), "%st6" },
  /* 637 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 638 */ { RegST7, NACL_OPFLAG(OpUse), "%st7" },
  /* 639 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 640 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 641 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 642 */ { RegST1, NACL_OPFLAG(OpUse), "%st1" },
  /* 643 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 644 */ { RegST2, NACL_OPFLAG(OpUse), "%st2" },
  /* 645 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 646 */ { RegST3, NACL_OPFLAG(OpUse), "%st3" },
  /* 647 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 648 */ { RegST4, NACL_OPFLAG(OpUse), "%st4" },
  /* 649 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 650 */ { RegST5, NACL_OPFLAG(OpUse), "%st5" },
  /* 651 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 652 */ { RegST6, NACL_OPFLAG(OpUse), "%st6" },
  /* 653 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 654 */ { RegST7, NACL_OPFLAG(OpUse), "%st7" },
  /* 655 */ { RegST0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 656 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 657 */ { RegST0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 658 */ { RegST1, NACL_OPFLAG(OpUse), "%st1" },
  /* 659 */ { RegST0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 660 */ { RegST2, NACL_OPFLAG(OpUse), "%st2" },
  /* 661 */ { RegST0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 662 */ { RegST3, NACL_OPFLAG(OpUse), "%st3" },
  /* 663 */ { RegST0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 664 */ { RegST4, NACL_OPFLAG(OpUse), "%st4" },
  /* 665 */ { RegST0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 666 */ { RegST5, NACL_OPFLAG(OpUse), "%st5" },
  /* 667 */ { RegST0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 668 */ { RegST6, NACL_OPFLAG(OpUse), "%st6" },
  /* 669 */ { RegST0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 670 */ { RegST7, NACL_OPFLAG(OpUse), "%st7" },
  /* 671 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 672 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 673 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 674 */ { RegST1, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st1" },
  /* 675 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 676 */ { RegST2, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st2" },
  /* 677 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 678 */ { RegST3, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st3" },
  /* 679 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 680 */ { RegST4, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st4" },
  /* 681 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 682 */ { RegST5, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st5" },
  /* 683 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 684 */ { RegST6, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st6" },
  /* 685 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 686 */ { RegST7, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st7" },
  /* 687 */ { RegST1, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st1" },
  /* 688 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 689 */ { RegST2, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st2" },
  /* 690 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 691 */ { RegST3, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st3" },
  /* 692 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 693 */ { RegST4, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st4" },
  /* 694 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 695 */ { RegST5, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st5" },
  /* 696 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 697 */ { RegST6, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st6" },
  /* 698 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 699 */ { RegST7, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st7" },
  /* 700 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 701 */ { RegST0, NACL_EMPTY_OPFLAGS, "%st0" },
  /* 702 */ { RegST1, NACL_EMPTY_OPFLAGS, "%st1" },
  /* 703 */ { RegST2, NACL_EMPTY_OPFLAGS, "%st2" },
  /* 704 */ { RegST3, NACL_EMPTY_OPFLAGS, "%st3" },
  /* 705 */ { RegST4, NACL_EMPTY_OPFLAGS, "%st4" },
  /* 706 */ { RegST5, NACL_EMPTY_OPFLAGS, "%st5" },
  /* 707 */ { RegST6, NACL_EMPTY_OPFLAGS, "%st6" },
  /* 708 */ { RegST7, NACL_EMPTY_OPFLAGS, "%st7" },
  /* 709 */ { RegST1, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st1" },
  /* 710 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 711 */ { RegST2, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st2" },
  /* 712 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 713 */ { RegST3, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st3" },
  /* 714 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 715 */ { RegST4, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st4" },
  /* 716 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 717 */ { RegST5, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st5" },
  /* 718 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 719 */ { RegST6, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st6" },
  /* 720 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 721 */ { RegST7, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st7" },
  /* 722 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 723 */ { RegAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%ax" },
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
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
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
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
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
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstAdd, 0x00,
    2, g_Operands + 10,
    NULL
  },
  /* 6 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstPush, 0x00,
    2, g_Operands + 12,
    NULL
  },
  /* 7 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstPop, 0x00,
    2, g_Operands + 14,
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
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
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
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
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
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstOr, 0x00,
    2, g_Operands + 10,
    NULL
  },
  /* 14 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstPush, 0x00,
    2, g_Operands + 16,
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
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
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
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
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
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstAdc, 0x00,
    2, g_Operands + 10,
    NULL
  },
  /* 22 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstPush, 0x00,
    2, g_Operands + 18,
    NULL
  },
  /* 23 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstPop, 0x00,
    2, g_Operands + 20,
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
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
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
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
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
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstSbb, 0x00,
    2, g_Operands + 10,
    NULL
  },
  /* 30 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstPush, 0x00,
    2, g_Operands + 22,
    NULL
  },
  /* 31 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstPop, 0x00,
    2, g_Operands + 24,
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
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
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
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
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
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
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
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstDaa, 0x00,
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
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
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
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
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
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
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
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstDas, 0x00,
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
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
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
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
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
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
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
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstAaa, 0x00,
    1, g_Operands + 26,
    NULL
  },
  /* 56 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstCmp, 0x00,
    2, g_Operands + 27,
    NULL
  },
  /* 57 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstCmp, 0x00,
    2, g_Operands + 29,
    NULL
  },
  /* 58 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstCmp, 0x00,
    2, g_Operands + 31,
    NULL
  },
  /* 59 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstCmp, 0x00,
    2, g_Operands + 33,
    NULL
  },
  /* 60 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b),
    InstCmp, 0x00,
    2, g_Operands + 35,
    NULL
  },
  /* 61 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstCmp, 0x00,
    2, g_Operands + 37,
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
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstAas, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 64 */
  { NACLi_386,
    NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstInc, 0x00,
    1, g_Operands + 10,
    NULL
  },
  /* 65 */
  { NACLi_386,
    NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstInc, 0x00,
    1, g_Operands + 39,
    NULL
  },
  /* 66 */
  { NACLi_386,
    NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstInc, 0x00,
    1, g_Operands + 40,
    NULL
  },
  /* 67 */
  { NACLi_386,
    NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstInc, 0x00,
    1, g_Operands + 41,
    NULL
  },
  /* 68 */
  { NACLi_386,
    NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstInc, 0x00,
    1, g_Operands + 42,
    NULL
  },
  /* 69 */
  { NACLi_386,
    NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstInc, 0x00,
    1, g_Operands + 43,
    NULL
  },
  /* 70 */
  { NACLi_386,
    NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstInc, 0x00,
    1, g_Operands + 44,
    NULL
  },
  /* 71 */
  { NACLi_386,
    NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstInc, 0x00,
    1, g_Operands + 45,
    NULL
  },
  /* 72 */
  { NACLi_386,
    NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstDec, 0x00,
    1, g_Operands + 10,
    NULL
  },
  /* 73 */
  { NACLi_386,
    NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstDec, 0x00,
    1, g_Operands + 39,
    NULL
  },
  /* 74 */
  { NACLi_386,
    NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstDec, 0x00,
    1, g_Operands + 40,
    NULL
  },
  /* 75 */
  { NACLi_386,
    NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstDec, 0x00,
    1, g_Operands + 41,
    NULL
  },
  /* 76 */
  { NACLi_386,
    NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstDec, 0x00,
    1, g_Operands + 42,
    NULL
  },
  /* 77 */
  { NACLi_386,
    NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstDec, 0x00,
    1, g_Operands + 43,
    NULL
  },
  /* 78 */
  { NACLi_386,
    NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstDec, 0x00,
    1, g_Operands + 44,
    NULL
  },
  /* 79 */
  { NACLi_386,
    NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstDec, 0x00,
    1, g_Operands + 45,
    NULL
  },
  /* 80 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstPush, 0x00,
    2, g_Operands + 46,
    NULL
  },
  /* 81 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstPush, 0x01,
    2, g_Operands + 46,
    NULL
  },
  /* 82 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstPush, 0x02,
    2, g_Operands + 46,
    NULL
  },
  /* 83 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstPush, 0x03,
    2, g_Operands + 46,
    NULL
  },
  /* 84 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstPush, 0x04,
    2, g_Operands + 46,
    NULL
  },
  /* 85 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstPush, 0x05,
    2, g_Operands + 46,
    NULL
  },
  /* 86 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstPush, 0x06,
    2, g_Operands + 46,
    NULL
  },
  /* 87 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstPush, 0x07,
    2, g_Operands + 46,
    NULL
  },
  /* 88 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstPop, 0x00,
    2, g_Operands + 48,
    NULL
  },
  /* 89 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstPop, 0x01,
    2, g_Operands + 48,
    NULL
  },
  /* 90 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstPop, 0x02,
    2, g_Operands + 48,
    NULL
  },
  /* 91 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstPop, 0x03,
    2, g_Operands + 48,
    NULL
  },
  /* 92 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstPop, 0x04,
    2, g_Operands + 48,
    NULL
  },
  /* 93 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstPop, 0x05,
    2, g_Operands + 48,
    NULL
  },
  /* 94 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstPop, 0x06,
    2, g_Operands + 48,
    NULL
  },
  /* 95 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstPop, 0x07,
    2, g_Operands + 48,
    NULL
  },
  /* 96 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(NaClIllegal),
    InstPusha, 0x00,
    2, g_Operands + 50,
    g_Opcodes + 97
  },
  /* 97 */
  { NACLi_386,
    NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstPushad, 0x00,
    2, g_Operands + 50,
    NULL
  },
  /* 98 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(NaClIllegal),
    InstPopa, 0x00,
    2, g_Operands + 52,
    g_Opcodes + 99
  },
  /* 99 */
  { NACLi_386,
    NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstPopad, 0x00,
    2, g_Operands + 52,
    NULL
  },
  /* 100 */
  { NACLi_386,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstBound, 0x00,
    3, g_Operands + 54,
    NULL
  },
  /* 101 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstArpl, 0x00,
    2, g_Operands + 57,
    NULL
  },
  /* 102 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 103 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 104 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 105 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 106 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16),
    InstPush, 0x00,
    2, g_Operands + 59,
    NULL
  },
  /* 107 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstImul, 0x00,
    3, g_Operands + 61,
    NULL
  },
  /* 108 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b),
    InstPush, 0x00,
    2, g_Operands + 64,
    NULL
  },
  /* 109 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstImul, 0x00,
    3, g_Operands + 66,
    NULL
  },
  /* 110 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(NaClIllegal),
    InstInsb, 0x00,
    2, g_Operands + 69,
    NULL
  },
  /* 111 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(NaClIllegal),
    InstInsw, 0x00,
    2, g_Operands + 71,
    g_Opcodes + 112
  },
  /* 112 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstInsd, 0x00,
    2, g_Operands + 73,
    NULL
  },
  /* 113 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(NaClIllegal),
    InstOutsb, 0x00,
    2, g_Operands + 75,
    NULL
  },
  /* 114 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(NaClIllegal),
    InstOutsw, 0x00,
    2, g_Operands + 77,
    g_Opcodes + 115
  },
  /* 115 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstOutsd, 0x00,
    2, g_Operands + 79,
    NULL
  },
  /* 116 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump),
    InstJo, 0x00,
    2, g_Operands + 81,
    NULL
  },
  /* 117 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump),
    InstJno, 0x00,
    2, g_Operands + 81,
    NULL
  },
  /* 118 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump),
    InstJb, 0x00,
    2, g_Operands + 81,
    NULL
  },
  /* 119 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump),
    InstJnb, 0x00,
    2, g_Operands + 81,
    NULL
  },
  /* 120 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump),
    InstJz, 0x00,
    2, g_Operands + 81,
    NULL
  },
  /* 121 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump),
    InstJnz, 0x00,
    2, g_Operands + 81,
    NULL
  },
  /* 122 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump),
    InstJbe, 0x00,
    2, g_Operands + 81,
    NULL
  },
  /* 123 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump),
    InstJnbe, 0x00,
    2, g_Operands + 81,
    NULL
  },
  /* 124 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump),
    InstJs, 0x00,
    2, g_Operands + 81,
    NULL
  },
  /* 125 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump),
    InstJns, 0x00,
    2, g_Operands + 81,
    NULL
  },
  /* 126 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump),
    InstJp, 0x00,
    2, g_Operands + 81,
    NULL
  },
  /* 127 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump),
    InstJnp, 0x00,
    2, g_Operands + 81,
    NULL
  },
  /* 128 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump),
    InstJl, 0x00,
    2, g_Operands + 81,
    NULL
  },
  /* 129 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump),
    InstJnl, 0x00,
    2, g_Operands + 81,
    NULL
  },
  /* 130 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump),
    InstJle, 0x00,
    2, g_Operands + 81,
    NULL
  },
  /* 131 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump),
    InstJnle, 0x00,
    2, g_Operands + 81,
    NULL
  },
  /* 132 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstAdd, 0x00,
    2, g_Operands + 83,
    g_Opcodes + 133
  },
  /* 133 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstOr, 0x01,
    2, g_Operands + 83,
    g_Opcodes + 134
  },
  /* 134 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstAdc, 0x02,
    2, g_Operands + 83,
    g_Opcodes + 135
  },
  /* 135 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstSbb, 0x03,
    2, g_Operands + 83,
    g_Opcodes + 136
  },
  /* 136 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstAnd, 0x04,
    2, g_Operands + 83,
    g_Opcodes + 137
  },
  /* 137 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstSub, 0x05,
    2, g_Operands + 83,
    g_Opcodes + 138
  },
  /* 138 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstXor, 0x06,
    2, g_Operands + 83,
    g_Opcodes + 139
  },
  /* 139 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstCmp, 0x07,
    2, g_Operands + 85,
    NULL
  },
  /* 140 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstAdd, 0x00,
    2, g_Operands + 87,
    g_Opcodes + 141
  },
  /* 141 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstOr, 0x01,
    2, g_Operands + 87,
    g_Opcodes + 142
  },
  /* 142 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstAdc, 0x02,
    2, g_Operands + 87,
    g_Opcodes + 143
  },
  /* 143 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstSbb, 0x03,
    2, g_Operands + 87,
    g_Opcodes + 144
  },
  /* 144 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstAnd, 0x04,
    2, g_Operands + 87,
    g_Opcodes + 145
  },
  /* 145 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstSub, 0x05,
    2, g_Operands + 87,
    g_Opcodes + 146
  },
  /* 146 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstXor, 0x06,
    2, g_Operands + 87,
    g_Opcodes + 147
  },
  /* 147 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstCmp, 0x07,
    2, g_Operands + 62,
    NULL
  },
  /* 148 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(NaClIllegal),
    InstAdd, 0x00,
    2, g_Operands + 83,
    g_Opcodes + 149
  },
  /* 149 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(NaClIllegal),
    InstOr, 0x01,
    2, g_Operands + 83,
    g_Opcodes + 150
  },
  /* 150 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(NaClIllegal),
    InstAdc, 0x02,
    2, g_Operands + 83,
    g_Opcodes + 151
  },
  /* 151 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(NaClIllegal),
    InstSbb, 0x03,
    2, g_Operands + 83,
    g_Opcodes + 152
  },
  /* 152 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(NaClIllegal),
    InstAnd, 0x04,
    2, g_Operands + 83,
    g_Opcodes + 153
  },
  /* 153 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(NaClIllegal),
    InstSub, 0x05,
    2, g_Operands + 83,
    g_Opcodes + 154
  },
  /* 154 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(NaClIllegal),
    InstXor, 0x06,
    2, g_Operands + 83,
    g_Opcodes + 155
  },
  /* 155 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(NaClIllegal),
    InstCmp, 0x07,
    2, g_Operands + 85,
    NULL
  },
  /* 156 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstAdd, 0x00,
    2, g_Operands + 89,
    g_Opcodes + 157
  },
  /* 157 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstOr, 0x01,
    2, g_Operands + 89,
    g_Opcodes + 158
  },
  /* 158 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstAdc, 0x02,
    2, g_Operands + 89,
    g_Opcodes + 159
  },
  /* 159 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstSbb, 0x03,
    2, g_Operands + 89,
    g_Opcodes + 160
  },
  /* 160 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstAnd, 0x04,
    2, g_Operands + 89,
    g_Opcodes + 161
  },
  /* 161 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstSub, 0x05,
    2, g_Operands + 89,
    g_Opcodes + 162
  },
  /* 162 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstXor, 0x06,
    2, g_Operands + 89,
    g_Opcodes + 163
  },
  /* 163 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstCmp, 0x07,
    2, g_Operands + 67,
    NULL
  },
  /* 164 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstTest, 0x00,
    2, g_Operands + 27,
    NULL
  },
  /* 165 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstTest, 0x00,
    2, g_Operands + 29,
    NULL
  },
  /* 166 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstXchg, 0x00,
    2, g_Operands + 91,
    NULL
  },
  /* 167 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstXchg, 0x00,
    2, g_Operands + 93,
    NULL
  },
  /* 168 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x00,
    2, g_Operands + 95,
    NULL
  },
  /* 169 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMov, 0x00,
    2, g_Operands + 97,
    NULL
  },
  /* 170 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x00,
    2, g_Operands + 99,
    NULL
  },
  /* 171 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMov, 0x00,
    2, g_Operands + 61,
    NULL
  },
  /* 172 */
  { NACLi_386,
    NACL_IFLAG(ModRmRegSOperand) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstMov, 0x00,
    2, g_Operands + 101,
    NULL
  },
  /* 173 */
  { NACLi_386,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstLea, 0x00,
    2, g_Operands + 103,
    NULL
  },
  /* 174 */
  { NACLi_386,
    NACL_IFLAG(ModRmRegSOperand) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstMov, 0x00,
    2, g_Operands + 105,
    NULL
  },
  /* 175 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstPop, 0x00,
    2, g_Operands + 107,
    g_Opcodes + 176
  },
  /* 176 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 177 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstXchg, 0x00,
    2, g_Operands + 109,
    NULL
  },
  /* 178 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstXchg, 0x01,
    2, g_Operands + 109,
    NULL
  },
  /* 179 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstXchg, 0x02,
    2, g_Operands + 109,
    NULL
  },
  /* 180 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstXchg, 0x03,
    2, g_Operands + 109,
    NULL
  },
  /* 181 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstXchg, 0x04,
    2, g_Operands + 109,
    NULL
  },
  /* 182 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstXchg, 0x05,
    2, g_Operands + 109,
    NULL
  },
  /* 183 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstXchg, 0x06,
    2, g_Operands + 109,
    NULL
  },
  /* 184 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstXchg, 0x07,
    2, g_Operands + 109,
    NULL
  },
  /* 185 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w),
    InstCbw, 0x00,
    2, g_Operands + 111,
    g_Opcodes + 186
  },
  /* 186 */
  { NACLi_386,
    NACL_IFLAG(OperandSize_v),
    InstCwde, 0x00,
    2, g_Operands + 113,
    NULL
  },
  /* 187 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w),
    InstCwd, 0x00,
    2, g_Operands + 115,
    g_Opcodes + 188
  },
  /* 188 */
  { NACLi_386,
    NACL_IFLAG(OperandSize_v),
    InstCdq, 0x00,
    2, g_Operands + 117,
    NULL
  },
  /* 189 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(JumpInstruction),
    InstCall, 0x00,
    3, g_Operands + 119,
    g_Opcodes + 190
  },
  /* 190 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_p) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(JumpInstruction),
    InstCall, 0x00,
    3, g_Operands + 122,
    NULL
  },
  /* 191 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFwait, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 192 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(NaClIllegal),
    InstPushf, 0x00,
    2, g_Operands + 125,
    g_Opcodes + 193
  },
  /* 193 */
  { NACLi_386,
    NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstPushfd, 0x00,
    2, g_Operands + 127,
    NULL
  },
  /* 194 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(NaClIllegal),
    InstPopf, 0x00,
    2, g_Operands + 129,
    g_Opcodes + 195
  },
  /* 195 */
  { NACLi_386,
    NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstPopfd, 0x00,
    2, g_Operands + 131,
    NULL
  },
  /* 196 */
  { NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstSahf, 0x00,
    1, g_Operands + 133,
    NULL
  },
  /* 197 */
  { NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstLahf, 0x00,
    1, g_Operands + 134,
    NULL
  },
  /* 198 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_Addr) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x00,
    2, g_Operands + 135,
    NULL
  },
  /* 199 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_Addr) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMov, 0x00,
    2, g_Operands + 137,
    NULL
  },
  /* 200 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_Addr) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x00,
    2, g_Operands + 139,
    NULL
  },
  /* 201 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_Addr) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMov, 0x00,
    2, g_Operands + 141,
    NULL
  },
  /* 202 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_b),
    InstMovsb, 0x00,
    2, g_Operands + 143,
    NULL
  },
  /* 203 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w),
    InstMovsw, 0x00,
    2, g_Operands + 145,
    g_Opcodes + 204
  },
  /* 204 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_v),
    InstMovsd, 0x00,
    2, g_Operands + 147,
    NULL
  },
  /* 205 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(OperandSize_b),
    InstCmpsb, 0x00,
    2, g_Operands + 149,
    NULL
  },
  /* 206 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w),
    InstCmpsw, 0x00,
    2, g_Operands + 151,
    g_Opcodes + 207
  },
  /* 207 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(OperandSize_v),
    InstCmpsd, 0x00,
    2, g_Operands + 153,
    NULL
  },
  /* 208 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b),
    InstTest, 0x00,
    2, g_Operands + 35,
    NULL
  },
  /* 209 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstTest, 0x00,
    2, g_Operands + 37,
    NULL
  },
  /* 210 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_b),
    InstStosb, 0x00,
    2, g_Operands + 155,
    NULL
  },
  /* 211 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w),
    InstStosw, 0x00,
    2, g_Operands + 157,
    g_Opcodes + 212
  },
  /* 212 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_v),
    InstStosd, 0x00,
    2, g_Operands + 159,
    NULL
  },
  /* 213 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_b),
    InstLodsb, 0x00,
    2, g_Operands + 161,
    NULL
  },
  /* 214 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w),
    InstLodsw, 0x00,
    2, g_Operands + 163,
    g_Opcodes + 215
  },
  /* 215 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_v),
    InstLodsd, 0x00,
    2, g_Operands + 165,
    NULL
  },
  /* 216 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(OperandSize_b),
    InstScasb, 0x00,
    2, g_Operands + 167,
    NULL
  },
  /* 217 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w),
    InstScasw, 0x00,
    2, g_Operands + 169,
    g_Opcodes + 218
  },
  /* 218 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(OperandSize_v),
    InstScasd, 0x00,
    2, g_Operands + 171,
    NULL
  },
  /* 219 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x00,
    2, g_Operands + 173,
    NULL
  },
  /* 220 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x01,
    2, g_Operands + 173,
    NULL
  },
  /* 221 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x02,
    2, g_Operands + 173,
    NULL
  },
  /* 222 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x03,
    2, g_Operands + 173,
    NULL
  },
  /* 223 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x04,
    2, g_Operands + 173,
    NULL
  },
  /* 224 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x05,
    2, g_Operands + 173,
    NULL
  },
  /* 225 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x06,
    2, g_Operands + 173,
    NULL
  },
  /* 226 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x07,
    2, g_Operands + 173,
    NULL
  },
  /* 227 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMov, 0x00,
    2, g_Operands + 175,
    NULL
  },
  /* 228 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMov, 0x01,
    2, g_Operands + 175,
    NULL
  },
  /* 229 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMov, 0x02,
    2, g_Operands + 175,
    NULL
  },
  /* 230 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMov, 0x03,
    2, g_Operands + 175,
    NULL
  },
  /* 231 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMov, 0x04,
    2, g_Operands + 175,
    NULL
  },
  /* 232 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMov, 0x05,
    2, g_Operands + 175,
    NULL
  },
  /* 233 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMov, 0x06,
    2, g_Operands + 175,
    NULL
  },
  /* 234 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMov, 0x07,
    2, g_Operands + 175,
    NULL
  },
  /* 235 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstRol, 0x00,
    2, g_Operands + 83,
    g_Opcodes + 236
  },
  /* 236 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstRor, 0x01,
    2, g_Operands + 83,
    g_Opcodes + 237
  },
  /* 237 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstRcl, 0x02,
    2, g_Operands + 83,
    g_Opcodes + 238
  },
  /* 238 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstRcr, 0x03,
    2, g_Operands + 83,
    g_Opcodes + 239
  },
  /* 239 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstShl, 0x04,
    2, g_Operands + 83,
    g_Opcodes + 240
  },
  /* 240 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstShr, 0x05,
    2, g_Operands + 83,
    g_Opcodes + 241
  },
  /* 241 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstShl, 0x06,
    2, g_Operands + 83,
    g_Opcodes + 242
  },
  /* 242 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstSar, 0x07,
    2, g_Operands + 83,
    NULL
  },
  /* 243 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstRol, 0x00,
    2, g_Operands + 89,
    g_Opcodes + 244
  },
  /* 244 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstRor, 0x01,
    2, g_Operands + 89,
    g_Opcodes + 245
  },
  /* 245 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstRcl, 0x02,
    2, g_Operands + 89,
    g_Opcodes + 246
  },
  /* 246 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstRcr, 0x03,
    2, g_Operands + 89,
    g_Opcodes + 247
  },
  /* 247 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstShl, 0x04,
    2, g_Operands + 89,
    g_Opcodes + 248
  },
  /* 248 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstShr, 0x05,
    2, g_Operands + 89,
    g_Opcodes + 249
  },
  /* 249 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstShl, 0x06,
    2, g_Operands + 89,
    g_Opcodes + 250
  },
  /* 250 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstSar, 0x07,
    2, g_Operands + 89,
    NULL
  },
  /* 251 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_w) | NACL_IFLAG(NaClIllegal),
    InstRet, 0x00,
    3, g_Operands + 177,
    NULL
  },
  /* 252 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstRet, 0x00,
    2, g_Operands + 177,
    NULL
  },
  /* 253 */
  { NACLi_386,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstLes, 0x00,
    2, g_Operands + 180,
    NULL
  },
  /* 254 */
  { NACLi_386,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstLds, 0x00,
    2, g_Operands + 180,
    NULL
  },
  /* 255 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x00,
    2, g_Operands + 182,
    g_Opcodes + 256
  },
  /* 256 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 257 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMov, 0x00,
    2, g_Operands + 184,
    g_Opcodes + 258
  },
  /* 258 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 259 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_w) | NACL_IFLAG(OpcodeHasImmed2_b) | NACL_IFLAG(NaClIllegal),
    InstEnter, 0x00,
    4, g_Operands + 186,
    NULL
  },
  /* 260 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstLeave, 0x00,
    2, g_Operands + 190,
    NULL
  },
  /* 261 */
  { NACLi_RETURN,
    NACL_IFLAG(OpcodeHasImmed_w) | NACL_IFLAG(NaClIllegal),
    InstRet, 0x00,
    3, g_Operands + 177,
    NULL
  },
  /* 262 */
  { NACLi_RETURN,
    NACL_IFLAG(NaClIllegal),
    InstRet, 0x00,
    2, g_Operands + 177,
    NULL
  },
  /* 263 */
  { NACLi_SYSTEM,
    NACL_IFLAG(NaClIllegal),
    InstInt3, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 264 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(NaClIllegal),
    InstInt, 0x00,
    1, g_Operands + 9,
    NULL
  },
  /* 265 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstInto, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 266 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstIretd, 0x00,
    2, g_Operands + 177,
    g_Opcodes + 267
  },
  /* 267 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(NaClIllegal),
    InstIret, 0x00,
    2, g_Operands + 177,
    NULL
  },
  /* 268 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstRol, 0x00,
    2, g_Operands + 192,
    g_Opcodes + 269
  },
  /* 269 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstRor, 0x01,
    2, g_Operands + 192,
    g_Opcodes + 270
  },
  /* 270 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstRcl, 0x02,
    2, g_Operands + 192,
    g_Opcodes + 271
  },
  /* 271 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstRcr, 0x03,
    2, g_Operands + 192,
    g_Opcodes + 272
  },
  /* 272 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstShl, 0x04,
    2, g_Operands + 192,
    g_Opcodes + 273
  },
  /* 273 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstShr, 0x05,
    2, g_Operands + 192,
    g_Opcodes + 274
  },
  /* 274 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstShl, 0x06,
    2, g_Operands + 192,
    g_Opcodes + 275
  },
  /* 275 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSar, 0x07,
    2, g_Operands + 192,
    NULL
  },
  /* 276 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstRol, 0x00,
    2, g_Operands + 194,
    g_Opcodes + 277
  },
  /* 277 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstRor, 0x01,
    2, g_Operands + 194,
    g_Opcodes + 278
  },
  /* 278 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstRcl, 0x02,
    2, g_Operands + 194,
    g_Opcodes + 279
  },
  /* 279 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstRcr, 0x03,
    2, g_Operands + 194,
    g_Opcodes + 280
  },
  /* 280 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstShl, 0x04,
    2, g_Operands + 194,
    g_Opcodes + 281
  },
  /* 281 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstShr, 0x05,
    2, g_Operands + 194,
    g_Opcodes + 282
  },
  /* 282 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstShl, 0x06,
    2, g_Operands + 194,
    g_Opcodes + 283
  },
  /* 283 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstSar, 0x07,
    2, g_Operands + 194,
    NULL
  },
  /* 284 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstRol, 0x00,
    2, g_Operands + 196,
    g_Opcodes + 285
  },
  /* 285 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstRor, 0x01,
    2, g_Operands + 196,
    g_Opcodes + 286
  },
  /* 286 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstRcl, 0x02,
    2, g_Operands + 196,
    g_Opcodes + 287
  },
  /* 287 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstRcr, 0x03,
    2, g_Operands + 196,
    g_Opcodes + 288
  },
  /* 288 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstShl, 0x04,
    2, g_Operands + 196,
    g_Opcodes + 289
  },
  /* 289 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstShr, 0x05,
    2, g_Operands + 196,
    g_Opcodes + 290
  },
  /* 290 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstShl, 0x06,
    2, g_Operands + 196,
    g_Opcodes + 291
  },
  /* 291 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSar, 0x07,
    2, g_Operands + 196,
    NULL
  },
  /* 292 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstRol, 0x00,
    2, g_Operands + 198,
    g_Opcodes + 293
  },
  /* 293 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstRor, 0x01,
    2, g_Operands + 198,
    g_Opcodes + 294
  },
  /* 294 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstRcl, 0x02,
    2, g_Operands + 198,
    g_Opcodes + 295
  },
  /* 295 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstRcr, 0x03,
    2, g_Operands + 198,
    g_Opcodes + 296
  },
  /* 296 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstShl, 0x04,
    2, g_Operands + 198,
    g_Opcodes + 297
  },
  /* 297 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstShr, 0x05,
    2, g_Operands + 198,
    g_Opcodes + 298
  },
  /* 298 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstShl, 0x06,
    2, g_Operands + 198,
    g_Opcodes + 299
  },
  /* 299 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstSar, 0x07,
    2, g_Operands + 198,
    NULL
  },
  /* 300 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(NaClIllegal),
    InstAam, 0x00,
    2, g_Operands + 200,
    NULL
  },
  /* 301 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(NaClIllegal),
    InstAad, 0x00,
    2, g_Operands + 200,
    NULL
  },
  /* 302 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstSalc, 0x00,
    1, g_Operands + 202,
    NULL
  },
  /* 303 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstXlat, 0x00,
    2, g_Operands + 203,
    NULL
  },
  /* 304 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFadd, 0x00,
    2, g_Operands + 205,
    g_Opcodes + 305
  },
  /* 305 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFmul, 0x01,
    2, g_Operands + 205,
    g_Opcodes + 306
  },
  /* 306 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFcom, 0x02,
    2, g_Operands + 207,
    g_Opcodes + 307
  },
  /* 307 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFcomp, 0x03,
    2, g_Operands + 207,
    g_Opcodes + 308
  },
  /* 308 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFsub, 0x04,
    2, g_Operands + 205,
    g_Opcodes + 309
  },
  /* 309 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFsubr, 0x05,
    2, g_Operands + 205,
    g_Opcodes + 310
  },
  /* 310 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFdiv, 0x06,
    2, g_Operands + 205,
    g_Opcodes + 311
  },
  /* 311 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFdivr, 0x07,
    2, g_Operands + 205,
    NULL
  },
  /* 312 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFld, 0x00,
    2, g_Operands + 209,
    g_Opcodes + 313
  },
  /* 313 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x01,
    0, g_Operands + 0,
    g_Opcodes + 314
  },
  /* 314 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFst, 0x02,
    2, g_Operands + 211,
    g_Opcodes + 315
  },
  /* 315 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFstp, 0x03,
    2, g_Operands + 211,
    g_Opcodes + 316
  },
  /* 316 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16),
    InstFldenv, 0x04,
    1, g_Operands + 213,
    g_Opcodes + 317
  },
  /* 317 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFldcw, 0x05,
    1, g_Operands + 214,
    g_Opcodes + 318
  },
  /* 318 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16),
    InstFnstenv, 0x06,
    1, g_Operands + 215,
    g_Opcodes + 319
  },
  /* 319 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFnstcw, 0x07,
    1, g_Operands + 216,
    NULL
  },
  /* 320 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFiadd, 0x00,
    2, g_Operands + 205,
    g_Opcodes + 321
  },
  /* 321 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFimul, 0x01,
    2, g_Operands + 205,
    g_Opcodes + 322
  },
  /* 322 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFicom, 0x02,
    2, g_Operands + 205,
    g_Opcodes + 323
  },
  /* 323 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFicomp, 0x03,
    2, g_Operands + 205,
    g_Opcodes + 324
  },
  /* 324 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFisub, 0x04,
    2, g_Operands + 205,
    g_Opcodes + 325
  },
  /* 325 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFisubr, 0x05,
    2, g_Operands + 205,
    g_Opcodes + 326
  },
  /* 326 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFidiv, 0x06,
    2, g_Operands + 205,
    g_Opcodes + 327
  },
  /* 327 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFidivr, 0x07,
    2, g_Operands + 205,
    NULL
  },
  /* 328 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFild, 0x00,
    2, g_Operands + 209,
    g_Opcodes + 329
  },
  /* 329 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFisttp, 0x01,
    2, g_Operands + 211,
    g_Opcodes + 330
  },
  /* 330 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFist, 0x02,
    2, g_Operands + 211,
    g_Opcodes + 331
  },
  /* 331 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFistp, 0x03,
    2, g_Operands + 211,
    g_Opcodes + 332
  },
  /* 332 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x04,
    0, g_Operands + 0,
    g_Opcodes + 333
  },
  /* 333 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16),
    InstFld, 0x05,
    2, g_Operands + 217,
    g_Opcodes + 334
  },
  /* 334 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x06,
    0, g_Operands + 0,
    g_Opcodes + 335
  },
  /* 335 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16),
    InstFstp, 0x07,
    2, g_Operands + 219,
    NULL
  },
  /* 336 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFadd, 0x00,
    2, g_Operands + 221,
    g_Opcodes + 337
  },
  /* 337 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFmul, 0x01,
    2, g_Operands + 221,
    g_Opcodes + 338
  },
  /* 338 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFcom, 0x02,
    2, g_Operands + 223,
    g_Opcodes + 339
  },
  /* 339 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFcomp, 0x03,
    2, g_Operands + 223,
    g_Opcodes + 340
  },
  /* 340 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFsub, 0x04,
    2, g_Operands + 221,
    g_Opcodes + 341
  },
  /* 341 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFsubr, 0x05,
    2, g_Operands + 221,
    g_Opcodes + 342
  },
  /* 342 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFdiv, 0x06,
    2, g_Operands + 221,
    g_Opcodes + 343
  },
  /* 343 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFdivr, 0x07,
    2, g_Operands + 221,
    NULL
  },
  /* 344 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFld, 0x00,
    2, g_Operands + 225,
    g_Opcodes + 345
  },
  /* 345 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFisttp, 0x01,
    2, g_Operands + 227,
    g_Opcodes + 346
  },
  /* 346 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFst, 0x02,
    2, g_Operands + 227,
    g_Opcodes + 347
  },
  /* 347 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFstp, 0x03,
    2, g_Operands + 227,
    g_Opcodes + 348
  },
  /* 348 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16),
    InstFrstor, 0x04,
    1, g_Operands + 213,
    g_Opcodes + 349
  },
  /* 349 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x05,
    0, g_Operands + 0,
    g_Opcodes + 350
  },
  /* 350 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16),
    InstFnsave, 0x06,
    1, g_Operands + 215,
    g_Opcodes + 351
  },
  /* 351 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFnstsw, 0x07,
    1, g_Operands + 216,
    NULL
  },
  /* 352 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFiadd, 0x00,
    2, g_Operands + 229,
    g_Opcodes + 353
  },
  /* 353 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFimul, 0x01,
    2, g_Operands + 229,
    g_Opcodes + 354
  },
  /* 354 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFicom, 0x02,
    2, g_Operands + 231,
    g_Opcodes + 355
  },
  /* 355 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFicomp, 0x03,
    2, g_Operands + 231,
    g_Opcodes + 356
  },
  /* 356 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFisub, 0x04,
    2, g_Operands + 229,
    g_Opcodes + 357
  },
  /* 357 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFisubr, 0x05,
    2, g_Operands + 229,
    g_Opcodes + 358
  },
  /* 358 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFidiv, 0x06,
    2, g_Operands + 229,
    g_Opcodes + 359
  },
  /* 359 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFidivr, 0x07,
    2, g_Operands + 229,
    NULL
  },
  /* 360 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFild, 0x00,
    2, g_Operands + 233,
    g_Opcodes + 361
  },
  /* 361 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFisttp, 0x01,
    2, g_Operands + 235,
    g_Opcodes + 362
  },
  /* 362 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFist, 0x02,
    2, g_Operands + 235,
    g_Opcodes + 363
  },
  /* 363 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFistp, 0x03,
    2, g_Operands + 235,
    g_Opcodes + 364
  },
  /* 364 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16),
    InstFbld, 0x04,
    2, g_Operands + 217,
    g_Opcodes + 365
  },
  /* 365 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16),
    InstFild, 0x05,
    2, g_Operands + 217,
    g_Opcodes + 366
  },
  /* 366 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16),
    InstFbstp, 0x06,
    2, g_Operands + 219,
    g_Opcodes + 367
  },
  /* 367 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16),
    InstFistp, 0x07,
    2, g_Operands + 219,
    NULL
  },
  /* 368 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump),
    InstLoopne, 0x00,
    2, g_Operands + 81,
    NULL
  },
  /* 369 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump),
    InstLoope, 0x00,
    2, g_Operands + 81,
    NULL
  },
  /* 370 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(JumpInstruction),
    InstLoop, 0x00,
    2, g_Operands + 81,
    NULL
  },
  /* 371 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(AddressSize_w) | NACL_IFLAG(ConditionalJump),
    InstJcxz, 0x00,
    3, g_Operands + 237,
    g_Opcodes + 372
  },
  /* 372 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(AddressSize_v) | NACL_IFLAG(ConditionalJump),
    InstJecxz, 0x00,
    3, g_Operands + 240,
    NULL
  },
  /* 373 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(NaClIllegal),
    InstIn, 0x00,
    2, g_Operands + 243,
    NULL
  },
  /* 374 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstIn, 0x00,
    2, g_Operands + 245,
    NULL
  },
  /* 375 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(NaClIllegal),
    InstOut, 0x00,
    2, g_Operands + 247,
    NULL
  },
  /* 376 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstOut, 0x00,
    2, g_Operands + 249,
    NULL
  },
  /* 377 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(JumpInstruction),
    InstCall, 0x00,
    3, g_Operands + 251,
    NULL
  },
  /* 378 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(JumpInstruction),
    InstJmp, 0x00,
    2, g_Operands + 254,
    NULL
  },
  /* 379 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_p) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(JumpInstruction),
    InstJmp, 0x00,
    2, g_Operands + 256,
    NULL
  },
  /* 380 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(JumpInstruction),
    InstJmp, 0x00,
    2, g_Operands + 81,
    NULL
  },
  /* 381 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstIn, 0x00,
    2, g_Operands + 258,
    NULL
  },
  /* 382 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstIn, 0x00,
    2, g_Operands + 260,
    NULL
  },
  /* 383 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstOut, 0x00,
    2, g_Operands + 262,
    NULL
  },
  /* 384 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstOut, 0x00,
    2, g_Operands + 264,
    NULL
  },
  /* 385 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 386 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstInt1, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 387 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 388 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 389 */
  { NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstHlt, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 390 */
  { NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstCmc, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 391 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstTest, 0x00,
    2, g_Operands + 85,
    g_Opcodes + 392
  },
  /* 392 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstTest, 0x01,
    2, g_Operands + 85,
    g_Opcodes + 393
  },
  /* 393 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstNot, 0x02,
    1, g_Operands + 0,
    g_Opcodes + 394
  },
  /* 394 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstNeg, 0x03,
    1, g_Operands + 0,
    g_Opcodes + 395
  },
  /* 395 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstMul, 0x04,
    3, g_Operands + 266,
    g_Opcodes + 396
  },
  /* 396 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstImul, 0x05,
    3, g_Operands + 266,
    g_Opcodes + 397
  },
  /* 397 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstDiv, 0x06,
    3, g_Operands + 266,
    g_Opcodes + 398
  },
  /* 398 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstIdiv, 0x07,
    3, g_Operands + 266,
    NULL
  },
  /* 399 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstTest, 0x00,
    2, g_Operands + 62,
    g_Opcodes + 400
  },
  /* 400 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstTest, 0x01,
    2, g_Operands + 62,
    g_Opcodes + 401
  },
  /* 401 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstNot, 0x02,
    1, g_Operands + 2,
    g_Opcodes + 402
  },
  /* 402 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstNeg, 0x03,
    1, g_Operands + 2,
    g_Opcodes + 403
  },
  /* 403 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMul, 0x04,
    3, g_Operands + 269,
    g_Opcodes + 404
  },
  /* 404 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstImul, 0x05,
    3, g_Operands + 269,
    g_Opcodes + 405
  },
  /* 405 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstDiv, 0x06,
    3, g_Operands + 269,
    g_Opcodes + 406
  },
  /* 406 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstIdiv, 0x07,
    3, g_Operands + 269,
    NULL
  },
  /* 407 */
  { NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstClc, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 408 */
  { NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstStc, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 409 */
  { NACLi_SYSTEM,
    NACL_IFLAG(NaClIllegal),
    InstCli, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 410 */
  { NACLi_SYSTEM,
    NACL_IFLAG(NaClIllegal),
    InstSti, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 411 */
  { NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstCld, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 412 */
  { NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstStd, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 413 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstInc, 0x00,
    1, g_Operands + 0,
    g_Opcodes + 414
  },
  /* 414 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstDec, 0x01,
    1, g_Operands + 0,
    g_Opcodes + 415
  },
  /* 415 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x02,
    0, g_Operands + 0,
    g_Opcodes + 416
  },
  /* 416 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x03,
    0, g_Operands + 0,
    g_Opcodes + 417
  },
  /* 417 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x04,
    0, g_Operands + 0,
    g_Opcodes + 418
  },
  /* 418 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x05,
    0, g_Operands + 0,
    g_Opcodes + 419
  },
  /* 419 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x06,
    0, g_Operands + 0,
    g_Opcodes + 420
  },
  /* 420 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x07,
    0, g_Operands + 0,
    NULL
  },
  /* 421 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstInc, 0x00,
    1, g_Operands + 2,
    g_Opcodes + 422
  },
  /* 422 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstDec, 0x01,
    1, g_Operands + 2,
    g_Opcodes + 423
  },
  /* 423 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(JumpInstruction),
    InstCall, 0x02,
    3, g_Operands + 272,
    g_Opcodes + 424
  },
  /* 424 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(JumpInstruction),
    InstCall, 0x03,
    3, g_Operands + 275,
    g_Opcodes + 425
  },
  /* 425 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(JumpInstruction),
    InstJmp, 0x04,
    2, g_Operands + 278,
    g_Opcodes + 426
  },
  /* 426 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(JumpInstruction),
    InstJmp, 0x05,
    2, g_Operands + 280,
    g_Opcodes + 427
  },
  /* 427 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstPush, 0x06,
    2, g_Operands + 282,
    g_Opcodes + 428
  },
  /* 428 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x07,
    0, g_Operands + 0,
    NULL
  },
  /* 429 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstSldt, 0x00,
    1, g_Operands + 101,
    g_Opcodes + 430
  },
  /* 430 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstStr, 0x01,
    1, g_Operands + 101,
    g_Opcodes + 431
  },
  /* 431 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstLldt, 0x02,
    1, g_Operands + 106,
    g_Opcodes + 432
  },
  /* 432 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstLtr, 0x03,
    1, g_Operands + 106,
    g_Opcodes + 433
  },
  /* 433 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstVerr, 0x04,
    1, g_Operands + 284,
    g_Opcodes + 434
  },
  /* 434 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstVerw, 0x05,
    1, g_Operands + 284,
    g_Opcodes + 435
  },
  /* 435 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x06,
    0, g_Operands + 0,
    g_Opcodes + 436
  },
  /* 436 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x07,
    0, g_Operands + 0,
    NULL
  },
  /* 437 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstSgdt, 0x00,
    1, g_Operands + 285,
    g_Opcodes + 438
  },
  /* 438 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstSidt, 0x01,
    1, g_Operands + 285,
    g_Opcodes + 439
  },
  /* 439 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstMonitor, 0x01,
    3, g_Operands + 286,
    g_Opcodes + 440
  },
  /* 440 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstMwait, 0x11,
    2, g_Operands + 289,
    g_Opcodes + 441
  },
  /* 441 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x01,
    0, g_Operands + 0,
    g_Opcodes + 442
  },
  /* 442 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstLgdt, 0x02,
    1, g_Operands + 291,
    g_Opcodes + 443
  },
  /* 443 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstLidt, 0x03,
    1, g_Operands + 291,
    g_Opcodes + 444
  },
  /* 444 */
  { NACLi_SVM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstVmmcall, 0x13,
    0, g_Operands + 0,
    g_Opcodes + 445
  },
  /* 445 */
  { NACLi_SVM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstVmload, 0x23,
    1, g_Operands + 292,
    g_Opcodes + 446
  },
  /* 446 */
  { NACLi_SVM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstVmsave, 0x33,
    1, g_Operands + 292,
    g_Opcodes + 447
  },
  /* 447 */
  { NACLi_SVM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstStgi, 0x43,
    0, g_Operands + 0,
    g_Opcodes + 448
  },
  /* 448 */
  { NACLi_SVM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstClgi, 0x53,
    0, g_Operands + 0,
    g_Opcodes + 449
  },
  /* 449 */
  { NACLi_SVM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstSkinit, 0x63,
    2, g_Operands + 293,
    g_Opcodes + 450
  },
  /* 450 */
  { NACLi_SVM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvlpga, 0x73,
    2, g_Operands + 295,
    g_Opcodes + 451
  },
  /* 451 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x03,
    0, g_Operands + 0,
    g_Opcodes + 452
  },
  /* 452 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstSmsw, 0x04,
    1, g_Operands + 101,
    g_Opcodes + 453
  },
  /* 453 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x05,
    0, g_Operands + 0,
    g_Opcodes + 454
  },
  /* 454 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstLmsw, 0x06,
    1, g_Operands + 106,
    g_Opcodes + 455
  },
  /* 455 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvlpg, 0x07,
    1, g_Operands + 297,
    g_Opcodes + 456
  },
  /* 456 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x07,
    0, g_Operands + 0,
    NULL
  },
  /* 457 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstLar, 0x00,
    2, g_Operands + 298,
    NULL
  },
  /* 458 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstLsl, 0x00,
    2, g_Operands + 298,
    NULL
  },
  /* 459 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 460 */
  { NACLi_SYSTEM,
    NACL_IFLAG(NaClIllegal),
    InstClts, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 461 */
  { NACLi_SYSTEM,
    NACL_IFLAG(NaClIllegal),
    InstInvd, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 462 */
  { NACLi_SYSTEM,
    NACL_IFLAG(NaClIllegal),
    InstWbinvd, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 463 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 464 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstUd2, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 465 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 466 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstPrefetch_exclusive, 0x00,
    1, g_Operands + 300,
    g_Opcodes + 467
  },
  /* 467 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstPrefetch_modified, 0x01,
    1, g_Operands + 300,
    g_Opcodes + 468
  },
  /* 468 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstPrefetch_reserved, 0x02,
    1, g_Operands + 300,
    g_Opcodes + 469
  },
  /* 469 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstPrefetch_modified, 0x03,
    1, g_Operands + 300,
    g_Opcodes + 470
  },
  /* 470 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstPrefetch_reserved, 0x04,
    1, g_Operands + 300,
    g_Opcodes + 471
  },
  /* 471 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstPrefetch_reserved, 0x05,
    1, g_Operands + 300,
    g_Opcodes + 472
  },
  /* 472 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstPrefetch_reserved, 0x06,
    1, g_Operands + 300,
    g_Opcodes + 473
  },
  /* 473 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstPrefetch_reserved, 0x07,
    1, g_Operands + 300,
    NULL
  },
  /* 474 */
  { NACLi_3DNOW,
    NACL_EMPTY_IFLAGS,
    InstFemms, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 475 */
  { NACLi_INVALID,
    NACL_IFLAG(Opcode0F0F) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    3, g_Operands + 301,
    NULL
  },
  /* 476 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstMovups, 0x00,
    2, g_Operands + 304,
    NULL
  },
  /* 477 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstMovups, 0x00,
    2, g_Operands + 306,
    NULL
  },
  /* 478 */
  { NACLi_SSE,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMovlps, 0x00,
    2, g_Operands + 308,
    g_Opcodes + 479
  },
  /* 479 */
  { NACLi_SSE,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMovhlps, 0x00,
    2, g_Operands + 310,
    NULL
  },
  /* 480 */
  { NACLi_SSE,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMovlps, 0x00,
    2, g_Operands + 312,
    NULL
  },
  /* 481 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstUnpcklps, 0x00,
    2, g_Operands + 314,
    NULL
  },
  /* 482 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstUnpckhps, 0x00,
    2, g_Operands + 314,
    NULL
  },
  /* 483 */
  { NACLi_SSE,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMovhps, 0x00,
    2, g_Operands + 308,
    g_Opcodes + 484
  },
  /* 484 */
  { NACLi_SSE,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMovlhps, 0x00,
    2, g_Operands + 310,
    NULL
  },
  /* 485 */
  { NACLi_SSE,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMovhps, 0x00,
    2, g_Operands + 312,
    NULL
  },
  /* 486 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstPrefetchnta, 0x00,
    1, g_Operands + 300,
    g_Opcodes + 487
  },
  /* 487 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstPrefetcht0, 0x01,
    1, g_Operands + 300,
    g_Opcodes + 488
  },
  /* 488 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstPrefetcht1, 0x02,
    1, g_Operands + 300,
    g_Opcodes + 489
  },
  /* 489 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstPrefetcht2, 0x03,
    1, g_Operands + 300,
    g_Opcodes + 490
  },
  /* 490 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm),
    InstNop, 0x04,
    0, g_Operands + 0,
    g_Opcodes + 491
  },
  /* 491 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm),
    InstNop, 0x05,
    0, g_Operands + 0,
    g_Opcodes + 492
  },
  /* 492 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm),
    InstNop, 0x06,
    0, g_Operands + 0,
    g_Opcodes + 493
  },
  /* 493 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm),
    InstNop, 0x07,
    0, g_Operands + 0,
    NULL
  },
  /* 494 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstNop, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 495 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstNop, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 496 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstNop, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 497 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstNop, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 498 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstNop, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 499 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstNop, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 500 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm),
    InstNop, 0x00,
    0, g_Operands + 0,
    g_Opcodes + 501
  },
  /* 501 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstNop, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 502 */
  { NACLi_SYSTEM,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstMov, 0x00,
    2, g_Operands + 316,
    NULL
  },
  /* 503 */
  { NACLi_SYSTEM,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstMov, 0x00,
    2, g_Operands + 318,
    NULL
  },
  /* 504 */
  { NACLi_SYSTEM,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstMov, 0x00,
    2, g_Operands + 320,
    NULL
  },
  /* 505 */
  { NACLi_SYSTEM,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstMov, 0x00,
    2, g_Operands + 322,
    NULL
  },
  /* 506 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 507 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 508 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 509 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 510 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstMovaps, 0x00,
    2, g_Operands + 304,
    NULL
  },
  /* 511 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstMovaps, 0x00,
    2, g_Operands + 306,
    NULL
  },
  /* 512 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstCvtpi2ps, 0x00,
    2, g_Operands + 324,
    NULL
  },
  /* 513 */
  { NACLi_SSE,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMovntps, 0x00,
    2, g_Operands + 326,
    NULL
  },
  /* 514 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstCvttps2pi, 0x00,
    2, g_Operands + 328,
    NULL
  },
  /* 515 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstCvtps2pi, 0x00,
    2, g_Operands + 328,
    NULL
  },
  /* 516 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstUcomiss, 0x00,
    2, g_Operands + 330,
    NULL
  },
  /* 517 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstComiss, 0x00,
    2, g_Operands + 332,
    NULL
  },
  /* 518 */
  { NACLi_RDMSR,
    NACL_IFLAG(NaClIllegal),
    InstWrmsr, 0x00,
    3, g_Operands + 334,
    NULL
  },
  /* 519 */
  { NACLi_RDTSC,
    NACL_EMPTY_IFLAGS,
    InstRdtsc, 0x00,
    2, g_Operands + 337,
    NULL
  },
  /* 520 */
  { NACLi_RDMSR,
    NACL_IFLAG(NaClIllegal),
    InstRdmsr, 0x00,
    3, g_Operands + 339,
    NULL
  },
  /* 521 */
  { NACLi_SYSTEM,
    NACL_IFLAG(NaClIllegal),
    InstRdpmc, 0x00,
    3, g_Operands + 339,
    NULL
  },
  /* 522 */
  { NACLi_SYSENTER,
    NACL_IFLAG(NaClIllegal),
    InstSysenter, 0x00,
    4, g_Operands + 342,
    NULL
  },
  /* 523 */
  { NACLi_SYSENTER,
    NACL_IFLAG(NaClIllegal),
    InstSysexit, 0x00,
    6, g_Operands + 346,
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
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 528 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 529 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 530 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 531 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 532 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 533 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 534 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstCmovo, 0x00,
    2, g_Operands + 61,
    NULL
  },
  /* 535 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstCmovno, 0x00,
    2, g_Operands + 61,
    NULL
  },
  /* 536 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstCmovb, 0x00,
    2, g_Operands + 61,
    NULL
  },
  /* 537 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstCmovnb, 0x00,
    2, g_Operands + 61,
    NULL
  },
  /* 538 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstCmovz, 0x00,
    2, g_Operands + 61,
    NULL
  },
  /* 539 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstCmovnz, 0x00,
    2, g_Operands + 61,
    NULL
  },
  /* 540 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstCmovbe, 0x00,
    2, g_Operands + 61,
    NULL
  },
  /* 541 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstCmovnbe, 0x00,
    2, g_Operands + 61,
    NULL
  },
  /* 542 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstCmovs, 0x00,
    2, g_Operands + 61,
    NULL
  },
  /* 543 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstCmovns, 0x00,
    2, g_Operands + 61,
    NULL
  },
  /* 544 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstCmovp, 0x00,
    2, g_Operands + 61,
    NULL
  },
  /* 545 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstCmovnp, 0x00,
    2, g_Operands + 61,
    NULL
  },
  /* 546 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstCmovl, 0x00,
    2, g_Operands + 61,
    NULL
  },
  /* 547 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstCmovnl, 0x00,
    2, g_Operands + 61,
    NULL
  },
  /* 548 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstCmovle, 0x00,
    2, g_Operands + 61,
    NULL
  },
  /* 549 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstCmovnle, 0x00,
    2, g_Operands + 61,
    NULL
  },
  /* 550 */
  { NACLi_SSE,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMovmskps, 0x00,
    2, g_Operands + 352,
    NULL
  },
  /* 551 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstSqrtps, 0x00,
    2, g_Operands + 304,
    NULL
  },
  /* 552 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstRsqrtps, 0x00,
    2, g_Operands + 304,
    NULL
  },
  /* 553 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstRcpps, 0x00,
    2, g_Operands + 304,
    NULL
  },
  /* 554 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstAndps, 0x00,
    2, g_Operands + 354,
    NULL
  },
  /* 555 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstAndnps, 0x00,
    2, g_Operands + 354,
    NULL
  },
  /* 556 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstOrps, 0x00,
    2, g_Operands + 354,
    NULL
  },
  /* 557 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstXorps, 0x00,
    2, g_Operands + 354,
    NULL
  },
  /* 558 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstAddps, 0x00,
    2, g_Operands + 354,
    NULL
  },
  /* 559 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstMulps, 0x00,
    2, g_Operands + 354,
    NULL
  },
  /* 560 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm),
    InstCvtps2pd, 0x00,
    2, g_Operands + 356,
    NULL
  },
  /* 561 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm),
    InstCvtdq2ps, 0x00,
    2, g_Operands + 358,
    NULL
  },
  /* 562 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstSubps, 0x00,
    2, g_Operands + 354,
    NULL
  },
  /* 563 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstMinps, 0x00,
    2, g_Operands + 354,
    NULL
  },
  /* 564 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstDivps, 0x00,
    2, g_Operands + 354,
    NULL
  },
  /* 565 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstMaxps, 0x00,
    2, g_Operands + 354,
    NULL
  },
  /* 566 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPunpcklbw, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 567 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPunpcklwd, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 568 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPunpckldq, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 569 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPacksswb, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 570 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPcmpgtb, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 571 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPcmpgtw, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 572 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPcmpgtd, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 573 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPackuswb, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 574 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPunpckhbw, 0x00,
    2, g_Operands + 362,
    NULL
  },
  /* 575 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPunpckhwd, 0x00,
    2, g_Operands + 362,
    NULL
  },
  /* 576 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPunpckhdq, 0x00,
    2, g_Operands + 362,
    NULL
  },
  /* 577 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPackssdw, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 578 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 579 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 580 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstMovd, 0x00,
    2, g_Operands + 364,
    NULL
  },
  /* 581 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstMovq, 0x00,
    2, g_Operands + 366,
    NULL
  },
  /* 582 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstPshufw, 0x00,
    3, g_Operands + 368,
    NULL
  },
  /* 583 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    g_Opcodes + 584
  },
  /* 584 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x01,
    0, g_Operands + 0,
    g_Opcodes + 585
  },
  /* 585 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstPsrlw, 0x02,
    2, g_Operands + 371,
    g_Opcodes + 586
  },
  /* 586 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x03,
    0, g_Operands + 0,
    g_Opcodes + 587
  },
  /* 587 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstPsraw, 0x04,
    2, g_Operands + 371,
    g_Opcodes + 588
  },
  /* 588 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x05,
    0, g_Operands + 0,
    g_Opcodes + 589
  },
  /* 589 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstPsllw, 0x06,
    2, g_Operands + 371,
    g_Opcodes + 590
  },
  /* 590 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x07,
    0, g_Operands + 0,
    NULL
  },
  /* 591 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    g_Opcodes + 592
  },
  /* 592 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x01,
    0, g_Operands + 0,
    g_Opcodes + 593
  },
  /* 593 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstPsrld, 0x02,
    2, g_Operands + 371,
    g_Opcodes + 594
  },
  /* 594 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x03,
    0, g_Operands + 0,
    g_Opcodes + 595
  },
  /* 595 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstPsrad, 0x04,
    2, g_Operands + 371,
    g_Opcodes + 596
  },
  /* 596 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x05,
    0, g_Operands + 0,
    g_Opcodes + 597
  },
  /* 597 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstPslld, 0x06,
    2, g_Operands + 371,
    g_Opcodes + 598
  },
  /* 598 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x07,
    0, g_Operands + 0,
    NULL
  },
  /* 599 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    g_Opcodes + 600
  },
  /* 600 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x01,
    0, g_Operands + 0,
    g_Opcodes + 601
  },
  /* 601 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstPsrlq, 0x02,
    2, g_Operands + 371,
    g_Opcodes + 602
  },
  /* 602 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x03,
    0, g_Operands + 0,
    g_Opcodes + 603
  },
  /* 603 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x04,
    0, g_Operands + 0,
    g_Opcodes + 604
  },
  /* 604 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x05,
    0, g_Operands + 0,
    g_Opcodes + 605
  },
  /* 605 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstPsllq, 0x06,
    2, g_Operands + 371,
    g_Opcodes + 606
  },
  /* 606 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x07,
    0, g_Operands + 0,
    NULL
  },
  /* 607 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPcmpeqb, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 608 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPcmpeqw, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 609 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPcmpeqd, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 610 */
  { NACLi_MMX,
    NACL_EMPTY_IFLAGS,
    InstEmms, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 611 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 612 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 613 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 614 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 615 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 616 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 617 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstMovd, 0x00,
    2, g_Operands + 373,
    NULL
  },
  /* 618 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstMovq, 0x00,
    2, g_Operands + 375,
    NULL
  },
  /* 619 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(ConditionalJump),
    InstJo, 0x00,
    2, g_Operands + 254,
    NULL
  },
  /* 620 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(ConditionalJump),
    InstJno, 0x00,
    2, g_Operands + 254,
    NULL
  },
  /* 621 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(ConditionalJump),
    InstJb, 0x00,
    2, g_Operands + 254,
    NULL
  },
  /* 622 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(ConditionalJump),
    InstJnb, 0x00,
    2, g_Operands + 254,
    NULL
  },
  /* 623 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(ConditionalJump),
    InstJz, 0x00,
    2, g_Operands + 254,
    NULL
  },
  /* 624 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(ConditionalJump),
    InstJnz, 0x00,
    2, g_Operands + 254,
    NULL
  },
  /* 625 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(ConditionalJump),
    InstJbe, 0x00,
    2, g_Operands + 254,
    NULL
  },
  /* 626 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(ConditionalJump),
    InstJnbe, 0x00,
    2, g_Operands + 254,
    NULL
  },
  /* 627 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(ConditionalJump),
    InstJs, 0x00,
    2, g_Operands + 254,
    NULL
  },
  /* 628 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(ConditionalJump),
    InstJns, 0x00,
    2, g_Operands + 254,
    NULL
  },
  /* 629 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(ConditionalJump),
    InstJp, 0x00,
    2, g_Operands + 254,
    NULL
  },
  /* 630 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(ConditionalJump),
    InstJnp, 0x00,
    2, g_Operands + 254,
    NULL
  },
  /* 631 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(ConditionalJump),
    InstJl, 0x00,
    2, g_Operands + 254,
    NULL
  },
  /* 632 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(ConditionalJump),
    InstJnl, 0x00,
    2, g_Operands + 254,
    NULL
  },
  /* 633 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(ConditionalJump),
    InstJle, 0x00,
    2, g_Operands + 254,
    NULL
  },
  /* 634 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(ConditionalJump),
    InstJnle, 0x00,
    2, g_Operands + 254,
    NULL
  },
  /* 635 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSeto, 0x00,
    1, g_Operands + 95,
    NULL
  },
  /* 636 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetno, 0x00,
    1, g_Operands + 95,
    NULL
  },
  /* 637 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetb, 0x00,
    1, g_Operands + 95,
    NULL
  },
  /* 638 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetnb, 0x00,
    1, g_Operands + 95,
    NULL
  },
  /* 639 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetz, 0x00,
    1, g_Operands + 95,
    NULL
  },
  /* 640 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetnz, 0x00,
    1, g_Operands + 95,
    NULL
  },
  /* 641 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetbe, 0x00,
    1, g_Operands + 95,
    NULL
  },
  /* 642 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetnbe, 0x00,
    1, g_Operands + 95,
    NULL
  },
  /* 643 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSets, 0x00,
    1, g_Operands + 95,
    NULL
  },
  /* 644 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetns, 0x00,
    1, g_Operands + 95,
    NULL
  },
  /* 645 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetp, 0x00,
    1, g_Operands + 95,
    NULL
  },
  /* 646 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetnp, 0x00,
    1, g_Operands + 95,
    NULL
  },
  /* 647 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetl, 0x00,
    1, g_Operands + 95,
    NULL
  },
  /* 648 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetnl, 0x00,
    1, g_Operands + 95,
    NULL
  },
  /* 649 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetle, 0x00,
    1, g_Operands + 95,
    NULL
  },
  /* 650 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetnle, 0x00,
    1, g_Operands + 95,
    NULL
  },
  /* 651 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstPush, 0x00,
    2, g_Operands + 377,
    NULL
  },
  /* 652 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstPop, 0x00,
    2, g_Operands + 379,
    NULL
  },
  /* 653 */
  { NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstCpuid, 0x00,
    4, g_Operands + 381,
    NULL
  },
  /* 654 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstBt, 0x00,
    2, g_Operands + 29,
    NULL
  },
  /* 655 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstShld, 0x00,
    3, g_Operands + 385,
    NULL
  },
  /* 656 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstShld, 0x00,
    3, g_Operands + 388,
    NULL
  },
  /* 657 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 658 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 659 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstPush, 0x00,
    2, g_Operands + 391,
    NULL
  },
  /* 660 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstPop, 0x00,
    2, g_Operands + 393,
    NULL
  },
  /* 661 */
  { NACLi_SYSTEM,
    NACL_IFLAG(NaClIllegal),
    InstRsm, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 662 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstBts, 0x00,
    2, g_Operands + 2,
    NULL
  },
  /* 663 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstShrd, 0x00,
    3, g_Operands + 395,
    NULL
  },
  /* 664 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstShrd, 0x00,
    3, g_Operands + 398,
    NULL
  },
  /* 665 */
  { NACLi_FXSAVE,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(NaClIllegal),
    InstFxsave, 0x00,
    1, g_Operands + 215,
    g_Opcodes + 666
  },
  /* 666 */
  { NACLi_FXSAVE,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(NaClIllegal),
    InstFxrstor, 0x01,
    1, g_Operands + 213,
    g_Opcodes + 667
  },
  /* 667 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstLdmxcsr, 0x02,
    1, g_Operands + 206,
    g_Opcodes + 668
  },
  /* 668 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstStmxcsr, 0x03,
    1, g_Operands + 211,
    g_Opcodes + 669
  },
  /* 669 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x04,
    0, g_Operands + 0,
    g_Opcodes + 670
  },
  /* 670 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstLfence, 0x05,
    0, g_Operands + 0,
    g_Opcodes + 671
  },
  /* 671 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstLfence, 0x15,
    0, g_Operands + 0,
    g_Opcodes + 672
  },
  /* 672 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstLfence, 0x25,
    0, g_Operands + 0,
    g_Opcodes + 673
  },
  /* 673 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstLfence, 0x35,
    0, g_Operands + 0,
    g_Opcodes + 674
  },
  /* 674 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstLfence, 0x45,
    0, g_Operands + 0,
    g_Opcodes + 675
  },
  /* 675 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstLfence, 0x55,
    0, g_Operands + 0,
    g_Opcodes + 676
  },
  /* 676 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstLfence, 0x65,
    0, g_Operands + 0,
    g_Opcodes + 677
  },
  /* 677 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstLfence, 0x75,
    0, g_Operands + 0,
    g_Opcodes + 678
  },
  /* 678 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMfence, 0x06,
    0, g_Operands + 0,
    g_Opcodes + 679
  },
  /* 679 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMfence, 0x16,
    0, g_Operands + 0,
    g_Opcodes + 680
  },
  /* 680 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMfence, 0x26,
    0, g_Operands + 0,
    g_Opcodes + 681
  },
  /* 681 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMfence, 0x36,
    0, g_Operands + 0,
    g_Opcodes + 682
  },
  /* 682 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMfence, 0x46,
    0, g_Operands + 0,
    g_Opcodes + 683
  },
  /* 683 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMfence, 0x56,
    0, g_Operands + 0,
    g_Opcodes + 684
  },
  /* 684 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMfence, 0x66,
    0, g_Operands + 0,
    g_Opcodes + 685
  },
  /* 685 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMfence, 0x76,
    0, g_Operands + 0,
    g_Opcodes + 686
  },
  /* 686 */
  { NACLi_SFENCE_CLFLUSH,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstSfence, 0x07,
    0, g_Operands + 0,
    g_Opcodes + 687
  },
  /* 687 */
  { NACLi_SFENCE_CLFLUSH,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstSfence, 0x17,
    0, g_Operands + 0,
    g_Opcodes + 688
  },
  /* 688 */
  { NACLi_SFENCE_CLFLUSH,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstSfence, 0x27,
    0, g_Operands + 0,
    g_Opcodes + 689
  },
  /* 689 */
  { NACLi_SFENCE_CLFLUSH,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstSfence, 0x37,
    0, g_Operands + 0,
    g_Opcodes + 690
  },
  /* 690 */
  { NACLi_SFENCE_CLFLUSH,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstSfence, 0x47,
    0, g_Operands + 0,
    g_Opcodes + 691
  },
  /* 691 */
  { NACLi_SFENCE_CLFLUSH,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstSfence, 0x57,
    0, g_Operands + 0,
    g_Opcodes + 692
  },
  /* 692 */
  { NACLi_SFENCE_CLFLUSH,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstSfence, 0x67,
    0, g_Operands + 0,
    g_Opcodes + 693
  },
  /* 693 */
  { NACLi_SFENCE_CLFLUSH,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstSfence, 0x77,
    0, g_Operands + 0,
    g_Opcodes + 694
  },
  /* 694 */
  { NACLi_SFENCE_CLFLUSH,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstClflush, 0x07,
    1, g_Operands + 297,
    NULL
  },
  /* 695 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstImul, 0x00,
    2, g_Operands + 6,
    NULL
  },
  /* 696 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstCmpxchg, 0x00,
    3, g_Operands + 401,
    NULL
  },
  /* 697 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstCmpxchg, 0x00,
    3, g_Operands + 404,
    NULL
  },
  /* 698 */
  { NACLi_386,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstLss, 0x00,
    2, g_Operands + 180,
    NULL
  },
  /* 699 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstBtr, 0x00,
    2, g_Operands + 2,
    NULL
  },
  /* 700 */
  { NACLi_386,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstLfs, 0x00,
    2, g_Operands + 180,
    NULL
  },
  /* 701 */
  { NACLi_386,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstLgs, 0x00,
    2, g_Operands + 180,
    NULL
  },
  /* 702 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMovzx, 0x00,
    2, g_Operands + 407,
    NULL
  },
  /* 703 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMovzx, 0x00,
    2, g_Operands + 409,
    NULL
  },
  /* 704 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
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
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstBt, 0x04,
    2, g_Operands + 67,
    g_Opcodes + 707
  },
  /* 707 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstBts, 0x05,
    2, g_Operands + 89,
    g_Opcodes + 708
  },
  /* 708 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstBtr, 0x06,
    2, g_Operands + 89,
    g_Opcodes + 709
  },
  /* 709 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstBtc, 0x07,
    2, g_Operands + 89,
    g_Opcodes + 710
  },
  /* 710 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 711 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstBtc, 0x00,
    2, g_Operands + 2,
    NULL
  },
  /* 712 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstBsf, 0x00,
    2, g_Operands + 61,
    NULL
  },
  /* 713 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstBsr, 0x00,
    2, g_Operands + 61,
    NULL
  },
  /* 714 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMovsx, 0x00,
    2, g_Operands + 407,
    NULL
  },
  /* 715 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMovsx, 0x00,
    2, g_Operands + 409,
    NULL
  },
  /* 716 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstXadd, 0x00,
    2, g_Operands + 91,
    NULL
  },
  /* 717 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstXadd, 0x00,
    2, g_Operands + 93,
    NULL
  },
  /* 718 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstCmpps, 0x00,
    3, g_Operands + 411,
    NULL
  },
  /* 719 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstMovnti, 0x00,
    2, g_Operands + 414,
    NULL
  },
  /* 720 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstPinsrw, 0x00,
    3, g_Operands + 416,
    NULL
  },
  /* 721 */
  { NACLi_SSE41,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstPextrw, 0x00,
    3, g_Operands + 419,
    NULL
  },
  /* 722 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstShufps, 0x00,
    3, g_Operands + 411,
    NULL
  },
  /* 723 */
  { NACLi_CMPXCHG8B,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_v),
    InstCmpxchg8b, 0x01,
    3, g_Operands + 422,
    g_Opcodes + 724
  },
  /* 724 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 725 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstBswap, 0x00,
    1, g_Operands + 109,
    NULL
  },
  /* 726 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstBswap, 0x01,
    1, g_Operands + 109,
    NULL
  },
  /* 727 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstBswap, 0x02,
    1, g_Operands + 109,
    NULL
  },
  /* 728 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstBswap, 0x03,
    1, g_Operands + 109,
    NULL
  },
  /* 729 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstBswap, 0x04,
    1, g_Operands + 109,
    NULL
  },
  /* 730 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstBswap, 0x05,
    1, g_Operands + 109,
    NULL
  },
  /* 731 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstBswap, 0x06,
    1, g_Operands + 109,
    NULL
  },
  /* 732 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstBswap, 0x07,
    1, g_Operands + 109,
    NULL
  },
  /* 733 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 734 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsrlw, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 735 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsrld, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 736 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsrlq, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 737 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPaddq, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 738 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPmullw, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 739 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 740 */
  { NACLi_MMX,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstPmovmskb, 0x00,
    2, g_Operands + 419,
    NULL
  },
  /* 741 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsubusb, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 742 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsubusw, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 743 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPminub, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 744 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPand, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 745 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPaddusb, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 746 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPaddusw, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 747 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPmaxub, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 748 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPandn, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 749 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPavgb, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 750 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsraw, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 751 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsrad, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 752 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPavgw, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 753 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPmulhuw, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 754 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPmulhw, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 755 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 756 */
  { NACLi_MMX,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMovntq, 0x00,
    2, g_Operands + 425,
    NULL
  },
  /* 757 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsubsb, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 758 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsubsw, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 759 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPminsw, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 760 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPor, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 761 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPaddsb, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 762 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPaddsw, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 763 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPmaxsw, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 764 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPxor, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 765 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 766 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsllw, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 767 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPslld, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 768 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsllq, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 769 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPmuludq, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 770 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPmaddwd, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 771 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsadbw, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 772 */
  { NACLi_MMX,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_v),
    InstMaskmovq, 0x00,
    3, g_Operands + 427,
    NULL
  },
  /* 773 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsubb, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 774 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsubw, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 775 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsubd, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 776 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsubq, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 777 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPaddb, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 778 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPaddw, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 779 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPaddd, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 780 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 781 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstMovsd, 0x00,
    2, g_Operands + 430,
    NULL
  },
  /* 782 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstMovsd, 0x00,
    2, g_Operands + 432,
    NULL
  },
  /* 783 */
  { NACLi_SSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstMovddup, 0x00,
    2, g_Operands + 434,
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
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 788 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 789 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 790 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 791 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstCvtsi2sd, 0x00,
    2, g_Operands + 436,
    NULL
  },
  /* 792 */
  { NACLi_SSE4A,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstMovntsd, 0x00,
    2, g_Operands + 438,
    NULL
  },
  /* 793 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstCvttsd2si, 0x00,
    2, g_Operands + 440,
    NULL
  },
  /* 794 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstCvtsd2si, 0x00,
    2, g_Operands + 440,
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
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstSqrtsd, 0x00,
    2, g_Operands + 430,
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
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 802 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 803 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
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
    InstAddsd, 0x00,
    2, g_Operands + 442,
    NULL
  },
  /* 806 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstMulsd, 0x00,
    2, g_Operands + 442,
    NULL
  },
  /* 807 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstCvtsd2ss, 0x00,
    2, g_Operands + 444,
    NULL
  },
  /* 808 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 809 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstSubsd, 0x00,
    2, g_Operands + 442,
    NULL
  },
  /* 810 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstMinsd, 0x00,
    2, g_Operands + 442,
    NULL
  },
  /* 811 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstDivsd, 0x00,
    2, g_Operands + 442,
    NULL
  },
  /* 812 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstMaxsd, 0x00,
    2, g_Operands + 442,
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
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
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
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsRepne),
    InstPshuflw, 0x00,
    3, g_Operands + 446,
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
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 834 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
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
  { NACLi_SSE4A,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeHasImmed2_b) | NACL_IFLAG(OpcodeAllowsRepne),
    InstInsertq, 0x00,
    4, g_Operands + 449,
    NULL
  },
  /* 838 */
  { NACLi_SSE4A,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstInsertq, 0x00,
    2, g_Operands + 453,
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
  { NACLi_SSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstHaddps, 0x00,
    2, g_Operands + 354,
    NULL
  },
  /* 842 */
  { NACLi_SSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstHsubps, 0x00,
    2, g_Operands + 354,
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
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
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
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsRepne),
    InstCmpsd_xmm, 0x00,
    3, g_Operands + 455,
    NULL
  },
  /* 855 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
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
  { NACLi_SSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstAddsubps, 0x00,
    2, g_Operands + 458,
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
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
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
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstMovdq2q, 0x00,
    2, g_Operands + 460,
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
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
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
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstCvtpd2dq, 0x00,
    2, g_Operands + 462,
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
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
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
  { NACLi_SSE3,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstLddqu, 0x00,
    2, g_Operands + 464,
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
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 904 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 905 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 906 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 907 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMovss, 0x00,
    2, g_Operands + 466,
    NULL
  },
  /* 908 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMovss, 0x00,
    2, g_Operands + 468,
    NULL
  },
  /* 909 */
  { NACLi_SSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMovsldup, 0x00,
    2, g_Operands + 304,
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
  { NACLi_SSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMovshdup, 0x00,
    2, g_Operands + 304,
    NULL
  },
  /* 914 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 915 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 916 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 917 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstCvtsi2ss, 0x00,
    2, g_Operands + 470,
    NULL
  },
  /* 918 */
  { NACLi_SSE4A,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMovntss, 0x00,
    2, g_Operands + 472,
    NULL
  },
  /* 919 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstCvttss2si, 0x00,
    2, g_Operands + 474,
    NULL
  },
  /* 920 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstCvtss2si, 0x00,
    2, g_Operands + 474,
    NULL
  },
  /* 921 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 922 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
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
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstSqrtss, 0x00,
    2, g_Operands + 304,
    NULL
  },
  /* 925 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstRsqrtss, 0x00,
    2, g_Operands + 466,
    NULL
  },
  /* 926 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstRcpss, 0x00,
    2, g_Operands + 466,
    NULL
  },
  /* 927 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 928 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 929 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 930 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 931 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstAddss, 0x00,
    2, g_Operands + 476,
    NULL
  },
  /* 932 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMulss, 0x00,
    2, g_Operands + 476,
    NULL
  },
  /* 933 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstCvtss2sd, 0x00,
    2, g_Operands + 478,
    NULL
  },
  /* 934 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstCvttps2dq, 0x00,
    2, g_Operands + 480,
    NULL
  },
  /* 935 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstSubss, 0x00,
    2, g_Operands + 476,
    NULL
  },
  /* 936 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMinss, 0x00,
    2, g_Operands + 476,
    NULL
  },
  /* 937 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstDivss, 0x00,
    2, g_Operands + 476,
    NULL
  },
  /* 938 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMaxss, 0x00,
    2, g_Operands + 476,
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
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 951 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
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
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMovdqu, 0x00,
    2, g_Operands + 482,
    NULL
  },
  /* 955 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsRep),
    InstPshufhw, 0x00,
    3, g_Operands + 446,
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
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 966 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 967 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
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
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMovq, 0x00,
    2, g_Operands + 446,
    NULL
  },
  /* 970 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMovdqu, 0x00,
    2, g_Operands + 484,
    NULL
  },
  /* 971 */
  { NACLi_POPCNT,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstPopcnt, 0x00,
    2, g_Operands + 61,
    NULL
  },
  /* 972 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
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
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 976 */
  { NACLi_LZCNT,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstLzcnt, 0x00,
    2, g_Operands + 61,
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
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsRep),
    InstCmpss, 0x00,
    3, g_Operands + 486,
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
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
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
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMovq2dq, 0x00,
    2, g_Operands + 489,
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
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
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
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstCvtdq2pd, 0x00,
    2, g_Operands + 491,
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
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1029 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1030 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1031 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1032 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovupd, 0x00,
    2, g_Operands + 493,
    NULL
  },
  /* 1033 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovupd, 0x00,
    2, g_Operands + 495,
    NULL
  },
  /* 1034 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovlpd, 0x00,
    2, g_Operands + 497,
    NULL
  },
  /* 1035 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovlpd, 0x00,
    2, g_Operands + 438,
    NULL
  },
  /* 1036 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstUnpcklpd, 0x00,
    2, g_Operands + 499,
    NULL
  },
  /* 1037 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstUnpckhpd, 0x00,
    2, g_Operands + 499,
    NULL
  },
  /* 1038 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovhpd, 0x00,
    2, g_Operands + 497,
    NULL
  },
  /* 1039 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovhpd, 0x00,
    2, g_Operands + 438,
    NULL
  },
  /* 1040 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovapd, 0x00,
    2, g_Operands + 493,
    NULL
  },
  /* 1041 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovapd, 0x00,
    2, g_Operands + 495,
    NULL
  },
  /* 1042 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstCvtpi2pd, 0x00,
    2, g_Operands + 501,
    NULL
  },
  /* 1043 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovntpd, 0x00,
    2, g_Operands + 503,
    NULL
  },
  /* 1044 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstCvttpd2pi, 0x00,
    2, g_Operands + 505,
    NULL
  },
  /* 1045 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstCvtpd2pi, 0x00,
    2, g_Operands + 505,
    NULL
  },
  /* 1046 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstUcomisd, 0x00,
    2, g_Operands + 507,
    NULL
  },
  /* 1047 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstComisd, 0x00,
    2, g_Operands + 509,
    NULL
  },
  /* 1048 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovmskpd, 0x00,
    2, g_Operands + 511,
    NULL
  },
  /* 1049 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstSqrtpd, 0x00,
    2, g_Operands + 513,
    NULL
  },
  /* 1050 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1051 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1052 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstAndpd, 0x00,
    2, g_Operands + 458,
    NULL
  },
  /* 1053 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstAndnpd, 0x00,
    2, g_Operands + 458,
    NULL
  },
  /* 1054 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstOrpd, 0x00,
    2, g_Operands + 458,
    NULL
  },
  /* 1055 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstXorpd, 0x00,
    2, g_Operands + 458,
    NULL
  },
  /* 1056 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstAddpd, 0x00,
    2, g_Operands + 458,
    NULL
  },
  /* 1057 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMulpd, 0x00,
    2, g_Operands + 458,
    NULL
  },
  /* 1058 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstCvtpd2ps, 0x00,
    2, g_Operands + 513,
    NULL
  },
  /* 1059 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstCvtps2dq, 0x00,
    2, g_Operands + 480,
    NULL
  },
  /* 1060 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstSubpd, 0x00,
    2, g_Operands + 458,
    NULL
  },
  /* 1061 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMinpd, 0x00,
    2, g_Operands + 458,
    NULL
  },
  /* 1062 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstDivpd, 0x00,
    2, g_Operands + 458,
    NULL
  },
  /* 1063 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMaxpd, 0x00,
    2, g_Operands + 458,
    NULL
  },
  /* 1064 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPunpcklbw, 0x00,
    2, g_Operands + 515,
    NULL
  },
  /* 1065 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPunpcklwd, 0x00,
    2, g_Operands + 515,
    NULL
  },
  /* 1066 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPunpckldq, 0x00,
    2, g_Operands + 515,
    NULL
  },
  /* 1067 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPacksswb, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1068 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPcmpgtb, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1069 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPcmpgtw, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1070 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPcmpgtd, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1071 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPackuswb, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1072 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPunpckhbw, 0x00,
    2, g_Operands + 515,
    NULL
  },
  /* 1073 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPunpckhwd, 0x00,
    2, g_Operands + 515,
    NULL
  },
  /* 1074 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPunpckhdq, 0x00,
    2, g_Operands + 515,
    NULL
  },
  /* 1075 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPackssdw, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1076 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPunpcklqdq, 0x00,
    2, g_Operands + 515,
    NULL
  },
  /* 1077 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPunpckhqdq, 0x00,
    2, g_Operands + 515,
    NULL
  },
  /* 1078 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstMovd, 0x00,
    2, g_Operands + 519,
    NULL
  },
  /* 1079 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovdqa, 0x00,
    2, g_Operands + 482,
    NULL
  },
  /* 1080 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPshufd, 0x00,
    3, g_Operands + 521,
    NULL
  },
  /* 1081 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    g_Opcodes + 1082
  },
  /* 1082 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x01,
    0, g_Operands + 0,
    g_Opcodes + 1083
  },
  /* 1083 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsrlw, 0x02,
    2, g_Operands + 524,
    g_Opcodes + 1084
  },
  /* 1084 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x03,
    0, g_Operands + 0,
    g_Opcodes + 1085
  },
  /* 1085 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsraw, 0x04,
    2, g_Operands + 524,
    g_Opcodes + 1086
  },
  /* 1086 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x05,
    0, g_Operands + 0,
    g_Opcodes + 1087
  },
  /* 1087 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsllw, 0x06,
    2, g_Operands + 524,
    g_Opcodes + 1088
  },
  /* 1088 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x07,
    0, g_Operands + 0,
    NULL
  },
  /* 1089 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    g_Opcodes + 1090
  },
  /* 1090 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x01,
    0, g_Operands + 0,
    g_Opcodes + 1091
  },
  /* 1091 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsrld, 0x02,
    2, g_Operands + 524,
    g_Opcodes + 1092
  },
  /* 1092 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x03,
    0, g_Operands + 0,
    g_Opcodes + 1093
  },
  /* 1093 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsrad, 0x04,
    2, g_Operands + 524,
    g_Opcodes + 1094
  },
  /* 1094 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x05,
    0, g_Operands + 0,
    g_Opcodes + 1095
  },
  /* 1095 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPslld, 0x06,
    2, g_Operands + 524,
    g_Opcodes + 1096
  },
  /* 1096 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x07,
    0, g_Operands + 0,
    NULL
  },
  /* 1097 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    g_Opcodes + 1098
  },
  /* 1098 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x01,
    0, g_Operands + 0,
    g_Opcodes + 1099
  },
  /* 1099 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsrlq, 0x02,
    2, g_Operands + 524,
    g_Opcodes + 1100
  },
  /* 1100 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsrldq, 0x03,
    2, g_Operands + 524,
    g_Opcodes + 1101
  },
  /* 1101 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x04,
    0, g_Operands + 0,
    g_Opcodes + 1102
  },
  /* 1102 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x05,
    0, g_Operands + 0,
    g_Opcodes + 1103
  },
  /* 1103 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsllq, 0x06,
    2, g_Operands + 524,
    g_Opcodes + 1104
  },
  /* 1104 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPslldq, 0x07,
    2, g_Operands + 524,
    NULL
  },
  /* 1105 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPcmpeqb, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1106 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPcmpeqw, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1107 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPcmpeqd, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1108 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1109 */
  { NACLi_SSE4A,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeHasImmed2_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstExtrq, 0x00,
    3, g_Operands + 526,
    g_Opcodes + 1110
  },
  /* 1110 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1111 */
  { NACLi_SSE4A,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstExtrq, 0x00,
    2, g_Operands + 453,
    NULL
  },
  /* 1112 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1113 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1114 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstHaddpd, 0x00,
    2, g_Operands + 458,
    NULL
  },
  /* 1115 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstHsubpd, 0x00,
    2, g_Operands + 458,
    NULL
  },
  /* 1116 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstMovd, 0x00,
    2, g_Operands + 529,
    NULL
  },
  /* 1117 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovdqa, 0x00,
    2, g_Operands + 484,
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
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstCmppd, 0x00,
    3, g_Operands + 531,
    NULL
  },
  /* 1120 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1121 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstPinsrw, 0x00,
    3, g_Operands + 534,
    NULL
  },
  /* 1122 */
  { NACLi_SSE41,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPextrw, 0x00,
    3, g_Operands + 537,
    NULL
  },
  /* 1123 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstShufpd, 0x00,
    3, g_Operands + 531,
    NULL
  },
  /* 1124 */
  { NACLi_SSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstAddsubpd, 0x00,
    2, g_Operands + 458,
    NULL
  },
  /* 1125 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsrlw, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1126 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsrld, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1127 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsrlq, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1128 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPaddq, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1129 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmullw, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1130 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovq, 0x00,
    2, g_Operands + 540,
    NULL
  },
  /* 1131 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovmskb, 0x00,
    2, g_Operands + 537,
    NULL
  },
  /* 1132 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsubusb, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1133 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsubusw, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1134 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPminub, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1135 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPand, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1136 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPaddusb, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1137 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPaddusw, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1138 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmaxub, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1139 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPandn, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1140 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPavgb, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1141 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsraw, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1142 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsrad, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1143 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPavgw, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1144 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmulhuw, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1145 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmulhw, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1146 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstCvttpd2dq, 0x00,
    2, g_Operands + 542,
    NULL
  },
  /* 1147 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovntdq, 0x00,
    2, g_Operands + 544,
    NULL
  },
  /* 1148 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsubsb, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1149 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsubsw, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1150 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPminsw, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1151 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPor, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1152 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPaddsb, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1153 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPaddsw, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1154 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmaxsw, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1155 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPxor, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1156 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1157 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsllw, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1158 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPslld, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1159 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsllq, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1160 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmuludq, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1161 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmaddwd, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1162 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsadbw, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1163 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstMaskmovdqu, 0x00,
    3, g_Operands + 546,
    NULL
  },
  /* 1164 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsubb, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1165 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsubw, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1166 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsubd, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1167 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsubq, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1168 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPaddb, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1169 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPaddw, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1170 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPaddd, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1171 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1172 */
  { NACLi_E3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPi2fw, 0x00,
    2, g_Operands + 366,
    NULL
  },
  /* 1173 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPi2fd, 0x00,
    2, g_Operands + 366,
    NULL
  },
  /* 1174 */
  { NACLi_E3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPf2iw, 0x00,
    2, g_Operands + 366,
    NULL
  },
  /* 1175 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPf2id, 0x00,
    2, g_Operands + 366,
    NULL
  },
  /* 1176 */
  { NACLi_E3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfnacc, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 1177 */
  { NACLi_E3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfpnacc, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 1178 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfcmpge, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 1179 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfmin, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 1180 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfrcp, 0x00,
    2, g_Operands + 366,
    NULL
  },
  /* 1181 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfrsqrt, 0x00,
    2, g_Operands + 366,
    NULL
  },
  /* 1182 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfsub, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 1183 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfadd, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 1184 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfcmpgt, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 1185 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfmax, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 1186 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfrcpit1, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 1187 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfrsqit1, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 1188 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfsubr, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 1189 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfacc, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 1190 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfcmpeq, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 1191 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfmul, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 1192 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfrcpit2, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 1193 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPmulhrw, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 1194 */
  { NACLi_E3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPswapd, 0x00,
    2, g_Operands + 366,
    NULL
  },
  /* 1195 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPavgusb, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 1196 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPshufb, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 1197 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPhaddw, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 1198 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPhaddd, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 1199 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPhaddsw, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 1200 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPmaddubsw, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 1201 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPhsubw, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 1202 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPhsubd, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 1203 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPhsubsw, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 1204 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsignb, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 1205 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsignw, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 1206 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsignd, 0x00,
    2, g_Operands + 360,
    NULL
  },
  /* 1207 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPmulhrsw, 0x00,
    2, g_Operands + 360,
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
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1223 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1224 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPabsb, 0x00,
    2, g_Operands + 366,
    NULL
  },
  /* 1225 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPabsw, 0x00,
    2, g_Operands + 366,
    NULL
  },
  /* 1226 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPabsd, 0x00,
    2, g_Operands + 366,
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
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1435 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1436 */
  { NACLi_MOVBE,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMovbe, 0x00,
    2, g_Operands + 549,
    NULL
  },
  /* 1437 */
  { NACLi_MOVBE,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMovbe, 0x00,
    2, g_Operands + 551,
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
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1451 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1452 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPshufb, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1453 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPhaddw, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1454 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPhaddd, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1455 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPhaddsw, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1456 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmaddubsw, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1457 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPhsubw, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1458 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPhsubd, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1459 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPhsubsw, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1460 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsignb, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1461 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsignw, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1462 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsignd, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1463 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmulhrsw, 0x00,
    2, g_Operands + 517,
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
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
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
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPblendvb, 0x00,
    3, g_Operands + 553,
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
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1471 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1472 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstBlendvps, 0x00,
    3, g_Operands + 553,
    NULL
  },
  /* 1473 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstBlendvpd, 0x00,
    3, g_Operands + 553,
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
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPtest, 0x00,
    2, g_Operands + 556,
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
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1479 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1480 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPabsb, 0x00,
    2, g_Operands + 482,
    NULL
  },
  /* 1481 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPabsw, 0x00,
    2, g_Operands + 482,
    NULL
  },
  /* 1482 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPabsd, 0x00,
    2, g_Operands + 482,
    NULL
  },
  /* 1483 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1484 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovsxbw, 0x00,
    2, g_Operands + 558,
    NULL
  },
  /* 1485 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovsxbd, 0x00,
    2, g_Operands + 560,
    NULL
  },
  /* 1486 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovsxbq, 0x00,
    2, g_Operands + 562,
    NULL
  },
  /* 1487 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovsxwd, 0x00,
    2, g_Operands + 558,
    NULL
  },
  /* 1488 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovsxwq, 0x00,
    2, g_Operands + 560,
    NULL
  },
  /* 1489 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovsxdq, 0x00,
    2, g_Operands + 558,
    NULL
  },
  /* 1490 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1491 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1492 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmuldq, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1493 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPcmpeqq, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1494 */
  { NACLi_SSE41,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovntdqa, 0x00,
    2, g_Operands + 464,
    NULL
  },
  /* 1495 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPackusdw, 0x00,
    2, g_Operands + 517,
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
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1499 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1500 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovzxbw, 0x00,
    2, g_Operands + 558,
    NULL
  },
  /* 1501 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovzxbd, 0x00,
    2, g_Operands + 560,
    NULL
  },
  /* 1502 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovzxbq, 0x00,
    2, g_Operands + 562,
    NULL
  },
  /* 1503 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovzxwd, 0x00,
    2, g_Operands + 558,
    NULL
  },
  /* 1504 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovzxwq, 0x00,
    2, g_Operands + 560,
    NULL
  },
  /* 1505 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovzxdq, 0x00,
    2, g_Operands + 558,
    NULL
  },
  /* 1506 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1507 */
  { NACLi_SSE42,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPcmpgtq, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1508 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPminsb, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1509 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPminsd, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1510 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPminuw, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1511 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPminud, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1512 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmaxsb, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1513 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmaxsd, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1514 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmaxuw, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1515 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmaxud, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1516 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmulld, 0x00,
    2, g_Operands + 517,
    NULL
  },
  /* 1517 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPhminposuw, 0x00,
    2, g_Operands + 517,
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
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1579 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1580 */
  { NACLi_VMX,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvept, 0x00,
    2, g_Operands + 564,
    NULL
  },
  /* 1581 */
  { NACLi_VMX,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvvpid, 0x00,
    2, g_Operands + 564,
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
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1705 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00,
    0, g_Operands + 0,
    NULL
  },
  /* 1706 */
  { NACLi_SSE42,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(OperandSize_b),
    InstCrc32, 0x00,
    2, g_Operands + 566,
    NULL
  },
  /* 1707 */
  { NACLi_SSE42,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstCrc32, 0x00,
    2, g_Operands + 568,
    NULL
  },
  /* 1708 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstPalignr, 0x00,
    3, g_Operands + 570,
    NULL
  },
  /* 1709 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstRoundps, 0x00,
    3, g_Operands + 521,
    NULL
  },
  /* 1710 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstRoundpd, 0x00,
    3, g_Operands + 521,
    NULL
  },
  /* 1711 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstRoundss, 0x00,
    3, g_Operands + 573,
    NULL
  },
  /* 1712 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstRoundsd, 0x00,
    3, g_Operands + 576,
    NULL
  },
  /* 1713 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstBlendps, 0x00,
    3, g_Operands + 579,
    NULL
  },
  /* 1714 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstBlendpd, 0x00,
    3, g_Operands + 579,
    NULL
  },
  /* 1715 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPblendw, 0x00,
    3, g_Operands + 579,
    NULL
  },
  /* 1716 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPalignr, 0x00,
    3, g_Operands + 579,
    NULL
  },
  /* 1717 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPextrb, 0x00,
    3, g_Operands + 582,
    NULL
  },
  /* 1718 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPextrw, 0x00,
    3, g_Operands + 585,
    NULL
  },
  /* 1719 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstPextrd, 0x00,
    3, g_Operands + 588,
    NULL
  },
  /* 1720 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstExtractps, 0x00,
    3, g_Operands + 591,
    NULL
  },
  /* 1721 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstPinsrb, 0x00,
    3, g_Operands + 594,
    NULL
  },
  /* 1722 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstInsertps, 0x00,
    3, g_Operands + 597,
    NULL
  },
  /* 1723 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstPinsrd, 0x00,
    3, g_Operands + 600,
    NULL
  },
  /* 1724 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstDpps, 0x00,
    3, g_Operands + 579,
    NULL
  },
  /* 1725 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstDppd, 0x00,
    3, g_Operands + 579,
    NULL
  },
  /* 1726 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMpsadbw, 0x00,
    3, g_Operands + 579,
    NULL
  },
  /* 1727 */
  { NACLi_SSE42,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstPcmpestrm, 0x00,
    6, g_Operands + 603,
    NULL
  },
  /* 1728 */
  { NACLi_SSE42,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstPcmpestri, 0x00,
    6, g_Operands + 609,
    NULL
  },
  /* 1729 */
  { NACLi_SSE42,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPcmpistrm, 0x00,
    4, g_Operands + 615,
    NULL
  },
  /* 1730 */
  { NACLi_SSE42,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstPcmpistri, 0x00,
    4, g_Operands + 619,
    NULL
  },
  /* 1731 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00,
    2, g_Operands + 623,
    NULL
  },
  /* 1732 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00,
    2, g_Operands + 625,
    NULL
  },
  /* 1733 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00,
    2, g_Operands + 627,
    NULL
  },
  /* 1734 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00,
    2, g_Operands + 629,
    NULL
  },
  /* 1735 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00,
    2, g_Operands + 631,
    NULL
  },
  /* 1736 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00,
    2, g_Operands + 633,
    NULL
  },
  /* 1737 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00,
    2, g_Operands + 635,
    NULL
  },
  /* 1738 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00,
    2, g_Operands + 637,
    NULL
  },
  /* 1739 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00,
    2, g_Operands + 623,
    NULL
  },
  /* 1740 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00,
    2, g_Operands + 625,
    NULL
  },
  /* 1741 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00,
    2, g_Operands + 627,
    NULL
  },
  /* 1742 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00,
    2, g_Operands + 629,
    NULL
  },
  /* 1743 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00,
    2, g_Operands + 631,
    NULL
  },
  /* 1744 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00,
    2, g_Operands + 633,
    NULL
  },
  /* 1745 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00,
    2, g_Operands + 635,
    NULL
  },
  /* 1746 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00,
    2, g_Operands + 637,
    NULL
  },
  /* 1747 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcom, 0x00,
    2, g_Operands + 639,
    NULL
  },
  /* 1748 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcom, 0x00,
    2, g_Operands + 641,
    NULL
  },
  /* 1749 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcom, 0x00,
    2, g_Operands + 643,
    NULL
  },
  /* 1750 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcom, 0x00,
    2, g_Operands + 645,
    NULL
  },
  /* 1751 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcom, 0x00,
    2, g_Operands + 647,
    NULL
  },
  /* 1752 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcom, 0x00,
    2, g_Operands + 649,
    NULL
  },
  /* 1753 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcom, 0x00,
    2, g_Operands + 651,
    NULL
  },
  /* 1754 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcom, 0x00,
    2, g_Operands + 653,
    NULL
  },
  /* 1755 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomp, 0x00,
    2, g_Operands + 639,
    NULL
  },
  /* 1756 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomp, 0x00,
    2, g_Operands + 641,
    NULL
  },
  /* 1757 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomp, 0x00,
    2, g_Operands + 643,
    NULL
  },
  /* 1758 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomp, 0x00,
    2, g_Operands + 645,
    NULL
  },
  /* 1759 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomp, 0x00,
    2, g_Operands + 647,
    NULL
  },
  /* 1760 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomp, 0x00,
    2, g_Operands + 649,
    NULL
  },
  /* 1761 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomp, 0x00,
    2, g_Operands + 651,
    NULL
  },
  /* 1762 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomp, 0x00,
    2, g_Operands + 653,
    NULL
  },
  /* 1763 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00,
    2, g_Operands + 623,
    NULL
  },
  /* 1764 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00,
    2, g_Operands + 625,
    NULL
  },
  /* 1765 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00,
    2, g_Operands + 627,
    NULL
  },
  /* 1766 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00,
    2, g_Operands + 629,
    NULL
  },
  /* 1767 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00,
    2, g_Operands + 631,
    NULL
  },
  /* 1768 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00,
    2, g_Operands + 633,
    NULL
  },
  /* 1769 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00,
    2, g_Operands + 635,
    NULL
  },
  /* 1770 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00,
    2, g_Operands + 637,
    NULL
  },
  /* 1771 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00,
    2, g_Operands + 623,
    NULL
  },
  /* 1772 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00,
    2, g_Operands + 625,
    NULL
  },
  /* 1773 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00,
    2, g_Operands + 627,
    NULL
  },
  /* 1774 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00,
    2, g_Operands + 629,
    NULL
  },
  /* 1775 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00,
    2, g_Operands + 631,
    NULL
  },
  /* 1776 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00,
    2, g_Operands + 633,
    NULL
  },
  /* 1777 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00,
    2, g_Operands + 635,
    NULL
  },
  /* 1778 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00,
    2, g_Operands + 637,
    NULL
  },
  /* 1779 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00,
    2, g_Operands + 623,
    NULL
  },
  /* 1780 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00,
    2, g_Operands + 625,
    NULL
  },
  /* 1781 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00,
    2, g_Operands + 627,
    NULL
  },
  /* 1782 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00,
    2, g_Operands + 629,
    NULL
  },
  /* 1783 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00,
    2, g_Operands + 631,
    NULL
  },
  /* 1784 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00,
    2, g_Operands + 633,
    NULL
  },
  /* 1785 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00,
    2, g_Operands + 635,
    NULL
  },
  /* 1786 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00,
    2, g_Operands + 637,
    NULL
  },
  /* 1787 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00,
    2, g_Operands + 623,
    NULL
  },
  /* 1788 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00,
    2, g_Operands + 625,
    NULL
  },
  /* 1789 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00,
    2, g_Operands + 627,
    NULL
  },
  /* 1790 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00,
    2, g_Operands + 629,
    NULL
  },
  /* 1791 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00,
    2, g_Operands + 631,
    NULL
  },
  /* 1792 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00,
    2, g_Operands + 633,
    NULL
  },
  /* 1793 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00,
    2, g_Operands + 635,
    NULL
  },
  /* 1794 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00,
    2, g_Operands + 637,
    NULL
  },
  /* 1795 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFld, 0x00,
    2, g_Operands + 655,
    NULL
  },
  /* 1796 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFld, 0x00,
    2, g_Operands + 657,
    NULL
  },
  /* 1797 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFld, 0x00,
    2, g_Operands + 659,
    NULL
  },
  /* 1798 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFld, 0x00,
    2, g_Operands + 661,
    NULL
  },
  /* 1799 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFld, 0x00,
    2, g_Operands + 663,
    NULL
  },
  /* 1800 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFld, 0x00,
    2, g_Operands + 665,
    NULL
  },
  /* 1801 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFld, 0x00,
    2, g_Operands + 667,
    NULL
  },
  /* 1802 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFld, 0x00,
    2, g_Operands + 669,
    NULL
  },
  /* 1803 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFxch, 0x00,
    2, g_Operands + 671,
    NULL
  },
  /* 1804 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFxch, 0x00,
    2, g_Operands + 673,
    NULL
  },
  /* 1805 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFxch, 0x00,
    2, g_Operands + 675,
    NULL
  },
  /* 1806 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFxch, 0x00,
    2, g_Operands + 677,
    NULL
  },
  /* 1807 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFxch, 0x00,
    2, g_Operands + 679,
    NULL
  },
  /* 1808 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFxch, 0x00,
    2, g_Operands + 681,
    NULL
  },
  /* 1809 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFxch, 0x00,
    2, g_Operands + 683,
    NULL
  },
  /* 1810 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFxch, 0x00,
    2, g_Operands + 685,
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
    1, g_Operands + 205,
    NULL
  },
  /* 1828 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFabs, 0x00,
    1, g_Operands + 205,
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
    1, g_Operands + 207,
    NULL
  },
  /* 1832 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFxam, 0x00,
    1, g_Operands + 207,
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
    1, g_Operands + 205,
    NULL
  },
  /* 1836 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFldl2t, 0x00,
    1, g_Operands + 205,
    NULL
  },
  /* 1837 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFldl2e, 0x00,
    1, g_Operands + 205,
    NULL
  },
  /* 1838 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFldpi, 0x00,
    1, g_Operands + 205,
    NULL
  },
  /* 1839 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFldlg2, 0x00,
    1, g_Operands + 205,
    NULL
  },
  /* 1840 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFldln2, 0x00,
    1, g_Operands + 205,
    NULL
  },
  /* 1841 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFldz, 0x00,
    1, g_Operands + 205,
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
    1, g_Operands + 205,
    NULL
  },
  /* 1844 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFyl2x, 0x00,
    2, g_Operands + 625,
    NULL
  },
  /* 1845 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFptan, 0x00,
    2, g_Operands + 657,
    NULL
  },
  /* 1846 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFpatan, 0x00,
    2, g_Operands + 625,
    NULL
  },
  /* 1847 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFxtract, 0x00,
    2, g_Operands + 657,
    NULL
  },
  /* 1848 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFprem1, 0x00,
    2, g_Operands + 625,
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
    2, g_Operands + 625,
    NULL
  },
  /* 1852 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFyl2xp1, 0x00,
    2, g_Operands + 625,
    NULL
  },
  /* 1853 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsqrt, 0x00,
    1, g_Operands + 205,
    NULL
  },
  /* 1854 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsincos, 0x00,
    2, g_Operands + 657,
    NULL
  },
  /* 1855 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFrndint, 0x00,
    1, g_Operands + 205,
    NULL
  },
  /* 1856 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFscale, 0x00,
    2, g_Operands + 625,
    NULL
  },
  /* 1857 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsin, 0x00,
    1, g_Operands + 205,
    NULL
  },
  /* 1858 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcos, 0x00,
    1, g_Operands + 205,
    NULL
  },
  /* 1859 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovb, 0x00,
    2, g_Operands + 623,
    NULL
  },
  /* 1860 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovb, 0x00,
    2, g_Operands + 625,
    NULL
  },
  /* 1861 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovb, 0x00,
    2, g_Operands + 627,
    NULL
  },
  /* 1862 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovb, 0x00,
    2, g_Operands + 629,
    NULL
  },
  /* 1863 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovb, 0x00,
    2, g_Operands + 631,
    NULL
  },
  /* 1864 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovb, 0x00,
    2, g_Operands + 633,
    NULL
  },
  /* 1865 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovb, 0x00,
    2, g_Operands + 635,
    NULL
  },
  /* 1866 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovb, 0x00,
    2, g_Operands + 637,
    NULL
  },
  /* 1867 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmove, 0x00,
    2, g_Operands + 623,
    NULL
  },
  /* 1868 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmove, 0x00,
    2, g_Operands + 625,
    NULL
  },
  /* 1869 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmove, 0x00,
    2, g_Operands + 627,
    NULL
  },
  /* 1870 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmove, 0x00,
    2, g_Operands + 629,
    NULL
  },
  /* 1871 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmove, 0x00,
    2, g_Operands + 631,
    NULL
  },
  /* 1872 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmove, 0x00,
    2, g_Operands + 633,
    NULL
  },
  /* 1873 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmove, 0x00,
    2, g_Operands + 635,
    NULL
  },
  /* 1874 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmove, 0x00,
    2, g_Operands + 637,
    NULL
  },
  /* 1875 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovbe, 0x00,
    2, g_Operands + 623,
    NULL
  },
  /* 1876 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovbe, 0x00,
    2, g_Operands + 625,
    NULL
  },
  /* 1877 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovbe, 0x00,
    2, g_Operands + 627,
    NULL
  },
  /* 1878 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovbe, 0x00,
    2, g_Operands + 629,
    NULL
  },
  /* 1879 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovbe, 0x00,
    2, g_Operands + 631,
    NULL
  },
  /* 1880 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovbe, 0x00,
    2, g_Operands + 633,
    NULL
  },
  /* 1881 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovbe, 0x00,
    2, g_Operands + 635,
    NULL
  },
  /* 1882 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovbe, 0x00,
    2, g_Operands + 637,
    NULL
  },
  /* 1883 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovu, 0x00,
    2, g_Operands + 623,
    NULL
  },
  /* 1884 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovu, 0x00,
    2, g_Operands + 625,
    NULL
  },
  /* 1885 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovu, 0x00,
    2, g_Operands + 627,
    NULL
  },
  /* 1886 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovu, 0x00,
    2, g_Operands + 629,
    NULL
  },
  /* 1887 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovu, 0x00,
    2, g_Operands + 631,
    NULL
  },
  /* 1888 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovu, 0x00,
    2, g_Operands + 633,
    NULL
  },
  /* 1889 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovu, 0x00,
    2, g_Operands + 635,
    NULL
  },
  /* 1890 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovu, 0x00,
    2, g_Operands + 637,
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
    2, g_Operands + 641,
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
    2, g_Operands + 623,
    NULL
  },
  /* 1924 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnb, 0x00,
    2, g_Operands + 625,
    NULL
  },
  /* 1925 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnb, 0x00,
    2, g_Operands + 627,
    NULL
  },
  /* 1926 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnb, 0x00,
    2, g_Operands + 629,
    NULL
  },
  /* 1927 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnb, 0x00,
    2, g_Operands + 631,
    NULL
  },
  /* 1928 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnb, 0x00,
    2, g_Operands + 633,
    NULL
  },
  /* 1929 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnb, 0x00,
    2, g_Operands + 635,
    NULL
  },
  /* 1930 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnb, 0x00,
    2, g_Operands + 637,
    NULL
  },
  /* 1931 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovne, 0x00,
    2, g_Operands + 623,
    NULL
  },
  /* 1932 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovne, 0x00,
    2, g_Operands + 625,
    NULL
  },
  /* 1933 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovne, 0x00,
    2, g_Operands + 627,
    NULL
  },
  /* 1934 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovne, 0x00,
    2, g_Operands + 629,
    NULL
  },
  /* 1935 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovne, 0x00,
    2, g_Operands + 631,
    NULL
  },
  /* 1936 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovne, 0x00,
    2, g_Operands + 633,
    NULL
  },
  /* 1937 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovne, 0x00,
    2, g_Operands + 635,
    NULL
  },
  /* 1938 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovne, 0x00,
    2, g_Operands + 637,
    NULL
  },
  /* 1939 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnbe, 0x00,
    2, g_Operands + 623,
    NULL
  },
  /* 1940 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnbe, 0x00,
    2, g_Operands + 625,
    NULL
  },
  /* 1941 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnbe, 0x00,
    2, g_Operands + 627,
    NULL
  },
  /* 1942 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnbe, 0x00,
    2, g_Operands + 629,
    NULL
  },
  /* 1943 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnbe, 0x00,
    2, g_Operands + 631,
    NULL
  },
  /* 1944 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnbe, 0x00,
    2, g_Operands + 633,
    NULL
  },
  /* 1945 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnbe, 0x00,
    2, g_Operands + 635,
    NULL
  },
  /* 1946 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnbe, 0x00,
    2, g_Operands + 637,
    NULL
  },
  /* 1947 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnu, 0x00,
    2, g_Operands + 623,
    NULL
  },
  /* 1948 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnu, 0x00,
    2, g_Operands + 625,
    NULL
  },
  /* 1949 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnu, 0x00,
    2, g_Operands + 627,
    NULL
  },
  /* 1950 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnu, 0x00,
    2, g_Operands + 629,
    NULL
  },
  /* 1951 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnu, 0x00,
    2, g_Operands + 631,
    NULL
  },
  /* 1952 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnu, 0x00,
    2, g_Operands + 633,
    NULL
  },
  /* 1953 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnu, 0x00,
    2, g_Operands + 635,
    NULL
  },
  /* 1954 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnu, 0x00,
    2, g_Operands + 637,
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
    2, g_Operands + 639,
    NULL
  },
  /* 1964 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomi, 0x00,
    2, g_Operands + 641,
    NULL
  },
  /* 1965 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomi, 0x00,
    2, g_Operands + 643,
    NULL
  },
  /* 1966 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomi, 0x00,
    2, g_Operands + 645,
    NULL
  },
  /* 1967 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomi, 0x00,
    2, g_Operands + 647,
    NULL
  },
  /* 1968 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomi, 0x00,
    2, g_Operands + 649,
    NULL
  },
  /* 1969 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomi, 0x00,
    2, g_Operands + 651,
    NULL
  },
  /* 1970 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomi, 0x00,
    2, g_Operands + 653,
    NULL
  },
  /* 1971 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomi, 0x00,
    2, g_Operands + 639,
    NULL
  },
  /* 1972 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomi, 0x00,
    2, g_Operands + 641,
    NULL
  },
  /* 1973 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomi, 0x00,
    2, g_Operands + 643,
    NULL
  },
  /* 1974 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomi, 0x00,
    2, g_Operands + 645,
    NULL
  },
  /* 1975 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomi, 0x00,
    2, g_Operands + 647,
    NULL
  },
  /* 1976 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomi, 0x00,
    2, g_Operands + 649,
    NULL
  },
  /* 1977 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomi, 0x00,
    2, g_Operands + 651,
    NULL
  },
  /* 1978 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomi, 0x00,
    2, g_Operands + 653,
    NULL
  },
  /* 1979 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00,
    2, g_Operands + 623,
    NULL
  },
  /* 1980 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00,
    2, g_Operands + 687,
    NULL
  },
  /* 1981 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00,
    2, g_Operands + 689,
    NULL
  },
  /* 1982 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00,
    2, g_Operands + 691,
    NULL
  },
  /* 1983 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00,
    2, g_Operands + 693,
    NULL
  },
  /* 1984 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00,
    2, g_Operands + 695,
    NULL
  },
  /* 1985 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00,
    2, g_Operands + 697,
    NULL
  },
  /* 1986 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00,
    2, g_Operands + 699,
    NULL
  },
  /* 1987 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00,
    2, g_Operands + 623,
    NULL
  },
  /* 1988 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00,
    2, g_Operands + 687,
    NULL
  },
  /* 1989 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00,
    2, g_Operands + 689,
    NULL
  },
  /* 1990 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00,
    2, g_Operands + 691,
    NULL
  },
  /* 1991 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00,
    2, g_Operands + 693,
    NULL
  },
  /* 1992 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00,
    2, g_Operands + 695,
    NULL
  },
  /* 1993 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00,
    2, g_Operands + 697,
    NULL
  },
  /* 1994 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00,
    2, g_Operands + 699,
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
    2, g_Operands + 623,
    NULL
  },
  /* 2012 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00,
    2, g_Operands + 687,
    NULL
  },
  /* 2013 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00,
    2, g_Operands + 689,
    NULL
  },
  /* 2014 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00,
    2, g_Operands + 691,
    NULL
  },
  /* 2015 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00,
    2, g_Operands + 693,
    NULL
  },
  /* 2016 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00,
    2, g_Operands + 695,
    NULL
  },
  /* 2017 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00,
    2, g_Operands + 697,
    NULL
  },
  /* 2018 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00,
    2, g_Operands + 699,
    NULL
  },
  /* 2019 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00,
    2, g_Operands + 623,
    NULL
  },
  /* 2020 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00,
    2, g_Operands + 687,
    NULL
  },
  /* 2021 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00,
    2, g_Operands + 689,
    NULL
  },
  /* 2022 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00,
    2, g_Operands + 691,
    NULL
  },
  /* 2023 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00,
    2, g_Operands + 693,
    NULL
  },
  /* 2024 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00,
    2, g_Operands + 695,
    NULL
  },
  /* 2025 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00,
    2, g_Operands + 697,
    NULL
  },
  /* 2026 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00,
    2, g_Operands + 699,
    NULL
  },
  /* 2027 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00,
    2, g_Operands + 623,
    NULL
  },
  /* 2028 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00,
    2, g_Operands + 687,
    NULL
  },
  /* 2029 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00,
    2, g_Operands + 689,
    NULL
  },
  /* 2030 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00,
    2, g_Operands + 691,
    NULL
  },
  /* 2031 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00,
    2, g_Operands + 693,
    NULL
  },
  /* 2032 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00,
    2, g_Operands + 695,
    NULL
  },
  /* 2033 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00,
    2, g_Operands + 697,
    NULL
  },
  /* 2034 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00,
    2, g_Operands + 699,
    NULL
  },
  /* 2035 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00,
    2, g_Operands + 623,
    NULL
  },
  /* 2036 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00,
    2, g_Operands + 687,
    NULL
  },
  /* 2037 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00,
    2, g_Operands + 689,
    NULL
  },
  /* 2038 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00,
    2, g_Operands + 691,
    NULL
  },
  /* 2039 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00,
    2, g_Operands + 693,
    NULL
  },
  /* 2040 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00,
    2, g_Operands + 695,
    NULL
  },
  /* 2041 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00,
    2, g_Operands + 697,
    NULL
  },
  /* 2042 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00,
    2, g_Operands + 699,
    NULL
  },
  /* 2043 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFfree, 0x00,
    1, g_Operands + 701,
    NULL
  },
  /* 2044 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFfree, 0x00,
    1, g_Operands + 702,
    NULL
  },
  /* 2045 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFfree, 0x00,
    1, g_Operands + 703,
    NULL
  },
  /* 2046 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFfree, 0x00,
    1, g_Operands + 704,
    NULL
  },
  /* 2047 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFfree, 0x00,
    1, g_Operands + 705,
    NULL
  },
  /* 2048 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFfree, 0x00,
    1, g_Operands + 706,
    NULL
  },
  /* 2049 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFfree, 0x00,
    1, g_Operands + 707,
    NULL
  },
  /* 2050 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFfree, 0x00,
    1, g_Operands + 708,
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
    2, g_Operands + 655,
    NULL
  },
  /* 2060 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFst, 0x00,
    2, g_Operands + 709,
    NULL
  },
  /* 2061 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFst, 0x00,
    2, g_Operands + 711,
    NULL
  },
  /* 2062 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFst, 0x00,
    2, g_Operands + 713,
    NULL
  },
  /* 2063 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFst, 0x00,
    2, g_Operands + 715,
    NULL
  },
  /* 2064 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFst, 0x00,
    2, g_Operands + 717,
    NULL
  },
  /* 2065 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFst, 0x00,
    2, g_Operands + 719,
    NULL
  },
  /* 2066 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFst, 0x00,
    2, g_Operands + 721,
    NULL
  },
  /* 2067 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFstp, 0x00,
    2, g_Operands + 655,
    NULL
  },
  /* 2068 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFstp, 0x00,
    2, g_Operands + 709,
    NULL
  },
  /* 2069 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFstp, 0x00,
    2, g_Operands + 711,
    NULL
  },
  /* 2070 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFstp, 0x00,
    2, g_Operands + 713,
    NULL
  },
  /* 2071 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFstp, 0x00,
    2, g_Operands + 715,
    NULL
  },
  /* 2072 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFstp, 0x00,
    2, g_Operands + 717,
    NULL
  },
  /* 2073 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFstp, 0x00,
    2, g_Operands + 719,
    NULL
  },
  /* 2074 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFstp, 0x00,
    2, g_Operands + 721,
    NULL
  },
  /* 2075 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucom, 0x00,
    2, g_Operands + 639,
    NULL
  },
  /* 2076 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucom, 0x00,
    2, g_Operands + 641,
    NULL
  },
  /* 2077 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucom, 0x00,
    2, g_Operands + 643,
    NULL
  },
  /* 2078 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucom, 0x00,
    2, g_Operands + 645,
    NULL
  },
  /* 2079 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucom, 0x00,
    2, g_Operands + 647,
    NULL
  },
  /* 2080 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucom, 0x00,
    2, g_Operands + 649,
    NULL
  },
  /* 2081 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucom, 0x00,
    2, g_Operands + 651,
    NULL
  },
  /* 2082 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucom, 0x00,
    2, g_Operands + 653,
    NULL
  },
  /* 2083 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomp, 0x00,
    2, g_Operands + 639,
    NULL
  },
  /* 2084 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomp, 0x00,
    2, g_Operands + 641,
    NULL
  },
  /* 2085 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomp, 0x00,
    2, g_Operands + 643,
    NULL
  },
  /* 2086 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomp, 0x00,
    2, g_Operands + 645,
    NULL
  },
  /* 2087 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomp, 0x00,
    2, g_Operands + 647,
    NULL
  },
  /* 2088 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomp, 0x00,
    2, g_Operands + 649,
    NULL
  },
  /* 2089 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomp, 0x00,
    2, g_Operands + 651,
    NULL
  },
  /* 2090 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomp, 0x00,
    2, g_Operands + 653,
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
    2, g_Operands + 623,
    NULL
  },
  /* 2108 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFaddp, 0x00,
    2, g_Operands + 687,
    NULL
  },
  /* 2109 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFaddp, 0x00,
    2, g_Operands + 689,
    NULL
  },
  /* 2110 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFaddp, 0x00,
    2, g_Operands + 691,
    NULL
  },
  /* 2111 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFaddp, 0x00,
    2, g_Operands + 693,
    NULL
  },
  /* 2112 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFaddp, 0x00,
    2, g_Operands + 695,
    NULL
  },
  /* 2113 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFaddp, 0x00,
    2, g_Operands + 697,
    NULL
  },
  /* 2114 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFaddp, 0x00,
    2, g_Operands + 699,
    NULL
  },
  /* 2115 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmulp, 0x00,
    2, g_Operands + 623,
    NULL
  },
  /* 2116 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmulp, 0x00,
    2, g_Operands + 687,
    NULL
  },
  /* 2117 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmulp, 0x00,
    2, g_Operands + 689,
    NULL
  },
  /* 2118 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmulp, 0x00,
    2, g_Operands + 691,
    NULL
  },
  /* 2119 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmulp, 0x00,
    2, g_Operands + 693,
    NULL
  },
  /* 2120 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmulp, 0x00,
    2, g_Operands + 695,
    NULL
  },
  /* 2121 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmulp, 0x00,
    2, g_Operands + 697,
    NULL
  },
  /* 2122 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmulp, 0x00,
    2, g_Operands + 699,
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
    2, g_Operands + 641,
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
    2, g_Operands + 623,
    NULL
  },
  /* 2140 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubrp, 0x00,
    2, g_Operands + 687,
    NULL
  },
  /* 2141 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubrp, 0x00,
    2, g_Operands + 689,
    NULL
  },
  /* 2142 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubrp, 0x00,
    2, g_Operands + 691,
    NULL
  },
  /* 2143 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubrp, 0x00,
    2, g_Operands + 693,
    NULL
  },
  /* 2144 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubrp, 0x00,
    2, g_Operands + 695,
    NULL
  },
  /* 2145 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubrp, 0x00,
    2, g_Operands + 697,
    NULL
  },
  /* 2146 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubrp, 0x00,
    2, g_Operands + 699,
    NULL
  },
  /* 2147 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubp, 0x00,
    2, g_Operands + 623,
    NULL
  },
  /* 2148 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubp, 0x00,
    2, g_Operands + 687,
    NULL
  },
  /* 2149 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubp, 0x00,
    2, g_Operands + 689,
    NULL
  },
  /* 2150 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubp, 0x00,
    2, g_Operands + 691,
    NULL
  },
  /* 2151 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubp, 0x00,
    2, g_Operands + 693,
    NULL
  },
  /* 2152 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubp, 0x00,
    2, g_Operands + 695,
    NULL
  },
  /* 2153 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubp, 0x00,
    2, g_Operands + 697,
    NULL
  },
  /* 2154 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubp, 0x00,
    2, g_Operands + 699,
    NULL
  },
  /* 2155 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivrp, 0x00,
    2, g_Operands + 623,
    NULL
  },
  /* 2156 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivrp, 0x00,
    2, g_Operands + 687,
    NULL
  },
  /* 2157 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivrp, 0x00,
    2, g_Operands + 689,
    NULL
  },
  /* 2158 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivrp, 0x00,
    2, g_Operands + 691,
    NULL
  },
  /* 2159 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivrp, 0x00,
    2, g_Operands + 693,
    NULL
  },
  /* 2160 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivrp, 0x00,
    2, g_Operands + 695,
    NULL
  },
  /* 2161 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivrp, 0x00,
    2, g_Operands + 697,
    NULL
  },
  /* 2162 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivrp, 0x00,
    2, g_Operands + 699,
    NULL
  },
  /* 2163 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivp, 0x00,
    2, g_Operands + 623,
    NULL
  },
  /* 2164 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivp, 0x00,
    2, g_Operands + 687,
    NULL
  },
  /* 2165 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivp, 0x00,
    2, g_Operands + 689,
    NULL
  },
  /* 2166 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivp, 0x00,
    2, g_Operands + 691,
    NULL
  },
  /* 2167 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivp, 0x00,
    2, g_Operands + 693,
    NULL
  },
  /* 2168 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivp, 0x00,
    2, g_Operands + 695,
    NULL
  },
  /* 2169 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivp, 0x00,
    2, g_Operands + 697,
    NULL
  },
  /* 2170 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivp, 0x00,
    2, g_Operands + 699,
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
    1, g_Operands + 723,
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
    2, g_Operands + 639,
    NULL
  },
  /* 2212 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomip, 0x00,
    2, g_Operands + 641,
    NULL
  },
  /* 2213 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomip, 0x00,
    2, g_Operands + 643,
    NULL
  },
  /* 2214 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomip, 0x00,
    2, g_Operands + 645,
    NULL
  },
  /* 2215 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomip, 0x00,
    2, g_Operands + 647,
    NULL
  },
  /* 2216 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomip, 0x00,
    2, g_Operands + 649,
    NULL
  },
  /* 2217 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomip, 0x00,
    2, g_Operands + 651,
    NULL
  },
  /* 2218 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomip, 0x00,
    2, g_Operands + 653,
    NULL
  },
  /* 2219 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomip, 0x00,
    2, g_Operands + 639,
    NULL
  },
  /* 2220 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomip, 0x00,
    2, g_Operands + 641,
    NULL
  },
  /* 2221 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomip, 0x00,
    2, g_Operands + 643,
    NULL
  },
  /* 2222 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomip, 0x00,
    2, g_Operands + 645,
    NULL
  },
  /* 2223 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomip, 0x00,
    2, g_Operands + 647,
    NULL
  },
  /* 2224 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomip, 0x00,
    2, g_Operands + 649,
    NULL
  },
  /* 2225 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomip, 0x00,
    2, g_Operands + 651,
    NULL
  },
  /* 2226 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomip, 0x00,
    2, g_Operands + 653,
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
  /* 40 */ g_Opcodes + 64  ,
  /* 41 */ g_Opcodes + 65  ,
  /* 42 */ g_Opcodes + 66  ,
  /* 43 */ g_Opcodes + 67  ,
  /* 44 */ g_Opcodes + 68  ,
  /* 45 */ g_Opcodes + 69  ,
  /* 46 */ g_Opcodes + 70  ,
  /* 47 */ g_Opcodes + 71  ,
  /* 48 */ g_Opcodes + 72  ,
  /* 49 */ g_Opcodes + 73  ,
  /* 4a */ g_Opcodes + 74  ,
  /* 4b */ g_Opcodes + 75  ,
  /* 4c */ g_Opcodes + 76  ,
  /* 4d */ g_Opcodes + 77  ,
  /* 4e */ g_Opcodes + 78  ,
  /* 4f */ g_Opcodes + 79  ,
  /* 50 */ g_Opcodes + 80  ,
  /* 51 */ g_Opcodes + 81  ,
  /* 52 */ g_Opcodes + 82  ,
  /* 53 */ g_Opcodes + 83  ,
  /* 54 */ g_Opcodes + 84  ,
  /* 55 */ g_Opcodes + 85  ,
  /* 56 */ g_Opcodes + 86  ,
  /* 57 */ g_Opcodes + 87  ,
  /* 58 */ g_Opcodes + 88  ,
  /* 59 */ g_Opcodes + 89  ,
  /* 5a */ g_Opcodes + 90  ,
  /* 5b */ g_Opcodes + 91  ,
  /* 5c */ g_Opcodes + 92  ,
  /* 5d */ g_Opcodes + 93  ,
  /* 5e */ g_Opcodes + 94  ,
  /* 5f */ g_Opcodes + 95  ,
  /* 60 */ g_Opcodes + 96  ,
  /* 61 */ g_Opcodes + 98  ,
  /* 62 */ g_Opcodes + 100  ,
  /* 63 */ g_Opcodes + 101  ,
  /* 64 */ g_Opcodes + 102  ,
  /* 65 */ g_Opcodes + 103  ,
  /* 66 */ g_Opcodes + 104  ,
  /* 67 */ g_Opcodes + 105  ,
  /* 68 */ g_Opcodes + 106  ,
  /* 69 */ g_Opcodes + 107  ,
  /* 6a */ g_Opcodes + 108  ,
  /* 6b */ g_Opcodes + 109  ,
  /* 6c */ g_Opcodes + 110  ,
  /* 6d */ g_Opcodes + 111  ,
  /* 6e */ g_Opcodes + 113  ,
  /* 6f */ g_Opcodes + 114  ,
  /* 70 */ g_Opcodes + 116  ,
  /* 71 */ g_Opcodes + 117  ,
  /* 72 */ g_Opcodes + 118  ,
  /* 73 */ g_Opcodes + 119  ,
  /* 74 */ g_Opcodes + 120  ,
  /* 75 */ g_Opcodes + 121  ,
  /* 76 */ g_Opcodes + 122  ,
  /* 77 */ g_Opcodes + 123  ,
  /* 78 */ g_Opcodes + 124  ,
  /* 79 */ g_Opcodes + 125  ,
  /* 7a */ g_Opcodes + 126  ,
  /* 7b */ g_Opcodes + 127  ,
  /* 7c */ g_Opcodes + 128  ,
  /* 7d */ g_Opcodes + 129  ,
  /* 7e */ g_Opcodes + 130  ,
  /* 7f */ g_Opcodes + 131  ,
  /* 80 */ g_Opcodes + 132  ,
  /* 81 */ g_Opcodes + 140  ,
  /* 82 */ g_Opcodes + 148  ,
  /* 83 */ g_Opcodes + 156  ,
  /* 84 */ g_Opcodes + 164  ,
  /* 85 */ g_Opcodes + 165  ,
  /* 86 */ g_Opcodes + 166  ,
  /* 87 */ g_Opcodes + 167  ,
  /* 88 */ g_Opcodes + 168  ,
  /* 89 */ g_Opcodes + 169  ,
  /* 8a */ g_Opcodes + 170  ,
  /* 8b */ g_Opcodes + 171  ,
  /* 8c */ g_Opcodes + 172  ,
  /* 8d */ g_Opcodes + 173  ,
  /* 8e */ g_Opcodes + 174  ,
  /* 8f */ g_Opcodes + 175  ,
  /* 90 */ g_Opcodes + 177  ,
  /* 91 */ g_Opcodes + 178  ,
  /* 92 */ g_Opcodes + 179  ,
  /* 93 */ g_Opcodes + 180  ,
  /* 94 */ g_Opcodes + 181  ,
  /* 95 */ g_Opcodes + 182  ,
  /* 96 */ g_Opcodes + 183  ,
  /* 97 */ g_Opcodes + 184  ,
  /* 98 */ g_Opcodes + 185  ,
  /* 99 */ g_Opcodes + 187  ,
  /* 9a */ g_Opcodes + 189  ,
  /* 9b */ g_Opcodes + 191  ,
  /* 9c */ g_Opcodes + 192  ,
  /* 9d */ g_Opcodes + 194  ,
  /* 9e */ g_Opcodes + 196  ,
  /* 9f */ g_Opcodes + 197  ,
  /* a0 */ g_Opcodes + 198  ,
  /* a1 */ g_Opcodes + 199  ,
  /* a2 */ g_Opcodes + 200  ,
  /* a3 */ g_Opcodes + 201  ,
  /* a4 */ g_Opcodes + 202  ,
  /* a5 */ g_Opcodes + 203  ,
  /* a6 */ g_Opcodes + 205  ,
  /* a7 */ g_Opcodes + 206  ,
  /* a8 */ g_Opcodes + 208  ,
  /* a9 */ g_Opcodes + 209  ,
  /* aa */ g_Opcodes + 210  ,
  /* ab */ g_Opcodes + 211  ,
  /* ac */ g_Opcodes + 213  ,
  /* ad */ g_Opcodes + 214  ,
  /* ae */ g_Opcodes + 216  ,
  /* af */ g_Opcodes + 217  ,
  /* b0 */ g_Opcodes + 219  ,
  /* b1 */ g_Opcodes + 220  ,
  /* b2 */ g_Opcodes + 221  ,
  /* b3 */ g_Opcodes + 222  ,
  /* b4 */ g_Opcodes + 223  ,
  /* b5 */ g_Opcodes + 224  ,
  /* b6 */ g_Opcodes + 225  ,
  /* b7 */ g_Opcodes + 226  ,
  /* b8 */ g_Opcodes + 227  ,
  /* b9 */ g_Opcodes + 228  ,
  /* ba */ g_Opcodes + 229  ,
  /* bb */ g_Opcodes + 230  ,
  /* bc */ g_Opcodes + 231  ,
  /* bd */ g_Opcodes + 232  ,
  /* be */ g_Opcodes + 233  ,
  /* bf */ g_Opcodes + 234  ,
  /* c0 */ g_Opcodes + 235  ,
  /* c1 */ g_Opcodes + 243  ,
  /* c2 */ g_Opcodes + 251  ,
  /* c3 */ g_Opcodes + 252  ,
  /* c4 */ g_Opcodes + 253  ,
  /* c5 */ g_Opcodes + 254  ,
  /* c6 */ g_Opcodes + 255  ,
  /* c7 */ g_Opcodes + 257  ,
  /* c8 */ g_Opcodes + 259  ,
  /* c9 */ g_Opcodes + 260  ,
  /* ca */ g_Opcodes + 261  ,
  /* cb */ g_Opcodes + 262  ,
  /* cc */ g_Opcodes + 263  ,
  /* cd */ g_Opcodes + 264  ,
  /* ce */ g_Opcodes + 265  ,
  /* cf */ g_Opcodes + 266  ,
  /* d0 */ g_Opcodes + 268  ,
  /* d1 */ g_Opcodes + 276  ,
  /* d2 */ g_Opcodes + 284  ,
  /* d3 */ g_Opcodes + 292  ,
  /* d4 */ g_Opcodes + 300  ,
  /* d5 */ g_Opcodes + 301  ,
  /* d6 */ g_Opcodes + 302  ,
  /* d7 */ g_Opcodes + 303  ,
  /* d8 */ g_Opcodes + 304  ,
  /* d9 */ g_Opcodes + 312  ,
  /* da */ g_Opcodes + 320  ,
  /* db */ g_Opcodes + 328  ,
  /* dc */ g_Opcodes + 336  ,
  /* dd */ g_Opcodes + 344  ,
  /* de */ g_Opcodes + 352  ,
  /* df */ g_Opcodes + 360  ,
  /* e0 */ g_Opcodes + 368  ,
  /* e1 */ g_Opcodes + 369  ,
  /* e2 */ g_Opcodes + 370  ,
  /* e3 */ g_Opcodes + 371  ,
  /* e4 */ g_Opcodes + 373  ,
  /* e5 */ g_Opcodes + 374  ,
  /* e6 */ g_Opcodes + 375  ,
  /* e7 */ g_Opcodes + 376  ,
  /* e8 */ g_Opcodes + 377  ,
  /* e9 */ g_Opcodes + 378  ,
  /* ea */ g_Opcodes + 379  ,
  /* eb */ g_Opcodes + 380  ,
  /* ec */ g_Opcodes + 381  ,
  /* ed */ g_Opcodes + 382  ,
  /* ee */ g_Opcodes + 383  ,
  /* ef */ g_Opcodes + 384  ,
  /* f0 */ g_Opcodes + 385  ,
  /* f1 */ g_Opcodes + 386  ,
  /* f2 */ g_Opcodes + 387  ,
  /* f3 */ g_Opcodes + 388  ,
  /* f4 */ g_Opcodes + 389  ,
  /* f5 */ g_Opcodes + 390  ,
  /* f6 */ g_Opcodes + 391  ,
  /* f7 */ g_Opcodes + 399  ,
  /* f8 */ g_Opcodes + 407  ,
  /* f9 */ g_Opcodes + 408  ,
  /* fa */ g_Opcodes + 409  ,
  /* fb */ g_Opcodes + 410  ,
  /* fc */ g_Opcodes + 411  ,
  /* fd */ g_Opcodes + 412  ,
  /* fe */ g_Opcodes + 413  ,
  /* ff */ g_Opcodes + 421  ,
},
/* Prefix0F */
{
  /* 00 */ g_Opcodes + 429  ,
  /* 01 */ g_Opcodes + 437  ,
  /* 02 */ g_Opcodes + 457  ,
  /* 03 */ g_Opcodes + 458  ,
  /* 04 */ g_Opcodes + 459  ,
  /* 05 */ NULL  ,
  /* 06 */ g_Opcodes + 460  ,
  /* 07 */ NULL  ,
  /* 08 */ g_Opcodes + 461  ,
  /* 09 */ g_Opcodes + 462  ,
  /* 0a */ g_Opcodes + 463  ,
  /* 0b */ g_Opcodes + 464  ,
  /* 0c */ g_Opcodes + 465  ,
  /* 0d */ g_Opcodes + 466  ,
  /* 0e */ g_Opcodes + 474  ,
  /* 0f */ g_Opcodes + 475  ,
  /* 10 */ g_Opcodes + 476  ,
  /* 11 */ g_Opcodes + 477  ,
  /* 12 */ g_Opcodes + 478  ,
  /* 13 */ g_Opcodes + 480  ,
  /* 14 */ g_Opcodes + 481  ,
  /* 15 */ g_Opcodes + 482  ,
  /* 16 */ g_Opcodes + 483  ,
  /* 17 */ g_Opcodes + 485  ,
  /* 18 */ g_Opcodes + 486  ,
  /* 19 */ g_Opcodes + 494  ,
  /* 1a */ g_Opcodes + 495  ,
  /* 1b */ g_Opcodes + 496  ,
  /* 1c */ g_Opcodes + 497  ,
  /* 1d */ g_Opcodes + 498  ,
  /* 1e */ g_Opcodes + 499  ,
  /* 1f */ g_Opcodes + 500  ,
  /* 20 */ g_Opcodes + 502  ,
  /* 21 */ g_Opcodes + 503  ,
  /* 22 */ g_Opcodes + 504  ,
  /* 23 */ g_Opcodes + 505  ,
  /* 24 */ g_Opcodes + 506  ,
  /* 25 */ g_Opcodes + 507  ,
  /* 26 */ g_Opcodes + 508  ,
  /* 27 */ g_Opcodes + 509  ,
  /* 28 */ g_Opcodes + 510  ,
  /* 29 */ g_Opcodes + 511  ,
  /* 2a */ g_Opcodes + 512  ,
  /* 2b */ g_Opcodes + 513  ,
  /* 2c */ g_Opcodes + 514  ,
  /* 2d */ g_Opcodes + 515  ,
  /* 2e */ g_Opcodes + 516  ,
  /* 2f */ g_Opcodes + 517  ,
  /* 30 */ g_Opcodes + 518  ,
  /* 31 */ g_Opcodes + 519  ,
  /* 32 */ g_Opcodes + 520  ,
  /* 33 */ g_Opcodes + 521  ,
  /* 34 */ g_Opcodes + 522  ,
  /* 35 */ g_Opcodes + 523  ,
  /* 36 */ g_Opcodes + 524  ,
  /* 37 */ g_Opcodes + 525  ,
  /* 38 */ g_Opcodes + 526  ,
  /* 39 */ g_Opcodes + 527  ,
  /* 3a */ g_Opcodes + 528  ,
  /* 3b */ g_Opcodes + 529  ,
  /* 3c */ g_Opcodes + 530  ,
  /* 3d */ g_Opcodes + 531  ,
  /* 3e */ g_Opcodes + 532  ,
  /* 3f */ g_Opcodes + 533  ,
  /* 40 */ g_Opcodes + 534  ,
  /* 41 */ g_Opcodes + 535  ,
  /* 42 */ g_Opcodes + 536  ,
  /* 43 */ g_Opcodes + 537  ,
  /* 44 */ g_Opcodes + 538  ,
  /* 45 */ g_Opcodes + 539  ,
  /* 46 */ g_Opcodes + 540  ,
  /* 47 */ g_Opcodes + 541  ,
  /* 48 */ g_Opcodes + 542  ,
  /* 49 */ g_Opcodes + 543  ,
  /* 4a */ g_Opcodes + 544  ,
  /* 4b */ g_Opcodes + 545  ,
  /* 4c */ g_Opcodes + 546  ,
  /* 4d */ g_Opcodes + 547  ,
  /* 4e */ g_Opcodes + 548  ,
  /* 4f */ g_Opcodes + 549  ,
  /* 50 */ g_Opcodes + 550  ,
  /* 51 */ g_Opcodes + 551  ,
  /* 52 */ g_Opcodes + 552  ,
  /* 53 */ g_Opcodes + 553  ,
  /* 54 */ g_Opcodes + 554  ,
  /* 55 */ g_Opcodes + 555  ,
  /* 56 */ g_Opcodes + 556  ,
  /* 57 */ g_Opcodes + 557  ,
  /* 58 */ g_Opcodes + 558  ,
  /* 59 */ g_Opcodes + 559  ,
  /* 5a */ g_Opcodes + 560  ,
  /* 5b */ g_Opcodes + 561  ,
  /* 5c */ g_Opcodes + 562  ,
  /* 5d */ g_Opcodes + 563  ,
  /* 5e */ g_Opcodes + 564  ,
  /* 5f */ g_Opcodes + 565  ,
  /* 60 */ g_Opcodes + 566  ,
  /* 61 */ g_Opcodes + 567  ,
  /* 62 */ g_Opcodes + 568  ,
  /* 63 */ g_Opcodes + 569  ,
  /* 64 */ g_Opcodes + 570  ,
  /* 65 */ g_Opcodes + 571  ,
  /* 66 */ g_Opcodes + 572  ,
  /* 67 */ g_Opcodes + 573  ,
  /* 68 */ g_Opcodes + 574  ,
  /* 69 */ g_Opcodes + 575  ,
  /* 6a */ g_Opcodes + 576  ,
  /* 6b */ g_Opcodes + 577  ,
  /* 6c */ g_Opcodes + 578  ,
  /* 6d */ g_Opcodes + 579  ,
  /* 6e */ g_Opcodes + 580  ,
  /* 6f */ g_Opcodes + 581  ,
  /* 70 */ g_Opcodes + 582  ,
  /* 71 */ g_Opcodes + 583  ,
  /* 72 */ g_Opcodes + 591  ,
  /* 73 */ g_Opcodes + 599  ,
  /* 74 */ g_Opcodes + 607  ,
  /* 75 */ g_Opcodes + 608  ,
  /* 76 */ g_Opcodes + 609  ,
  /* 77 */ g_Opcodes + 610  ,
  /* 78 */ g_Opcodes + 611  ,
  /* 79 */ g_Opcodes + 612  ,
  /* 7a */ g_Opcodes + 613  ,
  /* 7b */ g_Opcodes + 614  ,
  /* 7c */ g_Opcodes + 615  ,
  /* 7d */ g_Opcodes + 616  ,
  /* 7e */ g_Opcodes + 617  ,
  /* 7f */ g_Opcodes + 618  ,
  /* 80 */ g_Opcodes + 619  ,
  /* 81 */ g_Opcodes + 620  ,
  /* 82 */ g_Opcodes + 621  ,
  /* 83 */ g_Opcodes + 622  ,
  /* 84 */ g_Opcodes + 623  ,
  /* 85 */ g_Opcodes + 624  ,
  /* 86 */ g_Opcodes + 625  ,
  /* 87 */ g_Opcodes + 626  ,
  /* 88 */ g_Opcodes + 627  ,
  /* 89 */ g_Opcodes + 628  ,
  /* 8a */ g_Opcodes + 629  ,
  /* 8b */ g_Opcodes + 630  ,
  /* 8c */ g_Opcodes + 631  ,
  /* 8d */ g_Opcodes + 632  ,
  /* 8e */ g_Opcodes + 633  ,
  /* 8f */ g_Opcodes + 634  ,
  /* 90 */ g_Opcodes + 635  ,
  /* 91 */ g_Opcodes + 636  ,
  /* 92 */ g_Opcodes + 637  ,
  /* 93 */ g_Opcodes + 638  ,
  /* 94 */ g_Opcodes + 639  ,
  /* 95 */ g_Opcodes + 640  ,
  /* 96 */ g_Opcodes + 641  ,
  /* 97 */ g_Opcodes + 642  ,
  /* 98 */ g_Opcodes + 643  ,
  /* 99 */ g_Opcodes + 644  ,
  /* 9a */ g_Opcodes + 645  ,
  /* 9b */ g_Opcodes + 646  ,
  /* 9c */ g_Opcodes + 647  ,
  /* 9d */ g_Opcodes + 648  ,
  /* 9e */ g_Opcodes + 649  ,
  /* 9f */ g_Opcodes + 650  ,
  /* a0 */ g_Opcodes + 651  ,
  /* a1 */ g_Opcodes + 652  ,
  /* a2 */ g_Opcodes + 653  ,
  /* a3 */ g_Opcodes + 654  ,
  /* a4 */ g_Opcodes + 655  ,
  /* a5 */ g_Opcodes + 656  ,
  /* a6 */ g_Opcodes + 657  ,
  /* a7 */ g_Opcodes + 658  ,
  /* a8 */ g_Opcodes + 659  ,
  /* a9 */ g_Opcodes + 660  ,
  /* aa */ g_Opcodes + 661  ,
  /* ab */ g_Opcodes + 662  ,
  /* ac */ g_Opcodes + 663  ,
  /* ad */ g_Opcodes + 664  ,
  /* ae */ g_Opcodes + 665  ,
  /* af */ g_Opcodes + 695  ,
  /* b0 */ g_Opcodes + 696  ,
  /* b1 */ g_Opcodes + 697  ,
  /* b2 */ g_Opcodes + 698  ,
  /* b3 */ g_Opcodes + 699  ,
  /* b4 */ g_Opcodes + 700  ,
  /* b5 */ g_Opcodes + 701  ,
  /* b6 */ g_Opcodes + 702  ,
  /* b7 */ g_Opcodes + 703  ,
  /* b8 */ g_Opcodes + 704  ,
  /* b9 */ g_Opcodes + 705  ,
  /* ba */ g_Opcodes + 706  ,
  /* bb */ g_Opcodes + 711  ,
  /* bc */ g_Opcodes + 712  ,
  /* bd */ g_Opcodes + 713  ,
  /* be */ g_Opcodes + 714  ,
  /* bf */ g_Opcodes + 715  ,
  /* c0 */ g_Opcodes + 716  ,
  /* c1 */ g_Opcodes + 717  ,
  /* c2 */ g_Opcodes + 718  ,
  /* c3 */ g_Opcodes + 719  ,
  /* c4 */ g_Opcodes + 720  ,
  /* c5 */ g_Opcodes + 721  ,
  /* c6 */ g_Opcodes + 722  ,
  /* c7 */ g_Opcodes + 723  ,
  /* c8 */ g_Opcodes + 725  ,
  /* c9 */ g_Opcodes + 726  ,
  /* ca */ g_Opcodes + 727  ,
  /* cb */ g_Opcodes + 728  ,
  /* cc */ g_Opcodes + 729  ,
  /* cd */ g_Opcodes + 730  ,
  /* ce */ g_Opcodes + 731  ,
  /* cf */ g_Opcodes + 732  ,
  /* d0 */ g_Opcodes + 733  ,
  /* d1 */ g_Opcodes + 734  ,
  /* d2 */ g_Opcodes + 735  ,
  /* d3 */ g_Opcodes + 736  ,
  /* d4 */ g_Opcodes + 737  ,
  /* d5 */ g_Opcodes + 738  ,
  /* d6 */ g_Opcodes + 739  ,
  /* d7 */ g_Opcodes + 740  ,
  /* d8 */ g_Opcodes + 741  ,
  /* d9 */ g_Opcodes + 742  ,
  /* da */ g_Opcodes + 743  ,
  /* db */ g_Opcodes + 744  ,
  /* dc */ g_Opcodes + 745  ,
  /* dd */ g_Opcodes + 746  ,
  /* de */ g_Opcodes + 747  ,
  /* df */ g_Opcodes + 748  ,
  /* e0 */ g_Opcodes + 749  ,
  /* e1 */ g_Opcodes + 750  ,
  /* e2 */ g_Opcodes + 751  ,
  /* e3 */ g_Opcodes + 752  ,
  /* e4 */ g_Opcodes + 753  ,
  /* e5 */ g_Opcodes + 754  ,
  /* e6 */ g_Opcodes + 755  ,
  /* e7 */ g_Opcodes + 756  ,
  /* e8 */ g_Opcodes + 757  ,
  /* e9 */ g_Opcodes + 758  ,
  /* ea */ g_Opcodes + 759  ,
  /* eb */ g_Opcodes + 760  ,
  /* ec */ g_Opcodes + 761  ,
  /* ed */ g_Opcodes + 762  ,
  /* ee */ g_Opcodes + 763  ,
  /* ef */ g_Opcodes + 764  ,
  /* f0 */ g_Opcodes + 765  ,
  /* f1 */ g_Opcodes + 766  ,
  /* f2 */ g_Opcodes + 767  ,
  /* f3 */ g_Opcodes + 768  ,
  /* f4 */ g_Opcodes + 769  ,
  /* f5 */ g_Opcodes + 770  ,
  /* f6 */ g_Opcodes + 771  ,
  /* f7 */ g_Opcodes + 772  ,
  /* f8 */ g_Opcodes + 773  ,
  /* f9 */ g_Opcodes + 774  ,
  /* fa */ g_Opcodes + 775  ,
  /* fb */ g_Opcodes + 776  ,
  /* fc */ g_Opcodes + 777  ,
  /* fd */ g_Opcodes + 778  ,
  /* fe */ g_Opcodes + 779  ,
  /* ff */ g_Opcodes + 780  ,
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
  /* 10 */ g_Opcodes + 781  ,
  /* 11 */ g_Opcodes + 782  ,
  /* 12 */ g_Opcodes + 783  ,
  /* 13 */ g_Opcodes + 784  ,
  /* 14 */ g_Opcodes + 785  ,
  /* 15 */ g_Opcodes + 786  ,
  /* 16 */ g_Opcodes + 787  ,
  /* 17 */ g_Opcodes + 788  ,
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
  /* 28 */ g_Opcodes + 789  ,
  /* 29 */ g_Opcodes + 790  ,
  /* 2a */ g_Opcodes + 791  ,
  /* 2b */ g_Opcodes + 792  ,
  /* 2c */ g_Opcodes + 793  ,
  /* 2d */ g_Opcodes + 794  ,
  /* 2e */ g_Opcodes + 795  ,
  /* 2f */ g_Opcodes + 796  ,
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
  /* 50 */ g_Opcodes + 797  ,
  /* 51 */ g_Opcodes + 798  ,
  /* 52 */ g_Opcodes + 799  ,
  /* 53 */ g_Opcodes + 800  ,
  /* 54 */ g_Opcodes + 801  ,
  /* 55 */ g_Opcodes + 802  ,
  /* 56 */ g_Opcodes + 803  ,
  /* 57 */ g_Opcodes + 804  ,
  /* 58 */ g_Opcodes + 805  ,
  /* 59 */ g_Opcodes + 806  ,
  /* 5a */ g_Opcodes + 807  ,
  /* 5b */ g_Opcodes + 808  ,
  /* 5c */ g_Opcodes + 809  ,
  /* 5d */ g_Opcodes + 810  ,
  /* 5e */ g_Opcodes + 811  ,
  /* 5f */ g_Opcodes + 812  ,
  /* 60 */ g_Opcodes + 813  ,
  /* 61 */ g_Opcodes + 814  ,
  /* 62 */ g_Opcodes + 815  ,
  /* 63 */ g_Opcodes + 816  ,
  /* 64 */ g_Opcodes + 817  ,
  /* 65 */ g_Opcodes + 818  ,
  /* 66 */ g_Opcodes + 819  ,
  /* 67 */ g_Opcodes + 820  ,
  /* 68 */ g_Opcodes + 821  ,
  /* 69 */ g_Opcodes + 822  ,
  /* 6a */ g_Opcodes + 823  ,
  /* 6b */ g_Opcodes + 824  ,
  /* 6c */ g_Opcodes + 825  ,
  /* 6d */ g_Opcodes + 826  ,
  /* 6e */ g_Opcodes + 827  ,
  /* 6f */ g_Opcodes + 828  ,
  /* 70 */ g_Opcodes + 829  ,
  /* 71 */ g_Opcodes + 830  ,
  /* 72 */ g_Opcodes + 831  ,
  /* 73 */ g_Opcodes + 832  ,
  /* 74 */ g_Opcodes + 833  ,
  /* 75 */ g_Opcodes + 834  ,
  /* 76 */ g_Opcodes + 835  ,
  /* 77 */ g_Opcodes + 836  ,
  /* 78 */ g_Opcodes + 837  ,
  /* 79 */ g_Opcodes + 838  ,
  /* 7a */ g_Opcodes + 839  ,
  /* 7b */ g_Opcodes + 840  ,
  /* 7c */ g_Opcodes + 841  ,
  /* 7d */ g_Opcodes + 842  ,
  /* 7e */ g_Opcodes + 843  ,
  /* 7f */ g_Opcodes + 844  ,
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
  /* ae */ g_Opcodes + 845  ,
  /* af */ NULL  ,
  /* b0 */ NULL  ,
  /* b1 */ NULL  ,
  /* b2 */ NULL  ,
  /* b3 */ NULL  ,
  /* b4 */ NULL  ,
  /* b5 */ NULL  ,
  /* b6 */ NULL  ,
  /* b7 */ NULL  ,
  /* b8 */ g_Opcodes + 846  ,
  /* b9 */ g_Opcodes + 847  ,
  /* ba */ g_Opcodes + 848  ,
  /* bb */ g_Opcodes + 849  ,
  /* bc */ g_Opcodes + 850  ,
  /* bd */ g_Opcodes + 851  ,
  /* be */ g_Opcodes + 852  ,
  /* bf */ g_Opcodes + 853  ,
  /* c0 */ NULL  ,
  /* c1 */ NULL  ,
  /* c2 */ g_Opcodes + 854  ,
  /* c3 */ g_Opcodes + 855  ,
  /* c4 */ g_Opcodes + 856  ,
  /* c5 */ g_Opcodes + 857  ,
  /* c6 */ g_Opcodes + 858  ,
  /* c7 */ NULL  ,
  /* c8 */ NULL  ,
  /* c9 */ NULL  ,
  /* ca */ NULL  ,
  /* cb */ NULL  ,
  /* cc */ NULL  ,
  /* cd */ NULL  ,
  /* ce */ NULL  ,
  /* cf */ NULL  ,
  /* d0 */ g_Opcodes + 859  ,
  /* d1 */ g_Opcodes + 860  ,
  /* d2 */ g_Opcodes + 861  ,
  /* d3 */ g_Opcodes + 862  ,
  /* d4 */ g_Opcodes + 863  ,
  /* d5 */ g_Opcodes + 864  ,
  /* d6 */ g_Opcodes + 865  ,
  /* d7 */ g_Opcodes + 866  ,
  /* d8 */ g_Opcodes + 867  ,
  /* d9 */ g_Opcodes + 868  ,
  /* da */ g_Opcodes + 869  ,
  /* db */ g_Opcodes + 870  ,
  /* dc */ g_Opcodes + 871  ,
  /* dd */ g_Opcodes + 872  ,
  /* de */ g_Opcodes + 873  ,
  /* df */ g_Opcodes + 874  ,
  /* e0 */ g_Opcodes + 875  ,
  /* e1 */ g_Opcodes + 876  ,
  /* e2 */ g_Opcodes + 877  ,
  /* e3 */ g_Opcodes + 878  ,
  /* e4 */ g_Opcodes + 879  ,
  /* e5 */ g_Opcodes + 880  ,
  /* e6 */ g_Opcodes + 881  ,
  /* e7 */ g_Opcodes + 882  ,
  /* e8 */ g_Opcodes + 883  ,
  /* e9 */ g_Opcodes + 884  ,
  /* ea */ g_Opcodes + 885  ,
  /* eb */ g_Opcodes + 886  ,
  /* ec */ g_Opcodes + 887  ,
  /* ed */ g_Opcodes + 888  ,
  /* ee */ g_Opcodes + 889  ,
  /* ef */ g_Opcodes + 890  ,
  /* f0 */ g_Opcodes + 891  ,
  /* f1 */ g_Opcodes + 892  ,
  /* f2 */ g_Opcodes + 893  ,
  /* f3 */ g_Opcodes + 894  ,
  /* f4 */ g_Opcodes + 895  ,
  /* f5 */ g_Opcodes + 896  ,
  /* f6 */ g_Opcodes + 897  ,
  /* f7 */ g_Opcodes + 898  ,
  /* f8 */ g_Opcodes + 899  ,
  /* f9 */ g_Opcodes + 900  ,
  /* fa */ g_Opcodes + 901  ,
  /* fb */ g_Opcodes + 902  ,
  /* fc */ g_Opcodes + 903  ,
  /* fd */ g_Opcodes + 904  ,
  /* fe */ g_Opcodes + 905  ,
  /* ff */ g_Opcodes + 906  ,
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
  /* 10 */ g_Opcodes + 907  ,
  /* 11 */ g_Opcodes + 908  ,
  /* 12 */ g_Opcodes + 909  ,
  /* 13 */ g_Opcodes + 910  ,
  /* 14 */ g_Opcodes + 911  ,
  /* 15 */ g_Opcodes + 912  ,
  /* 16 */ g_Opcodes + 913  ,
  /* 17 */ g_Opcodes + 914  ,
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
  /* 28 */ g_Opcodes + 915  ,
  /* 29 */ g_Opcodes + 916  ,
  /* 2a */ g_Opcodes + 917  ,
  /* 2b */ g_Opcodes + 918  ,
  /* 2c */ g_Opcodes + 919  ,
  /* 2d */ g_Opcodes + 920  ,
  /* 2e */ g_Opcodes + 921  ,
  /* 2f */ g_Opcodes + 922  ,
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
  /* 50 */ g_Opcodes + 923  ,
  /* 51 */ g_Opcodes + 924  ,
  /* 52 */ g_Opcodes + 925  ,
  /* 53 */ g_Opcodes + 926  ,
  /* 54 */ g_Opcodes + 927  ,
  /* 55 */ g_Opcodes + 928  ,
  /* 56 */ g_Opcodes + 929  ,
  /* 57 */ g_Opcodes + 930  ,
  /* 58 */ g_Opcodes + 931  ,
  /* 59 */ g_Opcodes + 932  ,
  /* 5a */ g_Opcodes + 933  ,
  /* 5b */ g_Opcodes + 934  ,
  /* 5c */ g_Opcodes + 935  ,
  /* 5d */ g_Opcodes + 936  ,
  /* 5e */ g_Opcodes + 937  ,
  /* 5f */ g_Opcodes + 938  ,
  /* 60 */ g_Opcodes + 939  ,
  /* 61 */ g_Opcodes + 940  ,
  /* 62 */ g_Opcodes + 941  ,
  /* 63 */ g_Opcodes + 942  ,
  /* 64 */ g_Opcodes + 943  ,
  /* 65 */ g_Opcodes + 944  ,
  /* 66 */ g_Opcodes + 945  ,
  /* 67 */ g_Opcodes + 946  ,
  /* 68 */ g_Opcodes + 947  ,
  /* 69 */ g_Opcodes + 948  ,
  /* 6a */ g_Opcodes + 949  ,
  /* 6b */ g_Opcodes + 950  ,
  /* 6c */ g_Opcodes + 951  ,
  /* 6d */ g_Opcodes + 952  ,
  /* 6e */ g_Opcodes + 953  ,
  /* 6f */ g_Opcodes + 954  ,
  /* 70 */ g_Opcodes + 955  ,
  /* 71 */ g_Opcodes + 956  ,
  /* 72 */ g_Opcodes + 957  ,
  /* 73 */ g_Opcodes + 958  ,
  /* 74 */ g_Opcodes + 959  ,
  /* 75 */ g_Opcodes + 960  ,
  /* 76 */ g_Opcodes + 961  ,
  /* 77 */ g_Opcodes + 962  ,
  /* 78 */ g_Opcodes + 963  ,
  /* 79 */ g_Opcodes + 964  ,
  /* 7a */ g_Opcodes + 965  ,
  /* 7b */ g_Opcodes + 966  ,
  /* 7c */ g_Opcodes + 967  ,
  /* 7d */ g_Opcodes + 968  ,
  /* 7e */ g_Opcodes + 969  ,
  /* 7f */ g_Opcodes + 970  ,
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
  /* b8 */ g_Opcodes + 971  ,
  /* b9 */ g_Opcodes + 972  ,
  /* ba */ g_Opcodes + 973  ,
  /* bb */ g_Opcodes + 974  ,
  /* bc */ g_Opcodes + 975  ,
  /* bd */ g_Opcodes + 976  ,
  /* be */ g_Opcodes + 977  ,
  /* bf */ g_Opcodes + 978  ,
  /* c0 */ NULL  ,
  /* c1 */ NULL  ,
  /* c2 */ g_Opcodes + 979  ,
  /* c3 */ g_Opcodes + 980  ,
  /* c4 */ g_Opcodes + 981  ,
  /* c5 */ g_Opcodes + 982  ,
  /* c6 */ g_Opcodes + 983  ,
  /* c7 */ NULL  ,
  /* c8 */ NULL  ,
  /* c9 */ NULL  ,
  /* ca */ NULL  ,
  /* cb */ NULL  ,
  /* cc */ NULL  ,
  /* cd */ NULL  ,
  /* ce */ NULL  ,
  /* cf */ NULL  ,
  /* d0 */ g_Opcodes + 984  ,
  /* d1 */ g_Opcodes + 985  ,
  /* d2 */ g_Opcodes + 986  ,
  /* d3 */ g_Opcodes + 987  ,
  /* d4 */ g_Opcodes + 988  ,
  /* d5 */ g_Opcodes + 989  ,
  /* d6 */ g_Opcodes + 990  ,
  /* d7 */ g_Opcodes + 991  ,
  /* d8 */ g_Opcodes + 992  ,
  /* d9 */ g_Opcodes + 993  ,
  /* da */ g_Opcodes + 994  ,
  /* db */ g_Opcodes + 995  ,
  /* dc */ g_Opcodes + 996  ,
  /* dd */ g_Opcodes + 997  ,
  /* de */ g_Opcodes + 998  ,
  /* df */ g_Opcodes + 999  ,
  /* e0 */ g_Opcodes + 1000  ,
  /* e1 */ g_Opcodes + 1001  ,
  /* e2 */ g_Opcodes + 1002  ,
  /* e3 */ g_Opcodes + 1003  ,
  /* e4 */ g_Opcodes + 1004  ,
  /* e5 */ g_Opcodes + 1005  ,
  /* e6 */ g_Opcodes + 1006  ,
  /* e7 */ g_Opcodes + 1007  ,
  /* e8 */ g_Opcodes + 1008  ,
  /* e9 */ g_Opcodes + 1009  ,
  /* ea */ g_Opcodes + 1010  ,
  /* eb */ g_Opcodes + 1011  ,
  /* ec */ g_Opcodes + 1012  ,
  /* ed */ g_Opcodes + 1013  ,
  /* ee */ g_Opcodes + 1014  ,
  /* ef */ g_Opcodes + 1015  ,
  /* f0 */ g_Opcodes + 1016  ,
  /* f1 */ g_Opcodes + 1017  ,
  /* f2 */ g_Opcodes + 1018  ,
  /* f3 */ g_Opcodes + 1019  ,
  /* f4 */ g_Opcodes + 1020  ,
  /* f5 */ g_Opcodes + 1021  ,
  /* f6 */ g_Opcodes + 1022  ,
  /* f7 */ g_Opcodes + 1023  ,
  /* f8 */ g_Opcodes + 1024  ,
  /* f9 */ g_Opcodes + 1025  ,
  /* fa */ g_Opcodes + 1026  ,
  /* fb */ g_Opcodes + 1027  ,
  /* fc */ g_Opcodes + 1028  ,
  /* fd */ g_Opcodes + 1029  ,
  /* fe */ g_Opcodes + 1030  ,
  /* ff */ g_Opcodes + 1031  ,
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
  /* 10 */ g_Opcodes + 1032  ,
  /* 11 */ g_Opcodes + 1033  ,
  /* 12 */ g_Opcodes + 1034  ,
  /* 13 */ g_Opcodes + 1035  ,
  /* 14 */ g_Opcodes + 1036  ,
  /* 15 */ g_Opcodes + 1037  ,
  /* 16 */ g_Opcodes + 1038  ,
  /* 17 */ g_Opcodes + 1039  ,
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
  /* 28 */ g_Opcodes + 1040  ,
  /* 29 */ g_Opcodes + 1041  ,
  /* 2a */ g_Opcodes + 1042  ,
  /* 2b */ g_Opcodes + 1043  ,
  /* 2c */ g_Opcodes + 1044  ,
  /* 2d */ g_Opcodes + 1045  ,
  /* 2e */ g_Opcodes + 1046  ,
  /* 2f */ g_Opcodes + 1047  ,
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
  /* 50 */ g_Opcodes + 1048  ,
  /* 51 */ g_Opcodes + 1049  ,
  /* 52 */ g_Opcodes + 1050  ,
  /* 53 */ g_Opcodes + 1051  ,
  /* 54 */ g_Opcodes + 1052  ,
  /* 55 */ g_Opcodes + 1053  ,
  /* 56 */ g_Opcodes + 1054  ,
  /* 57 */ g_Opcodes + 1055  ,
  /* 58 */ g_Opcodes + 1056  ,
  /* 59 */ g_Opcodes + 1057  ,
  /* 5a */ g_Opcodes + 1058  ,
  /* 5b */ g_Opcodes + 1059  ,
  /* 5c */ g_Opcodes + 1060  ,
  /* 5d */ g_Opcodes + 1061  ,
  /* 5e */ g_Opcodes + 1062  ,
  /* 5f */ g_Opcodes + 1063  ,
  /* 60 */ g_Opcodes + 1064  ,
  /* 61 */ g_Opcodes + 1065  ,
  /* 62 */ g_Opcodes + 1066  ,
  /* 63 */ g_Opcodes + 1067  ,
  /* 64 */ g_Opcodes + 1068  ,
  /* 65 */ g_Opcodes + 1069  ,
  /* 66 */ g_Opcodes + 1070  ,
  /* 67 */ g_Opcodes + 1071  ,
  /* 68 */ g_Opcodes + 1072  ,
  /* 69 */ g_Opcodes + 1073  ,
  /* 6a */ g_Opcodes + 1074  ,
  /* 6b */ g_Opcodes + 1075  ,
  /* 6c */ g_Opcodes + 1076  ,
  /* 6d */ g_Opcodes + 1077  ,
  /* 6e */ g_Opcodes + 1078  ,
  /* 6f */ g_Opcodes + 1079  ,
  /* 70 */ g_Opcodes + 1080  ,
  /* 71 */ g_Opcodes + 1081  ,
  /* 72 */ g_Opcodes + 1089  ,
  /* 73 */ g_Opcodes + 1097  ,
  /* 74 */ g_Opcodes + 1105  ,
  /* 75 */ g_Opcodes + 1106  ,
  /* 76 */ g_Opcodes + 1107  ,
  /* 77 */ g_Opcodes + 1108  ,
  /* 78 */ g_Opcodes + 1109  ,
  /* 79 */ g_Opcodes + 1111  ,
  /* 7a */ g_Opcodes + 1112  ,
  /* 7b */ g_Opcodes + 1113  ,
  /* 7c */ g_Opcodes + 1114  ,
  /* 7d */ g_Opcodes + 1115  ,
  /* 7e */ g_Opcodes + 1116  ,
  /* 7f */ g_Opcodes + 1117  ,
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
  /* ae */ g_Opcodes + 1118  ,
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
  /* c2 */ g_Opcodes + 1119  ,
  /* c3 */ g_Opcodes + 1120  ,
  /* c4 */ g_Opcodes + 1121  ,
  /* c5 */ g_Opcodes + 1122  ,
  /* c6 */ g_Opcodes + 1123  ,
  /* c7 */ NULL  ,
  /* c8 */ NULL  ,
  /* c9 */ NULL  ,
  /* ca */ NULL  ,
  /* cb */ NULL  ,
  /* cc */ NULL  ,
  /* cd */ NULL  ,
  /* ce */ NULL  ,
  /* cf */ NULL  ,
  /* d0 */ g_Opcodes + 1124  ,
  /* d1 */ g_Opcodes + 1125  ,
  /* d2 */ g_Opcodes + 1126  ,
  /* d3 */ g_Opcodes + 1127  ,
  /* d4 */ g_Opcodes + 1128  ,
  /* d5 */ g_Opcodes + 1129  ,
  /* d6 */ g_Opcodes + 1130  ,
  /* d7 */ g_Opcodes + 1131  ,
  /* d8 */ g_Opcodes + 1132  ,
  /* d9 */ g_Opcodes + 1133  ,
  /* da */ g_Opcodes + 1134  ,
  /* db */ g_Opcodes + 1135  ,
  /* dc */ g_Opcodes + 1136  ,
  /* dd */ g_Opcodes + 1137  ,
  /* de */ g_Opcodes + 1138  ,
  /* df */ g_Opcodes + 1139  ,
  /* e0 */ g_Opcodes + 1140  ,
  /* e1 */ g_Opcodes + 1141  ,
  /* e2 */ g_Opcodes + 1142  ,
  /* e3 */ g_Opcodes + 1143  ,
  /* e4 */ g_Opcodes + 1144  ,
  /* e5 */ g_Opcodes + 1145  ,
  /* e6 */ g_Opcodes + 1146  ,
  /* e7 */ g_Opcodes + 1147  ,
  /* e8 */ g_Opcodes + 1148  ,
  /* e9 */ g_Opcodes + 1149  ,
  /* ea */ g_Opcodes + 1150  ,
  /* eb */ g_Opcodes + 1151  ,
  /* ec */ g_Opcodes + 1152  ,
  /* ed */ g_Opcodes + 1153  ,
  /* ee */ g_Opcodes + 1154  ,
  /* ef */ g_Opcodes + 1155  ,
  /* f0 */ g_Opcodes + 1156  ,
  /* f1 */ g_Opcodes + 1157  ,
  /* f2 */ g_Opcodes + 1158  ,
  /* f3 */ g_Opcodes + 1159  ,
  /* f4 */ g_Opcodes + 1160  ,
  /* f5 */ g_Opcodes + 1161  ,
  /* f6 */ g_Opcodes + 1162  ,
  /* f7 */ g_Opcodes + 1163  ,
  /* f8 */ g_Opcodes + 1164  ,
  /* f9 */ g_Opcodes + 1165  ,
  /* fa */ g_Opcodes + 1166  ,
  /* fb */ g_Opcodes + 1167  ,
  /* fc */ g_Opcodes + 1168  ,
  /* fd */ g_Opcodes + 1169  ,
  /* fe */ g_Opcodes + 1170  ,
  /* ff */ g_Opcodes + 1171  ,
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
  /* 0c */ g_Opcodes + 1172  ,
  /* 0d */ g_Opcodes + 1173  ,
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
  /* 1c */ g_Opcodes + 1174  ,
  /* 1d */ g_Opcodes + 1175  ,
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
  /* 8a */ g_Opcodes + 1176  ,
  /* 8b */ NULL  ,
  /* 8c */ NULL  ,
  /* 8d */ NULL  ,
  /* 8e */ g_Opcodes + 1177  ,
  /* 8f */ NULL  ,
  /* 90 */ g_Opcodes + 1178  ,
  /* 91 */ NULL  ,
  /* 92 */ NULL  ,
  /* 93 */ NULL  ,
  /* 94 */ g_Opcodes + 1179  ,
  /* 95 */ NULL  ,
  /* 96 */ g_Opcodes + 1180  ,
  /* 97 */ g_Opcodes + 1181  ,
  /* 98 */ NULL  ,
  /* 99 */ NULL  ,
  /* 9a */ g_Opcodes + 1182  ,
  /* 9b */ NULL  ,
  /* 9c */ NULL  ,
  /* 9d */ NULL  ,
  /* 9e */ g_Opcodes + 1183  ,
  /* 9f */ NULL  ,
  /* a0 */ g_Opcodes + 1184  ,
  /* a1 */ NULL  ,
  /* a2 */ NULL  ,
  /* a3 */ NULL  ,
  /* a4 */ g_Opcodes + 1185  ,
  /* a5 */ NULL  ,
  /* a6 */ g_Opcodes + 1186  ,
  /* a7 */ g_Opcodes + 1187  ,
  /* a8 */ NULL  ,
  /* a9 */ NULL  ,
  /* aa */ g_Opcodes + 1188  ,
  /* ab */ NULL  ,
  /* ac */ NULL  ,
  /* ad */ NULL  ,
  /* ae */ g_Opcodes + 1189  ,
  /* af */ NULL  ,
  /* b0 */ g_Opcodes + 1190  ,
  /* b1 */ NULL  ,
  /* b2 */ NULL  ,
  /* b3 */ NULL  ,
  /* b4 */ g_Opcodes + 1191  ,
  /* b5 */ NULL  ,
  /* b6 */ g_Opcodes + 1192  ,
  /* b7 */ g_Opcodes + 1193  ,
  /* b8 */ NULL  ,
  /* b9 */ NULL  ,
  /* ba */ NULL  ,
  /* bb */ g_Opcodes + 1194  ,
  /* bc */ NULL  ,
  /* bd */ NULL  ,
  /* be */ NULL  ,
  /* bf */ g_Opcodes + 1195  ,
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
  /* 00 */ g_Opcodes + 1196  ,
  /* 01 */ g_Opcodes + 1197  ,
  /* 02 */ g_Opcodes + 1198  ,
  /* 03 */ g_Opcodes + 1199  ,
  /* 04 */ g_Opcodes + 1200  ,
  /* 05 */ g_Opcodes + 1201  ,
  /* 06 */ g_Opcodes + 1202  ,
  /* 07 */ g_Opcodes + 1203  ,
  /* 08 */ g_Opcodes + 1204  ,
  /* 09 */ g_Opcodes + 1205  ,
  /* 0a */ g_Opcodes + 1206  ,
  /* 0b */ g_Opcodes + 1207  ,
  /* 0c */ g_Opcodes + 1208  ,
  /* 0d */ g_Opcodes + 1209  ,
  /* 0e */ g_Opcodes + 1210  ,
  /* 0f */ g_Opcodes + 1211  ,
  /* 10 */ g_Opcodes + 1212  ,
  /* 11 */ g_Opcodes + 1213  ,
  /* 12 */ g_Opcodes + 1214  ,
  /* 13 */ g_Opcodes + 1215  ,
  /* 14 */ g_Opcodes + 1216  ,
  /* 15 */ g_Opcodes + 1217  ,
  /* 16 */ g_Opcodes + 1218  ,
  /* 17 */ g_Opcodes + 1219  ,
  /* 18 */ g_Opcodes + 1220  ,
  /* 19 */ g_Opcodes + 1221  ,
  /* 1a */ g_Opcodes + 1222  ,
  /* 1b */ g_Opcodes + 1223  ,
  /* 1c */ g_Opcodes + 1224  ,
  /* 1d */ g_Opcodes + 1225  ,
  /* 1e */ g_Opcodes + 1226  ,
  /* 1f */ g_Opcodes + 1227  ,
  /* 20 */ g_Opcodes + 1228  ,
  /* 21 */ g_Opcodes + 1229  ,
  /* 22 */ g_Opcodes + 1230  ,
  /* 23 */ g_Opcodes + 1231  ,
  /* 24 */ g_Opcodes + 1232  ,
  /* 25 */ g_Opcodes + 1233  ,
  /* 26 */ g_Opcodes + 1234  ,
  /* 27 */ g_Opcodes + 1235  ,
  /* 28 */ g_Opcodes + 1236  ,
  /* 29 */ g_Opcodes + 1237  ,
  /* 2a */ g_Opcodes + 1238  ,
  /* 2b */ g_Opcodes + 1239  ,
  /* 2c */ g_Opcodes + 1240  ,
  /* 2d */ g_Opcodes + 1241  ,
  /* 2e */ g_Opcodes + 1242  ,
  /* 2f */ g_Opcodes + 1243  ,
  /* 30 */ g_Opcodes + 1244  ,
  /* 31 */ g_Opcodes + 1245  ,
  /* 32 */ g_Opcodes + 1246  ,
  /* 33 */ g_Opcodes + 1247  ,
  /* 34 */ g_Opcodes + 1248  ,
  /* 35 */ g_Opcodes + 1249  ,
  /* 36 */ g_Opcodes + 1250  ,
  /* 37 */ g_Opcodes + 1251  ,
  /* 38 */ g_Opcodes + 1252  ,
  /* 39 */ g_Opcodes + 1253  ,
  /* 3a */ g_Opcodes + 1254  ,
  /* 3b */ g_Opcodes + 1255  ,
  /* 3c */ g_Opcodes + 1256  ,
  /* 3d */ g_Opcodes + 1257  ,
  /* 3e */ g_Opcodes + 1258  ,
  /* 3f */ g_Opcodes + 1259  ,
  /* 40 */ g_Opcodes + 1260  ,
  /* 41 */ g_Opcodes + 1261  ,
  /* 42 */ g_Opcodes + 1262  ,
  /* 43 */ g_Opcodes + 1263  ,
  /* 44 */ g_Opcodes + 1264  ,
  /* 45 */ g_Opcodes + 1265  ,
  /* 46 */ g_Opcodes + 1266  ,
  /* 47 */ g_Opcodes + 1267  ,
  /* 48 */ g_Opcodes + 1268  ,
  /* 49 */ g_Opcodes + 1269  ,
  /* 4a */ g_Opcodes + 1270  ,
  /* 4b */ g_Opcodes + 1271  ,
  /* 4c */ g_Opcodes + 1272  ,
  /* 4d */ g_Opcodes + 1273  ,
  /* 4e */ g_Opcodes + 1274  ,
  /* 4f */ g_Opcodes + 1275  ,
  /* 50 */ g_Opcodes + 1276  ,
  /* 51 */ g_Opcodes + 1277  ,
  /* 52 */ g_Opcodes + 1278  ,
  /* 53 */ g_Opcodes + 1279  ,
  /* 54 */ g_Opcodes + 1280  ,
  /* 55 */ g_Opcodes + 1281  ,
  /* 56 */ g_Opcodes + 1282  ,
  /* 57 */ g_Opcodes + 1283  ,
  /* 58 */ g_Opcodes + 1284  ,
  /* 59 */ g_Opcodes + 1285  ,
  /* 5a */ g_Opcodes + 1286  ,
  /* 5b */ g_Opcodes + 1287  ,
  /* 5c */ g_Opcodes + 1288  ,
  /* 5d */ g_Opcodes + 1289  ,
  /* 5e */ g_Opcodes + 1290  ,
  /* 5f */ g_Opcodes + 1291  ,
  /* 60 */ g_Opcodes + 1292  ,
  /* 61 */ g_Opcodes + 1293  ,
  /* 62 */ g_Opcodes + 1294  ,
  /* 63 */ g_Opcodes + 1295  ,
  /* 64 */ g_Opcodes + 1296  ,
  /* 65 */ g_Opcodes + 1297  ,
  /* 66 */ g_Opcodes + 1298  ,
  /* 67 */ g_Opcodes + 1299  ,
  /* 68 */ g_Opcodes + 1300  ,
  /* 69 */ g_Opcodes + 1301  ,
  /* 6a */ g_Opcodes + 1302  ,
  /* 6b */ g_Opcodes + 1303  ,
  /* 6c */ g_Opcodes + 1304  ,
  /* 6d */ g_Opcodes + 1305  ,
  /* 6e */ g_Opcodes + 1306  ,
  /* 6f */ g_Opcodes + 1307  ,
  /* 70 */ g_Opcodes + 1308  ,
  /* 71 */ g_Opcodes + 1309  ,
  /* 72 */ g_Opcodes + 1310  ,
  /* 73 */ g_Opcodes + 1311  ,
  /* 74 */ g_Opcodes + 1312  ,
  /* 75 */ g_Opcodes + 1313  ,
  /* 76 */ g_Opcodes + 1314  ,
  /* 77 */ g_Opcodes + 1315  ,
  /* 78 */ g_Opcodes + 1316  ,
  /* 79 */ g_Opcodes + 1317  ,
  /* 7a */ g_Opcodes + 1318  ,
  /* 7b */ g_Opcodes + 1319  ,
  /* 7c */ g_Opcodes + 1320  ,
  /* 7d */ g_Opcodes + 1321  ,
  /* 7e */ g_Opcodes + 1322  ,
  /* 7f */ g_Opcodes + 1323  ,
  /* 80 */ g_Opcodes + 1324  ,
  /* 81 */ g_Opcodes + 1325  ,
  /* 82 */ g_Opcodes + 1326  ,
  /* 83 */ g_Opcodes + 1327  ,
  /* 84 */ g_Opcodes + 1328  ,
  /* 85 */ g_Opcodes + 1329  ,
  /* 86 */ g_Opcodes + 1330  ,
  /* 87 */ g_Opcodes + 1331  ,
  /* 88 */ g_Opcodes + 1332  ,
  /* 89 */ g_Opcodes + 1333  ,
  /* 8a */ g_Opcodes + 1334  ,
  /* 8b */ g_Opcodes + 1335  ,
  /* 8c */ g_Opcodes + 1336  ,
  /* 8d */ g_Opcodes + 1337  ,
  /* 8e */ g_Opcodes + 1338  ,
  /* 8f */ g_Opcodes + 1339  ,
  /* 90 */ g_Opcodes + 1340  ,
  /* 91 */ g_Opcodes + 1341  ,
  /* 92 */ g_Opcodes + 1342  ,
  /* 93 */ g_Opcodes + 1343  ,
  /* 94 */ g_Opcodes + 1344  ,
  /* 95 */ g_Opcodes + 1345  ,
  /* 96 */ g_Opcodes + 1346  ,
  /* 97 */ g_Opcodes + 1347  ,
  /* 98 */ g_Opcodes + 1348  ,
  /* 99 */ g_Opcodes + 1349  ,
  /* 9a */ g_Opcodes + 1350  ,
  /* 9b */ g_Opcodes + 1351  ,
  /* 9c */ g_Opcodes + 1352  ,
  /* 9d */ g_Opcodes + 1353  ,
  /* 9e */ g_Opcodes + 1354  ,
  /* 9f */ g_Opcodes + 1355  ,
  /* a0 */ g_Opcodes + 1356  ,
  /* a1 */ g_Opcodes + 1357  ,
  /* a2 */ g_Opcodes + 1358  ,
  /* a3 */ g_Opcodes + 1359  ,
  /* a4 */ g_Opcodes + 1360  ,
  /* a5 */ g_Opcodes + 1361  ,
  /* a6 */ g_Opcodes + 1362  ,
  /* a7 */ g_Opcodes + 1363  ,
  /* a8 */ g_Opcodes + 1364  ,
  /* a9 */ g_Opcodes + 1365  ,
  /* aa */ g_Opcodes + 1366  ,
  /* ab */ g_Opcodes + 1367  ,
  /* ac */ g_Opcodes + 1368  ,
  /* ad */ g_Opcodes + 1369  ,
  /* ae */ g_Opcodes + 1370  ,
  /* af */ g_Opcodes + 1371  ,
  /* b0 */ g_Opcodes + 1372  ,
  /* b1 */ g_Opcodes + 1373  ,
  /* b2 */ g_Opcodes + 1374  ,
  /* b3 */ g_Opcodes + 1375  ,
  /* b4 */ g_Opcodes + 1376  ,
  /* b5 */ g_Opcodes + 1377  ,
  /* b6 */ g_Opcodes + 1378  ,
  /* b7 */ g_Opcodes + 1379  ,
  /* b8 */ g_Opcodes + 1380  ,
  /* b9 */ g_Opcodes + 1381  ,
  /* ba */ g_Opcodes + 1382  ,
  /* bb */ g_Opcodes + 1383  ,
  /* bc */ g_Opcodes + 1384  ,
  /* bd */ g_Opcodes + 1385  ,
  /* be */ g_Opcodes + 1386  ,
  /* bf */ g_Opcodes + 1387  ,
  /* c0 */ g_Opcodes + 1388  ,
  /* c1 */ g_Opcodes + 1389  ,
  /* c2 */ g_Opcodes + 1390  ,
  /* c3 */ g_Opcodes + 1391  ,
  /* c4 */ g_Opcodes + 1392  ,
  /* c5 */ g_Opcodes + 1393  ,
  /* c6 */ g_Opcodes + 1394  ,
  /* c7 */ g_Opcodes + 1395  ,
  /* c8 */ g_Opcodes + 1396  ,
  /* c9 */ g_Opcodes + 1397  ,
  /* ca */ g_Opcodes + 1398  ,
  /* cb */ g_Opcodes + 1399  ,
  /* cc */ g_Opcodes + 1400  ,
  /* cd */ g_Opcodes + 1401  ,
  /* ce */ g_Opcodes + 1402  ,
  /* cf */ g_Opcodes + 1403  ,
  /* d0 */ g_Opcodes + 1404  ,
  /* d1 */ g_Opcodes + 1405  ,
  /* d2 */ g_Opcodes + 1406  ,
  /* d3 */ g_Opcodes + 1407  ,
  /* d4 */ g_Opcodes + 1408  ,
  /* d5 */ g_Opcodes + 1409  ,
  /* d6 */ g_Opcodes + 1410  ,
  /* d7 */ g_Opcodes + 1411  ,
  /* d8 */ g_Opcodes + 1412  ,
  /* d9 */ g_Opcodes + 1413  ,
  /* da */ g_Opcodes + 1414  ,
  /* db */ g_Opcodes + 1415  ,
  /* dc */ g_Opcodes + 1416  ,
  /* dd */ g_Opcodes + 1417  ,
  /* de */ g_Opcodes + 1418  ,
  /* df */ g_Opcodes + 1419  ,
  /* e0 */ g_Opcodes + 1420  ,
  /* e1 */ g_Opcodes + 1421  ,
  /* e2 */ g_Opcodes + 1422  ,
  /* e3 */ g_Opcodes + 1423  ,
  /* e4 */ g_Opcodes + 1424  ,
  /* e5 */ g_Opcodes + 1425  ,
  /* e6 */ g_Opcodes + 1426  ,
  /* e7 */ g_Opcodes + 1427  ,
  /* e8 */ g_Opcodes + 1428  ,
  /* e9 */ g_Opcodes + 1429  ,
  /* ea */ g_Opcodes + 1430  ,
  /* eb */ g_Opcodes + 1431  ,
  /* ec */ g_Opcodes + 1432  ,
  /* ed */ g_Opcodes + 1433  ,
  /* ee */ g_Opcodes + 1434  ,
  /* ef */ g_Opcodes + 1435  ,
  /* f0 */ g_Opcodes + 1436  ,
  /* f1 */ g_Opcodes + 1437  ,
  /* f2 */ g_Opcodes + 1438  ,
  /* f3 */ g_Opcodes + 1439  ,
  /* f4 */ g_Opcodes + 1440  ,
  /* f5 */ g_Opcodes + 1441  ,
  /* f6 */ g_Opcodes + 1442  ,
  /* f7 */ g_Opcodes + 1443  ,
  /* f8 */ g_Opcodes + 1444  ,
  /* f9 */ g_Opcodes + 1445  ,
  /* fa */ g_Opcodes + 1446  ,
  /* fb */ g_Opcodes + 1447  ,
  /* fc */ g_Opcodes + 1448  ,
  /* fd */ g_Opcodes + 1449  ,
  /* fe */ g_Opcodes + 1450  ,
  /* ff */ g_Opcodes + 1451  ,
},
/* Prefix660F38 */
{
  /* 00 */ g_Opcodes + 1452  ,
  /* 01 */ g_Opcodes + 1453  ,
  /* 02 */ g_Opcodes + 1454  ,
  /* 03 */ g_Opcodes + 1455  ,
  /* 04 */ g_Opcodes + 1456  ,
  /* 05 */ g_Opcodes + 1457  ,
  /* 06 */ g_Opcodes + 1458  ,
  /* 07 */ g_Opcodes + 1459  ,
  /* 08 */ g_Opcodes + 1460  ,
  /* 09 */ g_Opcodes + 1461  ,
  /* 0a */ g_Opcodes + 1462  ,
  /* 0b */ g_Opcodes + 1463  ,
  /* 0c */ g_Opcodes + 1464  ,
  /* 0d */ g_Opcodes + 1465  ,
  /* 0e */ g_Opcodes + 1466  ,
  /* 0f */ g_Opcodes + 1467  ,
  /* 10 */ g_Opcodes + 1468  ,
  /* 11 */ g_Opcodes + 1469  ,
  /* 12 */ g_Opcodes + 1470  ,
  /* 13 */ g_Opcodes + 1471  ,
  /* 14 */ g_Opcodes + 1472  ,
  /* 15 */ g_Opcodes + 1473  ,
  /* 16 */ g_Opcodes + 1474  ,
  /* 17 */ g_Opcodes + 1475  ,
  /* 18 */ g_Opcodes + 1476  ,
  /* 19 */ g_Opcodes + 1477  ,
  /* 1a */ g_Opcodes + 1478  ,
  /* 1b */ g_Opcodes + 1479  ,
  /* 1c */ g_Opcodes + 1480  ,
  /* 1d */ g_Opcodes + 1481  ,
  /* 1e */ g_Opcodes + 1482  ,
  /* 1f */ g_Opcodes + 1483  ,
  /* 20 */ g_Opcodes + 1484  ,
  /* 21 */ g_Opcodes + 1485  ,
  /* 22 */ g_Opcodes + 1486  ,
  /* 23 */ g_Opcodes + 1487  ,
  /* 24 */ g_Opcodes + 1488  ,
  /* 25 */ g_Opcodes + 1489  ,
  /* 26 */ g_Opcodes + 1490  ,
  /* 27 */ g_Opcodes + 1491  ,
  /* 28 */ g_Opcodes + 1492  ,
  /* 29 */ g_Opcodes + 1493  ,
  /* 2a */ g_Opcodes + 1494  ,
  /* 2b */ g_Opcodes + 1495  ,
  /* 2c */ g_Opcodes + 1496  ,
  /* 2d */ g_Opcodes + 1497  ,
  /* 2e */ g_Opcodes + 1498  ,
  /* 2f */ g_Opcodes + 1499  ,
  /* 30 */ g_Opcodes + 1500  ,
  /* 31 */ g_Opcodes + 1501  ,
  /* 32 */ g_Opcodes + 1502  ,
  /* 33 */ g_Opcodes + 1503  ,
  /* 34 */ g_Opcodes + 1504  ,
  /* 35 */ g_Opcodes + 1505  ,
  /* 36 */ g_Opcodes + 1506  ,
  /* 37 */ g_Opcodes + 1507  ,
  /* 38 */ g_Opcodes + 1508  ,
  /* 39 */ g_Opcodes + 1509  ,
  /* 3a */ g_Opcodes + 1510  ,
  /* 3b */ g_Opcodes + 1511  ,
  /* 3c */ g_Opcodes + 1512  ,
  /* 3d */ g_Opcodes + 1513  ,
  /* 3e */ g_Opcodes + 1514  ,
  /* 3f */ g_Opcodes + 1515  ,
  /* 40 */ g_Opcodes + 1516  ,
  /* 41 */ g_Opcodes + 1517  ,
  /* 42 */ g_Opcodes + 1518  ,
  /* 43 */ g_Opcodes + 1519  ,
  /* 44 */ g_Opcodes + 1520  ,
  /* 45 */ g_Opcodes + 1521  ,
  /* 46 */ g_Opcodes + 1522  ,
  /* 47 */ g_Opcodes + 1523  ,
  /* 48 */ g_Opcodes + 1524  ,
  /* 49 */ g_Opcodes + 1525  ,
  /* 4a */ g_Opcodes + 1526  ,
  /* 4b */ g_Opcodes + 1527  ,
  /* 4c */ g_Opcodes + 1528  ,
  /* 4d */ g_Opcodes + 1529  ,
  /* 4e */ g_Opcodes + 1530  ,
  /* 4f */ g_Opcodes + 1531  ,
  /* 50 */ g_Opcodes + 1532  ,
  /* 51 */ g_Opcodes + 1533  ,
  /* 52 */ g_Opcodes + 1534  ,
  /* 53 */ g_Opcodes + 1535  ,
  /* 54 */ g_Opcodes + 1536  ,
  /* 55 */ g_Opcodes + 1537  ,
  /* 56 */ g_Opcodes + 1538  ,
  /* 57 */ g_Opcodes + 1539  ,
  /* 58 */ g_Opcodes + 1540  ,
  /* 59 */ g_Opcodes + 1541  ,
  /* 5a */ g_Opcodes + 1542  ,
  /* 5b */ g_Opcodes + 1543  ,
  /* 5c */ g_Opcodes + 1544  ,
  /* 5d */ g_Opcodes + 1545  ,
  /* 5e */ g_Opcodes + 1546  ,
  /* 5f */ g_Opcodes + 1547  ,
  /* 60 */ g_Opcodes + 1548  ,
  /* 61 */ g_Opcodes + 1549  ,
  /* 62 */ g_Opcodes + 1550  ,
  /* 63 */ g_Opcodes + 1551  ,
  /* 64 */ g_Opcodes + 1552  ,
  /* 65 */ g_Opcodes + 1553  ,
  /* 66 */ g_Opcodes + 1554  ,
  /* 67 */ g_Opcodes + 1555  ,
  /* 68 */ g_Opcodes + 1556  ,
  /* 69 */ g_Opcodes + 1557  ,
  /* 6a */ g_Opcodes + 1558  ,
  /* 6b */ g_Opcodes + 1559  ,
  /* 6c */ g_Opcodes + 1560  ,
  /* 6d */ g_Opcodes + 1561  ,
  /* 6e */ g_Opcodes + 1562  ,
  /* 6f */ g_Opcodes + 1563  ,
  /* 70 */ g_Opcodes + 1564  ,
  /* 71 */ g_Opcodes + 1565  ,
  /* 72 */ g_Opcodes + 1566  ,
  /* 73 */ g_Opcodes + 1567  ,
  /* 74 */ g_Opcodes + 1568  ,
  /* 75 */ g_Opcodes + 1569  ,
  /* 76 */ g_Opcodes + 1570  ,
  /* 77 */ g_Opcodes + 1571  ,
  /* 78 */ g_Opcodes + 1572  ,
  /* 79 */ g_Opcodes + 1573  ,
  /* 7a */ g_Opcodes + 1574  ,
  /* 7b */ g_Opcodes + 1575  ,
  /* 7c */ g_Opcodes + 1576  ,
  /* 7d */ g_Opcodes + 1577  ,
  /* 7e */ g_Opcodes + 1578  ,
  /* 7f */ g_Opcodes + 1579  ,
  /* 80 */ g_Opcodes + 1580  ,
  /* 81 */ g_Opcodes + 1581  ,
  /* 82 */ g_Opcodes + 1582  ,
  /* 83 */ g_Opcodes + 1583  ,
  /* 84 */ g_Opcodes + 1584  ,
  /* 85 */ g_Opcodes + 1585  ,
  /* 86 */ g_Opcodes + 1586  ,
  /* 87 */ g_Opcodes + 1587  ,
  /* 88 */ g_Opcodes + 1588  ,
  /* 89 */ g_Opcodes + 1589  ,
  /* 8a */ g_Opcodes + 1590  ,
  /* 8b */ g_Opcodes + 1591  ,
  /* 8c */ g_Opcodes + 1592  ,
  /* 8d */ g_Opcodes + 1593  ,
  /* 8e */ g_Opcodes + 1594  ,
  /* 8f */ g_Opcodes + 1595  ,
  /* 90 */ g_Opcodes + 1596  ,
  /* 91 */ g_Opcodes + 1597  ,
  /* 92 */ g_Opcodes + 1598  ,
  /* 93 */ g_Opcodes + 1599  ,
  /* 94 */ g_Opcodes + 1600  ,
  /* 95 */ g_Opcodes + 1601  ,
  /* 96 */ g_Opcodes + 1602  ,
  /* 97 */ g_Opcodes + 1603  ,
  /* 98 */ g_Opcodes + 1604  ,
  /* 99 */ g_Opcodes + 1605  ,
  /* 9a */ g_Opcodes + 1606  ,
  /* 9b */ g_Opcodes + 1607  ,
  /* 9c */ g_Opcodes + 1608  ,
  /* 9d */ g_Opcodes + 1609  ,
  /* 9e */ g_Opcodes + 1610  ,
  /* 9f */ g_Opcodes + 1611  ,
  /* a0 */ g_Opcodes + 1612  ,
  /* a1 */ g_Opcodes + 1613  ,
  /* a2 */ g_Opcodes + 1614  ,
  /* a3 */ g_Opcodes + 1615  ,
  /* a4 */ g_Opcodes + 1616  ,
  /* a5 */ g_Opcodes + 1617  ,
  /* a6 */ g_Opcodes + 1618  ,
  /* a7 */ g_Opcodes + 1619  ,
  /* a8 */ g_Opcodes + 1620  ,
  /* a9 */ g_Opcodes + 1621  ,
  /* aa */ g_Opcodes + 1622  ,
  /* ab */ g_Opcodes + 1623  ,
  /* ac */ g_Opcodes + 1624  ,
  /* ad */ g_Opcodes + 1625  ,
  /* ae */ g_Opcodes + 1626  ,
  /* af */ g_Opcodes + 1627  ,
  /* b0 */ g_Opcodes + 1628  ,
  /* b1 */ g_Opcodes + 1629  ,
  /* b2 */ g_Opcodes + 1630  ,
  /* b3 */ g_Opcodes + 1631  ,
  /* b4 */ g_Opcodes + 1632  ,
  /* b5 */ g_Opcodes + 1633  ,
  /* b6 */ g_Opcodes + 1634  ,
  /* b7 */ g_Opcodes + 1635  ,
  /* b8 */ g_Opcodes + 1636  ,
  /* b9 */ g_Opcodes + 1637  ,
  /* ba */ g_Opcodes + 1638  ,
  /* bb */ g_Opcodes + 1639  ,
  /* bc */ g_Opcodes + 1640  ,
  /* bd */ g_Opcodes + 1641  ,
  /* be */ g_Opcodes + 1642  ,
  /* bf */ g_Opcodes + 1643  ,
  /* c0 */ g_Opcodes + 1644  ,
  /* c1 */ g_Opcodes + 1645  ,
  /* c2 */ g_Opcodes + 1646  ,
  /* c3 */ g_Opcodes + 1647  ,
  /* c4 */ g_Opcodes + 1648  ,
  /* c5 */ g_Opcodes + 1649  ,
  /* c6 */ g_Opcodes + 1650  ,
  /* c7 */ g_Opcodes + 1651  ,
  /* c8 */ g_Opcodes + 1652  ,
  /* c9 */ g_Opcodes + 1653  ,
  /* ca */ g_Opcodes + 1654  ,
  /* cb */ g_Opcodes + 1655  ,
  /* cc */ g_Opcodes + 1656  ,
  /* cd */ g_Opcodes + 1657  ,
  /* ce */ g_Opcodes + 1658  ,
  /* cf */ g_Opcodes + 1659  ,
  /* d0 */ g_Opcodes + 1660  ,
  /* d1 */ g_Opcodes + 1661  ,
  /* d2 */ g_Opcodes + 1662  ,
  /* d3 */ g_Opcodes + 1663  ,
  /* d4 */ g_Opcodes + 1664  ,
  /* d5 */ g_Opcodes + 1665  ,
  /* d6 */ g_Opcodes + 1666  ,
  /* d7 */ g_Opcodes + 1667  ,
  /* d8 */ g_Opcodes + 1668  ,
  /* d9 */ g_Opcodes + 1669  ,
  /* da */ g_Opcodes + 1670  ,
  /* db */ g_Opcodes + 1671  ,
  /* dc */ g_Opcodes + 1672  ,
  /* dd */ g_Opcodes + 1673  ,
  /* de */ g_Opcodes + 1674  ,
  /* df */ g_Opcodes + 1675  ,
  /* e0 */ g_Opcodes + 1676  ,
  /* e1 */ g_Opcodes + 1677  ,
  /* e2 */ g_Opcodes + 1678  ,
  /* e3 */ g_Opcodes + 1679  ,
  /* e4 */ g_Opcodes + 1680  ,
  /* e5 */ g_Opcodes + 1681  ,
  /* e6 */ g_Opcodes + 1682  ,
  /* e7 */ g_Opcodes + 1683  ,
  /* e8 */ g_Opcodes + 1684  ,
  /* e9 */ g_Opcodes + 1685  ,
  /* ea */ g_Opcodes + 1686  ,
  /* eb */ g_Opcodes + 1687  ,
  /* ec */ g_Opcodes + 1688  ,
  /* ed */ g_Opcodes + 1689  ,
  /* ee */ g_Opcodes + 1690  ,
  /* ef */ g_Opcodes + 1691  ,
  /* f0 */ NULL  ,
  /* f1 */ NULL  ,
  /* f2 */ g_Opcodes + 1692  ,
  /* f3 */ g_Opcodes + 1693  ,
  /* f4 */ g_Opcodes + 1694  ,
  /* f5 */ g_Opcodes + 1695  ,
  /* f6 */ g_Opcodes + 1696  ,
  /* f7 */ g_Opcodes + 1697  ,
  /* f8 */ g_Opcodes + 1698  ,
  /* f9 */ g_Opcodes + 1699  ,
  /* fa */ g_Opcodes + 1700  ,
  /* fb */ g_Opcodes + 1701  ,
  /* fc */ g_Opcodes + 1702  ,
  /* fd */ g_Opcodes + 1703  ,
  /* fe */ g_Opcodes + 1704  ,
  /* ff */ g_Opcodes + 1705  ,
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
  /* f0 */ g_Opcodes + 1706  ,
  /* f1 */ g_Opcodes + 1707  ,
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
  /* 0f */ g_Opcodes + 1708  ,
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
  /* 08 */ g_Opcodes + 1709  ,
  /* 09 */ g_Opcodes + 1710  ,
  /* 0a */ g_Opcodes + 1711  ,
  /* 0b */ g_Opcodes + 1712  ,
  /* 0c */ g_Opcodes + 1713  ,
  /* 0d */ g_Opcodes + 1714  ,
  /* 0e */ g_Opcodes + 1715  ,
  /* 0f */ g_Opcodes + 1716  ,
  /* 10 */ NULL  ,
  /* 11 */ NULL  ,
  /* 12 */ NULL  ,
  /* 13 */ NULL  ,
  /* 14 */ g_Opcodes + 1717  ,
  /* 15 */ g_Opcodes + 1718  ,
  /* 16 */ g_Opcodes + 1719  ,
  /* 17 */ g_Opcodes + 1720  ,
  /* 18 */ NULL  ,
  /* 19 */ NULL  ,
  /* 1a */ NULL  ,
  /* 1b */ NULL  ,
  /* 1c */ NULL  ,
  /* 1d */ NULL  ,
  /* 1e */ NULL  ,
  /* 1f */ NULL  ,
  /* 20 */ g_Opcodes + 1721  ,
  /* 21 */ g_Opcodes + 1722  ,
  /* 22 */ g_Opcodes + 1723  ,
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
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
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
