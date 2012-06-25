/*
 * THIS FILE IS AUTO-GENERATED. DO NOT EDIT.
 * Compiled for x86-32 bit mode.
 *
 * You must include ncopcode_desc.h before this file.
 */

static const NaClOp g_Operands[722] = {
  /* 0 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Eb" },
  /* 1 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gb" },
  /* 2 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Ev" },
  /* 3 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gv" },
  /* 4 */ { G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Gb" },
  /* 5 */ { E_Operand, NACL_OPFLAG(OpUse), "$Eb" },
  /* 6 */ { G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Gv" },
  /* 7 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ev" },
  /* 8 */ { RegAL, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%al" },
  /* 9 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 10 */ { RegREAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$rAXv" },
  /* 11 */ { I_Operand, NACL_OPFLAG(OpUse), "$Iz" },
  /* 12 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 13 */ { RegES, NACL_OPFLAG(OpUse), "%es" },
  /* 14 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 15 */ { RegES, NACL_OPFLAG(OpSet), "%es" },
  /* 16 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 17 */ { RegCS, NACL_OPFLAG(OpUse), "%cs" },
  /* 18 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 19 */ { RegSS, NACL_OPFLAG(OpUse), "%ss" },
  /* 20 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 21 */ { RegSS, NACL_OPFLAG(OpSet), "%ss" },
  /* 22 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 23 */ { RegDS, NACL_OPFLAG(OpUse), "%ds" },
  /* 24 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 25 */ { RegDS, NACL_OPFLAG(OpSet), "%ds" },
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
  /* 39 */ { RegRECX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$rCXv" },
  /* 40 */ { RegREDX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$rDXv" },
  /* 41 */ { RegREBX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$rBXv" },
  /* 42 */ { RegRESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$rSPv" },
  /* 43 */ { RegREBP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$rBPv" },
  /* 44 */ { RegRESI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$rSIv" },
  /* 45 */ { RegREDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$rDIv" },
  /* 46 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 47 */ { G_OpcodeBase, NACL_OPFLAG(OpUse), "$r8v" },
  /* 48 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 49 */ { G_OpcodeBase, NACL_OPFLAG(OpSet), "$r8v" },
  /* 50 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 51 */ { RegGP7, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%gp7}" },
  /* 52 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 53 */ { RegGP7, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%gp7}" },
  /* 54 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gv" },
  /* 55 */ { M_Operand, NACL_OPFLAG(OpUse), "$Ma" },
  /* 56 */ { M_Operand, NACL_OPFLAG(OpUse), NULL },
  /* 57 */ { Ew_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Ew" },
  /* 58 */ { Gw_Operand, NACL_OPFLAG(OpUse), "$Gw" },
  /* 59 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 60 */ { I_Operand, NACL_OPFLAG(OpUse), "$Iz" },
  /* 61 */ { G_Operand, NACL_OPFLAG(OpSet), "$Gv" },
  /* 62 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ev" },
  /* 63 */ { I_Operand, NACL_OPFLAG(OpUse), "$Iz" },
  /* 64 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 65 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 66 */ { G_Operand, NACL_OPFLAG(OpSet), "$Gv" },
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
  /* 85 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Eb" },
  /* 86 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 87 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Ev" },
  /* 88 */ { I_Operand, NACL_OPFLAG(OpUse), "$Iz" },
  /* 89 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Ev" },
  /* 90 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 91 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Eb" },
  /* 92 */ { G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Gb" },
  /* 93 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Ev" },
  /* 94 */ { G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Gv" },
  /* 95 */ { E_Operand, NACL_OPFLAG(OpSet), "$Eb" },
  /* 96 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gb" },
  /* 97 */ { E_Operand, NACL_OPFLAG(OpSet), "$Ev" },
  /* 98 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gv" },
  /* 99 */ { G_Operand, NACL_OPFLAG(OpSet), "$Gb" },
  /* 100 */ { E_Operand, NACL_OPFLAG(OpUse), "$Eb" },
  /* 101 */ { E_Operand, NACL_OPFLAG(OpSet), "$Mw/Rv" },
  /* 102 */ { S_Operand, NACL_OPFLAG(OpUse), "$Sw" },
  /* 103 */ { G_Operand, NACL_OPFLAG(OpSet), "$Gv" },
  /* 104 */ { M_Operand, NACL_OPFLAG(OpAddress), "$M" },
  /* 105 */ { S_Operand, NACL_OPFLAG(OpSet), "$Sw" },
  /* 106 */ { Ew_Operand, NACL_OPFLAG(OpUse), "$Ew" },
  /* 107 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 108 */ { E_Operand, NACL_OPFLAG(OpSet), "$Ev" },
  /* 109 */ { G_OpcodeBase, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$r8v" },
  /* 110 */ { RegREAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$rAXv" },
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
  /* 121 */ { A_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandFar) | NACL_OPFLAG(OperandRelative), "$Ap" },
  /* 122 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 123 */ { RegRFLAGS, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Fvd}" },
  /* 124 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 125 */ { RegRFLAGS, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Fvw}" },
  /* 126 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 127 */ { RegRFLAGS, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$Fvd}" },
  /* 128 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 129 */ { RegRFLAGS, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$Fvw}" },
  /* 130 */ { RegAH, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%ah}" },
  /* 131 */ { RegAH, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%ah}" },
  /* 132 */ { RegAL, NACL_OPFLAG(OpSet), "%al" },
  /* 133 */ { O_Operand, NACL_OPFLAG(OpUse), "$Ob" },
  /* 134 */ { RegREAX, NACL_OPFLAG(OpSet), "$rAXv" },
  /* 135 */ { O_Operand, NACL_OPFLAG(OpUse), "$Ov" },
  /* 136 */ { O_Operand, NACL_OPFLAG(OpSet), "$Ob" },
  /* 137 */ { RegAL, NACL_OPFLAG(OpUse), "%al" },
  /* 138 */ { O_Operand, NACL_OPFLAG(OpSet), "$Ov" },
  /* 139 */ { RegREAX, NACL_OPFLAG(OpUse), "$rAXv" },
  /* 140 */ { RegES_EDI, NACL_OPFLAG(OpSet), "$Yb" },
  /* 141 */ { RegDS_ESI, NACL_OPFLAG(OpUse), "$Xb" },
  /* 142 */ { RegES_EDI, NACL_OPFLAG(OpSet), "$Yvd" },
  /* 143 */ { RegDS_ESI, NACL_OPFLAG(OpUse), "$Xvd" },
  /* 144 */ { RegES_EDI, NACL_OPFLAG(OpSet), "$Yvw" },
  /* 145 */ { RegDS_ESI, NACL_OPFLAG(OpUse), "$Xvw" },
  /* 146 */ { RegES_EDI, NACL_OPFLAG(OpUse), "$Yb" },
  /* 147 */ { RegDS_ESI, NACL_OPFLAG(OpUse), "$Xb" },
  /* 148 */ { RegES_EDI, NACL_OPFLAG(OpUse), "$Yvd" },
  /* 149 */ { RegDS_ESI, NACL_OPFLAG(OpUse), "$Xvd" },
  /* 150 */ { RegES_EDI, NACL_OPFLAG(OpUse), "$Yvw" },
  /* 151 */ { RegDS_ESI, NACL_OPFLAG(OpUse), "$Xvw" },
  /* 152 */ { RegES_EDI, NACL_OPFLAG(OpSet), "$Yb" },
  /* 153 */ { RegAL, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%al}" },
  /* 154 */ { RegES_EDI, NACL_OPFLAG(OpSet), "$Yvd" },
  /* 155 */ { RegEAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$rAXvd}" },
  /* 156 */ { RegES_EDI, NACL_OPFLAG(OpSet), "$Yvw" },
  /* 157 */ { RegAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$rAXvw}" },
  /* 158 */ { RegAL, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%al}" },
  /* 159 */ { RegDS_ESI, NACL_OPFLAG(OpUse), "$Xb" },
  /* 160 */ { RegEAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$rAXvd}" },
  /* 161 */ { RegDS_ESI, NACL_OPFLAG(OpUse), "$Xvd" },
  /* 162 */ { RegAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$rAXvw}" },
  /* 163 */ { RegDS_ESI, NACL_OPFLAG(OpUse), "$Xvw" },
  /* 164 */ { RegAL, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%al}" },
  /* 165 */ { RegES_EDI, NACL_OPFLAG(OpUse), "$Yb" },
  /* 166 */ { RegEAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$rAXvd}" },
  /* 167 */ { RegES_EDI, NACL_OPFLAG(OpUse), "$Yvd" },
  /* 168 */ { RegAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$rAXvw}" },
  /* 169 */ { RegES_EDI, NACL_OPFLAG(OpUse), "$Yvw" },
  /* 170 */ { G_OpcodeBase, NACL_OPFLAG(OpSet), "$r8b" },
  /* 171 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 172 */ { G_OpcodeBase, NACL_OPFLAG(OpSet), "$r8v" },
  /* 173 */ { I_Operand, NACL_OPFLAG(OpUse), "$Iv" },
  /* 174 */ { RegEIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eip}" },
  /* 175 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 176 */ { I_Operand, NACL_OPFLAG(OpUse), "$Iw" },
  /* 177 */ { Seg_G_Operand, NACL_OPFLAG(OpSet), "$SGz" },
  /* 178 */ { M_Operand, NACL_OPFLAG(OperandFar), "$Mp" },
  /* 179 */ { E_Operand, NACL_OPFLAG(OpSet), "$Eb" },
  /* 180 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 181 */ { E_Operand, NACL_OPFLAG(OpSet), "$Ev" },
  /* 182 */ { I_Operand, NACL_OPFLAG(OpUse), "$Iz" },
  /* 183 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 184 */ { RegEBP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%ebp}" },
  /* 185 */ { I_Operand, NACL_OPFLAG(OpUse), "$Iw" },
  /* 186 */ { I2_Operand, NACL_OPFLAG(OpUse), "$I2b" },
  /* 187 */ { RegESP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 188 */ { RegEBP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%ebp}" },
  /* 189 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Eb" },
  /* 190 */ { Const_1, NACL_OPFLAG(OpUse), "1" },
  /* 191 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Ev" },
  /* 192 */ { Const_1, NACL_OPFLAG(OpUse), "1" },
  /* 193 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Eb" },
  /* 194 */ { RegCL, NACL_OPFLAG(OpUse), "%cl" },
  /* 195 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Ev" },
  /* 196 */ { RegCL, NACL_OPFLAG(OpUse), "%cl" },
  /* 197 */ { RegAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%ax}" },
  /* 198 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 199 */ { RegAL, NACL_OPFLAG(OpImplicit), "{%al}" },
  /* 200 */ { RegAL, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%al}" },
  /* 201 */ { RegDS_EBX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%DS_EBX}" },
  /* 202 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st0" },
  /* 203 */ { Mv_Operand, NACL_OPFLAG(OpUse), "$Md" },
  /* 204 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 205 */ { Mv_Operand, NACL_OPFLAG(OpUse), "$Md" },
  /* 206 */ { Mw_Operand, NACL_OPFLAG(OpSet), "$Mw" },
  /* 207 */ { M_Operand, NACL_OPFLAG(OpSet), "$M" },
  /* 208 */ { Mw_Operand, NACL_OPFLAG(OpUse), "$Mw" },
  /* 209 */ { M_Operand, NACL_OPFLAG(OpUse), "$M" },
  /* 210 */ { Mv_Operand, NACL_OPFLAG(OpSet), "$Md" },
  /* 211 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 212 */ { RegST0, NACL_OPFLAG(OpSet), "%st0" },
  /* 213 */ { Mv_Operand, NACL_OPFLAG(OpUse), "$Md" },
  /* 214 */ { M_Operand, NACL_OPFLAG(OpSet), "$M" },
  /* 215 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 216 */ { RegST0, NACL_OPFLAG(OpSet), "%st0" },
  /* 217 */ { M_Operand, NACL_OPFLAG(OpUse), "$M" },
  /* 218 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st0" },
  /* 219 */ { Mo_Operand, NACL_OPFLAG(OpUse), "$Mq" },
  /* 220 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 221 */ { Mo_Operand, NACL_OPFLAG(OpUse), "$Mq" },
  /* 222 */ { Mo_Operand, NACL_OPFLAG(OpSet), "$Mq" },
  /* 223 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 224 */ { RegST0, NACL_OPFLAG(OpSet), "%st0" },
  /* 225 */ { Mo_Operand, NACL_OPFLAG(OpUse), "$Mq" },
  /* 226 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st0" },
  /* 227 */ { Mw_Operand, NACL_OPFLAG(OpUse), "$Mw" },
  /* 228 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 229 */ { Mw_Operand, NACL_OPFLAG(OpUse), "$Mw" },
  /* 230 */ { Mw_Operand, NACL_OPFLAG(OpSet), "$Mw" },
  /* 231 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 232 */ { RegST0, NACL_OPFLAG(OpSet), "%st0" },
  /* 233 */ { Mw_Operand, NACL_OPFLAG(OpUse), "$Mw" },
  /* 234 */ { RegEIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eip}" },
  /* 235 */ { RegECX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%ecx}" },
  /* 236 */ { J_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandNear) | NACL_OPFLAG(OperandRelative), "$Jb" },
  /* 237 */ { RegEIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eip}" },
  /* 238 */ { RegCX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%cx}" },
  /* 239 */ { J_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandNear) | NACL_OPFLAG(OperandRelative), "$Jb" },
  /* 240 */ { RegAL, NACL_OPFLAG(OpSet), "%al" },
  /* 241 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 242 */ { RegREAX, NACL_OPFLAG(OpSet), "$rAXv" },
  /* 243 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 244 */ { I_Operand, NACL_OPFLAG(OpSet), "$Ib" },
  /* 245 */ { RegAL, NACL_OPFLAG(OpUse), "%al" },
  /* 246 */ { I_Operand, NACL_OPFLAG(OpSet), "$Ib" },
  /* 247 */ { RegREAX, NACL_OPFLAG(OpUse), "$rAXv" },
  /* 248 */ { RegEIP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eip}" },
  /* 249 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 250 */ { J_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandNear) | NACL_OPFLAG(OperandRelative), "$Jzd" },
  /* 251 */ { RegEIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eip}" },
  /* 252 */ { J_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandNear) | NACL_OPFLAG(OperandRelative), "$Jzd" },
  /* 253 */ { RegEIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eip}" },
  /* 254 */ { A_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandFar) | NACL_OPFLAG(OperandRelative), "$Ap" },
  /* 255 */ { RegAL, NACL_OPFLAG(OpSet), "%al" },
  /* 256 */ { RegDX, NACL_OPFLAG(OpUse), "%dx" },
  /* 257 */ { RegREAX, NACL_OPFLAG(OpSet), "$rAXv" },
  /* 258 */ { RegDX, NACL_OPFLAG(OpUse), "%dx" },
  /* 259 */ { RegDX, NACL_OPFLAG(OpSet), "%dx" },
  /* 260 */ { RegAL, NACL_OPFLAG(OpUse), "%al" },
  /* 261 */ { RegDX, NACL_OPFLAG(OpSet), "%dx" },
  /* 262 */ { RegREAX, NACL_OPFLAG(OpUse), "$rAXv" },
  /* 263 */ { RegAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%ax}" },
  /* 264 */ { RegAL, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%al}" },
  /* 265 */ { E_Operand, NACL_OPFLAG(OpUse), "$Eb" },
  /* 266 */ { RegREDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%redx}" },
  /* 267 */ { RegREAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%reax}" },
  /* 268 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ev" },
  /* 269 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 270 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ev" },
  /* 271 */ { RegEIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eip}" },
  /* 272 */ { M_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandFar), "$Mp" },
  /* 273 */ { RegEIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eip}" },
  /* 274 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandNear), "$Ev" },
  /* 275 */ { RegEIP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eip}" },
  /* 276 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 277 */ { M_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandFar), "$Mp" },
  /* 278 */ { RegEIP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eip}" },
  /* 279 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 280 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandNear), "$Ev" },
  /* 281 */ { Ew_Operand, NACL_EMPTY_OPFLAGS, "$Ew" },
  /* 282 */ { Mb_Operand, NACL_OPFLAG(OpUse), "$Mb" },
  /* 283 */ { RegREAXa, NACL_OPFLAG(OpUse), "$rAXva" },
  /* 284 */ { RegECX, NACL_OPFLAG(OpUse), "%ecx" },
  /* 285 */ { RegEIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eip}" },
  /* 286 */ { RegEAX, NACL_OPFLAG(OpUse), "%eax" },
  /* 287 */ { M_Operand, NACL_OPFLAG(OpUse), "$Ms" },
  /* 288 */ { RegEAX, NACL_EMPTY_OPFLAGS, "%eax" },
  /* 289 */ { RegECX, NACL_EMPTY_OPFLAGS, "%ecx" },
  /* 290 */ { RegREAX, NACL_OPFLAG(OpUse), "%reax" },
  /* 291 */ { RegECX, NACL_OPFLAG(OpUse), "%ecx" },
  /* 292 */ { RegEDX, NACL_OPFLAG(OpUse), "%edx" },
  /* 293 */ { M_Operand, NACL_OPFLAG(OpSet), "$Ms" },
  /* 294 */ { G_Operand, NACL_EMPTY_OPFLAGS, "$Gv" },
  /* 295 */ { Ew_Operand, NACL_EMPTY_OPFLAGS, "$Ew" },
  /* 296 */ { Mb_Operand, NACL_EMPTY_OPFLAGS, "$Mb" },
  /* 297 */ { Mmx_G_Operand, NACL_EMPTY_OPFLAGS, "$Pq" },
  /* 298 */ { Mmx_E_Operand, NACL_EMPTY_OPFLAGS, "$Qq" },
  /* 299 */ { I_Operand, NACL_EMPTY_OPFLAGS, "$Ib" },
  /* 300 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vps" },
  /* 301 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wps" },
  /* 302 */ { Xmm_E_Operand, NACL_OPFLAG(OpSet), "$Wps" },
  /* 303 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vps" },
  /* 304 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vps" },
  /* 305 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$VRq" },
  /* 306 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vps" },
  /* 307 */ { Mo_Operand, NACL_OPFLAG(OpUse), "$Mq" },
  /* 308 */ { Mo_Operand, NACL_OPFLAG(OpSet), "$Mq" },
  /* 309 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vps" },
  /* 310 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Vps" },
  /* 311 */ { Xmm_Eo_Operand, NACL_OPFLAG(OpUse), "$Wq" },
  /* 312 */ { Ev_Operand, NACL_OPFLAG(OpSet), "$Rd/q" },
  /* 313 */ { C_Operand, NACL_OPFLAG(OpUse), "$Cd/q" },
  /* 314 */ { Ev_Operand, NACL_OPFLAG(OpSet), "$Rd/q" },
  /* 315 */ { D_Operand, NACL_OPFLAG(OpUse), "$Dd/q" },
  /* 316 */ { C_Operand, NACL_OPFLAG(OpSet), "$Cd/q" },
  /* 317 */ { Ev_Operand, NACL_OPFLAG(OpUse), "$Rd/q" },
  /* 318 */ { D_Operand, NACL_OPFLAG(OpSet), "$Dd/q" },
  /* 319 */ { Ev_Operand, NACL_OPFLAG(OpUse), "$Rd/q" },
  /* 320 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vps" },
  /* 321 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse), "$Qq" },
  /* 322 */ { Mdq_Operand, NACL_OPFLAG(OpSet), "$Mdq" },
  /* 323 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vps" },
  /* 324 */ { Mmx_G_Operand, NACL_OPFLAG(OpSet), "$Pq" },
  /* 325 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wps" },
  /* 326 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vss" },
  /* 327 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wss" },
  /* 328 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vps" },
  /* 329 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wps" },
  /* 330 */ { RegEAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%eax}" },
  /* 331 */ { RegEDX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%edx}" },
  /* 332 */ { RegECX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%ecx}" },
  /* 333 */ { RegEAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eax}" },
  /* 334 */ { RegEDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%edx}" },
  /* 335 */ { RegEAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eax}" },
  /* 336 */ { RegEDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%edx}" },
  /* 337 */ { RegECX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%ecx}" },
  /* 338 */ { RegEIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eip}" },
  /* 339 */ { RegESP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 340 */ { RegCS, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%cs}" },
  /* 341 */ { RegSS, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%ss}" },
  /* 342 */ { RegEIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eip}" },
  /* 343 */ { RegESP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 344 */ { RegCS, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%cs}" },
  /* 345 */ { RegSS, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%ss}" },
  /* 346 */ { RegEDX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%edx}" },
  /* 347 */ { RegECX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%ecx}" },
  /* 348 */ { Gv_Operand, NACL_OPFLAG(OpSet), "$Gd" },
  /* 349 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$VRps" },
  /* 350 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Vps" },
  /* 351 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wps" },
  /* 352 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vpd" },
  /* 353 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wps" },
  /* 354 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vps" },
  /* 355 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 356 */ { Mmx_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Pq" },
  /* 357 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse), "$Qq" },
  /* 358 */ { Mmx_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Pq" },
  /* 359 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse), "$Qd" },
  /* 360 */ { Mmx_G_Operand, NACL_OPFLAG(OpSet), "$Pq" },
  /* 361 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ed/q/d" },
  /* 362 */ { Mmx_G_Operand, NACL_OPFLAG(OpSet), "$Pq" },
  /* 363 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse), "$Qq" },
  /* 364 */ { Mmx_G_Operand, NACL_OPFLAG(OpSet), "$Pq" },
  /* 365 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse), "$Qq" },
  /* 366 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 367 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$PRq" },
  /* 368 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 369 */ { E_Operand, NACL_OPFLAG(OpSet), "$Ed/q/d" },
  /* 370 */ { Mmx_G_Operand, NACL_OPFLAG(OpUse), "$Pd/q/d" },
  /* 371 */ { Mmx_E_Operand, NACL_OPFLAG(OpSet), "$Qq" },
  /* 372 */ { Mmx_G_Operand, NACL_OPFLAG(OpUse), "$Pq" },
  /* 373 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 374 */ { RegFS, NACL_OPFLAG(OpUse), "%fs" },
  /* 375 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 376 */ { RegFS, NACL_OPFLAG(OpSet), "%fs" },
  /* 377 */ { RegEBX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%ebx}" },
  /* 378 */ { RegEDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%edx}" },
  /* 379 */ { RegEAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eax}" },
  /* 380 */ { RegECX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%ecx}" },
  /* 381 */ { E_Operand, NACL_OPFLAG(OpSet), "$Ev" },
  /* 382 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gv" },
  /* 383 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 384 */ { E_Operand, NACL_OPFLAG(OpSet), "$Ev" },
  /* 385 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gv" },
  /* 386 */ { RegCL, NACL_OPFLAG(OpUse), "%cl" },
  /* 387 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 388 */ { RegGS, NACL_OPFLAG(OpUse), "%gs" },
  /* 389 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 390 */ { RegGS, NACL_OPFLAG(OpSet), "%gs" },
  /* 391 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Ev" },
  /* 392 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gv" },
  /* 393 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 394 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Ev" },
  /* 395 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gv" },
  /* 396 */ { RegCL, NACL_OPFLAG(OpUse), "%cl" },
  /* 397 */ { RegAL, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%al}" },
  /* 398 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Eb" },
  /* 399 */ { G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Gb" },
  /* 400 */ { RegREAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$rAXv}" },
  /* 401 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Ev" },
  /* 402 */ { G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Gv" },
  /* 403 */ { G_Operand, NACL_OPFLAG(OpSet), "$Gv" },
  /* 404 */ { Eb_Operand, NACL_OPFLAG(OpUse), "$Eb" },
  /* 405 */ { G_Operand, NACL_OPFLAG(OpSet), "$Gv" },
  /* 406 */ { Ew_Operand, NACL_OPFLAG(OpUse), "$Ew" },
  /* 407 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Vps" },
  /* 408 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wps" },
  /* 409 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 410 */ { M_Operand, NACL_OPFLAG(OpSet), "$Md/q" },
  /* 411 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gd/q" },
  /* 412 */ { Mmx_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Pq" },
  /* 413 */ { E_Operand, NACL_OPFLAG(OpUse), "$Rd/q/Mw" },
  /* 414 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 415 */ { Gv_Operand, NACL_OPFLAG(OpSet), "$Gd" },
  /* 416 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse), "$PRq" },
  /* 417 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 418 */ { RegEDX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%edx}" },
  /* 419 */ { RegEAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eax}" },
  /* 420 */ { Mo_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Mq" },
  /* 421 */ { G_OpcodeBase, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$r8vq" },
  /* 422 */ { G_OpcodeBase, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$r8vd" },
  /* 423 */ { Mo_Operand, NACL_OPFLAG(OpSet), "$Mq" },
  /* 424 */ { Mmx_G_Operand, NACL_OPFLAG(OpUse), "$Pq" },
  /* 425 */ { RegDS_EDI, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$Zvd}" },
  /* 426 */ { Mmx_G_Operand, NACL_OPFLAG(OpUse), "$Pq" },
  /* 427 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse), "$PRq" },
  /* 428 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vsd" },
  /* 429 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wsd" },
  /* 430 */ { Xmm_E_Operand, NACL_OPFLAG(OpSet), "$Wsd" },
  /* 431 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vsd" },
  /* 432 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vpd" },
  /* 433 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wsd" },
  /* 434 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vsd" },
  /* 435 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ed/q" },
  /* 436 */ { Mo_Operand, NACL_OPFLAG(OpSet), "$Mq" },
  /* 437 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vsd" },
  /* 438 */ { G_Operand, NACL_OPFLAG(OpSet), "$Gd/q" },
  /* 439 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wsd" },
  /* 440 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Vsd" },
  /* 441 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wsd" },
  /* 442 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vss" },
  /* 443 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wsd" },
  /* 444 */ { Xmm_Go_Operand, NACL_OPFLAG(OpSet), "$Vq" },
  /* 445 */ { Xmm_Eo_Operand, NACL_OPFLAG(OpUse), "$Wq" },
  /* 446 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 447 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Vdq" },
  /* 448 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$VRq" },
  /* 449 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 450 */ { I2_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 451 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Vdq" },
  /* 452 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$VRdq" },
  /* 453 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Vsd" },
  /* 454 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wsd" },
  /* 455 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 456 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Vpd" },
  /* 457 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wpd" },
  /* 458 */ { Mmx_G_Operand, NACL_OPFLAG(OpSet), "$Pq" },
  /* 459 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$VRq" },
  /* 460 */ { Xmm_Go_Operand, NACL_OPFLAG(OpSet), "$Vq" },
  /* 461 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wpd" },
  /* 462 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vdq" },
  /* 463 */ { Mdq_Operand, NACL_OPFLAG(OpUse), "$Mdq" },
  /* 464 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vss" },
  /* 465 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wss" },
  /* 466 */ { Xmm_E_Operand, NACL_OPFLAG(OpSet), "$Wss" },
  /* 467 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vss" },
  /* 468 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vss" },
  /* 469 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ed/q" },
  /* 470 */ { Mv_Operand, NACL_OPFLAG(OpSet), "$Md" },
  /* 471 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vss" },
  /* 472 */ { G_Operand, NACL_OPFLAG(OpSet), "$Gd/q" },
  /* 473 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wss" },
  /* 474 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Vss" },
  /* 475 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wss" },
  /* 476 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vsd" },
  /* 477 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wss" },
  /* 478 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vdq" },
  /* 479 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wps" },
  /* 480 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vdq" },
  /* 481 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 482 */ { Xmm_E_Operand, NACL_OPFLAG(OpSet), "$Wdq" },
  /* 483 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 484 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Vss" },
  /* 485 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wss" },
  /* 486 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 487 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vdq" },
  /* 488 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse), "$PRq" },
  /* 489 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vpd" },
  /* 490 */ { Xmm_Eo_Operand, NACL_OPFLAG(OpUse), "$Wq" },
  /* 491 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vpd" },
  /* 492 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wpd" },
  /* 493 */ { Xmm_E_Operand, NACL_OPFLAG(OpSet), "$Wpd" },
  /* 494 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vpd" },
  /* 495 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vsd" },
  /* 496 */ { Mo_Operand, NACL_OPFLAG(OpUse), "$Mq" },
  /* 497 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Vpd" },
  /* 498 */ { Xmm_Eo_Operand, NACL_OPFLAG(OpUse), "$Wq" },
  /* 499 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vpd" },
  /* 500 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse), "$Qq" },
  /* 501 */ { Mdq_Operand, NACL_OPFLAG(OpSet), "$Mdq" },
  /* 502 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vpd" },
  /* 503 */ { Mmx_G_Operand, NACL_OPFLAG(OpSet), "$Pq" },
  /* 504 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wpd" },
  /* 505 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vsd" },
  /* 506 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wsd" },
  /* 507 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vpd" },
  /* 508 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wsd" },
  /* 509 */ { Gv_Operand, NACL_OPFLAG(OpSet), "$Gd" },
  /* 510 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$VRpd" },
  /* 511 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vps" },
  /* 512 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wpd" },
  /* 513 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Vdq" },
  /* 514 */ { Xmm_Eo_Operand, NACL_OPFLAG(OpUse), "$Wq" },
  /* 515 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Vdq" },
  /* 516 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 517 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vdq" },
  /* 518 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ed/q/d" },
  /* 519 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vdq" },
  /* 520 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 521 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 522 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$VRdq" },
  /* 523 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 524 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(AllowGOperandWithOpcodeInModRm), "$Vdq" },
  /* 525 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 526 */ { I2_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 527 */ { E_Operand, NACL_OPFLAG(OpSet), "$Ed/q/d" },
  /* 528 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vd/q/d" },
  /* 529 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Vpd" },
  /* 530 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wpd" },
  /* 531 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 532 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vdq" },
  /* 533 */ { E_Operand, NACL_OPFLAG(OpUse), "$Rd/q/Mw" },
  /* 534 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 535 */ { Gv_Operand, NACL_OPFLAG(OpSet), "$Gd" },
  /* 536 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$VRdq" },
  /* 537 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 538 */ { Xmm_Eo_Operand, NACL_OPFLAG(OpSet), "$Wq" },
  /* 539 */ { Xmm_Go_Operand, NACL_OPFLAG(OpUse), "$Vq" },
  /* 540 */ { Xmm_Go_Operand, NACL_OPFLAG(OpSet), "$Vq" },
  /* 541 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 542 */ { Mdq_Operand, NACL_OPFLAG(OpSet), "$Mdq" },
  /* 543 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 544 */ { RegDS_EDI, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$Zvd}" },
  /* 545 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 546 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$VRdq" },
  /* 547 */ { G_Operand, NACL_OPFLAG(OpSet), "$Gv" },
  /* 548 */ { M_Operand, NACL_OPFLAG(OpUse), "$Mv" },
  /* 549 */ { M_Operand, NACL_OPFLAG(OpSet), "$Mv" },
  /* 550 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gv" },
  /* 551 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vdq" },
  /* 552 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 553 */ { RegXMM0, NACL_OPFLAG(OpUse), "%xmm0" },
  /* 554 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 555 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 556 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vdq" },
  /* 557 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Udq/Mq" },
  /* 558 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vdq" },
  /* 559 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Udq/Md" },
  /* 560 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vdq" },
  /* 561 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Udq/Mw" },
  /* 562 */ { Gv_Operand, NACL_OPFLAG(OpUse), "$Gd" },
  /* 563 */ { Mdq_Operand, NACL_OPFLAG(OpUse), "$Mdq" },
  /* 564 */ { Gv_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Gd" },
  /* 565 */ { E_Operand, NACL_OPFLAG(OpUse), "$Eb" },
  /* 566 */ { Gv_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Gd" },
  /* 567 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ev" },
  /* 568 */ { Mmx_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Pq" },
  /* 569 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse), "$Qq" },
  /* 570 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 571 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vss" },
  /* 572 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wss" },
  /* 573 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 574 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vsd" },
  /* 575 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wsd" },
  /* 576 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 577 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Vdq" },
  /* 578 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 579 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 580 */ { Ev_Operand, NACL_OPFLAG(OpSet), "$Rd/Mb" },
  /* 581 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 582 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 583 */ { Ev_Operand, NACL_OPFLAG(OpSet), "$Rd/Mw" },
  /* 584 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 585 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 586 */ { E_Operand, NACL_OPFLAG(OpSet), "$Ed/q/d" },
  /* 587 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 588 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 589 */ { Ev_Operand, NACL_OPFLAG(OpSet), "$Ed" },
  /* 590 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 591 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 592 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vdq" },
  /* 593 */ { E_Operand, NACL_OPFLAG(OpUse), "$Rd/q/Mb" },
  /* 594 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 595 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vdq" },
  /* 596 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Udq/Md" },
  /* 597 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 598 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vdq" },
  /* 599 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ed/q/d" },
  /* 600 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 601 */ { RegXMM0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%xmm0}" },
  /* 602 */ { RegREAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$rAXv}" },
  /* 603 */ { RegREDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$rDXv}" },
  /* 604 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 605 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 606 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 607 */ { RegRECX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$rCXv}" },
  /* 608 */ { RegREAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$rAXv}" },
  /* 609 */ { RegREDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$rDXv}" },
  /* 610 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 611 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 612 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 613 */ { RegXMM0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%xmm0}" },
  /* 614 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 615 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 616 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 617 */ { RegRECX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$rCXv}" },
  /* 618 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 619 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 620 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 621 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st0" },
  /* 622 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 623 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st0" },
  /* 624 */ { RegST1, NACL_OPFLAG(OpUse), "%st1" },
  /* 625 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st0" },
  /* 626 */ { RegST2, NACL_OPFLAG(OpUse), "%st2" },
  /* 627 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st0" },
  /* 628 */ { RegST3, NACL_OPFLAG(OpUse), "%st3" },
  /* 629 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st0" },
  /* 630 */ { RegST4, NACL_OPFLAG(OpUse), "%st4" },
  /* 631 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st0" },
  /* 632 */ { RegST5, NACL_OPFLAG(OpUse), "%st5" },
  /* 633 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st0" },
  /* 634 */ { RegST6, NACL_OPFLAG(OpUse), "%st6" },
  /* 635 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st0" },
  /* 636 */ { RegST7, NACL_OPFLAG(OpUse), "%st7" },
  /* 637 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 638 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 639 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 640 */ { RegST1, NACL_OPFLAG(OpUse), "%st1" },
  /* 641 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 642 */ { RegST2, NACL_OPFLAG(OpUse), "%st2" },
  /* 643 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 644 */ { RegST3, NACL_OPFLAG(OpUse), "%st3" },
  /* 645 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 646 */ { RegST4, NACL_OPFLAG(OpUse), "%st4" },
  /* 647 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 648 */ { RegST5, NACL_OPFLAG(OpUse), "%st5" },
  /* 649 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 650 */ { RegST6, NACL_OPFLAG(OpUse), "%st6" },
  /* 651 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 652 */ { RegST7, NACL_OPFLAG(OpUse), "%st7" },
  /* 653 */ { RegST0, NACL_OPFLAG(OpSet), "%st0" },
  /* 654 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 655 */ { RegST0, NACL_OPFLAG(OpSet), "%st0" },
  /* 656 */ { RegST1, NACL_OPFLAG(OpUse), "%st1" },
  /* 657 */ { RegST0, NACL_OPFLAG(OpSet), "%st0" },
  /* 658 */ { RegST2, NACL_OPFLAG(OpUse), "%st2" },
  /* 659 */ { RegST0, NACL_OPFLAG(OpSet), "%st0" },
  /* 660 */ { RegST3, NACL_OPFLAG(OpUse), "%st3" },
  /* 661 */ { RegST0, NACL_OPFLAG(OpSet), "%st0" },
  /* 662 */ { RegST4, NACL_OPFLAG(OpUse), "%st4" },
  /* 663 */ { RegST0, NACL_OPFLAG(OpSet), "%st0" },
  /* 664 */ { RegST5, NACL_OPFLAG(OpUse), "%st5" },
  /* 665 */ { RegST0, NACL_OPFLAG(OpSet), "%st0" },
  /* 666 */ { RegST6, NACL_OPFLAG(OpUse), "%st6" },
  /* 667 */ { RegST0, NACL_OPFLAG(OpSet), "%st0" },
  /* 668 */ { RegST7, NACL_OPFLAG(OpUse), "%st7" },
  /* 669 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st0" },
  /* 670 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st0" },
  /* 671 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st0" },
  /* 672 */ { RegST1, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st1" },
  /* 673 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st0" },
  /* 674 */ { RegST2, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st2" },
  /* 675 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st0" },
  /* 676 */ { RegST3, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st3" },
  /* 677 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st0" },
  /* 678 */ { RegST4, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st4" },
  /* 679 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st0" },
  /* 680 */ { RegST5, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st5" },
  /* 681 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st0" },
  /* 682 */ { RegST6, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st6" },
  /* 683 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st0" },
  /* 684 */ { RegST7, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st7" },
  /* 685 */ { RegST1, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st1" },
  /* 686 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 687 */ { RegST2, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st2" },
  /* 688 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 689 */ { RegST3, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st3" },
  /* 690 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 691 */ { RegST4, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st4" },
  /* 692 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 693 */ { RegST5, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st5" },
  /* 694 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 695 */ { RegST6, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st6" },
  /* 696 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 697 */ { RegST7, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st7" },
  /* 698 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 699 */ { RegST0, NACL_EMPTY_OPFLAGS, "%st0" },
  /* 700 */ { RegST1, NACL_EMPTY_OPFLAGS, "%st1" },
  /* 701 */ { RegST2, NACL_EMPTY_OPFLAGS, "%st2" },
  /* 702 */ { RegST3, NACL_EMPTY_OPFLAGS, "%st3" },
  /* 703 */ { RegST4, NACL_EMPTY_OPFLAGS, "%st4" },
  /* 704 */ { RegST5, NACL_EMPTY_OPFLAGS, "%st5" },
  /* 705 */ { RegST6, NACL_EMPTY_OPFLAGS, "%st6" },
  /* 706 */ { RegST7, NACL_EMPTY_OPFLAGS, "%st7" },
  /* 707 */ { RegST1, NACL_OPFLAG(OpSet), "%st1" },
  /* 708 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 709 */ { RegST2, NACL_OPFLAG(OpSet), "%st2" },
  /* 710 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 711 */ { RegST3, NACL_OPFLAG(OpSet), "%st3" },
  /* 712 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 713 */ { RegST4, NACL_OPFLAG(OpSet), "%st4" },
  /* 714 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 715 */ { RegST5, NACL_OPFLAG(OpSet), "%st5" },
  /* 716 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 717 */ { RegST6, NACL_OPFLAG(OpSet), "%st6" },
  /* 718 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 719 */ { RegST7, NACL_OPFLAG(OpSet), "%st7" },
  /* 720 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 721 */ { RegAX, NACL_OPFLAG(OpSet), "%ax" },
};

static const NaClInst g_Opcodes[1370] = {
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
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFwait, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 184 */
  { NACLi_386,
    NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstPushfd, 0x00, 2, 122, NACL_OPCODE_NULL_OFFSET  },
  /* 185 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(NaClIllegal),
    InstPushf, 0x00, 2, 124, 184  },
  /* 186 */
  { NACLi_386,
    NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstPopfd, 0x00, 2, 126, NACL_OPCODE_NULL_OFFSET  },
  /* 187 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(NaClIllegal),
    InstPopf, 0x00, 2, 128, 186  },
  /* 188 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstSahf, 0x00, 1, 130, NACL_OPCODE_NULL_OFFSET  },
  /* 189 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstLahf, 0x00, 1, 131, NACL_OPCODE_NULL_OFFSET  },
  /* 190 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_Addr) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x00, 2, 132, NACL_OPCODE_NULL_OFFSET  },
  /* 191 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_Addr) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMov, 0x00, 2, 134, NACL_OPCODE_NULL_OFFSET  },
  /* 192 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_Addr) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x00, 2, 136, NACL_OPCODE_NULL_OFFSET  },
  /* 193 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_Addr) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMov, 0x00, 2, 138, NACL_OPCODE_NULL_OFFSET  },
  /* 194 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_b),
    InstMovsb, 0x00, 2, 140, NACL_OPCODE_NULL_OFFSET  },
  /* 195 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_v),
    InstMovsd, 0x00, 2, 142, NACL_OPCODE_NULL_OFFSET  },
  /* 196 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w),
    InstMovsw, 0x00, 2, 144, 195  },
  /* 197 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(OperandSize_b),
    InstCmpsb, 0x00, 2, 146, NACL_OPCODE_NULL_OFFSET  },
  /* 198 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(OperandSize_v),
    InstCmpsd, 0x00, 2, 148, NACL_OPCODE_NULL_OFFSET  },
  /* 199 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w),
    InstCmpsw, 0x00, 2, 150, 198  },
  /* 200 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b),
    InstTest, 0x00, 2, 35, NACL_OPCODE_NULL_OFFSET  },
  /* 201 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstTest, 0x00, 2, 37, NACL_OPCODE_NULL_OFFSET  },
  /* 202 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_b),
    InstStosb, 0x00, 2, 152, NACL_OPCODE_NULL_OFFSET  },
  /* 203 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_v),
    InstStosd, 0x00, 2, 154, NACL_OPCODE_NULL_OFFSET  },
  /* 204 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w),
    InstStosw, 0x00, 2, 156, 203  },
  /* 205 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_b),
    InstLodsb, 0x00, 2, 158, NACL_OPCODE_NULL_OFFSET  },
  /* 206 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_v),
    InstLodsd, 0x00, 2, 160, NACL_OPCODE_NULL_OFFSET  },
  /* 207 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w),
    InstLodsw, 0x00, 2, 162, 206  },
  /* 208 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(OperandSize_b),
    InstScasb, 0x00, 2, 164, NACL_OPCODE_NULL_OFFSET  },
  /* 209 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(OperandSize_v),
    InstScasd, 0x00, 2, 166, NACL_OPCODE_NULL_OFFSET  },
  /* 210 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w),
    InstScasw, 0x00, 2, 168, 209  },
  /* 211 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x00, 2, 170, NACL_OPCODE_NULL_OFFSET  },
  /* 212 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x01, 2, 170, NACL_OPCODE_NULL_OFFSET  },
  /* 213 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x02, 2, 170, NACL_OPCODE_NULL_OFFSET  },
  /* 214 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x03, 2, 170, NACL_OPCODE_NULL_OFFSET  },
  /* 215 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x04, 2, 170, NACL_OPCODE_NULL_OFFSET  },
  /* 216 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x05, 2, 170, NACL_OPCODE_NULL_OFFSET  },
  /* 217 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x06, 2, 170, NACL_OPCODE_NULL_OFFSET  },
  /* 218 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x07, 2, 170, NACL_OPCODE_NULL_OFFSET  },
  /* 219 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMov, 0x00, 2, 172, NACL_OPCODE_NULL_OFFSET  },
  /* 220 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMov, 0x01, 2, 172, NACL_OPCODE_NULL_OFFSET  },
  /* 221 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMov, 0x02, 2, 172, NACL_OPCODE_NULL_OFFSET  },
  /* 222 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMov, 0x03, 2, 172, NACL_OPCODE_NULL_OFFSET  },
  /* 223 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMov, 0x04, 2, 172, NACL_OPCODE_NULL_OFFSET  },
  /* 224 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMov, 0x05, 2, 172, NACL_OPCODE_NULL_OFFSET  },
  /* 225 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMov, 0x06, 2, 172, NACL_OPCODE_NULL_OFFSET  },
  /* 226 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMov, 0x07, 2, 172, NACL_OPCODE_NULL_OFFSET  },
  /* 227 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstSar, 0x07, 2, 85, NACL_OPCODE_NULL_OFFSET  },
  /* 228 */
  { NACLi_ILLEGAL,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(NaClIllegal),
    InstShl, 0x06, 2, 85, 227  },
  /* 229 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstShr, 0x05, 2, 85, 228  },
  /* 230 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstShl, 0x04, 2, 85, 229  },
  /* 231 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstRcr, 0x03, 2, 85, 230  },
  /* 232 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstRcl, 0x02, 2, 85, 231  },
  /* 233 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstRor, 0x01, 2, 85, 232  },
  /* 234 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstRol, 0x00, 2, 85, 233  },
  /* 235 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstSar, 0x07, 2, 89, NACL_OPCODE_NULL_OFFSET  },
  /* 236 */
  { NACLi_ILLEGAL,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstShl, 0x06, 2, 89, 235  },
  /* 237 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstShr, 0x05, 2, 89, 236  },
  /* 238 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstShl, 0x04, 2, 89, 237  },
  /* 239 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstRcr, 0x03, 2, 89, 238  },
  /* 240 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstRcl, 0x02, 2, 89, 239  },
  /* 241 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstRor, 0x01, 2, 89, 240  },
  /* 242 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstRol, 0x00, 2, 89, 241  },
  /* 243 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_w) | NACL_IFLAG(NaClIllegal),
    InstRet, 0x00, 3, 174, NACL_OPCODE_NULL_OFFSET  },
  /* 244 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstRet, 0x00, 2, 174, NACL_OPCODE_NULL_OFFSET  },
  /* 245 */
  { NACLi_386,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstLes, 0x00, 2, 177, NACL_OPCODE_NULL_OFFSET  },
  /* 246 */
  { NACLi_386,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstLds, 0x00, 2, 177, NACL_OPCODE_NULL_OFFSET  },
  /* 247 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x00, 2, 179, 168  },
  /* 248 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMov, 0x00, 2, 181, 168  },
  /* 249 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_w) | NACL_IFLAG(OpcodeHasImmed2_b) | NACL_IFLAG(NaClIllegal),
    InstEnter, 0x00, 4, 183, NACL_OPCODE_NULL_OFFSET  },
  /* 250 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstLeave, 0x00, 2, 187, NACL_OPCODE_NULL_OFFSET  },
  /* 251 */
  { NACLi_RETURN,
    NACL_IFLAG(OpcodeHasImmed_w) | NACL_IFLAG(NaClIllegal),
    InstRet, 0x00, 3, 174, NACL_OPCODE_NULL_OFFSET  },
  /* 252 */
  { NACLi_RETURN,
    NACL_IFLAG(NaClIllegal),
    InstRet, 0x00, 2, 174, NACL_OPCODE_NULL_OFFSET  },
  /* 253 */
  { NACLi_SYSTEM,
    NACL_IFLAG(NaClIllegal),
    InstInt3, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 254 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(NaClIllegal),
    InstInt, 0x00, 1, 9, NACL_OPCODE_NULL_OFFSET  },
  /* 255 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstInto, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 256 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(NaClIllegal),
    InstIret, 0x00, 2, 174, NACL_OPCODE_NULL_OFFSET  },
  /* 257 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstIretd, 0x00, 2, 174, 256  },
  /* 258 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSar, 0x07, 2, 189, NACL_OPCODE_NULL_OFFSET  },
  /* 259 */
  { NACLi_ILLEGAL,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(NaClIllegal),
    InstShl, 0x06, 2, 189, 258  },
  /* 260 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstShr, 0x05, 2, 189, 259  },
  /* 261 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstShl, 0x04, 2, 189, 260  },
  /* 262 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstRcr, 0x03, 2, 189, 261  },
  /* 263 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstRcl, 0x02, 2, 189, 262  },
  /* 264 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstRor, 0x01, 2, 189, 263  },
  /* 265 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstRol, 0x00, 2, 189, 264  },
  /* 266 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstSar, 0x07, 2, 191, NACL_OPCODE_NULL_OFFSET  },
  /* 267 */
  { NACLi_ILLEGAL,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstShl, 0x06, 2, 191, 266  },
  /* 268 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstShr, 0x05, 2, 191, 267  },
  /* 269 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstShl, 0x04, 2, 191, 268  },
  /* 270 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstRcr, 0x03, 2, 191, 269  },
  /* 271 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstRcl, 0x02, 2, 191, 270  },
  /* 272 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstRor, 0x01, 2, 191, 271  },
  /* 273 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstRol, 0x00, 2, 191, 272  },
  /* 274 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSar, 0x07, 2, 193, NACL_OPCODE_NULL_OFFSET  },
  /* 275 */
  { NACLi_ILLEGAL,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(NaClIllegal),
    InstShl, 0x06, 2, 193, 274  },
  /* 276 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstShr, 0x05, 2, 193, 275  },
  /* 277 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstShl, 0x04, 2, 193, 276  },
  /* 278 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstRcr, 0x03, 2, 193, 277  },
  /* 279 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstRcl, 0x02, 2, 193, 278  },
  /* 280 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstRor, 0x01, 2, 193, 279  },
  /* 281 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstRol, 0x00, 2, 193, 280  },
  /* 282 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstSar, 0x07, 2, 195, NACL_OPCODE_NULL_OFFSET  },
  /* 283 */
  { NACLi_ILLEGAL,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstShl, 0x06, 2, 195, 282  },
  /* 284 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstShr, 0x05, 2, 195, 283  },
  /* 285 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstShl, 0x04, 2, 195, 284  },
  /* 286 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstRcr, 0x03, 2, 195, 285  },
  /* 287 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstRcl, 0x02, 2, 195, 286  },
  /* 288 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstRor, 0x01, 2, 195, 287  },
  /* 289 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstRol, 0x00, 2, 195, 288  },
  /* 290 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(NaClIllegal),
    InstAam, 0x00, 2, 197, NACL_OPCODE_NULL_OFFSET  },
  /* 291 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(NaClIllegal),
    InstAad, 0x00, 2, 197, NACL_OPCODE_NULL_OFFSET  },
  /* 292 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstSalc, 0x00, 1, 199, NACL_OPCODE_NULL_OFFSET  },
  /* 293 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstXlat, 0x00, 2, 200, NACL_OPCODE_NULL_OFFSET  },
  /* 294 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFdivr, 0x07, 2, 202, NACL_OPCODE_NULL_OFFSET  },
  /* 295 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFdiv, 0x06, 2, 202, 294  },
  /* 296 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFsubr, 0x05, 2, 202, 295  },
  /* 297 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFsub, 0x04, 2, 202, 296  },
  /* 298 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFcomp, 0x03, 2, 204, 297  },
  /* 299 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFcom, 0x02, 2, 204, 298  },
  /* 300 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFmul, 0x01, 2, 202, 299  },
  /* 301 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFadd, 0x00, 2, 202, 300  },
  /* 302 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFnstcw, 0x07, 1, 206, NACL_OPCODE_NULL_OFFSET  },
  /* 303 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16),
    InstFnstenv, 0x06, 1, 207, 302  },
  /* 304 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFldcw, 0x05, 1, 208, 303  },
  /* 305 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16),
    InstFldenv, 0x04, 1, 209, 304  },
  /* 306 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFstp, 0x03, 2, 210, 305  },
  /* 307 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFst, 0x02, 2, 210, 306  },
  /* 308 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x01, 0, 0, 307  },
  /* 309 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFld, 0x00, 2, 212, 308  },
  /* 310 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFidivr, 0x07, 2, 202, NACL_OPCODE_NULL_OFFSET  },
  /* 311 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFidiv, 0x06, 2, 202, 310  },
  /* 312 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFisubr, 0x05, 2, 202, 311  },
  /* 313 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFisub, 0x04, 2, 202, 312  },
  /* 314 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFicomp, 0x03, 2, 202, 313  },
  /* 315 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFicom, 0x02, 2, 202, 314  },
  /* 316 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFimul, 0x01, 2, 202, 315  },
  /* 317 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFiadd, 0x00, 2, 202, 316  },
  /* 318 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16),
    InstFstp, 0x07, 2, 214, NACL_OPCODE_NULL_OFFSET  },
  /* 319 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x06, 0, 0, 318  },
  /* 320 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16),
    InstFld, 0x05, 2, 216, 319  },
  /* 321 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x04, 0, 0, 320  },
  /* 322 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFistp, 0x03, 2, 210, 321  },
  /* 323 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFist, 0x02, 2, 210, 322  },
  /* 324 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFisttp, 0x01, 2, 210, 323  },
  /* 325 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFild, 0x00, 2, 212, 324  },
  /* 326 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFdivr, 0x07, 2, 218, NACL_OPCODE_NULL_OFFSET  },
  /* 327 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFdiv, 0x06, 2, 218, 326  },
  /* 328 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFsubr, 0x05, 2, 218, 327  },
  /* 329 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFsub, 0x04, 2, 218, 328  },
  /* 330 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFcomp, 0x03, 2, 220, 329  },
  /* 331 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFcom, 0x02, 2, 220, 330  },
  /* 332 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFmul, 0x01, 2, 218, 331  },
  /* 333 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFadd, 0x00, 2, 218, 332  },
  /* 334 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFnstsw, 0x07, 1, 206, NACL_OPCODE_NULL_OFFSET  },
  /* 335 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16),
    InstFnsave, 0x06, 1, 207, 334  },
  /* 336 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x05, 0, 0, 335  },
  /* 337 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16),
    InstFrstor, 0x04, 1, 209, 336  },
  /* 338 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFstp, 0x03, 2, 222, 337  },
  /* 339 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFst, 0x02, 2, 222, 338  },
  /* 340 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFisttp, 0x01, 2, 222, 339  },
  /* 341 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFld, 0x00, 2, 224, 340  },
  /* 342 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFidivr, 0x07, 2, 226, NACL_OPCODE_NULL_OFFSET  },
  /* 343 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFidiv, 0x06, 2, 226, 342  },
  /* 344 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFisubr, 0x05, 2, 226, 343  },
  /* 345 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFisub, 0x04, 2, 226, 344  },
  /* 346 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFicomp, 0x03, 2, 228, 345  },
  /* 347 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFicom, 0x02, 2, 228, 346  },
  /* 348 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFimul, 0x01, 2, 226, 347  },
  /* 349 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFiadd, 0x00, 2, 226, 348  },
  /* 350 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16),
    InstFistp, 0x07, 2, 214, NACL_OPCODE_NULL_OFFSET  },
  /* 351 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16),
    InstFbstp, 0x06, 2, 214, 350  },
  /* 352 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16),
    InstFild, 0x05, 2, 216, 351  },
  /* 353 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16),
    InstFbld, 0x04, 2, 216, 352  },
  /* 354 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFistp, 0x03, 2, 230, 353  },
  /* 355 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFist, 0x02, 2, 230, 354  },
  /* 356 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFisttp, 0x01, 2, 230, 355  },
  /* 357 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFild, 0x00, 2, 232, 356  },
  /* 358 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump),
    InstLoopne, 0x00, 2, 81, NACL_OPCODE_NULL_OFFSET  },
  /* 359 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump),
    InstLoope, 0x00, 2, 81, NACL_OPCODE_NULL_OFFSET  },
  /* 360 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(JumpInstruction),
    InstLoop, 0x00, 2, 81, NACL_OPCODE_NULL_OFFSET  },
  /* 361 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(AddressSize_v) | NACL_IFLAG(ConditionalJump),
    InstJecxz, 0x00, 3, 234, NACL_OPCODE_NULL_OFFSET  },
  /* 362 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(AddressSize_w) | NACL_IFLAG(ConditionalJump),
    InstJcxz, 0x00, 3, 237, 361  },
  /* 363 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(NaClIllegal),
    InstIn, 0x00, 2, 240, NACL_OPCODE_NULL_OFFSET  },
  /* 364 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstIn, 0x00, 2, 242, NACL_OPCODE_NULL_OFFSET  },
  /* 365 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(NaClIllegal),
    InstOut, 0x00, 2, 244, NACL_OPCODE_NULL_OFFSET  },
  /* 366 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstOut, 0x00, 2, 246, NACL_OPCODE_NULL_OFFSET  },
  /* 367 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(JumpInstruction),
    InstCall, 0x00, 3, 248, NACL_OPCODE_NULL_OFFSET  },
  /* 368 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(JumpInstruction),
    InstJmp, 0x00, 2, 251, NACL_OPCODE_NULL_OFFSET  },
  /* 369 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_p) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(JumpInstruction),
    InstJmp, 0x00, 2, 253, NACL_OPCODE_NULL_OFFSET  },
  /* 370 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(JumpInstruction),
    InstJmp, 0x00, 2, 81, NACL_OPCODE_NULL_OFFSET  },
  /* 371 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstIn, 0x00, 2, 255, NACL_OPCODE_NULL_OFFSET  },
  /* 372 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstIn, 0x00, 2, 257, NACL_OPCODE_NULL_OFFSET  },
  /* 373 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstOut, 0x00, 2, 259, NACL_OPCODE_NULL_OFFSET  },
  /* 374 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstOut, 0x00, 2, 261, NACL_OPCODE_NULL_OFFSET  },
  /* 375 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstInt1, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 376 */
  { NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstHlt, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 377 */
  { NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstCmc, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 378 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstIdiv, 0x07, 3, 263, NACL_OPCODE_NULL_OFFSET  },
  /* 379 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstDiv, 0x06, 3, 263, 378  },
  /* 380 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstImul, 0x05, 3, 263, 379  },
  /* 381 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstMul, 0x04, 3, 263, 380  },
  /* 382 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstNeg, 0x03, 1, 0, 381  },
  /* 383 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstNot, 0x02, 1, 0, 382  },
  /* 384 */
  { NACLi_ILLEGAL,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(NaClIllegal),
    InstTest, 0x01, 2, 83, 383  },
  /* 385 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstTest, 0x00, 2, 83, 384  },
  /* 386 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstIdiv, 0x07, 3, 266, NACL_OPCODE_NULL_OFFSET  },
  /* 387 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstDiv, 0x06, 3, 266, 386  },
  /* 388 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstImul, 0x05, 3, 266, 387  },
  /* 389 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMul, 0x04, 3, 266, 388  },
  /* 390 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstNeg, 0x03, 1, 2, 389  },
  /* 391 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstNot, 0x02, 1, 2, 390  },
  /* 392 */
  { NACLi_ILLEGAL,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstTest, 0x01, 2, 62, 391  },
  /* 393 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstTest, 0x00, 2, 62, 392  },
  /* 394 */
  { NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstClc, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 395 */
  { NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstStc, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 396 */
  { NACLi_SYSTEM,
    NACL_IFLAG(NaClIllegal),
    InstCli, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 397 */
  { NACLi_SYSTEM,
    NACL_IFLAG(NaClIllegal),
    InstSti, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 398 */
  { NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstCld, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 399 */
  { NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstStd, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 400 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x07, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 401 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x06, 0, 0, 400  },
  /* 402 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x05, 0, 0, 401  },
  /* 403 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x04, 0, 0, 402  },
  /* 404 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x03, 0, 0, 403  },
  /* 405 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x02, 0, 0, 404  },
  /* 406 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstDec, 0x01, 1, 0, 405  },
  /* 407 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstInc, 0x00, 1, 0, 406  },
  /* 408 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstPush, 0x06, 2, 269, 400  },
  /* 409 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(JumpInstruction),
    InstJmp, 0x05, 2, 271, 408  },
  /* 410 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(JumpInstruction),
    InstJmp, 0x04, 2, 273, 409  },
  /* 411 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(JumpInstruction),
    InstCall, 0x03, 3, 275, 410  },
  /* 412 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(JumpInstruction),
    InstCall, 0x02, 3, 278, 411  },
  /* 413 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstDec, 0x01, 1, 2, 412  },
  /* 414 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstInc, 0x00, 1, 2, 413  },
  /* 415 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstVerw, 0x05, 1, 281, 401  },
  /* 416 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstVerr, 0x04, 1, 281, 415  },
  /* 417 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstLtr, 0x03, 1, 106, 416  },
  /* 418 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstLldt, 0x02, 1, 106, 417  },
  /* 419 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstStr, 0x01, 1, 101, 418  },
  /* 420 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstSldt, 0x00, 1, 101, 419  },
  /* 421 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvlpg, 0x07, 1, 282, 400  },
  /* 422 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstLmsw, 0x06, 1, 106, 421  },
  /* 423 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x05, 0, 0, 422  },
  /* 424 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstSmsw, 0x04, 1, 101, 423  },
  /* 425 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x03, 0, 0, 424  },
  /* 426 */
  { NACLi_SVM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvlpga, 0x73, 2, 283, 425  },
  /* 427 */
  { NACLi_SVM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstSkinit, 0x63, 2, 285, 426  },
  /* 428 */
  { NACLi_SVM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstClgi, 0x53, 0, 0, 427  },
  /* 429 */
  { NACLi_SVM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstStgi, 0x43, 0, 0, 428  },
  /* 430 */
  { NACLi_SVM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstVmsave, 0x33, 1, 283, 429  },
  /* 431 */
  { NACLi_SVM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstVmload, 0x23, 1, 283, 430  },
  /* 432 */
  { NACLi_SVM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstVmmcall, 0x13, 0, 0, 431  },
  /* 433 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstLidt, 0x03, 1, 287, 432  },
  /* 434 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstLgdt, 0x02, 1, 287, 433  },
  /* 435 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x01, 0, 0, 434  },
  /* 436 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstMwait, 0x11, 2, 288, 435  },
  /* 437 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstMonitor, 0x01, 3, 290, 436  },
  /* 438 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstSidt, 0x01, 1, 293, 437  },
  /* 439 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstSgdt, 0x00, 1, 293, 438  },
  /* 440 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstLar, 0x00, 2, 294, NACL_OPCODE_NULL_OFFSET  },
  /* 441 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstLsl, 0x00, 2, 294, NACL_OPCODE_NULL_OFFSET  },
  /* 442 */
  { NACLi_SYSTEM,
    NACL_IFLAG(NaClIllegal),
    InstClts, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 443 */
  { NACLi_SYSTEM,
    NACL_IFLAG(NaClIllegal),
    InstInvd, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 444 */
  { NACLi_SYSTEM,
    NACL_IFLAG(NaClIllegal),
    InstWbinvd, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 445 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstUd2, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 446 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstPrefetch_reserved, 0x07, 1, 296, NACL_OPCODE_NULL_OFFSET  },
  /* 447 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstPrefetch_reserved, 0x06, 1, 296, 446  },
  /* 448 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstPrefetch_reserved, 0x05, 1, 296, 447  },
  /* 449 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstPrefetch_reserved, 0x04, 1, 296, 448  },
  /* 450 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstPrefetch_modified, 0x03, 1, 296, 449  },
  /* 451 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstPrefetch_reserved, 0x02, 1, 296, 450  },
  /* 452 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstPrefetch_modified, 0x01, 1, 296, 451  },
  /* 453 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstPrefetch_exclusive, 0x00, 1, 296, 452  },
  /* 454 */
  { NACLi_3DNOW,
    NACL_EMPTY_IFLAGS,
    InstFemms, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 455 */
  { NACLi_INVALID,
    NACL_IFLAG(Opcode0F0F) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00, 3, 297, NACL_OPCODE_NULL_OFFSET  },
  /* 456 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstMovups, 0x00, 2, 300, NACL_OPCODE_NULL_OFFSET  },
  /* 457 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstMovups, 0x00, 2, 302, NACL_OPCODE_NULL_OFFSET  },
  /* 458 */
  { NACLi_SSE,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMovhlps, 0x00, 2, 304, NACL_OPCODE_NULL_OFFSET  },
  /* 459 */
  { NACLi_SSE,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMovlps, 0x00, 2, 306, 458  },
  /* 460 */
  { NACLi_SSE,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMovlps, 0x00, 2, 308, NACL_OPCODE_NULL_OFFSET  },
  /* 461 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstUnpcklps, 0x00, 2, 310, NACL_OPCODE_NULL_OFFSET  },
  /* 462 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstUnpckhps, 0x00, 2, 310, NACL_OPCODE_NULL_OFFSET  },
  /* 463 */
  { NACLi_SSE,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMovlhps, 0x00, 2, 304, NACL_OPCODE_NULL_OFFSET  },
  /* 464 */
  { NACLi_SSE,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMovhps, 0x00, 2, 306, 463  },
  /* 465 */
  { NACLi_SSE,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMovhps, 0x00, 2, 308, NACL_OPCODE_NULL_OFFSET  },
  /* 466 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm),
    InstNop, 0x07, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 467 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm),
    InstNop, 0x06, 0, 0, 466  },
  /* 468 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm),
    InstNop, 0x05, 0, 0, 467  },
  /* 469 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm),
    InstNop, 0x04, 0, 0, 468  },
  /* 470 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstPrefetcht2, 0x03, 1, 296, 469  },
  /* 471 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstPrefetcht1, 0x02, 1, 296, 470  },
  /* 472 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstPrefetcht0, 0x01, 1, 296, 471  },
  /* 473 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstPrefetchnta, 0x00, 1, 296, 472  },
  /* 474 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstNop, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 475 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm),
    InstNop, 0x00, 0, 0, 474  },
  /* 476 */
  { NACLi_SYSTEM,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstMov, 0x00, 2, 312, NACL_OPCODE_NULL_OFFSET  },
  /* 477 */
  { NACLi_SYSTEM,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstMov, 0x00, 2, 314, NACL_OPCODE_NULL_OFFSET  },
  /* 478 */
  { NACLi_SYSTEM,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstMov, 0x00, 2, 316, NACL_OPCODE_NULL_OFFSET  },
  /* 479 */
  { NACLi_SYSTEM,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstMov, 0x00, 2, 318, NACL_OPCODE_NULL_OFFSET  },
  /* 480 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstMovaps, 0x00, 2, 300, NACL_OPCODE_NULL_OFFSET  },
  /* 481 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstMovaps, 0x00, 2, 302, NACL_OPCODE_NULL_OFFSET  },
  /* 482 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstCvtpi2ps, 0x00, 2, 320, NACL_OPCODE_NULL_OFFSET  },
  /* 483 */
  { NACLi_SSE,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMovntps, 0x00, 2, 322, NACL_OPCODE_NULL_OFFSET  },
  /* 484 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstCvttps2pi, 0x00, 2, 324, NACL_OPCODE_NULL_OFFSET  },
  /* 485 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstCvtps2pi, 0x00, 2, 324, NACL_OPCODE_NULL_OFFSET  },
  /* 486 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstUcomiss, 0x00, 2, 326, NACL_OPCODE_NULL_OFFSET  },
  /* 487 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstComiss, 0x00, 2, 328, NACL_OPCODE_NULL_OFFSET  },
  /* 488 */
  { NACLi_RDMSR,
    NACL_IFLAG(NaClIllegal),
    InstWrmsr, 0x00, 3, 330, NACL_OPCODE_NULL_OFFSET  },
  /* 489 */
  { NACLi_RDTSC,
    NACL_EMPTY_IFLAGS,
    InstRdtsc, 0x00, 2, 333, NACL_OPCODE_NULL_OFFSET  },
  /* 490 */
  { NACLi_RDMSR,
    NACL_IFLAG(NaClIllegal),
    InstRdmsr, 0x00, 3, 335, NACL_OPCODE_NULL_OFFSET  },
  /* 491 */
  { NACLi_SYSTEM,
    NACL_IFLAG(NaClIllegal),
    InstRdpmc, 0x00, 3, 335, NACL_OPCODE_NULL_OFFSET  },
  /* 492 */
  { NACLi_SYSENTER,
    NACL_IFLAG(NaClIllegal),
    InstSysenter, 0x00, 4, 338, NACL_OPCODE_NULL_OFFSET  },
  /* 493 */
  { NACLi_SYSENTER,
    NACL_IFLAG(NaClIllegal),
    InstSysexit, 0x00, 6, 342, NACL_OPCODE_NULL_OFFSET  },
  /* 494 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstCmovo, 0x00, 2, 61, NACL_OPCODE_NULL_OFFSET  },
  /* 495 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstCmovno, 0x00, 2, 61, NACL_OPCODE_NULL_OFFSET  },
  /* 496 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstCmovb, 0x00, 2, 61, NACL_OPCODE_NULL_OFFSET  },
  /* 497 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstCmovnb, 0x00, 2, 61, NACL_OPCODE_NULL_OFFSET  },
  /* 498 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstCmovz, 0x00, 2, 61, NACL_OPCODE_NULL_OFFSET  },
  /* 499 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstCmovnz, 0x00, 2, 61, NACL_OPCODE_NULL_OFFSET  },
  /* 500 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstCmovbe, 0x00, 2, 61, NACL_OPCODE_NULL_OFFSET  },
  /* 501 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstCmovnbe, 0x00, 2, 61, NACL_OPCODE_NULL_OFFSET  },
  /* 502 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstCmovs, 0x00, 2, 61, NACL_OPCODE_NULL_OFFSET  },
  /* 503 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstCmovns, 0x00, 2, 61, NACL_OPCODE_NULL_OFFSET  },
  /* 504 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstCmovp, 0x00, 2, 61, NACL_OPCODE_NULL_OFFSET  },
  /* 505 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstCmovnp, 0x00, 2, 61, NACL_OPCODE_NULL_OFFSET  },
  /* 506 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstCmovl, 0x00, 2, 61, NACL_OPCODE_NULL_OFFSET  },
  /* 507 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstCmovnl, 0x00, 2, 61, NACL_OPCODE_NULL_OFFSET  },
  /* 508 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstCmovle, 0x00, 2, 61, NACL_OPCODE_NULL_OFFSET  },
  /* 509 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstCmovnle, 0x00, 2, 61, NACL_OPCODE_NULL_OFFSET  },
  /* 510 */
  { NACLi_SSE,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMovmskps, 0x00, 2, 348, NACL_OPCODE_NULL_OFFSET  },
  /* 511 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstSqrtps, 0x00, 2, 300, NACL_OPCODE_NULL_OFFSET  },
  /* 512 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstRsqrtps, 0x00, 2, 300, NACL_OPCODE_NULL_OFFSET  },
  /* 513 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstRcpps, 0x00, 2, 300, NACL_OPCODE_NULL_OFFSET  },
  /* 514 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstAndps, 0x00, 2, 350, NACL_OPCODE_NULL_OFFSET  },
  /* 515 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstAndnps, 0x00, 2, 350, NACL_OPCODE_NULL_OFFSET  },
  /* 516 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstOrps, 0x00, 2, 350, NACL_OPCODE_NULL_OFFSET  },
  /* 517 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstXorps, 0x00, 2, 350, NACL_OPCODE_NULL_OFFSET  },
  /* 518 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstAddps, 0x00, 2, 350, NACL_OPCODE_NULL_OFFSET  },
  /* 519 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstMulps, 0x00, 2, 350, NACL_OPCODE_NULL_OFFSET  },
  /* 520 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm),
    InstCvtps2pd, 0x00, 2, 352, NACL_OPCODE_NULL_OFFSET  },
  /* 521 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm),
    InstCvtdq2ps, 0x00, 2, 354, NACL_OPCODE_NULL_OFFSET  },
  /* 522 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstSubps, 0x00, 2, 350, NACL_OPCODE_NULL_OFFSET  },
  /* 523 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstMinps, 0x00, 2, 350, NACL_OPCODE_NULL_OFFSET  },
  /* 524 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstDivps, 0x00, 2, 350, NACL_OPCODE_NULL_OFFSET  },
  /* 525 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstMaxps, 0x00, 2, 350, NACL_OPCODE_NULL_OFFSET  },
  /* 526 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPunpcklbw, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 527 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPunpcklwd, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 528 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPunpckldq, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 529 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPacksswb, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 530 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPcmpgtb, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 531 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPcmpgtw, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 532 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPcmpgtd, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 533 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPackuswb, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 534 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPunpckhbw, 0x00, 2, 358, NACL_OPCODE_NULL_OFFSET  },
  /* 535 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPunpckhwd, 0x00, 2, 358, NACL_OPCODE_NULL_OFFSET  },
  /* 536 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPunpckhdq, 0x00, 2, 358, NACL_OPCODE_NULL_OFFSET  },
  /* 537 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPackssdw, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 538 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstMovd, 0x00, 2, 360, NACL_OPCODE_NULL_OFFSET  },
  /* 539 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstMovq, 0x00, 2, 362, NACL_OPCODE_NULL_OFFSET  },
  /* 540 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstPshufw, 0x00, 3, 364, NACL_OPCODE_NULL_OFFSET  },
  /* 541 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstPsllw, 0x06, 2, 367, 400  },
  /* 542 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x05, 0, 0, 541  },
  /* 543 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstPsraw, 0x04, 2, 367, 542  },
  /* 544 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x03, 0, 0, 543  },
  /* 545 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstPsrlw, 0x02, 2, 367, 544  },
  /* 546 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x01, 0, 0, 545  },
  /* 547 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00, 0, 0, 546  },
  /* 548 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstPslld, 0x06, 2, 367, 400  },
  /* 549 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x05, 0, 0, 548  },
  /* 550 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstPsrad, 0x04, 2, 367, 549  },
  /* 551 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x03, 0, 0, 550  },
  /* 552 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstPsrld, 0x02, 2, 367, 551  },
  /* 553 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x01, 0, 0, 552  },
  /* 554 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00, 0, 0, 553  },
  /* 555 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstPsllq, 0x06, 2, 367, 400  },
  /* 556 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x05, 0, 0, 555  },
  /* 557 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x04, 0, 0, 556  },
  /* 558 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x03, 0, 0, 557  },
  /* 559 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstPsrlq, 0x02, 2, 367, 558  },
  /* 560 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x01, 0, 0, 559  },
  /* 561 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00, 0, 0, 560  },
  /* 562 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPcmpeqb, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 563 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPcmpeqw, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 564 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPcmpeqd, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 565 */
  { NACLi_MMX,
    NACL_EMPTY_IFLAGS,
    InstEmms, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 566 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstMovd, 0x00, 2, 369, NACL_OPCODE_NULL_OFFSET  },
  /* 567 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstMovq, 0x00, 2, 371, NACL_OPCODE_NULL_OFFSET  },
  /* 568 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(ConditionalJump),
    InstJo, 0x00, 2, 251, NACL_OPCODE_NULL_OFFSET  },
  /* 569 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(ConditionalJump),
    InstJno, 0x00, 2, 251, NACL_OPCODE_NULL_OFFSET  },
  /* 570 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(ConditionalJump),
    InstJb, 0x00, 2, 251, NACL_OPCODE_NULL_OFFSET  },
  /* 571 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(ConditionalJump),
    InstJnb, 0x00, 2, 251, NACL_OPCODE_NULL_OFFSET  },
  /* 572 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(ConditionalJump),
    InstJz, 0x00, 2, 251, NACL_OPCODE_NULL_OFFSET  },
  /* 573 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(ConditionalJump),
    InstJnz, 0x00, 2, 251, NACL_OPCODE_NULL_OFFSET  },
  /* 574 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(ConditionalJump),
    InstJbe, 0x00, 2, 251, NACL_OPCODE_NULL_OFFSET  },
  /* 575 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(ConditionalJump),
    InstJnbe, 0x00, 2, 251, NACL_OPCODE_NULL_OFFSET  },
  /* 576 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(ConditionalJump),
    InstJs, 0x00, 2, 251, NACL_OPCODE_NULL_OFFSET  },
  /* 577 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(ConditionalJump),
    InstJns, 0x00, 2, 251, NACL_OPCODE_NULL_OFFSET  },
  /* 578 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(ConditionalJump),
    InstJp, 0x00, 2, 251, NACL_OPCODE_NULL_OFFSET  },
  /* 579 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(ConditionalJump),
    InstJnp, 0x00, 2, 251, NACL_OPCODE_NULL_OFFSET  },
  /* 580 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(ConditionalJump),
    InstJl, 0x00, 2, 251, NACL_OPCODE_NULL_OFFSET  },
  /* 581 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(ConditionalJump),
    InstJnl, 0x00, 2, 251, NACL_OPCODE_NULL_OFFSET  },
  /* 582 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(ConditionalJump),
    InstJle, 0x00, 2, 251, NACL_OPCODE_NULL_OFFSET  },
  /* 583 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(ConditionalJump),
    InstJnle, 0x00, 2, 251, NACL_OPCODE_NULL_OFFSET  },
  /* 584 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSeto, 0x00, 1, 95, NACL_OPCODE_NULL_OFFSET  },
  /* 585 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetno, 0x00, 1, 95, NACL_OPCODE_NULL_OFFSET  },
  /* 586 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetb, 0x00, 1, 95, NACL_OPCODE_NULL_OFFSET  },
  /* 587 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetnb, 0x00, 1, 95, NACL_OPCODE_NULL_OFFSET  },
  /* 588 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetz, 0x00, 1, 95, NACL_OPCODE_NULL_OFFSET  },
  /* 589 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetnz, 0x00, 1, 95, NACL_OPCODE_NULL_OFFSET  },
  /* 590 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetbe, 0x00, 1, 95, NACL_OPCODE_NULL_OFFSET  },
  /* 591 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetnbe, 0x00, 1, 95, NACL_OPCODE_NULL_OFFSET  },
  /* 592 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSets, 0x00, 1, 95, NACL_OPCODE_NULL_OFFSET  },
  /* 593 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetns, 0x00, 1, 95, NACL_OPCODE_NULL_OFFSET  },
  /* 594 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetp, 0x00, 1, 95, NACL_OPCODE_NULL_OFFSET  },
  /* 595 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetnp, 0x00, 1, 95, NACL_OPCODE_NULL_OFFSET  },
  /* 596 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetl, 0x00, 1, 95, NACL_OPCODE_NULL_OFFSET  },
  /* 597 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetnl, 0x00, 1, 95, NACL_OPCODE_NULL_OFFSET  },
  /* 598 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetle, 0x00, 1, 95, NACL_OPCODE_NULL_OFFSET  },
  /* 599 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetnle, 0x00, 1, 95, NACL_OPCODE_NULL_OFFSET  },
  /* 600 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstPush, 0x00, 2, 373, NACL_OPCODE_NULL_OFFSET  },
  /* 601 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstPop, 0x00, 2, 375, NACL_OPCODE_NULL_OFFSET  },
  /* 602 */
  { NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstCpuid, 0x00, 4, 377, NACL_OPCODE_NULL_OFFSET  },
  /* 603 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstBt, 0x00, 2, 29, NACL_OPCODE_NULL_OFFSET  },
  /* 604 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstShld, 0x00, 3, 381, NACL_OPCODE_NULL_OFFSET  },
  /* 605 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstShld, 0x00, 3, 384, NACL_OPCODE_NULL_OFFSET  },
  /* 606 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstPush, 0x00, 2, 387, NACL_OPCODE_NULL_OFFSET  },
  /* 607 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstPop, 0x00, 2, 389, NACL_OPCODE_NULL_OFFSET  },
  /* 608 */
  { NACLi_SYSTEM,
    NACL_IFLAG(NaClIllegal),
    InstRsm, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 609 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstBts, 0x00, 2, 2, NACL_OPCODE_NULL_OFFSET  },
  /* 610 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstShrd, 0x00, 3, 391, NACL_OPCODE_NULL_OFFSET  },
  /* 611 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstShrd, 0x00, 3, 394, NACL_OPCODE_NULL_OFFSET  },
  /* 612 */
  { NACLi_SFENCE_CLFLUSH,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstClflush, 0x07, 1, 282, NACL_OPCODE_NULL_OFFSET  },
  /* 613 */
  { NACLi_SFENCE_CLFLUSH,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstSfence, 0x77, 0, 0, 612  },
  /* 614 */
  { NACLi_SFENCE_CLFLUSH,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstSfence, 0x67, 0, 0, 613  },
  /* 615 */
  { NACLi_SFENCE_CLFLUSH,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstSfence, 0x57, 0, 0, 614  },
  /* 616 */
  { NACLi_SFENCE_CLFLUSH,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstSfence, 0x47, 0, 0, 615  },
  /* 617 */
  { NACLi_SFENCE_CLFLUSH,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstSfence, 0x37, 0, 0, 616  },
  /* 618 */
  { NACLi_SFENCE_CLFLUSH,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstSfence, 0x27, 0, 0, 617  },
  /* 619 */
  { NACLi_SFENCE_CLFLUSH,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstSfence, 0x17, 0, 0, 618  },
  /* 620 */
  { NACLi_SFENCE_CLFLUSH,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstSfence, 0x07, 0, 0, 619  },
  /* 621 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMfence, 0x76, 0, 0, 620  },
  /* 622 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMfence, 0x66, 0, 0, 621  },
  /* 623 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMfence, 0x56, 0, 0, 622  },
  /* 624 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMfence, 0x46, 0, 0, 623  },
  /* 625 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMfence, 0x36, 0, 0, 624  },
  /* 626 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMfence, 0x26, 0, 0, 625  },
  /* 627 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMfence, 0x16, 0, 0, 626  },
  /* 628 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMfence, 0x06, 0, 0, 627  },
  /* 629 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstLfence, 0x75, 0, 0, 628  },
  /* 630 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstLfence, 0x65, 0, 0, 629  },
  /* 631 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstLfence, 0x55, 0, 0, 630  },
  /* 632 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstLfence, 0x45, 0, 0, 631  },
  /* 633 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstLfence, 0x35, 0, 0, 632  },
  /* 634 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstLfence, 0x25, 0, 0, 633  },
  /* 635 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstLfence, 0x15, 0, 0, 634  },
  /* 636 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstLfence, 0x05, 0, 0, 635  },
  /* 637 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x04, 0, 0, 636  },
  /* 638 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstStmxcsr, 0x03, 1, 210, 637  },
  /* 639 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstLdmxcsr, 0x02, 1, 203, 638  },
  /* 640 */
  { NACLi_FXSAVE,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(NaClIllegal),
    InstFxrstor, 0x01, 1, 209, 639  },
  /* 641 */
  { NACLi_FXSAVE,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(NaClIllegal),
    InstFxsave, 0x00, 1, 207, 640  },
  /* 642 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstImul, 0x00, 2, 6, NACL_OPCODE_NULL_OFFSET  },
  /* 643 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstCmpxchg, 0x00, 3, 397, NACL_OPCODE_NULL_OFFSET  },
  /* 644 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstCmpxchg, 0x00, 3, 400, NACL_OPCODE_NULL_OFFSET  },
  /* 645 */
  { NACLi_386,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstLss, 0x00, 2, 177, NACL_OPCODE_NULL_OFFSET  },
  /* 646 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstBtr, 0x00, 2, 2, NACL_OPCODE_NULL_OFFSET  },
  /* 647 */
  { NACLi_386,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstLfs, 0x00, 2, 177, NACL_OPCODE_NULL_OFFSET  },
  /* 648 */
  { NACLi_386,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstLgs, 0x00, 2, 177, NACL_OPCODE_NULL_OFFSET  },
  /* 649 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMovzx, 0x00, 2, 403, NACL_OPCODE_NULL_OFFSET  },
  /* 650 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMovzx, 0x00, 2, 405, NACL_OPCODE_NULL_OFFSET  },
  /* 651 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstBtc, 0x07, 2, 89, 168  },
  /* 652 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstBtr, 0x06, 2, 89, 651  },
  /* 653 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstBts, 0x05, 2, 89, 652  },
  /* 654 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstBt, 0x04, 2, 67, 653  },
  /* 655 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstBtc, 0x00, 2, 2, NACL_OPCODE_NULL_OFFSET  },
  /* 656 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstBsf, 0x00, 2, 61, NACL_OPCODE_NULL_OFFSET  },
  /* 657 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstBsr, 0x00, 2, 61, NACL_OPCODE_NULL_OFFSET  },
  /* 658 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMovsx, 0x00, 2, 403, NACL_OPCODE_NULL_OFFSET  },
  /* 659 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMovsx, 0x00, 2, 405, NACL_OPCODE_NULL_OFFSET  },
  /* 660 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstXadd, 0x00, 2, 91, NACL_OPCODE_NULL_OFFSET  },
  /* 661 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstXadd, 0x00, 2, 93, NACL_OPCODE_NULL_OFFSET  },
  /* 662 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstCmpps, 0x00, 3, 407, NACL_OPCODE_NULL_OFFSET  },
  /* 663 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstMovnti, 0x00, 2, 410, NACL_OPCODE_NULL_OFFSET  },
  /* 664 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstPinsrw, 0x00, 3, 412, NACL_OPCODE_NULL_OFFSET  },
  /* 665 */
  { NACLi_SSE41,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstPextrw, 0x00, 3, 415, NACL_OPCODE_NULL_OFFSET  },
  /* 666 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstShufps, 0x00, 3, 407, NACL_OPCODE_NULL_OFFSET  },
  /* 667 */
  { NACLi_CMPXCHG8B,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_v),
    InstCmpxchg8b, 0x01, 3, 418, 168  },
  /* 668 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR),
    InstBswap, 0x00, 1, 421, NACL_OPCODE_NULL_OFFSET  },
  /* 669 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_v),
    InstBswap, 0x00, 1, 422, 668  },
  /* 670 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR),
    InstBswap, 0x01, 1, 421, NACL_OPCODE_NULL_OFFSET  },
  /* 671 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_v),
    InstBswap, 0x01, 1, 422, 670  },
  /* 672 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR),
    InstBswap, 0x02, 1, 421, NACL_OPCODE_NULL_OFFSET  },
  /* 673 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_v),
    InstBswap, 0x02, 1, 422, 672  },
  /* 674 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR),
    InstBswap, 0x03, 1, 421, NACL_OPCODE_NULL_OFFSET  },
  /* 675 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_v),
    InstBswap, 0x03, 1, 422, 674  },
  /* 676 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR),
    InstBswap, 0x04, 1, 421, NACL_OPCODE_NULL_OFFSET  },
  /* 677 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_v),
    InstBswap, 0x04, 1, 422, 676  },
  /* 678 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR),
    InstBswap, 0x05, 1, 421, NACL_OPCODE_NULL_OFFSET  },
  /* 679 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_v),
    InstBswap, 0x05, 1, 422, 678  },
  /* 680 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR),
    InstBswap, 0x06, 1, 421, NACL_OPCODE_NULL_OFFSET  },
  /* 681 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_v),
    InstBswap, 0x06, 1, 422, 680  },
  /* 682 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR),
    InstBswap, 0x07, 1, 421, NACL_OPCODE_NULL_OFFSET  },
  /* 683 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_v),
    InstBswap, 0x07, 1, 422, 682  },
  /* 684 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsrlw, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 685 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsrld, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 686 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsrlq, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 687 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPaddq, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 688 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPmullw, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 689 */
  { NACLi_MMX,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstPmovmskb, 0x00, 2, 415, NACL_OPCODE_NULL_OFFSET  },
  /* 690 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsubusb, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 691 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsubusw, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 692 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPminub, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 693 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPand, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 694 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPaddusb, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 695 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPaddusw, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 696 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPmaxub, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 697 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPandn, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 698 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPavgb, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 699 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsraw, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 700 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsrad, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 701 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPavgw, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 702 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPmulhuw, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 703 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPmulhw, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 704 */
  { NACLi_MMX,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMovntq, 0x00, 2, 423, NACL_OPCODE_NULL_OFFSET  },
  /* 705 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsubsb, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 706 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsubsw, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 707 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPminsw, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 708 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPor, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 709 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPaddsb, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 710 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPaddsw, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 711 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPmaxsw, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 712 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPxor, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 713 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsllw, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 714 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPslld, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 715 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsllq, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 716 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPmuludq, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 717 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPmaddwd, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 718 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsadbw, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 719 */
  { NACLi_MMX,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_v),
    InstMaskmovq, 0x00, 3, 425, NACL_OPCODE_NULL_OFFSET  },
  /* 720 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsubb, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 721 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsubw, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 722 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsubd, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 723 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsubq, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 724 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPaddb, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 725 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPaddw, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 726 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPaddd, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 727 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstMovsd, 0x00, 2, 428, NACL_OPCODE_NULL_OFFSET  },
  /* 728 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstMovsd, 0x00, 2, 430, NACL_OPCODE_NULL_OFFSET  },
  /* 729 */
  { NACLi_SSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstMovddup, 0x00, 2, 432, NACL_OPCODE_NULL_OFFSET  },
  /* 730 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 731 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstCvtsi2sd, 0x00, 2, 434, NACL_OPCODE_NULL_OFFSET  },
  /* 732 */
  { NACLi_SSE4A,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstMovntsd, 0x00, 2, 436, NACL_OPCODE_NULL_OFFSET  },
  /* 733 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstCvttsd2si, 0x00, 2, 438, NACL_OPCODE_NULL_OFFSET  },
  /* 734 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstCvtsd2si, 0x00, 2, 438, NACL_OPCODE_NULL_OFFSET  },
  /* 735 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstSqrtsd, 0x00, 2, 428, NACL_OPCODE_NULL_OFFSET  },
  /* 736 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstAddsd, 0x00, 2, 440, NACL_OPCODE_NULL_OFFSET  },
  /* 737 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstMulsd, 0x00, 2, 440, NACL_OPCODE_NULL_OFFSET  },
  /* 738 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstCvtsd2ss, 0x00, 2, 442, NACL_OPCODE_NULL_OFFSET  },
  /* 739 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstSubsd, 0x00, 2, 440, NACL_OPCODE_NULL_OFFSET  },
  /* 740 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstMinsd, 0x00, 2, 440, NACL_OPCODE_NULL_OFFSET  },
  /* 741 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstDivsd, 0x00, 2, 440, NACL_OPCODE_NULL_OFFSET  },
  /* 742 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstMaxsd, 0x00, 2, 440, NACL_OPCODE_NULL_OFFSET  },
  /* 743 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsRepne),
    InstPshuflw, 0x00, 3, 444, NACL_OPCODE_NULL_OFFSET  },
  /* 744 */
  { NACLi_SSE4A,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeHasImmed2_b) | NACL_IFLAG(OpcodeAllowsRepne),
    InstInsertq, 0x00, 4, 447, NACL_OPCODE_NULL_OFFSET  },
  /* 745 */
  { NACLi_SSE4A,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstInsertq, 0x00, 2, 451, NACL_OPCODE_NULL_OFFSET  },
  /* 746 */
  { NACLi_SSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstHaddps, 0x00, 2, 350, NACL_OPCODE_NULL_OFFSET  },
  /* 747 */
  { NACLi_SSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstHsubps, 0x00, 2, 350, NACL_OPCODE_NULL_OFFSET  },
  /* 748 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsRepne),
    InstCmpsd_xmm, 0x00, 3, 453, NACL_OPCODE_NULL_OFFSET  },
  /* 749 */
  { NACLi_SSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstAddsubps, 0x00, 2, 456, NACL_OPCODE_NULL_OFFSET  },
  /* 750 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstMovdq2q, 0x00, 2, 458, NACL_OPCODE_NULL_OFFSET  },
  /* 751 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstCvtpd2dq, 0x00, 2, 460, NACL_OPCODE_NULL_OFFSET  },
  /* 752 */
  { NACLi_SSE3,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstLddqu, 0x00, 2, 462, NACL_OPCODE_NULL_OFFSET  },
  /* 753 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMovss, 0x00, 2, 464, NACL_OPCODE_NULL_OFFSET  },
  /* 754 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMovss, 0x00, 2, 466, NACL_OPCODE_NULL_OFFSET  },
  /* 755 */
  { NACLi_SSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMovsldup, 0x00, 2, 300, NACL_OPCODE_NULL_OFFSET  },
  /* 756 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 757 */
  { NACLi_SSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMovshdup, 0x00, 2, 300, NACL_OPCODE_NULL_OFFSET  },
  /* 758 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstCvtsi2ss, 0x00, 2, 468, NACL_OPCODE_NULL_OFFSET  },
  /* 759 */
  { NACLi_SSE4A,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMovntss, 0x00, 2, 470, NACL_OPCODE_NULL_OFFSET  },
  /* 760 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstCvttss2si, 0x00, 2, 472, NACL_OPCODE_NULL_OFFSET  },
  /* 761 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstCvtss2si, 0x00, 2, 472, NACL_OPCODE_NULL_OFFSET  },
  /* 762 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstSqrtss, 0x00, 2, 300, NACL_OPCODE_NULL_OFFSET  },
  /* 763 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstRsqrtss, 0x00, 2, 464, NACL_OPCODE_NULL_OFFSET  },
  /* 764 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstRcpss, 0x00, 2, 464, NACL_OPCODE_NULL_OFFSET  },
  /* 765 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstAddss, 0x00, 2, 474, NACL_OPCODE_NULL_OFFSET  },
  /* 766 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMulss, 0x00, 2, 474, NACL_OPCODE_NULL_OFFSET  },
  /* 767 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstCvtss2sd, 0x00, 2, 476, NACL_OPCODE_NULL_OFFSET  },
  /* 768 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstCvttps2dq, 0x00, 2, 478, NACL_OPCODE_NULL_OFFSET  },
  /* 769 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstSubss, 0x00, 2, 474, NACL_OPCODE_NULL_OFFSET  },
  /* 770 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMinss, 0x00, 2, 474, NACL_OPCODE_NULL_OFFSET  },
  /* 771 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstDivss, 0x00, 2, 474, NACL_OPCODE_NULL_OFFSET  },
  /* 772 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMaxss, 0x00, 2, 474, NACL_OPCODE_NULL_OFFSET  },
  /* 773 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMovdqu, 0x00, 2, 480, NACL_OPCODE_NULL_OFFSET  },
  /* 774 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsRep),
    InstPshufhw, 0x00, 3, 444, NACL_OPCODE_NULL_OFFSET  },
  /* 775 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMovq, 0x00, 2, 444, NACL_OPCODE_NULL_OFFSET  },
  /* 776 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMovdqu, 0x00, 2, 482, NACL_OPCODE_NULL_OFFSET  },
  /* 777 */
  { NACLi_POPCNT,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstPopcnt, 0x00, 2, 61, NACL_OPCODE_NULL_OFFSET  },
  /* 778 */
  { NACLi_LZCNT,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstLzcnt, 0x00, 2, 61, NACL_OPCODE_NULL_OFFSET  },
  /* 779 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsRep),
    InstCmpss, 0x00, 3, 484, NACL_OPCODE_NULL_OFFSET  },
  /* 780 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMovq2dq, 0x00, 2, 487, NACL_OPCODE_NULL_OFFSET  },
  /* 781 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstCvtdq2pd, 0x00, 2, 489, NACL_OPCODE_NULL_OFFSET  },
  /* 782 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovupd, 0x00, 2, 491, NACL_OPCODE_NULL_OFFSET  },
  /* 783 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovupd, 0x00, 2, 493, NACL_OPCODE_NULL_OFFSET  },
  /* 784 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovlpd, 0x00, 2, 495, NACL_OPCODE_NULL_OFFSET  },
  /* 785 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovlpd, 0x00, 2, 436, NACL_OPCODE_NULL_OFFSET  },
  /* 786 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstUnpcklpd, 0x00, 2, 497, NACL_OPCODE_NULL_OFFSET  },
  /* 787 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstUnpckhpd, 0x00, 2, 497, NACL_OPCODE_NULL_OFFSET  },
  /* 788 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovhpd, 0x00, 2, 495, NACL_OPCODE_NULL_OFFSET  },
  /* 789 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovhpd, 0x00, 2, 436, NACL_OPCODE_NULL_OFFSET  },
  /* 790 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovapd, 0x00, 2, 491, NACL_OPCODE_NULL_OFFSET  },
  /* 791 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovapd, 0x00, 2, 493, NACL_OPCODE_NULL_OFFSET  },
  /* 792 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstCvtpi2pd, 0x00, 2, 499, NACL_OPCODE_NULL_OFFSET  },
  /* 793 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovntpd, 0x00, 2, 501, NACL_OPCODE_NULL_OFFSET  },
  /* 794 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstCvttpd2pi, 0x00, 2, 503, NACL_OPCODE_NULL_OFFSET  },
  /* 795 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstCvtpd2pi, 0x00, 2, 503, NACL_OPCODE_NULL_OFFSET  },
  /* 796 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstUcomisd, 0x00, 2, 505, NACL_OPCODE_NULL_OFFSET  },
  /* 797 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstComisd, 0x00, 2, 507, NACL_OPCODE_NULL_OFFSET  },
  /* 798 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovmskpd, 0x00, 2, 509, NACL_OPCODE_NULL_OFFSET  },
  /* 799 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstSqrtpd, 0x00, 2, 511, NACL_OPCODE_NULL_OFFSET  },
  /* 800 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 801 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstAndpd, 0x00, 2, 456, NACL_OPCODE_NULL_OFFSET  },
  /* 802 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstAndnpd, 0x00, 2, 456, NACL_OPCODE_NULL_OFFSET  },
  /* 803 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstOrpd, 0x00, 2, 456, NACL_OPCODE_NULL_OFFSET  },
  /* 804 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstXorpd, 0x00, 2, 456, NACL_OPCODE_NULL_OFFSET  },
  /* 805 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstAddpd, 0x00, 2, 456, NACL_OPCODE_NULL_OFFSET  },
  /* 806 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMulpd, 0x00, 2, 456, NACL_OPCODE_NULL_OFFSET  },
  /* 807 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstCvtpd2ps, 0x00, 2, 511, NACL_OPCODE_NULL_OFFSET  },
  /* 808 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstCvtps2dq, 0x00, 2, 478, NACL_OPCODE_NULL_OFFSET  },
  /* 809 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstSubpd, 0x00, 2, 456, NACL_OPCODE_NULL_OFFSET  },
  /* 810 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMinpd, 0x00, 2, 456, NACL_OPCODE_NULL_OFFSET  },
  /* 811 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstDivpd, 0x00, 2, 456, NACL_OPCODE_NULL_OFFSET  },
  /* 812 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMaxpd, 0x00, 2, 456, NACL_OPCODE_NULL_OFFSET  },
  /* 813 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPunpcklbw, 0x00, 2, 513, NACL_OPCODE_NULL_OFFSET  },
  /* 814 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPunpcklwd, 0x00, 2, 513, NACL_OPCODE_NULL_OFFSET  },
  /* 815 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPunpckldq, 0x00, 2, 513, NACL_OPCODE_NULL_OFFSET  },
  /* 816 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPacksswb, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 817 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPcmpgtb, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 818 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPcmpgtw, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 819 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPcmpgtd, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 820 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPackuswb, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 821 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPunpckhbw, 0x00, 2, 513, NACL_OPCODE_NULL_OFFSET  },
  /* 822 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPunpckhwd, 0x00, 2, 513, NACL_OPCODE_NULL_OFFSET  },
  /* 823 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPunpckhdq, 0x00, 2, 513, NACL_OPCODE_NULL_OFFSET  },
  /* 824 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPackssdw, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 825 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPunpcklqdq, 0x00, 2, 513, NACL_OPCODE_NULL_OFFSET  },
  /* 826 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPunpckhqdq, 0x00, 2, 513, NACL_OPCODE_NULL_OFFSET  },
  /* 827 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstMovd, 0x00, 2, 517, NACL_OPCODE_NULL_OFFSET  },
  /* 828 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovdqa, 0x00, 2, 480, NACL_OPCODE_NULL_OFFSET  },
  /* 829 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPshufd, 0x00, 3, 519, NACL_OPCODE_NULL_OFFSET  },
  /* 830 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x07, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 831 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsllw, 0x06, 2, 522, 830  },
  /* 832 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x05, 0, 0, 831  },
  /* 833 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsraw, 0x04, 2, 522, 832  },
  /* 834 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x03, 0, 0, 833  },
  /* 835 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsrlw, 0x02, 2, 522, 834  },
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
    InstPslld, 0x06, 2, 522, 830  },
  /* 839 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x05, 0, 0, 838  },
  /* 840 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsrad, 0x04, 2, 522, 839  },
  /* 841 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x03, 0, 0, 840  },
  /* 842 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsrld, 0x02, 2, 522, 841  },
  /* 843 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x01, 0, 0, 842  },
  /* 844 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00, 0, 0, 843  },
  /* 845 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPslldq, 0x07, 2, 522, NACL_OPCODE_NULL_OFFSET  },
  /* 846 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsllq, 0x06, 2, 522, 845  },
  /* 847 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x05, 0, 0, 846  },
  /* 848 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x04, 0, 0, 847  },
  /* 849 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsrldq, 0x03, 2, 522, 848  },
  /* 850 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsrlq, 0x02, 2, 522, 849  },
  /* 851 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x01, 0, 0, 850  },
  /* 852 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00, 0, 0, 851  },
  /* 853 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPcmpeqb, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 854 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPcmpeqw, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 855 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPcmpeqd, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 856 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 857 */
  { NACLi_SSE4A,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeHasImmed2_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstExtrq, 0x00, 3, 524, 856  },
  /* 858 */
  { NACLi_SSE4A,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstExtrq, 0x00, 2, 451, NACL_OPCODE_NULL_OFFSET  },
  /* 859 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstHaddpd, 0x00, 2, 456, NACL_OPCODE_NULL_OFFSET  },
  /* 860 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstHsubpd, 0x00, 2, 456, NACL_OPCODE_NULL_OFFSET  },
  /* 861 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstMovd, 0x00, 2, 527, NACL_OPCODE_NULL_OFFSET  },
  /* 862 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovdqa, 0x00, 2, 482, NACL_OPCODE_NULL_OFFSET  },
  /* 863 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstCmppd, 0x00, 3, 529, NACL_OPCODE_NULL_OFFSET  },
  /* 864 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstPinsrw, 0x00, 3, 532, NACL_OPCODE_NULL_OFFSET  },
  /* 865 */
  { NACLi_SSE41,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPextrw, 0x00, 3, 535, NACL_OPCODE_NULL_OFFSET  },
  /* 866 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstShufpd, 0x00, 3, 529, NACL_OPCODE_NULL_OFFSET  },
  /* 867 */
  { NACLi_SSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstAddsubpd, 0x00, 2, 456, NACL_OPCODE_NULL_OFFSET  },
  /* 868 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsrlw, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 869 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsrld, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 870 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsrlq, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 871 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPaddq, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 872 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmullw, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 873 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovq, 0x00, 2, 538, NACL_OPCODE_NULL_OFFSET  },
  /* 874 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovmskb, 0x00, 2, 535, NACL_OPCODE_NULL_OFFSET  },
  /* 875 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsubusb, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 876 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsubusw, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 877 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPminub, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 878 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPand, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 879 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPaddusb, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 880 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPaddusw, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 881 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmaxub, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 882 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPandn, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 883 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPavgb, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 884 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsraw, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 885 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsrad, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 886 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPavgw, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 887 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmulhuw, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 888 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmulhw, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 889 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstCvttpd2dq, 0x00, 2, 540, NACL_OPCODE_NULL_OFFSET  },
  /* 890 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovntdq, 0x00, 2, 542, NACL_OPCODE_NULL_OFFSET  },
  /* 891 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsubsb, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 892 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsubsw, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 893 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPminsw, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 894 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPor, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 895 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPaddsb, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 896 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPaddsw, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 897 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmaxsw, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 898 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPxor, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 899 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsllw, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 900 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPslld, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 901 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsllq, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 902 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmuludq, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 903 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmaddwd, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 904 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsadbw, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 905 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstMaskmovdqu, 0x00, 3, 544, NACL_OPCODE_NULL_OFFSET  },
  /* 906 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsubb, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 907 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsubw, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 908 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsubd, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 909 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsubq, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 910 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPaddb, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 911 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPaddw, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 912 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPaddd, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 913 */
  { NACLi_E3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPi2fw, 0x00, 2, 362, NACL_OPCODE_NULL_OFFSET  },
  /* 914 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPi2fd, 0x00, 2, 362, NACL_OPCODE_NULL_OFFSET  },
  /* 915 */
  { NACLi_E3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPf2iw, 0x00, 2, 362, NACL_OPCODE_NULL_OFFSET  },
  /* 916 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPf2id, 0x00, 2, 362, NACL_OPCODE_NULL_OFFSET  },
  /* 917 */
  { NACLi_E3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfnacc, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 918 */
  { NACLi_E3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfpnacc, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 919 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfcmpge, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 920 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfmin, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 921 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfrcp, 0x00, 2, 362, NACL_OPCODE_NULL_OFFSET  },
  /* 922 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfrsqrt, 0x00, 2, 362, NACL_OPCODE_NULL_OFFSET  },
  /* 923 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfsub, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 924 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfadd, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 925 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfcmpgt, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 926 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfmax, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 927 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfrcpit1, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 928 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfrsqit1, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 929 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfsubr, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 930 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfacc, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 931 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfcmpeq, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 932 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfmul, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 933 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfrcpit2, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 934 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPmulhrw, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 935 */
  { NACLi_E3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPswapd, 0x00, 2, 362, NACL_OPCODE_NULL_OFFSET  },
  /* 936 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPavgusb, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 937 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPshufb, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 938 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPhaddw, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 939 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPhaddd, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 940 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPhaddsw, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 941 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPmaddubsw, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 942 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPhsubw, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 943 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPhsubd, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 944 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPhsubsw, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 945 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsignb, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 946 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsignw, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 947 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsignd, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 948 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPmulhrsw, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 949 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPabsb, 0x00, 2, 362, NACL_OPCODE_NULL_OFFSET  },
  /* 950 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPabsw, 0x00, 2, 362, NACL_OPCODE_NULL_OFFSET  },
  /* 951 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPabsd, 0x00, 2, 362, NACL_OPCODE_NULL_OFFSET  },
  /* 952 */
  { NACLi_MOVBE,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMovbe, 0x00, 2, 547, NACL_OPCODE_NULL_OFFSET  },
  /* 953 */
  { NACLi_MOVBE,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMovbe, 0x00, 2, 549, NACL_OPCODE_NULL_OFFSET  },
  /* 954 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPshufb, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 955 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPhaddw, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 956 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPhaddd, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 957 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPhaddsw, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 958 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmaddubsw, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 959 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPhsubw, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 960 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPhsubd, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 961 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPhsubsw, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 962 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsignb, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 963 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsignw, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 964 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsignd, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 965 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmulhrsw, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 966 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPblendvb, 0x00, 3, 551, NACL_OPCODE_NULL_OFFSET  },
  /* 967 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstBlendvps, 0x00, 3, 551, NACL_OPCODE_NULL_OFFSET  },
  /* 968 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstBlendvpd, 0x00, 3, 551, NACL_OPCODE_NULL_OFFSET  },
  /* 969 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPtest, 0x00, 2, 554, NACL_OPCODE_NULL_OFFSET  },
  /* 970 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPabsb, 0x00, 2, 480, NACL_OPCODE_NULL_OFFSET  },
  /* 971 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPabsw, 0x00, 2, 480, NACL_OPCODE_NULL_OFFSET  },
  /* 972 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPabsd, 0x00, 2, 480, NACL_OPCODE_NULL_OFFSET  },
  /* 973 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovsxbw, 0x00, 2, 556, NACL_OPCODE_NULL_OFFSET  },
  /* 974 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovsxbd, 0x00, 2, 558, NACL_OPCODE_NULL_OFFSET  },
  /* 975 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovsxbq, 0x00, 2, 560, NACL_OPCODE_NULL_OFFSET  },
  /* 976 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovsxwd, 0x00, 2, 556, NACL_OPCODE_NULL_OFFSET  },
  /* 977 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovsxwq, 0x00, 2, 558, NACL_OPCODE_NULL_OFFSET  },
  /* 978 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovsxdq, 0x00, 2, 556, NACL_OPCODE_NULL_OFFSET  },
  /* 979 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmuldq, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 980 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPcmpeqq, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 981 */
  { NACLi_SSE41,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovntdqa, 0x00, 2, 462, NACL_OPCODE_NULL_OFFSET  },
  /* 982 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPackusdw, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 983 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovzxbw, 0x00, 2, 556, NACL_OPCODE_NULL_OFFSET  },
  /* 984 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovzxbd, 0x00, 2, 558, NACL_OPCODE_NULL_OFFSET  },
  /* 985 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovzxbq, 0x00, 2, 560, NACL_OPCODE_NULL_OFFSET  },
  /* 986 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovzxwd, 0x00, 2, 556, NACL_OPCODE_NULL_OFFSET  },
  /* 987 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovzxwq, 0x00, 2, 558, NACL_OPCODE_NULL_OFFSET  },
  /* 988 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovzxdq, 0x00, 2, 556, NACL_OPCODE_NULL_OFFSET  },
  /* 989 */
  { NACLi_SSE42,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPcmpgtq, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 990 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPminsb, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 991 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPminsd, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 992 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPminuw, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 993 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPminud, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 994 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmaxsb, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 995 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmaxsd, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 996 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmaxuw, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 997 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmaxud, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 998 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmulld, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 999 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPhminposuw, 0x00, 2, 515, NACL_OPCODE_NULL_OFFSET  },
  /* 1000 */
  { NACLi_VMX,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvept, 0x00, 2, 562, NACL_OPCODE_NULL_OFFSET  },
  /* 1001 */
  { NACLi_VMX,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvvpid, 0x00, 2, 562, NACL_OPCODE_NULL_OFFSET  },
  /* 1002 */
  { NACLi_SSE42,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(OperandSize_b),
    InstCrc32, 0x00, 2, 564, NACL_OPCODE_NULL_OFFSET  },
  /* 1003 */
  { NACLi_SSE42,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstCrc32, 0x00, 2, 566, NACL_OPCODE_NULL_OFFSET  },
  /* 1004 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstPalignr, 0x00, 3, 568, NACL_OPCODE_NULL_OFFSET  },
  /* 1005 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstRoundps, 0x00, 3, 519, NACL_OPCODE_NULL_OFFSET  },
  /* 1006 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstRoundpd, 0x00, 3, 519, NACL_OPCODE_NULL_OFFSET  },
  /* 1007 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstRoundss, 0x00, 3, 571, NACL_OPCODE_NULL_OFFSET  },
  /* 1008 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstRoundsd, 0x00, 3, 574, NACL_OPCODE_NULL_OFFSET  },
  /* 1009 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstBlendps, 0x00, 3, 577, NACL_OPCODE_NULL_OFFSET  },
  /* 1010 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstBlendpd, 0x00, 3, 577, NACL_OPCODE_NULL_OFFSET  },
  /* 1011 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPblendw, 0x00, 3, 577, NACL_OPCODE_NULL_OFFSET  },
  /* 1012 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPalignr, 0x00, 3, 577, NACL_OPCODE_NULL_OFFSET  },
  /* 1013 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPextrb, 0x00, 3, 580, NACL_OPCODE_NULL_OFFSET  },
  /* 1014 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPextrw, 0x00, 3, 583, NACL_OPCODE_NULL_OFFSET  },
  /* 1015 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstPextrd, 0x00, 3, 586, NACL_OPCODE_NULL_OFFSET  },
  /* 1016 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstExtractps, 0x00, 3, 589, NACL_OPCODE_NULL_OFFSET  },
  /* 1017 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstPinsrb, 0x00, 3, 592, NACL_OPCODE_NULL_OFFSET  },
  /* 1018 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstInsertps, 0x00, 3, 595, NACL_OPCODE_NULL_OFFSET  },
  /* 1019 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstPinsrd, 0x00, 3, 598, NACL_OPCODE_NULL_OFFSET  },
  /* 1020 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstDpps, 0x00, 3, 577, NACL_OPCODE_NULL_OFFSET  },
  /* 1021 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstDppd, 0x00, 3, 577, NACL_OPCODE_NULL_OFFSET  },
  /* 1022 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMpsadbw, 0x00, 3, 577, NACL_OPCODE_NULL_OFFSET  },
  /* 1023 */
  { NACLi_SSE42,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstPcmpestrm, 0x00, 6, 601, NACL_OPCODE_NULL_OFFSET  },
  /* 1024 */
  { NACLi_SSE42,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstPcmpestri, 0x00, 6, 607, NACL_OPCODE_NULL_OFFSET  },
  /* 1025 */
  { NACLi_SSE42,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPcmpistrm, 0x00, 4, 613, NACL_OPCODE_NULL_OFFSET  },
  /* 1026 */
  { NACLi_SSE42,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstPcmpistri, 0x00, 4, 617, NACL_OPCODE_NULL_OFFSET  },
  /* 1027 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00, 2, 621, NACL_OPCODE_NULL_OFFSET  },
  /* 1028 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00, 2, 623, NACL_OPCODE_NULL_OFFSET  },
  /* 1029 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00, 2, 625, NACL_OPCODE_NULL_OFFSET  },
  /* 1030 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00, 2, 627, NACL_OPCODE_NULL_OFFSET  },
  /* 1031 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00, 2, 629, NACL_OPCODE_NULL_OFFSET  },
  /* 1032 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00, 2, 631, NACL_OPCODE_NULL_OFFSET  },
  /* 1033 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00, 2, 633, NACL_OPCODE_NULL_OFFSET  },
  /* 1034 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00, 2, 635, NACL_OPCODE_NULL_OFFSET  },
  /* 1035 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00, 2, 621, NACL_OPCODE_NULL_OFFSET  },
  /* 1036 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00, 2, 623, NACL_OPCODE_NULL_OFFSET  },
  /* 1037 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00, 2, 625, NACL_OPCODE_NULL_OFFSET  },
  /* 1038 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00, 2, 627, NACL_OPCODE_NULL_OFFSET  },
  /* 1039 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00, 2, 629, NACL_OPCODE_NULL_OFFSET  },
  /* 1040 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00, 2, 631, NACL_OPCODE_NULL_OFFSET  },
  /* 1041 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00, 2, 633, NACL_OPCODE_NULL_OFFSET  },
  /* 1042 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00, 2, 635, NACL_OPCODE_NULL_OFFSET  },
  /* 1043 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcom, 0x00, 2, 637, NACL_OPCODE_NULL_OFFSET  },
  /* 1044 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcom, 0x00, 2, 639, NACL_OPCODE_NULL_OFFSET  },
  /* 1045 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcom, 0x00, 2, 641, NACL_OPCODE_NULL_OFFSET  },
  /* 1046 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcom, 0x00, 2, 643, NACL_OPCODE_NULL_OFFSET  },
  /* 1047 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcom, 0x00, 2, 645, NACL_OPCODE_NULL_OFFSET  },
  /* 1048 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcom, 0x00, 2, 647, NACL_OPCODE_NULL_OFFSET  },
  /* 1049 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcom, 0x00, 2, 649, NACL_OPCODE_NULL_OFFSET  },
  /* 1050 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcom, 0x00, 2, 651, NACL_OPCODE_NULL_OFFSET  },
  /* 1051 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomp, 0x00, 2, 637, NACL_OPCODE_NULL_OFFSET  },
  /* 1052 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomp, 0x00, 2, 639, NACL_OPCODE_NULL_OFFSET  },
  /* 1053 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomp, 0x00, 2, 641, NACL_OPCODE_NULL_OFFSET  },
  /* 1054 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomp, 0x00, 2, 643, NACL_OPCODE_NULL_OFFSET  },
  /* 1055 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomp, 0x00, 2, 645, NACL_OPCODE_NULL_OFFSET  },
  /* 1056 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomp, 0x00, 2, 647, NACL_OPCODE_NULL_OFFSET  },
  /* 1057 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomp, 0x00, 2, 649, NACL_OPCODE_NULL_OFFSET  },
  /* 1058 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomp, 0x00, 2, 651, NACL_OPCODE_NULL_OFFSET  },
  /* 1059 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00, 2, 621, NACL_OPCODE_NULL_OFFSET  },
  /* 1060 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00, 2, 623, NACL_OPCODE_NULL_OFFSET  },
  /* 1061 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00, 2, 625, NACL_OPCODE_NULL_OFFSET  },
  /* 1062 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00, 2, 627, NACL_OPCODE_NULL_OFFSET  },
  /* 1063 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00, 2, 629, NACL_OPCODE_NULL_OFFSET  },
  /* 1064 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00, 2, 631, NACL_OPCODE_NULL_OFFSET  },
  /* 1065 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00, 2, 633, NACL_OPCODE_NULL_OFFSET  },
  /* 1066 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00, 2, 635, NACL_OPCODE_NULL_OFFSET  },
  /* 1067 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00, 2, 621, NACL_OPCODE_NULL_OFFSET  },
  /* 1068 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00, 2, 623, NACL_OPCODE_NULL_OFFSET  },
  /* 1069 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00, 2, 625, NACL_OPCODE_NULL_OFFSET  },
  /* 1070 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00, 2, 627, NACL_OPCODE_NULL_OFFSET  },
  /* 1071 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00, 2, 629, NACL_OPCODE_NULL_OFFSET  },
  /* 1072 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00, 2, 631, NACL_OPCODE_NULL_OFFSET  },
  /* 1073 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00, 2, 633, NACL_OPCODE_NULL_OFFSET  },
  /* 1074 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00, 2, 635, NACL_OPCODE_NULL_OFFSET  },
  /* 1075 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00, 2, 621, NACL_OPCODE_NULL_OFFSET  },
  /* 1076 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00, 2, 623, NACL_OPCODE_NULL_OFFSET  },
  /* 1077 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00, 2, 625, NACL_OPCODE_NULL_OFFSET  },
  /* 1078 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00, 2, 627, NACL_OPCODE_NULL_OFFSET  },
  /* 1079 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00, 2, 629, NACL_OPCODE_NULL_OFFSET  },
  /* 1080 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00, 2, 631, NACL_OPCODE_NULL_OFFSET  },
  /* 1081 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00, 2, 633, NACL_OPCODE_NULL_OFFSET  },
  /* 1082 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00, 2, 635, NACL_OPCODE_NULL_OFFSET  },
  /* 1083 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00, 2, 621, NACL_OPCODE_NULL_OFFSET  },
  /* 1084 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00, 2, 623, NACL_OPCODE_NULL_OFFSET  },
  /* 1085 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00, 2, 625, NACL_OPCODE_NULL_OFFSET  },
  /* 1086 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00, 2, 627, NACL_OPCODE_NULL_OFFSET  },
  /* 1087 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00, 2, 629, NACL_OPCODE_NULL_OFFSET  },
  /* 1088 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00, 2, 631, NACL_OPCODE_NULL_OFFSET  },
  /* 1089 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00, 2, 633, NACL_OPCODE_NULL_OFFSET  },
  /* 1090 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00, 2, 635, NACL_OPCODE_NULL_OFFSET  },
  /* 1091 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFld, 0x00, 2, 653, NACL_OPCODE_NULL_OFFSET  },
  /* 1092 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFld, 0x00, 2, 655, NACL_OPCODE_NULL_OFFSET  },
  /* 1093 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFld, 0x00, 2, 657, NACL_OPCODE_NULL_OFFSET  },
  /* 1094 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFld, 0x00, 2, 659, NACL_OPCODE_NULL_OFFSET  },
  /* 1095 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFld, 0x00, 2, 661, NACL_OPCODE_NULL_OFFSET  },
  /* 1096 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFld, 0x00, 2, 663, NACL_OPCODE_NULL_OFFSET  },
  /* 1097 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFld, 0x00, 2, 665, NACL_OPCODE_NULL_OFFSET  },
  /* 1098 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFld, 0x00, 2, 667, NACL_OPCODE_NULL_OFFSET  },
  /* 1099 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFxch, 0x00, 2, 669, NACL_OPCODE_NULL_OFFSET  },
  /* 1100 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFxch, 0x00, 2, 671, NACL_OPCODE_NULL_OFFSET  },
  /* 1101 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFxch, 0x00, 2, 673, NACL_OPCODE_NULL_OFFSET  },
  /* 1102 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFxch, 0x00, 2, 675, NACL_OPCODE_NULL_OFFSET  },
  /* 1103 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFxch, 0x00, 2, 677, NACL_OPCODE_NULL_OFFSET  },
  /* 1104 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFxch, 0x00, 2, 679, NACL_OPCODE_NULL_OFFSET  },
  /* 1105 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFxch, 0x00, 2, 681, NACL_OPCODE_NULL_OFFSET  },
  /* 1106 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFxch, 0x00, 2, 683, NACL_OPCODE_NULL_OFFSET  },
  /* 1107 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFnop, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 1108 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFchs, 0x00, 1, 202, NACL_OPCODE_NULL_OFFSET  },
  /* 1109 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFabs, 0x00, 1, 202, NACL_OPCODE_NULL_OFFSET  },
  /* 1110 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFtst, 0x00, 1, 204, NACL_OPCODE_NULL_OFFSET  },
  /* 1111 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFxam, 0x00, 1, 204, NACL_OPCODE_NULL_OFFSET  },
  /* 1112 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFld1, 0x00, 1, 202, NACL_OPCODE_NULL_OFFSET  },
  /* 1113 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFldl2t, 0x00, 1, 202, NACL_OPCODE_NULL_OFFSET  },
  /* 1114 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFldl2e, 0x00, 1, 202, NACL_OPCODE_NULL_OFFSET  },
  /* 1115 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFldpi, 0x00, 1, 202, NACL_OPCODE_NULL_OFFSET  },
  /* 1116 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFldlg2, 0x00, 1, 202, NACL_OPCODE_NULL_OFFSET  },
  /* 1117 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFldln2, 0x00, 1, 202, NACL_OPCODE_NULL_OFFSET  },
  /* 1118 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFldz, 0x00, 1, 202, NACL_OPCODE_NULL_OFFSET  },
  /* 1119 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstF2xm1, 0x00, 1, 202, NACL_OPCODE_NULL_OFFSET  },
  /* 1120 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFyl2x, 0x00, 2, 623, NACL_OPCODE_NULL_OFFSET  },
  /* 1121 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFptan, 0x00, 2, 655, NACL_OPCODE_NULL_OFFSET  },
  /* 1122 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFpatan, 0x00, 2, 623, NACL_OPCODE_NULL_OFFSET  },
  /* 1123 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFxtract, 0x00, 2, 655, NACL_OPCODE_NULL_OFFSET  },
  /* 1124 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFprem1, 0x00, 2, 623, NACL_OPCODE_NULL_OFFSET  },
  /* 1125 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdecstp, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 1126 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFincstp, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 1127 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFprem, 0x00, 2, 623, NACL_OPCODE_NULL_OFFSET  },
  /* 1128 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFyl2xp1, 0x00, 2, 623, NACL_OPCODE_NULL_OFFSET  },
  /* 1129 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsqrt, 0x00, 1, 202, NACL_OPCODE_NULL_OFFSET  },
  /* 1130 */
  { NACLi_X87_FSINCOS,
    NACL_EMPTY_IFLAGS,
    InstFsincos, 0x00, 2, 655, NACL_OPCODE_NULL_OFFSET  },
  /* 1131 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFrndint, 0x00, 1, 202, NACL_OPCODE_NULL_OFFSET  },
  /* 1132 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFscale, 0x00, 2, 623, NACL_OPCODE_NULL_OFFSET  },
  /* 1133 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsin, 0x00, 1, 202, NACL_OPCODE_NULL_OFFSET  },
  /* 1134 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcos, 0x00, 1, 202, NACL_OPCODE_NULL_OFFSET  },
  /* 1135 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovb, 0x00, 2, 621, NACL_OPCODE_NULL_OFFSET  },
  /* 1136 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovb, 0x00, 2, 623, NACL_OPCODE_NULL_OFFSET  },
  /* 1137 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovb, 0x00, 2, 625, NACL_OPCODE_NULL_OFFSET  },
  /* 1138 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovb, 0x00, 2, 627, NACL_OPCODE_NULL_OFFSET  },
  /* 1139 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovb, 0x00, 2, 629, NACL_OPCODE_NULL_OFFSET  },
  /* 1140 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovb, 0x00, 2, 631, NACL_OPCODE_NULL_OFFSET  },
  /* 1141 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovb, 0x00, 2, 633, NACL_OPCODE_NULL_OFFSET  },
  /* 1142 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovb, 0x00, 2, 635, NACL_OPCODE_NULL_OFFSET  },
  /* 1143 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmove, 0x00, 2, 621, NACL_OPCODE_NULL_OFFSET  },
  /* 1144 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmove, 0x00, 2, 623, NACL_OPCODE_NULL_OFFSET  },
  /* 1145 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmove, 0x00, 2, 625, NACL_OPCODE_NULL_OFFSET  },
  /* 1146 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmove, 0x00, 2, 627, NACL_OPCODE_NULL_OFFSET  },
  /* 1147 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmove, 0x00, 2, 629, NACL_OPCODE_NULL_OFFSET  },
  /* 1148 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmove, 0x00, 2, 631, NACL_OPCODE_NULL_OFFSET  },
  /* 1149 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmove, 0x00, 2, 633, NACL_OPCODE_NULL_OFFSET  },
  /* 1150 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmove, 0x00, 2, 635, NACL_OPCODE_NULL_OFFSET  },
  /* 1151 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovbe, 0x00, 2, 621, NACL_OPCODE_NULL_OFFSET  },
  /* 1152 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovbe, 0x00, 2, 623, NACL_OPCODE_NULL_OFFSET  },
  /* 1153 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovbe, 0x00, 2, 625, NACL_OPCODE_NULL_OFFSET  },
  /* 1154 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovbe, 0x00, 2, 627, NACL_OPCODE_NULL_OFFSET  },
  /* 1155 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovbe, 0x00, 2, 629, NACL_OPCODE_NULL_OFFSET  },
  /* 1156 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovbe, 0x00, 2, 631, NACL_OPCODE_NULL_OFFSET  },
  /* 1157 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovbe, 0x00, 2, 633, NACL_OPCODE_NULL_OFFSET  },
  /* 1158 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovbe, 0x00, 2, 635, NACL_OPCODE_NULL_OFFSET  },
  /* 1159 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovu, 0x00, 2, 621, NACL_OPCODE_NULL_OFFSET  },
  /* 1160 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovu, 0x00, 2, 623, NACL_OPCODE_NULL_OFFSET  },
  /* 1161 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovu, 0x00, 2, 625, NACL_OPCODE_NULL_OFFSET  },
  /* 1162 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovu, 0x00, 2, 627, NACL_OPCODE_NULL_OFFSET  },
  /* 1163 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovu, 0x00, 2, 629, NACL_OPCODE_NULL_OFFSET  },
  /* 1164 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovu, 0x00, 2, 631, NACL_OPCODE_NULL_OFFSET  },
  /* 1165 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovu, 0x00, 2, 633, NACL_OPCODE_NULL_OFFSET  },
  /* 1166 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovu, 0x00, 2, 635, NACL_OPCODE_NULL_OFFSET  },
  /* 1167 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucompp, 0x00, 2, 639, NACL_OPCODE_NULL_OFFSET  },
  /* 1168 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnb, 0x00, 2, 621, NACL_OPCODE_NULL_OFFSET  },
  /* 1169 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnb, 0x00, 2, 623, NACL_OPCODE_NULL_OFFSET  },
  /* 1170 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnb, 0x00, 2, 625, NACL_OPCODE_NULL_OFFSET  },
  /* 1171 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnb, 0x00, 2, 627, NACL_OPCODE_NULL_OFFSET  },
  /* 1172 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnb, 0x00, 2, 629, NACL_OPCODE_NULL_OFFSET  },
  /* 1173 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnb, 0x00, 2, 631, NACL_OPCODE_NULL_OFFSET  },
  /* 1174 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnb, 0x00, 2, 633, NACL_OPCODE_NULL_OFFSET  },
  /* 1175 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnb, 0x00, 2, 635, NACL_OPCODE_NULL_OFFSET  },
  /* 1176 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovne, 0x00, 2, 621, NACL_OPCODE_NULL_OFFSET  },
  /* 1177 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovne, 0x00, 2, 623, NACL_OPCODE_NULL_OFFSET  },
  /* 1178 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovne, 0x00, 2, 625, NACL_OPCODE_NULL_OFFSET  },
  /* 1179 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovne, 0x00, 2, 627, NACL_OPCODE_NULL_OFFSET  },
  /* 1180 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovne, 0x00, 2, 629, NACL_OPCODE_NULL_OFFSET  },
  /* 1181 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovne, 0x00, 2, 631, NACL_OPCODE_NULL_OFFSET  },
  /* 1182 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovne, 0x00, 2, 633, NACL_OPCODE_NULL_OFFSET  },
  /* 1183 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovne, 0x00, 2, 635, NACL_OPCODE_NULL_OFFSET  },
  /* 1184 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnbe, 0x00, 2, 621, NACL_OPCODE_NULL_OFFSET  },
  /* 1185 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnbe, 0x00, 2, 623, NACL_OPCODE_NULL_OFFSET  },
  /* 1186 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnbe, 0x00, 2, 625, NACL_OPCODE_NULL_OFFSET  },
  /* 1187 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnbe, 0x00, 2, 627, NACL_OPCODE_NULL_OFFSET  },
  /* 1188 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnbe, 0x00, 2, 629, NACL_OPCODE_NULL_OFFSET  },
  /* 1189 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnbe, 0x00, 2, 631, NACL_OPCODE_NULL_OFFSET  },
  /* 1190 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnbe, 0x00, 2, 633, NACL_OPCODE_NULL_OFFSET  },
  /* 1191 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnbe, 0x00, 2, 635, NACL_OPCODE_NULL_OFFSET  },
  /* 1192 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnu, 0x00, 2, 621, NACL_OPCODE_NULL_OFFSET  },
  /* 1193 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnu, 0x00, 2, 623, NACL_OPCODE_NULL_OFFSET  },
  /* 1194 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnu, 0x00, 2, 625, NACL_OPCODE_NULL_OFFSET  },
  /* 1195 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnu, 0x00, 2, 627, NACL_OPCODE_NULL_OFFSET  },
  /* 1196 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnu, 0x00, 2, 629, NACL_OPCODE_NULL_OFFSET  },
  /* 1197 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnu, 0x00, 2, 631, NACL_OPCODE_NULL_OFFSET  },
  /* 1198 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnu, 0x00, 2, 633, NACL_OPCODE_NULL_OFFSET  },
  /* 1199 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnu, 0x00, 2, 635, NACL_OPCODE_NULL_OFFSET  },
  /* 1200 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFnclex, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 1201 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFninit, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 1202 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomi, 0x00, 2, 637, NACL_OPCODE_NULL_OFFSET  },
  /* 1203 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomi, 0x00, 2, 639, NACL_OPCODE_NULL_OFFSET  },
  /* 1204 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomi, 0x00, 2, 641, NACL_OPCODE_NULL_OFFSET  },
  /* 1205 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomi, 0x00, 2, 643, NACL_OPCODE_NULL_OFFSET  },
  /* 1206 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomi, 0x00, 2, 645, NACL_OPCODE_NULL_OFFSET  },
  /* 1207 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomi, 0x00, 2, 647, NACL_OPCODE_NULL_OFFSET  },
  /* 1208 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomi, 0x00, 2, 649, NACL_OPCODE_NULL_OFFSET  },
  /* 1209 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomi, 0x00, 2, 651, NACL_OPCODE_NULL_OFFSET  },
  /* 1210 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomi, 0x00, 2, 637, NACL_OPCODE_NULL_OFFSET  },
  /* 1211 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomi, 0x00, 2, 639, NACL_OPCODE_NULL_OFFSET  },
  /* 1212 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomi, 0x00, 2, 641, NACL_OPCODE_NULL_OFFSET  },
  /* 1213 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomi, 0x00, 2, 643, NACL_OPCODE_NULL_OFFSET  },
  /* 1214 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomi, 0x00, 2, 645, NACL_OPCODE_NULL_OFFSET  },
  /* 1215 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomi, 0x00, 2, 647, NACL_OPCODE_NULL_OFFSET  },
  /* 1216 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomi, 0x00, 2, 649, NACL_OPCODE_NULL_OFFSET  },
  /* 1217 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomi, 0x00, 2, 651, NACL_OPCODE_NULL_OFFSET  },
  /* 1218 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00, 2, 685, NACL_OPCODE_NULL_OFFSET  },
  /* 1219 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00, 2, 687, NACL_OPCODE_NULL_OFFSET  },
  /* 1220 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00, 2, 689, NACL_OPCODE_NULL_OFFSET  },
  /* 1221 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00, 2, 691, NACL_OPCODE_NULL_OFFSET  },
  /* 1222 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00, 2, 693, NACL_OPCODE_NULL_OFFSET  },
  /* 1223 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00, 2, 695, NACL_OPCODE_NULL_OFFSET  },
  /* 1224 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00, 2, 697, NACL_OPCODE_NULL_OFFSET  },
  /* 1225 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00, 2, 685, NACL_OPCODE_NULL_OFFSET  },
  /* 1226 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00, 2, 687, NACL_OPCODE_NULL_OFFSET  },
  /* 1227 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00, 2, 689, NACL_OPCODE_NULL_OFFSET  },
  /* 1228 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00, 2, 691, NACL_OPCODE_NULL_OFFSET  },
  /* 1229 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00, 2, 693, NACL_OPCODE_NULL_OFFSET  },
  /* 1230 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00, 2, 695, NACL_OPCODE_NULL_OFFSET  },
  /* 1231 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00, 2, 697, NACL_OPCODE_NULL_OFFSET  },
  /* 1232 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00, 2, 685, NACL_OPCODE_NULL_OFFSET  },
  /* 1233 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00, 2, 687, NACL_OPCODE_NULL_OFFSET  },
  /* 1234 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00, 2, 689, NACL_OPCODE_NULL_OFFSET  },
  /* 1235 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00, 2, 691, NACL_OPCODE_NULL_OFFSET  },
  /* 1236 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00, 2, 693, NACL_OPCODE_NULL_OFFSET  },
  /* 1237 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00, 2, 695, NACL_OPCODE_NULL_OFFSET  },
  /* 1238 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00, 2, 697, NACL_OPCODE_NULL_OFFSET  },
  /* 1239 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00, 2, 685, NACL_OPCODE_NULL_OFFSET  },
  /* 1240 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00, 2, 687, NACL_OPCODE_NULL_OFFSET  },
  /* 1241 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00, 2, 689, NACL_OPCODE_NULL_OFFSET  },
  /* 1242 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00, 2, 691, NACL_OPCODE_NULL_OFFSET  },
  /* 1243 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00, 2, 693, NACL_OPCODE_NULL_OFFSET  },
  /* 1244 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00, 2, 695, NACL_OPCODE_NULL_OFFSET  },
  /* 1245 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00, 2, 697, NACL_OPCODE_NULL_OFFSET  },
  /* 1246 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00, 2, 685, NACL_OPCODE_NULL_OFFSET  },
  /* 1247 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00, 2, 687, NACL_OPCODE_NULL_OFFSET  },
  /* 1248 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00, 2, 689, NACL_OPCODE_NULL_OFFSET  },
  /* 1249 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00, 2, 691, NACL_OPCODE_NULL_OFFSET  },
  /* 1250 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00, 2, 693, NACL_OPCODE_NULL_OFFSET  },
  /* 1251 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00, 2, 695, NACL_OPCODE_NULL_OFFSET  },
  /* 1252 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00, 2, 697, NACL_OPCODE_NULL_OFFSET  },
  /* 1253 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00, 2, 685, NACL_OPCODE_NULL_OFFSET  },
  /* 1254 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00, 2, 687, NACL_OPCODE_NULL_OFFSET  },
  /* 1255 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00, 2, 689, NACL_OPCODE_NULL_OFFSET  },
  /* 1256 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00, 2, 691, NACL_OPCODE_NULL_OFFSET  },
  /* 1257 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00, 2, 693, NACL_OPCODE_NULL_OFFSET  },
  /* 1258 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00, 2, 695, NACL_OPCODE_NULL_OFFSET  },
  /* 1259 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00, 2, 697, NACL_OPCODE_NULL_OFFSET  },
  /* 1260 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFfree, 0x00, 1, 699, NACL_OPCODE_NULL_OFFSET  },
  /* 1261 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFfree, 0x00, 1, 700, NACL_OPCODE_NULL_OFFSET  },
  /* 1262 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFfree, 0x00, 1, 701, NACL_OPCODE_NULL_OFFSET  },
  /* 1263 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFfree, 0x00, 1, 702, NACL_OPCODE_NULL_OFFSET  },
  /* 1264 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFfree, 0x00, 1, 703, NACL_OPCODE_NULL_OFFSET  },
  /* 1265 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFfree, 0x00, 1, 704, NACL_OPCODE_NULL_OFFSET  },
  /* 1266 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFfree, 0x00, 1, 705, NACL_OPCODE_NULL_OFFSET  },
  /* 1267 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFfree, 0x00, 1, 706, NACL_OPCODE_NULL_OFFSET  },
  /* 1268 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFst, 0x00, 2, 653, NACL_OPCODE_NULL_OFFSET  },
  /* 1269 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFst, 0x00, 2, 707, NACL_OPCODE_NULL_OFFSET  },
  /* 1270 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFst, 0x00, 2, 709, NACL_OPCODE_NULL_OFFSET  },
  /* 1271 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFst, 0x00, 2, 711, NACL_OPCODE_NULL_OFFSET  },
  /* 1272 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFst, 0x00, 2, 713, NACL_OPCODE_NULL_OFFSET  },
  /* 1273 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFst, 0x00, 2, 715, NACL_OPCODE_NULL_OFFSET  },
  /* 1274 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFst, 0x00, 2, 717, NACL_OPCODE_NULL_OFFSET  },
  /* 1275 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFst, 0x00, 2, 719, NACL_OPCODE_NULL_OFFSET  },
  /* 1276 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFstp, 0x00, 2, 653, NACL_OPCODE_NULL_OFFSET  },
  /* 1277 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFstp, 0x00, 2, 707, NACL_OPCODE_NULL_OFFSET  },
  /* 1278 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFstp, 0x00, 2, 709, NACL_OPCODE_NULL_OFFSET  },
  /* 1279 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFstp, 0x00, 2, 711, NACL_OPCODE_NULL_OFFSET  },
  /* 1280 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFstp, 0x00, 2, 713, NACL_OPCODE_NULL_OFFSET  },
  /* 1281 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFstp, 0x00, 2, 715, NACL_OPCODE_NULL_OFFSET  },
  /* 1282 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFstp, 0x00, 2, 717, NACL_OPCODE_NULL_OFFSET  },
  /* 1283 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFstp, 0x00, 2, 719, NACL_OPCODE_NULL_OFFSET  },
  /* 1284 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucom, 0x00, 2, 637, NACL_OPCODE_NULL_OFFSET  },
  /* 1285 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucom, 0x00, 2, 639, NACL_OPCODE_NULL_OFFSET  },
  /* 1286 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucom, 0x00, 2, 641, NACL_OPCODE_NULL_OFFSET  },
  /* 1287 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucom, 0x00, 2, 643, NACL_OPCODE_NULL_OFFSET  },
  /* 1288 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucom, 0x00, 2, 645, NACL_OPCODE_NULL_OFFSET  },
  /* 1289 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucom, 0x00, 2, 647, NACL_OPCODE_NULL_OFFSET  },
  /* 1290 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucom, 0x00, 2, 649, NACL_OPCODE_NULL_OFFSET  },
  /* 1291 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucom, 0x00, 2, 651, NACL_OPCODE_NULL_OFFSET  },
  /* 1292 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomp, 0x00, 2, 637, NACL_OPCODE_NULL_OFFSET  },
  /* 1293 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomp, 0x00, 2, 639, NACL_OPCODE_NULL_OFFSET  },
  /* 1294 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomp, 0x00, 2, 641, NACL_OPCODE_NULL_OFFSET  },
  /* 1295 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomp, 0x00, 2, 643, NACL_OPCODE_NULL_OFFSET  },
  /* 1296 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomp, 0x00, 2, 645, NACL_OPCODE_NULL_OFFSET  },
  /* 1297 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomp, 0x00, 2, 647, NACL_OPCODE_NULL_OFFSET  },
  /* 1298 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomp, 0x00, 2, 649, NACL_OPCODE_NULL_OFFSET  },
  /* 1299 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomp, 0x00, 2, 651, NACL_OPCODE_NULL_OFFSET  },
  /* 1300 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFaddp, 0x00, 2, 621, NACL_OPCODE_NULL_OFFSET  },
  /* 1301 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFaddp, 0x00, 2, 685, NACL_OPCODE_NULL_OFFSET  },
  /* 1302 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFaddp, 0x00, 2, 687, NACL_OPCODE_NULL_OFFSET  },
  /* 1303 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFaddp, 0x00, 2, 689, NACL_OPCODE_NULL_OFFSET  },
  /* 1304 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFaddp, 0x00, 2, 691, NACL_OPCODE_NULL_OFFSET  },
  /* 1305 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFaddp, 0x00, 2, 693, NACL_OPCODE_NULL_OFFSET  },
  /* 1306 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFaddp, 0x00, 2, 695, NACL_OPCODE_NULL_OFFSET  },
  /* 1307 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFaddp, 0x00, 2, 697, NACL_OPCODE_NULL_OFFSET  },
  /* 1308 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmulp, 0x00, 2, 621, NACL_OPCODE_NULL_OFFSET  },
  /* 1309 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmulp, 0x00, 2, 685, NACL_OPCODE_NULL_OFFSET  },
  /* 1310 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmulp, 0x00, 2, 687, NACL_OPCODE_NULL_OFFSET  },
  /* 1311 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmulp, 0x00, 2, 689, NACL_OPCODE_NULL_OFFSET  },
  /* 1312 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmulp, 0x00, 2, 691, NACL_OPCODE_NULL_OFFSET  },
  /* 1313 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmulp, 0x00, 2, 693, NACL_OPCODE_NULL_OFFSET  },
  /* 1314 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmulp, 0x00, 2, 695, NACL_OPCODE_NULL_OFFSET  },
  /* 1315 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmulp, 0x00, 2, 697, NACL_OPCODE_NULL_OFFSET  },
  /* 1316 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcompp, 0x00, 2, 639, NACL_OPCODE_NULL_OFFSET  },
  /* 1317 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubrp, 0x00, 2, 621, NACL_OPCODE_NULL_OFFSET  },
  /* 1318 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubrp, 0x00, 2, 685, NACL_OPCODE_NULL_OFFSET  },
  /* 1319 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubrp, 0x00, 2, 687, NACL_OPCODE_NULL_OFFSET  },
  /* 1320 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubrp, 0x00, 2, 689, NACL_OPCODE_NULL_OFFSET  },
  /* 1321 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubrp, 0x00, 2, 691, NACL_OPCODE_NULL_OFFSET  },
  /* 1322 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubrp, 0x00, 2, 693, NACL_OPCODE_NULL_OFFSET  },
  /* 1323 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubrp, 0x00, 2, 695, NACL_OPCODE_NULL_OFFSET  },
  /* 1324 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubrp, 0x00, 2, 697, NACL_OPCODE_NULL_OFFSET  },
  /* 1325 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubp, 0x00, 2, 621, NACL_OPCODE_NULL_OFFSET  },
  /* 1326 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubp, 0x00, 2, 685, NACL_OPCODE_NULL_OFFSET  },
  /* 1327 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubp, 0x00, 2, 687, NACL_OPCODE_NULL_OFFSET  },
  /* 1328 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubp, 0x00, 2, 689, NACL_OPCODE_NULL_OFFSET  },
  /* 1329 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubp, 0x00, 2, 691, NACL_OPCODE_NULL_OFFSET  },
  /* 1330 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubp, 0x00, 2, 693, NACL_OPCODE_NULL_OFFSET  },
  /* 1331 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubp, 0x00, 2, 695, NACL_OPCODE_NULL_OFFSET  },
  /* 1332 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubp, 0x00, 2, 697, NACL_OPCODE_NULL_OFFSET  },
  /* 1333 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivrp, 0x00, 2, 621, NACL_OPCODE_NULL_OFFSET  },
  /* 1334 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivrp, 0x00, 2, 685, NACL_OPCODE_NULL_OFFSET  },
  /* 1335 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivrp, 0x00, 2, 687, NACL_OPCODE_NULL_OFFSET  },
  /* 1336 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivrp, 0x00, 2, 689, NACL_OPCODE_NULL_OFFSET  },
  /* 1337 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivrp, 0x00, 2, 691, NACL_OPCODE_NULL_OFFSET  },
  /* 1338 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivrp, 0x00, 2, 693, NACL_OPCODE_NULL_OFFSET  },
  /* 1339 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivrp, 0x00, 2, 695, NACL_OPCODE_NULL_OFFSET  },
  /* 1340 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivrp, 0x00, 2, 697, NACL_OPCODE_NULL_OFFSET  },
  /* 1341 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivp, 0x00, 2, 621, NACL_OPCODE_NULL_OFFSET  },
  /* 1342 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivp, 0x00, 2, 685, NACL_OPCODE_NULL_OFFSET  },
  /* 1343 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivp, 0x00, 2, 687, NACL_OPCODE_NULL_OFFSET  },
  /* 1344 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivp, 0x00, 2, 689, NACL_OPCODE_NULL_OFFSET  },
  /* 1345 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivp, 0x00, 2, 691, NACL_OPCODE_NULL_OFFSET  },
  /* 1346 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivp, 0x00, 2, 693, NACL_OPCODE_NULL_OFFSET  },
  /* 1347 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivp, 0x00, 2, 695, NACL_OPCODE_NULL_OFFSET  },
  /* 1348 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivp, 0x00, 2, 697, NACL_OPCODE_NULL_OFFSET  },
  /* 1349 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 1350 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFnstsw, 0x00, 1, 721, NACL_OPCODE_NULL_OFFSET  },
  /* 1351 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomip, 0x00, 2, 637, NACL_OPCODE_NULL_OFFSET  },
  /* 1352 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomip, 0x00, 2, 639, NACL_OPCODE_NULL_OFFSET  },
  /* 1353 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomip, 0x00, 2, 641, NACL_OPCODE_NULL_OFFSET  },
  /* 1354 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomip, 0x00, 2, 643, NACL_OPCODE_NULL_OFFSET  },
  /* 1355 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomip, 0x00, 2, 645, NACL_OPCODE_NULL_OFFSET  },
  /* 1356 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomip, 0x00, 2, 647, NACL_OPCODE_NULL_OFFSET  },
  /* 1357 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomip, 0x00, 2, 649, NACL_OPCODE_NULL_OFFSET  },
  /* 1358 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomip, 0x00, 2, 651, NACL_OPCODE_NULL_OFFSET  },
  /* 1359 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomip, 0x00, 2, 637, NACL_OPCODE_NULL_OFFSET  },
  /* 1360 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomip, 0x00, 2, 639, NACL_OPCODE_NULL_OFFSET  },
  /* 1361 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomip, 0x00, 2, 641, NACL_OPCODE_NULL_OFFSET  },
  /* 1362 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomip, 0x00, 2, 643, NACL_OPCODE_NULL_OFFSET  },
  /* 1363 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomip, 0x00, 2, 645, NACL_OPCODE_NULL_OFFSET  },
  /* 1364 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomip, 0x00, 2, 647, NACL_OPCODE_NULL_OFFSET  },
  /* 1365 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomip, 0x00, 2, 649, NACL_OPCODE_NULL_OFFSET  },
  /* 1366 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomip, 0x00, 2, 651, NACL_OPCODE_NULL_OFFSET  },
  /* 1367 */
  { NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstUd2, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 1368 */
  { NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstNop, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 1369 */
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
  /*   150 */ 176, 177, 179, 181, 182, 183, 185, 187, 188, 189, 
  /*   160 */ 190, 191, 192, 193, 194, 196, 197, 199, 200, 201, 
  /*   170 */ 202, 204, 205, 207, 208, 210, 211, 212, 213, 214, 
  /*   180 */ 215, 216, 217, 218, 219, 220, 221, 222, 223, 224, 
  /*   190 */ 225, 226, 234, 242, 243, 244, 245, 246, 247, 248, 
  /*   200 */ 249, 250, 251, 252, 253, 254, 255, 257, 265, 273, 
  /*   210 */ 281, 289, 290, 291, 292, 293, 301, 309, 317, 325, 
  /*   220 */ 333, 341, 349, 357, 358, 359, 360, 362, 363, 364, 
  /*   230 */ 365, 366, 367, 368, 369, 370, 371, 372, 373, 374, 
  /*   240 */ 16, 375, 16, 16, 376, 377, 385, 393, 394, 395, 
  /*   250 */ 396, 397, 398, 399, 407, 414, 420, 439, 440, 441, 
  /*   260 */ 16, NACL_OPCODE_NULL_OFFSET, 442, NACL_OPCODE_NULL_OFFSET, 443, 444, 16, 445, 16, 453, 
  /*   270 */ 454, 455, 456, 457, 459, 460, 461, 462, 464, 465, 
  /*   280 */ 473, 474, 474, 474, 474, 474, 474, 475, 476, 477, 
  /*   290 */ 478, 479, 16, 16, 16, 16, 480, 481, 482, 483, 
  /*   300 */ 484, 485, 486, 487, 488, 489, 490, 491, 492, 493, 
  /*   310 */ 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 
  /*   320 */ 494, 495, 496, 497, 498, 499, 500, 501, 502, 503, 
  /*   330 */ 504, 505, 506, 507, 508, 509, 510, 511, 512, 513, 
  /*   340 */ 514, 515, 516, 517, 518, 519, 520, 521, 522, 523, 
  /*   350 */ 524, 525, 526, 527, 528, 529, 530, 531, 532, 533, 
  /*   360 */ 534, 535, 536, 537, 16, 16, 538, 539, 540, 547, 
  /*   370 */ 554, 561, 562, 563, 564, 565, 16, 16, 16, 16, 
  /*   380 */ 16, 16, 566, 567, 568, 569, 570, 571, 572, 573, 
  /*   390 */ 574, 575, 576, 577, 578, 579, 580, 581, 582, 583, 
  /*   400 */ 584, 585, 586, 587, 588, 589, 590, 591, 592, 593, 
  /*   410 */ 594, 595, 596, 597, 598, 599, 600, 601, 602, 603, 
  /*   420 */ 604, 605, 16, 16, 606, 607, 608, 609, 610, 611, 
  /*   430 */ 641, 642, 643, 644, 645, 646, 647, 648, 649, 650, 
  /*   440 */ 16, 168, 654, 655, 656, 657, 658, 659, 660, 661, 
  /*   450 */ 662, 663, 664, 665, 666, 667, 669, 671, 673, 675, 
  /*   460 */ 677, 679, 681, 683, 16, 684, 685, 686, 687, 688, 
  /*   470 */ 16, 689, 690, 691, 692, 693, 694, 695, 696, 697, 
  /*   480 */ 698, 699, 700, 701, 702, 703, 16, 704, 705, 706, 
  /*   490 */ 707, 708, 709, 710, 711, 712, 16, 713, 714, 715, 
  /*   500 */ 716, 717, 718, 719, 720, 721, 722, 723, 724, 725, 
  /*   510 */ 726, 16, NACL_OPCODE_NULL_OFFSET, 727, 728, 729, 730, 730, 730, 730, 
  /*   520 */ 730, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   530 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 730, 730, 731, 
  /*   540 */ 732, 733, 734, 730, 730, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   550 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   560 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   570 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 730, 735, 730, 
  /*   580 */ 730, 730, 730, 730, 730, 736, 737, 738, 730, 739, 
  /*   590 */ 740, 741, 742, 730, 730, 730, 730, 730, 730, 730, 
  /*   600 */ 730, 730, 730, 730, 730, 730, 730, 730, 730, 743, 
  /*   610 */ 730, 730, 730, 730, 730, 730, 730, 744, 745, 730, 
  /*   620 */ 730, 746, 747, 730, 730, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   630 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   640 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   650 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   660 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   670 */ NACL_OPCODE_NULL_OFFSET, 730, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   680 */ NACL_OPCODE_NULL_OFFSET, 730, 730, 730, 730, 730, 730, 730, 730, NACL_OPCODE_NULL_OFFSET, 
  /*   690 */ NACL_OPCODE_NULL_OFFSET, 748, 730, 730, 730, 730, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   700 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 749, 730, 730, 730, 730, 
  /*   710 */ 730, 750, 730, 730, 730, 730, 730, 730, 730, 730, 
  /*   720 */ 730, 730, 730, 730, 730, 730, 730, 751, 730, 730, 
  /*   730 */ 730, 730, 730, 730, 730, 730, 730, 752, 730, 730, 
  /*   740 */ 730, 730, 730, 730, 730, 730, 730, 730, 730, 730, 
  /*   750 */ 730, 730, 730, NACL_OPCODE_NULL_OFFSET, 753, 754, 755, 756, 756, 756, 
  /*   760 */ 757, 756, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   770 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 756, 756, 
  /*   780 */ 758, 759, 760, 761, 756, 756, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   790 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   800 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   810 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 756, 762, 
  /*   820 */ 763, 764, 756, 756, 756, 756, 765, 766, 767, 768, 
  /*   830 */ 769, 770, 771, 772, 756, 756, 756, 756, 756, 756, 
  /*   840 */ 756, 756, 756, 756, 756, 756, 756, 756, 756, 773, 
  /*   850 */ 774, 756, 756, 756, 756, 756, 756, 756, 756, 756, 
  /*   860 */ 756, 756, 756, 756, 775, 776, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   870 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   880 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   890 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   900 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   910 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   920 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 777, 756, 756, 756, 756, 778, 756, 756, 
  /*   930 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 779, 756, 756, 756, 756, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   940 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 756, 756, 756, 756, 
  /*   950 */ 756, 756, 780, 756, 756, 756, 756, 756, 756, 756, 
  /*   960 */ 756, 756, 756, 756, 756, 756, 756, 756, 781, 756, 
  /*   970 */ 756, 756, 756, 756, 756, 756, 756, 756, 756, 756, 
  /*   980 */ 756, 756, 756, 756, 756, 756, 756, 756, 756, 756, 
  /*   990 */ 756, 756, 756, 756, NACL_OPCODE_NULL_OFFSET, 782, 783, 784, 785, 786, 
  /*  1000 */ 787, 788, 789, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1010 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 790, 
  /*  1020 */ 791, 792, 793, 794, 795, 796, 797, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1030 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1040 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1050 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 798, 
  /*  1060 */ 799, 800, 800, 801, 802, 803, 804, 805, 806, 807, 
  /*  1070 */ 808, 809, 810, 811, 812, 813, 814, 815, 816, 817, 
  /*  1080 */ 818, 819, 820, 821, 822, 823, 824, 825, 826, 827, 
  /*  1090 */ 828, 829, 837, 844, 852, 853, 854, 855, 800, 857, 
  /*  1100 */ 858, 800, 800, 859, 860, 861, 862, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1110 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1120 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1130 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1140 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1150 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 800, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1160 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1170 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 863, 800, 864, 865, 866, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1180 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 867, 868, 869, 
  /*  1190 */ 870, 871, 872, 873, 874, 875, 876, 877, 878, 879, 
  /*  1200 */ 880, 881, 882, 883, 884, 885, 886, 887, 888, 889, 
  /*  1210 */ 890, 891, 892, 893, 894, 895, 896, 897, 898, 800, 
  /*  1220 */ 899, 900, 901, 902, 903, 904, 905, 906, 907, 908, 
  /*  1230 */ 909, 910, 911, 912, 800, NACL_OPCODE_NULL_OFFSET, 913, 914, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1240 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1250 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 915, 916, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
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
  /*  1360 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 917, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 918, NACL_OPCODE_NULL_OFFSET, 919, NACL_OPCODE_NULL_OFFSET, 
  /*  1370 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 920, NACL_OPCODE_NULL_OFFSET, 921, 922, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 923, NACL_OPCODE_NULL_OFFSET, 
  /*  1380 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 924, NACL_OPCODE_NULL_OFFSET, 925, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 926, NACL_OPCODE_NULL_OFFSET, 
  /*  1390 */ 927, 928, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 929, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 930, NACL_OPCODE_NULL_OFFSET, 
  /*  1400 */ 931, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 932, NACL_OPCODE_NULL_OFFSET, 933, 934, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1410 */ NACL_OPCODE_NULL_OFFSET, 935, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 936, NACL_OPCODE_NULL_OFFSET, 937, 938, 939, 
  /*  1420 */ 940, 941, 942, 943, 944, 945, 946, 947, 948, 16, 
  /*  1430 */ 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 
  /*  1440 */ 16, 16, 16, 16, 16, 949, 950, 951, 16, 16, 
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
  /*  1650 */ 16, 16, 16, 16, 16, 16, 16, 952, 953, 16, 
  /*  1660 */ 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 
  /*  1670 */ 16, 16, 16, 954, 955, 956, 957, 958, 959, 960, 
  /*  1680 */ 961, 962, 963, 964, 965, 800, 800, 800, 800, 966, 
  /*  1690 */ 800, 800, 800, 967, 968, 800, 969, 800, 800, 800, 
  /*  1700 */ 800, 970, 971, 972, 800, 973, 974, 975, 976, 977, 
  /*  1710 */ 978, 800, 800, 979, 980, 981, 982, 800, 800, 800, 
  /*  1720 */ 800, 983, 984, 985, 986, 987, 988, 800, 989, 990, 
  /*  1730 */ 991, 992, 993, 994, 995, 996, 997, 998, 999, 800, 
  /*  1740 */ 800, 800, 800, 800, 800, 800, 800, 800, 800, 800, 
  /*  1750 */ 800, 800, 800, 800, 800, 800, 800, 800, 800, 800, 
  /*  1760 */ 800, 800, 800, 800, 800, 800, 800, 800, 800, 800, 
  /*  1770 */ 800, 800, 800, 800, 800, 800, 800, 800, 800, 800, 
  /*  1780 */ 800, 800, 800, 800, 800, 800, 800, 800, 800, 800, 
  /*  1790 */ 800, 800, 800, 800, 800, 800, 800, 800, 800, 800, 
  /*  1800 */ 800, 1000, 1001, 800, 800, 800, 800, 800, 800, 800, 
  /*  1810 */ 800, 800, 800, 800, 800, 800, 800, 800, 800, 800, 
  /*  1820 */ 800, 800, 800, 800, 800, 800, 800, 800, 800, 800, 
  /*  1830 */ 800, 800, 800, 800, 800, 800, 800, 800, 800, 800, 
  /*  1840 */ 800, 800, 800, 800, 800, 800, 800, 800, 800, 800, 
  /*  1850 */ 800, 800, 800, 800, 800, 800, 800, 800, 800, 800, 
  /*  1860 */ 800, 800, 800, 800, 800, 800, 800, 800, 800, 800, 
  /*  1870 */ 800, 800, 800, 800, 800, 800, 800, 800, 800, 800, 
  /*  1880 */ 800, 800, 800, 800, 800, 800, 800, 800, 800, 800, 
  /*  1890 */ 800, 800, 800, 800, 800, 800, 800, 800, 800, 800, 
  /*  1900 */ 800, 800, 800, 800, 800, 800, 800, 800, 800, 800, 
  /*  1910 */ 800, 800, 800, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 800, 800, 800, 800, 800, 
  /*  1920 */ 800, 800, 800, 800, 800, 800, 800, 800, 800, NACL_OPCODE_NULL_OFFSET, 
  /*  1930 */ 1002, 1003, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 1004, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 1005, 1006, 1007, 
  /*  1940 */ 1008, 1009, 1010, 1011, 1012, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 1013, 
  /*  1950 */ 1014, 1015, 1016, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1960 */ NACL_OPCODE_NULL_OFFSET, 1017, 1018, 1019, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1970 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1980 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1990 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 1020, 1021, 1022, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  2000 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  2010 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  2020 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 1023, 1024, 1025, 1026, NACL_OPCODE_NULL_OFFSET, 
  /*  2030 */ NACL_OPCODE_NULL_OFFSET, 1027, 1028, 1029, 1030, 1031, 1032, 1033, 1034, 1035, 
  /*  2040 */ 1036, 1037, 1038, 1039, 1040, 1041, 1042, 1043, 1044, 1045, 
  /*  2050 */ 1046, 1047, 1048, 1049, 1050, 1051, 1052, 1053, 1054, 1055, 
  /*  2060 */ 1056, 1057, 1058, 1059, 1060, 1061, 1062, 1063, 1064, 1065, 
  /*  2070 */ 1066, 1067, 1068, 1069, 1070, 1071, 1072, 1073, 1074, 1075, 
  /*  2080 */ 1076, 1077, 1078, 1079, 1080, 1081, 1082, 1083, 1084, 1085, 
  /*  2090 */ 1086, 1087, 1088, 1089, 1090, NACL_OPCODE_NULL_OFFSET, 1091, 1092, 1093, 1094, 
  /*  2100 */ 1095, 1096, 1097, 1098, 1099, 1100, 1101, 1102, 1103, 1104, 
  /*  2110 */ 1105, 1106, 1107, 16, 16, 16, 16, 16, 16, 16, 
  /*  2120 */ 16, 16, 16, 16, 16, 16, 16, 16, 1108, 1109, 
  /*  2130 */ 16, 16, 1110, 1111, 16, 16, 1112, 1113, 1114, 1115, 
  /*  2140 */ 1116, 1117, 1118, 16, 1119, 1120, 1121, 1122, 1123, 1124, 
  /*  2150 */ 1125, 1126, 1127, 1128, 1129, 1130, 1131, 1132, 1133, 1134, 
  /*  2160 */ NACL_OPCODE_NULL_OFFSET, 1135, 1136, 1137, 1138, 1139, 1140, 1141, 1142, 1143, 
  /*  2170 */ 1144, 1145, 1146, 1147, 1148, 1149, 1150, 1151, 1152, 1153, 
  /*  2180 */ 1154, 1155, 1156, 1157, 1158, 1159, 1160, 1161, 1162, 1163, 
  /*  2190 */ 1164, 1165, 1166, 16, 16, 16, 16, 16, 16, 16, 
  /*  2200 */ 16, 16, 1167, 16, 16, 16, 16, 16, 16, 16, 
  /*  2210 */ 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 
  /*  2220 */ 16, 16, 16, 16, 16, NACL_OPCODE_NULL_OFFSET, 1168, 1169, 1170, 1171, 
  /*  2230 */ 1172, 1173, 1174, 1175, 1176, 1177, 1178, 1179, 1180, 1181, 
  /*  2240 */ 1182, 1183, 1184, 1185, 1186, 1187, 1188, 1189, 1190, 1191, 
  /*  2250 */ 1192, 1193, 1194, 1195, 1196, 1197, 1198, 1199, 16, 16, 
  /*  2260 */ 1200, 1201, 16, 16, 16, 16, 1202, 1203, 1204, 1205, 
  /*  2270 */ 1206, 1207, 1208, 1209, 1210, 1211, 1212, 1213, 1214, 1215, 
  /*  2280 */ 1216, 1217, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 1027, 1218, 1219, 1220, 1221, 1222, 
  /*  2290 */ 1223, 1224, 1035, 1225, 1226, 1227, 1228, 1229, 1230, 1231, 
  /*  2300 */ 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 
  /*  2310 */ 16, 16, 16, 16, 16, 16, 1067, 1232, 1233, 1234, 
  /*  2320 */ 1235, 1236, 1237, 1238, 1059, 1239, 1240, 1241, 1242, 1243, 
  /*  2330 */ 1244, 1245, 1083, 1246, 1247, 1248, 1249, 1250, 1251, 1252, 
  /*  2340 */ 1075, 1253, 1254, 1255, 1256, 1257, 1258, 1259, NACL_OPCODE_NULL_OFFSET, 1260, 
  /*  2350 */ 1261, 1262, 1263, 1264, 1265, 1266, 1267, 16, 16, 16, 
  /*  2360 */ 16, 16, 16, 16, 16, 1268, 1269, 1270, 1271, 1272, 
  /*  2370 */ 1273, 1274, 1275, 1276, 1277, 1278, 1279, 1280, 1281, 1282, 
  /*  2380 */ 1283, 1284, 1285, 1286, 1287, 1288, 1289, 1290, 1291, 1292, 
  /*  2390 */ 1293, 1294, 1295, 1296, 1297, 1298, 1299, 16, 16, 16, 
  /*  2400 */ 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 
  /*  2410 */ 16, 16, 16, NACL_OPCODE_NULL_OFFSET, 1300, 1301, 1302, 1303, 1304, 1305, 
  /*  2420 */ 1306, 1307, 1308, 1309, 1310, 1311, 1312, 1313, 1314, 1315, 
  /*  2430 */ 16, 16, 16, 16, 16, 16, 16, 16, 16, 1316, 
  /*  2440 */ 16, 16, 16, 16, 16, 16, 1317, 1318, 1319, 1320, 
  /*  2450 */ 1321, 1322, 1323, 1324, 1325, 1326, 1327, 1328, 1329, 1330, 
  /*  2460 */ 1331, 1332, 1333, 1334, 1335, 1336, 1337, 1338, 1339, 1340, 
  /*  2470 */ 1341, 1342, 1343, 1344, 1345, 1346, 1347, 1348, NACL_OPCODE_NULL_OFFSET, 1349, 
  /*  2480 */ 1349, 1349, 1349, 1349, 1349, 1349, 1349, 1349, 1349, 1349, 
  /*  2490 */ 1349, 1349, 1349, 1349, 1349, 1349, 1349, 1349, 1349, 1349, 
  /*  2500 */ 1349, 1349, 1349, 1349, 1349, 1349, 1349, 1349, 1349, 1349, 
  /*  2510 */ 1349, 1350, 16, 16, 16, 16, 16, 16, 16, 1351, 
  /*  2520 */ 1352, 1353, 1354, 1355, 1356, 1357, 1358, 1359, 1360, 1361, 
  /*  2530 */ 1362, 1363, 1364, 1365, 1366, 16, 16, 16, 16, 16, 
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
    1367,
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
    1368,
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
    1368,
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
    1368,
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
    1368,
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
    1368,
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
    1368,
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
    1368,
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
    1368,
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
    1368,
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
    1368,
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
    1368,
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
    1368,
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
    1368,
    NULL,
    NULL,
  },
  /* 91 */
  { 0x90,
    1368,
    NULL,
    NULL,
  },
  /* 92 */
  { 0x90,
    1368,
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
    1369,
    NULL,
    NULL,
  },
};
