/*
 * THIS FILE IS AUTO-GENERATED. DO NOT EDIT.
 * Compiled for x86-64 bit mode.
 *
 * You must include ncopcode_desc.h before this file.
 */

static const NaClOp g_Operands[741] = {
  /* 0 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Eb" },
  /* 1 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gb" },
  /* 2 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OperandZeroExtends_v), "$Ev" },
  /* 3 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gv" },
  /* 4 */ { G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Gb" },
  /* 5 */ { E_Operand, NACL_OPFLAG(OpUse), "$Eb" },
  /* 6 */ { G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OperandZeroExtends_v), "$Gv" },
  /* 7 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ev" },
  /* 8 */ { RegAL, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%al" },
  /* 9 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 10 */ { RegREAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OperandZeroExtends_v), "$rAXv" },
  /* 11 */ { I_Operand, NACL_OPFLAG(OpUse), "$Iz" },
  /* 12 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Ev" },
  /* 13 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gv" },
  /* 14 */ { G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Gv" },
  /* 15 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ev" },
  /* 16 */ { RegREAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$rAXv" },
  /* 17 */ { I_Operand, NACL_OPFLAG(OpUse), "$Iz" },
  /* 18 */ { E_Operand, NACL_OPFLAG(OpUse), "$Eb" },
  /* 19 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gb" },
  /* 20 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ev" },
  /* 21 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gv" },
  /* 22 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gb" },
  /* 23 */ { E_Operand, NACL_OPFLAG(OpUse), "$Eb" },
  /* 24 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gv" },
  /* 25 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ev" },
  /* 26 */ { RegAL, NACL_OPFLAG(OpUse), "%al" },
  /* 27 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 28 */ { RegREAX, NACL_OPFLAG(OpUse), "$rAXv" },
  /* 29 */ { I_Operand, NACL_OPFLAG(OpUse), "$Iz" },
  /* 30 */ { RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rsp}" },
  /* 31 */ { G_OpcodeBase, NACL_OPFLAG(OpUse), "$r8v" },
  /* 32 */ { RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rsp}" },
  /* 33 */ { G_OpcodeBase, NACL_OPFLAG(OpSet), "$r8v" },
  /* 34 */ { G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OperandZeroExtends_v), "$Gv" },
  /* 35 */ { Ev_Operand, NACL_OPFLAG(OpUse), "$Ed" },
  /* 36 */ { RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rsp}" },
  /* 37 */ { I_Operand, NACL_OPFLAG(OpUse), "$Iz" },
  /* 38 */ { G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OperandZeroExtends_v), "$Gv" },
  /* 39 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ev" },
  /* 40 */ { I_Operand, NACL_OPFLAG(OpUse), "$Iz" },
  /* 41 */ { RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rsp}" },
  /* 42 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 43 */ { G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OperandZeroExtends_v), "$Gv" },
  /* 44 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ev" },
  /* 45 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 46 */ { RegES_EDI, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$Yb}" },
  /* 47 */ { RegDX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%dx}" },
  /* 48 */ { RegES_EDI, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$Yzd}" },
  /* 49 */ { RegDX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%dx}" },
  /* 50 */ { RegES_EDI, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$Yzw}" },
  /* 51 */ { RegDX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%dx}" },
  /* 52 */ { RegDX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%dx}" },
  /* 53 */ { RegDS_ESI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Xb}" },
  /* 54 */ { RegDX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%dx}" },
  /* 55 */ { RegDS_ESI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Xzd}" },
  /* 56 */ { RegDX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%dx}" },
  /* 57 */ { RegDS_ESI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Xzw}" },
  /* 58 */ { RegRIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rip}" },
  /* 59 */ { J_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandNear) | NACL_OPFLAG(OperandRelative), "$Jb" },
  /* 60 */ { E_Operand, NACL_OPFLAG(OpUse), "$Eb" },
  /* 61 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 62 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Eb" },
  /* 63 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 64 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OperandZeroExtends_v), "$Ev" },
  /* 65 */ { I_Operand, NACL_OPFLAG(OpUse), "$Iz" },
  /* 66 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Ev" },
  /* 67 */ { I_Operand, NACL_OPFLAG(OpUse), "$Iz" },
  /* 68 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OperandZeroExtends_v), "$Ev" },
  /* 69 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 70 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Ev" },
  /* 71 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 72 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Eb" },
  /* 73 */ { G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Gb" },
  /* 74 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OperandZeroExtends_v), "$Ev" },
  /* 75 */ { G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OperandZeroExtends_v), "$Gv" },
  /* 76 */ { E_Operand, NACL_OPFLAG(OpSet), "$Eb" },
  /* 77 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gb" },
  /* 78 */ { E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OperandZeroExtends_v), "$Ev" },
  /* 79 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gv" },
  /* 80 */ { G_Operand, NACL_OPFLAG(OpSet), "$Gb" },
  /* 81 */ { E_Operand, NACL_OPFLAG(OpUse), "$Eb" },
  /* 82 */ { E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OperandZeroExtends_v), "$Mw/Rv" },
  /* 83 */ { S_Operand, NACL_OPFLAG(OpUse), "$Sw" },
  /* 84 */ { G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OperandZeroExtends_v), "$Gv" },
  /* 85 */ { M_Operand, NACL_OPFLAG(OpAddress), "$M" },
  /* 86 */ { S_Operand, NACL_OPFLAG(OpSet), "$Sw" },
  /* 87 */ { Ew_Operand, NACL_OPFLAG(OpUse), "$Ew" },
  /* 88 */ { RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rsp}" },
  /* 89 */ { E_Operand, NACL_OPFLAG(OpSet), "$Ev" },
  /* 90 */ { G_OpcodeBase, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OperandZeroExtends_v), "$r8v" },
  /* 91 */ { RegREAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OperandZeroExtends_v), "$rAXv" },
  /* 92 */ { RegRAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rax}" },
  /* 93 */ { RegEAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%eax}" },
  /* 94 */ { RegEAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit) | NACL_OPFLAG(OperandSignExtends_v), "{%eax}" },
  /* 95 */ { RegAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%ax}" },
  /* 96 */ { RegAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%ax}" },
  /* 97 */ { RegAL, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%al}" },
  /* 98 */ { RegRDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rdx}" },
  /* 99 */ { RegRAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%rax}" },
  /* 100 */ { RegEDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%edx}" },
  /* 101 */ { RegEAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%eax}" },
  /* 102 */ { RegDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%dx}" },
  /* 103 */ { RegAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%ax}" },
  /* 104 */ { RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rsp}" },
  /* 105 */ { RegRFLAGS, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Fvq}" },
  /* 106 */ { RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rsp}" },
  /* 107 */ { RegRFLAGS, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$Fvw}" },
  /* 108 */ { RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rsp}" },
  /* 109 */ { RegRFLAGS, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$Fvq}" },
  /* 110 */ { RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rsp}" },
  /* 111 */ { RegRFLAGS, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$Fvw}" },
  /* 112 */ { RegAH, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%ah}" },
  /* 113 */ { RegAH, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%ah}" },
  /* 114 */ { RegAL, NACL_OPFLAG(OpSet), "%al" },
  /* 115 */ { O_Operand, NACL_OPFLAG(OpUse), "$Ob" },
  /* 116 */ { RegREAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OperandZeroExtends_v), "$rAXv" },
  /* 117 */ { O_Operand, NACL_OPFLAG(OpUse), "$Ov" },
  /* 118 */ { O_Operand, NACL_OPFLAG(OpSet), "$Ob" },
  /* 119 */ { RegAL, NACL_OPFLAG(OpUse), "%al" },
  /* 120 */ { O_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OperandZeroExtends_v), "$Ov" },
  /* 121 */ { RegREAX, NACL_OPFLAG(OpUse), "$rAXv" },
  /* 122 */ { RegES_EDI, NACL_OPFLAG(OpSet), "$Yb" },
  /* 123 */ { RegDS_ESI, NACL_OPFLAG(OpUse), "$Xb" },
  /* 124 */ { RegES_EDI, NACL_OPFLAG(OpSet), "$Yvq" },
  /* 125 */ { RegDS_ESI, NACL_OPFLAG(OpUse), "$Xvq" },
  /* 126 */ { RegES_EDI, NACL_OPFLAG(OpSet), "$Yvd" },
  /* 127 */ { RegDS_ESI, NACL_OPFLAG(OpUse), "$Xvd" },
  /* 128 */ { RegES_EDI, NACL_OPFLAG(OpSet), "$Yvw" },
  /* 129 */ { RegDS_ESI, NACL_OPFLAG(OpUse), "$Xvw" },
  /* 130 */ { RegES_EDI, NACL_OPFLAG(OpUse), "$Yb" },
  /* 131 */ { RegDS_ESI, NACL_OPFLAG(OpUse), "$Xb" },
  /* 132 */ { RegES_EDI, NACL_OPFLAG(OpUse), "$Yvq" },
  /* 133 */ { RegDS_ESI, NACL_OPFLAG(OpUse), "$Xvq" },
  /* 134 */ { RegES_EDI, NACL_OPFLAG(OpUse), "$Yvd" },
  /* 135 */ { RegDS_ESI, NACL_OPFLAG(OpUse), "$Xvd" },
  /* 136 */ { RegES_EDI, NACL_OPFLAG(OpUse), "$Yvw" },
  /* 137 */ { RegDS_ESI, NACL_OPFLAG(OpUse), "$Xvw" },
  /* 138 */ { RegES_EDI, NACL_OPFLAG(OpSet), "$Yb" },
  /* 139 */ { RegAL, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%al}" },
  /* 140 */ { RegES_EDI, NACL_OPFLAG(OpSet), "$Yvq" },
  /* 141 */ { RegRAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$rAXvq}" },
  /* 142 */ { RegES_EDI, NACL_OPFLAG(OpSet), "$Yvd" },
  /* 143 */ { RegEAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$rAXvd}" },
  /* 144 */ { RegES_EDI, NACL_OPFLAG(OpSet), "$Yvw" },
  /* 145 */ { RegAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$rAXvw}" },
  /* 146 */ { RegAL, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%al}" },
  /* 147 */ { RegDS_ESI, NACL_OPFLAG(OpUse), "$Xb" },
  /* 148 */ { RegRAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$rAXvq}" },
  /* 149 */ { RegDS_ESI, NACL_OPFLAG(OpUse), "$Xvq" },
  /* 150 */ { RegEAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$rAXvd}" },
  /* 151 */ { RegDS_ESI, NACL_OPFLAG(OpUse), "$Xvd" },
  /* 152 */ { RegAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$rAXvw}" },
  /* 153 */ { RegDS_ESI, NACL_OPFLAG(OpUse), "$Xvw" },
  /* 154 */ { RegAL, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%al}" },
  /* 155 */ { RegES_EDI, NACL_OPFLAG(OpUse), "$Yb" },
  /* 156 */ { RegRAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$rAXvq}" },
  /* 157 */ { RegES_EDI, NACL_OPFLAG(OpUse), "$Yvq" },
  /* 158 */ { RegEAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$rAXvd}" },
  /* 159 */ { RegES_EDI, NACL_OPFLAG(OpUse), "$Yvd" },
  /* 160 */ { RegAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{$rAXvw}" },
  /* 161 */ { RegES_EDI, NACL_OPFLAG(OpUse), "$Yvw" },
  /* 162 */ { G_OpcodeBase, NACL_OPFLAG(OpSet), "$r8b" },
  /* 163 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 164 */ { G_OpcodeBase, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OperandZeroExtends_v), "$r8v" },
  /* 165 */ { I_Operand, NACL_OPFLAG(OpUse), "$Iv" },
  /* 166 */ { RegRIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rip}" },
  /* 167 */ { RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rsp}" },
  /* 168 */ { I_Operand, NACL_OPFLAG(OpUse), "$Iw" },
  /* 169 */ { E_Operand, NACL_OPFLAG(OpSet), "$Eb" },
  /* 170 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 171 */ { E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OperandZeroExtends_v), "$Ev" },
  /* 172 */ { I_Operand, NACL_OPFLAG(OpUse), "$Iz" },
  /* 173 */ { RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rsp}" },
  /* 174 */ { RegRBP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rbp}" },
  /* 175 */ { I_Operand, NACL_OPFLAG(OpUse), "$Iw" },
  /* 176 */ { I2_Operand, NACL_OPFLAG(OpUse), "$I2b" },
  /* 177 */ { RegRSP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rsp}" },
  /* 178 */ { RegRBP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rbp}" },
  /* 179 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Eb" },
  /* 180 */ { Const_1, NACL_OPFLAG(OpUse), "1" },
  /* 181 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Ev" },
  /* 182 */ { Const_1, NACL_OPFLAG(OpUse), "1" },
  /* 183 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Eb" },
  /* 184 */ { RegCL, NACL_OPFLAG(OpUse), "%cl" },
  /* 185 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Ev" },
  /* 186 */ { RegCL, NACL_OPFLAG(OpUse), "%cl" },
  /* 187 */ { RegAL, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%al}" },
  /* 188 */ { RegDS_EBX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%DS_EBX}" },
  /* 189 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st0" },
  /* 190 */ { Mv_Operand, NACL_OPFLAG(OpUse), "$Md" },
  /* 191 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 192 */ { Mv_Operand, NACL_OPFLAG(OpUse), "$Md" },
  /* 193 */ { Mw_Operand, NACL_OPFLAG(OpSet), "$Mw" },
  /* 194 */ { M_Operand, NACL_OPFLAG(OpSet), "$Mf" },
  /* 195 */ { Mw_Operand, NACL_OPFLAG(OpUse), "$Mw" },
  /* 196 */ { M_Operand, NACL_OPFLAG(OpUse), "$Mf" },
  /* 197 */ { Mv_Operand, NACL_OPFLAG(OpSet), "$Md" },
  /* 198 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 199 */ { RegST0, NACL_OPFLAG(OpSet), "%st0" },
  /* 200 */ { Mv_Operand, NACL_OPFLAG(OpUse), "$Md" },
  /* 201 */ { M_Operand, NACL_OPFLAG(OpSet), "$Mf" },
  /* 202 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 203 */ { RegST0, NACL_OPFLAG(OpSet), "%st0" },
  /* 204 */ { M_Operand, NACL_OPFLAG(OpUse), "$Mf" },
  /* 205 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st0" },
  /* 206 */ { Mo_Operand, NACL_OPFLAG(OpUse), "$Mq" },
  /* 207 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 208 */ { Mo_Operand, NACL_OPFLAG(OpUse), "$Mq" },
  /* 209 */ { Mo_Operand, NACL_OPFLAG(OpSet), "$Mq" },
  /* 210 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 211 */ { RegST0, NACL_OPFLAG(OpSet), "%st0" },
  /* 212 */ { Mo_Operand, NACL_OPFLAG(OpUse), "$Mq" },
  /* 213 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st0" },
  /* 214 */ { Mw_Operand, NACL_OPFLAG(OpUse), "$Mw" },
  /* 215 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 216 */ { Mw_Operand, NACL_OPFLAG(OpUse), "$Mw" },
  /* 217 */ { Mw_Operand, NACL_OPFLAG(OpSet), "$Mw" },
  /* 218 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 219 */ { RegST0, NACL_OPFLAG(OpSet), "%st0" },
  /* 220 */ { Mw_Operand, NACL_OPFLAG(OpUse), "$Mw" },
  /* 221 */ { RegRIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rip}" },
  /* 222 */ { RegRCX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rcx}" },
  /* 223 */ { J_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandNear) | NACL_OPFLAG(OperandRelative), "$Jb" },
  /* 224 */ { RegRIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rip}" },
  /* 225 */ { RegECX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%ecx}" },
  /* 226 */ { J_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandNear) | NACL_OPFLAG(OperandRelative), "$Jb" },
  /* 227 */ { RegRIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rip}" },
  /* 228 */ { RegRCX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%rcx}" },
  /* 229 */ { J_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandNear) | NACL_OPFLAG(OperandRelative), "$Jb" },
  /* 230 */ { RegRIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rip}" },
  /* 231 */ { RegECX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%ecx}" },
  /* 232 */ { J_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandNear) | NACL_OPFLAG(OperandRelative), "$Jb" },
  /* 233 */ { RegAL, NACL_OPFLAG(OpSet), "%al" },
  /* 234 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 235 */ { RegREAX, NACL_OPFLAG(OpSet), "$rAXv" },
  /* 236 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 237 */ { I_Operand, NACL_OPFLAG(OpSet), "$Ib" },
  /* 238 */ { RegAL, NACL_OPFLAG(OpUse), "%al" },
  /* 239 */ { I_Operand, NACL_OPFLAG(OpSet), "$Ib" },
  /* 240 */ { RegREAX, NACL_OPFLAG(OpUse), "$rAXv" },
  /* 241 */ { RegRIP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rip}" },
  /* 242 */ { RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rsp}" },
  /* 243 */ { J_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandNear) | NACL_OPFLAG(OperandRelative), "$Jzd" },
  /* 244 */ { RegRIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rip}" },
  /* 245 */ { J_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandNear) | NACL_OPFLAG(OperandRelative), "$Jzd" },
  /* 246 */ { RegAL, NACL_OPFLAG(OpSet), "%al" },
  /* 247 */ { RegDX, NACL_OPFLAG(OpUse), "%dx" },
  /* 248 */ { RegREAX, NACL_OPFLAG(OpSet), "$rAXv" },
  /* 249 */ { RegDX, NACL_OPFLAG(OpUse), "%dx" },
  /* 250 */ { RegDX, NACL_OPFLAG(OpSet), "%dx" },
  /* 251 */ { RegAL, NACL_OPFLAG(OpUse), "%al" },
  /* 252 */ { RegDX, NACL_OPFLAG(OpSet), "%dx" },
  /* 253 */ { RegREAX, NACL_OPFLAG(OpUse), "$rAXv" },
  /* 254 */ { RegAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%ax}" },
  /* 255 */ { RegAL, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%al}" },
  /* 256 */ { E_Operand, NACL_OPFLAG(OpUse), "$Eb" },
  /* 257 */ { RegREDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%redx}" },
  /* 258 */ { RegREAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%reax}" },
  /* 259 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ev" },
  /* 260 */ { RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rsp}" },
  /* 261 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ev" },
  /* 262 */ { RegRIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rip}" },
  /* 263 */ { M_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandFar), "$Mp" },
  /* 264 */ { RegRIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rip}" },
  /* 265 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandNear), "$Ev" },
  /* 266 */ { RegRIP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rip}" },
  /* 267 */ { RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rsp}" },
  /* 268 */ { M_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandFar), "$Mp" },
  /* 269 */ { RegRIP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rip}" },
  /* 270 */ { RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rsp}" },
  /* 271 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandNear), "$Ev" },
  /* 272 */ { Ew_Operand, NACL_EMPTY_OPFLAGS, "$Ew" },
  /* 273 */ { E_Operand, NACL_OPFLAG(OpSet), "$Mw/Rv" },
  /* 274 */ { RegRDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rdx}" },
  /* 275 */ { RegRAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rax}" },
  /* 276 */ { RegRCX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rcx}" },
  /* 277 */ { RegGS, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%gs}" },
  /* 278 */ { Mb_Operand, NACL_OPFLAG(OpUse), "$Mb" },
  /* 279 */ { RegREAXa, NACL_OPFLAG(OpUse), "$rAXva" },
  /* 280 */ { RegECX, NACL_OPFLAG(OpUse), "%ecx" },
  /* 281 */ { RegEIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eip}" },
  /* 282 */ { RegEAX, NACL_OPFLAG(OpUse), "%eax" },
  /* 283 */ { M_Operand, NACL_OPFLAG(OpUse), "$Ms" },
  /* 284 */ { RegEAX, NACL_EMPTY_OPFLAGS, "%eax" },
  /* 285 */ { RegECX, NACL_EMPTY_OPFLAGS, "%ecx" },
  /* 286 */ { RegREAX, NACL_OPFLAG(OpUse), "%reax" },
  /* 287 */ { RegECX, NACL_OPFLAG(OpUse), "%ecx" },
  /* 288 */ { RegEDX, NACL_OPFLAG(OpUse), "%edx" },
  /* 289 */ { M_Operand, NACL_OPFLAG(OpSet), "$Ms" },
  /* 290 */ { G_Operand, NACL_EMPTY_OPFLAGS, "$Gv" },
  /* 291 */ { Ew_Operand, NACL_EMPTY_OPFLAGS, "$Ew" },
  /* 292 */ { RegRIP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rip}" },
  /* 293 */ { RegRCX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rcx}" },
  /* 294 */ { Mb_Operand, NACL_EMPTY_OPFLAGS, "$Mb" },
  /* 295 */ { Mmx_G_Operand, NACL_EMPTY_OPFLAGS, "$Pq" },
  /* 296 */ { Mmx_E_Operand, NACL_EMPTY_OPFLAGS, "$Qq" },
  /* 297 */ { I_Operand, NACL_EMPTY_OPFLAGS, "$Ib" },
  /* 298 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vps" },
  /* 299 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wps" },
  /* 300 */ { Xmm_E_Operand, NACL_OPFLAG(OpSet), "$Wps" },
  /* 301 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vps" },
  /* 302 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vps" },
  /* 303 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$VRq" },
  /* 304 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vps" },
  /* 305 */ { Mo_Operand, NACL_OPFLAG(OpUse), "$Mq" },
  /* 306 */ { Mo_Operand, NACL_OPFLAG(OpSet), "$Mq" },
  /* 307 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vps" },
  /* 308 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Vps" },
  /* 309 */ { Xmm_Eo_Operand, NACL_OPFLAG(OpUse), "$Wq" },
  /* 310 */ { Eo_Operand, NACL_OPFLAG(OpSet), "$Rd/q" },
  /* 311 */ { C_Operand, NACL_OPFLAG(OpUse), "$Cd/q" },
  /* 312 */ { Eo_Operand, NACL_OPFLAG(OpSet), "$Rd/q" },
  /* 313 */ { D_Operand, NACL_OPFLAG(OpUse), "$Dd/q" },
  /* 314 */ { C_Operand, NACL_OPFLAG(OpSet), "$Cd/q" },
  /* 315 */ { Eo_Operand, NACL_OPFLAG(OpUse), "$Rd/q" },
  /* 316 */ { D_Operand, NACL_OPFLAG(OpSet), "$Dd/q" },
  /* 317 */ { Eo_Operand, NACL_OPFLAG(OpUse), "$Rd/q" },
  /* 318 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vps" },
  /* 319 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse), "$Qq" },
  /* 320 */ { Mdq_Operand, NACL_OPFLAG(OpSet), "$Mdq" },
  /* 321 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vps" },
  /* 322 */ { Mmx_G_Operand, NACL_OPFLAG(OpSet), "$Pq" },
  /* 323 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wps" },
  /* 324 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vss" },
  /* 325 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wss" },
  /* 326 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vps" },
  /* 327 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wps" },
  /* 328 */ { RegEAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%eax}" },
  /* 329 */ { RegEDX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%edx}" },
  /* 330 */ { RegECX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%ecx}" },
  /* 331 */ { RegEAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eax}" },
  /* 332 */ { RegEDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%edx}" },
  /* 333 */ { RegEAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eax}" },
  /* 334 */ { RegEDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%edx}" },
  /* 335 */ { RegECX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%ecx}" },
  /* 336 */ { RegEIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eip}" },
  /* 337 */ { RegESP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 338 */ { RegCS, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%cs}" },
  /* 339 */ { RegSS, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%ss}" },
  /* 340 */ { RegEIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eip}" },
  /* 341 */ { RegESP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 342 */ { RegCS, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%cs}" },
  /* 343 */ { RegSS, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%ss}" },
  /* 344 */ { RegEDX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%edx}" },
  /* 345 */ { RegECX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit), "{%ecx}" },
  /* 346 */ { G_Operand, NACL_OPFLAG(OpSet), "$Gv" },
  /* 347 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ev" },
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
  /* 361 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ed/q/q" },
  /* 362 */ { Mmx_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OperandZeroExtends_v), "$Pq" },
  /* 363 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ed/q/d" },
  /* 364 */ { Mmx_G_Operand, NACL_OPFLAG(OpSet), "$Pq" },
  /* 365 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse), "$Qq" },
  /* 366 */ { Mmx_G_Operand, NACL_OPFLAG(OpSet), "$Pq" },
  /* 367 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse), "$Qq" },
  /* 368 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 369 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$PRq" },
  /* 370 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 371 */ { E_Operand, NACL_OPFLAG(OpSet), "$Ed/q/q" },
  /* 372 */ { Mmx_G_Operand, NACL_OPFLAG(OpUse), "$Pd/q/q" },
  /* 373 */ { E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OperandZeroExtends_v), "$Ed/q/d" },
  /* 374 */ { Mmx_G_Operand, NACL_OPFLAG(OpUse), "$Pd/q/d" },
  /* 375 */ { Mmx_E_Operand, NACL_OPFLAG(OpSet), "$Qq" },
  /* 376 */ { Mmx_G_Operand, NACL_OPFLAG(OpUse), "$Pq" },
  /* 377 */ { RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rsp}" },
  /* 378 */ { RegFS, NACL_OPFLAG(OpUse), "%fs" },
  /* 379 */ { RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rsp}" },
  /* 380 */ { RegFS, NACL_OPFLAG(OpSet), "%fs" },
  /* 381 */ { RegEBX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%ebx}" },
  /* 382 */ { RegEDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%edx}" },
  /* 383 */ { RegEAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eax}" },
  /* 384 */ { RegECX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%ecx}" },
  /* 385 */ { E_Operand, NACL_OPFLAG(OpSet), "$Ev" },
  /* 386 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gv" },
  /* 387 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 388 */ { E_Operand, NACL_OPFLAG(OpSet), "$Ev" },
  /* 389 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gv" },
  /* 390 */ { RegCL, NACL_OPFLAG(OpUse), "%cl" },
  /* 391 */ { RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rsp}" },
  /* 392 */ { RegGS, NACL_OPFLAG(OpUse), "%gs" },
  /* 393 */ { RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rsp}" },
  /* 394 */ { RegGS, NACL_OPFLAG(OpSet), "%gs" },
  /* 395 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Ev" },
  /* 396 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gv" },
  /* 397 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 398 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Ev" },
  /* 399 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gv" },
  /* 400 */ { RegCL, NACL_OPFLAG(OpUse), "%cl" },
  /* 401 */ { RegAL, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%al}" },
  /* 402 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Eb" },
  /* 403 */ { G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Gb" },
  /* 404 */ { RegREAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$rAXv}" },
  /* 405 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Ev" },
  /* 406 */ { G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Gv" },
  /* 407 */ { Seg_G_Operand, NACL_OPFLAG(OpSet), "$SGz" },
  /* 408 */ { M_Operand, NACL_OPFLAG(OperandFar), "$Mp" },
  /* 409 */ { G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OperandZeroExtends_v), "$Gv" },
  /* 410 */ { Eb_Operand, NACL_OPFLAG(OpUse), "$Eb" },
  /* 411 */ { G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OperandZeroExtends_v), "$Gv" },
  /* 412 */ { Ew_Operand, NACL_OPFLAG(OpUse), "$Ew" },
  /* 413 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Vps" },
  /* 414 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wps" },
  /* 415 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 416 */ { M_Operand, NACL_OPFLAG(OpSet), "$Md/q" },
  /* 417 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gd/q" },
  /* 418 */ { Mmx_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Pq" },
  /* 419 */ { E_Operand, NACL_OPFLAG(OpUse), "$Rd/q/Mw" },
  /* 420 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 421 */ { Gv_Operand, NACL_OPFLAG(OpSet), "$Gd" },
  /* 422 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse), "$PRq" },
  /* 423 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 424 */ { RegRDX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%rdx}" },
  /* 425 */ { RegEAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eax}" },
  /* 426 */ { Mdq_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Mdq" },
  /* 427 */ { RegEDX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%edx}" },
  /* 428 */ { RegEAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eax}" },
  /* 429 */ { Mo_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Mq" },
  /* 430 */ { G_OpcodeBase, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$r8vq" },
  /* 431 */ { G_OpcodeBase, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$r8vd" },
  /* 432 */ { Mo_Operand, NACL_OPFLAG(OpSet), "$Mq" },
  /* 433 */ { Mmx_G_Operand, NACL_OPFLAG(OpUse), "$Pq" },
  /* 434 */ { RegDS_EDI, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$Zvd}" },
  /* 435 */ { Mmx_G_Operand, NACL_OPFLAG(OpUse), "$Pq" },
  /* 436 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse), "$PRq" },
  /* 437 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vsd" },
  /* 438 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wsd" },
  /* 439 */ { Xmm_E_Operand, NACL_OPFLAG(OpSet), "$Wsd" },
  /* 440 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vsd" },
  /* 441 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vpd" },
  /* 442 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wsd" },
  /* 443 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vsd" },
  /* 444 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ed/q" },
  /* 445 */ { Mo_Operand, NACL_OPFLAG(OpSet), "$Mq" },
  /* 446 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vsd" },
  /* 447 */ { G_Operand, NACL_OPFLAG(OpSet), "$Gd/q" },
  /* 448 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wsd" },
  /* 449 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Vsd" },
  /* 450 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wsd" },
  /* 451 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vss" },
  /* 452 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wsd" },
  /* 453 */ { Xmm_Go_Operand, NACL_OPFLAG(OpSet), "$Vq" },
  /* 454 */ { Xmm_Eo_Operand, NACL_OPFLAG(OpUse), "$Wq" },
  /* 455 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 456 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Vdq" },
  /* 457 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$VRq" },
  /* 458 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 459 */ { I2_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 460 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Vdq" },
  /* 461 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$VRdq" },
  /* 462 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Vsd" },
  /* 463 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wsd" },
  /* 464 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 465 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Vpd" },
  /* 466 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wpd" },
  /* 467 */ { Mmx_G_Operand, NACL_OPFLAG(OpSet), "$Pq" },
  /* 468 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$VRq" },
  /* 469 */ { Xmm_Go_Operand, NACL_OPFLAG(OpSet), "$Vq" },
  /* 470 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wpd" },
  /* 471 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vdq" },
  /* 472 */ { Mdq_Operand, NACL_OPFLAG(OpUse), "$Mdq" },
  /* 473 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vss" },
  /* 474 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wss" },
  /* 475 */ { Xmm_E_Operand, NACL_OPFLAG(OpSet), "$Wss" },
  /* 476 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vss" },
  /* 477 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vss" },
  /* 478 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ed/q" },
  /* 479 */ { Mv_Operand, NACL_OPFLAG(OpSet), "$Md" },
  /* 480 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vss" },
  /* 481 */ { G_Operand, NACL_OPFLAG(OpSet), "$Gd/q" },
  /* 482 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wss" },
  /* 483 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Vss" },
  /* 484 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wss" },
  /* 485 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vsd" },
  /* 486 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wss" },
  /* 487 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vdq" },
  /* 488 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wps" },
  /* 489 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vdq" },
  /* 490 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 491 */ { Xmm_E_Operand, NACL_OPFLAG(OpSet), "$Wdq" },
  /* 492 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 493 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Vss" },
  /* 494 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wss" },
  /* 495 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 496 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vdq" },
  /* 497 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse), "$PRq" },
  /* 498 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vpd" },
  /* 499 */ { Xmm_Eo_Operand, NACL_OPFLAG(OpUse), "$Wq" },
  /* 500 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vpd" },
  /* 501 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wpd" },
  /* 502 */ { Xmm_E_Operand, NACL_OPFLAG(OpSet), "$Wpd" },
  /* 503 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vpd" },
  /* 504 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vsd" },
  /* 505 */ { Mo_Operand, NACL_OPFLAG(OpUse), "$Mq" },
  /* 506 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Vpd" },
  /* 507 */ { Xmm_Eo_Operand, NACL_OPFLAG(OpUse), "$Wq" },
  /* 508 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vpd" },
  /* 509 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse), "$Qq" },
  /* 510 */ { Mdq_Operand, NACL_OPFLAG(OpSet), "$Mdq" },
  /* 511 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vpd" },
  /* 512 */ { Mmx_G_Operand, NACL_OPFLAG(OpSet), "$Pq" },
  /* 513 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wpd" },
  /* 514 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vsd" },
  /* 515 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wsd" },
  /* 516 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vpd" },
  /* 517 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wsd" },
  /* 518 */ { Gv_Operand, NACL_OPFLAG(OpSet), "$Gd" },
  /* 519 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$VRpd" },
  /* 520 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vps" },
  /* 521 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wpd" },
  /* 522 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Vdq" },
  /* 523 */ { Xmm_Eo_Operand, NACL_OPFLAG(OpUse), "$Wq" },
  /* 524 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Vdq" },
  /* 525 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 526 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vdq" },
  /* 527 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ed/q/q" },
  /* 528 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OperandZeroExtends_v), "$Vdq" },
  /* 529 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ed/q/d" },
  /* 530 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vdq" },
  /* 531 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 532 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 533 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$VRdq" },
  /* 534 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 535 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(AllowGOperandWithOpcodeInModRm), "$Vdq" },
  /* 536 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 537 */ { I2_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 538 */ { E_Operand, NACL_OPFLAG(OpSet), "$Ed/q/q" },
  /* 539 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vd/q/q" },
  /* 540 */ { E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OperandZeroExtends_v), "$Ed/q/d" },
  /* 541 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vd/q/d" },
  /* 542 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Vpd" },
  /* 543 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wpd" },
  /* 544 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 545 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vdq" },
  /* 546 */ { E_Operand, NACL_OPFLAG(OpUse), "$Rd/q/Mw" },
  /* 547 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 548 */ { Gv_Operand, NACL_OPFLAG(OpSet), "$Gd" },
  /* 549 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$VRdq" },
  /* 550 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 551 */ { Xmm_Eo_Operand, NACL_OPFLAG(OpSet), "$Wq" },
  /* 552 */ { Xmm_Go_Operand, NACL_OPFLAG(OpUse), "$Vq" },
  /* 553 */ { Xmm_Go_Operand, NACL_OPFLAG(OpSet), "$Vq" },
  /* 554 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 555 */ { Mdq_Operand, NACL_OPFLAG(OpSet), "$Mdq" },
  /* 556 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 557 */ { RegDS_EDI, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$Zvd}" },
  /* 558 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 559 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$VRdq" },
  /* 560 */ { G_Operand, NACL_OPFLAG(OpSet), "$Gv" },
  /* 561 */ { M_Operand, NACL_OPFLAG(OpUse), "$Mv" },
  /* 562 */ { M_Operand, NACL_OPFLAG(OpSet), "$Mv" },
  /* 563 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gv" },
  /* 564 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vdq" },
  /* 565 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 566 */ { RegXMM0, NACL_OPFLAG(OpUse), "%xmm0" },
  /* 567 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 568 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 569 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vdq" },
  /* 570 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Udq/Mq" },
  /* 571 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vdq" },
  /* 572 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Udq/Md" },
  /* 573 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vdq" },
  /* 574 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Udq/Mw" },
  /* 575 */ { Go_Operand, NACL_OPFLAG(OpUse), "$Gq" },
  /* 576 */ { Mdq_Operand, NACL_OPFLAG(OpUse), "$Mdq" },
  /* 577 */ { Gv_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Gd" },
  /* 578 */ { E_Operand, NACL_OPFLAG(OpUse), "$Eb" },
  /* 579 */ { Gv_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Gd" },
  /* 580 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ev" },
  /* 581 */ { Mmx_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Pq" },
  /* 582 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse), "$Qq" },
  /* 583 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 584 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vss" },
  /* 585 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wss" },
  /* 586 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 587 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vsd" },
  /* 588 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wsd" },
  /* 589 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 590 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Vdq" },
  /* 591 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 592 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 593 */ { Ev_Operand, NACL_OPFLAG(OpSet), "$Rd/Mb" },
  /* 594 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 595 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 596 */ { Ev_Operand, NACL_OPFLAG(OpSet), "$Rd/Mw" },
  /* 597 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 598 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 599 */ { E_Operand, NACL_OPFLAG(OpSet), "$Ed/q/q" },
  /* 600 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 601 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 602 */ { E_Operand, NACL_OPFLAG(OpSet), "$Ed/q/d" },
  /* 603 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 604 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 605 */ { Ev_Operand, NACL_OPFLAG(OpSet), "$Ed" },
  /* 606 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 607 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 608 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vdq" },
  /* 609 */ { E_Operand, NACL_OPFLAG(OpUse), "$Rd/q/Mb" },
  /* 610 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 611 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vdq" },
  /* 612 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Udq/Md" },
  /* 613 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 614 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vdq" },
  /* 615 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ed/q/q" },
  /* 616 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 617 */ { Xmm_G_Operand, NACL_OPFLAG(OpSet), "$Vdq" },
  /* 618 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ed/q/d" },
  /* 619 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 620 */ { RegXMM0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%xmm0}" },
  /* 621 */ { RegREAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$rAXv}" },
  /* 622 */ { RegREDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$rDXv}" },
  /* 623 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 624 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 625 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 626 */ { RegRECX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$rCXv}" },
  /* 627 */ { RegREAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$rAXv}" },
  /* 628 */ { RegREDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$rDXv}" },
  /* 629 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 630 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 631 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 632 */ { RegXMM0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%xmm0}" },
  /* 633 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 634 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 635 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 636 */ { RegRECX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{$rCXv}" },
  /* 637 */ { Xmm_G_Operand, NACL_OPFLAG(OpUse), "$Vdq" },
  /* 638 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 639 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 640 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st0" },
  /* 641 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 642 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st0" },
  /* 643 */ { RegST1, NACL_OPFLAG(OpUse), "%st1" },
  /* 644 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st0" },
  /* 645 */ { RegST2, NACL_OPFLAG(OpUse), "%st2" },
  /* 646 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st0" },
  /* 647 */ { RegST3, NACL_OPFLAG(OpUse), "%st3" },
  /* 648 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st0" },
  /* 649 */ { RegST4, NACL_OPFLAG(OpUse), "%st4" },
  /* 650 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st0" },
  /* 651 */ { RegST5, NACL_OPFLAG(OpUse), "%st5" },
  /* 652 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st0" },
  /* 653 */ { RegST6, NACL_OPFLAG(OpUse), "%st6" },
  /* 654 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st0" },
  /* 655 */ { RegST7, NACL_OPFLAG(OpUse), "%st7" },
  /* 656 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 657 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 658 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 659 */ { RegST1, NACL_OPFLAG(OpUse), "%st1" },
  /* 660 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 661 */ { RegST2, NACL_OPFLAG(OpUse), "%st2" },
  /* 662 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 663 */ { RegST3, NACL_OPFLAG(OpUse), "%st3" },
  /* 664 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 665 */ { RegST4, NACL_OPFLAG(OpUse), "%st4" },
  /* 666 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 667 */ { RegST5, NACL_OPFLAG(OpUse), "%st5" },
  /* 668 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 669 */ { RegST6, NACL_OPFLAG(OpUse), "%st6" },
  /* 670 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 671 */ { RegST7, NACL_OPFLAG(OpUse), "%st7" },
  /* 672 */ { RegST0, NACL_OPFLAG(OpSet), "%st0" },
  /* 673 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 674 */ { RegST0, NACL_OPFLAG(OpSet), "%st0" },
  /* 675 */ { RegST1, NACL_OPFLAG(OpUse), "%st1" },
  /* 676 */ { RegST0, NACL_OPFLAG(OpSet), "%st0" },
  /* 677 */ { RegST2, NACL_OPFLAG(OpUse), "%st2" },
  /* 678 */ { RegST0, NACL_OPFLAG(OpSet), "%st0" },
  /* 679 */ { RegST3, NACL_OPFLAG(OpUse), "%st3" },
  /* 680 */ { RegST0, NACL_OPFLAG(OpSet), "%st0" },
  /* 681 */ { RegST4, NACL_OPFLAG(OpUse), "%st4" },
  /* 682 */ { RegST0, NACL_OPFLAG(OpSet), "%st0" },
  /* 683 */ { RegST5, NACL_OPFLAG(OpUse), "%st5" },
  /* 684 */ { RegST0, NACL_OPFLAG(OpSet), "%st0" },
  /* 685 */ { RegST6, NACL_OPFLAG(OpUse), "%st6" },
  /* 686 */ { RegST0, NACL_OPFLAG(OpSet), "%st0" },
  /* 687 */ { RegST7, NACL_OPFLAG(OpUse), "%st7" },
  /* 688 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st0" },
  /* 689 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st0" },
  /* 690 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st0" },
  /* 691 */ { RegST1, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st1" },
  /* 692 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st0" },
  /* 693 */ { RegST2, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st2" },
  /* 694 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st0" },
  /* 695 */ { RegST3, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st3" },
  /* 696 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st0" },
  /* 697 */ { RegST4, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st4" },
  /* 698 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st0" },
  /* 699 */ { RegST5, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st5" },
  /* 700 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st0" },
  /* 701 */ { RegST6, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st6" },
  /* 702 */ { RegST0, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st0" },
  /* 703 */ { RegST7, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st7" },
  /* 704 */ { RegST1, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st1" },
  /* 705 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 706 */ { RegST2, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st2" },
  /* 707 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 708 */ { RegST3, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st3" },
  /* 709 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 710 */ { RegST4, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st4" },
  /* 711 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 712 */ { RegST5, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st5" },
  /* 713 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 714 */ { RegST6, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st6" },
  /* 715 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 716 */ { RegST7, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%st7" },
  /* 717 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 718 */ { RegST0, NACL_EMPTY_OPFLAGS, "%st0" },
  /* 719 */ { RegST1, NACL_EMPTY_OPFLAGS, "%st1" },
  /* 720 */ { RegST2, NACL_EMPTY_OPFLAGS, "%st2" },
  /* 721 */ { RegST3, NACL_EMPTY_OPFLAGS, "%st3" },
  /* 722 */ { RegST4, NACL_EMPTY_OPFLAGS, "%st4" },
  /* 723 */ { RegST5, NACL_EMPTY_OPFLAGS, "%st5" },
  /* 724 */ { RegST6, NACL_EMPTY_OPFLAGS, "%st6" },
  /* 725 */ { RegST7, NACL_EMPTY_OPFLAGS, "%st7" },
  /* 726 */ { RegST1, NACL_OPFLAG(OpSet), "%st1" },
  /* 727 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 728 */ { RegST2, NACL_OPFLAG(OpSet), "%st2" },
  /* 729 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 730 */ { RegST3, NACL_OPFLAG(OpSet), "%st3" },
  /* 731 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 732 */ { RegST4, NACL_OPFLAG(OpSet), "%st4" },
  /* 733 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 734 */ { RegST5, NACL_OPFLAG(OpSet), "%st5" },
  /* 735 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 736 */ { RegST6, NACL_OPFLAG(OpSet), "%st6" },
  /* 737 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 738 */ { RegST7, NACL_OPFLAG(OpSet), "%st7" },
  /* 739 */ { RegST0, NACL_OPFLAG(OpUse), "%st0" },
  /* 740 */ { RegAX, NACL_OPFLAG(OpSet), "%ax" },
};

static const NaClInst g_Opcodes[1344] = {
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
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstAdd, 0x00, 2, 2, NACL_OPCODE_NULL_OFFSET  },
  /* 3 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstAdd, 0x00, 2, 4, NACL_OPCODE_NULL_OFFSET  },
  /* 4 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstAdd, 0x00, 2, 6, NACL_OPCODE_NULL_OFFSET  },
  /* 5 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable),
    InstAdd, 0x00, 2, 8, NACL_OPCODE_NULL_OFFSET  },
  /* 6 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstAdd, 0x00, 2, 10, NACL_OPCODE_NULL_OFFSET  },
  /* 7 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 8 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstOr, 0x00, 2, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 9 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstOr, 0x00, 2, 2, NACL_OPCODE_NULL_OFFSET  },
  /* 10 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstOr, 0x00, 2, 4, NACL_OPCODE_NULL_OFFSET  },
  /* 11 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstOr, 0x00, 2, 6, NACL_OPCODE_NULL_OFFSET  },
  /* 12 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable),
    InstOr, 0x00, 2, 8, NACL_OPCODE_NULL_OFFSET  },
  /* 13 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstOr, 0x00, 2, 10, NACL_OPCODE_NULL_OFFSET  },
  /* 14 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstAdc, 0x00, 2, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 15 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstAdc, 0x00, 2, 12, NACL_OPCODE_NULL_OFFSET  },
  /* 16 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstAdc, 0x00, 2, 4, NACL_OPCODE_NULL_OFFSET  },
  /* 17 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstAdc, 0x00, 2, 14, NACL_OPCODE_NULL_OFFSET  },
  /* 18 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable),
    InstAdc, 0x00, 2, 8, NACL_OPCODE_NULL_OFFSET  },
  /* 19 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstAdc, 0x00, 2, 16, NACL_OPCODE_NULL_OFFSET  },
  /* 20 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstSbb, 0x00, 2, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 21 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstSbb, 0x00, 2, 12, NACL_OPCODE_NULL_OFFSET  },
  /* 22 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstSbb, 0x00, 2, 4, NACL_OPCODE_NULL_OFFSET  },
  /* 23 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstSbb, 0x00, 2, 14, NACL_OPCODE_NULL_OFFSET  },
  /* 24 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable),
    InstSbb, 0x00, 2, 8, NACL_OPCODE_NULL_OFFSET  },
  /* 25 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstSbb, 0x00, 2, 16, NACL_OPCODE_NULL_OFFSET  },
  /* 26 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstAnd, 0x00, 2, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 27 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstAnd, 0x00, 2, 2, NACL_OPCODE_NULL_OFFSET  },
  /* 28 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstAnd, 0x00, 2, 4, NACL_OPCODE_NULL_OFFSET  },
  /* 29 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstAnd, 0x00, 2, 6, NACL_OPCODE_NULL_OFFSET  },
  /* 30 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable),
    InstAnd, 0x00, 2, 8, NACL_OPCODE_NULL_OFFSET  },
  /* 31 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstAnd, 0x00, 2, 10, NACL_OPCODE_NULL_OFFSET  },
  /* 32 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstSub, 0x00, 2, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 33 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstSub, 0x00, 2, 2, NACL_OPCODE_NULL_OFFSET  },
  /* 34 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstSub, 0x00, 2, 4, NACL_OPCODE_NULL_OFFSET  },
  /* 35 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstSub, 0x00, 2, 6, NACL_OPCODE_NULL_OFFSET  },
  /* 36 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable),
    InstSub, 0x00, 2, 8, NACL_OPCODE_NULL_OFFSET  },
  /* 37 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstSub, 0x00, 2, 10, NACL_OPCODE_NULL_OFFSET  },
  /* 38 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstXor, 0x00, 2, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 39 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstXor, 0x00, 2, 2, NACL_OPCODE_NULL_OFFSET  },
  /* 40 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstXor, 0x00, 2, 4, NACL_OPCODE_NULL_OFFSET  },
  /* 41 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstXor, 0x00, 2, 6, NACL_OPCODE_NULL_OFFSET  },
  /* 42 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable),
    InstXor, 0x00, 2, 8, NACL_OPCODE_NULL_OFFSET  },
  /* 43 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstXor, 0x00, 2, 10, NACL_OPCODE_NULL_OFFSET  },
  /* 44 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstCmp, 0x00, 2, 18, NACL_OPCODE_NULL_OFFSET  },
  /* 45 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCmp, 0x00, 2, 20, NACL_OPCODE_NULL_OFFSET  },
  /* 46 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstCmp, 0x00, 2, 22, NACL_OPCODE_NULL_OFFSET  },
  /* 47 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCmp, 0x00, 2, 24, NACL_OPCODE_NULL_OFFSET  },
  /* 48 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b),
    InstCmp, 0x00, 2, 26, NACL_OPCODE_NULL_OFFSET  },
  /* 49 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCmp, 0x00, 2, 28, NACL_OPCODE_NULL_OFFSET  },
  /* 50 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPush, 0x00, 2, 30, NACL_OPCODE_NULL_OFFSET  },
  /* 51 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPush, 0x01, 2, 30, NACL_OPCODE_NULL_OFFSET  },
  /* 52 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPush, 0x02, 2, 30, NACL_OPCODE_NULL_OFFSET  },
  /* 53 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPush, 0x03, 2, 30, NACL_OPCODE_NULL_OFFSET  },
  /* 54 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPush, 0x04, 2, 30, NACL_OPCODE_NULL_OFFSET  },
  /* 55 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPush, 0x05, 2, 30, NACL_OPCODE_NULL_OFFSET  },
  /* 56 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPush, 0x06, 2, 30, NACL_OPCODE_NULL_OFFSET  },
  /* 57 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPush, 0x07, 2, 30, NACL_OPCODE_NULL_OFFSET  },
  /* 58 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPop, 0x00, 2, 32, NACL_OPCODE_NULL_OFFSET  },
  /* 59 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPop, 0x01, 2, 32, NACL_OPCODE_NULL_OFFSET  },
  /* 60 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPop, 0x02, 2, 32, NACL_OPCODE_NULL_OFFSET  },
  /* 61 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPop, 0x03, 2, 32, NACL_OPCODE_NULL_OFFSET  },
  /* 62 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPop, 0x04, 2, 32, NACL_OPCODE_NULL_OFFSET  },
  /* 63 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPop, 0x05, 2, 32, NACL_OPCODE_NULL_OFFSET  },
  /* 64 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPop, 0x06, 2, 32, NACL_OPCODE_NULL_OFFSET  },
  /* 65 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPop, 0x07, 2, 32, NACL_OPCODE_NULL_OFFSET  },
  /* 66 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(LongMode),
    InstMovsxd, 0x00, 2, 34, NACL_OPCODE_NULL_OFFSET  },
  /* 67 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPush, 0x00, 2, 36, NACL_OPCODE_NULL_OFFSET  },
  /* 68 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstImul, 0x00, 3, 38, NACL_OPCODE_NULL_OFFSET  },
  /* 69 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPush, 0x00, 2, 41, NACL_OPCODE_NULL_OFFSET  },
  /* 70 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstImul, 0x00, 3, 43, NACL_OPCODE_NULL_OFFSET  },
  /* 71 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(NaClIllegal),
    InstInsb, 0x00, 2, 46, NACL_OPCODE_NULL_OFFSET  },
  /* 72 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal),
    InstInsd, 0x00, 2, 48, NACL_OPCODE_NULL_OFFSET  },
  /* 73 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(NaClIllegal),
    InstInsw, 0x00, 2, 50, 72  },
  /* 74 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(NaClIllegal),
    InstOutsb, 0x00, 2, 52, NACL_OPCODE_NULL_OFFSET  },
  /* 75 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal),
    InstOutsd, 0x00, 2, 54, NACL_OPCODE_NULL_OFFSET  },
  /* 76 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(NaClIllegal),
    InstOutsw, 0x00, 2, 56, 75  },
  /* 77 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump) | NACL_IFLAG(BranchHints),
    InstJo, 0x00, 2, 58, NACL_OPCODE_NULL_OFFSET  },
  /* 78 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump) | NACL_IFLAG(BranchHints),
    InstJno, 0x00, 2, 58, NACL_OPCODE_NULL_OFFSET  },
  /* 79 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump) | NACL_IFLAG(BranchHints),
    InstJb, 0x00, 2, 58, NACL_OPCODE_NULL_OFFSET  },
  /* 80 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump) | NACL_IFLAG(BranchHints),
    InstJnb, 0x00, 2, 58, NACL_OPCODE_NULL_OFFSET  },
  /* 81 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump) | NACL_IFLAG(BranchHints),
    InstJz, 0x00, 2, 58, NACL_OPCODE_NULL_OFFSET  },
  /* 82 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump) | NACL_IFLAG(BranchHints),
    InstJnz, 0x00, 2, 58, NACL_OPCODE_NULL_OFFSET  },
  /* 83 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump) | NACL_IFLAG(BranchHints),
    InstJbe, 0x00, 2, 58, NACL_OPCODE_NULL_OFFSET  },
  /* 84 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump) | NACL_IFLAG(BranchHints),
    InstJnbe, 0x00, 2, 58, NACL_OPCODE_NULL_OFFSET  },
  /* 85 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump) | NACL_IFLAG(BranchHints),
    InstJs, 0x00, 2, 58, NACL_OPCODE_NULL_OFFSET  },
  /* 86 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump) | NACL_IFLAG(BranchHints),
    InstJns, 0x00, 2, 58, NACL_OPCODE_NULL_OFFSET  },
  /* 87 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump) | NACL_IFLAG(BranchHints),
    InstJp, 0x00, 2, 58, NACL_OPCODE_NULL_OFFSET  },
  /* 88 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump) | NACL_IFLAG(BranchHints),
    InstJnp, 0x00, 2, 58, NACL_OPCODE_NULL_OFFSET  },
  /* 89 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump) | NACL_IFLAG(BranchHints),
    InstJl, 0x00, 2, 58, NACL_OPCODE_NULL_OFFSET  },
  /* 90 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump) | NACL_IFLAG(BranchHints),
    InstJnl, 0x00, 2, 58, NACL_OPCODE_NULL_OFFSET  },
  /* 91 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump) | NACL_IFLAG(BranchHints),
    InstJle, 0x00, 2, 58, NACL_OPCODE_NULL_OFFSET  },
  /* 92 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump) | NACL_IFLAG(BranchHints),
    InstJnle, 0x00, 2, 58, NACL_OPCODE_NULL_OFFSET  },
  /* 93 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstCmp, 0x07, 2, 60, NACL_OPCODE_NULL_OFFSET  },
  /* 94 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstXor, 0x06, 2, 62, 93  },
  /* 95 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstSub, 0x05, 2, 62, 94  },
  /* 96 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstAnd, 0x04, 2, 62, 95  },
  /* 97 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstSbb, 0x03, 2, 62, 96  },
  /* 98 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstAdc, 0x02, 2, 62, 97  },
  /* 99 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstOr, 0x01, 2, 62, 98  },
  /* 100 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstAdd, 0x00, 2, 62, 99  },
  /* 101 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCmp, 0x07, 2, 39, NACL_OPCODE_NULL_OFFSET  },
  /* 102 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstXor, 0x06, 2, 64, 101  },
  /* 103 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstSub, 0x05, 2, 64, 102  },
  /* 104 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstAnd, 0x04, 2, 64, 103  },
  /* 105 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstSbb, 0x03, 2, 66, 104  },
  /* 106 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstAdc, 0x02, 2, 66, 105  },
  /* 107 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstOr, 0x01, 2, 64, 106  },
  /* 108 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstAdd, 0x00, 2, 64, 107  },
  /* 109 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x07, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 110 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x06, 0, 0, 109  },
  /* 111 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x05, 0, 0, 110  },
  /* 112 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x04, 0, 0, 111  },
  /* 113 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x03, 0, 0, 112  },
  /* 114 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x02, 0, 0, 113  },
  /* 115 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x01, 0, 0, 114  },
  /* 116 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00, 0, 0, 115  },
  /* 117 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCmp, 0x07, 2, 44, NACL_OPCODE_NULL_OFFSET  },
  /* 118 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstXor, 0x06, 2, 68, 117  },
  /* 119 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstSub, 0x05, 2, 68, 118  },
  /* 120 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstAnd, 0x04, 2, 68, 119  },
  /* 121 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstSbb, 0x03, 2, 70, 120  },
  /* 122 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstAdc, 0x02, 2, 70, 121  },
  /* 123 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstOr, 0x01, 2, 68, 122  },
  /* 124 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstAdd, 0x00, 2, 68, 123  },
  /* 125 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstTest, 0x00, 2, 18, NACL_OPCODE_NULL_OFFSET  },
  /* 126 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstTest, 0x00, 2, 20, NACL_OPCODE_NULL_OFFSET  },
  /* 127 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstXchg, 0x00, 2, 72, NACL_OPCODE_NULL_OFFSET  },
  /* 128 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstXchg, 0x00, 2, 74, NACL_OPCODE_NULL_OFFSET  },
  /* 129 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x00, 2, 76, NACL_OPCODE_NULL_OFFSET  },
  /* 130 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstMov, 0x00, 2, 78, NACL_OPCODE_NULL_OFFSET  },
  /* 131 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x00, 2, 80, NACL_OPCODE_NULL_OFFSET  },
  /* 132 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstMov, 0x00, 2, 38, NACL_OPCODE_NULL_OFFSET  },
  /* 133 */
  { NACLi_386,
    NACL_IFLAG(ModRmRegSOperand) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal),
    InstMov, 0x00, 2, 82, NACL_OPCODE_NULL_OFFSET  },
  /* 134 */
  { NACLi_386,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstLea, 0x00, 2, 84, NACL_OPCODE_NULL_OFFSET  },
  /* 135 */
  { NACLi_386,
    NACL_IFLAG(ModRmRegSOperand) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstMov, 0x00, 2, 86, NACL_OPCODE_NULL_OFFSET  },
  /* 136 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 137 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPop, 0x00, 2, 88, 136  },
  /* 138 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstXchg, 0x00, 2, 90, NACL_OPCODE_NULL_OFFSET  },
  /* 139 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstXchg, 0x01, 2, 90, NACL_OPCODE_NULL_OFFSET  },
  /* 140 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstXchg, 0x02, 2, 90, NACL_OPCODE_NULL_OFFSET  },
  /* 141 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstXchg, 0x03, 2, 90, NACL_OPCODE_NULL_OFFSET  },
  /* 142 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstXchg, 0x04, 2, 90, NACL_OPCODE_NULL_OFFSET  },
  /* 143 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstXchg, 0x05, 2, 90, NACL_OPCODE_NULL_OFFSET  },
  /* 144 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstXchg, 0x06, 2, 90, NACL_OPCODE_NULL_OFFSET  },
  /* 145 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstXchg, 0x07, 2, 90, NACL_OPCODE_NULL_OFFSET  },
  /* 146 */
  { NACLi_386,
    NACL_IFLAG(OperandSize_o) | NACL_IFLAG(LongMode),
    InstCdqe, 0x00, 2, 92, NACL_OPCODE_NULL_OFFSET  },
  /* 147 */
  { NACLi_386,
    NACL_IFLAG(OperandSize_v),
    InstCwde, 0x00, 2, 94, 146  },
  /* 148 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w),
    InstCbw, 0x00, 2, 96, 147  },
  /* 149 */
  { NACLi_386,
    NACL_IFLAG(OperandSize_o) | NACL_IFLAG(LongMode),
    InstCqo, 0x00, 2, 98, NACL_OPCODE_NULL_OFFSET  },
  /* 150 */
  { NACLi_386,
    NACL_IFLAG(OperandSize_v),
    InstCdq, 0x00, 2, 100, 149  },
  /* 151 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w),
    InstCwd, 0x00, 2, 102, 150  },
  /* 152 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFwait, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 153 */
  { NACLi_386,
    NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(OperandSizeDefaultIs64) | NACL_IFLAG(LongMode),
    InstPushfq, 0x00, 2, 104, NACL_OPCODE_NULL_OFFSET  },
  /* 154 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(NaClIllegal),
    InstPushf, 0x00, 2, 106, 153  },
  /* 155 */
  { NACLi_386,
    NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(OperandSizeDefaultIs64) | NACL_IFLAG(LongMode),
    InstPopfq, 0x00, 2, 108, NACL_OPCODE_NULL_OFFSET  },
  /* 156 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(NaClIllegal),
    InstPopf, 0x00, 2, 110, 155  },
  /* 157 */
  { NACLi_LAHF,
    NACL_EMPTY_IFLAGS,
    InstSahf, 0x00, 1, 112, NACL_OPCODE_NULL_OFFSET  },
  /* 158 */
  { NACLi_LAHF,
    NACL_EMPTY_IFLAGS,
    InstLahf, 0x00, 1, 113, NACL_OPCODE_NULL_OFFSET  },
  /* 159 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_Addr) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x00, 2, 114, NACL_OPCODE_NULL_OFFSET  },
  /* 160 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_Addr) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstMov, 0x00, 2, 116, NACL_OPCODE_NULL_OFFSET  },
  /* 161 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_Addr) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x00, 2, 118, NACL_OPCODE_NULL_OFFSET  },
  /* 162 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_Addr) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstMov, 0x00, 2, 120, NACL_OPCODE_NULL_OFFSET  },
  /* 163 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_b),
    InstMovsb, 0x00, 2, 122, NACL_OPCODE_NULL_OFFSET  },
  /* 164 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(LongMode),
    InstMovsq, 0x00, 2, 124, NACL_OPCODE_NULL_OFFSET  },
  /* 165 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_v),
    InstMovsd, 0x00, 2, 126, 164  },
  /* 166 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w),
    InstMovsw, 0x00, 2, 128, 165  },
  /* 167 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(OperandSize_b),
    InstCmpsb, 0x00, 2, 130, NACL_OPCODE_NULL_OFFSET  },
  /* 168 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(LongMode),
    InstCmpsq, 0x00, 2, 132, NACL_OPCODE_NULL_OFFSET  },
  /* 169 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(OperandSize_v),
    InstCmpsd, 0x00, 2, 134, 168  },
  /* 170 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w),
    InstCmpsw, 0x00, 2, 136, 169  },
  /* 171 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b),
    InstTest, 0x00, 2, 26, NACL_OPCODE_NULL_OFFSET  },
  /* 172 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstTest, 0x00, 2, 28, NACL_OPCODE_NULL_OFFSET  },
  /* 173 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_b),
    InstStosb, 0x00, 2, 138, NACL_OPCODE_NULL_OFFSET  },
  /* 174 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(LongMode),
    InstStosq, 0x00, 2, 140, NACL_OPCODE_NULL_OFFSET  },
  /* 175 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_v),
    InstStosd, 0x00, 2, 142, 174  },
  /* 176 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w),
    InstStosw, 0x00, 2, 144, 175  },
  /* 177 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_b),
    InstLodsb, 0x00, 2, 146, NACL_OPCODE_NULL_OFFSET  },
  /* 178 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(LongMode),
    InstLodsq, 0x00, 2, 148, NACL_OPCODE_NULL_OFFSET  },
  /* 179 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_v),
    InstLodsd, 0x00, 2, 150, 178  },
  /* 180 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w),
    InstLodsw, 0x00, 2, 152, 179  },
  /* 181 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(OperandSize_b),
    InstScasb, 0x00, 2, 154, NACL_OPCODE_NULL_OFFSET  },
  /* 182 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(LongMode),
    InstScasq, 0x00, 2, 156, NACL_OPCODE_NULL_OFFSET  },
  /* 183 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(OperandSize_v),
    InstScasd, 0x00, 2, 158, 182  },
  /* 184 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w),
    InstScasw, 0x00, 2, 160, 183  },
  /* 185 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x00, 2, 162, NACL_OPCODE_NULL_OFFSET  },
  /* 186 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x01, 2, 162, NACL_OPCODE_NULL_OFFSET  },
  /* 187 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x02, 2, 162, NACL_OPCODE_NULL_OFFSET  },
  /* 188 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x03, 2, 162, NACL_OPCODE_NULL_OFFSET  },
  /* 189 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x04, 2, 162, NACL_OPCODE_NULL_OFFSET  },
  /* 190 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x05, 2, 162, NACL_OPCODE_NULL_OFFSET  },
  /* 191 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x06, 2, 162, NACL_OPCODE_NULL_OFFSET  },
  /* 192 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x07, 2, 162, NACL_OPCODE_NULL_OFFSET  },
  /* 193 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstMov, 0x00, 2, 164, NACL_OPCODE_NULL_OFFSET  },
  /* 194 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstMov, 0x01, 2, 164, NACL_OPCODE_NULL_OFFSET  },
  /* 195 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstMov, 0x02, 2, 164, NACL_OPCODE_NULL_OFFSET  },
  /* 196 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstMov, 0x03, 2, 164, NACL_OPCODE_NULL_OFFSET  },
  /* 197 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstMov, 0x04, 2, 164, NACL_OPCODE_NULL_OFFSET  },
  /* 198 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstMov, 0x05, 2, 164, NACL_OPCODE_NULL_OFFSET  },
  /* 199 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstMov, 0x06, 2, 164, NACL_OPCODE_NULL_OFFSET  },
  /* 200 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstMov, 0x07, 2, 164, NACL_OPCODE_NULL_OFFSET  },
  /* 201 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstSar, 0x07, 2, 62, NACL_OPCODE_NULL_OFFSET  },
  /* 202 */
  { NACLi_ILLEGAL,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(NaClIllegal),
    InstShl, 0x06, 2, 62, 201  },
  /* 203 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstShr, 0x05, 2, 62, 202  },
  /* 204 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstShl, 0x04, 2, 62, 203  },
  /* 205 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstRcr, 0x03, 2, 62, 204  },
  /* 206 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstRcl, 0x02, 2, 62, 205  },
  /* 207 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstRor, 0x01, 2, 62, 206  },
  /* 208 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstRol, 0x00, 2, 62, 207  },
  /* 209 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstSar, 0x07, 2, 70, NACL_OPCODE_NULL_OFFSET  },
  /* 210 */
  { NACLi_ILLEGAL,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal),
    InstShl, 0x06, 2, 70, 209  },
  /* 211 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstShr, 0x05, 2, 70, 210  },
  /* 212 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstShl, 0x04, 2, 70, 211  },
  /* 213 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstRcr, 0x03, 2, 70, 212  },
  /* 214 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstRcl, 0x02, 2, 70, 213  },
  /* 215 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstRor, 0x01, 2, 70, 214  },
  /* 216 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstRol, 0x00, 2, 70, 215  },
  /* 217 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_w) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstRet, 0x00, 3, 166, NACL_OPCODE_NULL_OFFSET  },
  /* 218 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstRet, 0x00, 2, 166, NACL_OPCODE_NULL_OFFSET  },
  /* 219 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x00, 2, 169, 136  },
  /* 220 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstMov, 0x00, 2, 171, 136  },
  /* 221 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_w) | NACL_IFLAG(OpcodeHasImmed2_b) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstEnter, 0x00, 4, 173, NACL_OPCODE_NULL_OFFSET  },
  /* 222 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstLeave, 0x00, 2, 177, NACL_OPCODE_NULL_OFFSET  },
  /* 223 */
  { NACLi_RETURN,
    NACL_IFLAG(OpcodeHasImmed_w) | NACL_IFLAG(NaClIllegal),
    InstRet, 0x00, 3, 166, NACL_OPCODE_NULL_OFFSET  },
  /* 224 */
  { NACLi_RETURN,
    NACL_IFLAG(NaClIllegal),
    InstRet, 0x00, 2, 166, NACL_OPCODE_NULL_OFFSET  },
  /* 225 */
  { NACLi_SYSTEM,
    NACL_IFLAG(NaClIllegal),
    InstInt3, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 226 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(NaClIllegal),
    InstInt, 0x00, 1, 9, NACL_OPCODE_NULL_OFFSET  },
  /* 227 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstInto, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 228 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(NaClIllegal),
    InstIret, 0x00, 2, 166, NACL_OPCODE_NULL_OFFSET  },
  /* 229 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(LongMode),
    InstIretq, 0x00, 2, 166, 228  },
  /* 230 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal),
    InstIretd, 0x00, 2, 166, 229  },
  /* 231 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSar, 0x07, 2, 179, NACL_OPCODE_NULL_OFFSET  },
  /* 232 */
  { NACLi_ILLEGAL,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(NaClIllegal),
    InstShl, 0x06, 2, 179, 231  },
  /* 233 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstShr, 0x05, 2, 179, 232  },
  /* 234 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstShl, 0x04, 2, 179, 233  },
  /* 235 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstRcr, 0x03, 2, 179, 234  },
  /* 236 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstRcl, 0x02, 2, 179, 235  },
  /* 237 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstRor, 0x01, 2, 179, 236  },
  /* 238 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstRol, 0x00, 2, 179, 237  },
  /* 239 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstSar, 0x07, 2, 181, NACL_OPCODE_NULL_OFFSET  },
  /* 240 */
  { NACLi_ILLEGAL,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal),
    InstShl, 0x06, 2, 181, 239  },
  /* 241 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstShr, 0x05, 2, 181, 240  },
  /* 242 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstShl, 0x04, 2, 181, 241  },
  /* 243 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstRcr, 0x03, 2, 181, 242  },
  /* 244 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstRcl, 0x02, 2, 181, 243  },
  /* 245 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstRor, 0x01, 2, 181, 244  },
  /* 246 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstRol, 0x00, 2, 181, 245  },
  /* 247 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSar, 0x07, 2, 183, NACL_OPCODE_NULL_OFFSET  },
  /* 248 */
  { NACLi_ILLEGAL,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(NaClIllegal),
    InstShl, 0x06, 2, 183, 247  },
  /* 249 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstShr, 0x05, 2, 183, 248  },
  /* 250 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstShl, 0x04, 2, 183, 249  },
  /* 251 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstRcr, 0x03, 2, 183, 250  },
  /* 252 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstRcl, 0x02, 2, 183, 251  },
  /* 253 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstRor, 0x01, 2, 183, 252  },
  /* 254 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstRol, 0x00, 2, 183, 253  },
  /* 255 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstSar, 0x07, 2, 185, NACL_OPCODE_NULL_OFFSET  },
  /* 256 */
  { NACLi_ILLEGAL,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal),
    InstShl, 0x06, 2, 185, 255  },
  /* 257 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstShr, 0x05, 2, 185, 256  },
  /* 258 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstShl, 0x04, 2, 185, 257  },
  /* 259 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstRcr, 0x03, 2, 185, 258  },
  /* 260 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstRcl, 0x02, 2, 185, 259  },
  /* 261 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstRor, 0x01, 2, 185, 260  },
  /* 262 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstRol, 0x00, 2, 185, 261  },
  /* 263 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstXlat, 0x00, 2, 187, NACL_OPCODE_NULL_OFFSET  },
  /* 264 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFdivr, 0x07, 2, 189, NACL_OPCODE_NULL_OFFSET  },
  /* 265 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFdiv, 0x06, 2, 189, 264  },
  /* 266 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFsubr, 0x05, 2, 189, 265  },
  /* 267 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFsub, 0x04, 2, 189, 266  },
  /* 268 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFcomp, 0x03, 2, 191, 267  },
  /* 269 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFcom, 0x02, 2, 191, 268  },
  /* 270 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFmul, 0x01, 2, 189, 269  },
  /* 271 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFadd, 0x00, 2, 189, 270  },
  /* 272 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFnstcw, 0x07, 1, 193, NACL_OPCODE_NULL_OFFSET  },
  /* 273 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFnstenv, 0x06, 1, 194, 272  },
  /* 274 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFldcw, 0x05, 1, 195, 273  },
  /* 275 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFldenv, 0x04, 1, 196, 274  },
  /* 276 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFstp, 0x03, 2, 197, 275  },
  /* 277 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFst, 0x02, 2, 197, 276  },
  /* 278 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x01, 0, 0, 277  },
  /* 279 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFld, 0x00, 2, 199, 278  },
  /* 280 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFidivr, 0x07, 2, 189, NACL_OPCODE_NULL_OFFSET  },
  /* 281 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFidiv, 0x06, 2, 189, 280  },
  /* 282 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFisubr, 0x05, 2, 189, 281  },
  /* 283 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFisub, 0x04, 2, 189, 282  },
  /* 284 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFicomp, 0x03, 2, 189, 283  },
  /* 285 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFicom, 0x02, 2, 189, 284  },
  /* 286 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFimul, 0x01, 2, 189, 285  },
  /* 287 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFiadd, 0x00, 2, 189, 286  },
  /* 288 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFstp, 0x07, 2, 201, NACL_OPCODE_NULL_OFFSET  },
  /* 289 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x06, 0, 0, 288  },
  /* 290 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFld, 0x05, 2, 203, 289  },
  /* 291 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x04, 0, 0, 290  },
  /* 292 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFistp, 0x03, 2, 197, 291  },
  /* 293 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFist, 0x02, 2, 197, 292  },
  /* 294 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFisttp, 0x01, 2, 197, 293  },
  /* 295 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFild, 0x00, 2, 199, 294  },
  /* 296 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFdivr, 0x07, 2, 205, NACL_OPCODE_NULL_OFFSET  },
  /* 297 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFdiv, 0x06, 2, 205, 296  },
  /* 298 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFsubr, 0x05, 2, 205, 297  },
  /* 299 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFsub, 0x04, 2, 205, 298  },
  /* 300 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFcomp, 0x03, 2, 207, 299  },
  /* 301 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFcom, 0x02, 2, 207, 300  },
  /* 302 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFmul, 0x01, 2, 205, 301  },
  /* 303 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFadd, 0x00, 2, 205, 302  },
  /* 304 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFnstsw, 0x07, 1, 193, NACL_OPCODE_NULL_OFFSET  },
  /* 305 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFnsave, 0x06, 1, 194, 304  },
  /* 306 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x05, 0, 0, 305  },
  /* 307 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFrstor, 0x04, 1, 196, 306  },
  /* 308 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFstp, 0x03, 2, 209, 307  },
  /* 309 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFst, 0x02, 2, 209, 308  },
  /* 310 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFisttp, 0x01, 2, 209, 309  },
  /* 311 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFld, 0x00, 2, 211, 310  },
  /* 312 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFidivr, 0x07, 2, 213, NACL_OPCODE_NULL_OFFSET  },
  /* 313 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFidiv, 0x06, 2, 213, 312  },
  /* 314 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFisubr, 0x05, 2, 213, 313  },
  /* 315 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFisub, 0x04, 2, 213, 314  },
  /* 316 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFicomp, 0x03, 2, 215, 315  },
  /* 317 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFicom, 0x02, 2, 215, 316  },
  /* 318 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFimul, 0x01, 2, 213, 317  },
  /* 319 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFiadd, 0x00, 2, 213, 318  },
  /* 320 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFistp, 0x07, 2, 201, NACL_OPCODE_NULL_OFFSET  },
  /* 321 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFbstp, 0x06, 2, 201, 320  },
  /* 322 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFild, 0x05, 2, 203, 321  },
  /* 323 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFbld, 0x04, 2, 203, 322  },
  /* 324 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFistp, 0x03, 2, 217, 323  },
  /* 325 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFist, 0x02, 2, 217, 324  },
  /* 326 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFisttp, 0x01, 2, 217, 325  },
  /* 327 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstFild, 0x00, 2, 219, 326  },
  /* 328 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(AddressSize_o) | NACL_IFLAG(ConditionalJump),
    InstLoopne, 0x00, 3, 221, NACL_OPCODE_NULL_OFFSET  },
  /* 329 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(AddressSize_v) | NACL_IFLAG(ConditionalJump),
    InstLoopne, 0x00, 3, 224, 328  },
  /* 330 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(AddressSize_o) | NACL_IFLAG(ConditionalJump),
    InstLoope, 0x00, 3, 221, NACL_OPCODE_NULL_OFFSET  },
  /* 331 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(AddressSize_v) | NACL_IFLAG(ConditionalJump),
    InstLoope, 0x00, 3, 224, 330  },
  /* 332 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(AddressSize_o) | NACL_IFLAG(ConditionalJump),
    InstLoop, 0x00, 3, 221, NACL_OPCODE_NULL_OFFSET  },
  /* 333 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(AddressSize_v) | NACL_IFLAG(ConditionalJump),
    InstLoop, 0x00, 3, 224, 332  },
  /* 334 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(AddressSize_o) | NACL_IFLAG(ConditionalJump) | NACL_IFLAG(BranchHints),
    InstJrcxz, 0x00, 3, 227, NACL_OPCODE_NULL_OFFSET  },
  /* 335 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(AddressSize_v) | NACL_IFLAG(ConditionalJump) | NACL_IFLAG(BranchHints),
    InstJecxz, 0x00, 3, 230, 334  },
  /* 336 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(NaClIllegal),
    InstIn, 0x00, 2, 233, NACL_OPCODE_NULL_OFFSET  },
  /* 337 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal),
    InstIn, 0x00, 2, 235, NACL_OPCODE_NULL_OFFSET  },
  /* 338 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(NaClIllegal),
    InstOut, 0x00, 2, 237, NACL_OPCODE_NULL_OFFSET  },
  /* 339 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal),
    InstOut, 0x00, 2, 239, NACL_OPCODE_NULL_OFFSET  },
  /* 340 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64) | NACL_IFLAG(JumpInstruction),
    InstCall, 0x00, 3, 241, NACL_OPCODE_NULL_OFFSET  },
  /* 341 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64) | NACL_IFLAG(JumpInstruction),
    InstJmp, 0x00, 2, 244, NACL_OPCODE_NULL_OFFSET  },
  /* 342 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(JumpInstruction),
    InstJmp, 0x00, 2, 58, NACL_OPCODE_NULL_OFFSET  },
  /* 343 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstIn, 0x00, 2, 246, NACL_OPCODE_NULL_OFFSET  },
  /* 344 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal),
    InstIn, 0x00, 2, 248, NACL_OPCODE_NULL_OFFSET  },
  /* 345 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstOut, 0x00, 2, 250, NACL_OPCODE_NULL_OFFSET  },
  /* 346 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal),
    InstOut, 0x00, 2, 252, NACL_OPCODE_NULL_OFFSET  },
  /* 347 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstInt1, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 348 */
  { NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstHlt, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 349 */
  { NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstCmc, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 350 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstIdiv, 0x07, 3, 254, NACL_OPCODE_NULL_OFFSET  },
  /* 351 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstDiv, 0x06, 3, 254, 350  },
  /* 352 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstImul, 0x05, 3, 254, 351  },
  /* 353 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstMul, 0x04, 3, 254, 352  },
  /* 354 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstNeg, 0x03, 1, 0, 353  },
  /* 355 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstNot, 0x02, 1, 0, 354  },
  /* 356 */
  { NACLi_ILLEGAL,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(NaClIllegal),
    InstTest, 0x01, 2, 60, 355  },
  /* 357 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstTest, 0x00, 2, 60, 356  },
  /* 358 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstIdiv, 0x07, 3, 257, NACL_OPCODE_NULL_OFFSET  },
  /* 359 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstDiv, 0x06, 3, 257, 358  },
  /* 360 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstImul, 0x05, 3, 257, 359  },
  /* 361 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstMul, 0x04, 3, 257, 360  },
  /* 362 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstNeg, 0x03, 1, 2, 361  },
  /* 363 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstNot, 0x02, 1, 2, 362  },
  /* 364 */
  { NACLi_ILLEGAL,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal),
    InstTest, 0x01, 2, 39, 363  },
  /* 365 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstTest, 0x00, 2, 39, 364  },
  /* 366 */
  { NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstClc, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 367 */
  { NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstStc, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 368 */
  { NACLi_SYSTEM,
    NACL_IFLAG(NaClIllegal),
    InstCli, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 369 */
  { NACLi_SYSTEM,
    NACL_IFLAG(NaClIllegal),
    InstSti, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 370 */
  { NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstCld, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 371 */
  { NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstStd, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 372 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstDec, 0x01, 1, 0, 114  },
  /* 373 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstInc, 0x00, 1, 0, 372  },
  /* 374 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPush, 0x06, 2, 260, 109  },
  /* 375 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(JumpInstruction),
    InstJmp, 0x05, 2, 262, 374  },
  /* 376 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64) | NACL_IFLAG(JumpInstruction),
    InstJmp, 0x04, 2, 264, 375  },
  /* 377 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(JumpInstruction),
    InstCall, 0x03, 3, 266, 376  },
  /* 378 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64) | NACL_IFLAG(JumpInstruction),
    InstCall, 0x02, 3, 269, 377  },
  /* 379 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstDec, 0x01, 1, 2, 378  },
  /* 380 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstInc, 0x00, 1, 2, 379  },
  /* 381 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstVerw, 0x05, 1, 272, 110  },
  /* 382 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstVerr, 0x04, 1, 272, 381  },
  /* 383 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstLtr, 0x03, 1, 87, 382  },
  /* 384 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstLldt, 0x02, 1, 87, 383  },
  /* 385 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal),
    InstStr, 0x01, 1, 273, 384  },
  /* 386 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal),
    InstSldt, 0x00, 1, 273, 385  },
  /* 387 */
  { NACLi_RDTSCP,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstRdtscp, 0x17, 3, 274, 109  },
  /* 388 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(LongMode),
    InstSwapgs, 0x07, 1, 277, 387  },
  /* 389 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvlpg, 0x07, 1, 278, 388  },
  /* 390 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstLmsw, 0x06, 1, 87, 389  },
  /* 391 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x05, 0, 0, 390  },
  /* 392 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal),
    InstSmsw, 0x04, 1, 273, 391  },
  /* 393 */
  { NACLi_SVM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvlpga, 0x73, 2, 279, 392  },
  /* 394 */
  { NACLi_SVM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstSkinit, 0x63, 2, 281, 393  },
  /* 395 */
  { NACLi_SVM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstClgi, 0x53, 0, 0, 394  },
  /* 396 */
  { NACLi_SVM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstStgi, 0x43, 0, 0, 395  },
  /* 397 */
  { NACLi_SVM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstVmsave, 0x33, 1, 279, 396  },
  /* 398 */
  { NACLi_SVM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstVmload, 0x23, 1, 279, 397  },
  /* 399 */
  { NACLi_SVM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstVmmcall, 0x13, 0, 0, 398  },
  /* 400 */
  { NACLi_SVM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstVmrun, 0x03, 1, 279, 399  },
  /* 401 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstLidt, 0x03, 1, 283, 400  },
  /* 402 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstLgdt, 0x02, 1, 283, 401  },
  /* 403 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x01, 0, 0, 402  },
  /* 404 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstMwait, 0x11, 2, 284, 403  },
  /* 405 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstMonitor, 0x01, 3, 286, 404  },
  /* 406 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstSidt, 0x01, 1, 289, 405  },
  /* 407 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstSgdt, 0x00, 1, 289, 406  },
  /* 408 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal),
    InstLar, 0x00, 2, 290, NACL_OPCODE_NULL_OFFSET  },
  /* 409 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal),
    InstLsl, 0x00, 2, 290, NACL_OPCODE_NULL_OFFSET  },
  /* 410 */
  { NACLi_SYSCALL,
    NACL_IFLAG(NaClIllegal),
    InstSyscall, 0x00, 2, 292, NACL_OPCODE_NULL_OFFSET  },
  /* 411 */
  { NACLi_SYSTEM,
    NACL_IFLAG(NaClIllegal),
    InstClts, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 412 */
  { NACLi_SYSTEM,
    NACL_IFLAG(NaClIllegal),
    InstSysret, 0x00, 2, 227, NACL_OPCODE_NULL_OFFSET  },
  /* 413 */
  { NACLi_SYSTEM,
    NACL_IFLAG(NaClIllegal),
    InstInvd, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 414 */
  { NACLi_SYSTEM,
    NACL_IFLAG(NaClIllegal),
    InstWbinvd, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 415 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal),
    InstUd2, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 416 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstPrefetch_reserved, 0x07, 1, 294, NACL_OPCODE_NULL_OFFSET  },
  /* 417 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstPrefetch_reserved, 0x06, 1, 294, 416  },
  /* 418 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstPrefetch_reserved, 0x05, 1, 294, 417  },
  /* 419 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstPrefetch_reserved, 0x04, 1, 294, 418  },
  /* 420 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstPrefetch_modified, 0x03, 1, 294, 419  },
  /* 421 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstPrefetch_reserved, 0x02, 1, 294, 420  },
  /* 422 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstPrefetch_modified, 0x01, 1, 294, 421  },
  /* 423 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstPrefetch_exclusive, 0x00, 1, 294, 422  },
  /* 424 */
  { NACLi_3DNOW,
    NACL_EMPTY_IFLAGS,
    InstFemms, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 425 */
  { NACLi_INVALID,
    NACL_IFLAG(Opcode0F0F) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00, 3, 295, NACL_OPCODE_NULL_OFFSET  },
  /* 426 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstMovups, 0x00, 2, 298, NACL_OPCODE_NULL_OFFSET  },
  /* 427 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstMovups, 0x00, 2, 300, NACL_OPCODE_NULL_OFFSET  },
  /* 428 */
  { NACLi_SSE,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMovhlps, 0x00, 2, 302, NACL_OPCODE_NULL_OFFSET  },
  /* 429 */
  { NACLi_SSE,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMovlps, 0x00, 2, 304, 428  },
  /* 430 */
  { NACLi_SSE,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMovlps, 0x00, 2, 306, NACL_OPCODE_NULL_OFFSET  },
  /* 431 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstUnpcklps, 0x00, 2, 308, NACL_OPCODE_NULL_OFFSET  },
  /* 432 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstUnpckhps, 0x00, 2, 308, NACL_OPCODE_NULL_OFFSET  },
  /* 433 */
  { NACLi_SSE,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMovlhps, 0x00, 2, 302, NACL_OPCODE_NULL_OFFSET  },
  /* 434 */
  { NACLi_SSE,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMovhps, 0x00, 2, 304, 433  },
  /* 435 */
  { NACLi_SSE,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMovhps, 0x00, 2, 306, NACL_OPCODE_NULL_OFFSET  },
  /* 436 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstPrefetcht2, 0x03, 1, 294, 112  },
  /* 437 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstPrefetcht1, 0x02, 1, 294, 436  },
  /* 438 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstPrefetcht0, 0x01, 1, 294, 437  },
  /* 439 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstPrefetchnta, 0x00, 1, 294, 438  },
  /* 440 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstNop, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 441 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm),
    InstNop, 0x00, 0, 0, 440  },
  /* 442 */
  { NACLi_SYSTEM,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstMov, 0x00, 2, 310, NACL_OPCODE_NULL_OFFSET  },
  /* 443 */
  { NACLi_SYSTEM,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstMov, 0x00, 2, 312, NACL_OPCODE_NULL_OFFSET  },
  /* 444 */
  { NACLi_SYSTEM,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstMov, 0x00, 2, 314, NACL_OPCODE_NULL_OFFSET  },
  /* 445 */
  { NACLi_SYSTEM,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstMov, 0x00, 2, 316, NACL_OPCODE_NULL_OFFSET  },
  /* 446 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstMovaps, 0x00, 2, 298, NACL_OPCODE_NULL_OFFSET  },
  /* 447 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstMovaps, 0x00, 2, 300, NACL_OPCODE_NULL_OFFSET  },
  /* 448 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstCvtpi2ps, 0x00, 2, 318, NACL_OPCODE_NULL_OFFSET  },
  /* 449 */
  { NACLi_SSE,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMovntps, 0x00, 2, 320, NACL_OPCODE_NULL_OFFSET  },
  /* 450 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstCvttps2pi, 0x00, 2, 322, NACL_OPCODE_NULL_OFFSET  },
  /* 451 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstCvtps2pi, 0x00, 2, 322, NACL_OPCODE_NULL_OFFSET  },
  /* 452 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstUcomiss, 0x00, 2, 324, NACL_OPCODE_NULL_OFFSET  },
  /* 453 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstComiss, 0x00, 2, 326, NACL_OPCODE_NULL_OFFSET  },
  /* 454 */
  { NACLi_RDMSR,
    NACL_IFLAG(NaClIllegal),
    InstWrmsr, 0x00, 3, 328, NACL_OPCODE_NULL_OFFSET  },
  /* 455 */
  { NACLi_RDTSC,
    NACL_EMPTY_IFLAGS,
    InstRdtsc, 0x00, 2, 331, NACL_OPCODE_NULL_OFFSET  },
  /* 456 */
  { NACLi_RDMSR,
    NACL_IFLAG(NaClIllegal),
    InstRdmsr, 0x00, 3, 333, NACL_OPCODE_NULL_OFFSET  },
  /* 457 */
  { NACLi_SYSTEM,
    NACL_IFLAG(NaClIllegal),
    InstRdpmc, 0x00, 3, 333, NACL_OPCODE_NULL_OFFSET  },
  /* 458 */
  { NACLi_SYSENTER,
    NACL_IFLAG(NaClIllegal),
    InstSysenter, 0x00, 4, 336, NACL_OPCODE_NULL_OFFSET  },
  /* 459 */
  { NACLi_SYSENTER,
    NACL_IFLAG(NaClIllegal),
    InstSysexit, 0x00, 6, 340, NACL_OPCODE_NULL_OFFSET  },
  /* 460 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCmovo, 0x00, 2, 346, NACL_OPCODE_NULL_OFFSET  },
  /* 461 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCmovno, 0x00, 2, 346, NACL_OPCODE_NULL_OFFSET  },
  /* 462 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCmovb, 0x00, 2, 346, NACL_OPCODE_NULL_OFFSET  },
  /* 463 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCmovnb, 0x00, 2, 346, NACL_OPCODE_NULL_OFFSET  },
  /* 464 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCmovz, 0x00, 2, 346, NACL_OPCODE_NULL_OFFSET  },
  /* 465 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCmovnz, 0x00, 2, 346, NACL_OPCODE_NULL_OFFSET  },
  /* 466 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCmovbe, 0x00, 2, 346, NACL_OPCODE_NULL_OFFSET  },
  /* 467 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCmovnbe, 0x00, 2, 346, NACL_OPCODE_NULL_OFFSET  },
  /* 468 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCmovs, 0x00, 2, 346, NACL_OPCODE_NULL_OFFSET  },
  /* 469 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCmovns, 0x00, 2, 346, NACL_OPCODE_NULL_OFFSET  },
  /* 470 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCmovp, 0x00, 2, 346, NACL_OPCODE_NULL_OFFSET  },
  /* 471 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCmovnp, 0x00, 2, 346, NACL_OPCODE_NULL_OFFSET  },
  /* 472 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCmovl, 0x00, 2, 346, NACL_OPCODE_NULL_OFFSET  },
  /* 473 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCmovnl, 0x00, 2, 346, NACL_OPCODE_NULL_OFFSET  },
  /* 474 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCmovle, 0x00, 2, 346, NACL_OPCODE_NULL_OFFSET  },
  /* 475 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCmovnle, 0x00, 2, 346, NACL_OPCODE_NULL_OFFSET  },
  /* 476 */
  { NACLi_SSE,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMovmskps, 0x00, 2, 348, NACL_OPCODE_NULL_OFFSET  },
  /* 477 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstSqrtps, 0x00, 2, 298, NACL_OPCODE_NULL_OFFSET  },
  /* 478 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstRsqrtps, 0x00, 2, 298, NACL_OPCODE_NULL_OFFSET  },
  /* 479 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstRcpps, 0x00, 2, 298, NACL_OPCODE_NULL_OFFSET  },
  /* 480 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstAndps, 0x00, 2, 350, NACL_OPCODE_NULL_OFFSET  },
  /* 481 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstAndnps, 0x00, 2, 350, NACL_OPCODE_NULL_OFFSET  },
  /* 482 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstOrps, 0x00, 2, 350, NACL_OPCODE_NULL_OFFSET  },
  /* 483 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstXorps, 0x00, 2, 350, NACL_OPCODE_NULL_OFFSET  },
  /* 484 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstAddps, 0x00, 2, 350, NACL_OPCODE_NULL_OFFSET  },
  /* 485 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstMulps, 0x00, 2, 350, NACL_OPCODE_NULL_OFFSET  },
  /* 486 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm),
    InstCvtps2pd, 0x00, 2, 352, NACL_OPCODE_NULL_OFFSET  },
  /* 487 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm),
    InstCvtdq2ps, 0x00, 2, 354, NACL_OPCODE_NULL_OFFSET  },
  /* 488 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstSubps, 0x00, 2, 350, NACL_OPCODE_NULL_OFFSET  },
  /* 489 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstMinps, 0x00, 2, 350, NACL_OPCODE_NULL_OFFSET  },
  /* 490 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstDivps, 0x00, 2, 350, NACL_OPCODE_NULL_OFFSET  },
  /* 491 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm),
    InstMaxps, 0x00, 2, 350, NACL_OPCODE_NULL_OFFSET  },
  /* 492 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPunpcklbw, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 493 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPunpcklwd, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 494 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPunpckldq, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 495 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPacksswb, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 496 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPcmpgtb, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 497 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPcmpgtw, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 498 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPcmpgtd, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 499 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPackuswb, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 500 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPunpckhbw, 0x00, 2, 358, NACL_OPCODE_NULL_OFFSET  },
  /* 501 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPunpckhwd, 0x00, 2, 358, NACL_OPCODE_NULL_OFFSET  },
  /* 502 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPunpckhdq, 0x00, 2, 358, NACL_OPCODE_NULL_OFFSET  },
  /* 503 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPackssdw, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 504 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_o),
    InstMovq, 0x00, 2, 360, NACL_OPCODE_NULL_OFFSET  },
  /* 505 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstMovd, 0x00, 2, 362, 504  },
  /* 506 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstMovq, 0x00, 2, 364, NACL_OPCODE_NULL_OFFSET  },
  /* 507 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstPshufw, 0x00, 3, 366, NACL_OPCODE_NULL_OFFSET  },
  /* 508 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstPsllw, 0x06, 2, 369, 109  },
  /* 509 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x05, 0, 0, 508  },
  /* 510 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstPsraw, 0x04, 2, 369, 509  },
  /* 511 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x03, 0, 0, 510  },
  /* 512 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstPsrlw, 0x02, 2, 369, 511  },
  /* 513 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x01, 0, 0, 512  },
  /* 514 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00, 0, 0, 513  },
  /* 515 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstPslld, 0x06, 2, 369, 109  },
  /* 516 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x05, 0, 0, 515  },
  /* 517 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstPsrad, 0x04, 2, 369, 516  },
  /* 518 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x03, 0, 0, 517  },
  /* 519 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstPsrld, 0x02, 2, 369, 518  },
  /* 520 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x01, 0, 0, 519  },
  /* 521 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00, 0, 0, 520  },
  /* 522 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstPsllq, 0x06, 2, 369, 109  },
  /* 523 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x05, 0, 0, 522  },
  /* 524 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x04, 0, 0, 523  },
  /* 525 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x03, 0, 0, 524  },
  /* 526 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstPsrlq, 0x02, 2, 369, 525  },
  /* 527 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x01, 0, 0, 526  },
  /* 528 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00, 0, 0, 527  },
  /* 529 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPcmpeqb, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 530 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPcmpeqw, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 531 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPcmpeqd, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 532 */
  { NACLi_MMX,
    NACL_EMPTY_IFLAGS,
    InstEmms, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 533 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_o),
    InstMovq, 0x00, 2, 371, NACL_OPCODE_NULL_OFFSET  },
  /* 534 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstMovd, 0x00, 2, 373, 533  },
  /* 535 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstMovq, 0x00, 2, 375, NACL_OPCODE_NULL_OFFSET  },
  /* 536 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64) | NACL_IFLAG(ConditionalJump) | NACL_IFLAG(BranchHints),
    InstJo, 0x00, 2, 244, NACL_OPCODE_NULL_OFFSET  },
  /* 537 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64) | NACL_IFLAG(ConditionalJump) | NACL_IFLAG(BranchHints),
    InstJno, 0x00, 2, 244, NACL_OPCODE_NULL_OFFSET  },
  /* 538 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64) | NACL_IFLAG(ConditionalJump) | NACL_IFLAG(BranchHints),
    InstJb, 0x00, 2, 244, NACL_OPCODE_NULL_OFFSET  },
  /* 539 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64) | NACL_IFLAG(ConditionalJump) | NACL_IFLAG(BranchHints),
    InstJnb, 0x00, 2, 244, NACL_OPCODE_NULL_OFFSET  },
  /* 540 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64) | NACL_IFLAG(ConditionalJump) | NACL_IFLAG(BranchHints),
    InstJz, 0x00, 2, 244, NACL_OPCODE_NULL_OFFSET  },
  /* 541 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64) | NACL_IFLAG(ConditionalJump) | NACL_IFLAG(BranchHints),
    InstJnz, 0x00, 2, 244, NACL_OPCODE_NULL_OFFSET  },
  /* 542 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64) | NACL_IFLAG(ConditionalJump) | NACL_IFLAG(BranchHints),
    InstJbe, 0x00, 2, 244, NACL_OPCODE_NULL_OFFSET  },
  /* 543 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64) | NACL_IFLAG(ConditionalJump) | NACL_IFLAG(BranchHints),
    InstJnbe, 0x00, 2, 244, NACL_OPCODE_NULL_OFFSET  },
  /* 544 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64) | NACL_IFLAG(ConditionalJump) | NACL_IFLAG(BranchHints),
    InstJs, 0x00, 2, 244, NACL_OPCODE_NULL_OFFSET  },
  /* 545 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64) | NACL_IFLAG(ConditionalJump) | NACL_IFLAG(BranchHints),
    InstJns, 0x00, 2, 244, NACL_OPCODE_NULL_OFFSET  },
  /* 546 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64) | NACL_IFLAG(ConditionalJump) | NACL_IFLAG(BranchHints),
    InstJp, 0x00, 2, 244, NACL_OPCODE_NULL_OFFSET  },
  /* 547 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64) | NACL_IFLAG(ConditionalJump) | NACL_IFLAG(BranchHints),
    InstJnp, 0x00, 2, 244, NACL_OPCODE_NULL_OFFSET  },
  /* 548 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64) | NACL_IFLAG(ConditionalJump) | NACL_IFLAG(BranchHints),
    InstJl, 0x00, 2, 244, NACL_OPCODE_NULL_OFFSET  },
  /* 549 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64) | NACL_IFLAG(ConditionalJump) | NACL_IFLAG(BranchHints),
    InstJnl, 0x00, 2, 244, NACL_OPCODE_NULL_OFFSET  },
  /* 550 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64) | NACL_IFLAG(ConditionalJump) | NACL_IFLAG(BranchHints),
    InstJle, 0x00, 2, 244, NACL_OPCODE_NULL_OFFSET  },
  /* 551 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeDefaultIs64) | NACL_IFLAG(ConditionalJump) | NACL_IFLAG(BranchHints),
    InstJnle, 0x00, 2, 244, NACL_OPCODE_NULL_OFFSET  },
  /* 552 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSeto, 0x00, 1, 76, NACL_OPCODE_NULL_OFFSET  },
  /* 553 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetno, 0x00, 1, 76, NACL_OPCODE_NULL_OFFSET  },
  /* 554 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetb, 0x00, 1, 76, NACL_OPCODE_NULL_OFFSET  },
  /* 555 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetnb, 0x00, 1, 76, NACL_OPCODE_NULL_OFFSET  },
  /* 556 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetz, 0x00, 1, 76, NACL_OPCODE_NULL_OFFSET  },
  /* 557 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetnz, 0x00, 1, 76, NACL_OPCODE_NULL_OFFSET  },
  /* 558 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetbe, 0x00, 1, 76, NACL_OPCODE_NULL_OFFSET  },
  /* 559 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetnbe, 0x00, 1, 76, NACL_OPCODE_NULL_OFFSET  },
  /* 560 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSets, 0x00, 1, 76, NACL_OPCODE_NULL_OFFSET  },
  /* 561 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetns, 0x00, 1, 76, NACL_OPCODE_NULL_OFFSET  },
  /* 562 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetp, 0x00, 1, 76, NACL_OPCODE_NULL_OFFSET  },
  /* 563 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetnp, 0x00, 1, 76, NACL_OPCODE_NULL_OFFSET  },
  /* 564 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetl, 0x00, 1, 76, NACL_OPCODE_NULL_OFFSET  },
  /* 565 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetnl, 0x00, 1, 76, NACL_OPCODE_NULL_OFFSET  },
  /* 566 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetle, 0x00, 1, 76, NACL_OPCODE_NULL_OFFSET  },
  /* 567 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstSetnle, 0x00, 1, 76, NACL_OPCODE_NULL_OFFSET  },
  /* 568 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPush, 0x00, 2, 377, NACL_OPCODE_NULL_OFFSET  },
  /* 569 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPop, 0x00, 2, 379, NACL_OPCODE_NULL_OFFSET  },
  /* 570 */
  { NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstCpuid, 0x00, 4, 381, NACL_OPCODE_NULL_OFFSET  },
  /* 571 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal),
    InstBt, 0x00, 2, 20, NACL_OPCODE_NULL_OFFSET  },
  /* 572 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstShld, 0x00, 3, 385, NACL_OPCODE_NULL_OFFSET  },
  /* 573 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstShld, 0x00, 3, 388, NACL_OPCODE_NULL_OFFSET  },
  /* 574 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPush, 0x00, 2, 391, NACL_OPCODE_NULL_OFFSET  },
  /* 575 */
  { NACLi_386,
    NACL_IFLAG(NaClIllegal) | NACL_IFLAG(OperandSizeDefaultIs64),
    InstPop, 0x00, 2, 393, NACL_OPCODE_NULL_OFFSET  },
  /* 576 */
  { NACLi_SYSTEM,
    NACL_IFLAG(NaClIllegal),
    InstRsm, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 577 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal),
    InstBts, 0x00, 2, 12, NACL_OPCODE_NULL_OFFSET  },
  /* 578 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstShrd, 0x00, 3, 395, NACL_OPCODE_NULL_OFFSET  },
  /* 579 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstShrd, 0x00, 3, 398, NACL_OPCODE_NULL_OFFSET  },
  /* 580 */
  { NACLi_SFENCE_CLFLUSH,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstClflush, 0x07, 1, 278, NACL_OPCODE_NULL_OFFSET  },
  /* 581 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x77, 0, 0, 580  },
  /* 582 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x67, 0, 0, 581  },
  /* 583 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x57, 0, 0, 582  },
  /* 584 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x47, 0, 0, 583  },
  /* 585 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x37, 0, 0, 584  },
  /* 586 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x27, 0, 0, 585  },
  /* 587 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x17, 0, 0, 586  },
  /* 588 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x76, 0, 0, 587  },
  /* 589 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x66, 0, 0, 588  },
  /* 590 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x56, 0, 0, 589  },
  /* 591 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x46, 0, 0, 590  },
  /* 592 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x36, 0, 0, 591  },
  /* 593 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x26, 0, 0, 592  },
  /* 594 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x16, 0, 0, 593  },
  /* 595 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x75, 0, 0, 594  },
  /* 596 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x65, 0, 0, 595  },
  /* 597 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x55, 0, 0, 596  },
  /* 598 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x45, 0, 0, 597  },
  /* 599 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x35, 0, 0, 598  },
  /* 600 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x25, 0, 0, 599  },
  /* 601 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x15, 0, 0, 600  },
  /* 602 */
  { NACLi_SFENCE_CLFLUSH,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstSfence, 0x07, 0, 0, 601  },
  /* 603 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMfence, 0x06, 0, 0, 602  },
  /* 604 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstLfence, 0x05, 0, 0, 603  },
  /* 605 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x04, 0, 0, 604  },
  /* 606 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstStmxcsr, 0x03, 1, 197, 605  },
  /* 607 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstLdmxcsr, 0x02, 1, 190, 606  },
  /* 608 */
  { NACLi_FXSAVE,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstFxrstor, 0x01, 1, 196, 607  },
  /* 609 */
  { NACLi_FXSAVE,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstFxsave, 0x00, 1, 194, 608  },
  /* 610 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstImul, 0x00, 2, 6, NACL_OPCODE_NULL_OFFSET  },
  /* 611 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstCmpxchg, 0x00, 3, 401, NACL_OPCODE_NULL_OFFSET  },
  /* 612 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCmpxchg, 0x00, 3, 404, NACL_OPCODE_NULL_OFFSET  },
  /* 613 */
  { NACLi_386,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal),
    InstLss, 0x00, 2, 407, NACL_OPCODE_NULL_OFFSET  },
  /* 614 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal),
    InstBtr, 0x00, 2, 12, NACL_OPCODE_NULL_OFFSET  },
  /* 615 */
  { NACLi_386,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal),
    InstLfs, 0x00, 2, 407, NACL_OPCODE_NULL_OFFSET  },
  /* 616 */
  { NACLi_386,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal),
    InstLgs, 0x00, 2, 407, NACL_OPCODE_NULL_OFFSET  },
  /* 617 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstMovzx, 0x00, 2, 409, NACL_OPCODE_NULL_OFFSET  },
  /* 618 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstMovzx, 0x00, 2, 411, NACL_OPCODE_NULL_OFFSET  },
  /* 619 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstBtc, 0x07, 2, 70, 136  },
  /* 620 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstBtr, 0x06, 2, 70, 619  },
  /* 621 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstBts, 0x05, 2, 70, 620  },
  /* 622 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstBt, 0x04, 2, 44, 621  },
  /* 623 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) | NACL_IFLAG(NaClIllegal),
    InstBtc, 0x00, 2, 12, NACL_OPCODE_NULL_OFFSET  },
  /* 624 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstBsf, 0x00, 2, 346, NACL_OPCODE_NULL_OFFSET  },
  /* 625 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstBsr, 0x00, 2, 346, NACL_OPCODE_NULL_OFFSET  },
  /* 626 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstMovsx, 0x00, 2, 409, NACL_OPCODE_NULL_OFFSET  },
  /* 627 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstMovsx, 0x00, 2, 411, NACL_OPCODE_NULL_OFFSET  },
  /* 628 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstXadd, 0x00, 2, 72, NACL_OPCODE_NULL_OFFSET  },
  /* 629 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstXadd, 0x00, 2, 74, NACL_OPCODE_NULL_OFFSET  },
  /* 630 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstCmpps, 0x00, 3, 413, NACL_OPCODE_NULL_OFFSET  },
  /* 631 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstMovnti, 0x00, 2, 416, NACL_OPCODE_NULL_OFFSET  },
  /* 632 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstPinsrw, 0x00, 3, 418, NACL_OPCODE_NULL_OFFSET  },
  /* 633 */
  { NACLi_SSE41,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstPextrw, 0x00, 3, 421, NACL_OPCODE_NULL_OFFSET  },
  /* 634 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstShufps, 0x00, 3, 413, NACL_OPCODE_NULL_OFFSET  },
  /* 635 */
  { NACLi_CMPXCHG16B,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_o),
    InstCmpxchg16b, 0x01, 3, 424, 136  },
  /* 636 */
  { NACLi_CMPXCHG8B,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_v),
    InstCmpxchg8b, 0x01, 3, 427, 635  },
  /* 637 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_o),
    InstBswap, 0x00, 1, 430, NACL_OPCODE_NULL_OFFSET  },
  /* 638 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_v),
    InstBswap, 0x00, 1, 431, 637  },
  /* 639 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_o),
    InstBswap, 0x01, 1, 430, NACL_OPCODE_NULL_OFFSET  },
  /* 640 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_v),
    InstBswap, 0x01, 1, 431, 639  },
  /* 641 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_o),
    InstBswap, 0x02, 1, 430, NACL_OPCODE_NULL_OFFSET  },
  /* 642 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_v),
    InstBswap, 0x02, 1, 431, 641  },
  /* 643 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_o),
    InstBswap, 0x03, 1, 430, NACL_OPCODE_NULL_OFFSET  },
  /* 644 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_v),
    InstBswap, 0x03, 1, 431, 643  },
  /* 645 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_o),
    InstBswap, 0x04, 1, 430, NACL_OPCODE_NULL_OFFSET  },
  /* 646 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_v),
    InstBswap, 0x04, 1, 431, 645  },
  /* 647 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_o),
    InstBswap, 0x05, 1, 430, NACL_OPCODE_NULL_OFFSET  },
  /* 648 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_v),
    InstBswap, 0x05, 1, 431, 647  },
  /* 649 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_o),
    InstBswap, 0x06, 1, 430, NACL_OPCODE_NULL_OFFSET  },
  /* 650 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_v),
    InstBswap, 0x06, 1, 431, 649  },
  /* 651 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_o),
    InstBswap, 0x07, 1, 430, NACL_OPCODE_NULL_OFFSET  },
  /* 652 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_v),
    InstBswap, 0x07, 1, 431, 651  },
  /* 653 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsrlw, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 654 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsrld, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 655 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsrlq, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 656 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPaddq, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 657 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPmullw, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 658 */
  { NACLi_MMX,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstPmovmskb, 0x00, 2, 421, NACL_OPCODE_NULL_OFFSET  },
  /* 659 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsubusb, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 660 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsubusw, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 661 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPminub, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 662 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPand, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 663 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPaddusb, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 664 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPaddusw, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 665 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPmaxub, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 666 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPandn, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 667 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPavgb, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 668 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsraw, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 669 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsrad, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 670 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPavgw, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 671 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPmulhuw, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 672 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPmulhw, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 673 */
  { NACLi_MMX,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm),
    InstMovntq, 0x00, 2, 432, NACL_OPCODE_NULL_OFFSET  },
  /* 674 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsubsb, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 675 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsubsw, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 676 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPminsw, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 677 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPor, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 678 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPaddsb, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 679 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPaddsw, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 680 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPmaxsw, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 681 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPxor, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 682 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsllw, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 683 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPslld, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 684 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsllq, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 685 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPmuludq, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 686 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPmaddwd, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 687 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsadbw, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 688 */
  { NACLi_MMX,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_v),
    InstMaskmovq, 0x00, 3, 434, NACL_OPCODE_NULL_OFFSET  },
  /* 689 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsubb, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 690 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsubw, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 691 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsubd, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 692 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsubq, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 693 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPaddb, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 694 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPaddw, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 695 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPaddd, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 696 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstMovsd, 0x00, 2, 437, NACL_OPCODE_NULL_OFFSET  },
  /* 697 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstMovsd, 0x00, 2, 439, NACL_OPCODE_NULL_OFFSET  },
  /* 698 */
  { NACLi_SSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstMovddup, 0x00, 2, 441, NACL_OPCODE_NULL_OFFSET  },
  /* 699 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 700 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCvtsi2sd, 0x00, 2, 443, NACL_OPCODE_NULL_OFFSET  },
  /* 701 */
  { NACLi_SSE4A,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstMovntsd, 0x00, 2, 445, NACL_OPCODE_NULL_OFFSET  },
  /* 702 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCvttsd2si, 0x00, 2, 447, NACL_OPCODE_NULL_OFFSET  },
  /* 703 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCvtsd2si, 0x00, 2, 447, NACL_OPCODE_NULL_OFFSET  },
  /* 704 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstSqrtsd, 0x00, 2, 437, NACL_OPCODE_NULL_OFFSET  },
  /* 705 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstAddsd, 0x00, 2, 449, NACL_OPCODE_NULL_OFFSET  },
  /* 706 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstMulsd, 0x00, 2, 449, NACL_OPCODE_NULL_OFFSET  },
  /* 707 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstCvtsd2ss, 0x00, 2, 451, NACL_OPCODE_NULL_OFFSET  },
  /* 708 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstSubsd, 0x00, 2, 449, NACL_OPCODE_NULL_OFFSET  },
  /* 709 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstMinsd, 0x00, 2, 449, NACL_OPCODE_NULL_OFFSET  },
  /* 710 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstDivsd, 0x00, 2, 449, NACL_OPCODE_NULL_OFFSET  },
  /* 711 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstMaxsd, 0x00, 2, 449, NACL_OPCODE_NULL_OFFSET  },
  /* 712 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsRepne),
    InstPshuflw, 0x00, 3, 453, NACL_OPCODE_NULL_OFFSET  },
  /* 713 */
  { NACLi_SSE4A,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeHasImmed2_b) | NACL_IFLAG(OpcodeAllowsRepne),
    InstInsertq, 0x00, 4, 456, NACL_OPCODE_NULL_OFFSET  },
  /* 714 */
  { NACLi_SSE4A,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstInsertq, 0x00, 2, 460, NACL_OPCODE_NULL_OFFSET  },
  /* 715 */
  { NACLi_SSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstHaddps, 0x00, 2, 350, NACL_OPCODE_NULL_OFFSET  },
  /* 716 */
  { NACLi_SSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstHsubps, 0x00, 2, 350, NACL_OPCODE_NULL_OFFSET  },
  /* 717 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsRepne),
    InstCmpsd_xmm, 0x00, 3, 462, NACL_OPCODE_NULL_OFFSET  },
  /* 718 */
  { NACLi_SSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstAddsubps, 0x00, 2, 465, NACL_OPCODE_NULL_OFFSET  },
  /* 719 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstMovdq2q, 0x00, 2, 467, NACL_OPCODE_NULL_OFFSET  },
  /* 720 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstCvtpd2dq, 0x00, 2, 469, NACL_OPCODE_NULL_OFFSET  },
  /* 721 */
  { NACLi_SSE3,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne),
    InstLddqu, 0x00, 2, 471, NACL_OPCODE_NULL_OFFSET  },
  /* 722 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMovss, 0x00, 2, 473, NACL_OPCODE_NULL_OFFSET  },
  /* 723 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMovss, 0x00, 2, 475, NACL_OPCODE_NULL_OFFSET  },
  /* 724 */
  { NACLi_SSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMovsldup, 0x00, 2, 298, NACL_OPCODE_NULL_OFFSET  },
  /* 725 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 726 */
  { NACLi_SSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMovshdup, 0x00, 2, 298, NACL_OPCODE_NULL_OFFSET  },
  /* 727 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCvtsi2ss, 0x00, 2, 477, NACL_OPCODE_NULL_OFFSET  },
  /* 728 */
  { NACLi_SSE4A,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMovntss, 0x00, 2, 479, NACL_OPCODE_NULL_OFFSET  },
  /* 729 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCvttss2si, 0x00, 2, 481, NACL_OPCODE_NULL_OFFSET  },
  /* 730 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCvtss2si, 0x00, 2, 481, NACL_OPCODE_NULL_OFFSET  },
  /* 731 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstSqrtss, 0x00, 2, 298, NACL_OPCODE_NULL_OFFSET  },
  /* 732 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstRsqrtss, 0x00, 2, 473, NACL_OPCODE_NULL_OFFSET  },
  /* 733 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstRcpss, 0x00, 2, 473, NACL_OPCODE_NULL_OFFSET  },
  /* 734 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstAddss, 0x00, 2, 483, NACL_OPCODE_NULL_OFFSET  },
  /* 735 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMulss, 0x00, 2, 483, NACL_OPCODE_NULL_OFFSET  },
  /* 736 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstCvtss2sd, 0x00, 2, 485, NACL_OPCODE_NULL_OFFSET  },
  /* 737 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstCvttps2dq, 0x00, 2, 487, NACL_OPCODE_NULL_OFFSET  },
  /* 738 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstSubss, 0x00, 2, 483, NACL_OPCODE_NULL_OFFSET  },
  /* 739 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMinss, 0x00, 2, 483, NACL_OPCODE_NULL_OFFSET  },
  /* 740 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstDivss, 0x00, 2, 483, NACL_OPCODE_NULL_OFFSET  },
  /* 741 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMaxss, 0x00, 2, 483, NACL_OPCODE_NULL_OFFSET  },
  /* 742 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMovdqu, 0x00, 2, 489, NACL_OPCODE_NULL_OFFSET  },
  /* 743 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsRep),
    InstPshufhw, 0x00, 3, 453, NACL_OPCODE_NULL_OFFSET  },
  /* 744 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMovq, 0x00, 2, 453, NACL_OPCODE_NULL_OFFSET  },
  /* 745 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMovdqu, 0x00, 2, 491, NACL_OPCODE_NULL_OFFSET  },
  /* 746 */
  { NACLi_POPCNT,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstPopcnt, 0x00, 2, 346, NACL_OPCODE_NULL_OFFSET  },
  /* 747 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstTzcnt, 0x00, 2, 346, NACL_OPCODE_NULL_OFFSET  },
  /* 748 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstLzcnt, 0x00, 2, 346, NACL_OPCODE_NULL_OFFSET  },
  /* 749 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsRep),
    InstCmpss, 0x00, 3, 493, NACL_OPCODE_NULL_OFFSET  },
  /* 750 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstMovq2dq, 0x00, 2, 496, NACL_OPCODE_NULL_OFFSET  },
  /* 751 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep),
    InstCvtdq2pd, 0x00, 2, 498, NACL_OPCODE_NULL_OFFSET  },
  /* 752 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovupd, 0x00, 2, 500, NACL_OPCODE_NULL_OFFSET  },
  /* 753 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovupd, 0x00, 2, 502, NACL_OPCODE_NULL_OFFSET  },
  /* 754 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovlpd, 0x00, 2, 504, NACL_OPCODE_NULL_OFFSET  },
  /* 755 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovlpd, 0x00, 2, 445, NACL_OPCODE_NULL_OFFSET  },
  /* 756 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstUnpcklpd, 0x00, 2, 506, NACL_OPCODE_NULL_OFFSET  },
  /* 757 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstUnpckhpd, 0x00, 2, 506, NACL_OPCODE_NULL_OFFSET  },
  /* 758 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovhpd, 0x00, 2, 504, NACL_OPCODE_NULL_OFFSET  },
  /* 759 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovhpd, 0x00, 2, 445, NACL_OPCODE_NULL_OFFSET  },
  /* 760 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovapd, 0x00, 2, 500, NACL_OPCODE_NULL_OFFSET  },
  /* 761 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovapd, 0x00, 2, 502, NACL_OPCODE_NULL_OFFSET  },
  /* 762 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstCvtpi2pd, 0x00, 2, 508, NACL_OPCODE_NULL_OFFSET  },
  /* 763 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovntpd, 0x00, 2, 510, NACL_OPCODE_NULL_OFFSET  },
  /* 764 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstCvttpd2pi, 0x00, 2, 512, NACL_OPCODE_NULL_OFFSET  },
  /* 765 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstCvtpd2pi, 0x00, 2, 512, NACL_OPCODE_NULL_OFFSET  },
  /* 766 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstUcomisd, 0x00, 2, 514, NACL_OPCODE_NULL_OFFSET  },
  /* 767 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstComisd, 0x00, 2, 516, NACL_OPCODE_NULL_OFFSET  },
  /* 768 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovmskpd, 0x00, 2, 518, NACL_OPCODE_NULL_OFFSET  },
  /* 769 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstSqrtpd, 0x00, 2, 520, NACL_OPCODE_NULL_OFFSET  },
  /* 770 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 771 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstAndpd, 0x00, 2, 465, NACL_OPCODE_NULL_OFFSET  },
  /* 772 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstAndnpd, 0x00, 2, 465, NACL_OPCODE_NULL_OFFSET  },
  /* 773 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstOrpd, 0x00, 2, 465, NACL_OPCODE_NULL_OFFSET  },
  /* 774 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstXorpd, 0x00, 2, 465, NACL_OPCODE_NULL_OFFSET  },
  /* 775 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstAddpd, 0x00, 2, 465, NACL_OPCODE_NULL_OFFSET  },
  /* 776 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMulpd, 0x00, 2, 465, NACL_OPCODE_NULL_OFFSET  },
  /* 777 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstCvtpd2ps, 0x00, 2, 520, NACL_OPCODE_NULL_OFFSET  },
  /* 778 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstCvtps2dq, 0x00, 2, 487, NACL_OPCODE_NULL_OFFSET  },
  /* 779 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstSubpd, 0x00, 2, 465, NACL_OPCODE_NULL_OFFSET  },
  /* 780 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMinpd, 0x00, 2, 465, NACL_OPCODE_NULL_OFFSET  },
  /* 781 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstDivpd, 0x00, 2, 465, NACL_OPCODE_NULL_OFFSET  },
  /* 782 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMaxpd, 0x00, 2, 465, NACL_OPCODE_NULL_OFFSET  },
  /* 783 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPunpcklbw, 0x00, 2, 522, NACL_OPCODE_NULL_OFFSET  },
  /* 784 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPunpcklwd, 0x00, 2, 522, NACL_OPCODE_NULL_OFFSET  },
  /* 785 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPunpckldq, 0x00, 2, 522, NACL_OPCODE_NULL_OFFSET  },
  /* 786 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPacksswb, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 787 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPcmpgtb, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 788 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPcmpgtw, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 789 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPcmpgtd, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 790 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPackuswb, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 791 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPunpckhbw, 0x00, 2, 522, NACL_OPCODE_NULL_OFFSET  },
  /* 792 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPunpckhwd, 0x00, 2, 522, NACL_OPCODE_NULL_OFFSET  },
  /* 793 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPunpckhdq, 0x00, 2, 522, NACL_OPCODE_NULL_OFFSET  },
  /* 794 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPackssdw, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 795 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPunpcklqdq, 0x00, 2, 522, NACL_OPCODE_NULL_OFFSET  },
  /* 796 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPunpckhqdq, 0x00, 2, 522, NACL_OPCODE_NULL_OFFSET  },
  /* 797 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_o),
    InstMovq, 0x00, 2, 526, NACL_OPCODE_NULL_OFFSET  },
  /* 798 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstMovd, 0x00, 2, 528, 797  },
  /* 799 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovdqa, 0x00, 2, 489, NACL_OPCODE_NULL_OFFSET  },
  /* 800 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPshufd, 0x00, 3, 530, NACL_OPCODE_NULL_OFFSET  },
  /* 801 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x07, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 802 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsllw, 0x06, 2, 533, 801  },
  /* 803 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x05, 0, 0, 802  },
  /* 804 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsraw, 0x04, 2, 533, 803  },
  /* 805 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x03, 0, 0, 804  },
  /* 806 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsrlw, 0x02, 2, 533, 805  },
  /* 807 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x01, 0, 0, 806  },
  /* 808 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00, 0, 0, 807  },
  /* 809 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPslld, 0x06, 2, 533, 801  },
  /* 810 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x05, 0, 0, 809  },
  /* 811 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsrad, 0x04, 2, 533, 810  },
  /* 812 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x03, 0, 0, 811  },
  /* 813 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsrld, 0x02, 2, 533, 812  },
  /* 814 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x01, 0, 0, 813  },
  /* 815 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00, 0, 0, 814  },
  /* 816 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPslldq, 0x07, 2, 533, NACL_OPCODE_NULL_OFFSET  },
  /* 817 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsllq, 0x06, 2, 533, 816  },
  /* 818 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x05, 0, 0, 817  },
  /* 819 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x04, 0, 0, 818  },
  /* 820 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsrldq, 0x03, 2, 533, 819  },
  /* 821 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsrlq, 0x02, 2, 533, 820  },
  /* 822 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x01, 0, 0, 821  },
  /* 823 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00, 0, 0, 822  },
  /* 824 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPcmpeqb, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 825 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPcmpeqw, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 826 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPcmpeqd, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 827 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 828 */
  { NACLi_SSE4A,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeHasImmed2_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstExtrq, 0x00, 3, 535, 827  },
  /* 829 */
  { NACLi_SSE4A,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstExtrq, 0x00, 2, 460, NACL_OPCODE_NULL_OFFSET  },
  /* 830 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstHaddpd, 0x00, 2, 465, NACL_OPCODE_NULL_OFFSET  },
  /* 831 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstHsubpd, 0x00, 2, 465, NACL_OPCODE_NULL_OFFSET  },
  /* 832 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_o),
    InstMovq, 0x00, 2, 538, NACL_OPCODE_NULL_OFFSET  },
  /* 833 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstMovd, 0x00, 2, 540, 832  },
  /* 834 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovdqa, 0x00, 2, 491, NACL_OPCODE_NULL_OFFSET  },
  /* 835 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstCmppd, 0x00, 3, 542, NACL_OPCODE_NULL_OFFSET  },
  /* 836 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstPinsrw, 0x00, 3, 545, NACL_OPCODE_NULL_OFFSET  },
  /* 837 */
  { NACLi_SSE41,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPextrw, 0x00, 3, 548, NACL_OPCODE_NULL_OFFSET  },
  /* 838 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstShufpd, 0x00, 3, 542, NACL_OPCODE_NULL_OFFSET  },
  /* 839 */
  { NACLi_SSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstAddsubpd, 0x00, 2, 465, NACL_OPCODE_NULL_OFFSET  },
  /* 840 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsrlw, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 841 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsrld, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 842 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsrlq, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 843 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPaddq, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 844 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmullw, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 845 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovq, 0x00, 2, 551, NACL_OPCODE_NULL_OFFSET  },
  /* 846 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovmskb, 0x00, 2, 548, NACL_OPCODE_NULL_OFFSET  },
  /* 847 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsubusb, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 848 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsubusw, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 849 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPminub, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 850 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPand, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 851 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPaddusb, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 852 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPaddusw, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 853 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmaxub, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 854 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPandn, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 855 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPavgb, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 856 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsraw, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 857 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsrad, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 858 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPavgw, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 859 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmulhuw, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 860 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmulhw, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 861 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstCvttpd2dq, 0x00, 2, 553, NACL_OPCODE_NULL_OFFSET  },
  /* 862 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovntdq, 0x00, 2, 555, NACL_OPCODE_NULL_OFFSET  },
  /* 863 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsubsb, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 864 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsubsw, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 865 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPminsw, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 866 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPor, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 867 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPaddsb, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 868 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPaddsw, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 869 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmaxsw, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 870 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPxor, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 871 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsllw, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 872 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPslld, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 873 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsllq, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 874 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmuludq, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 875 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmaddwd, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 876 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsadbw, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 877 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstMaskmovdqu, 0x00, 3, 557, NACL_OPCODE_NULL_OFFSET  },
  /* 878 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsubb, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 879 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsubw, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 880 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsubd, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 881 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsubq, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 882 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPaddb, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 883 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPaddw, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 884 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPaddd, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 885 */
  { NACLi_E3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPi2fw, 0x00, 2, 364, NACL_OPCODE_NULL_OFFSET  },
  /* 886 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPi2fd, 0x00, 2, 364, NACL_OPCODE_NULL_OFFSET  },
  /* 887 */
  { NACLi_E3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPf2iw, 0x00, 2, 364, NACL_OPCODE_NULL_OFFSET  },
  /* 888 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPf2id, 0x00, 2, 364, NACL_OPCODE_NULL_OFFSET  },
  /* 889 */
  { NACLi_E3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfnacc, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 890 */
  { NACLi_E3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfpnacc, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 891 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfcmpge, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 892 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfmin, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 893 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfrcp, 0x00, 2, 364, NACL_OPCODE_NULL_OFFSET  },
  /* 894 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfrsqrt, 0x00, 2, 364, NACL_OPCODE_NULL_OFFSET  },
  /* 895 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfsub, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 896 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfadd, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 897 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfcmpgt, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 898 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfmax, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 899 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfrcpit1, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 900 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfrsqit1, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 901 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfsubr, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 902 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfacc, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 903 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfcmpeq, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 904 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfmul, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 905 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPfrcpit2, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 906 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPmulhrw, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 907 */
  { NACLi_E3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPswapd, 0x00, 2, 364, NACL_OPCODE_NULL_OFFSET  },
  /* 908 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPavgusb, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 909 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPshufb, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 910 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPhaddw, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 911 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPhaddd, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 912 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPhaddsw, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 913 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPmaddubsw, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 914 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPhsubw, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 915 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPhsubd, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 916 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPhsubsw, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 917 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsignb, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 918 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsignw, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 919 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPsignd, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 920 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPmulhrsw, 0x00, 2, 356, NACL_OPCODE_NULL_OFFSET  },
  /* 921 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPabsb, 0x00, 2, 364, NACL_OPCODE_NULL_OFFSET  },
  /* 922 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPabsw, 0x00, 2, 364, NACL_OPCODE_NULL_OFFSET  },
  /* 923 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm),
    InstPabsd, 0x00, 2, 364, NACL_OPCODE_NULL_OFFSET  },
  /* 924 */
  { NACLi_MOVBE,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstMovbe, 0x00, 2, 560, NACL_OPCODE_NULL_OFFSET  },
  /* 925 */
  { NACLi_MOVBE,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstMovbe, 0x00, 2, 562, NACL_OPCODE_NULL_OFFSET  },
  /* 926 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPshufb, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 927 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPhaddw, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 928 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPhaddd, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 929 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPhaddsw, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 930 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmaddubsw, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 931 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPhsubw, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 932 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPhsubd, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 933 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPhsubsw, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 934 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsignb, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 935 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsignw, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 936 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPsignd, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 937 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmulhrsw, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 938 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPblendvb, 0x00, 3, 564, NACL_OPCODE_NULL_OFFSET  },
  /* 939 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstBlendvps, 0x00, 3, 564, NACL_OPCODE_NULL_OFFSET  },
  /* 940 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstBlendvpd, 0x00, 3, 564, NACL_OPCODE_NULL_OFFSET  },
  /* 941 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPtest, 0x00, 2, 567, NACL_OPCODE_NULL_OFFSET  },
  /* 942 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPabsb, 0x00, 2, 489, NACL_OPCODE_NULL_OFFSET  },
  /* 943 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPabsw, 0x00, 2, 489, NACL_OPCODE_NULL_OFFSET  },
  /* 944 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPabsd, 0x00, 2, 489, NACL_OPCODE_NULL_OFFSET  },
  /* 945 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovsxbw, 0x00, 2, 569, NACL_OPCODE_NULL_OFFSET  },
  /* 946 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovsxbd, 0x00, 2, 571, NACL_OPCODE_NULL_OFFSET  },
  /* 947 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovsxbq, 0x00, 2, 573, NACL_OPCODE_NULL_OFFSET  },
  /* 948 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovsxwd, 0x00, 2, 569, NACL_OPCODE_NULL_OFFSET  },
  /* 949 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovsxwq, 0x00, 2, 571, NACL_OPCODE_NULL_OFFSET  },
  /* 950 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovsxdq, 0x00, 2, 569, NACL_OPCODE_NULL_OFFSET  },
  /* 951 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmuldq, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 952 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPcmpeqq, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 953 */
  { NACLi_SSE41,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMovntdqa, 0x00, 2, 471, NACL_OPCODE_NULL_OFFSET  },
  /* 954 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPackusdw, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 955 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovzxbw, 0x00, 2, 569, NACL_OPCODE_NULL_OFFSET  },
  /* 956 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovzxbd, 0x00, 2, 571, NACL_OPCODE_NULL_OFFSET  },
  /* 957 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovzxbq, 0x00, 2, 573, NACL_OPCODE_NULL_OFFSET  },
  /* 958 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovzxwd, 0x00, 2, 569, NACL_OPCODE_NULL_OFFSET  },
  /* 959 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovzxwq, 0x00, 2, 571, NACL_OPCODE_NULL_OFFSET  },
  /* 960 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmovzxdq, 0x00, 2, 569, NACL_OPCODE_NULL_OFFSET  },
  /* 961 */
  { NACLi_SSE42,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPcmpgtq, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 962 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPminsb, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 963 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPminsd, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 964 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPminuw, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 965 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPminud, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 966 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmaxsb, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 967 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmaxsd, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 968 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmaxuw, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 969 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmaxud, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 970 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPmulld, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 971 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPhminposuw, 0x00, 2, 524, NACL_OPCODE_NULL_OFFSET  },
  /* 972 */
  { NACLi_VMX,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvept, 0x00, 2, 575, NACL_OPCODE_NULL_OFFSET  },
  /* 973 */
  { NACLi_VMX,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvvpid, 0x00, 2, 575, NACL_OPCODE_NULL_OFFSET  },
  /* 974 */
  { NACLi_SSE42,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(OperandSize_b),
    InstCrc32, 0x00, 2, 577, NACL_OPCODE_NULL_OFFSET  },
  /* 975 */
  { NACLi_SSE42,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstCrc32, 0x00, 2, 579, NACL_OPCODE_NULL_OFFSET  },
  /* 976 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
    InstPalignr, 0x00, 3, 581, NACL_OPCODE_NULL_OFFSET  },
  /* 977 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstRoundps, 0x00, 3, 530, NACL_OPCODE_NULL_OFFSET  },
  /* 978 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstRoundpd, 0x00, 3, 530, NACL_OPCODE_NULL_OFFSET  },
  /* 979 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstRoundss, 0x00, 3, 584, NACL_OPCODE_NULL_OFFSET  },
  /* 980 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstRoundsd, 0x00, 3, 587, NACL_OPCODE_NULL_OFFSET  },
  /* 981 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstBlendps, 0x00, 3, 590, NACL_OPCODE_NULL_OFFSET  },
  /* 982 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstBlendpd, 0x00, 3, 590, NACL_OPCODE_NULL_OFFSET  },
  /* 983 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPblendw, 0x00, 3, 590, NACL_OPCODE_NULL_OFFSET  },
  /* 984 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPalignr, 0x00, 3, 590, NACL_OPCODE_NULL_OFFSET  },
  /* 985 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPextrb, 0x00, 3, 593, NACL_OPCODE_NULL_OFFSET  },
  /* 986 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPextrw, 0x00, 3, 596, NACL_OPCODE_NULL_OFFSET  },
  /* 987 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_o),
    InstPextrq, 0x00, 3, 599, NACL_OPCODE_NULL_OFFSET  },
  /* 988 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstPextrd, 0x00, 3, 602, 987  },
  /* 989 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstExtractps, 0x00, 3, 605, NACL_OPCODE_NULL_OFFSET  },
  /* 990 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstPinsrb, 0x00, 3, 608, NACL_OPCODE_NULL_OFFSET  },
  /* 991 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstInsertps, 0x00, 3, 611, NACL_OPCODE_NULL_OFFSET  },
  /* 992 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_o),
    InstPinsrq, 0x00, 3, 614, NACL_OPCODE_NULL_OFFSET  },
  /* 993 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v),
    InstPinsrd, 0x00, 3, 617, 992  },
  /* 994 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstDpps, 0x00, 3, 590, NACL_OPCODE_NULL_OFFSET  },
  /* 995 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstDppd, 0x00, 3, 590, NACL_OPCODE_NULL_OFFSET  },
  /* 996 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstMpsadbw, 0x00, 3, 590, NACL_OPCODE_NULL_OFFSET  },
  /* 997 */
  { NACLi_SSE42,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstPcmpestrm, 0x00, 6, 620, NACL_OPCODE_NULL_OFFSET  },
  /* 998 */
  { NACLi_SSE42,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstPcmpestri, 0x00, 6, 626, NACL_OPCODE_NULL_OFFSET  },
  /* 999 */
  { NACLi_SSE42,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16),
    InstPcmpistrm, 0x00, 4, 632, NACL_OPCODE_NULL_OFFSET  },
  /* 1000 */
  { NACLi_SSE42,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o),
    InstPcmpistri, 0x00, 4, 636, NACL_OPCODE_NULL_OFFSET  },
  /* 1001 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00, 2, 640, NACL_OPCODE_NULL_OFFSET  },
  /* 1002 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00, 2, 642, NACL_OPCODE_NULL_OFFSET  },
  /* 1003 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00, 2, 644, NACL_OPCODE_NULL_OFFSET  },
  /* 1004 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00, 2, 646, NACL_OPCODE_NULL_OFFSET  },
  /* 1005 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00, 2, 648, NACL_OPCODE_NULL_OFFSET  },
  /* 1006 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00, 2, 650, NACL_OPCODE_NULL_OFFSET  },
  /* 1007 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00, 2, 652, NACL_OPCODE_NULL_OFFSET  },
  /* 1008 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00, 2, 654, NACL_OPCODE_NULL_OFFSET  },
  /* 1009 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00, 2, 640, NACL_OPCODE_NULL_OFFSET  },
  /* 1010 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00, 2, 642, NACL_OPCODE_NULL_OFFSET  },
  /* 1011 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00, 2, 644, NACL_OPCODE_NULL_OFFSET  },
  /* 1012 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00, 2, 646, NACL_OPCODE_NULL_OFFSET  },
  /* 1013 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00, 2, 648, NACL_OPCODE_NULL_OFFSET  },
  /* 1014 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00, 2, 650, NACL_OPCODE_NULL_OFFSET  },
  /* 1015 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00, 2, 652, NACL_OPCODE_NULL_OFFSET  },
  /* 1016 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00, 2, 654, NACL_OPCODE_NULL_OFFSET  },
  /* 1017 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcom, 0x00, 2, 656, NACL_OPCODE_NULL_OFFSET  },
  /* 1018 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcom, 0x00, 2, 658, NACL_OPCODE_NULL_OFFSET  },
  /* 1019 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcom, 0x00, 2, 660, NACL_OPCODE_NULL_OFFSET  },
  /* 1020 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcom, 0x00, 2, 662, NACL_OPCODE_NULL_OFFSET  },
  /* 1021 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcom, 0x00, 2, 664, NACL_OPCODE_NULL_OFFSET  },
  /* 1022 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcom, 0x00, 2, 666, NACL_OPCODE_NULL_OFFSET  },
  /* 1023 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcom, 0x00, 2, 668, NACL_OPCODE_NULL_OFFSET  },
  /* 1024 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcom, 0x00, 2, 670, NACL_OPCODE_NULL_OFFSET  },
  /* 1025 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomp, 0x00, 2, 656, NACL_OPCODE_NULL_OFFSET  },
  /* 1026 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomp, 0x00, 2, 658, NACL_OPCODE_NULL_OFFSET  },
  /* 1027 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomp, 0x00, 2, 660, NACL_OPCODE_NULL_OFFSET  },
  /* 1028 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomp, 0x00, 2, 662, NACL_OPCODE_NULL_OFFSET  },
  /* 1029 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomp, 0x00, 2, 664, NACL_OPCODE_NULL_OFFSET  },
  /* 1030 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomp, 0x00, 2, 666, NACL_OPCODE_NULL_OFFSET  },
  /* 1031 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomp, 0x00, 2, 668, NACL_OPCODE_NULL_OFFSET  },
  /* 1032 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomp, 0x00, 2, 670, NACL_OPCODE_NULL_OFFSET  },
  /* 1033 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00, 2, 640, NACL_OPCODE_NULL_OFFSET  },
  /* 1034 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00, 2, 642, NACL_OPCODE_NULL_OFFSET  },
  /* 1035 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00, 2, 644, NACL_OPCODE_NULL_OFFSET  },
  /* 1036 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00, 2, 646, NACL_OPCODE_NULL_OFFSET  },
  /* 1037 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00, 2, 648, NACL_OPCODE_NULL_OFFSET  },
  /* 1038 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00, 2, 650, NACL_OPCODE_NULL_OFFSET  },
  /* 1039 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00, 2, 652, NACL_OPCODE_NULL_OFFSET  },
  /* 1040 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00, 2, 654, NACL_OPCODE_NULL_OFFSET  },
  /* 1041 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00, 2, 640, NACL_OPCODE_NULL_OFFSET  },
  /* 1042 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00, 2, 642, NACL_OPCODE_NULL_OFFSET  },
  /* 1043 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00, 2, 644, NACL_OPCODE_NULL_OFFSET  },
  /* 1044 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00, 2, 646, NACL_OPCODE_NULL_OFFSET  },
  /* 1045 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00, 2, 648, NACL_OPCODE_NULL_OFFSET  },
  /* 1046 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00, 2, 650, NACL_OPCODE_NULL_OFFSET  },
  /* 1047 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00, 2, 652, NACL_OPCODE_NULL_OFFSET  },
  /* 1048 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00, 2, 654, NACL_OPCODE_NULL_OFFSET  },
  /* 1049 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00, 2, 640, NACL_OPCODE_NULL_OFFSET  },
  /* 1050 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00, 2, 642, NACL_OPCODE_NULL_OFFSET  },
  /* 1051 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00, 2, 644, NACL_OPCODE_NULL_OFFSET  },
  /* 1052 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00, 2, 646, NACL_OPCODE_NULL_OFFSET  },
  /* 1053 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00, 2, 648, NACL_OPCODE_NULL_OFFSET  },
  /* 1054 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00, 2, 650, NACL_OPCODE_NULL_OFFSET  },
  /* 1055 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00, 2, 652, NACL_OPCODE_NULL_OFFSET  },
  /* 1056 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00, 2, 654, NACL_OPCODE_NULL_OFFSET  },
  /* 1057 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00, 2, 640, NACL_OPCODE_NULL_OFFSET  },
  /* 1058 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00, 2, 642, NACL_OPCODE_NULL_OFFSET  },
  /* 1059 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00, 2, 644, NACL_OPCODE_NULL_OFFSET  },
  /* 1060 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00, 2, 646, NACL_OPCODE_NULL_OFFSET  },
  /* 1061 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00, 2, 648, NACL_OPCODE_NULL_OFFSET  },
  /* 1062 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00, 2, 650, NACL_OPCODE_NULL_OFFSET  },
  /* 1063 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00, 2, 652, NACL_OPCODE_NULL_OFFSET  },
  /* 1064 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00, 2, 654, NACL_OPCODE_NULL_OFFSET  },
  /* 1065 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFld, 0x00, 2, 672, NACL_OPCODE_NULL_OFFSET  },
  /* 1066 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFld, 0x00, 2, 674, NACL_OPCODE_NULL_OFFSET  },
  /* 1067 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFld, 0x00, 2, 676, NACL_OPCODE_NULL_OFFSET  },
  /* 1068 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFld, 0x00, 2, 678, NACL_OPCODE_NULL_OFFSET  },
  /* 1069 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFld, 0x00, 2, 680, NACL_OPCODE_NULL_OFFSET  },
  /* 1070 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFld, 0x00, 2, 682, NACL_OPCODE_NULL_OFFSET  },
  /* 1071 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFld, 0x00, 2, 684, NACL_OPCODE_NULL_OFFSET  },
  /* 1072 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFld, 0x00, 2, 686, NACL_OPCODE_NULL_OFFSET  },
  /* 1073 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFxch, 0x00, 2, 688, NACL_OPCODE_NULL_OFFSET  },
  /* 1074 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFxch, 0x00, 2, 690, NACL_OPCODE_NULL_OFFSET  },
  /* 1075 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFxch, 0x00, 2, 692, NACL_OPCODE_NULL_OFFSET  },
  /* 1076 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFxch, 0x00, 2, 694, NACL_OPCODE_NULL_OFFSET  },
  /* 1077 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFxch, 0x00, 2, 696, NACL_OPCODE_NULL_OFFSET  },
  /* 1078 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFxch, 0x00, 2, 698, NACL_OPCODE_NULL_OFFSET  },
  /* 1079 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFxch, 0x00, 2, 700, NACL_OPCODE_NULL_OFFSET  },
  /* 1080 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFxch, 0x00, 2, 702, NACL_OPCODE_NULL_OFFSET  },
  /* 1081 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFnop, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 1082 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFchs, 0x00, 1, 189, NACL_OPCODE_NULL_OFFSET  },
  /* 1083 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFabs, 0x00, 1, 189, NACL_OPCODE_NULL_OFFSET  },
  /* 1084 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFtst, 0x00, 1, 191, NACL_OPCODE_NULL_OFFSET  },
  /* 1085 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFxam, 0x00, 1, 191, NACL_OPCODE_NULL_OFFSET  },
  /* 1086 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFld1, 0x00, 1, 189, NACL_OPCODE_NULL_OFFSET  },
  /* 1087 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFldl2t, 0x00, 1, 189, NACL_OPCODE_NULL_OFFSET  },
  /* 1088 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFldl2e, 0x00, 1, 189, NACL_OPCODE_NULL_OFFSET  },
  /* 1089 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFldpi, 0x00, 1, 189, NACL_OPCODE_NULL_OFFSET  },
  /* 1090 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFldlg2, 0x00, 1, 189, NACL_OPCODE_NULL_OFFSET  },
  /* 1091 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFldln2, 0x00, 1, 189, NACL_OPCODE_NULL_OFFSET  },
  /* 1092 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFldz, 0x00, 1, 189, NACL_OPCODE_NULL_OFFSET  },
  /* 1093 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstF2xm1, 0x00, 1, 189, NACL_OPCODE_NULL_OFFSET  },
  /* 1094 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFyl2x, 0x00, 2, 642, NACL_OPCODE_NULL_OFFSET  },
  /* 1095 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFptan, 0x00, 2, 674, NACL_OPCODE_NULL_OFFSET  },
  /* 1096 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFpatan, 0x00, 2, 642, NACL_OPCODE_NULL_OFFSET  },
  /* 1097 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFxtract, 0x00, 2, 674, NACL_OPCODE_NULL_OFFSET  },
  /* 1098 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFprem1, 0x00, 2, 642, NACL_OPCODE_NULL_OFFSET  },
  /* 1099 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdecstp, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 1100 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFincstp, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 1101 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFprem, 0x00, 2, 642, NACL_OPCODE_NULL_OFFSET  },
  /* 1102 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFyl2xp1, 0x00, 2, 642, NACL_OPCODE_NULL_OFFSET  },
  /* 1103 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsqrt, 0x00, 1, 189, NACL_OPCODE_NULL_OFFSET  },
  /* 1104 */
  { NACLi_X87_FSINCOS,
    NACL_EMPTY_IFLAGS,
    InstFsincos, 0x00, 2, 674, NACL_OPCODE_NULL_OFFSET  },
  /* 1105 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFrndint, 0x00, 1, 189, NACL_OPCODE_NULL_OFFSET  },
  /* 1106 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFscale, 0x00, 2, 642, NACL_OPCODE_NULL_OFFSET  },
  /* 1107 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsin, 0x00, 1, 189, NACL_OPCODE_NULL_OFFSET  },
  /* 1108 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcos, 0x00, 1, 189, NACL_OPCODE_NULL_OFFSET  },
  /* 1109 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovb, 0x00, 2, 640, NACL_OPCODE_NULL_OFFSET  },
  /* 1110 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovb, 0x00, 2, 642, NACL_OPCODE_NULL_OFFSET  },
  /* 1111 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovb, 0x00, 2, 644, NACL_OPCODE_NULL_OFFSET  },
  /* 1112 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovb, 0x00, 2, 646, NACL_OPCODE_NULL_OFFSET  },
  /* 1113 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovb, 0x00, 2, 648, NACL_OPCODE_NULL_OFFSET  },
  /* 1114 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovb, 0x00, 2, 650, NACL_OPCODE_NULL_OFFSET  },
  /* 1115 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovb, 0x00, 2, 652, NACL_OPCODE_NULL_OFFSET  },
  /* 1116 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovb, 0x00, 2, 654, NACL_OPCODE_NULL_OFFSET  },
  /* 1117 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmove, 0x00, 2, 640, NACL_OPCODE_NULL_OFFSET  },
  /* 1118 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmove, 0x00, 2, 642, NACL_OPCODE_NULL_OFFSET  },
  /* 1119 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmove, 0x00, 2, 644, NACL_OPCODE_NULL_OFFSET  },
  /* 1120 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmove, 0x00, 2, 646, NACL_OPCODE_NULL_OFFSET  },
  /* 1121 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmove, 0x00, 2, 648, NACL_OPCODE_NULL_OFFSET  },
  /* 1122 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmove, 0x00, 2, 650, NACL_OPCODE_NULL_OFFSET  },
  /* 1123 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmove, 0x00, 2, 652, NACL_OPCODE_NULL_OFFSET  },
  /* 1124 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmove, 0x00, 2, 654, NACL_OPCODE_NULL_OFFSET  },
  /* 1125 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovbe, 0x00, 2, 640, NACL_OPCODE_NULL_OFFSET  },
  /* 1126 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovbe, 0x00, 2, 642, NACL_OPCODE_NULL_OFFSET  },
  /* 1127 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovbe, 0x00, 2, 644, NACL_OPCODE_NULL_OFFSET  },
  /* 1128 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovbe, 0x00, 2, 646, NACL_OPCODE_NULL_OFFSET  },
  /* 1129 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovbe, 0x00, 2, 648, NACL_OPCODE_NULL_OFFSET  },
  /* 1130 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovbe, 0x00, 2, 650, NACL_OPCODE_NULL_OFFSET  },
  /* 1131 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovbe, 0x00, 2, 652, NACL_OPCODE_NULL_OFFSET  },
  /* 1132 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovbe, 0x00, 2, 654, NACL_OPCODE_NULL_OFFSET  },
  /* 1133 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovu, 0x00, 2, 640, NACL_OPCODE_NULL_OFFSET  },
  /* 1134 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovu, 0x00, 2, 642, NACL_OPCODE_NULL_OFFSET  },
  /* 1135 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovu, 0x00, 2, 644, NACL_OPCODE_NULL_OFFSET  },
  /* 1136 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovu, 0x00, 2, 646, NACL_OPCODE_NULL_OFFSET  },
  /* 1137 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovu, 0x00, 2, 648, NACL_OPCODE_NULL_OFFSET  },
  /* 1138 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovu, 0x00, 2, 650, NACL_OPCODE_NULL_OFFSET  },
  /* 1139 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovu, 0x00, 2, 652, NACL_OPCODE_NULL_OFFSET  },
  /* 1140 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovu, 0x00, 2, 654, NACL_OPCODE_NULL_OFFSET  },
  /* 1141 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucompp, 0x00, 2, 658, NACL_OPCODE_NULL_OFFSET  },
  /* 1142 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnb, 0x00, 2, 640, NACL_OPCODE_NULL_OFFSET  },
  /* 1143 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnb, 0x00, 2, 642, NACL_OPCODE_NULL_OFFSET  },
  /* 1144 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnb, 0x00, 2, 644, NACL_OPCODE_NULL_OFFSET  },
  /* 1145 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnb, 0x00, 2, 646, NACL_OPCODE_NULL_OFFSET  },
  /* 1146 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnb, 0x00, 2, 648, NACL_OPCODE_NULL_OFFSET  },
  /* 1147 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnb, 0x00, 2, 650, NACL_OPCODE_NULL_OFFSET  },
  /* 1148 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnb, 0x00, 2, 652, NACL_OPCODE_NULL_OFFSET  },
  /* 1149 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnb, 0x00, 2, 654, NACL_OPCODE_NULL_OFFSET  },
  /* 1150 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovne, 0x00, 2, 640, NACL_OPCODE_NULL_OFFSET  },
  /* 1151 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovne, 0x00, 2, 642, NACL_OPCODE_NULL_OFFSET  },
  /* 1152 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovne, 0x00, 2, 644, NACL_OPCODE_NULL_OFFSET  },
  /* 1153 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovne, 0x00, 2, 646, NACL_OPCODE_NULL_OFFSET  },
  /* 1154 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovne, 0x00, 2, 648, NACL_OPCODE_NULL_OFFSET  },
  /* 1155 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovne, 0x00, 2, 650, NACL_OPCODE_NULL_OFFSET  },
  /* 1156 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovne, 0x00, 2, 652, NACL_OPCODE_NULL_OFFSET  },
  /* 1157 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovne, 0x00, 2, 654, NACL_OPCODE_NULL_OFFSET  },
  /* 1158 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnbe, 0x00, 2, 640, NACL_OPCODE_NULL_OFFSET  },
  /* 1159 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnbe, 0x00, 2, 642, NACL_OPCODE_NULL_OFFSET  },
  /* 1160 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnbe, 0x00, 2, 644, NACL_OPCODE_NULL_OFFSET  },
  /* 1161 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnbe, 0x00, 2, 646, NACL_OPCODE_NULL_OFFSET  },
  /* 1162 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnbe, 0x00, 2, 648, NACL_OPCODE_NULL_OFFSET  },
  /* 1163 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnbe, 0x00, 2, 650, NACL_OPCODE_NULL_OFFSET  },
  /* 1164 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnbe, 0x00, 2, 652, NACL_OPCODE_NULL_OFFSET  },
  /* 1165 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnbe, 0x00, 2, 654, NACL_OPCODE_NULL_OFFSET  },
  /* 1166 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnu, 0x00, 2, 640, NACL_OPCODE_NULL_OFFSET  },
  /* 1167 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnu, 0x00, 2, 642, NACL_OPCODE_NULL_OFFSET  },
  /* 1168 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnu, 0x00, 2, 644, NACL_OPCODE_NULL_OFFSET  },
  /* 1169 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnu, 0x00, 2, 646, NACL_OPCODE_NULL_OFFSET  },
  /* 1170 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnu, 0x00, 2, 648, NACL_OPCODE_NULL_OFFSET  },
  /* 1171 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnu, 0x00, 2, 650, NACL_OPCODE_NULL_OFFSET  },
  /* 1172 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnu, 0x00, 2, 652, NACL_OPCODE_NULL_OFFSET  },
  /* 1173 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcmovnu, 0x00, 2, 654, NACL_OPCODE_NULL_OFFSET  },
  /* 1174 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFnclex, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 1175 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFninit, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 1176 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomi, 0x00, 2, 656, NACL_OPCODE_NULL_OFFSET  },
  /* 1177 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomi, 0x00, 2, 658, NACL_OPCODE_NULL_OFFSET  },
  /* 1178 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomi, 0x00, 2, 660, NACL_OPCODE_NULL_OFFSET  },
  /* 1179 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomi, 0x00, 2, 662, NACL_OPCODE_NULL_OFFSET  },
  /* 1180 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomi, 0x00, 2, 664, NACL_OPCODE_NULL_OFFSET  },
  /* 1181 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomi, 0x00, 2, 666, NACL_OPCODE_NULL_OFFSET  },
  /* 1182 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomi, 0x00, 2, 668, NACL_OPCODE_NULL_OFFSET  },
  /* 1183 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomi, 0x00, 2, 670, NACL_OPCODE_NULL_OFFSET  },
  /* 1184 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomi, 0x00, 2, 656, NACL_OPCODE_NULL_OFFSET  },
  /* 1185 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomi, 0x00, 2, 658, NACL_OPCODE_NULL_OFFSET  },
  /* 1186 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomi, 0x00, 2, 660, NACL_OPCODE_NULL_OFFSET  },
  /* 1187 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomi, 0x00, 2, 662, NACL_OPCODE_NULL_OFFSET  },
  /* 1188 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomi, 0x00, 2, 664, NACL_OPCODE_NULL_OFFSET  },
  /* 1189 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomi, 0x00, 2, 666, NACL_OPCODE_NULL_OFFSET  },
  /* 1190 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomi, 0x00, 2, 668, NACL_OPCODE_NULL_OFFSET  },
  /* 1191 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomi, 0x00, 2, 670, NACL_OPCODE_NULL_OFFSET  },
  /* 1192 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00, 2, 704, NACL_OPCODE_NULL_OFFSET  },
  /* 1193 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00, 2, 706, NACL_OPCODE_NULL_OFFSET  },
  /* 1194 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00, 2, 708, NACL_OPCODE_NULL_OFFSET  },
  /* 1195 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00, 2, 710, NACL_OPCODE_NULL_OFFSET  },
  /* 1196 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00, 2, 712, NACL_OPCODE_NULL_OFFSET  },
  /* 1197 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00, 2, 714, NACL_OPCODE_NULL_OFFSET  },
  /* 1198 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFadd, 0x00, 2, 716, NACL_OPCODE_NULL_OFFSET  },
  /* 1199 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00, 2, 704, NACL_OPCODE_NULL_OFFSET  },
  /* 1200 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00, 2, 706, NACL_OPCODE_NULL_OFFSET  },
  /* 1201 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00, 2, 708, NACL_OPCODE_NULL_OFFSET  },
  /* 1202 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00, 2, 710, NACL_OPCODE_NULL_OFFSET  },
  /* 1203 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00, 2, 712, NACL_OPCODE_NULL_OFFSET  },
  /* 1204 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00, 2, 714, NACL_OPCODE_NULL_OFFSET  },
  /* 1205 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmul, 0x00, 2, 716, NACL_OPCODE_NULL_OFFSET  },
  /* 1206 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00, 2, 704, NACL_OPCODE_NULL_OFFSET  },
  /* 1207 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00, 2, 706, NACL_OPCODE_NULL_OFFSET  },
  /* 1208 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00, 2, 708, NACL_OPCODE_NULL_OFFSET  },
  /* 1209 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00, 2, 710, NACL_OPCODE_NULL_OFFSET  },
  /* 1210 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00, 2, 712, NACL_OPCODE_NULL_OFFSET  },
  /* 1211 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00, 2, 714, NACL_OPCODE_NULL_OFFSET  },
  /* 1212 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubr, 0x00, 2, 716, NACL_OPCODE_NULL_OFFSET  },
  /* 1213 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00, 2, 704, NACL_OPCODE_NULL_OFFSET  },
  /* 1214 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00, 2, 706, NACL_OPCODE_NULL_OFFSET  },
  /* 1215 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00, 2, 708, NACL_OPCODE_NULL_OFFSET  },
  /* 1216 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00, 2, 710, NACL_OPCODE_NULL_OFFSET  },
  /* 1217 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00, 2, 712, NACL_OPCODE_NULL_OFFSET  },
  /* 1218 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00, 2, 714, NACL_OPCODE_NULL_OFFSET  },
  /* 1219 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsub, 0x00, 2, 716, NACL_OPCODE_NULL_OFFSET  },
  /* 1220 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00, 2, 704, NACL_OPCODE_NULL_OFFSET  },
  /* 1221 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00, 2, 706, NACL_OPCODE_NULL_OFFSET  },
  /* 1222 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00, 2, 708, NACL_OPCODE_NULL_OFFSET  },
  /* 1223 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00, 2, 710, NACL_OPCODE_NULL_OFFSET  },
  /* 1224 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00, 2, 712, NACL_OPCODE_NULL_OFFSET  },
  /* 1225 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00, 2, 714, NACL_OPCODE_NULL_OFFSET  },
  /* 1226 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivr, 0x00, 2, 716, NACL_OPCODE_NULL_OFFSET  },
  /* 1227 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00, 2, 704, NACL_OPCODE_NULL_OFFSET  },
  /* 1228 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00, 2, 706, NACL_OPCODE_NULL_OFFSET  },
  /* 1229 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00, 2, 708, NACL_OPCODE_NULL_OFFSET  },
  /* 1230 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00, 2, 710, NACL_OPCODE_NULL_OFFSET  },
  /* 1231 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00, 2, 712, NACL_OPCODE_NULL_OFFSET  },
  /* 1232 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00, 2, 714, NACL_OPCODE_NULL_OFFSET  },
  /* 1233 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdiv, 0x00, 2, 716, NACL_OPCODE_NULL_OFFSET  },
  /* 1234 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFfree, 0x00, 1, 718, NACL_OPCODE_NULL_OFFSET  },
  /* 1235 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFfree, 0x00, 1, 719, NACL_OPCODE_NULL_OFFSET  },
  /* 1236 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFfree, 0x00, 1, 720, NACL_OPCODE_NULL_OFFSET  },
  /* 1237 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFfree, 0x00, 1, 721, NACL_OPCODE_NULL_OFFSET  },
  /* 1238 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFfree, 0x00, 1, 722, NACL_OPCODE_NULL_OFFSET  },
  /* 1239 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFfree, 0x00, 1, 723, NACL_OPCODE_NULL_OFFSET  },
  /* 1240 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFfree, 0x00, 1, 724, NACL_OPCODE_NULL_OFFSET  },
  /* 1241 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFfree, 0x00, 1, 725, NACL_OPCODE_NULL_OFFSET  },
  /* 1242 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFst, 0x00, 2, 672, NACL_OPCODE_NULL_OFFSET  },
  /* 1243 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFst, 0x00, 2, 726, NACL_OPCODE_NULL_OFFSET  },
  /* 1244 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFst, 0x00, 2, 728, NACL_OPCODE_NULL_OFFSET  },
  /* 1245 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFst, 0x00, 2, 730, NACL_OPCODE_NULL_OFFSET  },
  /* 1246 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFst, 0x00, 2, 732, NACL_OPCODE_NULL_OFFSET  },
  /* 1247 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFst, 0x00, 2, 734, NACL_OPCODE_NULL_OFFSET  },
  /* 1248 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFst, 0x00, 2, 736, NACL_OPCODE_NULL_OFFSET  },
  /* 1249 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFst, 0x00, 2, 738, NACL_OPCODE_NULL_OFFSET  },
  /* 1250 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFstp, 0x00, 2, 672, NACL_OPCODE_NULL_OFFSET  },
  /* 1251 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFstp, 0x00, 2, 726, NACL_OPCODE_NULL_OFFSET  },
  /* 1252 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFstp, 0x00, 2, 728, NACL_OPCODE_NULL_OFFSET  },
  /* 1253 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFstp, 0x00, 2, 730, NACL_OPCODE_NULL_OFFSET  },
  /* 1254 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFstp, 0x00, 2, 732, NACL_OPCODE_NULL_OFFSET  },
  /* 1255 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFstp, 0x00, 2, 734, NACL_OPCODE_NULL_OFFSET  },
  /* 1256 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFstp, 0x00, 2, 736, NACL_OPCODE_NULL_OFFSET  },
  /* 1257 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFstp, 0x00, 2, 738, NACL_OPCODE_NULL_OFFSET  },
  /* 1258 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucom, 0x00, 2, 656, NACL_OPCODE_NULL_OFFSET  },
  /* 1259 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucom, 0x00, 2, 658, NACL_OPCODE_NULL_OFFSET  },
  /* 1260 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucom, 0x00, 2, 660, NACL_OPCODE_NULL_OFFSET  },
  /* 1261 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucom, 0x00, 2, 662, NACL_OPCODE_NULL_OFFSET  },
  /* 1262 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucom, 0x00, 2, 664, NACL_OPCODE_NULL_OFFSET  },
  /* 1263 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucom, 0x00, 2, 666, NACL_OPCODE_NULL_OFFSET  },
  /* 1264 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucom, 0x00, 2, 668, NACL_OPCODE_NULL_OFFSET  },
  /* 1265 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucom, 0x00, 2, 670, NACL_OPCODE_NULL_OFFSET  },
  /* 1266 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomp, 0x00, 2, 656, NACL_OPCODE_NULL_OFFSET  },
  /* 1267 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomp, 0x00, 2, 658, NACL_OPCODE_NULL_OFFSET  },
  /* 1268 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomp, 0x00, 2, 660, NACL_OPCODE_NULL_OFFSET  },
  /* 1269 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomp, 0x00, 2, 662, NACL_OPCODE_NULL_OFFSET  },
  /* 1270 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomp, 0x00, 2, 664, NACL_OPCODE_NULL_OFFSET  },
  /* 1271 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomp, 0x00, 2, 666, NACL_OPCODE_NULL_OFFSET  },
  /* 1272 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomp, 0x00, 2, 668, NACL_OPCODE_NULL_OFFSET  },
  /* 1273 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomp, 0x00, 2, 670, NACL_OPCODE_NULL_OFFSET  },
  /* 1274 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFaddp, 0x00, 2, 640, NACL_OPCODE_NULL_OFFSET  },
  /* 1275 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFaddp, 0x00, 2, 704, NACL_OPCODE_NULL_OFFSET  },
  /* 1276 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFaddp, 0x00, 2, 706, NACL_OPCODE_NULL_OFFSET  },
  /* 1277 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFaddp, 0x00, 2, 708, NACL_OPCODE_NULL_OFFSET  },
  /* 1278 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFaddp, 0x00, 2, 710, NACL_OPCODE_NULL_OFFSET  },
  /* 1279 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFaddp, 0x00, 2, 712, NACL_OPCODE_NULL_OFFSET  },
  /* 1280 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFaddp, 0x00, 2, 714, NACL_OPCODE_NULL_OFFSET  },
  /* 1281 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFaddp, 0x00, 2, 716, NACL_OPCODE_NULL_OFFSET  },
  /* 1282 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmulp, 0x00, 2, 640, NACL_OPCODE_NULL_OFFSET  },
  /* 1283 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmulp, 0x00, 2, 704, NACL_OPCODE_NULL_OFFSET  },
  /* 1284 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmulp, 0x00, 2, 706, NACL_OPCODE_NULL_OFFSET  },
  /* 1285 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmulp, 0x00, 2, 708, NACL_OPCODE_NULL_OFFSET  },
  /* 1286 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmulp, 0x00, 2, 710, NACL_OPCODE_NULL_OFFSET  },
  /* 1287 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmulp, 0x00, 2, 712, NACL_OPCODE_NULL_OFFSET  },
  /* 1288 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmulp, 0x00, 2, 714, NACL_OPCODE_NULL_OFFSET  },
  /* 1289 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFmulp, 0x00, 2, 716, NACL_OPCODE_NULL_OFFSET  },
  /* 1290 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcompp, 0x00, 2, 658, NACL_OPCODE_NULL_OFFSET  },
  /* 1291 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubrp, 0x00, 2, 640, NACL_OPCODE_NULL_OFFSET  },
  /* 1292 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubrp, 0x00, 2, 704, NACL_OPCODE_NULL_OFFSET  },
  /* 1293 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubrp, 0x00, 2, 706, NACL_OPCODE_NULL_OFFSET  },
  /* 1294 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubrp, 0x00, 2, 708, NACL_OPCODE_NULL_OFFSET  },
  /* 1295 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubrp, 0x00, 2, 710, NACL_OPCODE_NULL_OFFSET  },
  /* 1296 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubrp, 0x00, 2, 712, NACL_OPCODE_NULL_OFFSET  },
  /* 1297 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubrp, 0x00, 2, 714, NACL_OPCODE_NULL_OFFSET  },
  /* 1298 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubrp, 0x00, 2, 716, NACL_OPCODE_NULL_OFFSET  },
  /* 1299 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubp, 0x00, 2, 640, NACL_OPCODE_NULL_OFFSET  },
  /* 1300 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubp, 0x00, 2, 704, NACL_OPCODE_NULL_OFFSET  },
  /* 1301 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubp, 0x00, 2, 706, NACL_OPCODE_NULL_OFFSET  },
  /* 1302 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubp, 0x00, 2, 708, NACL_OPCODE_NULL_OFFSET  },
  /* 1303 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubp, 0x00, 2, 710, NACL_OPCODE_NULL_OFFSET  },
  /* 1304 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubp, 0x00, 2, 712, NACL_OPCODE_NULL_OFFSET  },
  /* 1305 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubp, 0x00, 2, 714, NACL_OPCODE_NULL_OFFSET  },
  /* 1306 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFsubp, 0x00, 2, 716, NACL_OPCODE_NULL_OFFSET  },
  /* 1307 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivrp, 0x00, 2, 640, NACL_OPCODE_NULL_OFFSET  },
  /* 1308 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivrp, 0x00, 2, 704, NACL_OPCODE_NULL_OFFSET  },
  /* 1309 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivrp, 0x00, 2, 706, NACL_OPCODE_NULL_OFFSET  },
  /* 1310 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivrp, 0x00, 2, 708, NACL_OPCODE_NULL_OFFSET  },
  /* 1311 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivrp, 0x00, 2, 710, NACL_OPCODE_NULL_OFFSET  },
  /* 1312 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivrp, 0x00, 2, 712, NACL_OPCODE_NULL_OFFSET  },
  /* 1313 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivrp, 0x00, 2, 714, NACL_OPCODE_NULL_OFFSET  },
  /* 1314 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivrp, 0x00, 2, 716, NACL_OPCODE_NULL_OFFSET  },
  /* 1315 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivp, 0x00, 2, 640, NACL_OPCODE_NULL_OFFSET  },
  /* 1316 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivp, 0x00, 2, 704, NACL_OPCODE_NULL_OFFSET  },
  /* 1317 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivp, 0x00, 2, 706, NACL_OPCODE_NULL_OFFSET  },
  /* 1318 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivp, 0x00, 2, 708, NACL_OPCODE_NULL_OFFSET  },
  /* 1319 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivp, 0x00, 2, 710, NACL_OPCODE_NULL_OFFSET  },
  /* 1320 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivp, 0x00, 2, 712, NACL_OPCODE_NULL_OFFSET  },
  /* 1321 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivp, 0x00, 2, 714, NACL_OPCODE_NULL_OFFSET  },
  /* 1322 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFdivp, 0x00, 2, 716, NACL_OPCODE_NULL_OFFSET  },
  /* 1323 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 1324 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFnstsw, 0x00, 1, 740, NACL_OPCODE_NULL_OFFSET  },
  /* 1325 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomip, 0x00, 2, 656, NACL_OPCODE_NULL_OFFSET  },
  /* 1326 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomip, 0x00, 2, 658, NACL_OPCODE_NULL_OFFSET  },
  /* 1327 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomip, 0x00, 2, 660, NACL_OPCODE_NULL_OFFSET  },
  /* 1328 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomip, 0x00, 2, 662, NACL_OPCODE_NULL_OFFSET  },
  /* 1329 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomip, 0x00, 2, 664, NACL_OPCODE_NULL_OFFSET  },
  /* 1330 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomip, 0x00, 2, 666, NACL_OPCODE_NULL_OFFSET  },
  /* 1331 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomip, 0x00, 2, 668, NACL_OPCODE_NULL_OFFSET  },
  /* 1332 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFucomip, 0x00, 2, 670, NACL_OPCODE_NULL_OFFSET  },
  /* 1333 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomip, 0x00, 2, 656, NACL_OPCODE_NULL_OFFSET  },
  /* 1334 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomip, 0x00, 2, 658, NACL_OPCODE_NULL_OFFSET  },
  /* 1335 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomip, 0x00, 2, 660, NACL_OPCODE_NULL_OFFSET  },
  /* 1336 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomip, 0x00, 2, 662, NACL_OPCODE_NULL_OFFSET  },
  /* 1337 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomip, 0x00, 2, 664, NACL_OPCODE_NULL_OFFSET  },
  /* 1338 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomip, 0x00, 2, 666, NACL_OPCODE_NULL_OFFSET  },
  /* 1339 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomip, 0x00, 2, 668, NACL_OPCODE_NULL_OFFSET  },
  /* 1340 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstFcomip, 0x00, 2, 670, NACL_OPCODE_NULL_OFFSET  },
  /* 1341 */
  { NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstUd2, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 1342 */
  { NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstNop, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 1343 */
  { NACLi_386,
    NACL_EMPTY_IFLAGS,
    InstPause, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
};

static const NaClPrefixOpcodeArrayOffset g_LookupTable[2543] = {
  /*     0 */ 1, 2, 3, 4, 5, 6, 7, 7, 8, 9, 
  /*    10 */ 10, 11, 12, 13, 7, 7, 14, 15, 16, 17, 
  /*    20 */ 18, 19, 7, 7, 20, 21, 22, 23, 24, 25, 
  /*    30 */ 7, 7, 26, 27, 28, 29, 30, 31, 7, 7, 
  /*    40 */ 32, 33, 34, 35, 36, 37, 7, 7, 38, 39, 
  /*    50 */ 40, 41, 42, 43, 7, 7, 44, 45, 46, 47, 
  /*    60 */ 48, 49, 7, 7, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*    70 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*    80 */ 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 
  /*    90 */ 60, 61, 62, 63, 64, 65, 7, 7, 7, 66, 
  /*   100 */ 7, 7, 7, 7, 67, 68, 69, 70, 71, 73, 
  /*   110 */ 74, 76, 77, 78, 79, 80, 81, 82, 83, 84, 
  /*   120 */ 85, 86, 87, 88, 89, 90, 91, 92, 100, 108, 
  /*   130 */ 116, 124, 125, 126, 127, 128, 129, 130, 131, 132, 
  /*   140 */ 133, 134, 135, 137, 138, 139, 140, 141, 142, 143, 
  /*   150 */ 144, 145, 148, 151, 7, 152, 154, 156, 157, 158, 
  /*   160 */ 159, 160, 161, 162, 163, 166, 167, 170, 171, 172, 
  /*   170 */ 173, 176, 177, 180, 181, 184, 185, 186, 187, 188, 
  /*   180 */ 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 
  /*   190 */ 199, 200, 208, 216, 217, 218, 7, 7, 219, 220, 
  /*   200 */ 221, 222, 223, 224, 225, 226, 227, 230, 238, 246, 
  /*   210 */ 254, 262, 7, 7, 7, 263, 271, 279, 287, 295, 
  /*   220 */ 303, 311, 319, 327, 329, 331, 333, 335, 336, 337, 
  /*   230 */ 338, 339, 340, 341, 7, 342, 343, 344, 345, 346, 
  /*   240 */ 7, 347, 7, 7, 348, 349, 357, 365, 366, 367, 
  /*   250 */ 368, 369, 370, 371, 373, 380, 386, 407, 408, 409, 
  /*   260 */ 7, 410, 411, 412, 413, 414, 7, 415, 7, 423, 
  /*   270 */ 424, 425, 426, 427, 429, 430, 431, 432, 434, 435, 
  /*   280 */ 439, 440, 440, 440, 440, 440, 440, 441, 442, 443, 
  /*   290 */ 444, 445, 7, 7, 7, 7, 446, 447, 448, 449, 
  /*   300 */ 450, 451, 452, 453, 454, 455, 456, 457, 458, 459, 
  /*   310 */ 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
  /*   320 */ 460, 461, 462, 463, 464, 465, 466, 467, 468, 469, 
  /*   330 */ 470, 471, 472, 473, 474, 475, 476, 477, 478, 479, 
  /*   340 */ 480, 481, 482, 483, 484, 485, 486, 487, 488, 489, 
  /*   350 */ 490, 491, 492, 493, 494, 495, 496, 497, 498, 499, 
  /*   360 */ 500, 501, 502, 503, 7, 7, 505, 506, 507, 514, 
  /*   370 */ 521, 528, 529, 530, 531, 532, 7, 7, 7, 7, 
  /*   380 */ 7, 7, 534, 535, 536, 537, 538, 539, 540, 541, 
  /*   390 */ 542, 543, 544, 545, 546, 547, 548, 549, 550, 551, 
  /*   400 */ 552, 553, 554, 555, 556, 557, 558, 559, 560, 561, 
  /*   410 */ 562, 563, 564, 565, 566, 567, 568, 569, 570, 571, 
  /*   420 */ 572, 573, 7, 7, 574, 575, 576, 577, 578, 579, 
  /*   430 */ 609, 610, 611, 612, 613, 614, 615, 616, 617, 618, 
  /*   440 */ 7, 136, 622, 623, 624, 625, 626, 627, 628, 629, 
  /*   450 */ 630, 631, 632, 633, 634, 636, 638, 640, 642, 644, 
  /*   460 */ 646, 648, 650, 652, 7, 653, 654, 655, 656, 657, 
  /*   470 */ 7, 658, 659, 660, 661, 662, 663, 664, 665, 666, 
  /*   480 */ 667, 668, 669, 670, 671, 672, 7, 673, 674, 675, 
  /*   490 */ 676, 677, 678, 679, 680, 681, 7, 682, 683, 684, 
  /*   500 */ 685, 686, 687, 688, 689, 690, 691, 692, 693, 694, 
  /*   510 */ 695, 7, NACL_OPCODE_NULL_OFFSET, 696, 697, 698, 699, 699, 699, 699, 
  /*   520 */ 699, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   530 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 699, 699, 700, 
  /*   540 */ 701, 702, 703, 699, 699, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   550 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   560 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   570 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 699, 704, 699, 
  /*   580 */ 699, 699, 699, 699, 699, 705, 706, 707, 699, 708, 
  /*   590 */ 709, 710, 711, 699, 699, 699, 699, 699, 699, 699, 
  /*   600 */ 699, 699, 699, 699, 699, 699, 699, 699, 699, 712, 
  /*   610 */ 699, 699, 699, 699, 699, 699, 699, 713, 714, 699, 
  /*   620 */ 699, 715, 716, 699, 699, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   630 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   640 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   650 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   660 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   670 */ NACL_OPCODE_NULL_OFFSET, 699, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   680 */ NACL_OPCODE_NULL_OFFSET, 699, 699, 699, 699, 699, 699, 699, 699, NACL_OPCODE_NULL_OFFSET, 
  /*   690 */ NACL_OPCODE_NULL_OFFSET, 717, 699, 699, 699, 699, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   700 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 718, 699, 699, 699, 699, 
  /*   710 */ 699, 719, 699, 699, 699, 699, 699, 699, 699, 699, 
  /*   720 */ 699, 699, 699, 699, 699, 699, 699, 720, 699, 699, 
  /*   730 */ 699, 699, 699, 699, 699, 699, 699, 721, 699, 699, 
  /*   740 */ 699, 699, 699, 699, 699, 699, 699, 699, 699, 699, 
  /*   750 */ 699, 699, 699, NACL_OPCODE_NULL_OFFSET, 722, 723, 724, 725, 725, 725, 
  /*   760 */ 726, 725, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   770 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 725, 725, 
  /*   780 */ 727, 728, 729, 730, 725, 725, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   790 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   800 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   810 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 725, 731, 
  /*   820 */ 732, 733, 725, 725, 725, 725, 734, 735, 736, 737, 
  /*   830 */ 738, 739, 740, 741, 725, 725, 725, 725, 725, 725, 
  /*   840 */ 725, 725, 725, 725, 725, 725, 725, 725, 725, 742, 
  /*   850 */ 743, 725, 725, 725, 725, 725, 725, 725, 725, 725, 
  /*   860 */ 725, 725, 725, 725, 744, 745, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   870 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   880 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   890 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   900 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   910 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   920 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 746, 725, 725, 725, 747, 748, 725, 725, 
  /*   930 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 749, 725, 725, 725, 725, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   940 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 725, 725, 725, 725, 
  /*   950 */ 725, 725, 750, 725, 725, 725, 725, 725, 725, 725, 
  /*   960 */ 725, 725, 725, 725, 725, 725, 725, 725, 751, 725, 
  /*   970 */ 725, 725, 725, 725, 725, 725, 725, 725, 725, 725, 
  /*   980 */ 725, 725, 725, 725, 725, 725, 725, 725, 725, 725, 
  /*   990 */ 725, 725, 725, 725, NACL_OPCODE_NULL_OFFSET, 752, 753, 754, 755, 756, 
  /*  1000 */ 757, 758, 759, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1010 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 760, 
  /*  1020 */ 761, 762, 763, 764, 765, 766, 767, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1030 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1040 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1050 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 768, 
  /*  1060 */ 769, 770, 770, 771, 772, 773, 774, 775, 776, 777, 
  /*  1070 */ 778, 779, 780, 781, 782, 783, 784, 785, 786, 787, 
  /*  1080 */ 788, 789, 790, 791, 792, 793, 794, 795, 796, 798, 
  /*  1090 */ 799, 800, 808, 815, 823, 824, 825, 826, 770, 828, 
  /*  1100 */ 829, 770, 770, 830, 831, 833, 834, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1110 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1120 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1130 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1140 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1150 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 770, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1160 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1170 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 835, 770, 836, 837, 838, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1180 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 839, 840, 841, 
  /*  1190 */ 842, 843, 844, 845, 846, 847, 848, 849, 850, 851, 
  /*  1200 */ 852, 853, 854, 855, 856, 857, 858, 859, 860, 861, 
  /*  1210 */ 862, 863, 864, 865, 866, 867, 868, 869, 870, 770, 
  /*  1220 */ 871, 872, 873, 874, 875, 876, 877, 878, 879, 880, 
  /*  1230 */ 881, 882, 883, 884, 770, NACL_OPCODE_NULL_OFFSET, 885, 886, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1240 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1250 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 887, 888, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
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
  /*  1360 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 889, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 890, NACL_OPCODE_NULL_OFFSET, 891, NACL_OPCODE_NULL_OFFSET, 
  /*  1370 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 892, NACL_OPCODE_NULL_OFFSET, 893, 894, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 895, NACL_OPCODE_NULL_OFFSET, 
  /*  1380 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 896, NACL_OPCODE_NULL_OFFSET, 897, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 898, NACL_OPCODE_NULL_OFFSET, 
  /*  1390 */ 899, 900, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 901, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 902, NACL_OPCODE_NULL_OFFSET, 
  /*  1400 */ 903, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 904, NACL_OPCODE_NULL_OFFSET, 905, 906, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1410 */ NACL_OPCODE_NULL_OFFSET, 907, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 908, NACL_OPCODE_NULL_OFFSET, 909, 910, 911, 
  /*  1420 */ 912, 913, 914, 915, 916, 917, 918, 919, 920, 7, 
  /*  1430 */ 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
  /*  1440 */ 7, 7, 7, 7, 7, 921, 922, 923, 7, 7, 
  /*  1450 */ 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
  /*  1460 */ 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
  /*  1470 */ 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
  /*  1480 */ 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
  /*  1490 */ 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
  /*  1500 */ 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
  /*  1510 */ 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
  /*  1520 */ 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
  /*  1530 */ 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
  /*  1540 */ 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
  /*  1550 */ 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
  /*  1560 */ 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
  /*  1570 */ 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
  /*  1580 */ 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
  /*  1590 */ 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
  /*  1600 */ 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
  /*  1610 */ 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
  /*  1620 */ 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
  /*  1630 */ 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
  /*  1640 */ 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
  /*  1650 */ 7, 7, 7, 7, 7, 7, 7, 924, 925, 7, 
  /*  1660 */ 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
  /*  1670 */ 7, 7, 7, 926, 927, 928, 929, 930, 931, 932, 
  /*  1680 */ 933, 934, 935, 936, 937, 770, 770, 770, 770, 938, 
  /*  1690 */ 770, 770, 770, 939, 940, 770, 941, 770, 770, 770, 
  /*  1700 */ 770, 942, 943, 944, 770, 945, 946, 947, 948, 949, 
  /*  1710 */ 950, 770, 770, 951, 952, 953, 954, 770, 770, 770, 
  /*  1720 */ 770, 955, 956, 957, 958, 959, 960, 770, 961, 962, 
  /*  1730 */ 963, 964, 965, 966, 967, 968, 969, 970, 971, 770, 
  /*  1740 */ 770, 770, 770, 770, 770, 770, 770, 770, 770, 770, 
  /*  1750 */ 770, 770, 770, 770, 770, 770, 770, 770, 770, 770, 
  /*  1760 */ 770, 770, 770, 770, 770, 770, 770, 770, 770, 770, 
  /*  1770 */ 770, 770, 770, 770, 770, 770, 770, 770, 770, 770, 
  /*  1780 */ 770, 770, 770, 770, 770, 770, 770, 770, 770, 770, 
  /*  1790 */ 770, 770, 770, 770, 770, 770, 770, 770, 770, 770, 
  /*  1800 */ 770, 972, 973, 770, 770, 770, 770, 770, 770, 770, 
  /*  1810 */ 770, 770, 770, 770, 770, 770, 770, 770, 770, 770, 
  /*  1820 */ 770, 770, 770, 770, 770, 770, 770, 770, 770, 770, 
  /*  1830 */ 770, 770, 770, 770, 770, 770, 770, 770, 770, 770, 
  /*  1840 */ 770, 770, 770, 770, 770, 770, 770, 770, 770, 770, 
  /*  1850 */ 770, 770, 770, 770, 770, 770, 770, 770, 770, 770, 
  /*  1860 */ 770, 770, 770, 770, 770, 770, 770, 770, 770, 770, 
  /*  1870 */ 770, 770, 770, 770, 770, 770, 770, 770, 770, 770, 
  /*  1880 */ 770, 770, 770, 770, 770, 770, 770, 770, 770, 770, 
  /*  1890 */ 770, 770, 770, 770, 770, 770, 770, 770, 770, 770, 
  /*  1900 */ 770, 770, 770, 770, 770, 770, 770, 770, 770, 770, 
  /*  1910 */ 770, 770, 770, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 770, 770, 770, 770, 770, 
  /*  1920 */ 770, 770, 770, 770, 770, 770, 770, 770, 770, NACL_OPCODE_NULL_OFFSET, 
  /*  1930 */ 974, 975, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 976, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 977, 978, 979, 
  /*  1940 */ 980, 981, 982, 983, 984, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 985, 
  /*  1950 */ 986, 988, 989, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1960 */ NACL_OPCODE_NULL_OFFSET, 990, 991, 993, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1970 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1980 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1990 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 994, 995, 996, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  2000 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  2010 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  2020 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 997, 998, 999, 1000, NACL_OPCODE_NULL_OFFSET, 
  /*  2030 */ NACL_OPCODE_NULL_OFFSET, 1001, 1002, 1003, 1004, 1005, 1006, 1007, 1008, 1009, 
  /*  2040 */ 1010, 1011, 1012, 1013, 1014, 1015, 1016, 1017, 1018, 1019, 
  /*  2050 */ 1020, 1021, 1022, 1023, 1024, 1025, 1026, 1027, 1028, 1029, 
  /*  2060 */ 1030, 1031, 1032, 1033, 1034, 1035, 1036, 1037, 1038, 1039, 
  /*  2070 */ 1040, 1041, 1042, 1043, 1044, 1045, 1046, 1047, 1048, 1049, 
  /*  2080 */ 1050, 1051, 1052, 1053, 1054, 1055, 1056, 1057, 1058, 1059, 
  /*  2090 */ 1060, 1061, 1062, 1063, 1064, NACL_OPCODE_NULL_OFFSET, 1065, 1066, 1067, 1068, 
  /*  2100 */ 1069, 1070, 1071, 1072, 1073, 1074, 1075, 1076, 1077, 1078, 
  /*  2110 */ 1079, 1080, 1081, 7, 7, 7, 7, 7, 7, 7, 
  /*  2120 */ 7, 7, 7, 7, 7, 7, 7, 7, 1082, 1083, 
  /*  2130 */ 7, 7, 1084, 1085, 7, 7, 1086, 1087, 1088, 1089, 
  /*  2140 */ 1090, 1091, 1092, 7, 1093, 1094, 1095, 1096, 1097, 1098, 
  /*  2150 */ 1099, 1100, 1101, 1102, 1103, 1104, 1105, 1106, 1107, 1108, 
  /*  2160 */ NACL_OPCODE_NULL_OFFSET, 1109, 1110, 1111, 1112, 1113, 1114, 1115, 1116, 1117, 
  /*  2170 */ 1118, 1119, 1120, 1121, 1122, 1123, 1124, 1125, 1126, 1127, 
  /*  2180 */ 1128, 1129, 1130, 1131, 1132, 1133, 1134, 1135, 1136, 1137, 
  /*  2190 */ 1138, 1139, 1140, 7, 7, 7, 7, 7, 7, 7, 
  /*  2200 */ 7, 7, 1141, 7, 7, 7, 7, 7, 7, 7, 
  /*  2210 */ 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
  /*  2220 */ 7, 7, 7, 7, 7, NACL_OPCODE_NULL_OFFSET, 1142, 1143, 1144, 1145, 
  /*  2230 */ 1146, 1147, 1148, 1149, 1150, 1151, 1152, 1153, 1154, 1155, 
  /*  2240 */ 1156, 1157, 1158, 1159, 1160, 1161, 1162, 1163, 1164, 1165, 
  /*  2250 */ 1166, 1167, 1168, 1169, 1170, 1171, 1172, 1173, 7, 7, 
  /*  2260 */ 1174, 1175, 7, 7, 7, 7, 1176, 1177, 1178, 1179, 
  /*  2270 */ 1180, 1181, 1182, 1183, 1184, 1185, 1186, 1187, 1188, 1189, 
  /*  2280 */ 1190, 1191, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 1001, 1192, 1193, 1194, 1195, 1196, 
  /*  2290 */ 1197, 1198, 1009, 1199, 1200, 1201, 1202, 1203, 1204, 1205, 
  /*  2300 */ 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
  /*  2310 */ 7, 7, 7, 7, 7, 7, 1041, 1206, 1207, 1208, 
  /*  2320 */ 1209, 1210, 1211, 1212, 1033, 1213, 1214, 1215, 1216, 1217, 
  /*  2330 */ 1218, 1219, 1057, 1220, 1221, 1222, 1223, 1224, 1225, 1226, 
  /*  2340 */ 1049, 1227, 1228, 1229, 1230, 1231, 1232, 1233, NACL_OPCODE_NULL_OFFSET, 1234, 
  /*  2350 */ 1235, 1236, 1237, 1238, 1239, 1240, 1241, 7, 7, 7, 
  /*  2360 */ 7, 7, 7, 7, 7, 1242, 1243, 1244, 1245, 1246, 
  /*  2370 */ 1247, 1248, 1249, 1250, 1251, 1252, 1253, 1254, 1255, 1256, 
  /*  2380 */ 1257, 1258, 1259, 1260, 1261, 1262, 1263, 1264, 1265, 1266, 
  /*  2390 */ 1267, 1268, 1269, 1270, 1271, 1272, 1273, 7, 7, 7, 
  /*  2400 */ 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
  /*  2410 */ 7, 7, 7, NACL_OPCODE_NULL_OFFSET, 1274, 1275, 1276, 1277, 1278, 1279, 
  /*  2420 */ 1280, 1281, 1282, 1283, 1284, 1285, 1286, 1287, 1288, 1289, 
  /*  2430 */ 7, 7, 7, 7, 7, 7, 7, 7, 7, 1290, 
  /*  2440 */ 7, 7, 7, 7, 7, 7, 1291, 1292, 1293, 1294, 
  /*  2450 */ 1295, 1296, 1297, 1298, 1299, 1300, 1301, 1302, 1303, 1304, 
  /*  2460 */ 1305, 1306, 1307, 1308, 1309, 1310, 1311, 1312, 1313, 1314, 
  /*  2470 */ 1315, 1316, 1317, 1318, 1319, 1320, 1321, 1322, NACL_OPCODE_NULL_OFFSET, 1323, 
  /*  2480 */ 1323, 1323, 1323, 1323, 1323, 1323, 1323, 1323, 1323, 1323, 
  /*  2490 */ 1323, 1323, 1323, 1323, 1323, 1323, 1323, 1323, 1323, 1323, 
  /*  2500 */ 1323, 1323, 1323, 1323, 1323, 1323, 1323, 1323, 1323, 1323, 
  /*  2510 */ 1323, 1324, 7, 7, 7, 7, 7, 7, 7, 1325, 
  /*  2520 */ 1326, 1327, 1328, 1329, 1330, 1331, 1332, 1333, 1334, 1335, 
  /*  2530 */ 1336, 1337, 1338, 1339, 1340, 7, 7, 7, 7, 7, 
  /*  2540 */ 7, 7, 7, };

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

static const NaClInstNode g_OpcodeSeq[95] = {
  /* 0 */
  { 0x0f,
    NACL_OPCODE_NULL_OFFSET,
    g_OpcodeSeq + 1,
    g_OpcodeSeq + 20,
  },
  /* 1 */
  { 0x0b,
    1341,
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
    1342,
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
    1342,
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
    1342,
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
    1342,
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
    1342,
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
    1342,
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
    1342,
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
    1342,
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
    1342,
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
    1342,
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
    1342,
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
    1342,
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
    1342,
    NULL,
    NULL,
  },
  /* 91 */
  { 0x90,
    1342,
    NULL,
    NULL,
  },
  /* 92 */
  { 0x90,
    1342,
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
    1343,
    NULL,
    NULL,
  },
};
