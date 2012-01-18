/*
 * THIS FILE IS AUTO-GENERATED. DO NOT EDIT.
 * Compiled for x86-32 bit mode.
 *
 * You must include ncopcode_desc.h before this file.
 */

static const NaClOp g_Operands[723] = {
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
  /* 71 */ { RegES_EDI, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$Yzd}" },
  /* 72 */ { RegDX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%dx}" },
  /* 73 */ { RegES_EDI, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$Yzw}" },
  /* 74 */ { RegDX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%dx}" },
  /* 75 */ { RegDX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%dx}" },
  /* 76 */ { RegDS_ESI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Xb}" },
  /* 77 */ { RegDX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%dx}" },
  /* 78 */ { RegDS_ESI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Xzd}" },
  /* 79 */ { RegDX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%dx}" },
  /* 80 */ { RegDS_ESI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Xzw}" },
  /* 81 */ { RegEIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eip}" },
  /* 82 */ { J_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandNear) | NACL_OPFLAG(OperandRelative), "$Jb" },
  /* 83 */ { E_Operand, NACL_OPFLAG(OpUse), "$Eb" },
  /* 84 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 85 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Eb" },
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
  /* 111 */ { RegEAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit) | NACL_OPFLAG(OperandSignExtends_v), "{%eax}" },
  /* 112 */ { RegAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%ax}" },
  /* 113 */ { RegAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%ax}" },
  /* 114 */ { RegAL, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%al}" },
  /* 115 */ { RegEDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%edx}" },
  /* 116 */ { RegEAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%eax}" },
  /* 117 */ { RegDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%dx}" },
  /* 118 */ { RegAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%ax}" },
  /* 119 */ { RegEIP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eip}" },
  /* 120 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 121 */ { J_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandFar) | NACL_OPFLAG(OperandRelative), "$Ap" },
  /* 122 */ { RegEIP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eip}" },
  /* 123 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 124 */ { J_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandFar) | NACL_OPFLAG(OperandRelative), "$Ad" },
  /* 125 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 126 */ { RegRFLAGS, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Fvd}" },
  /* 127 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 128 */ { RegRFLAGS, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Fvw}" },
  /* 129 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 130 */ { RegRFLAGS, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$Fvd}" },
  /* 131 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 132 */ { RegRFLAGS, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$Fvw}" },
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
  /* 144 */ { RegDS_ESI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Xb}" },
  /* 145 */ { RegES_EDI, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$Yvd}" },
  /* 146 */ { RegDS_ESI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Xvd}" },
  /* 147 */ { RegES_EDI, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$Yvw}" },
  /* 148 */ { RegDS_ESI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Xvw}" },
  /* 149 */ { RegES_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Yb}" },
  /* 150 */ { RegDS_ESI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Xb}" },
  /* 151 */ { RegES_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Yvd}" },
  /* 152 */ { RegDS_ESI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Xvd}" },
  /* 153 */ { RegES_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Yvw}" },
  /* 154 */ { RegDS_ESI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Xvw}" },
  /* 155 */ { RegES_EDI, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$Yb}" },
  /* 156 */ { RegAL, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%al}" },
  /* 157 */ { RegES_EDI, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$Yvd}" },
  /* 158 */ { RegEAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$rAXvd}" },
  /* 159 */ { RegES_EDI, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$Yvw}" },
  /* 160 */ { RegAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$rAXvw}" },
  /* 161 */ { RegAL, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%al}" },
  /* 162 */ { RegDS_ESI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Xb}" },
  /* 163 */ { RegEAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$rAXvd}" },
  /* 164 */ { RegDS_ESI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Xvd}" },
  /* 165 */ { RegAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$rAXvw}" },
  /* 166 */ { RegDS_ESI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Xvw}" },
  /* 167 */ { RegAL, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%al}" },
  /* 168 */ { RegES_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Yb}" },
  /* 169 */ { RegEAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$rAXvd}" },
  /* 170 */ { RegES_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Yvd}" },
  /* 171 */ { RegAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$rAXvw}" },
  /* 172 */ { RegES_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Yvw}" },
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
  /* 209 */ { Mw_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Mw" },
  /* 210 */ { M_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$M" },
  /* 211 */ { Mw_Operand, NACL_OPFLAG(OpUse), "$Mw" },
  /* 212 */ { M_Operand, NACL_OPFLAG(OpUse), "$M" },
  /* 213 */ { Mv_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Md" },
  /* 214 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 215 */ { RegST0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 216 */ { Mv_Operand, NACL_OPFLAG(OpUse), "$Md" },
  /* 217 */ { M_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$M" },
  /* 218 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 219 */ { RegST0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 220 */ { M_Operand, NACL_OPFLAG(OpUse), "$M" },
  /* 221 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 222 */ { Mo_Operand, NACL_OPFLAG(OpUse), "$Mq" },
  /* 223 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 224 */ { Mo_Operand, NACL_OPFLAG(OpUse), "$Mq" },
  /* 225 */ { Mo_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Mq" },
  /* 226 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 227 */ { RegST0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 228 */ { Mo_Operand, NACL_OPFLAG(OpUse), "$Mq" },
  /* 229 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 230 */ { Mw_Operand, NACL_OPFLAG(OpUse), "$Mw" },
  /* 231 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 232 */ { Mw_Operand, NACL_OPFLAG(OpUse), "$Mw" },
  /* 233 */ { Mw_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Mw" },
  /* 234 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 235 */ { RegST0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 236 */ { Mw_Operand, NACL_OPFLAG(OpUse), "$Mw" },
  /* 237 */ { RegEIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eip}" },
  /* 238 */ { RegECX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%ecx}" },
  /* 239 */ { J_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandNear) | NACL_OPFLAG(OperandRelative), "$Jb" },
  /* 240 */ { RegEIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eip}" },
  /* 241 */ { RegCX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%cx}" },
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
  /* 272 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 273 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ev" },
  /* 274 */ { RegEIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eip}" },
  /* 275 */ { M_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandFar), "$Mp" },
  /* 276 */ { RegEIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eip}" },
  /* 277 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandNear), "$Ev" },
  /* 278 */ { RegEIP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eip}" },
  /* 279 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 280 */ { M_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandFar), "$Mp" },
  /* 281 */ { RegEIP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eip}" },
  /* 282 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 283 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandNear), "$Ev" },
  /* 284 */ { Ew_Operand, NACL_EMPTY_OPFLAGS, "$Ew" },
  /* 285 */ { Mb_Operand, NACL_OPFLAG(OpUse), "$Mb" },
  /* 286 */ { RegREAXa, NACL_OPFLAG(OpUse), "$rAXva" },
  /* 287 */ { RegECX, NACL_OPFLAG(OpUse), "%ecx" },
  /* 288 */ { RegEIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eip}" },
  /* 289 */ { RegEAX, NACL_OPFLAG(OpUse), "%eax" },
  /* 290 */ { M_Operand, NACL_OPFLAG(OpUse), "$Ms" },
  /* 291 */ { RegEAX, NACL_EMPTY_OPFLAGS, "%eax" },
  /* 292 */ { RegECX, NACL_EMPTY_OPFLAGS, "%ecx" },
  /* 293 */ { RegREAX, NACL_OPFLAG(OpUse), "%reax" },
  /* 294 */ { RegECX, NACL_OPFLAG(OpUse), "%ecx" },
  /* 295 */ { RegEDX, NACL_OPFLAG(OpUse), "%edx" },
  /* 296 */ { M_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Ms" },
  /* 297 */ { G_Operand, NACL_EMPTY_OPFLAGS, "$Gv" },
  /* 298 */ { Ew_Operand, NACL_EMPTY_OPFLAGS, "$Ew" },
  /* 299 */ { Mb_Operand, NACL_EMPTY_OPFLAGS, "$Mb" },
  /* 300 */ { Mmx_G_Operand, NACL_EMPTY_OPFLAGS, "$Pq" },
  /* 301 */ { Mmx_E_Operand, NACL_EMPTY_OPFLAGS, "$Qq" },
  /* 302 */ { I_Operand, NACL_EMPTY_OPFLAGS, "$Ib" },
  /* 303 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vps" },
  /* 304 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wps" },
  /* 305 */ { Xmm_E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Wps" },
  /* 306 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vps" },
  /* 307 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vps" },
  /* 308 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$VRq" },
  /* 309 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vps" },
  /* 310 */ { Mo_Operand, NACL_OPFLAG(OpUse), "$Mq" },
  /* 311 */ { Mo_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Mq" },
  /* 312 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vps" },
  /* 313 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vps" },
  /* 314 */ { Xmm_Eo_Operand, NACL_OPFLAG(OpUse), "$Wq" },
  /* 315 */ { Ev_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Rd/q" },
  /* 316 */ { C_Operand, NACL_OPFLAG(OpUse), "$Cd/q" },
  /* 317 */ { Ev_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Rd/q" },
  /* 318 */ { D_Operand, NACL_OPFLAG(OpUse), "$Dd/q" },
  /* 319 */ { C_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Cd/q" },
  /* 320 */ { Ev_Operand, NACL_OPFLAG(OpUse), "$Rd/q" },
  /* 321 */ { D_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Dd/q" },
  /* 322 */ { Ev_Operand, NACL_OPFLAG(OpUse), "$Rd/q" },
  /* 323 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vps" },
  /* 324 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse), "$Qq" },
  /* 325 */ { Mdq_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Mdq" },
  /* 326 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vps" },
  /* 327 */ { Mmx_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Pq" },
  /* 328 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wps" },
  /* 329 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vss" },
  /* 330 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wss" },
  /* 331 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vps" },
  /* 332 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wps" },
  /* 333 */ { RegEAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%eax}" },
  /* 334 */ { RegEDX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%edx}" },
  /* 335 */ { RegECX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%ecx}" },
  /* 336 */ { RegEAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eax}" },
  /* 337 */ { RegEDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%edx}" },
  /* 338 */ { RegEAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eax}" },
  /* 339 */ { RegEDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%edx}" },
  /* 340 */ { RegECX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%ecx}" },
  /* 341 */ { RegEIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eip}" },
  /* 342 */ { RegESP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 343 */ { RegCS, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%cs}" },
  /* 344 */ { RegSS, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%ss}" },
  /* 345 */ { RegEIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eip}" },
  /* 346 */ { RegESP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 347 */ { RegCS, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%cs}" },
  /* 348 */ { RegSS, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%ss}" },
  /* 349 */ { RegEDX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%edx}" },
  /* 350 */ { RegECX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%ecx}" },
  /* 351 */ { Gv_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Gd" },
  /* 352 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$VRps" },
  /* 353 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vps" },
  /* 354 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wps" },
  /* 355 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vpd" },
  /* 356 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wps" },
  /* 357 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vps" },
  /* 358 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 359 */ { Mmx_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Pq" },
  /* 360 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse), "$Qq" },
  /* 361 */ { Mmx_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Pq" },
  /* 362 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse), "$Qd" },
  /* 363 */ { Mmx_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Pq" },
  /* 364 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ed/q/d" },
  /* 365 */ { Mmx_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Pq" },
  /* 366 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse), "$Qq" },
  /* 367 */ { Mmx_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Pq" },
  /* 368 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse), "$Qq" },
  /* 369 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 370 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$PRq" },
  /* 371 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 372 */ { E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Ed/q/d" },
  /* 373 */ { Mmx_G_Operand, NACL_OPFLAG(OpUse), "$Pd/q/d" },
  /* 374 */ { Mmx_E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Qq" },
  /* 375 */ { Mmx_G_Operand, NACL_OPFLAG(OpUse), "$Pq" },
  /* 376 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 377 */ { RegFS, NACL_OPFLAG(OpUse), "%fs" },
  /* 378 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 379 */ { RegFS, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%fs" },
  /* 380 */ { RegEBX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%ebx}" },
  /* 381 */ { RegEDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%edx}" },
  /* 382 */ { RegEAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eax}" },
  /* 383 */ { RegECX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%ecx}" },
  /* 384 */ { E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Ev" },
  /* 385 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gv" },
  /* 386 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 387 */ { E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Ev" },
  /* 388 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gv" },
  /* 389 */ { RegCL, NACL_OPFLAG(OpUse), "%cl" },
  /* 390 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 391 */ { RegGS, NACL_OPFLAG(OpUse), "%gs" },
  /* 392 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 393 */ { RegGS, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%gs" },
  /* 394 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Ev" },
  /* 395 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gv" },
  /* 396 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 397 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Ev" },
  /* 398 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gv" },
  /* 399 */ { RegCL, NACL_OPFLAG(OpUse), "%cl" },
  /* 400 */ { RegAL, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%al}" },
  /* 401 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Eb" },
  /* 402 */ { G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Gb" },
  /* 403 */ { RegREAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$rAXv}" },
  /* 404 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Ev" },
  /* 405 */ { G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Gv" },
  /* 406 */ { G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Gv" },
  /* 407 */ { Eb_Operand, NACL_OPFLAG(OpUse), "$Eb" },
  /* 408 */ { G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Gv" },
  /* 409 */ { Ew_Operand, NACL_OPFLAG(OpUse), "$Ew" },
  /* 410 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vps" },
  /* 411 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wps" },
  /* 412 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 413 */ { M_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Md/q" },
  /* 414 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gd/q" },
  /* 415 */ { Mmx_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Pq" },
  /* 416 */ { E_Operand, NACL_OPFLAG(OpUse), "$Rd/q/Mw" },
  /* 417 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 418 */ { Gv_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Gd" },
  /* 419 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse), "$PRq" },
  /* 420 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 421 */ { RegEDX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%edx}" },
  /* 422 */ { RegEAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eax}" },
  /* 423 */ { Mo_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Mq" },
  /* 424 */ { Mo_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Mq" },
  /* 425 */ { Mmx_G_Operand, NACL_OPFLAG(OpUse), "$Pq" },
  /* 426 */ { RegDS_EDI, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$Zvd}" },
  /* 427 */ { Mmx_G_Operand, NACL_OPFLAG(OpUse), "$Pq" },
  /* 428 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse), "$PRq" },
  /* 429 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vsd" },
  /* 430 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wsd" },
  /* 431 */ { Xmm_E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Wsd" },
  /* 432 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vsd" },
  /* 433 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vpd" },
  /* 434 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wsd" },
  /* 435 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vsd" },
  /* 436 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ed/q" },
  /* 437 */ { Mo_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Mq" },
  /* 438 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vsd" },
  /* 439 */ { G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Gd/q" },
  /* 440 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wsd" },
  /* 441 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vsd" },
  /* 442 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wsd" },
  /* 443 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vss" },
  /* 444 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wsd" },
  /* 445 */ { Xmm_Go_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vq" },
  /* 446 */ { Xmm_Eo_Operand, NACL_OPFLAG(OpUse), "$Wq" },
  /* 447 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 448 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 449 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$VRq" },
  /* 450 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 451 */ { I2_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 452 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 453 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$VRdq" },
  /* 454 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vsd" },
  /* 455 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wsd" },
  /* 456 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 457 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vpd" },
  /* 458 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wpd" },
  /* 459 */ { Mmx_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Pq" },
  /* 460 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$VRq" },
  /* 461 */ { Xmm_Go_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vq" },
  /* 462 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wpd" },
  /* 463 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 464 */ { Mdq_Operand, NACL_OPFLAG(OpUse), "$Mdq" },
  /* 465 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vss" },
  /* 466 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wss" },
  /* 467 */ { Xmm_E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Wss" },
  /* 468 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vss" },
  /* 469 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vss" },
  /* 470 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ed/q" },
  /* 471 */ { Mv_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Md" },
  /* 472 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vss" },
  /* 473 */ { G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Gd/q" },
  /* 474 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wss" },
  /* 475 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vss" },
  /* 476 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wss" },
  /* 477 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vsd" },
  /* 478 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wss" },
  /* 479 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 480 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wps" },
  /* 481 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 482 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 483 */ { Xmm_E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Wdq" },
  /* 484 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 485 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vss" },
  /* 486 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wss" },
  /* 487 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 488 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 489 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse), "$PRq" },
  /* 490 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vpd" },
  /* 491 */ { Xmm_Eo_Operand, NACL_OPFLAG(OpUse), "$Wq" },
  /* 492 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vpd" },
  /* 493 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wpd" },
  /* 494 */ { Xmm_E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Wpd" },
  /* 495 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vpd" },
  /* 496 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vsd" },
  /* 497 */ { Mo_Operand, NACL_OPFLAG(OpUse), "$Mq" },
  /* 498 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vpd" },
  /* 499 */ { Xmm_Eo_Operand, NACL_OPFLAG(OpUse), "$Wq" },
  /* 500 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vpd" },
  /* 501 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse), "$Qq" },
  /* 502 */ { Mdq_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Mdq" },
  /* 503 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vpd" },
  /* 504 */ { Mmx_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Pq" },
  /* 505 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wpd" },
  /* 506 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vsd" },
  /* 507 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wsd" },
  /* 508 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vpd" },
  /* 509 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wsd" },
  /* 510 */ { Gv_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Gd" },
  /* 511 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$VRpd" },
  /* 512 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vps" },
  /* 513 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wpd" },
  /* 514 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 515 */ { Xmm_Eo_Operand, NACL_OPFLAG(OpUse), "$Wq" },
  /* 516 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 517 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 518 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 519 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ed/q/d" },
  /* 520 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 521 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 522 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 523 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$VRdq" },
  /* 524 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 525 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest) | NACL_OPFLAG(AllowGOperandWithOpcodeInModRm), "$Vdq" },
  /* 526 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 527 */ { I2_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 528 */ { E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Ed/q/d" },
  /* 529 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vd/q/d" },
  /* 530 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vpd" },
  /* 531 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wpd" },
  /* 532 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 533 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 534 */ { E_Operand, NACL_OPFLAG(OpUse), "$Rd/q/Mw" },
  /* 535 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 536 */ { Gv_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Gd" },
  /* 537 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$VRdq" },
  /* 538 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 539 */ { Xmm_Eo_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Wq" },
  /* 540 */ { Xmm_Go_Operand, NACL_OPFLAG(OpUse), "$Vq" },
  /* 541 */ { Xmm_Go_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vq" },
  /* 542 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 543 */ { Mdq_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Mdq" },
  /* 544 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 545 */ { RegDS_EDI, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$Zvd}" },
  /* 546 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 547 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$VRdq" },
  /* 548 */ { G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Gv" },
  /* 549 */ { M_Operand, NACL_OPFLAG(OpUse), "$Mv" },
  /* 550 */ { M_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Mv" },
  /* 551 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gv" },
  /* 552 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 553 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 554 */ { RegXMM0, NACL_OPFLAG(OpUse), "%xmm0" },
  /* 555 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 556 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 557 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 558 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Udq/Mq" },
  /* 559 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 560 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Udq/Md" },
  /* 561 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 562 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Udq/Mw" },
  /* 563 */ { Gv_Operand, NACL_OPFLAG(OpUse), "$Gd" },
  /* 564 */ { Mdq_Operand, NACL_OPFLAG(OpUse), "$Mdq" },
  /* 565 */ { Gv_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Gd" },
  /* 566 */ { E_Operand, NACL_OPFLAG(OpUse), "$Eb" },
  /* 567 */ { Gv_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Gd" },
  /* 568 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ev" },
  /* 569 */ { Mmx_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Pq" },
  /* 570 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse), "$Qq" },
  /* 571 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 572 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vss" },
  /* 573 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wss" },
  /* 574 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 575 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vsd" },
  /* 576 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wsd" },
  /* 577 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 578 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 579 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 580 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 581 */ { Ev_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Rd/Mb" },
  /* 582 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 583 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 584 */ { Ev_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Rd/Mw" },
  /* 585 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 586 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 587 */ { E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Ed/q/d" },
  /* 588 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 589 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 590 */ { Ev_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Ed" },
  /* 591 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 592 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 593 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 594 */ { E_Operand, NACL_OPFLAG(OpUse), "$Rd/q/Mb" },
  /* 595 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 596 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 597 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Udq/Md" },
  /* 598 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 599 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "$Vdq" },
  /* 600 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ed/q/d" },
  /* 601 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 602 */ { RegXMM0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%xmm0}" },
  /* 603 */ { RegREAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$rAXv}" },
  /* 604 */ { RegREDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$rDXv}" },
  /* 605 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 606 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 607 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 608 */ { RegRECX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$rCXv}" },
  /* 609 */ { RegREAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$rAXv}" },
  /* 610 */ { RegREDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$rDXv}" },
  /* 611 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 612 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 613 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 614 */ { RegXMM0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%xmm0}" },
  /* 615 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 616 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 617 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 618 */ { RegRECX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$rCXv}" },
  /* 619 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 620 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 621 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 622 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 623 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 624 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 625 */ { RegST1, NACL_OPFLAG(OpUse), "%st1" },
  /* 626 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 627 */ { RegST2, NACL_OPFLAG(OpUse), "%st2" },
  /* 628 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 629 */ { RegST3, NACL_OPFLAG(OpUse), "%st3" },
  /* 630 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 631 */ { RegST4, NACL_OPFLAG(OpUse), "%st4" },
  /* 632 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 633 */ { RegST5, NACL_OPFLAG(OpUse), "%st5" },
  /* 634 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 635 */ { RegST6, NACL_OPFLAG(OpUse), "%st6" },
  /* 636 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 637 */ { RegST7, NACL_OPFLAG(OpUse), "%st7" },
  /* 638 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 639 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 640 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 641 */ { RegST1, NACL_OPFLAG(OpUse), "%st1" },
  /* 642 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 643 */ { RegST2, NACL_OPFLAG(OpUse), "%st2" },
  /* 644 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 645 */ { RegST3, NACL_OPFLAG(OpUse), "%st3" },
  /* 646 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 647 */ { RegST4, NACL_OPFLAG(OpUse), "%st4" },
  /* 648 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 649 */ { RegST5, NACL_OPFLAG(OpUse), "%st5" },
  /* 650 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 651 */ { RegST6, NACL_OPFLAG(OpUse), "%st6" },
  /* 652 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 653 */ { RegST7, NACL_OPFLAG(OpUse), "%st7" },
  /* 654 */ { RegST0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 655 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 656 */ { RegST0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 657 */ { RegST1, NACL_OPFLAG(OpUse), "%st1" },
  /* 658 */ { RegST0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 659 */ { RegST2, NACL_OPFLAG(OpUse), "%st2" },
  /* 660 */ { RegST0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 661 */ { RegST3, NACL_OPFLAG(OpUse), "%st3" },
  /* 662 */ { RegST0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 663 */ { RegST4, NACL_OPFLAG(OpUse), "%st4" },
  /* 664 */ { RegST0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 665 */ { RegST5, NACL_OPFLAG(OpUse), "%st5" },
  /* 666 */ { RegST0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 667 */ { RegST6, NACL_OPFLAG(OpUse), "%st6" },
  /* 668 */ { RegST0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 669 */ { RegST7, NACL_OPFLAG(OpUse), "%st7" },
  /* 670 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 671 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 672 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 673 */ { RegST1, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st1" },
  /* 674 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 675 */ { RegST2, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st2" },
  /* 676 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 677 */ { RegST3, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st3" },
  /* 678 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 679 */ { RegST4, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st4" },
  /* 680 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 681 */ { RegST5, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st5" },
  /* 682 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 683 */ { RegST6, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st6" },
  /* 684 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st0" },
  /* 685 */ { RegST7, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st7" },
  /* 686 */ { RegST1, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st1" },
  /* 687 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 688 */ { RegST2, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st2" },
  /* 689 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 690 */ { RegST3, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st3" },
  /* 691 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 692 */ { RegST4, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st4" },
  /* 693 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 694 */ { RegST5, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st5" },
  /* 695 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 696 */ { RegST6, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st6" },
  /* 697 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 698 */ { RegST7, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st7" },
  /* 699 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 700 */ { RegST0, NACL_EMPTY_OPFLAGS, "%st0" },
  /* 701 */ { RegST1, NACL_EMPTY_OPFLAGS, "%st1" },
  /* 702 */ { RegST2, NACL_EMPTY_OPFLAGS, "%st2" },
  /* 703 */ { RegST3, NACL_EMPTY_OPFLAGS, "%st3" },
  /* 704 */ { RegST4, NACL_EMPTY_OPFLAGS, "%st4" },
  /* 705 */ { RegST5, NACL_EMPTY_OPFLAGS, "%st5" },
  /* 706 */ { RegST6, NACL_EMPTY_OPFLAGS, "%st6" },
  /* 707 */ { RegST7, NACL_EMPTY_OPFLAGS, "%st7" },
  /* 708 */ { RegST1, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st1" },
  /* 709 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 710 */ { RegST2, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st2" },
  /* 711 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 712 */ { RegST3, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st3" },
  /* 713 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 714 */ { RegST4, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st4" },
  /* 715 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 716 */ { RegST5, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st5" },
  /* 717 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 718 */ { RegST6, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st6" },
  /* 719 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 720 */ { RegST7, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%st7" },
  /* 721 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 722 */ { RegAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest), "%ax" },
};

static const NaClInst g_Opcodes[1363] = {
  /* 0 */
  { NACLi_INVALID,
    NACL_EMPTY_IFLAGS,
    InstInvalid, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 1 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstAdd, 0x00, 2, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 2 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstAdd, 0x00, 2, 2, NACL_OPCODE_NULL_OFFSET  },
  /* 3 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstAdd, 0x00, 2, 4, NACL_OPCODE_NULL_OFFSET  },
  /* 4 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstAdd, 0x00, 2, 6, NACL_OPCODE_NULL_OFFSET  },
  /* 5 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable),
    InstAdd, 0x00, 2, 8, NACL_OPCODE_NULL_OFFSET  },
  /* 6 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstAdd, 0x00, 2, 10, NACL_OPCODE_NULL_OFFSET  },
  /* 7 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstPush, 0x00, 2, 12, NACL_OPCODE_NULL_OFFSET  },
  /* 8 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstPop, 0x00, 2, 14, NACL_OPCODE_NULL_OFFSET  },
  /* 9 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstOr, 0x00, 2, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 10 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstOr, 0x00, 2, 2, NACL_OPCODE_NULL_OFFSET  },
  /* 11 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstOr, 0x00, 2, 4, NACL_OPCODE_NULL_OFFSET  },
  /* 12 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstOr, 0x00, 2, 6, NACL_OPCODE_NULL_OFFSET  },
  /* 13 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable),
    InstOr, 0x00, 2, 8, NACL_OPCODE_NULL_OFFSET  },
  /* 14 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstOr, 0x00, 2, 10, NACL_OPCODE_NULL_OFFSET  },
  /* 15 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstPush, 0x00, 2, 16, NACL_OPCODE_NULL_OFFSET  },
  /* 16 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 17 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstAdc, 0x00, 2, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 18 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstAdc, 0x00, 2, 2, NACL_OPCODE_NULL_OFFSET  },
  /* 19 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstAdc, 0x00, 2, 4, NACL_OPCODE_NULL_OFFSET  },
  /* 20 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstAdc, 0x00, 2, 6, NACL_OPCODE_NULL_OFFSET  },
  /* 21 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable),
    InstAdc, 0x00, 2, 8, NACL_OPCODE_NULL_OFFSET  },
  /* 22 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstAdc, 0x00, 2, 10, NACL_OPCODE_NULL_OFFSET  },
  /* 23 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstPush, 0x00, 2, 18, NACL_OPCODE_NULL_OFFSET  },
  /* 24 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstPop, 0x00, 2, 20, NACL_OPCODE_NULL_OFFSET  },
  /* 25 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstSbb, 0x00, 2, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 26 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstSbb, 0x00, 2, 2, NACL_OPCODE_NULL_OFFSET  },
  /* 27 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstSbb, 0x00, 2, 4, NACL_OPCODE_NULL_OFFSET  },
  /* 28 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstSbb, 0x00, 2, 6, NACL_OPCODE_NULL_OFFSET  },
  /* 29 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable),
    InstSbb, 0x00, 2, 8, NACL_OPCODE_NULL_OFFSET  },
  /* 30 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstSbb, 0x00, 2, 10, NACL_OPCODE_NULL_OFFSET  },
  /* 31 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstPush, 0x00, 2, 22, NACL_OPCODE_NULL_OFFSET  },
  /* 32 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstPop, 0x00, 2, 24, NACL_OPCODE_NULL_OFFSET  },
  /* 33 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstAnd, 0x00, 2, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 34 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstAnd, 0x00, 2, 2, NACL_OPCODE_NULL_OFFSET  },
  /* 35 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstAnd, 0x00, 2, 4, NACL_OPCODE_NULL_OFFSET  },
  /* 36 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstAnd, 0x00, 2, 6, NACL_OPCODE_NULL_OFFSET  },
  /* 37 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable),
    InstAnd, 0x00, 2, 8, NACL_OPCODE_NULL_OFFSET  },
  /* 38 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstAnd, 0x00, 2, 10, NACL_OPCODE_NULL_OFFSET  },
  /* 39 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstDaa, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 40 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstSub, 0x00, 2, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 41 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstSub, 0x00, 2, 2, NACL_OPCODE_NULL_OFFSET  },
  /* 42 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstSub, 0x00, 2, 4, NACL_OPCODE_NULL_OFFSET  },
  /* 43 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstSub, 0x00, 2, 6, NACL_OPCODE_NULL_OFFSET  },
  /* 44 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable),
    InstSub, 0x00, 2, 8, NACL_OPCODE_NULL_OFFSET  },
  /* 45 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstSub, 0x00, 2, 10, NACL_OPCODE_NULL_OFFSET  },
  /* 46 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstDas, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 47 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstXor, 0x00, 2, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 48 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstXor, 0x00, 2, 2, NACL_OPCODE_NULL_OFFSET  },
  /* 49 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstXor, 0x00, 2, 4, NACL_OPCODE_NULL_OFFSET  },
  /* 50 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstXor, 0x00, 2, 6, NACL_OPCODE_NULL_OFFSET  },
  /* 51 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable),
    InstXor, 0x00, 2, 8, NACL_OPCODE_NULL_OFFSET  },
  /* 52 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstXor, 0x00, 2, 10, NACL_OPCODE_NULL_OFFSET  },
  /* 53 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstAaa, 0x00, 1, 26, NACL_OPCODE_NULL_OFFSET  },
  /* 54 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstCmp, 0x00, 2, 27, NACL_OPCODE_NULL_OFFSET  },
  /* 55 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstCmp, 0x00, 2, 29, NACL_OPCODE_NULL_OFFSET  },
  /* 56 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstCmp, 0x00, 2, 31, NACL_OPCODE_NULL_OFFSET  },
  /* 57 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstCmp, 0x00, 2, 33, NACL_OPCODE_NULL_OFFSET  },
  /* 58 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b),
    InstCmp, 0x00, 2, 35, NACL_OPCODE_NULL_OFFSET  },
  /* 59 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstCmp, 0x00, 2, 37, NACL_OPCODE_NULL_OFFSET  },
  /* 60 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstAas, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 61 */
  { NACLi_386,
    NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstInc, 0x00, 1, 10, NACL_OPCODE_NULL_OFFSET  },
  /* 62 */
  { NACLi_386,
    NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstInc, 0x00, 1, 39, NACL_OPCODE_NULL_OFFSET  },
  /* 63 */
  { NACLi_386,
    NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstInc, 0x00, 1, 40, NACL_OPCODE_NULL_OFFSET  },
  /* 64 */
  { NACLi_386,
    NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstInc, 0x00, 1, 41, NACL_OPCODE_NULL_OFFSET  },
  /* 65 */
  { NACLi_386,
    NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstInc, 0x00, 1, 42, NACL_OPCODE_NULL_OFFSET  },
  /* 66 */
  { NACLi_386,
    NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstInc, 0x00, 1, 43, NACL_OPCODE_NULL_OFFSET  },
  /* 67 */
  { NACLi_386,
    NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstInc, 0x00, 1, 44, NACL_OPCODE_NULL_OFFSET  },
  /* 68 */
  { NACLi_386,
    NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstInc, 0x00, 1, 45, NACL_OPCODE_NULL_OFFSET  },
  /* 69 */
  { NACLi_386,
    NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstDec, 0x00, 1, 10, NACL_OPCODE_NULL_OFFSET  },
  /* 70 */
  { NACLi_386,
    NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstDec, 0x00, 1, 39, NACL_OPCODE_NULL_OFFSET  },
  /* 71 */
  { NACLi_386,
    NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstDec, 0x00, 1, 40, NACL_OPCODE_NULL_OFFSET  },
  /* 72 */
  { NACLi_386,
    NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstDec, 0x00, 1, 41, NACL_OPCODE_NULL_OFFSET  },
  /* 73 */
  { NACLi_386,
    NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstDec, 0x00, 1, 42, NACL_OPCODE_NULL_OFFSET  },
  /* 74 */
  { NACLi_386,
    NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstDec, 0x00, 1, 43, NACL_OPCODE_NULL_OFFSET  },
  /* 75 */
  { NACLi_386,
    NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstDec, 0x00, 1, 44, NACL_OPCODE_NULL_OFFSET  },
  /* 76 */
  { NACLi_386,
    NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstDec, 0x00, 1, 45, NACL_OPCODE_NULL_OFFSET  },
  /* 77 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstPush, 0x00, 2, 46, NACL_OPCODE_NULL_OFFSET  },
  /* 78 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstPush, 0x01, 2, 46, NACL_OPCODE_NULL_OFFSET  },
  /* 79 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstPush, 0x02, 2, 46, NACL_OPCODE_NULL_OFFSET  },
  /* 80 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstPush, 0x03, 2, 46, NACL_OPCODE_NULL_OFFSET  },
  /* 81 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstPush, 0x04, 2, 46, NACL_OPCODE_NULL_OFFSET  },
  /* 82 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstPush, 0x05, 2, 46, NACL_OPCODE_NULL_OFFSET  },
  /* 83 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstPush, 0x06, 2, 46, NACL_OPCODE_NULL_OFFSET  },
  /* 84 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstPush, 0x07, 2, 46, NACL_OPCODE_NULL_OFFSET  },
  /* 85 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstPop, 0x00, 2, 48, NACL_OPCODE_NULL_OFFSET  },
  /* 86 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstPop, 0x01, 2, 48, NACL_OPCODE_NULL_OFFSET  },
  /* 87 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstPop, 0x02, 2, 48, NACL_OPCODE_NULL_OFFSET  },
  /* 88 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstPop, 0x03, 2, 48, NACL_OPCODE_NULL_OFFSET  },
  /* 89 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstPop, 0x04, 2, 48, NACL_OPCODE_NULL_OFFSET  },
  /* 90 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstPop, 0x05, 2, 48, NACL_OPCODE_NULL_OFFSET  },
  /* 91 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstPop, 0x06, 2, 48, NACL_OPCODE_NULL_OFFSET  },
  /* 92 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstPop, 0x07, 2, 48, NACL_OPCODE_NULL_OFFSET  },
  /* 93 */
  { NACLi_386,
    NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstPushad, 0x00, 2, 50, NACL_OPCODE_NULL_OFFSET  },
  /* 94 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(NaClIllegal),
    InstPusha, 0x00, 2, 50, 93  },
  /* 95 */
  { NACLi_386,
    NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstPopad, 0x00, 2, 52, NACL_OPCODE_NULL_OFFSET  },
  /* 96 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(NaClIllegal),
    InstPopa, 0x00, 2, 52, 95  },
  /* 97 */
  { NACLi_386,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstBound, 0x00, 3, 54, NACL_OPCODE_NULL_OFFSET  },
  /* 98 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstArpl, 0x00, 2, 57, NACL_OPCODE_NULL_OFFSET  },
  /* 99 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16),
    InstPush, 0x00, 2, 59, NACL_OPCODE_NULL_OFFSET  },
  /* 100 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstImul, 0x00, 3, 61, NACL_OPCODE_NULL_OFFSET  },
  /* 101 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b),
    InstPush, 0x00, 2, 64, NACL_OPCODE_NULL_OFFSET  },
  /* 102 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstImul, 0x00, 3, 66, NACL_OPCODE_NULL_OFFSET  },
  /* 103 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(NaClIllegal),
    InstInsb, 0x00, 2, 69, NACL_OPCODE_NULL_OFFSET  },
  /* 104 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstInsd, 0x00, 2, 71, NACL_OPCODE_NULL_OFFSET  },
  /* 105 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(NaClIllegal),
    InstInsw, 0x00, 2, 73, 104  },
  /* 106 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(NaClIllegal),
    InstOutsb, 0x00, 2, 75, NACL_OPCODE_NULL_OFFSET  },
  /* 107 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstOutsd, 0x00, 2, 77, NACL_OPCODE_NULL_OFFSET  },
  /* 108 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(NaClIllegal),
    InstOutsw, 0x00, 2, 79, 107  },
  /* 109 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump),
    InstJo, 0x00, 2, 81, NACL_OPCODE_NULL_OFFSET  },
  /* 110 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump),
    InstJno, 0x00, 2, 81, NACL_OPCODE_NULL_OFFSET  },
  /* 111 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump),
    InstJb, 0x00, 2, 81, NACL_OPCODE_NULL_OFFSET  },
  /* 112 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump),
    InstJnb, 0x00, 2, 81, NACL_OPCODE_NULL_OFFSET  },
  /* 113 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump),
    InstJz, 0x00, 2, 81, NACL_OPCODE_NULL_OFFSET  },
  /* 114 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump),
    InstJnz, 0x00, 2, 81, NACL_OPCODE_NULL_OFFSET  },
  /* 115 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump),
    InstJbe, 0x00, 2, 81, NACL_OPCODE_NULL_OFFSET  },
  /* 116 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump),
    InstJnbe, 0x00, 2, 81, NACL_OPCODE_NULL_OFFSET  },
  /* 117 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump),
    InstJs, 0x00, 2, 81, NACL_OPCODE_NULL_OFFSET  },
  /* 118 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump),
    InstJns, 0x00, 2, 81, NACL_OPCODE_NULL_OFFSET  },
  /* 119 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump),
    InstJp, 0x00, 2, 81, NACL_OPCODE_NULL_OFFSET  },
  /* 120 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump),
    InstJnp, 0x00, 2, 81, NACL_OPCODE_NULL_OFFSET  },
  /* 121 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump),
    InstJl, 0x00, 2, 81, NACL_OPCODE_NULL_OFFSET  },
  /* 122 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump),
    InstJnl, 0x00, 2, 81, NACL_OPCODE_NULL_OFFSET  },
  /* 123 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump),
    InstJle, 0x00, 2, 81, NACL_OPCODE_NULL_OFFSET  },
  /* 124 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump),
    InstJnle, 0x00, 2, 81, NACL_OPCODE_NULL_OFFSET  },
  /* 125 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstCmp, 0x07, 2, 83, NACL_OPCODE_NULL_OFFSET  },
  /* 126 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstXor, 0x06, 2, 85, 125  },
  /* 127 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstSub, 0x05, 2, 85, 126  },
  /* 128 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstAnd, 0x04, 2, 85, 127  },
  /* 129 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstSbb, 0x03, 2, 85, 128  },
  /* 130 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstAdc, 0x02, 2, 85, 129  },
  /* 131 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstOr, 0x01, 2, 85, 130  },
  /* 132 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstAdd, 0x00, 2, 85, 131  },
  /* 133 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstCmp, 0x07, 2, 62, NACL_OPCODE_NULL_OFFSET  },
  /* 134 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstXor, 0x06, 2, 87, 133  },
  /* 135 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstSub, 0x05, 2, 87, 134  },
  /* 136 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstAnd, 0x04, 2, 87, 135  },
  /* 137 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstSbb, 0x03, 2, 87, 136  },
  /* 138 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstAdc, 0x02, 2, 87, 137  },
  /* 139 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstOr, 0x01, 2, 87, 138  },
  /* 140 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstAdd, 0x00, 2, 87, 139  },
  /* 141 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(NaClIllegal),
    InstCmp, 0x07, 2, 83, NACL_OPCODE_NULL_OFFSET  },
  /* 142 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(NaClIllegal),
    InstXor, 0x06, 2, 85, 141  },
  /* 143 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(NaClIllegal),
    InstSub, 0x05, 2, 85, 142  },
  /* 144 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(NaClIllegal),
    InstAnd, 0x04, 2, 85, 143  },
  /* 145 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(NaClIllegal),
    InstSbb, 0x03, 2, 85, 144  },
  /* 146 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(NaClIllegal),
    InstAdc, 0x02, 2, 85, 145  },
  /* 147 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(NaClIllegal),
    InstOr, 0x01, 2, 85, 146  },
  /* 148 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(NaClIllegal),
    InstAdd, 0x00, 2, 85, 147  },
  /* 149 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstCmp, 0x07, 2, 67, NACL_OPCODE_NULL_OFFSET  },
  /* 150 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstXor, 0x06, 2, 89, 149  },
  /* 151 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstSub, 0x05, 2, 89, 150  },
  /* 152 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstAnd, 0x04, 2, 89, 151  },
  /* 153 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstSbb, 0x03, 2, 89, 152  },
  /* 154 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstAdc, 0x02, 2, 89, 153  },
  /* 155 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstOr, 0x01, 2, 89, 154  },
  /* 156 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstAdd, 0x00, 2, 89, 155  },
  /* 157 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstTest, 0x00, 2, 27, NACL_OPCODE_NULL_OFFSET  },
  /* 158 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstTest, 0x00, 2, 29, NACL_OPCODE_NULL_OFFSET  },
  /* 159 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstXchg, 0x00, 2, 91, NACL_OPCODE_NULL_OFFSET  },
  /* 160 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstXchg, 0x00, 2, 93, NACL_OPCODE_NULL_OFFSET  },
  /* 161 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x00, 2, 95, NACL_OPCODE_NULL_OFFSET  },
  /* 162 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMov, 0x00, 2, 97, NACL_OPCODE_NULL_OFFSET  },
  /* 163 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x00, 2, 99, NACL_OPCODE_NULL_OFFSET  },
  /* 164 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMov, 0x00, 2, 61, NACL_OPCODE_NULL_OFFSET  },
  /* 165 */
  { NACLi_386,
    NACL_IFLAG(ModRmRegSOperand) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstMov, 0x00, 2, 101, NACL_OPCODE_NULL_OFFSET  },
  /* 166 */
  { NACLi_386,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstLea, 0x00, 2, 103, NACL_OPCODE_NULL_OFFSET  },
  /* 167 */
  { NACLi_386,
    NACL_IFLAG(ModRmRegSOperand) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstMov, 0x00, 2, 105, NACL_OPCODE_NULL_OFFSET  },
  /* 168 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 169 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstPop, 0x00, 2, 107, 168  },
  /* 170 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstXchg, 0x00, 2, 109, NACL_OPCODE_NULL_OFFSET  },
  /* 171 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstXchg, 0x01, 2, 109, NACL_OPCODE_NULL_OFFSET  },
  /* 172 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstXchg, 0x02, 2, 109, NACL_OPCODE_NULL_OFFSET  },
  /* 173 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstXchg, 0x03, 2, 109, NACL_OPCODE_NULL_OFFSET  },
  /* 174 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstXchg, 0x04, 2, 109, NACL_OPCODE_NULL_OFFSET  },
  /* 175 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstXchg, 0x05, 2, 109, NACL_OPCODE_NULL_OFFSET  },
  /* 176 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstXchg, 0x06, 2, 109, NACL_OPCODE_NULL_OFFSET  },
  /* 177 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstXchg, 0x07, 2, 109, NACL_OPCODE_NULL_OFFSET  },
  /* 178 */
  { NACLi_386,
    NACL_IFLAG(OperandSize_v),
    InstCwde, 0x00, 2, 111, NACL_OPCODE_NULL_OFFSET  },
  /* 179 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w),
    InstCbw, 0x00, 2, 113, 178  },
  /* 180 */
  { NACLi_386,
    NACL_IFLAG(OperandSize_v),
    InstCdq, 0x00, 2, 115, NACL_OPCODE_NULL_OFFSET  },
  /* 181 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w),
    InstCwd, 0x00, 2, 117, 180  },
  /* 182 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_p) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(JumpInstruction),
    InstCall, 0x00, 3, 119, NACL_OPCODE_NULL_OFFSET  },
  /* 183 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(JumpInstruction),
    InstCall, 0x00, 3, 122, 182  },
  /* 184 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFwait, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 185 */
  { NACLi_386,
    NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstPushfd, 0x00, 2, 125, NACL_OPCODE_NULL_OFFSET  },
  /* 186 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(NaClIllegal),
    InstPushf, 0x00, 2, 127, 185  },
  /* 187 */
  { NACLi_386,
    NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstPopfd, 0x00, 2, 129, NACL_OPCODE_NULL_OFFSET  },
  /* 188 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(NaClIllegal),
    InstPopf, 0x00, 2, 131, 187  },
  /* 189 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstSahf, 0x00, 1, 133, NACL_OPCODE_NULL_OFFSET  },
  /* 190 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstLahf, 0x00, 1, 134, NACL_OPCODE_NULL_OFFSET  },
  /* 191 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_Addr) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x00, 2, 135, NACL_OPCODE_NULL_OFFSET  },
  /* 192 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_Addr) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMov, 0x00, 2, 137, NACL_OPCODE_NULL_OFFSET  },
  /* 193 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_Addr) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x00, 2, 139, NACL_OPCODE_NULL_OFFSET  },
  /* 194 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_Addr) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMov, 0x00, 2, 141, NACL_OPCODE_NULL_OFFSET  },
  /* 195 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_b),
    InstMovsb, 0x00, 2, 143, NACL_OPCODE_NULL_OFFSET  },
  /* 196 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_v),
    InstMovsd, 0x00, 2, 145, NACL_OPCODE_NULL_OFFSET  },
  /* 197 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w),
    InstMovsw, 0x00, 2, 147, 196  },
  /* 198 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(OperandSize_b),
    InstCmpsb, 0x00, 2, 149, NACL_OPCODE_NULL_OFFSET  },
  /* 199 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(OperandSize_v),
    InstCmpsd, 0x00, 2, 151, NACL_OPCODE_NULL_OFFSET  },
  /* 200 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w),
    InstCmpsw, 0x00, 2, 153, 199  },
  /* 201 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b),
    InstTest, 0x00, 2, 35, NACL_OPCODE_NULL_OFFSET  },
  /* 202 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstTest, 0x00, 2, 37, NACL_OPCODE_NULL_OFFSET  },
  /* 203 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_b),
    InstStosb, 0x00, 2, 155, NACL_OPCODE_NULL_OFFSET  },
  /* 204 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_v),
    InstStosd, 0x00, 2, 157, NACL_OPCODE_NULL_OFFSET  },
  /* 205 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w),
    InstStosw, 0x00, 2, 159, 204  },
  /* 206 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_b),
    InstLodsb, 0x00, 2, 161, NACL_OPCODE_NULL_OFFSET  },
  /* 207 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_v),
    InstLodsd, 0x00, 2, 163, NACL_OPCODE_NULL_OFFSET  },
  /* 208 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w),
    InstLodsw, 0x00, 2, 165, 207  },
  /* 209 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(OperandSize_b),
    InstScasb, 0x00, 2, 167, NACL_OPCODE_NULL_OFFSET  },
  /* 210 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(OperandSize_v),
    InstScasd, 0x00, 2, 169, NACL_OPCODE_NULL_OFFSET  },
  /* 211 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w),
    InstScasw, 0x00, 2, 171, 210  },
  /* 212 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x00, 2, 173, NACL_OPCODE_NULL_OFFSET  },
  /* 213 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x01, 2, 173, NACL_OPCODE_NULL_OFFSET  },
  /* 214 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x02, 2, 173, NACL_OPCODE_NULL_OFFSET  },
  /* 215 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x03, 2, 173, NACL_OPCODE_NULL_OFFSET  },
  /* 216 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x04, 2, 173, NACL_OPCODE_NULL_OFFSET  },
  /* 217 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x05, 2, 173, NACL_OPCODE_NULL_OFFSET  },
  /* 218 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x06, 2, 173, NACL_OPCODE_NULL_OFFSET  },
  /* 219 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x07, 2, 173, NACL_OPCODE_NULL_OFFSET  },
  /* 220 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMov, 0x00, 2, 175, NACL_OPCODE_NULL_OFFSET  },
  /* 221 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMov, 0x01, 2, 175, NACL_OPCODE_NULL_OFFSET  },
  /* 222 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMov, 0x02, 2, 175, NACL_OPCODE_NULL_OFFSET  },
  /* 223 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMov, 0x03, 2, 175, NACL_OPCODE_NULL_OFFSET  },
  /* 224 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMov, 0x04, 2, 175, NACL_OPCODE_NULL_OFFSET  },
  /* 225 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMov, 0x05, 2, 175, NACL_OPCODE_NULL_OFFSET  },
  /* 226 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMov, 0x06, 2, 175, NACL_OPCODE_NULL_OFFSET  },
  /* 227 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMov, 0x07, 2, 175, NACL_OPCODE_NULL_OFFSET  },
  /* 228 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstSar, 0x07, 2, 85, NACL_OPCODE_NULL_OFFSET  },
  /* 229 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstShl, 0x06, 2, 85, 228  },
  /* 230 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstShr, 0x05, 2, 85, 229  },
  /* 231 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstShl, 0x04, 2, 85, 230  },
  /* 232 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstRcr, 0x03, 2, 85, 231  },
  /* 233 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstRcl, 0x02, 2, 85, 232  },
  /* 234 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstRor, 0x01, 2, 85, 233  },
  /* 235 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstRol, 0x00, 2, 85, 234  },
  /* 236 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstSar, 0x07, 2, 89, NACL_OPCODE_NULL_OFFSET  },
  /* 237 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstShl, 0x06, 2, 89, 236  },
  /* 238 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstShr, 0x05, 2, 89, 237  },
  /* 239 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstShl, 0x04, 2, 89, 238  },
  /* 240 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstRcr, 0x03, 2, 89, 239  },
  /* 241 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstRcl, 0x02, 2, 89, 240  },
  /* 242 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstRor, 0x01, 2, 89, 241  },
  /* 243 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstRol, 0x00, 2, 89, 242  },
  /* 244 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_w) | NACL_IFLAG(NaClIllegal),
    InstRet, 0x00, 3, 177, NACL_OPCODE_NULL_OFFSET  },
  /* 245 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstRet, 0x00, 2, 177, NACL_OPCODE_NULL_OFFSET  },
  /* 246 */
  { NACLi_386,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstLes, 0x00, 2, 180, NACL_OPCODE_NULL_OFFSET  },
  /* 247 */
  { NACLi_386,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstLds, 0x00, 2, 180, NACL_OPCODE_NULL_OFFSET  },
  /* 248 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x00, 2, 182, 168  },
  /* 249 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMov, 0x00, 2, 184, 168  },
  /* 250 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_w) | NACL_IFLAG(OpcodeHasImmed2_b) | NACL_IFLAG(NaClIllegal),
    InstEnter, 0x00, 4, 186, NACL_OPCODE_NULL_OFFSET  },
  /* 251 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstLeave, 0x00, 2, 190, NACL_OPCODE_NULL_OFFSET  },
  /* 252 */
  { NACLi_RETURN,
    NACL_IFLAG(OpcodeHasImmed_w) | NACL_IFLAG(NaClIllegal),
    InstRet, 0x00, 3, 177, NACL_OPCODE_NULL_OFFSET  },
  /* 253 */
  { NACLi_RETURN,
    NACL_IFLAG(NaClIllegal),
    InstRet, 0x00, 2, 177, NACL_OPCODE_NULL_OFFSET  },
  /* 254 */
  { NACLi_SYSTEM,
    NACL_IFLAG(NaClIllegal),
    InstInt3, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 255 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(NaClIllegal),
    InstInt, 0x00, 1, 9, NACL_OPCODE_NULL_OFFSET  },
  /* 256 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstInto, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 257 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(NaClIllegal),
    InstIret, 0x00, 2, 177, NACL_OPCODE_NULL_OFFSET  },
  /* 258 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstIretd, 0x00, 2, 177, 257  },
  /* 259 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSar, 0x07, 2, 192, NACL_OPCODE_NULL_OFFSET  },
  /* 260 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstShl, 0x06, 2, 192, 259  },
  /* 261 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstShr, 0x05, 2, 192, 260  },
  /* 262 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstShl, 0x04, 2, 192, 261  },
  /* 263 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstRcr, 0x03, 2, 192, 262  },
  /* 264 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstRcl, 0x02, 2, 192, 263  },
  /* 265 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstRor, 0x01, 2, 192, 264  },
  /* 266 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstRol, 0x00, 2, 192, 265  },
  /* 267 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstSar, 0x07, 2, 194, NACL_OPCODE_NULL_OFFSET  },
  /* 268 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstShl, 0x06, 2, 194, 267  },
  /* 269 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstShr, 0x05, 2, 194, 268  },
  /* 270 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstShl, 0x04, 2, 194, 269  },
  /* 271 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstRcr, 0x03, 2, 194, 270  },
  /* 272 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstRcl, 0x02, 2, 194, 271  },
  /* 273 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstRor, 0x01, 2, 194, 272  },
  /* 274 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstRol, 0x00, 2, 194, 273  },
  /* 275 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSar, 0x07, 2, 196, NACL_OPCODE_NULL_OFFSET  },
  /* 276 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstShl, 0x06, 2, 196, 275  },
  /* 277 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstShr, 0x05, 2, 196, 276  },
  /* 278 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstShl, 0x04, 2, 196, 277  },
  /* 279 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstRcr, 0x03, 2, 196, 278  },
  /* 280 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstRcl, 0x02, 2, 196, 279  },
  /* 281 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstRor, 0x01, 2, 196, 280  },
  /* 282 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstRol, 0x00, 2, 196, 281  },
  /* 283 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstSar, 0x07, 2, 198, NACL_OPCODE_NULL_OFFSET  },
  /* 284 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstShl, 0x06, 2, 198, 283  },
  /* 285 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstShr, 0x05, 2, 198, 284  },
  /* 286 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstShl, 0x04, 2, 198, 285  },
  /* 287 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstRcr, 0x03, 2, 198, 286  },
  /* 288 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstRcl, 0x02, 2, 198, 287  },
  /* 289 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstRor, 0x01, 2, 198, 288  },
  /* 290 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstRol, 0x00, 2, 198, 289  },
  /* 291 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(NaClIllegal),
    InstAam, 0x00, 2, 200, NACL_OPCODE_NULL_OFFSET  },
  /* 292 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(NaClIllegal),
    InstAad, 0x00, 2, 200, NACL_OPCODE_NULL_OFFSET  },
  /* 293 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstSalc, 0x00, 1, 202, NACL_OPCODE_NULL_OFFSET  },
  /* 294 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstXlat, 0x00, 2, 203, NACL_OPCODE_NULL_OFFSET  },
  /* 295 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFdivr, 0x07, 2, 205, NACL_OPCODE_NULL_OFFSET  },
  /* 296 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFdiv, 0x06, 2, 205, 295  },
  /* 297 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFsubr, 0x05, 2, 205, 296  },
  /* 298 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFsub, 0x04, 2, 205, 297  },
  /* 299 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFcomp, 0x03, 2, 207, 298  },
  /* 300 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFcom, 0x02, 2, 207, 299  },
  /* 301 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFmul, 0x01, 2, 205, 300  },
  /* 302 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFadd, 0x00, 2, 205, 301  },
  /* 303 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFnstcw, 0x07, 1, 209, NACL_OPCODE_NULL_OFFSET  },
  /* 304 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16),
    InstFnstenv, 0x06, 1, 210, 303  },
  /* 305 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFldcw, 0x05, 1, 211, 304  },
  /* 306 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16),
    InstFldenv, 0x04, 1, 212, 305  },
  /* 307 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFstp, 0x03, 2, 213, 306  },
  /* 308 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFst, 0x02, 2, 213, 307  },
  /* 309 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x01, 0, 0, 308  },
  /* 310 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFld, 0x00, 2, 215, 309  },
  /* 311 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFidivr, 0x07, 2, 205, NACL_OPCODE_NULL_OFFSET  },
  /* 312 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFidiv, 0x06, 2, 205, 311  },
  /* 313 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFisubr, 0x05, 2, 205, 312  },
  /* 314 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFisub, 0x04, 2, 205, 313  },
  /* 315 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFicomp, 0x03, 2, 205, 314  },
  /* 316 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFicom, 0x02, 2, 205, 315  },
  /* 317 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFimul, 0x01, 2, 205, 316  },
  /* 318 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFiadd, 0x00, 2, 205, 317  },
  /* 319 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16),
    InstFstp, 0x07, 2, 217, NACL_OPCODE_NULL_OFFSET  },
  /* 320 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x06, 0, 0, 319  },
  /* 321 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16),
    InstFld, 0x05, 2, 219, 320  },
  /* 322 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x04, 0, 0, 321  },
  /* 323 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFistp, 0x03, 2, 213, 322  },
  /* 324 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFist, 0x02, 2, 213, 323  },
  /* 325 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFisttp, 0x01, 2, 213, 324  },
  /* 326 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFild, 0x00, 2, 215, 325  },
  /* 327 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFdivr, 0x07, 2, 221, NACL_OPCODE_NULL_OFFSET  },
  /* 328 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFdiv, 0x06, 2, 221, 327  },
  /* 329 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFsubr, 0x05, 2, 221, 328  },
  /* 330 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFsub, 0x04, 2, 221, 329  },
  /* 331 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFcomp, 0x03, 2, 223, 330  },
  /* 332 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFcom, 0x02, 2, 223, 331  },
  /* 333 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFmul, 0x01, 2, 221, 332  },
  /* 334 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFadd, 0x00, 2, 221, 333  },
  /* 335 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFnstsw, 0x07, 1, 209, NACL_OPCODE_NULL_OFFSET  },
  /* 336 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16),
    InstFnsave, 0x06, 1, 210, 335  },
  /* 337 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x05, 0, 0, 336  },
  /* 338 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16),
    InstFrstor, 0x04, 1, 212, 337  },
  /* 339 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFstp, 0x03, 2, 225, 338  },
  /* 340 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFst, 0x02, 2, 225, 339  },
  /* 341 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFisttp, 0x01, 2, 225, 340  },
  /* 342 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFld, 0x00, 2, 227, 341  },
  /* 343 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFidivr, 0x07, 2, 229, NACL_OPCODE_NULL_OFFSET  },
  /* 344 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFidiv, 0x06, 2, 229, 343  },
  /* 345 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFisubr, 0x05, 2, 229, 344  },
  /* 346 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFisub, 0x04, 2, 229, 345  },
  /* 347 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFicomp, 0x03, 2, 231, 346  },
  /* 348 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFicom, 0x02, 2, 231, 347  },
  /* 349 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFimul, 0x01, 2, 229, 348  },
  /* 350 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFiadd, 0x00, 2, 229, 349  },
  /* 351 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16),
    InstFistp, 0x07, 2, 217, NACL_OPCODE_NULL_OFFSET  },
  /* 352 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16),
    InstFbstp, 0x06, 2, 217, 351  },
  /* 353 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16),
    InstFild, 0x05, 2, 219, 352  },
  /* 354 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16),
    InstFbld, 0x04, 2, 219, 353  },
  /* 355 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFistp, 0x03, 2, 233, 354  },
  /* 356 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFist, 0x02, 2, 233, 355  },
  /* 357 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFisttp, 0x01, 2, 233, 356  },
  /* 358 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFild, 0x00, 2, 235, 357  },
  /* 359 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump),
    InstLoopne, 0x00, 2, 81, NACL_OPCODE_NULL_OFFSET  },
  /* 360 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump),
    InstLoope, 0x00, 2, 81, NACL_OPCODE_NULL_OFFSET  },
  /* 361 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(JumpInstruction),
    InstLoop, 0x00, 2, 81, NACL_OPCODE_NULL_OFFSET  },
  /* 362 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(AddressSize_v) | NACL_IFLAG(ConditionalJump),
    InstJecxz, 0x00, 3, 237, NACL_OPCODE_NULL_OFFSET  },
  /* 363 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(AddressSize_w) | NACL_IFLAG(ConditionalJump),
    InstJcxz, 0x00, 3, 240, 362  },
  /* 364 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(NaClIllegal),
    InstIn, 0x00, 2, 243, NACL_OPCODE_NULL_OFFSET  },
  /* 365 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstIn, 0x00, 2, 245, NACL_OPCODE_NULL_OFFSET  },
  /* 366 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(NaClIllegal),
    InstOut, 0x00, 2, 247, NACL_OPCODE_NULL_OFFSET  },
  /* 367 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstOut, 0x00, 2, 249, NACL_OPCODE_NULL_OFFSET  },
  /* 368 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(JumpInstruction),
    InstCall, 0x00, 3, 251, NACL_OPCODE_NULL_OFFSET  },
  /* 369 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(JumpInstruction),
    InstJmp, 0x00, 2, 254, NACL_OPCODE_NULL_OFFSET  },
  /* 370 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_p) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(JumpInstruction),
    InstJmp, 0x00, 2, 256, NACL_OPCODE_NULL_OFFSET  },
  /* 371 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(JumpInstruction),
    InstJmp, 0x00, 2, 81, NACL_OPCODE_NULL_OFFSET  },
  /* 372 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstIn, 0x00, 2, 258, NACL_OPCODE_NULL_OFFSET  },
  /* 373 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstIn, 0x00, 2, 260, NACL_OPCODE_NULL_OFFSET  },
  /* 374 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstOut, 0x00, 2, 262, NACL_OPCODE_NULL_OFFSET  },
  /* 375 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstOut, 0x00, 2, 264, NACL_OPCODE_NULL_OFFSET  },
  /* 376 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstInt1, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 377 */
  { NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstHlt, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 378 */
  { NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstCmc, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 379 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstIdiv, 0x07, 3, 266, NACL_OPCODE_NULL_OFFSET  },
  /* 380 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstDiv, 0x06, 3, 266, 379  },
  /* 381 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstImul, 0x05, 3, 266, 380  },
  /* 382 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstMul, 0x04, 3, 266, 381  },
  /* 383 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstNeg, 0x03, 1, 0, 382  },
  /* 384 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstNot, 0x02, 1, 0, 383  },
  /* 385 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstTest, 0x01, 2, 83, 384  },
  /* 386 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstTest, 0x00, 2, 83, 385  },
  /* 387 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstIdiv, 0x07, 3, 269, NACL_OPCODE_NULL_OFFSET  },
  /* 388 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstDiv, 0x06, 3, 269, 387  },
  /* 389 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstImul, 0x05, 3, 269, 388  },
  /* 390 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMul, 0x04, 3, 269, 389  },
  /* 391 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstNeg, 0x03, 1, 2, 390  },
  /* 392 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstNot, 0x02, 1, 2, 391  },
  /* 393 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstTest, 0x01, 2, 62, 392  },
  /* 394 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstTest, 0x00, 2, 62, 393  },
  /* 395 */
  { NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstClc, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 396 */
  { NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstStc, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 397 */
  { NACLi_SYSTEM,
    NACL_IFLAG(NaClIllegal),
    InstCli, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 398 */
  { NACLi_SYSTEM,
    NACL_IFLAG(NaClIllegal),
    InstSti, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 399 */
  { NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstCld, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 400 */
  { NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstStd, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 401 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x07, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 402 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x06, 0, 0, 401  },
  /* 403 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x05, 0, 0, 402  },
  /* 404 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x04, 0, 0, 403  },
  /* 405 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x03, 0, 0, 404  },
  /* 406 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x02, 0, 0, 405  },
  /* 407 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstDec, 0x01, 1, 0, 406  },
  /* 408 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstInc, 0x00, 1, 0, 407  },
  /* 409 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstPush, 0x06, 2, 272, 401  },
  /* 410 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(JumpInstruction),
    InstJmp, 0x05, 2, 274, 409  },
  /* 411 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(JumpInstruction),
    InstJmp, 0x04, 2, 276, 410  },
  /* 412 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(JumpInstruction),
    InstCall, 0x03, 3, 278, 411  },
  /* 413 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(JumpInstruction),
    InstCall, 0x02, 3, 281, 412  },
  /* 414 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstDec, 0x01, 1, 2, 413  },
  /* 415 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstInc, 0x00, 1, 2, 414  },
  /* 416 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstVerw, 0x05, 1, 284, 402  },
  /* 417 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstVerr, 0x04, 1, 284, 416  },
  /* 418 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstLtr, 0x03, 1, 106, 417  },
  /* 419 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstLldt, 0x02, 1, 106, 418  },
  /* 420 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstStr, 0x01, 1, 101, 419  },
  /* 421 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstSldt, 0x00, 1, 101, 420  },
  /* 422 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvlpg, 0x07, 1, 285, 401  },
  /* 423 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstLmsw, 0x06, 1, 106, 422  },
  /* 424 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x05, 0, 0, 423  },
  /* 425 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstSmsw, 0x04, 1, 101, 424  },
  /* 426 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x03, 0, 0, 425  },
  /* 427 */
  { NACLi_SVM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvlpga, 0x73, 2, 286, 426  },
  /* 428 */
  { NACLi_SVM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstSkinit, 0x63, 2, 288, 427  },
  /* 429 */
  { NACLi_SVM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstClgi, 0x53, 0, 0, 428  },
  /* 430 */
  { NACLi_SVM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstStgi, 0x43, 0, 0, 429  },
  /* 431 */
  { NACLi_SVM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstVmsave, 0x33, 1, 286, 430  },
  /* 432 */
  { NACLi_SVM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstVmload, 0x23, 1, 286, 431  },
  /* 433 */
  { NACLi_SVM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstVmmcall, 0x13, 0, 0, 432  },
  /* 434 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstLidt, 0x03, 1, 290, 433  },
  /* 435 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstLgdt, 0x02, 1, 290, 434  },
  /* 436 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x01, 0, 0, 435  },
  /* 437 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstMwait, 0x11, 2, 291, 436  },
  /* 438 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstMonitor, 0x01, 3, 293, 437  },
  /* 439 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstSidt, 0x01, 1, 296, 438  },
  /* 440 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstSgdt, 0x00, 1, 296, 439  },
  /* 441 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstLar, 0x00, 2, 297, NACL_OPCODE_NULL_OFFSET  },
  /* 442 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstLsl, 0x00, 2, 297, NACL_OPCODE_NULL_OFFSET  },
  /* 443 */
  { NACLi_SYSTEM,
    NACL_IFLAG(NaClIllegal),
    InstClts, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 444 */
  { NACLi_SYSTEM,
    NACL_IFLAG(NaClIllegal),
    InstInvd, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 445 */
  { NACLi_SYSTEM,
    NACL_IFLAG(NaClIllegal),
    InstWbinvd, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 446 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstUd2, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 447 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstPrefetch_reserved, 0x07, 1, 299, NACL_OPCODE_NULL_OFFSET  },
  /* 448 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstPrefetch_reserved, 0x06, 1, 299, 447  },
  /* 449 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstPrefetch_reserved, 0x05, 1, 299, 448  },
  /* 450 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstPrefetch_reserved, 0x04, 1, 299, 449  },
  /* 451 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstPrefetch_modified, 0x03, 1, 299, 450  },
  /* 452 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstPrefetch_reserved, 0x02, 1, 299, 451  },
  /* 453 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstPrefetch_modified, 0x01, 1, 299, 452  },
  /* 454 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstPrefetch_exclusive, 0x00, 1, 299, 453  },
  /* 455 */
  { NACLi_3DNOW,
    NACL_EMPTY_IFLAGS,
    InstFemms, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 456 */
  { NACLi_INVALID,
    NACL_IFLAG(Opcode0F0F) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00, 3, 300, NACL_OPCODE_NULL_OFFSET  },
  /* 457 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstMovups, 0x00, 2, 303, NACL_OPCODE_NULL_OFFSET  },
  /* 458 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstMovups, 0x00, 2, 305, NACL_OPCODE_NULL_OFFSET  },
  /* 459 */
  { NACLi_SSE,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMovhlps, 0x00, 2, 307, NACL_OPCODE_NULL_OFFSET  },
  /* 460 */
  { NACLi_SSE,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMovlps, 0x00, 2, 309, 459  },
  /* 461 */
  { NACLi_SSE,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMovlps, 0x00, 2, 311, NACL_OPCODE_NULL_OFFSET  },
  /* 462 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstUnpcklps, 0x00, 2, 313, NACL_OPCODE_NULL_OFFSET  },
  /* 463 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstUnpckhps, 0x00, 2, 313, NACL_OPCODE_NULL_OFFSET  },
  /* 464 */
  { NACLi_SSE,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMovlhps, 0x00, 2, 307, NACL_OPCODE_NULL_OFFSET  },
  /* 465 */
  { NACLi_SSE,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMovhps, 0x00, 2, 309, 464  },
  /* 466 */
  { NACLi_SSE,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMovhps, 0x00, 2, 311, NACL_OPCODE_NULL_OFFSET  },
  /* 467 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm),
    InstNop, 0x07, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 468 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm),
    InstNop, 0x06, 0, 0, 467  },
  /* 469 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm),
    InstNop, 0x05, 0, 0, 468  },
  /* 470 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm),
    InstNop, 0x04, 0, 0, 469  },
  /* 471 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstPrefetcht2, 0x03, 1, 299, 470  },
  /* 472 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstPrefetcht1, 0x02, 1, 299, 471  },
  /* 473 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstPrefetcht0, 0x01, 1, 299, 472  },
  /* 474 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstPrefetchnta, 0x00, 1, 299, 473  },
  /* 475 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstNop, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 476 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm),
    InstNop, 0x00, 0, 0, 475  },
  /* 477 */
  { NACLi_SYSTEM,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstMov, 0x00, 2, 315, NACL_OPCODE_NULL_OFFSET  },
  /* 478 */
  { NACLi_SYSTEM,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstMov, 0x00, 2, 317, NACL_OPCODE_NULL_OFFSET  },
  /* 479 */
  { NACLi_SYSTEM,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstMov, 0x00, 2, 319, NACL_OPCODE_NULL_OFFSET  },
  /* 480 */
  { NACLi_SYSTEM,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstMov, 0x00, 2, 321, NACL_OPCODE_NULL_OFFSET  },
  /* 481 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstMovaps, 0x00, 2, 303, NACL_OPCODE_NULL_OFFSET  },
  /* 482 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstMovaps, 0x00, 2, 305, NACL_OPCODE_NULL_OFFSET  },
  /* 483 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstCvtpi2ps, 0x00, 2, 323, NACL_OPCODE_NULL_OFFSET  },
  /* 484 */
  { NACLi_SSE,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMovntps, 0x00, 2, 325, NACL_OPCODE_NULL_OFFSET  },
  /* 485 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstCvttps2pi, 0x00, 2, 327, NACL_OPCODE_NULL_OFFSET  },
  /* 486 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstCvtps2pi, 0x00, 2, 327, NACL_OPCODE_NULL_OFFSET  },
  /* 487 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstUcomiss, 0x00, 2, 329, NACL_OPCODE_NULL_OFFSET  },
  /* 488 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstComiss, 0x00, 2, 331, NACL_OPCODE_NULL_OFFSET  },
  /* 489 */
  { NACLi_RDMSR,
    NACL_IFLAG(NaClIllegal),
    InstWrmsr, 0x00, 3, 333, NACL_OPCODE_NULL_OFFSET  },
  /* 490 */
  { NACLi_RDTSC,
    NACL_EMPTY_IFLAGS,
    InstRdtsc, 0x00, 2, 336, NACL_OPCODE_NULL_OFFSET  },
  /* 491 */
  { NACLi_RDMSR,
    NACL_IFLAG(NaClIllegal),
    InstRdmsr, 0x00, 3, 338, NACL_OPCODE_NULL_OFFSET  },
  /* 492 */
  { NACLi_SYSTEM,
    NACL_IFLAG(NaClIllegal),
    InstRdpmc, 0x00, 3, 338, NACL_OPCODE_NULL_OFFSET  },
  /* 493 */
  { NACLi_SYSENTER,
    NACL_IFLAG(NaClIllegal),
    InstSysenter, 0x00, 4, 341, NACL_OPCODE_NULL_OFFSET  },
  /* 494 */
  { NACLi_SYSENTER,
    NACL_IFLAG(NaClIllegal),
    InstSysexit, 0x00, 6, 345, NACL_OPCODE_NULL_OFFSET  },
  /* 495 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstCmovo, 0x00, 2, 61, NACL_OPCODE_NULL_OFFSET  },
  /* 496 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstCmovno, 0x00, 2, 61, NACL_OPCODE_NULL_OFFSET  },
  /* 497 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstCmovb, 0x00, 2, 61, NACL_OPCODE_NULL_OFFSET  },
  /* 498 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstCmovnb, 0x00, 2, 61, NACL_OPCODE_NULL_OFFSET  },
  /* 499 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstCmovz, 0x00, 2, 61, NACL_OPCODE_NULL_OFFSET  },
  /* 500 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstCmovnz, 0x00, 2, 61, NACL_OPCODE_NULL_OFFSET  },
  /* 501 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstCmovbe, 0x00, 2, 61, NACL_OPCODE_NULL_OFFSET  },
  /* 502 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstCmovnbe, 0x00, 2, 61, NACL_OPCODE_NULL_OFFSET  },
  /* 503 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstCmovs, 0x00, 2, 61, NACL_OPCODE_NULL_OFFSET  },
  /* 504 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstCmovns, 0x00, 2, 61, NACL_OPCODE_NULL_OFFSET  },
  /* 505 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstCmovp, 0x00, 2, 61, NACL_OPCODE_NULL_OFFSET  },
  /* 506 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstCmovnp, 0x00, 2, 61, NACL_OPCODE_NULL_OFFSET  },
  /* 507 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstCmovl, 0x00, 2, 61, NACL_OPCODE_NULL_OFFSET  },
  /* 508 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstCmovnl, 0x00, 2, 61, NACL_OPCODE_NULL_OFFSET  },
  /* 509 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstCmovle, 0x00, 2, 61, NACL_OPCODE_NULL_OFFSET  },
  /* 510 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstCmovnle, 0x00, 2, 61, NACL_OPCODE_NULL_OFFSET  },
  /* 511 */
  { NACLi_SSE,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMovmskps, 0x00, 2, 351, NACL_OPCODE_NULL_OFFSET  },
  /* 512 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstSqrtps, 0x00, 2, 303, NACL_OPCODE_NULL_OFFSET  },
  /* 513 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstRsqrtps, 0x00, 2, 303, NACL_OPCODE_NULL_OFFSET  },
  /* 514 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstRcpps, 0x00, 2, 303, NACL_OPCODE_NULL_OFFSET  },
  /* 515 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstAndps, 0x00, 2, 353, NACL_OPCODE_NULL_OFFSET  },
  /* 516 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstAndnps, 0x00, 2, 353, NACL_OPCODE_NULL_OFFSET  },
  /* 517 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstOrps, 0x00, 2, 353, NACL_OPCODE_NULL_OFFSET  },
  /* 518 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstXorps, 0x00, 2, 353, NACL_OPCODE_NULL_OFFSET  },
  /* 519 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstAddps, 0x00, 2, 353, NACL_OPCODE_NULL_OFFSET  },
  /* 520 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstMulps, 0x00, 2, 353, NACL_OPCODE_NULL_OFFSET  },
  /* 521 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm),
    InstCvtps2pd, 0x00, 2, 355, NACL_OPCODE_NULL_OFFSET  },
  /* 522 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm),
    InstCvtdq2ps, 0x00, 2, 357, NACL_OPCODE_NULL_OFFSET  },
  /* 523 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstSubps, 0x00, 2, 353, NACL_OPCODE_NULL_OFFSET  },
  /* 524 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstMinps, 0x00, 2, 353, NACL_OPCODE_NULL_OFFSET  },
  /* 525 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstDivps, 0x00, 2, 353, NACL_OPCODE_NULL_OFFSET  },
  /* 526 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstMaxps, 0x00, 2, 353, NACL_OPCODE_NULL_OFFSET  },
  /* 527 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPunpcklbw, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 528 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPunpcklwd, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 529 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPunpckldq, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 530 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPacksswb, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 531 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPcmpgtb, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 532 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPcmpgtw, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 533 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPcmpgtd, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 534 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPackuswb, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 535 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPunpckhbw, 0x00, 2, 361, NACL_OPCODE_NULL_OFFSET  },
  /* 536 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPunpckhwd, 0x00, 2, 361, NACL_OPCODE_NULL_OFFSET  },
  /* 537 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPunpckhdq, 0x00, 2, 361, NACL_OPCODE_NULL_OFFSET  },
  /* 538 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPackssdw, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 539 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstMovd, 0x00, 2, 363, NACL_OPCODE_NULL_OFFSET  },
  /* 540 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstMovq, 0x00, 2, 365, NACL_OPCODE_NULL_OFFSET  },
  /* 541 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstPshufw, 0x00, 3, 367, NACL_OPCODE_NULL_OFFSET  },
  /* 542 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstPsllw, 0x06, 2, 370, 401  },
  /* 543 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x05, 0, 0, 542  },
  /* 544 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstPsraw, 0x04, 2, 370, 543  },
  /* 545 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x03, 0, 0, 544  },
  /* 546 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstPsrlw, 0x02, 2, 370, 545  },
  /* 547 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x01, 0, 0, 546  },
  /* 548 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00, 0, 0, 547  },
  /* 549 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstPslld, 0x06, 2, 370, 401  },
  /* 550 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x05, 0, 0, 549  },
  /* 551 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstPsrad, 0x04, 2, 370, 550  },
  /* 552 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x03, 0, 0, 551  },
  /* 553 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstPsrld, 0x02, 2, 370, 552  },
  /* 554 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x01, 0, 0, 553  },
  /* 555 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00, 0, 0, 554  },
  /* 556 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstPsllq, 0x06, 2, 370, 401  },
  /* 557 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x05, 0, 0, 556  },
  /* 558 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x04, 0, 0, 557  },
  /* 559 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x03, 0, 0, 558  },
  /* 560 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstPsrlq, 0x02, 2, 370, 559  },
  /* 561 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x01, 0, 0, 560  },
  /* 562 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00, 0, 0, 561  },
  /* 563 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPcmpeqb, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 564 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPcmpeqw, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 565 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPcmpeqd, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 566 */
  { NACLi_MMX,
    NACL_EMPTY_IFLAGS,
    InstEmms, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 567 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstMovd, 0x00, 2, 372, NACL_OPCODE_NULL_OFFSET  },
  /* 568 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstMovq, 0x00, 2, 374, NACL_OPCODE_NULL_OFFSET  },
  /* 569 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(ConditionalJump),
    InstJo, 0x00, 2, 254, NACL_OPCODE_NULL_OFFSET  },
  /* 570 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(ConditionalJump),
    InstJno, 0x00, 2, 254, NACL_OPCODE_NULL_OFFSET  },
  /* 571 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(ConditionalJump),
    InstJb, 0x00, 2, 254, NACL_OPCODE_NULL_OFFSET  },
  /* 572 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(ConditionalJump),
    InstJnb, 0x00, 2, 254, NACL_OPCODE_NULL_OFFSET  },
  /* 573 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(ConditionalJump),
    InstJz, 0x00, 2, 254, NACL_OPCODE_NULL_OFFSET  },
  /* 574 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(ConditionalJump),
    InstJnz, 0x00, 2, 254, NACL_OPCODE_NULL_OFFSET  },
  /* 575 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(ConditionalJump),
    InstJbe, 0x00, 2, 254, NACL_OPCODE_NULL_OFFSET  },
  /* 576 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(ConditionalJump),
    InstJnbe, 0x00, 2, 254, NACL_OPCODE_NULL_OFFSET  },
  /* 577 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(ConditionalJump),
    InstJs, 0x00, 2, 254, NACL_OPCODE_NULL_OFFSET  },
  /* 578 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(ConditionalJump),
    InstJns, 0x00, 2, 254, NACL_OPCODE_NULL_OFFSET  },
  /* 579 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(ConditionalJump),
    InstJp, 0x00, 2, 254, NACL_OPCODE_NULL_OFFSET  },
  /* 580 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(ConditionalJump),
    InstJnp, 0x00, 2, 254, NACL_OPCODE_NULL_OFFSET  },
  /* 581 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(ConditionalJump),
    InstJl, 0x00, 2, 254, NACL_OPCODE_NULL_OFFSET  },
  /* 582 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(ConditionalJump),
    InstJnl, 0x00, 2, 254, NACL_OPCODE_NULL_OFFSET  },
  /* 583 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(ConditionalJump),
    InstJle, 0x00, 2, 254, NACL_OPCODE_NULL_OFFSET  },
  /* 584 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(ConditionalJump),
    InstJnle, 0x00, 2, 254, NACL_OPCODE_NULL_OFFSET  },
  /* 585 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSeto, 0x00, 1, 95, NACL_OPCODE_NULL_OFFSET  },
  /* 586 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetno, 0x00, 1, 95, NACL_OPCODE_NULL_OFFSET  },
  /* 587 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetb, 0x00, 1, 95, NACL_OPCODE_NULL_OFFSET  },
  /* 588 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetnb, 0x00, 1, 95, NACL_OPCODE_NULL_OFFSET  },
  /* 589 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetz, 0x00, 1, 95, NACL_OPCODE_NULL_OFFSET  },
  /* 590 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetnz, 0x00, 1, 95, NACL_OPCODE_NULL_OFFSET  },
  /* 591 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetbe, 0x00, 1, 95, NACL_OPCODE_NULL_OFFSET  },
  /* 592 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetnbe, 0x00, 1, 95, NACL_OPCODE_NULL_OFFSET  },
  /* 593 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSets, 0x00, 1, 95, NACL_OPCODE_NULL_OFFSET  },
  /* 594 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetns, 0x00, 1, 95, NACL_OPCODE_NULL_OFFSET  },
  /* 595 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetp, 0x00, 1, 95, NACL_OPCODE_NULL_OFFSET  },
  /* 596 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetnp, 0x00, 1, 95, NACL_OPCODE_NULL_OFFSET  },
  /* 597 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetl, 0x00, 1, 95, NACL_OPCODE_NULL_OFFSET  },
  /* 598 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetnl, 0x00, 1, 95, NACL_OPCODE_NULL_OFFSET  },
  /* 599 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetle, 0x00, 1, 95, NACL_OPCODE_NULL_OFFSET  },
  /* 600 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetnle, 0x00, 1, 95, NACL_OPCODE_NULL_OFFSET  },
  /* 601 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstPush, 0x00, 2, 376, NACL_OPCODE_NULL_OFFSET  },
  /* 602 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstPop, 0x00, 2, 378, NACL_OPCODE_NULL_OFFSET  },
  /* 603 */
  { NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstCpuid, 0x00, 4, 380, NACL_OPCODE_NULL_OFFSET  },
  /* 604 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstBt, 0x00, 2, 29, NACL_OPCODE_NULL_OFFSET  },
  /* 605 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstShld, 0x00, 3, 384, NACL_OPCODE_NULL_OFFSET  },
  /* 606 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstShld, 0x00, 3, 387, NACL_OPCODE_NULL_OFFSET  },
  /* 607 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstPush, 0x00, 2, 390, NACL_OPCODE_NULL_OFFSET  },
  /* 608 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstPop, 0x00, 2, 392, NACL_OPCODE_NULL_OFFSET  },
  /* 609 */
  { NACLi_SYSTEM,
    NACL_IFLAG(NaClIllegal),
    InstRsm, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 610 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstBts, 0x00, 2, 2, NACL_OPCODE_NULL_OFFSET  },
  /* 611 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstShrd, 0x00, 3, 394, NACL_OPCODE_NULL_OFFSET  },
  /* 612 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstShrd, 0x00, 3, 397, NACL_OPCODE_NULL_OFFSET  },
  /* 613 */
  { NACLi_SFENCE_CLFLUSH,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstClflush, 0x07, 1, 285, NACL_OPCODE_NULL_OFFSET  },
  /* 614 */
  { NACLi_SFENCE_CLFLUSH,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstSfence, 0x77, 0, 0, 613  },
  /* 615 */
  { NACLi_SFENCE_CLFLUSH,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstSfence, 0x67, 0, 0, 614  },
  /* 616 */
  { NACLi_SFENCE_CLFLUSH,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstSfence, 0x57, 0, 0, 615  },
  /* 617 */
  { NACLi_SFENCE_CLFLUSH,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstSfence, 0x47, 0, 0, 616  },
  /* 618 */
  { NACLi_SFENCE_CLFLUSH,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstSfence, 0x37, 0, 0, 617  },
  /* 619 */
  { NACLi_SFENCE_CLFLUSH,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstSfence, 0x27, 0, 0, 618  },
  /* 620 */
  { NACLi_SFENCE_CLFLUSH,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstSfence, 0x17, 0, 0, 619  },
  /* 621 */
  { NACLi_SFENCE_CLFLUSH,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstSfence, 0x07, 0, 0, 620  },
  /* 622 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMfence, 0x76, 0, 0, 621  },
  /* 623 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMfence, 0x66, 0, 0, 622  },
  /* 624 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMfence, 0x56, 0, 0, 623  },
  /* 625 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMfence, 0x46, 0, 0, 624  },
  /* 626 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMfence, 0x36, 0, 0, 625  },
  /* 627 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMfence, 0x26, 0, 0, 626  },
  /* 628 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMfence, 0x16, 0, 0, 627  },
  /* 629 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMfence, 0x06, 0, 0, 628  },
  /* 630 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstLfence, 0x75, 0, 0, 629  },
  /* 631 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstLfence, 0x65, 0, 0, 630  },
  /* 632 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstLfence, 0x55, 0, 0, 631  },
  /* 633 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstLfence, 0x45, 0, 0, 632  },
  /* 634 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstLfence, 0x35, 0, 0, 633  },
  /* 635 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstLfence, 0x25, 0, 0, 634  },
  /* 636 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstLfence, 0x15, 0, 0, 635  },
  /* 637 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstLfence, 0x05, 0, 0, 636  },
  /* 638 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x04, 0, 0, 637  },
  /* 639 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstStmxcsr, 0x03, 1, 213, 638  },
  /* 640 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstLdmxcsr, 0x02, 1, 206, 639  },
  /* 641 */
  { NACLi_FXSAVE,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(NaClIllegal),
    InstFxrstor, 0x01, 1, 212, 640  },
  /* 642 */
  { NACLi_FXSAVE,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(NaClIllegal),
    InstFxsave, 0x00, 1, 210, 641  },
  /* 643 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstImul, 0x00, 2, 6, NACL_OPCODE_NULL_OFFSET  },
  /* 644 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstCmpxchg, 0x00, 3, 400, NACL_OPCODE_NULL_OFFSET  },
  /* 645 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstCmpxchg, 0x00, 3, 403, NACL_OPCODE_NULL_OFFSET  },
  /* 646 */
  { NACLi_386,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstLss, 0x00, 2, 180, NACL_OPCODE_NULL_OFFSET  },
  /* 647 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstBtr, 0x00, 2, 2, NACL_OPCODE_NULL_OFFSET  },
  /* 648 */
  { NACLi_386,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstLfs, 0x00, 2, 180, NACL_OPCODE_NULL_OFFSET  },
  /* 649 */
  { NACLi_386,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstLgs, 0x00, 2, 180, NACL_OPCODE_NULL_OFFSET  },
  /* 650 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMovzx, 0x00, 2, 406, NACL_OPCODE_NULL_OFFSET  },
  /* 651 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMovzx, 0x00, 2, 408, NACL_OPCODE_NULL_OFFSET  },
  /* 652 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstBtc, 0x07, 2, 89, 168  },
  /* 653 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstBtr, 0x06, 2, 89, 652  },
  /* 654 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstBts, 0x05, 2, 89, 653  },
  /* 655 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstBt, 0x04, 2, 67, 654  },
  /* 656 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstBtc, 0x00, 2, 2, NACL_OPCODE_NULL_OFFSET  },
  /* 657 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstBsf, 0x00, 2, 61, NACL_OPCODE_NULL_OFFSET  },
  /* 658 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstBsr, 0x00, 2, 61, NACL_OPCODE_NULL_OFFSET  },
  /* 659 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMovsx, 0x00, 2, 406, NACL_OPCODE_NULL_OFFSET  },
  /* 660 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMovsx, 0x00, 2, 408, NACL_OPCODE_NULL_OFFSET  },
  /* 661 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstXadd, 0x00, 2, 91, NACL_OPCODE_NULL_OFFSET  },
  /* 662 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstXadd, 0x00, 2, 93, NACL_OPCODE_NULL_OFFSET  },
  /* 663 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstCmpps, 0x00, 3, 410, NACL_OPCODE_NULL_OFFSET  },
  /* 664 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstMovnti, 0x00, 2, 413, NACL_OPCODE_NULL_OFFSET  },
  /* 665 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstPinsrw, 0x00, 3, 415, NACL_OPCODE_NULL_OFFSET  },
  /* 666 */
  { NACLi_SSE41,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstPextrw, 0x00, 3, 418, NACL_OPCODE_NULL_OFFSET  },
  /* 667 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstShufps, 0x00, 3, 410, NACL_OPCODE_NULL_OFFSET  },
  /* 668 */
  { NACLi_CMPXCHG8B,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_v),
    InstCmpxchg8b, 0x01, 3, 421, 168  },
  /* 669 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstBswap, 0x00, 1, 109, NACL_OPCODE_NULL_OFFSET  },
  /* 670 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstBswap, 0x01, 1, 109, NACL_OPCODE_NULL_OFFSET  },
  /* 671 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstBswap, 0x02, 1, 109, NACL_OPCODE_NULL_OFFSET  },
  /* 672 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstBswap, 0x03, 1, 109, NACL_OPCODE_NULL_OFFSET  },
  /* 673 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstBswap, 0x04, 1, 109, NACL_OPCODE_NULL_OFFSET  },
  /* 674 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstBswap, 0x05, 1, 109, NACL_OPCODE_NULL_OFFSET  },
  /* 675 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstBswap, 0x06, 1, 109, NACL_OPCODE_NULL_OFFSET  },
  /* 676 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstBswap, 0x07, 1, 109, NACL_OPCODE_NULL_OFFSET  },
  /* 677 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsrlw, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 678 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsrld, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 679 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsrlq, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 680 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPaddq, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 681 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPmullw, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 682 */
  { NACLi_MMX,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstPmovmskb, 0x00, 2, 418, NACL_OPCODE_NULL_OFFSET  },
  /* 683 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsubusb, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 684 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsubusw, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 685 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPminub, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 686 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPand, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 687 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPaddusb, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 688 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPaddusw, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 689 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPmaxub, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 690 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPandn, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 691 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPavgb, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 692 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsraw, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 693 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsrad, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 694 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPavgw, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 695 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPmulhuw, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 696 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPmulhw, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 697 */
  { NACLi_MMX,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMovntq, 0x00, 2, 424, NACL_OPCODE_NULL_OFFSET  },
  /* 698 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsubsb, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 699 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsubsw, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 700 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPminsw, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 701 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPor, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 702 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPaddsb, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 703 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPaddsw, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 704 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPmaxsw, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 705 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPxor, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 706 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsllw, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 707 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPslld, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 708 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsllq, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 709 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPmuludq, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 710 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPmaddwd, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 711 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsadbw, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 712 */
  { NACLi_MMX,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_v),
    InstMaskmovq, 0x00, 3, 426, NACL_OPCODE_NULL_OFFSET  },
  /* 713 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsubb, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 714 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsubw, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 715 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsubd, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 716 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsubq, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 717 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPaddb, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 718 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPaddw, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 719 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPaddd, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 720 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstMovsd, 0x00, 2, 429, NACL_OPCODE_NULL_OFFSET  },
  /* 721 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstMovsd, 0x00, 2, 431, NACL_OPCODE_NULL_OFFSET  },
  /* 722 */
  { NACLi_SSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstMovddup, 0x00, 2, 433, NACL_OPCODE_NULL_OFFSET  },
  /* 723 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 724 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstCvtsi2sd, 0x00, 2, 435, NACL_OPCODE_NULL_OFFSET  },
  /* 725 */
  { NACLi_SSE4A,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstMovntsd, 0x00, 2, 437, NACL_OPCODE_NULL_OFFSET  },
  /* 726 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstCvttsd2si, 0x00, 2, 439, NACL_OPCODE_NULL_OFFSET  },
  /* 727 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstCvtsd2si, 0x00, 2, 439, NACL_OPCODE_NULL_OFFSET  },
  /* 728 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstSqrtsd, 0x00, 2, 429, NACL_OPCODE_NULL_OFFSET  },
  /* 729 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstAddsd, 0x00, 2, 441, NACL_OPCODE_NULL_OFFSET  },
  /* 730 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstMulsd, 0x00, 2, 441, NACL_OPCODE_NULL_OFFSET  },
  /* 731 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstCvtsd2ss, 0x00, 2, 443, NACL_OPCODE_NULL_OFFSET  },
  /* 732 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstSubsd, 0x00, 2, 441, NACL_OPCODE_NULL_OFFSET  },
  /* 733 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstMinsd, 0x00, 2, 441, NACL_OPCODE_NULL_OFFSET  },
  /* 734 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstDivsd, 0x00, 2, 441, NACL_OPCODE_NULL_OFFSET  },
  /* 735 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstMaxsd, 0x00, 2, 441, NACL_OPCODE_NULL_OFFSET  },
  /* 736 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsRepne),
    InstPshuflw, 0x00, 3, 445, NACL_OPCODE_NULL_OFFSET  },
  /* 737 */
  { NACLi_SSE4A,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeHasImmed2_b) | NACL_IFLAG(OpcodeAllowsRepne),
    InstInsertq, 0x00, 4, 448, NACL_OPCODE_NULL_OFFSET  },
  /* 738 */
  { NACLi_SSE4A,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstInsertq, 0x00, 2, 452, NACL_OPCODE_NULL_OFFSET  },
  /* 739 */
  { NACLi_SSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstHaddps, 0x00, 2, 353, NACL_OPCODE_NULL_OFFSET  },
  /* 740 */
  { NACLi_SSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstHsubps, 0x00, 2, 353, NACL_OPCODE_NULL_OFFSET  },
  /* 741 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsRepne),
    InstCmpsd_xmm, 0x00, 3, 454, NACL_OPCODE_NULL_OFFSET  },
  /* 742 */
  { NACLi_SSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstAddsubps, 0x00, 2, 457, NACL_OPCODE_NULL_OFFSET  },
  /* 743 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstMovdq2q, 0x00, 2, 459, NACL_OPCODE_NULL_OFFSET  },
  /* 744 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstCvtpd2dq, 0x00, 2, 461, NACL_OPCODE_NULL_OFFSET  },
  /* 745 */
  { NACLi_SSE3,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstLddqu, 0x00, 2, 463, NACL_OPCODE_NULL_OFFSET  },
  /* 746 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMovss, 0x00, 2, 465, NACL_OPCODE_NULL_OFFSET  },
  /* 747 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMovss, 0x00, 2, 467, NACL_OPCODE_NULL_OFFSET  },
  /* 748 */
  { NACLi_SSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMovsldup, 0x00, 2, 303, NACL_OPCODE_NULL_OFFSET  },
  /* 749 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 750 */
  { NACLi_SSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMovshdup, 0x00, 2, 303, NACL_OPCODE_NULL_OFFSET  },
  /* 751 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstCvtsi2ss, 0x00, 2, 469, NACL_OPCODE_NULL_OFFSET  },
  /* 752 */
  { NACLi_SSE4A,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMovntss, 0x00, 2, 471, NACL_OPCODE_NULL_OFFSET  },
  /* 753 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstCvttss2si, 0x00, 2, 473, NACL_OPCODE_NULL_OFFSET  },
  /* 754 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstCvtss2si, 0x00, 2, 473, NACL_OPCODE_NULL_OFFSET  },
  /* 755 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstSqrtss, 0x00, 2, 303, NACL_OPCODE_NULL_OFFSET  },
  /* 756 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstRsqrtss, 0x00, 2, 465, NACL_OPCODE_NULL_OFFSET  },
  /* 757 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstRcpss, 0x00, 2, 465, NACL_OPCODE_NULL_OFFSET  },
  /* 758 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstAddss, 0x00, 2, 475, NACL_OPCODE_NULL_OFFSET  },
  /* 759 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMulss, 0x00, 2, 475, NACL_OPCODE_NULL_OFFSET  },
  /* 760 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstCvtss2sd, 0x00, 2, 477, NACL_OPCODE_NULL_OFFSET  },
  /* 761 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstCvttps2dq, 0x00, 2, 479, NACL_OPCODE_NULL_OFFSET  },
  /* 762 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstSubss, 0x00, 2, 475, NACL_OPCODE_NULL_OFFSET  },
  /* 763 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMinss, 0x00, 2, 475, NACL_OPCODE_NULL_OFFSET  },
  /* 764 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstDivss, 0x00, 2, 475, NACL_OPCODE_NULL_OFFSET  },
  /* 765 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMaxss, 0x00, 2, 475, NACL_OPCODE_NULL_OFFSET  },
  /* 766 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMovdqu, 0x00, 2, 481, NACL_OPCODE_NULL_OFFSET  },
  /* 767 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsRep),
    InstPshufhw, 0x00, 3, 445, NACL_OPCODE_NULL_OFFSET  },
  /* 768 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMovq, 0x00, 2, 445, NACL_OPCODE_NULL_OFFSET  },
  /* 769 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMovdqu, 0x00, 2, 483, NACL_OPCODE_NULL_OFFSET  },
  /* 770 */
  { NACLi_POPCNT,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstPopcnt, 0x00, 2, 61, NACL_OPCODE_NULL_OFFSET  },
  /* 771 */
  { NACLi_LZCNT,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstLzcnt, 0x00, 2, 61, NACL_OPCODE_NULL_OFFSET  },
  /* 772 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsRep),
    InstCmpss, 0x00, 3, 485, NACL_OPCODE_NULL_OFFSET  },
  /* 773 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMovq2dq, 0x00, 2, 488, NACL_OPCODE_NULL_OFFSET  },
  /* 774 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstCvtdq2pd, 0x00, 2, 490, NACL_OPCODE_NULL_OFFSET  },
  /* 775 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovupd, 0x00, 2, 492, NACL_OPCODE_NULL_OFFSET  },
  /* 776 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovupd, 0x00, 2, 494, NACL_OPCODE_NULL_OFFSET  },
  /* 777 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovlpd, 0x00, 2, 496, NACL_OPCODE_NULL_OFFSET  },
  /* 778 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovlpd, 0x00, 2, 437, NACL_OPCODE_NULL_OFFSET  },
  /* 779 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstUnpcklpd, 0x00, 2, 498, NACL_OPCODE_NULL_OFFSET  },
  /* 780 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstUnpckhpd, 0x00, 2, 498, NACL_OPCODE_NULL_OFFSET  },
  /* 781 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovhpd, 0x00, 2, 496, NACL_OPCODE_NULL_OFFSET  },
  /* 782 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovhpd, 0x00, 2, 437, NACL_OPCODE_NULL_OFFSET  },
  /* 783 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovapd, 0x00, 2, 492, NACL_OPCODE_NULL_OFFSET  },
  /* 784 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovapd, 0x00, 2, 494, NACL_OPCODE_NULL_OFFSET  },
  /* 785 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstCvtpi2pd, 0x00, 2, 500, NACL_OPCODE_NULL_OFFSET  },
  /* 786 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovntpd, 0x00, 2, 502, NACL_OPCODE_NULL_OFFSET  },
  /* 787 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstCvttpd2pi, 0x00, 2, 504, NACL_OPCODE_NULL_OFFSET  },
  /* 788 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstCvtpd2pi, 0x00, 2, 504, NACL_OPCODE_NULL_OFFSET  },
  /* 789 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstUcomisd, 0x00, 2, 506, NACL_OPCODE_NULL_OFFSET  },
  /* 790 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstComisd, 0x00, 2, 508, NACL_OPCODE_NULL_OFFSET  },
  /* 791 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovmskpd, 0x00, 2, 510, NACL_OPCODE_NULL_OFFSET  },
  /* 792 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstSqrtpd, 0x00, 2, 512, NACL_OPCODE_NULL_OFFSET  },
  /* 793 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 794 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstAndpd, 0x00, 2, 457, NACL_OPCODE_NULL_OFFSET  },
  /* 795 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstAndnpd, 0x00, 2, 457, NACL_OPCODE_NULL_OFFSET  },
  /* 796 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstOrpd, 0x00, 2, 457, NACL_OPCODE_NULL_OFFSET  },
  /* 797 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstXorpd, 0x00, 2, 457, NACL_OPCODE_NULL_OFFSET  },
  /* 798 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstAddpd, 0x00, 2, 457, NACL_OPCODE_NULL_OFFSET  },
  /* 799 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMulpd, 0x00, 2, 457, NACL_OPCODE_NULL_OFFSET  },
  /* 800 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstCvtpd2ps, 0x00, 2, 512, NACL_OPCODE_NULL_OFFSET  },
  /* 801 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstCvtps2dq, 0x00, 2, 479, NACL_OPCODE_NULL_OFFSET  },
  /* 802 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstSubpd, 0x00, 2, 457, NACL_OPCODE_NULL_OFFSET  },
  /* 803 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMinpd, 0x00, 2, 457, NACL_OPCODE_NULL_OFFSET  },
  /* 804 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstDivpd, 0x00, 2, 457, NACL_OPCODE_NULL_OFFSET  },
  /* 805 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMaxpd, 0x00, 2, 457, NACL_OPCODE_NULL_OFFSET  },
  /* 806 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPunpcklbw, 0x00, 2, 514, NACL_OPCODE_NULL_OFFSET  },
  /* 807 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPunpcklwd, 0x00, 2, 514, NACL_OPCODE_NULL_OFFSET  },
  /* 808 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPunpckldq, 0x00, 2, 514, NACL_OPCODE_NULL_OFFSET  },
  /* 809 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPacksswb, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 810 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPcmpgtb, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 811 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPcmpgtw, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 812 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPcmpgtd, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 813 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPackuswb, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 814 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPunpckhbw, 0x00, 2, 514, NACL_OPCODE_NULL_OFFSET  },
  /* 815 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPunpckhwd, 0x00, 2, 514, NACL_OPCODE_NULL_OFFSET  },
  /* 816 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPunpckhdq, 0x00, 2, 514, NACL_OPCODE_NULL_OFFSET  },
  /* 817 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPackssdw, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 818 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPunpcklqdq, 0x00, 2, 514, NACL_OPCODE_NULL_OFFSET  },
  /* 819 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPunpckhqdq, 0x00, 2, 514, NACL_OPCODE_NULL_OFFSET  },
  /* 820 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstMovd, 0x00, 2, 518, NACL_OPCODE_NULL_OFFSET  },
  /* 821 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovdqa, 0x00, 2, 481, NACL_OPCODE_NULL_OFFSET  },
  /* 822 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPshufd, 0x00, 3, 520, NACL_OPCODE_NULL_OFFSET  },
  /* 823 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x07, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 824 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsllw, 0x06, 2, 523, 823  },
  /* 825 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x05, 0, 0, 824  },
  /* 826 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsraw, 0x04, 2, 523, 825  },
  /* 827 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x03, 0, 0, 826  },
  /* 828 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsrlw, 0x02, 2, 523, 827  },
  /* 829 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x01, 0, 0, 828  },
  /* 830 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00, 0, 0, 829  },
  /* 831 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPslld, 0x06, 2, 523, 823  },
  /* 832 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x05, 0, 0, 831  },
  /* 833 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsrad, 0x04, 2, 523, 832  },
  /* 834 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x03, 0, 0, 833  },
  /* 835 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsrld, 0x02, 2, 523, 834  },
  /* 836 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x01, 0, 0, 835  },
  /* 837 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00, 0, 0, 836  },
  /* 838 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPslldq, 0x07, 2, 523, NACL_OPCODE_NULL_OFFSET  },
  /* 839 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsllq, 0x06, 2, 523, 838  },
  /* 840 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x05, 0, 0, 839  },
  /* 841 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x04, 0, 0, 840  },
  /* 842 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsrldq, 0x03, 2, 523, 841  },
  /* 843 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsrlq, 0x02, 2, 523, 842  },
  /* 844 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x01, 0, 0, 843  },
  /* 845 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00, 0, 0, 844  },
  /* 846 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPcmpeqb, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 847 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPcmpeqw, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 848 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPcmpeqd, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 849 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 850 */
  { NACLi_SSE4A,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeHasImmed2_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstExtrq, 0x00, 3, 525, 849  },
  /* 851 */
  { NACLi_SSE4A,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstExtrq, 0x00, 2, 452, NACL_OPCODE_NULL_OFFSET  },
  /* 852 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstHaddpd, 0x00, 2, 457, NACL_OPCODE_NULL_OFFSET  },
  /* 853 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstHsubpd, 0x00, 2, 457, NACL_OPCODE_NULL_OFFSET  },
  /* 854 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstMovd, 0x00, 2, 528, NACL_OPCODE_NULL_OFFSET  },
  /* 855 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovdqa, 0x00, 2, 483, NACL_OPCODE_NULL_OFFSET  },
  /* 856 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstCmppd, 0x00, 3, 530, NACL_OPCODE_NULL_OFFSET  },
  /* 857 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstPinsrw, 0x00, 3, 533, NACL_OPCODE_NULL_OFFSET  },
  /* 858 */
  { NACLi_SSE41,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPextrw, 0x00, 3, 536, NACL_OPCODE_NULL_OFFSET  },
  /* 859 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstShufpd, 0x00, 3, 530, NACL_OPCODE_NULL_OFFSET  },
  /* 860 */
  { NACLi_SSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstAddsubpd, 0x00, 2, 457, NACL_OPCODE_NULL_OFFSET  },
  /* 861 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsrlw, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 862 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsrld, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 863 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsrlq, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 864 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPaddq, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 865 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmullw, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 866 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovq, 0x00, 2, 539, NACL_OPCODE_NULL_OFFSET  },
  /* 867 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovmskb, 0x00, 2, 536, NACL_OPCODE_NULL_OFFSET  },
  /* 868 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsubusb, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 869 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsubusw, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 870 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPminub, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 871 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPand, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 872 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPaddusb, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 873 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPaddusw, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 874 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmaxub, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 875 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPandn, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 876 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPavgb, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 877 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsraw, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 878 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsrad, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 879 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPavgw, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 880 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmulhuw, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 881 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmulhw, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 882 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstCvttpd2dq, 0x00, 2, 541, NACL_OPCODE_NULL_OFFSET  },
  /* 883 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovntdq, 0x00, 2, 543, NACL_OPCODE_NULL_OFFSET  },
  /* 884 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsubsb, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 885 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsubsw, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 886 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPminsw, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 887 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPor, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 888 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPaddsb, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 889 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPaddsw, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 890 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmaxsw, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 891 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPxor, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 892 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsllw, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 893 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPslld, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 894 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsllq, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 895 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmuludq, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 896 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmaddwd, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 897 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsadbw, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 898 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstMaskmovdqu, 0x00, 3, 545, NACL_OPCODE_NULL_OFFSET  },
  /* 899 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsubb, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 900 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsubw, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 901 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsubd, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 902 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsubq, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 903 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPaddb, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 904 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPaddw, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 905 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPaddd, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 906 */
  { NACLi_E3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPi2fw, 0x00, 2, 365, NACL_OPCODE_NULL_OFFSET  },
  /* 907 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPi2fd, 0x00, 2, 365, NACL_OPCODE_NULL_OFFSET  },
  /* 908 */
  { NACLi_E3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPf2iw, 0x00, 2, 365, NACL_OPCODE_NULL_OFFSET  },
  /* 909 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPf2id, 0x00, 2, 365, NACL_OPCODE_NULL_OFFSET  },
  /* 910 */
  { NACLi_E3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfnacc, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 911 */
  { NACLi_E3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfpnacc, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 912 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfcmpge, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 913 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfmin, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 914 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfrcp, 0x00, 2, 365, NACL_OPCODE_NULL_OFFSET  },
  /* 915 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfrsqrt, 0x00, 2, 365, NACL_OPCODE_NULL_OFFSET  },
  /* 916 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfsub, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 917 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfadd, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 918 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfcmpgt, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 919 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfmax, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 920 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfrcpit1, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 921 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfrsqit1, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 922 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfsubr, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 923 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfacc, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 924 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfcmpeq, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 925 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfmul, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 926 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfrcpit2, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 927 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPmulhrw, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 928 */
  { NACLi_E3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPswapd, 0x00, 2, 365, NACL_OPCODE_NULL_OFFSET  },
  /* 929 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPavgusb, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 930 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPshufb, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 931 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPhaddw, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 932 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPhaddd, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 933 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPhaddsw, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 934 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPmaddubsw, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 935 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPhsubw, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 936 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPhsubd, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 937 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPhsubsw, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 938 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsignb, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 939 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsignw, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 940 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsignd, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 941 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPmulhrsw, 0x00, 2, 359, NACL_OPCODE_NULL_OFFSET  },
  /* 942 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPabsb, 0x00, 2, 365, NACL_OPCODE_NULL_OFFSET  },
  /* 943 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPabsw, 0x00, 2, 365, NACL_OPCODE_NULL_OFFSET  },
  /* 944 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPabsd, 0x00, 2, 365, NACL_OPCODE_NULL_OFFSET  },
  /* 945 */
  { NACLi_MOVBE,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMovbe, 0x00, 2, 548, NACL_OPCODE_NULL_OFFSET  },
  /* 946 */
  { NACLi_MOVBE,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMovbe, 0x00, 2, 550, NACL_OPCODE_NULL_OFFSET  },
  /* 947 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPshufb, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 948 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPhaddw, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 949 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPhaddd, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 950 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPhaddsw, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 951 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmaddubsw, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 952 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPhsubw, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 953 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPhsubd, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 954 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPhsubsw, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 955 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsignb, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 956 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsignw, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 957 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsignd, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 958 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmulhrsw, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 959 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPblendvb, 0x00, 3, 552, NACL_OPCODE_NULL_OFFSET  },
  /* 960 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstBlendvps, 0x00, 3, 552, NACL_OPCODE_NULL_OFFSET  },
  /* 961 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstBlendvpd, 0x00, 3, 552, NACL_OPCODE_NULL_OFFSET  },
  /* 962 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPtest, 0x00, 2, 555, NACL_OPCODE_NULL_OFFSET  },
  /* 963 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPabsb, 0x00, 2, 481, NACL_OPCODE_NULL_OFFSET  },
  /* 964 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPabsw, 0x00, 2, 481, NACL_OPCODE_NULL_OFFSET  },
  /* 965 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPabsd, 0x00, 2, 481, NACL_OPCODE_NULL_OFFSET  },
  /* 966 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovsxbw, 0x00, 2, 557, NACL_OPCODE_NULL_OFFSET  },
  /* 967 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovsxbd, 0x00, 2, 559, NACL_OPCODE_NULL_OFFSET  },
  /* 968 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovsxbq, 0x00, 2, 561, NACL_OPCODE_NULL_OFFSET  },
  /* 969 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovsxwd, 0x00, 2, 557, NACL_OPCODE_NULL_OFFSET  },
  /* 970 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovsxwq, 0x00, 2, 559, NACL_OPCODE_NULL_OFFSET  },
  /* 971 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovsxdq, 0x00, 2, 557, NACL_OPCODE_NULL_OFFSET  },
  /* 972 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmuldq, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 973 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPcmpeqq, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 974 */
  { NACLi_SSE41,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovntdqa, 0x00, 2, 463, NACL_OPCODE_NULL_OFFSET  },
  /* 975 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPackusdw, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 976 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovzxbw, 0x00, 2, 557, NACL_OPCODE_NULL_OFFSET  },
  /* 977 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovzxbd, 0x00, 2, 559, NACL_OPCODE_NULL_OFFSET  },
  /* 978 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovzxbq, 0x00, 2, 561, NACL_OPCODE_NULL_OFFSET  },
  /* 979 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovzxwd, 0x00, 2, 557, NACL_OPCODE_NULL_OFFSET  },
  /* 980 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovzxwq, 0x00, 2, 559, NACL_OPCODE_NULL_OFFSET  },
  /* 981 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovzxdq, 0x00, 2, 557, NACL_OPCODE_NULL_OFFSET  },
  /* 982 */
  { NACLi_SSE42,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPcmpgtq, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 983 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPminsb, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 984 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPminsd, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 985 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPminuw, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 986 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPminud, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 987 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmaxsb, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 988 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmaxsd, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 989 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmaxuw, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 990 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmaxud, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 991 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmulld, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 992 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPhminposuw, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 993 */
  { NACLi_VMX,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvept, 0x00, 2, 563, NACL_OPCODE_NULL_OFFSET  },
  /* 994 */
  { NACLi_VMX,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvvpid, 0x00, 2, 563, NACL_OPCODE_NULL_OFFSET  },
  /* 995 */
  { NACLi_SSE42,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(OperandSize_b),
    InstCrc32, 0x00, 2, 565, NACL_OPCODE_NULL_OFFSET  },
  /* 996 */
  { NACLi_SSE42,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstCrc32, 0x00, 2, 567, NACL_OPCODE_NULL_OFFSET  },
  /* 997 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstPalignr, 0x00, 3, 569, NACL_OPCODE_NULL_OFFSET  },
  /* 998 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstRoundps, 0x00, 3, 520, NACL_OPCODE_NULL_OFFSET  },
  /* 999 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstRoundpd, 0x00, 3, 520, NACL_OPCODE_NULL_OFFSET  },
  /* 1000 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstRoundss, 0x00, 3, 572, NACL_OPCODE_NULL_OFFSET  },
  /* 1001 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstRoundsd, 0x00, 3, 575, NACL_OPCODE_NULL_OFFSET  },
  /* 1002 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstBlendps, 0x00, 3, 578, NACL_OPCODE_NULL_OFFSET  },
  /* 1003 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstBlendpd, 0x00, 3, 578, NACL_OPCODE_NULL_OFFSET  },
  /* 1004 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPblendw, 0x00, 3, 578, NACL_OPCODE_NULL_OFFSET  },
  /* 1005 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPalignr, 0x00, 3, 578, NACL_OPCODE_NULL_OFFSET  },
  /* 1006 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPextrb, 0x00, 3, 581, NACL_OPCODE_NULL_OFFSET  },
  /* 1007 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPextrw, 0x00, 3, 584, NACL_OPCODE_NULL_OFFSET  },
  /* 1008 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstPextrd, 0x00, 3, 587, NACL_OPCODE_NULL_OFFSET  },
  /* 1009 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstExtractps, 0x00, 3, 590, NACL_OPCODE_NULL_OFFSET  },
  /* 1010 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstPinsrb, 0x00, 3, 593, NACL_OPCODE_NULL_OFFSET  },
  /* 1011 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstInsertps, 0x00, 3, 596, NACL_OPCODE_NULL_OFFSET  },
  /* 1012 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstPinsrd, 0x00, 3, 599, NACL_OPCODE_NULL_OFFSET  },
  /* 1013 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstDpps, 0x00, 3, 578, NACL_OPCODE_NULL_OFFSET  },
  /* 1014 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstDppd, 0x00, 3, 578, NACL_OPCODE_NULL_OFFSET  },
  /* 1015 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMpsadbw, 0x00, 3, 578, NACL_OPCODE_NULL_OFFSET  },
  /* 1016 */
  { NACLi_SSE42,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstPcmpestrm, 0x00, 6, 602, NACL_OPCODE_NULL_OFFSET  },
  /* 1017 */
  { NACLi_SSE42,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstPcmpestri, 0x00, 6, 608, NACL_OPCODE_NULL_OFFSET  },
  /* 1018 */
  { NACLi_SSE42,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPcmpistrm, 0x00, 4, 614, NACL_OPCODE_NULL_OFFSET  },
  /* 1019 */
  { NACLi_SSE42,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstPcmpistri, 0x00, 4, 618, NACL_OPCODE_NULL_OFFSET  },
  /* 1020 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00, 2, 622, NACL_OPCODE_NULL_OFFSET  },
  /* 1021 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00, 2, 624, NACL_OPCODE_NULL_OFFSET  },
  /* 1022 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00, 2, 626, NACL_OPCODE_NULL_OFFSET  },
  /* 1023 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00, 2, 628, NACL_OPCODE_NULL_OFFSET  },
  /* 1024 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00, 2, 630, NACL_OPCODE_NULL_OFFSET  },
  /* 1025 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00, 2, 632, NACL_OPCODE_NULL_OFFSET  },
  /* 1026 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00, 2, 634, NACL_OPCODE_NULL_OFFSET  },
  /* 1027 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00, 2, 636, NACL_OPCODE_NULL_OFFSET  },
  /* 1028 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00, 2, 622, NACL_OPCODE_NULL_OFFSET  },
  /* 1029 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00, 2, 624, NACL_OPCODE_NULL_OFFSET  },
  /* 1030 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00, 2, 626, NACL_OPCODE_NULL_OFFSET  },
  /* 1031 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00, 2, 628, NACL_OPCODE_NULL_OFFSET  },
  /* 1032 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00, 2, 630, NACL_OPCODE_NULL_OFFSET  },
  /* 1033 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00, 2, 632, NACL_OPCODE_NULL_OFFSET  },
  /* 1034 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00, 2, 634, NACL_OPCODE_NULL_OFFSET  },
  /* 1035 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00, 2, 636, NACL_OPCODE_NULL_OFFSET  },
  /* 1036 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcom, 0x00, 2, 638, NACL_OPCODE_NULL_OFFSET  },
  /* 1037 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcom, 0x00, 2, 640, NACL_OPCODE_NULL_OFFSET  },
  /* 1038 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcom, 0x00, 2, 642, NACL_OPCODE_NULL_OFFSET  },
  /* 1039 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcom, 0x00, 2, 644, NACL_OPCODE_NULL_OFFSET  },
  /* 1040 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcom, 0x00, 2, 646, NACL_OPCODE_NULL_OFFSET  },
  /* 1041 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcom, 0x00, 2, 648, NACL_OPCODE_NULL_OFFSET  },
  /* 1042 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcom, 0x00, 2, 650, NACL_OPCODE_NULL_OFFSET  },
  /* 1043 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcom, 0x00, 2, 652, NACL_OPCODE_NULL_OFFSET  },
  /* 1044 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomp, 0x00, 2, 638, NACL_OPCODE_NULL_OFFSET  },
  /* 1045 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomp, 0x00, 2, 640, NACL_OPCODE_NULL_OFFSET  },
  /* 1046 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomp, 0x00, 2, 642, NACL_OPCODE_NULL_OFFSET  },
  /* 1047 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomp, 0x00, 2, 644, NACL_OPCODE_NULL_OFFSET  },
  /* 1048 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomp, 0x00, 2, 646, NACL_OPCODE_NULL_OFFSET  },
  /* 1049 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomp, 0x00, 2, 648, NACL_OPCODE_NULL_OFFSET  },
  /* 1050 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomp, 0x00, 2, 650, NACL_OPCODE_NULL_OFFSET  },
  /* 1051 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomp, 0x00, 2, 652, NACL_OPCODE_NULL_OFFSET  },
  /* 1052 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00, 2, 622, NACL_OPCODE_NULL_OFFSET  },
  /* 1053 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00, 2, 624, NACL_OPCODE_NULL_OFFSET  },
  /* 1054 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00, 2, 626, NACL_OPCODE_NULL_OFFSET  },
  /* 1055 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00, 2, 628, NACL_OPCODE_NULL_OFFSET  },
  /* 1056 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00, 2, 630, NACL_OPCODE_NULL_OFFSET  },
  /* 1057 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00, 2, 632, NACL_OPCODE_NULL_OFFSET  },
  /* 1058 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00, 2, 634, NACL_OPCODE_NULL_OFFSET  },
  /* 1059 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00, 2, 636, NACL_OPCODE_NULL_OFFSET  },
  /* 1060 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00, 2, 622, NACL_OPCODE_NULL_OFFSET  },
  /* 1061 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00, 2, 624, NACL_OPCODE_NULL_OFFSET  },
  /* 1062 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00, 2, 626, NACL_OPCODE_NULL_OFFSET  },
  /* 1063 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00, 2, 628, NACL_OPCODE_NULL_OFFSET  },
  /* 1064 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00, 2, 630, NACL_OPCODE_NULL_OFFSET  },
  /* 1065 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00, 2, 632, NACL_OPCODE_NULL_OFFSET  },
  /* 1066 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00, 2, 634, NACL_OPCODE_NULL_OFFSET  },
  /* 1067 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00, 2, 636, NACL_OPCODE_NULL_OFFSET  },
  /* 1068 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00, 2, 622, NACL_OPCODE_NULL_OFFSET  },
  /* 1069 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00, 2, 624, NACL_OPCODE_NULL_OFFSET  },
  /* 1070 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00, 2, 626, NACL_OPCODE_NULL_OFFSET  },
  /* 1071 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00, 2, 628, NACL_OPCODE_NULL_OFFSET  },
  /* 1072 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00, 2, 630, NACL_OPCODE_NULL_OFFSET  },
  /* 1073 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00, 2, 632, NACL_OPCODE_NULL_OFFSET  },
  /* 1074 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00, 2, 634, NACL_OPCODE_NULL_OFFSET  },
  /* 1075 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00, 2, 636, NACL_OPCODE_NULL_OFFSET  },
  /* 1076 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00, 2, 622, NACL_OPCODE_NULL_OFFSET  },
  /* 1077 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00, 2, 624, NACL_OPCODE_NULL_OFFSET  },
  /* 1078 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00, 2, 626, NACL_OPCODE_NULL_OFFSET  },
  /* 1079 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00, 2, 628, NACL_OPCODE_NULL_OFFSET  },
  /* 1080 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00, 2, 630, NACL_OPCODE_NULL_OFFSET  },
  /* 1081 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00, 2, 632, NACL_OPCODE_NULL_OFFSET  },
  /* 1082 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00, 2, 634, NACL_OPCODE_NULL_OFFSET  },
  /* 1083 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00, 2, 636, NACL_OPCODE_NULL_OFFSET  },
  /* 1084 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFld, 0x00, 2, 654, NACL_OPCODE_NULL_OFFSET  },
  /* 1085 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFld, 0x00, 2, 656, NACL_OPCODE_NULL_OFFSET  },
  /* 1086 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFld, 0x00, 2, 658, NACL_OPCODE_NULL_OFFSET  },
  /* 1087 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFld, 0x00, 2, 660, NACL_OPCODE_NULL_OFFSET  },
  /* 1088 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFld, 0x00, 2, 662, NACL_OPCODE_NULL_OFFSET  },
  /* 1089 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFld, 0x00, 2, 664, NACL_OPCODE_NULL_OFFSET  },
  /* 1090 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFld, 0x00, 2, 666, NACL_OPCODE_NULL_OFFSET  },
  /* 1091 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFld, 0x00, 2, 668, NACL_OPCODE_NULL_OFFSET  },
  /* 1092 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFxch, 0x00, 2, 670, NACL_OPCODE_NULL_OFFSET  },
  /* 1093 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFxch, 0x00, 2, 672, NACL_OPCODE_NULL_OFFSET  },
  /* 1094 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFxch, 0x00, 2, 674, NACL_OPCODE_NULL_OFFSET  },
  /* 1095 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFxch, 0x00, 2, 676, NACL_OPCODE_NULL_OFFSET  },
  /* 1096 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFxch, 0x00, 2, 678, NACL_OPCODE_NULL_OFFSET  },
  /* 1097 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFxch, 0x00, 2, 680, NACL_OPCODE_NULL_OFFSET  },
  /* 1098 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFxch, 0x00, 2, 682, NACL_OPCODE_NULL_OFFSET  },
  /* 1099 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFxch, 0x00, 2, 684, NACL_OPCODE_NULL_OFFSET  },
  /* 1100 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFnop, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 1101 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFchs, 0x00, 1, 205, NACL_OPCODE_NULL_OFFSET  },
  /* 1102 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFabs, 0x00, 1, 205, NACL_OPCODE_NULL_OFFSET  },
  /* 1103 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFtst, 0x00, 1, 207, NACL_OPCODE_NULL_OFFSET  },
  /* 1104 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFxam, 0x00, 1, 207, NACL_OPCODE_NULL_OFFSET  },
  /* 1105 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFld1, 0x00, 1, 205, NACL_OPCODE_NULL_OFFSET  },
  /* 1106 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFldl2t, 0x00, 1, 205, NACL_OPCODE_NULL_OFFSET  },
  /* 1107 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFldl2e, 0x00, 1, 205, NACL_OPCODE_NULL_OFFSET  },
  /* 1108 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFldpi, 0x00, 1, 205, NACL_OPCODE_NULL_OFFSET  },
  /* 1109 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFldlg2, 0x00, 1, 205, NACL_OPCODE_NULL_OFFSET  },
  /* 1110 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFldln2, 0x00, 1, 205, NACL_OPCODE_NULL_OFFSET  },
  /* 1111 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFldz, 0x00, 1, 205, NACL_OPCODE_NULL_OFFSET  },
  /* 1112 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstF2xm1, 0x00, 1, 205, NACL_OPCODE_NULL_OFFSET  },
  /* 1113 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFyl2x, 0x00, 2, 624, NACL_OPCODE_NULL_OFFSET  },
  /* 1114 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFptan, 0x00, 2, 656, NACL_OPCODE_NULL_OFFSET  },
  /* 1115 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFpatan, 0x00, 2, 624, NACL_OPCODE_NULL_OFFSET  },
  /* 1116 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFxtract, 0x00, 2, 656, NACL_OPCODE_NULL_OFFSET  },
  /* 1117 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFprem1, 0x00, 2, 624, NACL_OPCODE_NULL_OFFSET  },
  /* 1118 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdecstp, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 1119 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFincstp, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 1120 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFprem, 0x00, 2, 624, NACL_OPCODE_NULL_OFFSET  },
  /* 1121 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFyl2xp1, 0x00, 2, 624, NACL_OPCODE_NULL_OFFSET  },
  /* 1122 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsqrt, 0x00, 1, 205, NACL_OPCODE_NULL_OFFSET  },
  /* 1123 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsincos, 0x00, 2, 656, NACL_OPCODE_NULL_OFFSET  },
  /* 1124 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFrndint, 0x00, 1, 205, NACL_OPCODE_NULL_OFFSET  },
  /* 1125 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFscale, 0x00, 2, 624, NACL_OPCODE_NULL_OFFSET  },
  /* 1126 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsin, 0x00, 1, 205, NACL_OPCODE_NULL_OFFSET  },
  /* 1127 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcos, 0x00, 1, 205, NACL_OPCODE_NULL_OFFSET  },
  /* 1128 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovb, 0x00, 2, 622, NACL_OPCODE_NULL_OFFSET  },
  /* 1129 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovb, 0x00, 2, 624, NACL_OPCODE_NULL_OFFSET  },
  /* 1130 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovb, 0x00, 2, 626, NACL_OPCODE_NULL_OFFSET  },
  /* 1131 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovb, 0x00, 2, 628, NACL_OPCODE_NULL_OFFSET  },
  /* 1132 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovb, 0x00, 2, 630, NACL_OPCODE_NULL_OFFSET  },
  /* 1133 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovb, 0x00, 2, 632, NACL_OPCODE_NULL_OFFSET  },
  /* 1134 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovb, 0x00, 2, 634, NACL_OPCODE_NULL_OFFSET  },
  /* 1135 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovb, 0x00, 2, 636, NACL_OPCODE_NULL_OFFSET  },
  /* 1136 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmove, 0x00, 2, 622, NACL_OPCODE_NULL_OFFSET  },
  /* 1137 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmove, 0x00, 2, 624, NACL_OPCODE_NULL_OFFSET  },
  /* 1138 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmove, 0x00, 2, 626, NACL_OPCODE_NULL_OFFSET  },
  /* 1139 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmove, 0x00, 2, 628, NACL_OPCODE_NULL_OFFSET  },
  /* 1140 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmove, 0x00, 2, 630, NACL_OPCODE_NULL_OFFSET  },
  /* 1141 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmove, 0x00, 2, 632, NACL_OPCODE_NULL_OFFSET  },
  /* 1142 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmove, 0x00, 2, 634, NACL_OPCODE_NULL_OFFSET  },
  /* 1143 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmove, 0x00, 2, 636, NACL_OPCODE_NULL_OFFSET  },
  /* 1144 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovbe, 0x00, 2, 622, NACL_OPCODE_NULL_OFFSET  },
  /* 1145 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovbe, 0x00, 2, 624, NACL_OPCODE_NULL_OFFSET  },
  /* 1146 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovbe, 0x00, 2, 626, NACL_OPCODE_NULL_OFFSET  },
  /* 1147 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovbe, 0x00, 2, 628, NACL_OPCODE_NULL_OFFSET  },
  /* 1148 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovbe, 0x00, 2, 630, NACL_OPCODE_NULL_OFFSET  },
  /* 1149 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovbe, 0x00, 2, 632, NACL_OPCODE_NULL_OFFSET  },
  /* 1150 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovbe, 0x00, 2, 634, NACL_OPCODE_NULL_OFFSET  },
  /* 1151 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovbe, 0x00, 2, 636, NACL_OPCODE_NULL_OFFSET  },
  /* 1152 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovu, 0x00, 2, 622, NACL_OPCODE_NULL_OFFSET  },
  /* 1153 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovu, 0x00, 2, 624, NACL_OPCODE_NULL_OFFSET  },
  /* 1154 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovu, 0x00, 2, 626, NACL_OPCODE_NULL_OFFSET  },
  /* 1155 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovu, 0x00, 2, 628, NACL_OPCODE_NULL_OFFSET  },
  /* 1156 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovu, 0x00, 2, 630, NACL_OPCODE_NULL_OFFSET  },
  /* 1157 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovu, 0x00, 2, 632, NACL_OPCODE_NULL_OFFSET  },
  /* 1158 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovu, 0x00, 2, 634, NACL_OPCODE_NULL_OFFSET  },
  /* 1159 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovu, 0x00, 2, 636, NACL_OPCODE_NULL_OFFSET  },
  /* 1160 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucompp, 0x00, 2, 640, NACL_OPCODE_NULL_OFFSET  },
  /* 1161 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnb, 0x00, 2, 622, NACL_OPCODE_NULL_OFFSET  },
  /* 1162 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnb, 0x00, 2, 624, NACL_OPCODE_NULL_OFFSET  },
  /* 1163 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnb, 0x00, 2, 626, NACL_OPCODE_NULL_OFFSET  },
  /* 1164 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnb, 0x00, 2, 628, NACL_OPCODE_NULL_OFFSET  },
  /* 1165 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnb, 0x00, 2, 630, NACL_OPCODE_NULL_OFFSET  },
  /* 1166 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnb, 0x00, 2, 632, NACL_OPCODE_NULL_OFFSET  },
  /* 1167 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnb, 0x00, 2, 634, NACL_OPCODE_NULL_OFFSET  },
  /* 1168 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnb, 0x00, 2, 636, NACL_OPCODE_NULL_OFFSET  },
  /* 1169 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovne, 0x00, 2, 622, NACL_OPCODE_NULL_OFFSET  },
  /* 1170 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovne, 0x00, 2, 624, NACL_OPCODE_NULL_OFFSET  },
  /* 1171 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovne, 0x00, 2, 626, NACL_OPCODE_NULL_OFFSET  },
  /* 1172 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovne, 0x00, 2, 628, NACL_OPCODE_NULL_OFFSET  },
  /* 1173 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovne, 0x00, 2, 630, NACL_OPCODE_NULL_OFFSET  },
  /* 1174 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovne, 0x00, 2, 632, NACL_OPCODE_NULL_OFFSET  },
  /* 1175 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovne, 0x00, 2, 634, NACL_OPCODE_NULL_OFFSET  },
  /* 1176 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovne, 0x00, 2, 636, NACL_OPCODE_NULL_OFFSET  },
  /* 1177 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnbe, 0x00, 2, 622, NACL_OPCODE_NULL_OFFSET  },
  /* 1178 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnbe, 0x00, 2, 624, NACL_OPCODE_NULL_OFFSET  },
  /* 1179 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnbe, 0x00, 2, 626, NACL_OPCODE_NULL_OFFSET  },
  /* 1180 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnbe, 0x00, 2, 628, NACL_OPCODE_NULL_OFFSET  },
  /* 1181 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnbe, 0x00, 2, 630, NACL_OPCODE_NULL_OFFSET  },
  /* 1182 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnbe, 0x00, 2, 632, NACL_OPCODE_NULL_OFFSET  },
  /* 1183 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnbe, 0x00, 2, 634, NACL_OPCODE_NULL_OFFSET  },
  /* 1184 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnbe, 0x00, 2, 636, NACL_OPCODE_NULL_OFFSET  },
  /* 1185 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnu, 0x00, 2, 622, NACL_OPCODE_NULL_OFFSET  },
  /* 1186 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnu, 0x00, 2, 624, NACL_OPCODE_NULL_OFFSET  },
  /* 1187 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnu, 0x00, 2, 626, NACL_OPCODE_NULL_OFFSET  },
  /* 1188 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnu, 0x00, 2, 628, NACL_OPCODE_NULL_OFFSET  },
  /* 1189 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnu, 0x00, 2, 630, NACL_OPCODE_NULL_OFFSET  },
  /* 1190 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnu, 0x00, 2, 632, NACL_OPCODE_NULL_OFFSET  },
  /* 1191 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnu, 0x00, 2, 634, NACL_OPCODE_NULL_OFFSET  },
  /* 1192 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnu, 0x00, 2, 636, NACL_OPCODE_NULL_OFFSET  },
  /* 1193 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFnclex, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 1194 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFninit, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 1195 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomi, 0x00, 2, 638, NACL_OPCODE_NULL_OFFSET  },
  /* 1196 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomi, 0x00, 2, 640, NACL_OPCODE_NULL_OFFSET  },
  /* 1197 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomi, 0x00, 2, 642, NACL_OPCODE_NULL_OFFSET  },
  /* 1198 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomi, 0x00, 2, 644, NACL_OPCODE_NULL_OFFSET  },
  /* 1199 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomi, 0x00, 2, 646, NACL_OPCODE_NULL_OFFSET  },
  /* 1200 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomi, 0x00, 2, 648, NACL_OPCODE_NULL_OFFSET  },
  /* 1201 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomi, 0x00, 2, 650, NACL_OPCODE_NULL_OFFSET  },
  /* 1202 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomi, 0x00, 2, 652, NACL_OPCODE_NULL_OFFSET  },
  /* 1203 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomi, 0x00, 2, 638, NACL_OPCODE_NULL_OFFSET  },
  /* 1204 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomi, 0x00, 2, 640, NACL_OPCODE_NULL_OFFSET  },
  /* 1205 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomi, 0x00, 2, 642, NACL_OPCODE_NULL_OFFSET  },
  /* 1206 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomi, 0x00, 2, 644, NACL_OPCODE_NULL_OFFSET  },
  /* 1207 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomi, 0x00, 2, 646, NACL_OPCODE_NULL_OFFSET  },
  /* 1208 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomi, 0x00, 2, 648, NACL_OPCODE_NULL_OFFSET  },
  /* 1209 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomi, 0x00, 2, 650, NACL_OPCODE_NULL_OFFSET  },
  /* 1210 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomi, 0x00, 2, 652, NACL_OPCODE_NULL_OFFSET  },
  /* 1211 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00, 2, 686, NACL_OPCODE_NULL_OFFSET  },
  /* 1212 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00, 2, 688, NACL_OPCODE_NULL_OFFSET  },
  /* 1213 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00, 2, 690, NACL_OPCODE_NULL_OFFSET  },
  /* 1214 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00, 2, 692, NACL_OPCODE_NULL_OFFSET  },
  /* 1215 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00, 2, 694, NACL_OPCODE_NULL_OFFSET  },
  /* 1216 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00, 2, 696, NACL_OPCODE_NULL_OFFSET  },
  /* 1217 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00, 2, 698, NACL_OPCODE_NULL_OFFSET  },
  /* 1218 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00, 2, 686, NACL_OPCODE_NULL_OFFSET  },
  /* 1219 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00, 2, 688, NACL_OPCODE_NULL_OFFSET  },
  /* 1220 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00, 2, 690, NACL_OPCODE_NULL_OFFSET  },
  /* 1221 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00, 2, 692, NACL_OPCODE_NULL_OFFSET  },
  /* 1222 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00, 2, 694, NACL_OPCODE_NULL_OFFSET  },
  /* 1223 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00, 2, 696, NACL_OPCODE_NULL_OFFSET  },
  /* 1224 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00, 2, 698, NACL_OPCODE_NULL_OFFSET  },
  /* 1225 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00, 2, 686, NACL_OPCODE_NULL_OFFSET  },
  /* 1226 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00, 2, 688, NACL_OPCODE_NULL_OFFSET  },
  /* 1227 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00, 2, 690, NACL_OPCODE_NULL_OFFSET  },
  /* 1228 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00, 2, 692, NACL_OPCODE_NULL_OFFSET  },
  /* 1229 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00, 2, 694, NACL_OPCODE_NULL_OFFSET  },
  /* 1230 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00, 2, 696, NACL_OPCODE_NULL_OFFSET  },
  /* 1231 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00, 2, 698, NACL_OPCODE_NULL_OFFSET  },
  /* 1232 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00, 2, 686, NACL_OPCODE_NULL_OFFSET  },
  /* 1233 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00, 2, 688, NACL_OPCODE_NULL_OFFSET  },
  /* 1234 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00, 2, 690, NACL_OPCODE_NULL_OFFSET  },
  /* 1235 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00, 2, 692, NACL_OPCODE_NULL_OFFSET  },
  /* 1236 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00, 2, 694, NACL_OPCODE_NULL_OFFSET  },
  /* 1237 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00, 2, 696, NACL_OPCODE_NULL_OFFSET  },
  /* 1238 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00, 2, 698, NACL_OPCODE_NULL_OFFSET  },
  /* 1239 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00, 2, 686, NACL_OPCODE_NULL_OFFSET  },
  /* 1240 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00, 2, 688, NACL_OPCODE_NULL_OFFSET  },
  /* 1241 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00, 2, 690, NACL_OPCODE_NULL_OFFSET  },
  /* 1242 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00, 2, 692, NACL_OPCODE_NULL_OFFSET  },
  /* 1243 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00, 2, 694, NACL_OPCODE_NULL_OFFSET  },
  /* 1244 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00, 2, 696, NACL_OPCODE_NULL_OFFSET  },
  /* 1245 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00, 2, 698, NACL_OPCODE_NULL_OFFSET  },
  /* 1246 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00, 2, 686, NACL_OPCODE_NULL_OFFSET  },
  /* 1247 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00, 2, 688, NACL_OPCODE_NULL_OFFSET  },
  /* 1248 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00, 2, 690, NACL_OPCODE_NULL_OFFSET  },
  /* 1249 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00, 2, 692, NACL_OPCODE_NULL_OFFSET  },
  /* 1250 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00, 2, 694, NACL_OPCODE_NULL_OFFSET  },
  /* 1251 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00, 2, 696, NACL_OPCODE_NULL_OFFSET  },
  /* 1252 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00, 2, 698, NACL_OPCODE_NULL_OFFSET  },
  /* 1253 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFfree, 0x00, 1, 700, NACL_OPCODE_NULL_OFFSET  },
  /* 1254 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFfree, 0x00, 1, 701, NACL_OPCODE_NULL_OFFSET  },
  /* 1255 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFfree, 0x00, 1, 702, NACL_OPCODE_NULL_OFFSET  },
  /* 1256 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFfree, 0x00, 1, 703, NACL_OPCODE_NULL_OFFSET  },
  /* 1257 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFfree, 0x00, 1, 704, NACL_OPCODE_NULL_OFFSET  },
  /* 1258 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFfree, 0x00, 1, 705, NACL_OPCODE_NULL_OFFSET  },
  /* 1259 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFfree, 0x00, 1, 706, NACL_OPCODE_NULL_OFFSET  },
  /* 1260 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFfree, 0x00, 1, 707, NACL_OPCODE_NULL_OFFSET  },
  /* 1261 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFst, 0x00, 2, 654, NACL_OPCODE_NULL_OFFSET  },
  /* 1262 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFst, 0x00, 2, 708, NACL_OPCODE_NULL_OFFSET  },
  /* 1263 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFst, 0x00, 2, 710, NACL_OPCODE_NULL_OFFSET  },
  /* 1264 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFst, 0x00, 2, 712, NACL_OPCODE_NULL_OFFSET  },
  /* 1265 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFst, 0x00, 2, 714, NACL_OPCODE_NULL_OFFSET  },
  /* 1266 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFst, 0x00, 2, 716, NACL_OPCODE_NULL_OFFSET  },
  /* 1267 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFst, 0x00, 2, 718, NACL_OPCODE_NULL_OFFSET  },
  /* 1268 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFst, 0x00, 2, 720, NACL_OPCODE_NULL_OFFSET  },
  /* 1269 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFstp, 0x00, 2, 654, NACL_OPCODE_NULL_OFFSET  },
  /* 1270 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFstp, 0x00, 2, 708, NACL_OPCODE_NULL_OFFSET  },
  /* 1271 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFstp, 0x00, 2, 710, NACL_OPCODE_NULL_OFFSET  },
  /* 1272 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFstp, 0x00, 2, 712, NACL_OPCODE_NULL_OFFSET  },
  /* 1273 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFstp, 0x00, 2, 714, NACL_OPCODE_NULL_OFFSET  },
  /* 1274 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFstp, 0x00, 2, 716, NACL_OPCODE_NULL_OFFSET  },
  /* 1275 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFstp, 0x00, 2, 718, NACL_OPCODE_NULL_OFFSET  },
  /* 1276 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFstp, 0x00, 2, 720, NACL_OPCODE_NULL_OFFSET  },
  /* 1277 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucom, 0x00, 2, 638, NACL_OPCODE_NULL_OFFSET  },
  /* 1278 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucom, 0x00, 2, 640, NACL_OPCODE_NULL_OFFSET  },
  /* 1279 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucom, 0x00, 2, 642, NACL_OPCODE_NULL_OFFSET  },
  /* 1280 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucom, 0x00, 2, 644, NACL_OPCODE_NULL_OFFSET  },
  /* 1281 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucom, 0x00, 2, 646, NACL_OPCODE_NULL_OFFSET  },
  /* 1282 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucom, 0x00, 2, 648, NACL_OPCODE_NULL_OFFSET  },
  /* 1283 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucom, 0x00, 2, 650, NACL_OPCODE_NULL_OFFSET  },
  /* 1284 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucom, 0x00, 2, 652, NACL_OPCODE_NULL_OFFSET  },
  /* 1285 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomp, 0x00, 2, 638, NACL_OPCODE_NULL_OFFSET  },
  /* 1286 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomp, 0x00, 2, 640, NACL_OPCODE_NULL_OFFSET  },
  /* 1287 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomp, 0x00, 2, 642, NACL_OPCODE_NULL_OFFSET  },
  /* 1288 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomp, 0x00, 2, 644, NACL_OPCODE_NULL_OFFSET  },
  /* 1289 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomp, 0x00, 2, 646, NACL_OPCODE_NULL_OFFSET  },
  /* 1290 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomp, 0x00, 2, 648, NACL_OPCODE_NULL_OFFSET  },
  /* 1291 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomp, 0x00, 2, 650, NACL_OPCODE_NULL_OFFSET  },
  /* 1292 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomp, 0x00, 2, 652, NACL_OPCODE_NULL_OFFSET  },
  /* 1293 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFaddp, 0x00, 2, 622, NACL_OPCODE_NULL_OFFSET  },
  /* 1294 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFaddp, 0x00, 2, 686, NACL_OPCODE_NULL_OFFSET  },
  /* 1295 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFaddp, 0x00, 2, 688, NACL_OPCODE_NULL_OFFSET  },
  /* 1296 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFaddp, 0x00, 2, 690, NACL_OPCODE_NULL_OFFSET  },
  /* 1297 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFaddp, 0x00, 2, 692, NACL_OPCODE_NULL_OFFSET  },
  /* 1298 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFaddp, 0x00, 2, 694, NACL_OPCODE_NULL_OFFSET  },
  /* 1299 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFaddp, 0x00, 2, 696, NACL_OPCODE_NULL_OFFSET  },
  /* 1300 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFaddp, 0x00, 2, 698, NACL_OPCODE_NULL_OFFSET  },
  /* 1301 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmulp, 0x00, 2, 622, NACL_OPCODE_NULL_OFFSET  },
  /* 1302 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmulp, 0x00, 2, 686, NACL_OPCODE_NULL_OFFSET  },
  /* 1303 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmulp, 0x00, 2, 688, NACL_OPCODE_NULL_OFFSET  },
  /* 1304 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmulp, 0x00, 2, 690, NACL_OPCODE_NULL_OFFSET  },
  /* 1305 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmulp, 0x00, 2, 692, NACL_OPCODE_NULL_OFFSET  },
  /* 1306 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmulp, 0x00, 2, 694, NACL_OPCODE_NULL_OFFSET  },
  /* 1307 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmulp, 0x00, 2, 696, NACL_OPCODE_NULL_OFFSET  },
  /* 1308 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmulp, 0x00, 2, 698, NACL_OPCODE_NULL_OFFSET  },
  /* 1309 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcompp, 0x00, 2, 640, NACL_OPCODE_NULL_OFFSET  },
  /* 1310 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubrp, 0x00, 2, 622, NACL_OPCODE_NULL_OFFSET  },
  /* 1311 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubrp, 0x00, 2, 686, NACL_OPCODE_NULL_OFFSET  },
  /* 1312 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubrp, 0x00, 2, 688, NACL_OPCODE_NULL_OFFSET  },
  /* 1313 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubrp, 0x00, 2, 690, NACL_OPCODE_NULL_OFFSET  },
  /* 1314 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubrp, 0x00, 2, 692, NACL_OPCODE_NULL_OFFSET  },
  /* 1315 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubrp, 0x00, 2, 694, NACL_OPCODE_NULL_OFFSET  },
  /* 1316 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubrp, 0x00, 2, 696, NACL_OPCODE_NULL_OFFSET  },
  /* 1317 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubrp, 0x00, 2, 698, NACL_OPCODE_NULL_OFFSET  },
  /* 1318 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubp, 0x00, 2, 622, NACL_OPCODE_NULL_OFFSET  },
  /* 1319 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubp, 0x00, 2, 686, NACL_OPCODE_NULL_OFFSET  },
  /* 1320 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubp, 0x00, 2, 688, NACL_OPCODE_NULL_OFFSET  },
  /* 1321 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubp, 0x00, 2, 690, NACL_OPCODE_NULL_OFFSET  },
  /* 1322 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubp, 0x00, 2, 692, NACL_OPCODE_NULL_OFFSET  },
  /* 1323 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubp, 0x00, 2, 694, NACL_OPCODE_NULL_OFFSET  },
  /* 1324 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubp, 0x00, 2, 696, NACL_OPCODE_NULL_OFFSET  },
  /* 1325 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubp, 0x00, 2, 698, NACL_OPCODE_NULL_OFFSET  },
  /* 1326 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivrp, 0x00, 2, 622, NACL_OPCODE_NULL_OFFSET  },
  /* 1327 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivrp, 0x00, 2, 686, NACL_OPCODE_NULL_OFFSET  },
  /* 1328 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivrp, 0x00, 2, 688, NACL_OPCODE_NULL_OFFSET  },
  /* 1329 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivrp, 0x00, 2, 690, NACL_OPCODE_NULL_OFFSET  },
  /* 1330 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivrp, 0x00, 2, 692, NACL_OPCODE_NULL_OFFSET  },
  /* 1331 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivrp, 0x00, 2, 694, NACL_OPCODE_NULL_OFFSET  },
  /* 1332 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivrp, 0x00, 2, 696, NACL_OPCODE_NULL_OFFSET  },
  /* 1333 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivrp, 0x00, 2, 698, NACL_OPCODE_NULL_OFFSET  },
  /* 1334 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivp, 0x00, 2, 622, NACL_OPCODE_NULL_OFFSET  },
  /* 1335 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivp, 0x00, 2, 686, NACL_OPCODE_NULL_OFFSET  },
  /* 1336 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivp, 0x00, 2, 688, NACL_OPCODE_NULL_OFFSET  },
  /* 1337 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivp, 0x00, 2, 690, NACL_OPCODE_NULL_OFFSET  },
  /* 1338 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivp, 0x00, 2, 692, NACL_OPCODE_NULL_OFFSET  },
  /* 1339 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivp, 0x00, 2, 694, NACL_OPCODE_NULL_OFFSET  },
  /* 1340 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivp, 0x00, 2, 696, NACL_OPCODE_NULL_OFFSET  },
  /* 1341 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivp, 0x00, 2, 698, NACL_OPCODE_NULL_OFFSET  },
  /* 1342 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 1343 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFnstsw, 0x00, 1, 722, NACL_OPCODE_NULL_OFFSET  },
  /* 1344 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomip, 0x00, 2, 638, NACL_OPCODE_NULL_OFFSET  },
  /* 1345 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomip, 0x00, 2, 640, NACL_OPCODE_NULL_OFFSET  },
  /* 1346 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomip, 0x00, 2, 642, NACL_OPCODE_NULL_OFFSET  },
  /* 1347 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomip, 0x00, 2, 644, NACL_OPCODE_NULL_OFFSET  },
  /* 1348 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomip, 0x00, 2, 646, NACL_OPCODE_NULL_OFFSET  },
  /* 1349 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomip, 0x00, 2, 648, NACL_OPCODE_NULL_OFFSET  },
  /* 1350 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomip, 0x00, 2, 650, NACL_OPCODE_NULL_OFFSET  },
  /* 1351 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomip, 0x00, 2, 652, NACL_OPCODE_NULL_OFFSET  },
  /* 1352 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomip, 0x00, 2, 638, NACL_OPCODE_NULL_OFFSET  },
  /* 1353 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomip, 0x00, 2, 640, NACL_OPCODE_NULL_OFFSET  },
  /* 1354 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomip, 0x00, 2, 642, NACL_OPCODE_NULL_OFFSET  },
  /* 1355 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomip, 0x00, 2, 644, NACL_OPCODE_NULL_OFFSET  },
  /* 1356 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomip, 0x00, 2, 646, NACL_OPCODE_NULL_OFFSET  },
  /* 1357 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomip, 0x00, 2, 648, NACL_OPCODE_NULL_OFFSET  },
  /* 1358 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomip, 0x00, 2, 650, NACL_OPCODE_NULL_OFFSET  },
  /* 1359 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomip, 0x00, 2, 652, NACL_OPCODE_NULL_OFFSET  },
  /* 1360 */
  { NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstUd2, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 1361 */
  { NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstNop, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 1362 */
  { NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstPause, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
};

static const NaClPrefixOpcodeArrayOffset g_LookupTable[2543] = {
  /*     0 */ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 
  /*    10 */ 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 
  /*    20 */ 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 
  /*    30 */ 31, 32, 33, 34, 35, 36, 37, 38, 16, 39, 
  /*    40 */ 40, 41, 42, 43, 44, 45, 16, 46, 47, 48, 
  /*    50 */ 49, 50, 51, 52, 16, 53, 54, 55, 56, 57, 
  /*    60 */ 58, 59, 16, 60, 61, 62, 63, 64, 65, 66, 
  /*    70 */ 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 
  /*    80 */ 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 
  /*    90 */ 87, 88, 89, 90, 91, 92, 94, 96, 97, 98, 
  /*   100 */ 16, 16, 16, 16, 99, 100, 101, 102, 103, 105, 
  /*   110 */ 106, 108, 109, 110, 111, 112, 113, 114, 115, 116, 
  /*   120 */ 117, 118, 119, 120, 121, 122, 123, 124, 132, 140, 
  /*   130 */ 148, 156, 157, 158, 159, 160, 161, 162, 163, 164, 
  /*   140 */ 165, 166, 167, 169, 170, 171, 172, 173, 174, 175, 
  /*   150 */ 176, 177, 179, 181, 183, 184, 186, 188, 189, 190, 
  /*   160 */ 191, 192, 193, 194, 195, 197, 198, 200, 201, 202, 
  /*   170 */ 203, 205, 206, 208, 209, 211, 212, 213, 214, 215, 
  /*   180 */ 216, 217, 218, 219, 220, 221, 222, 223, 224, 225, 
  /*   190 */ 226, 227, 235, 243, 244, 245, 246, 247, 248, 249, 
  /*   200 */ 250, 251, 252, 253, 254, 255, 256, 258, 266, 274, 
  /*   210 */ 282, 290, 291, 292, 293, 294, 302, 310, 318, 326, 
  /*   220 */ 334, 342, 350, 358, 359, 360, 361, 363, 364, 365, 
  /*   230 */ 366, 367, 368, 369, 370, 371, 372, 373, 374, 375, 
  /*   240 */ 16, 376, 16, 16, 377, 378, 386, 394, 395, 396, 
  /*   250 */ 397, 398, 399, 400, 408, 415, 421, 440, 441, 442, 
  /*   260 */ 16, NACL_OPCODE_NULL_OFFSET, 443, NACL_OPCODE_NULL_OFFSET, 444, 445, 16, 446, 16, 454, 
  /*   270 */ 455, 456, 457, 458, 460, 461, 462, 463, 465, 466, 
  /*   280 */ 474, 475, 475, 475, 475, 475, 475, 476, 477, 478, 
  /*   290 */ 479, 480, 16, 16, 16, 16, 481, 482, 483, 484, 
  /*   300 */ 485, 486, 487, 488, 489, 490, 491, 492, 493, 494, 
  /*   310 */ 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 
  /*   320 */ 495, 496, 497, 498, 499, 500, 501, 502, 503, 504, 
  /*   330 */ 505, 506, 507, 508, 509, 510, 511, 512, 513, 514, 
  /*   340 */ 515, 516, 517, 518, 519, 520, 521, 522, 523, 524, 
  /*   350 */ 525, 526, 527, 528, 529, 530, 531, 532, 533, 534, 
  /*   360 */ 535, 536, 537, 538, 16, 16, 539, 540, 541, 548, 
  /*   370 */ 555, 562, 563, 564, 565, 566, 16, 16, 16, 16, 
  /*   380 */ 16, 16, 567, 568, 569, 570, 571, 572, 573, 574, 
  /*   390 */ 575, 576, 577, 578, 579, 580, 581, 582, 583, 584, 
  /*   400 */ 585, 586, 587, 588, 589, 590, 591, 592, 593, 594, 
  /*   410 */ 595, 596, 597, 598, 599, 600, 601, 602, 603, 604, 
  /*   420 */ 605, 606, 16, 16, 607, 608, 609, 610, 611, 612, 
  /*   430 */ 642, 643, 644, 645, 646, 647, 648, 649, 650, 651, 
  /*   440 */ 16, 168, 655, 656, 657, 658, 659, 660, 661, 662, 
  /*   450 */ 663, 664, 665, 666, 667, 668, 669, 670, 671, 672, 
  /*   460 */ 673, 674, 675, 676, 16, 677, 678, 679, 680, 681, 
  /*   470 */ 16, 682, 683, 684, 685, 686, 687, 688, 689, 690, 
  /*   480 */ 691, 692, 693, 694, 695, 696, 16, 697, 698, 699, 
  /*   490 */ 700, 701, 702, 703, 704, 705, 16, 706, 707, 708, 
  /*   500 */ 709, 710, 711, 712, 713, 714, 715, 716, 717, 718, 
  /*   510 */ 719, 16, NACL_OPCODE_NULL_OFFSET, 720, 721, 722, 723, 723, 723, 723, 
  /*   520 */ 723, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   530 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 723, 723, 724, 
  /*   540 */ 725, 726, 727, 723, 723, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   550 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   560 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   570 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 723, 728, 723, 
  /*   580 */ 723, 723, 723, 723, 723, 729, 730, 731, 723, 732, 
  /*   590 */ 733, 734, 735, 723, 723, 723, 723, 723, 723, 723, 
  /*   600 */ 723, 723, 723, 723, 723, 723, 723, 723, 723, 736, 
  /*   610 */ 723, 723, 723, 723, 723, 723, 723, 737, 738, 723, 
  /*   620 */ 723, 739, 740, 723, 723, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   630 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   640 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   650 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   660 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   670 */ NACL_OPCODE_NULL_OFFSET, 723, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   680 */ NACL_OPCODE_NULL_OFFSET, 723, 723, 723, 723, 723, 723, 723, 723, NACL_OPCODE_NULL_OFFSET, 
  /*   690 */ NACL_OPCODE_NULL_OFFSET, 741, 723, 723, 723, 723, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   700 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 742, 723, 723, 723, 723, 
  /*   710 */ 723, 743, 723, 723, 723, 723, 723, 723, 723, 723, 
  /*   720 */ 723, 723, 723, 723, 723, 723, 723, 744, 723, 723, 
  /*   730 */ 723, 723, 723, 723, 723, 723, 723, 745, 723, 723, 
  /*   740 */ 723, 723, 723, 723, 723, 723, 723, 723, 723, 723, 
  /*   750 */ 723, 723, 723, NACL_OPCODE_NULL_OFFSET, 746, 747, 748, 749, 749, 749, 
  /*   760 */ 750, 749, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   770 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 749, 749, 
  /*   780 */ 751, 752, 753, 754, 749, 749, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   790 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   800 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   810 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 749, 755, 
  /*   820 */ 756, 757, 749, 749, 749, 749, 758, 759, 760, 761, 
  /*   830 */ 762, 763, 764, 765, 749, 749, 749, 749, 749, 749, 
  /*   840 */ 749, 749, 749, 749, 749, 749, 749, 749, 749, 766, 
  /*   850 */ 767, 749, 749, 749, 749, 749, 749, 749, 749, 749, 
  /*   860 */ 749, 749, 749, 749, 768, 769, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   870 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   880 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   890 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   900 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   910 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   920 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 770, 749, 749, 749, 749, 771, 749, 749, 
  /*   930 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 772, 749, 749, 749, 749, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   940 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 749, 749, 749, 749, 
  /*   950 */ 749, 749, 773, 749, 749, 749, 749, 749, 749, 749, 
  /*   960 */ 749, 749, 749, 749, 749, 749, 749, 749, 774, 749, 
  /*   970 */ 749, 749, 749, 749, 749, 749, 749, 749, 749, 749, 
  /*   980 */ 749, 749, 749, 749, 749, 749, 749, 749, 749, 749, 
  /*   990 */ 749, 749, 749, 749, NACL_OPCODE_NULL_OFFSET, 775, 776, 777, 778, 779, 
  /*  1000 */ 780, 781, 782, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1010 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 783, 
  /*  1020 */ 784, 785, 786, 787, 788, 789, 790, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1030 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1040 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1050 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 791, 
  /*  1060 */ 792, 793, 793, 794, 795, 796, 797, 798, 799, 800, 
  /*  1070 */ 801, 802, 803, 804, 805, 806, 807, 808, 809, 810, 
  /*  1080 */ 811, 812, 813, 814, 815, 816, 817, 818, 819, 820, 
  /*  1090 */ 821, 822, 830, 837, 845, 846, 847, 848, 793, 850, 
  /*  1100 */ 851, 793, 793, 852, 853, 854, 855, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1110 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1120 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1130 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1140 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1150 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 793, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1160 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1170 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 856, 793, 857, 858, 859, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1180 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 860, 861, 862, 
  /*  1190 */ 863, 864, 865, 866, 867, 868, 869, 870, 871, 872, 
  /*  1200 */ 873, 874, 875, 876, 877, 878, 879, 880, 881, 882, 
  /*  1210 */ 883, 884, 885, 886, 887, 888, 889, 890, 891, 793, 
  /*  1220 */ 892, 893, 894, 895, 896, 897, 898, 899, 900, 901, 
  /*  1230 */ 902, 903, 904, 905, 793, NACL_OPCODE_NULL_OFFSET, 906, 907, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1240 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1250 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 908, 909, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1260 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1270 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1280 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1290 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1300 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1310 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1320 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1330 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1340 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1350 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1360 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 910, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 911, NACL_OPCODE_NULL_OFFSET, 912, NACL_OPCODE_NULL_OFFSET, 
  /*  1370 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 913, NACL_OPCODE_NULL_OFFSET, 914, 915, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 916, NACL_OPCODE_NULL_OFFSET, 
  /*  1380 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 917, NACL_OPCODE_NULL_OFFSET, 918, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 919, NACL_OPCODE_NULL_OFFSET, 
  /*  1390 */ 920, 921, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 922, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 923, NACL_OPCODE_NULL_OFFSET, 
  /*  1400 */ 924, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 925, NACL_OPCODE_NULL_OFFSET, 926, 927, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1410 */ NACL_OPCODE_NULL_OFFSET, 928, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 929, NACL_OPCODE_NULL_OFFSET, 930, 931, 932, 
  /*  1420 */ 933, 934, 935, 936, 937, 938, 939, 940, 941, 16, 
  /*  1430 */ 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 
  /*  1440 */ 16, 16, 16, 16, 16, 942, 943, 944, 16, 16, 
  /*  1450 */ 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 
  /*  1460 */ 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 
  /*  1470 */ 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 
  /*  1480 */ 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 
  /*  1490 */ 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 
  /*  1500 */ 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 
  /*  1510 */ 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 
  /*  1520 */ 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 
  /*  1530 */ 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 
  /*  1540 */ 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 
  /*  1550 */ 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 
  /*  1560 */ 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 
  /*  1570 */ 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 
  /*  1580 */ 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 
  /*  1590 */ 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 
  /*  1600 */ 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 
  /*  1610 */ 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 
  /*  1620 */ 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 
  /*  1630 */ 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 
  /*  1640 */ 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 
  /*  1650 */ 16, 16, 16, 16, 16, 16, 16, 945, 946, 16, 
  /*  1660 */ 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 
  /*  1670 */ 16, 16, 16, 947, 948, 949, 950, 951, 952, 953, 
  /*  1680 */ 954, 955, 956, 957, 958, 793, 793, 793, 793, 959, 
  /*  1690 */ 793, 793, 793, 960, 961, 793, 962, 793, 793, 793, 
  /*  1700 */ 793, 963, 964, 965, 793, 966, 967, 968, 969, 970, 
  /*  1710 */ 971, 793, 793, 972, 973, 974, 975, 793, 793, 793, 
  /*  1720 */ 793, 976, 977, 978, 979, 980, 981, 793, 982, 983, 
  /*  1730 */ 984, 985, 986, 987, 988, 989, 990, 991, 992, 793, 
  /*  1740 */ 793, 793, 793, 793, 793, 793, 793, 793, 793, 793, 
  /*  1750 */ 793, 793, 793, 793, 793, 793, 793, 793, 793, 793, 
  /*  1760 */ 793, 793, 793, 793, 793, 793, 793, 793, 793, 793, 
  /*  1770 */ 793, 793, 793, 793, 793, 793, 793, 793, 793, 793, 
  /*  1780 */ 793, 793, 793, 793, 793, 793, 793, 793, 793, 793, 
  /*  1790 */ 793, 793, 793, 793, 793, 793, 793, 793, 793, 793, 
  /*  1800 */ 793, 993, 994, 793, 793, 793, 793, 793, 793, 793, 
  /*  1810 */ 793, 793, 793, 793, 793, 793, 793, 793, 793, 793, 
  /*  1820 */ 793, 793, 793, 793, 793, 793, 793, 793, 793, 793, 
  /*  1830 */ 793, 793, 793, 793, 793, 793, 793, 793, 793, 793, 
  /*  1840 */ 793, 793, 793, 793, 793, 793, 793, 793, 793, 793, 
  /*  1850 */ 793, 793, 793, 793, 793, 793, 793, 793, 793, 793, 
  /*  1860 */ 793, 793, 793, 793, 793, 793, 793, 793, 793, 793, 
  /*  1870 */ 793, 793, 793, 793, 793, 793, 793, 793, 793, 793, 
  /*  1880 */ 793, 793, 793, 793, 793, 793, 793, 793, 793, 793, 
  /*  1890 */ 793, 793, 793, 793, 793, 793, 793, 793, 793, 793, 
  /*  1900 */ 793, 793, 793, 793, 793, 793, 793, 793, 793, 793, 
  /*  1910 */ 793, 793, 793, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 793, 793, 793, 793, 793, 
  /*  1920 */ 793, 793, 793, 793, 793, 793, 793, 793, 793, NACL_OPCODE_NULL_OFFSET, 
  /*  1930 */ 995, 996, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 997, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 998, 999, 1000, 
  /*  1940 */ 1001, 1002, 1003, 1004, 1005, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 1006, 
  /*  1950 */ 1007, 1008, 1009, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1960 */ NACL_OPCODE_NULL_OFFSET, 1010, 1011, 1012, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1970 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1980 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1990 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 1013, 1014, 1015, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  2000 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  2010 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  2020 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 1016, 1017, 1018, 1019, NACL_OPCODE_NULL_OFFSET, 
  /*  2030 */ NACL_OPCODE_NULL_OFFSET, 1020, 1021, 1022, 1023, 1024, 1025, 1026, 1027, 1028, 
  /*  2040 */ 1029, 1030, 1031, 1032, 1033, 1034, 1035, 1036, 1037, 1038, 
  /*  2050 */ 1039, 1040, 1041, 1042, 1043, 1044, 1045, 1046, 1047, 1048, 
  /*  2060 */ 1049, 1050, 1051, 1052, 1053, 1054, 1055, 1056, 1057, 1058, 
  /*  2070 */ 1059, 1060, 1061, 1062, 1063, 1064, 1065, 1066, 1067, 1068, 
  /*  2080 */ 1069, 1070, 1071, 1072, 1073, 1074, 1075, 1076, 1077, 1078, 
  /*  2090 */ 1079, 1080, 1081, 1082, 1083, NACL_OPCODE_NULL_OFFSET, 1084, 1085, 1086, 1087, 
  /*  2100 */ 1088, 1089, 1090, 1091, 1092, 1093, 1094, 1095, 1096, 1097, 
  /*  2110 */ 1098, 1099, 1100, 16, 16, 16, 16, 16, 16, 16, 
  /*  2120 */ 16, 16, 16, 16, 16, 16, 16, 16, 1101, 1102, 
  /*  2130 */ 16, 16, 1103, 1104, 16, 16, 1105, 1106, 1107, 1108, 
  /*  2140 */ 1109, 1110, 1111, 16, 1112, 1113, 1114, 1115, 1116, 1117, 
  /*  2150 */ 1118, 1119, 1120, 1121, 1122, 1123, 1124, 1125, 1126, 1127, 
  /*  2160 */ NACL_OPCODE_NULL_OFFSET, 1128, 1129, 1130, 1131, 1132, 1133, 1134, 1135, 1136, 
  /*  2170 */ 1137, 1138, 1139, 1140, 1141, 1142, 1143, 1144, 1145, 1146, 
  /*  2180 */ 1147, 1148, 1149, 1150, 1151, 1152, 1153, 1154, 1155, 1156, 
  /*  2190 */ 1157, 1158, 1159, 16, 16, 16, 16, 16, 16, 16, 
  /*  2200 */ 16, 16, 1160, 16, 16, 16, 16, 16, 16, 16, 
  /*  2210 */ 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 
  /*  2220 */ 16, 16, 16, 16, 16, NACL_OPCODE_NULL_OFFSET, 1161, 1162, 1163, 1164, 
  /*  2230 */ 1165, 1166, 1167, 1168, 1169, 1170, 1171, 1172, 1173, 1174, 
  /*  2240 */ 1175, 1176, 1177, 1178, 1179, 1180, 1181, 1182, 1183, 1184, 
  /*  2250 */ 1185, 1186, 1187, 1188, 1189, 1190, 1191, 1192, 16, 16, 
  /*  2260 */ 1193, 1194, 16, 16, 16, 16, 1195, 1196, 1197, 1198, 
  /*  2270 */ 1199, 1200, 1201, 1202, 1203, 1204, 1205, 1206, 1207, 1208, 
  /*  2280 */ 1209, 1210, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 1020, 1211, 1212, 1213, 1214, 1215, 
  /*  2290 */ 1216, 1217, 1028, 1218, 1219, 1220, 1221, 1222, 1223, 1224, 
  /*  2300 */ 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 
  /*  2310 */ 16, 16, 16, 16, 16, 16, 1060, 1225, 1226, 1227, 
  /*  2320 */ 1228, 1229, 1230, 1231, 1052, 1232, 1233, 1234, 1235, 1236, 
  /*  2330 */ 1237, 1238, 1076, 1239, 1240, 1241, 1242, 1243, 1244, 1245, 
  /*  2340 */ 1068, 1246, 1247, 1248, 1249, 1250, 1251, 1252, NACL_OPCODE_NULL_OFFSET, 1253, 
  /*  2350 */ 1254, 1255, 1256, 1257, 1258, 1259, 1260, 16, 16, 16, 
  /*  2360 */ 16, 16, 16, 16, 16, 1261, 1262, 1263, 1264, 1265, 
  /*  2370 */ 1266, 1267, 1268, 1269, 1270, 1271, 1272, 1273, 1274, 1275, 
  /*  2380 */ 1276, 1277, 1278, 1279, 1280, 1281, 1282, 1283, 1284, 1285, 
  /*  2390 */ 1286, 1287, 1288, 1289, 1290, 1291, 1292, 16, 16, 16, 
  /*  2400 */ 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 
  /*  2410 */ 16, 16, 16, NACL_OPCODE_NULL_OFFSET, 1293, 1294, 1295, 1296, 1297, 1298, 
  /*  2420 */ 1299, 1300, 1301, 1302, 1303, 1304, 1305, 1306, 1307, 1308, 
  /*  2430 */ 16, 16, 16, 16, 16, 16, 16, 16, 16, 1309, 
  /*  2440 */ 16, 16, 16, 16, 16, 16, 1310, 1311, 1312, 1313, 
  /*  2450 */ 1314, 1315, 1316, 1317, 1318, 1319, 1320, 1321, 1322, 1323, 
  /*  2460 */ 1324, 1325, 1326, 1327, 1328, 1329, 1330, 1331, 1332, 1333, 
  /*  2470 */ 1334, 1335, 1336, 1337, 1338, 1339, 1340, 1341, NACL_OPCODE_NULL_OFFSET, 1342, 
  /*  2480 */ 1342, 1342, 1342, 1342, 1342, 1342, 1342, 1342, 1342, 1342, 
  /*  2490 */ 1342, 1342, 1342, 1342, 1342, 1342, 1342, 1342, 1342, 1342, 
  /*  2500 */ 1342, 1342, 1342, 1342, 1342, 1342, 1342, 1342, 1342, 1342, 
  /*  2510 */ 1342, 1343, 16, 16, 16, 16, 16, 16, 16, 1344, 
  /*  2520 */ 1345, 1346, 1347, 1348, 1349, 1350, 1351, 1352, 1353, 1354, 
  /*  2530 */ 1355, 1356, 1357, 1358, 1359, 16, 16, 16, 16, 16, 
  /*  2540 */ 16, 16, 16, };

static const NaClPrefixOpcodeSelector g_PrefixOpcode[NaClInstPrefixEnumSize] = {
  /*             NoPrefix */ { 0 , 0x00, 0xff },
  /*             Prefix0F */ { 256 , 0x00, 0xff },
  /*           PrefixF20F */ { 512 , 0x0f, 0xff },
  /*           PrefixF30F */ { 753 , 0x0f, 0xff },
  /*           Prefix660F */ { 994 , 0x0f, 0xff },
  /*           Prefix0F0F */ { 1235 , 0x0b, 0xc0 },
  /*           Prefix0F38 */ { 1417 , 0x00, 0xff },
  /*         Prefix660F38 */ { 1673 , 0x00, 0xff },
  /*         PrefixF20F38 */ { 1929 , 0xef, 0xf2 },
  /*           Prefix0F3A */ { 1933 , 0x0e, 0x10 },
  /*         Prefix660F3A */ { 1936 , 0x07, 0x64 },
  /*             PrefixD8 */ { 2030 , 0xbf, 0xff },
  /*             PrefixD9 */ { 2095 , 0xbf, 0xff },
  /*             PrefixDA */ { 2160 , 0xbf, 0xff },
  /*             PrefixDB */ { 2225 , 0xbf, 0xf8 },
  /*             PrefixDC */ { 2283 , 0xbf, 0xff },
  /*             PrefixDD */ { 2348 , 0xbf, 0xff },
  /*             PrefixDE */ { 2413 , 0xbf, 0xff },
  /*             PrefixDF */ { 2478 , 0xbf, 0xff },
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

static const NaClInstNode g_OpcodeSeq[95] = {
  /* 0 */
  { 0x0f,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 1,
    g_OpcodeSeq + 20,
  },
  /* 1 */
  { 0x0b,
    1360,
    NULL,
    g_OpcodeSeq + 2,
  },
  /* 2 */
  { 0x1f,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 3,
    NULL,
  },
  /* 3 */
  { 0x00,
    1361,
    NULL,
    g_OpcodeSeq + 4,
  },
  /* 4 */
  { 0x40,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 5,
    g_OpcodeSeq + 6,
  },
  /* 5 */
  { 0x00,
    1361,
    NULL,
    NULL,
  },
  /* 6 */
  { 0x44,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 7,
    g_OpcodeSeq + 9,
  },
  /* 7 */
  { 0x00,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 8,
    NULL,
  },
  /* 8 */
  { 0x00,
    1361,
    NULL,
    NULL,
  },
  /* 9 */
  { 0x80,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 10,
    g_OpcodeSeq + 14,
  },
  /* 10 */
  { 0x00,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 11,
    NULL,
  },
  /* 11 */
  { 0x00,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 12,
    NULL,
  },
  /* 12 */
  { 0x00,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 13,
    NULL,
  },
  /* 13 */
  { 0x00,
    1361,
    NULL,
    NULL,
  },
  /* 14 */
  { 0x84,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 15,
    NULL,
  },
  /* 15 */
  { 0x00,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 16,
    NULL,
  },
  /* 16 */
  { 0x00,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 17,
    NULL,
  },
  /* 17 */
  { 0x00,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 18,
    NULL,
  },
  /* 18 */
  { 0x00,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 19,
    NULL,
  },
  /* 19 */
  { 0x00,
    1361,
    NULL,
    NULL,
  },
  /* 20 */
  { 0x66,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 21,
    g_OpcodeSeq + 92,
  },
  /* 21 */
  { 0x0f,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 22,
    g_OpcodeSeq + 32,
  },
  /* 22 */
  { 0x1f,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 23,
    NULL,
  },
  /* 23 */
  { 0x44,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 24,
    g_OpcodeSeq + 26,
  },
  /* 24 */
  { 0x00,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 25,
    NULL,
  },
  /* 25 */
  { 0x00,
    1361,
    NULL,
    NULL,
  },
  /* 26 */
  { 0x84,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 27,
    NULL,
  },
  /* 27 */
  { 0x00,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 28,
    NULL,
  },
  /* 28 */
  { 0x00,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 29,
    NULL,
  },
  /* 29 */
  { 0x00,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 30,
    NULL,
  },
  /* 30 */
  { 0x00,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 31,
    NULL,
  },
  /* 31 */
  { 0x00,
    1361,
    NULL,
    NULL,
  },
  /* 32 */
  { 0x2e,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 33,
    g_OpcodeSeq + 41,
  },
  /* 33 */
  { 0x0f,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 34,
    NULL,
  },
  /* 34 */
  { 0x1f,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 35,
    NULL,
  },
  /* 35 */
  { 0x84,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 36,
    NULL,
  },
  /* 36 */
  { 0x00,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 37,
    NULL,
  },
  /* 37 */
  { 0x00,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 38,
    NULL,
  },
  /* 38 */
  { 0x00,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 39,
    NULL,
  },
  /* 39 */
  { 0x00,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 40,
    NULL,
  },
  /* 40 */
  { 0x00,
    1361,
    NULL,
    NULL,
  },
  /* 41 */
  { 0x66,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 42,
    g_OpcodeSeq + 91,
  },
  /* 42 */
  { 0x2e,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 43,
    g_OpcodeSeq + 51,
  },
  /* 43 */
  { 0x0f,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 44,
    NULL,
  },
  /* 44 */
  { 0x1f,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 45,
    NULL,
  },
  /* 45 */
  { 0x84,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 46,
    NULL,
  },
  /* 46 */
  { 0x00,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 47,
    NULL,
  },
  /* 47 */
  { 0x00,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 48,
    NULL,
  },
  /* 48 */
  { 0x00,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 49,
    NULL,
  },
  /* 49 */
  { 0x00,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 50,
    NULL,
  },
  /* 50 */
  { 0x00,
    1361,
    NULL,
    NULL,
  },
  /* 51 */
  { 0x66,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 52,
    NULL,
  },
  /* 52 */
  { 0x2e,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 53,
    g_OpcodeSeq + 61,
  },
  /* 53 */
  { 0x0f,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 54,
    NULL,
  },
  /* 54 */
  { 0x1f,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 55,
    NULL,
  },
  /* 55 */
  { 0x84,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 56,
    NULL,
  },
  /* 56 */
  { 0x00,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 57,
    NULL,
  },
  /* 57 */
  { 0x00,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 58,
    NULL,
  },
  /* 58 */
  { 0x00,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 59,
    NULL,
  },
  /* 59 */
  { 0x00,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 60,
    NULL,
  },
  /* 60 */
  { 0x00,
    1361,
    NULL,
    NULL,
  },
  /* 61 */
  { 0x66,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 62,
    NULL,
  },
  /* 62 */
  { 0x2e,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 63,
    g_OpcodeSeq + 71,
  },
  /* 63 */
  { 0x0f,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 64,
    NULL,
  },
  /* 64 */
  { 0x1f,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 65,
    NULL,
  },
  /* 65 */
  { 0x84,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 66,
    NULL,
  },
  /* 66 */
  { 0x00,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 67,
    NULL,
  },
  /* 67 */
  { 0x00,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 68,
    NULL,
  },
  /* 68 */
  { 0x00,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 69,
    NULL,
  },
  /* 69 */
  { 0x00,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 70,
    NULL,
  },
  /* 70 */
  { 0x00,
    1361,
    NULL,
    NULL,
  },
  /* 71 */
  { 0x66,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 72,
    NULL,
  },
  /* 72 */
  { 0x2e,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 73,
    g_OpcodeSeq + 81,
  },
  /* 73 */
  { 0x0f,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 74,
    NULL,
  },
  /* 74 */
  { 0x1f,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 75,
    NULL,
  },
  /* 75 */
  { 0x84,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 76,
    NULL,
  },
  /* 76 */
  { 0x00,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 77,
    NULL,
  },
  /* 77 */
  { 0x00,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 78,
    NULL,
  },
  /* 78 */
  { 0x00,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 79,
    NULL,
  },
  /* 79 */
  { 0x00,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 80,
    NULL,
  },
  /* 80 */
  { 0x00,
    1361,
    NULL,
    NULL,
  },
  /* 81 */
  { 0x66,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 82,
    NULL,
  },
  /* 82 */
  { 0x2e,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 83,
    NULL,
  },
  /* 83 */
  { 0x0f,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 84,
    NULL,
  },
  /* 84 */
  { 0x1f,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 85,
    NULL,
  },
  /* 85 */
  { 0x84,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 86,
    NULL,
  },
  /* 86 */
  { 0x00,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 87,
    NULL,
  },
  /* 87 */
  { 0x00,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 88,
    NULL,
  },
  /* 88 */
  { 0x00,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 89,
    NULL,
  },
  /* 89 */
  { 0x00,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 90,
    NULL,
  },
  /* 90 */
  { 0x00,
    1361,
    NULL,
    NULL,
  },
  /* 91 */
  { 0x90,
    1361,
    NULL,
    NULL,
  },
  /* 92 */
  { 0x90,
    1361,
    NULL,
    g_OpcodeSeq + 93,
  },
  /* 93 */
  { 0xf3,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 94,
    NULL,
  },
  /* 94 */
  { 0x90,
    1362,
    NULL,
    NULL,
  },
};
