/*
 * THIS FILE IS AUTO-GENERATED. DO NOT EDIT.
 * Compiled for x86-32 bit mode.
 *
 * You must include ncopcode_desc.h before this file.
 */

static const NaClOp g_Operands[306] = {
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
  /* 24 */ { RegRECX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$rCXv" },
  /* 25 */ { RegREDX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$rDXv" },
  /* 26 */ { RegREBX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$rBXv" },
  /* 27 */ { RegRESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$rSPv" },
  /* 28 */ { RegREBP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$rBPv" },
  /* 29 */ { RegRESI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$rSIv" },
  /* 30 */ { RegREDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$rDIv" },
  /* 31 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 32 */ { G_OpcodeBase, NACL_OPFLAG(OpUse), "$r8v" },
  /* 33 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 34 */ { G_OpcodeBase, NACL_OPFLAG(OpSet), "$r8v" },
  /* 35 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 36 */ { I_Operand, NACL_OPFLAG(OpUse), "$Iz" },
  /* 37 */ { G_Operand, NACL_OPFLAG(OpSet), "$Gv" },
  /* 38 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ev" },
  /* 39 */ { I_Operand, NACL_OPFLAG(OpUse), "$Iz" },
  /* 40 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 41 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 42 */ { G_Operand, NACL_OPFLAG(OpSet), "$Gv" },
  /* 43 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ev" },
  /* 44 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 45 */ { RegEIP, NACL_OPFLAG(OpSet), "%eip" },
  /* 46 */ { J_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandNear) | NACL_OPFLAG(OperandRelative), "$Jb" },
  /* 47 */ { E_Operand, NACL_OPFLAG(OpUse), "$Eb" },
  /* 48 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 49 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Eb" },
  /* 50 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 51 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Ev" },
  /* 52 */ { I_Operand, NACL_OPFLAG(OpUse), "$Iz" },
  /* 53 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Ev" },
  /* 54 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 55 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Eb" },
  /* 56 */ { G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Gb" },
  /* 57 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Ev" },
  /* 58 */ { G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Gv" },
  /* 59 */ { E_Operand, NACL_OPFLAG(OpSet), "$Eb" },
  /* 60 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gb" },
  /* 61 */ { E_Operand, NACL_OPFLAG(OpSet), "$Ev" },
  /* 62 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gv" },
  /* 63 */ { G_Operand, NACL_OPFLAG(OpSet), "$Gb" },
  /* 64 */ { E_Operand, NACL_OPFLAG(OpUse), "$Eb" },
  /* 65 */ { G_Operand, NACL_OPFLAG(OpSet), "$Gv" },
  /* 66 */ { M_Operand, NACL_OPFLAG(OpAddress), "$M" },
  /* 67 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 68 */ { E_Operand, NACL_OPFLAG(OpSet), "$Ev" },
  /* 69 */ { G_OpcodeBase, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$r8v" },
  /* 70 */ { RegREAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$rAXv" },
  /* 71 */ { RegEAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OperandSignExtends_v), "%eax" },
  /* 72 */ { RegAX, NACL_OPFLAG(OpUse), "%ax" },
  /* 73 */ { RegAX, NACL_OPFLAG(OpSet), "%ax" },
  /* 74 */ { RegAL, NACL_OPFLAG(OpUse), "%al" },
  /* 75 */ { RegEDX, NACL_OPFLAG(OpSet), "%edx" },
  /* 76 */ { RegEAX, NACL_OPFLAG(OpUse), "%eax" },
  /* 77 */ { RegDX, NACL_OPFLAG(OpSet), "%dx" },
  /* 78 */ { RegAX, NACL_OPFLAG(OpUse), "%ax" },
  /* 79 */ { RegAL, NACL_OPFLAG(OpSet), "%al" },
  /* 80 */ { O_Operand, NACL_OPFLAG(OpUse), "$Ob" },
  /* 81 */ { RegREAX, NACL_OPFLAG(OpSet), "$rAXv" },
  /* 82 */ { O_Operand, NACL_OPFLAG(OpUse), "$Ov" },
  /* 83 */ { O_Operand, NACL_OPFLAG(OpSet), "$Ob" },
  /* 84 */ { RegAL, NACL_OPFLAG(OpUse), "%al" },
  /* 85 */ { O_Operand, NACL_OPFLAG(OpSet), "$Ov" },
  /* 86 */ { RegREAX, NACL_OPFLAG(OpUse), "$rAXv" },
  /* 87 */ { RegES_EDI, NACL_OPFLAG(OpSet), "$Yb" },
  /* 88 */ { RegDS_ESI, NACL_OPFLAG(OpUse), "$Xb" },
  /* 89 */ { RegES_EDI, NACL_OPFLAG(OpSet), "$Yvd" },
  /* 90 */ { RegDS_ESI, NACL_OPFLAG(OpUse), "$Xvd" },
  /* 91 */ { RegES_EDI, NACL_OPFLAG(OpSet), "$Yvw" },
  /* 92 */ { RegDS_ESI, NACL_OPFLAG(OpUse), "$Xvw" },
  /* 93 */ { RegES_EDI, NACL_OPFLAG(OpUse), "$Yb" },
  /* 94 */ { RegDS_ESI, NACL_OPFLAG(OpUse), "$Xb" },
  /* 95 */ { RegES_EDI, NACL_OPFLAG(OpUse), "$Yvd" },
  /* 96 */ { RegDS_ESI, NACL_OPFLAG(OpUse), "$Xvd" },
  /* 97 */ { RegES_EDI, NACL_OPFLAG(OpUse), "$Yvw" },
  /* 98 */ { RegDS_ESI, NACL_OPFLAG(OpUse), "$Xvw" },
  /* 99 */ { RegES_EDI, NACL_OPFLAG(OpSet), "$Yb" },
  /* 100 */ { RegAL, NACL_OPFLAG(OpUse), "%al" },
  /* 101 */ { RegES_EDI, NACL_OPFLAG(OpSet), "$Yvd" },
  /* 102 */ { RegEAX, NACL_OPFLAG(OpUse), "$rAXvd" },
  /* 103 */ { RegES_EDI, NACL_OPFLAG(OpSet), "$Yvw" },
  /* 104 */ { RegAX, NACL_OPFLAG(OpUse), "$rAXvw" },
  /* 105 */ { RegAL, NACL_OPFLAG(OpSet), "%al" },
  /* 106 */ { RegDS_ESI, NACL_OPFLAG(OpUse), "$Xb" },
  /* 107 */ { RegEAX, NACL_OPFLAG(OpSet), "$rAXvd" },
  /* 108 */ { RegDS_ESI, NACL_OPFLAG(OpUse), "$Xvd" },
  /* 109 */ { RegAX, NACL_OPFLAG(OpSet), "$rAXvw" },
  /* 110 */ { RegDS_ESI, NACL_OPFLAG(OpUse), "$Xvw" },
  /* 111 */ { RegAL, NACL_OPFLAG(OpUse), "%al" },
  /* 112 */ { RegES_EDI, NACL_OPFLAG(OpUse), "$Yb" },
  /* 113 */ { RegEAX, NACL_OPFLAG(OpUse), "$rAXvd" },
  /* 114 */ { RegES_EDI, NACL_OPFLAG(OpUse), "$Yvd" },
  /* 115 */ { RegAX, NACL_OPFLAG(OpUse), "$rAXvw" },
  /* 116 */ { RegES_EDI, NACL_OPFLAG(OpUse), "$Yvw" },
  /* 117 */ { G_OpcodeBase, NACL_OPFLAG(OpSet), "$r8b" },
  /* 118 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 119 */ { G_OpcodeBase, NACL_OPFLAG(OpSet), "$r8v" },
  /* 120 */ { I_Operand, NACL_OPFLAG(OpUse), "$Iv" },
  /* 121 */ { E_Operand, NACL_OPFLAG(OpSet), "$Eb" },
  /* 122 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 123 */ { E_Operand, NACL_OPFLAG(OpSet), "$Ev" },
  /* 124 */ { I_Operand, NACL_OPFLAG(OpUse), "$Iz" },
  /* 125 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Eb" },
  /* 126 */ { Const_1, NACL_OPFLAG(OpUse), "1" },
  /* 127 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Ev" },
  /* 128 */ { Const_1, NACL_OPFLAG(OpUse), "1" },
  /* 129 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Eb" },
  /* 130 */ { RegCL, NACL_OPFLAG(OpUse), "%cl" },
  /* 131 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Ev" },
  /* 132 */ { RegCL, NACL_OPFLAG(OpUse), "%cl" },
  /* 133 */ { Mv_Operand, NACL_OPFLAG(OpUse), "$Md" },
  /* 134 */ { Mw_Operand, NACL_OPFLAG(OpSet), "$Mw" },
  /* 135 */ { M_Operand, NACL_OPFLAG(OpSet), "$M" },
  /* 136 */ { Mw_Operand, NACL_OPFLAG(OpUse), "$Mw" },
  /* 137 */ { M_Operand, NACL_OPFLAG(OpUse), "$M" },
  /* 138 */ { Mv_Operand, NACL_OPFLAG(OpSet), "$Md" },
  /* 139 */ { Mo_Operand, NACL_OPFLAG(OpUse), "$Mq" },
  /* 140 */ { Mo_Operand, NACL_OPFLAG(OpSet), "$Mq" },
  /* 141 */ { RegEIP, NACL_OPFLAG(OpSet), "%eip" },
  /* 142 */ { RegECX, NACL_OPFLAG(OpUse), "%ecx" },
  /* 143 */ { J_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandNear) | NACL_OPFLAG(OperandRelative), "$Jb" },
  /* 144 */ { RegEIP, NACL_OPFLAG(OpSet), "%eip" },
  /* 145 */ { RegCX, NACL_OPFLAG(OpUse), "%cx" },
  /* 146 */ { J_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandNear) | NACL_OPFLAG(OperandRelative), "$Jb" },
  /* 147 */ { RegEIP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eip}" },
  /* 148 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 149 */ { J_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandNear) | NACL_OPFLAG(OperandRelative), "$Jzd" },
  /* 150 */ { RegEIP, NACL_OPFLAG(OpSet), "%eip" },
  /* 151 */ { J_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandNear) | NACL_OPFLAG(OperandRelative), "$Jzd" },
  /* 152 */ { RegAX, NACL_OPFLAG(OpSet), "%ax" },
  /* 153 */ { RegAL, NACL_OPFLAG(OpUse), "%al" },
  /* 154 */ { E_Operand, NACL_OPFLAG(OpUse), "$Eb" },
  /* 155 */ { RegREDX, NACL_OPFLAG(OpSet), "%redx" },
  /* 156 */ { RegREAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%reax" },
  /* 157 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ev" },
  /* 158 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 159 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ev" },
  /* 160 */ { RegEIP, NACL_OPFLAG(OpSet), "%eip" },
  /* 161 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandNear), "$Ev" },
  /* 162 */ { RegEIP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%eip}" },
  /* 163 */ { RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit), "{%esp}" },
  /* 164 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandNear), "$Ev" },
  /* 165 */ { Ew_Operand, NACL_OPFLAG(OpUse), "$Ew" },
  /* 166 */ { Mb_Operand, NACL_EMPTY_OPFLAGS, "$Mb" },
  /* 167 */ { Mmx_G_Operand, NACL_EMPTY_OPFLAGS, "$Pq" },
  /* 168 */ { Mmx_E_Operand, NACL_EMPTY_OPFLAGS, "$Qq" },
  /* 169 */ { I_Operand, NACL_EMPTY_OPFLAGS, "$Ib" },
  /* 170 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wps" },
  /* 171 */ { Xmm_E_Operand, NACL_OPFLAG(OpSet), "$Wps" },
  /* 172 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$VRq" },
  /* 173 */ { Xmm_Eo_Operand, NACL_OPFLAG(OpUse), "$Wq" },
  /* 174 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse), "$Qq" },
  /* 175 */ { Mdq_Operand, NACL_OPFLAG(OpSet), "$Mdq" },
  /* 176 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wss" },
  /* 177 */ { RegEAX, NACL_OPFLAG(OpSet), "%eax" },
  /* 178 */ { RegEDX, NACL_OPFLAG(OpSet), "%edx" },
  /* 179 */ { Gv_Operand, NACL_OPFLAG(OpSet), "$Gd" },
  /* 180 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$VRps" },
  /* 181 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 182 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse), "$Qd" },
  /* 183 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ed/q/d" },
  /* 184 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse), "$Qq" },
  /* 185 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 186 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$PRq" },
  /* 187 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 188 */ { E_Operand, NACL_OPFLAG(OpSet), "$Ed/q/d" },
  /* 189 */ { Mmx_E_Operand, NACL_OPFLAG(OpSet), "$Qq" },
  /* 190 */ { RegEBX, NACL_OPFLAG(OpSet), "%ebx" },
  /* 191 */ { RegEDX, NACL_OPFLAG(OpSet), "%edx" },
  /* 192 */ { RegEAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%eax" },
  /* 193 */ { RegECX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%ecx" },
  /* 194 */ { E_Operand, NACL_OPFLAG(OpSet), "$Ev" },
  /* 195 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gv" },
  /* 196 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 197 */ { E_Operand, NACL_OPFLAG(OpSet), "$Ev" },
  /* 198 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gv" },
  /* 199 */ { RegCL, NACL_OPFLAG(OpUse), "%cl" },
  /* 200 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Ev" },
  /* 201 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gv" },
  /* 202 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 203 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Ev" },
  /* 204 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gv" },
  /* 205 */ { RegCL, NACL_OPFLAG(OpUse), "%cl" },
  /* 206 */ { Mb_Operand, NACL_OPFLAG(OpUse), "$Mb" },
  /* 207 */ { RegAL, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%al" },
  /* 208 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Eb" },
  /* 209 */ { G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Gb" },
  /* 210 */ { RegREAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$rAXv" },
  /* 211 */ { E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Ev" },
  /* 212 */ { G_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Gv" },
  /* 213 */ { G_Operand, NACL_OPFLAG(OpSet), "$Gv" },
  /* 214 */ { Eb_Operand, NACL_OPFLAG(OpUse), "$Eb" },
  /* 215 */ { G_Operand, NACL_OPFLAG(OpSet), "$Gv" },
  /* 216 */ { Ew_Operand, NACL_OPFLAG(OpUse), "$Ew" },
  /* 217 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wps" },
  /* 218 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 219 */ { M_Operand, NACL_OPFLAG(OpSet), "$Md/q" },
  /* 220 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gd/q" },
  /* 221 */ { E_Operand, NACL_OPFLAG(OpUse), "$Rd/q/Mw" },
  /* 222 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 223 */ { Gv_Operand, NACL_OPFLAG(OpSet), "$Gd" },
  /* 224 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse), "$PRq" },
  /* 225 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 226 */ { RegEDX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%edx" },
  /* 227 */ { RegEAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "%eax" },
  /* 228 */ { Mo_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Mq" },
  /* 229 */ { G_OpcodeBase, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$r8vq" },
  /* 230 */ { G_OpcodeBase, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$r8vd" },
  /* 231 */ { RegDS_EDI, NACL_OPFLAG(OpSet), "$Zvd" },
  /* 232 */ { Mmx_E_Operand, NACL_OPFLAG(OpUse), "$PRq" },
  /* 233 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wsd" },
  /* 234 */ { Xmm_E_Operand, NACL_OPFLAG(OpSet), "$Wsd" },
  /* 235 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ed/q" },
  /* 236 */ { G_Operand, NACL_OPFLAG(OpSet), "$Gd/q" },
  /* 237 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wsd" },
  /* 238 */ { Xmm_Eo_Operand, NACL_OPFLAG(OpUse), "$Wq" },
  /* 239 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 240 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$VRq" },
  /* 241 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 242 */ { I2_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 243 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$VRdq" },
  /* 244 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wsd" },
  /* 245 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 246 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wpd" },
  /* 247 */ { Mdq_Operand, NACL_OPFLAG(OpUse), "$Mdq" },
  /* 248 */ { Xmm_E_Operand, NACL_OPFLAG(OpSet), "$Wss" },
  /* 249 */ { G_Operand, NACL_OPFLAG(OpSet), "$Gd/q" },
  /* 250 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wss" },
  /* 251 */ { Xmm_E_Operand, NACL_OPFLAG(OpSet), "$Wdq" },
  /* 252 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wss" },
  /* 253 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 254 */ { Xmm_E_Operand, NACL_OPFLAG(OpSet), "$Wpd" },
  /* 255 */ { Gv_Operand, NACL_OPFLAG(OpSet), "$Gd" },
  /* 256 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$VRpd" },
  /* 257 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 258 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 259 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$VRdq" },
  /* 260 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 261 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wpd" },
  /* 262 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 263 */ { Gv_Operand, NACL_OPFLAG(OpSet), "$Gd" },
  /* 264 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$VRdq" },
  /* 265 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 266 */ { Xmm_Eo_Operand, NACL_OPFLAG(OpSet), "$Wq" },
  /* 267 */ { RegDS_EDI, NACL_OPFLAG(OpSet), "$Zvd" },
  /* 268 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$VRdq" },
  /* 269 */ { G_Operand, NACL_OPFLAG(OpSet), "$Gv" },
  /* 270 */ { M_Operand, NACL_OPFLAG(OpUse), "$Mv" },
  /* 271 */ { M_Operand, NACL_OPFLAG(OpSet), "$Mv" },
  /* 272 */ { G_Operand, NACL_OPFLAG(OpUse), "$Gv" },
  /* 273 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Udq/Mq" },
  /* 274 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Udq/Md" },
  /* 275 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Udq/Mw" },
  /* 276 */ { Gv_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Gd" },
  /* 277 */ { E_Operand, NACL_OPFLAG(OpUse), "$Eb" },
  /* 278 */ { Gv_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet), "$Gd" },
  /* 279 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ev" },
  /* 280 */ { Ev_Operand, NACL_OPFLAG(OpSet), "$Rd/Mb" },
  /* 281 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 282 */ { Ev_Operand, NACL_OPFLAG(OpSet), "$Rd/Mw" },
  /* 283 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 284 */ { E_Operand, NACL_OPFLAG(OpSet), "$Ed/q/d" },
  /* 285 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 286 */ { Ev_Operand, NACL_OPFLAG(OpSet), "$Ed" },
  /* 287 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 288 */ { E_Operand, NACL_OPFLAG(OpUse), "$Rd/q/Mb" },
  /* 289 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 290 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Udq/Md" },
  /* 291 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 292 */ { E_Operand, NACL_OPFLAG(OpUse), "$Ed/q/d" },
  /* 293 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 294 */ { RegREAX, NACL_OPFLAG(OpSet), "$rAXv" },
  /* 295 */ { RegREDX, NACL_OPFLAG(OpSet), "$rDXv" },
  /* 296 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 297 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 298 */ { RegRECX, NACL_OPFLAG(OpSet), "$rCXv" },
  /* 299 */ { RegREAX, NACL_OPFLAG(OpSet), "$rAXv" },
  /* 300 */ { RegREDX, NACL_OPFLAG(OpSet), "$rDXv" },
  /* 301 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 302 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
  /* 303 */ { RegRECX, NACL_OPFLAG(OpSet), "$rCXv" },
  /* 304 */ { Xmm_E_Operand, NACL_OPFLAG(OpUse), "$Wdq" },
  /* 305 */ { I_Operand, NACL_OPFLAG(OpUse), "$Ib" },
};

static const NaClInst g_Opcodes[582] = {
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
    NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 8 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstOr, 0x00, 2, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 9 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstOr, 0x00, 2, 2, NACL_OPCODE_NULL_OFFSET  },
  /* 10 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstOr, 0x00, 2, 4, NACL_OPCODE_NULL_OFFSET  },
  /* 11 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstOr, 0x00, 2, 6, NACL_OPCODE_NULL_OFFSET  },
  /* 12 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable),
    InstOr, 0x00, 2, 8, NACL_OPCODE_NULL_OFFSET  },
  /* 13 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstOr, 0x00, 2, 10, NACL_OPCODE_NULL_OFFSET  },
  /* 14 */
  { NACLi_INVALID,
    NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 15 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 16 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 2, NACL_OPCODE_NULL_OFFSET  },
  /* 17 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 4, NACL_OPCODE_NULL_OFFSET  },
  /* 18 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 6, NACL_OPCODE_NULL_OFFSET  },
  /* 19 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 8, NACL_OPCODE_NULL_OFFSET  },
  /* 20 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 10, NACL_OPCODE_NULL_OFFSET  },
  /* 21 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstAnd, 0x00, 2, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 22 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstAnd, 0x00, 2, 2, NACL_OPCODE_NULL_OFFSET  },
  /* 23 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstAnd, 0x00, 2, 4, NACL_OPCODE_NULL_OFFSET  },
  /* 24 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstAnd, 0x00, 2, 6, NACL_OPCODE_NULL_OFFSET  },
  /* 25 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable),
    InstAnd, 0x00, 2, 8, NACL_OPCODE_NULL_OFFSET  },
  /* 26 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstAnd, 0x00, 2, 10, NACL_OPCODE_NULL_OFFSET  },
  /* 27 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstSub, 0x00, 2, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 28 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstSub, 0x00, 2, 2, NACL_OPCODE_NULL_OFFSET  },
  /* 29 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstSub, 0x00, 2, 4, NACL_OPCODE_NULL_OFFSET  },
  /* 30 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstSub, 0x00, 2, 6, NACL_OPCODE_NULL_OFFSET  },
  /* 31 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable),
    InstSub, 0x00, 2, 8, NACL_OPCODE_NULL_OFFSET  },
  /* 32 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstSub, 0x00, 2, 10, NACL_OPCODE_NULL_OFFSET  },
  /* 33 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 12, NACL_OPCODE_NULL_OFFSET  },
  /* 34 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 14, NACL_OPCODE_NULL_OFFSET  },
  /* 35 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 16, NACL_OPCODE_NULL_OFFSET  },
  /* 36 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 18, NACL_OPCODE_NULL_OFFSET  },
  /* 37 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 20, NACL_OPCODE_NULL_OFFSET  },
  /* 38 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 22, NACL_OPCODE_NULL_OFFSET  },
  /* 39 */
  { NACLi_386,
    NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 10, NACL_OPCODE_NULL_OFFSET  },
  /* 40 */
  { NACLi_386,
    NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 24, NACL_OPCODE_NULL_OFFSET  },
  /* 41 */
  { NACLi_386,
    NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 25, NACL_OPCODE_NULL_OFFSET  },
  /* 42 */
  { NACLi_386,
    NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 26, NACL_OPCODE_NULL_OFFSET  },
  /* 43 */
  { NACLi_386,
    NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 27, NACL_OPCODE_NULL_OFFSET  },
  /* 44 */
  { NACLi_386,
    NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 28, NACL_OPCODE_NULL_OFFSET  },
  /* 45 */
  { NACLi_386,
    NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 29, NACL_OPCODE_NULL_OFFSET  },
  /* 46 */
  { NACLi_386,
    NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 30, NACL_OPCODE_NULL_OFFSET  },
  /* 47 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstPush, 0x00, 2, 31, NACL_OPCODE_NULL_OFFSET  },
  /* 48 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstPush, 0x01, 2, 31, NACL_OPCODE_NULL_OFFSET  },
  /* 49 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstPush, 0x02, 2, 31, NACL_OPCODE_NULL_OFFSET  },
  /* 50 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstPush, 0x03, 2, 31, NACL_OPCODE_NULL_OFFSET  },
  /* 51 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstPush, 0x04, 2, 31, NACL_OPCODE_NULL_OFFSET  },
  /* 52 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstPush, 0x05, 2, 31, NACL_OPCODE_NULL_OFFSET  },
  /* 53 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstPush, 0x06, 2, 31, NACL_OPCODE_NULL_OFFSET  },
  /* 54 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstPush, 0x07, 2, 31, NACL_OPCODE_NULL_OFFSET  },
  /* 55 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstPop, 0x00, 2, 33, NACL_OPCODE_NULL_OFFSET  },
  /* 56 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstPop, 0x01, 2, 33, NACL_OPCODE_NULL_OFFSET  },
  /* 57 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstPop, 0x02, 2, 33, NACL_OPCODE_NULL_OFFSET  },
  /* 58 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstPop, 0x03, 2, 33, NACL_OPCODE_NULL_OFFSET  },
  /* 59 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstPop, 0x04, 2, 33, NACL_OPCODE_NULL_OFFSET  },
  /* 60 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstPop, 0x05, 2, 33, NACL_OPCODE_NULL_OFFSET  },
  /* 61 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstPop, 0x06, 2, 33, NACL_OPCODE_NULL_OFFSET  },
  /* 62 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstPop, 0x07, 2, 33, NACL_OPCODE_NULL_OFFSET  },
  /* 63 */
  { NACLi_386,
    NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 64 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 0, 0, 63  },
  /* 65 */
  { NACLi_386,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 66 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 67 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16),
    InstPush, 0x00, 2, 35, NACL_OPCODE_NULL_OFFSET  },
  /* 68 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 3, 37, NACL_OPCODE_NULL_OFFSET  },
  /* 69 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b),
    InstPush, 0x00, 2, 40, NACL_OPCODE_NULL_OFFSET  },
  /* 70 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 3, 42, NACL_OPCODE_NULL_OFFSET  },
  /* 71 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 72 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 73 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 0, 0, 72  },
  /* 74 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(ConditionalJump) | NACL_IFLAG(PartialInstruction),
    InstDontCareCondJump, 0x00, 2, 45, NACL_OPCODE_NULL_OFFSET  },
  /* 75 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x07, 2, 47, NACL_OPCODE_NULL_OFFSET  },
  /* 76 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x06, 2, 49, 75  },
  /* 77 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstSub, 0x05, 2, 49, 76  },
  /* 78 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstAnd, 0x04, 2, 49, 77  },
  /* 79 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x03, 2, 49, 78  },
  /* 80 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x02, 2, 49, 79  },
  /* 81 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstOr, 0x01, 2, 49, 80  },
  /* 82 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b),
    InstAdd, 0x00, 2, 49, 81  },
  /* 83 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x07, 2, 38, NACL_OPCODE_NULL_OFFSET  },
  /* 84 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x06, 2, 51, 83  },
  /* 85 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstSub, 0x05, 2, 51, 84  },
  /* 86 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstAnd, 0x04, 2, 51, 85  },
  /* 87 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x03, 2, 51, 86  },
  /* 88 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x02, 2, 51, 87  },
  /* 89 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstOr, 0x01, 2, 51, 88  },
  /* 90 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstAdd, 0x00, 2, 51, 89  },
  /* 91 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x07, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 92 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x06, 0, 0, 91  },
  /* 93 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x05, 0, 0, 92  },
  /* 94 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x04, 0, 0, 93  },
  /* 95 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x03, 0, 0, 94  },
  /* 96 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x02, 0, 0, 95  },
  /* 97 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x01, 0, 0, 96  },
  /* 98 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 0, 0, 97  },
  /* 99 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x07, 2, 43, NACL_OPCODE_NULL_OFFSET  },
  /* 100 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x06, 2, 53, 99  },
  /* 101 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstSub, 0x05, 2, 53, 100  },
  /* 102 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstAnd, 0x04, 2, 53, 101  },
  /* 103 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x03, 2, 53, 102  },
  /* 104 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x02, 2, 53, 103  },
  /* 105 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstOr, 0x01, 2, 53, 104  },
  /* 106 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstAdd, 0x00, 2, 53, 105  },
  /* 107 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 55, NACL_OPCODE_NULL_OFFSET  },
  /* 108 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 57, NACL_OPCODE_NULL_OFFSET  },
  /* 109 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x00, 2, 59, NACL_OPCODE_NULL_OFFSET  },
  /* 110 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMov, 0x00, 2, 61, NACL_OPCODE_NULL_OFFSET  },
  /* 111 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x00, 2, 63, NACL_OPCODE_NULL_OFFSET  },
  /* 112 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMov, 0x00, 2, 37, NACL_OPCODE_NULL_OFFSET  },
  /* 113 */
  { NACLi_386,
    NACL_IFLAG(ModRmRegSOperand) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 114 */
  { NACLi_386,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstLea, 0x00, 2, 65, NACL_OPCODE_NULL_OFFSET  },
  /* 115 */
  { NACLi_386,
    NACL_IFLAG(ModRmRegSOperand) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 116 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 117 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstPop, 0x00, 2, 67, 116  },
  /* 118 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 69, NACL_OPCODE_NULL_OFFSET  },
  /* 119 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x01, 2, 69, NACL_OPCODE_NULL_OFFSET  },
  /* 120 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x02, 2, 69, NACL_OPCODE_NULL_OFFSET  },
  /* 121 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x03, 2, 69, NACL_OPCODE_NULL_OFFSET  },
  /* 122 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x04, 2, 69, NACL_OPCODE_NULL_OFFSET  },
  /* 123 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x05, 2, 69, NACL_OPCODE_NULL_OFFSET  },
  /* 124 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x06, 2, 69, NACL_OPCODE_NULL_OFFSET  },
  /* 125 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x07, 2, 69, NACL_OPCODE_NULL_OFFSET  },
  /* 126 */
  { NACLi_386,
    NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 71, NACL_OPCODE_NULL_OFFSET  },
  /* 127 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 73, 126  },
  /* 128 */
  { NACLi_386,
    NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 75, NACL_OPCODE_NULL_OFFSET  },
  /* 129 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 77, 128  },
  /* 130 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_p) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(JumpInstruction) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 131 */
  { NACLi_X87,
    NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 132 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_Addr) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x00, 2, 79, NACL_OPCODE_NULL_OFFSET  },
  /* 133 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_Addr) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMov, 0x00, 2, 81, NACL_OPCODE_NULL_OFFSET  },
  /* 134 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_Addr) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x00, 2, 83, NACL_OPCODE_NULL_OFFSET  },
  /* 135 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_Addr) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMov, 0x00, 2, 85, NACL_OPCODE_NULL_OFFSET  },
  /* 136 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 87, NACL_OPCODE_NULL_OFFSET  },
  /* 137 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 89, NACL_OPCODE_NULL_OFFSET  },
  /* 138 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 91, 137  },
  /* 139 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 93, NACL_OPCODE_NULL_OFFSET  },
  /* 140 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 95, NACL_OPCODE_NULL_OFFSET  },
  /* 141 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 97, 140  },
  /* 142 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 99, NACL_OPCODE_NULL_OFFSET  },
  /* 143 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 101, NACL_OPCODE_NULL_OFFSET  },
  /* 144 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 103, 143  },
  /* 145 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 105, NACL_OPCODE_NULL_OFFSET  },
  /* 146 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 107, NACL_OPCODE_NULL_OFFSET  },
  /* 147 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 109, 146  },
  /* 148 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 111, NACL_OPCODE_NULL_OFFSET  },
  /* 149 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 113, NACL_OPCODE_NULL_OFFSET  },
  /* 150 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 115, 149  },
  /* 151 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x00, 2, 117, NACL_OPCODE_NULL_OFFSET  },
  /* 152 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x01, 2, 117, NACL_OPCODE_NULL_OFFSET  },
  /* 153 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x02, 2, 117, NACL_OPCODE_NULL_OFFSET  },
  /* 154 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x03, 2, 117, NACL_OPCODE_NULL_OFFSET  },
  /* 155 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x04, 2, 117, NACL_OPCODE_NULL_OFFSET  },
  /* 156 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x05, 2, 117, NACL_OPCODE_NULL_OFFSET  },
  /* 157 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x06, 2, 117, NACL_OPCODE_NULL_OFFSET  },
  /* 158 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x07, 2, 117, NACL_OPCODE_NULL_OFFSET  },
  /* 159 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMov, 0x00, 2, 119, NACL_OPCODE_NULL_OFFSET  },
  /* 160 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMov, 0x01, 2, 119, NACL_OPCODE_NULL_OFFSET  },
  /* 161 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMov, 0x02, 2, 119, NACL_OPCODE_NULL_OFFSET  },
  /* 162 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMov, 0x03, 2, 119, NACL_OPCODE_NULL_OFFSET  },
  /* 163 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMov, 0x04, 2, 119, NACL_OPCODE_NULL_OFFSET  },
  /* 164 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMov, 0x05, 2, 119, NACL_OPCODE_NULL_OFFSET  },
  /* 165 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMov, 0x06, 2, 119, NACL_OPCODE_NULL_OFFSET  },
  /* 166 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMov, 0x07, 2, 119, NACL_OPCODE_NULL_OFFSET  },
  /* 167 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x07, 2, 49, NACL_OPCODE_NULL_OFFSET  },
  /* 168 */
  { NACLi_ILLEGAL,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x06, 0, 0, 167  },
  /* 169 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x05, 2, 49, 168  },
  /* 170 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x04, 2, 49, 169  },
  /* 171 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x03, 2, 49, 170  },
  /* 172 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x02, 2, 49, 171  },
  /* 173 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x01, 2, 49, 172  },
  /* 174 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 49, 173  },
  /* 175 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x07, 2, 53, NACL_OPCODE_NULL_OFFSET  },
  /* 176 */
  { NACLi_ILLEGAL,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x06, 0, 0, 175  },
  /* 177 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x05, 2, 53, 176  },
  /* 178 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x04, 2, 53, 177  },
  /* 179 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x03, 2, 53, 178  },
  /* 180 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x02, 2, 53, 179  },
  /* 181 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x01, 2, 53, 180  },
  /* 182 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 53, 181  },
  /* 183 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_w) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 184 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
    InstMov, 0x00, 2, 121, 116  },
  /* 185 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstMov, 0x00, 2, 123, 116  },
  /* 186 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_w) | NACL_IFLAG(OpcodeHasImmed2_b) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 187 */
  { NACLi_RETURN,
    NACL_IFLAG(OpcodeHasImmed_w) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 188 */
  { NACLi_RETURN,
    NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 189 */
  { NACLi_SYSTEM,
    NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 190 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 191 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 192 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 0, 0, 191  },
  /* 193 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x07, 2, 125, NACL_OPCODE_NULL_OFFSET  },
  /* 194 */
  { NACLi_ILLEGAL,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x06, 0, 0, 193  },
  /* 195 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x05, 2, 125, 194  },
  /* 196 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x04, 2, 125, 195  },
  /* 197 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x03, 2, 125, 196  },
  /* 198 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x02, 2, 125, 197  },
  /* 199 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x01, 2, 125, 198  },
  /* 200 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 125, 199  },
  /* 201 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x07, 2, 127, NACL_OPCODE_NULL_OFFSET  },
  /* 202 */
  { NACLi_ILLEGAL,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x06, 0, 0, 201  },
  /* 203 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x05, 2, 127, 202  },
  /* 204 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x04, 2, 127, 203  },
  /* 205 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x03, 2, 127, 204  },
  /* 206 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x02, 2, 127, 205  },
  /* 207 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x01, 2, 127, 206  },
  /* 208 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 127, 207  },
  /* 209 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x07, 2, 129, NACL_OPCODE_NULL_OFFSET  },
  /* 210 */
  { NACLi_ILLEGAL,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x06, 0, 0, 209  },
  /* 211 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x05, 2, 129, 210  },
  /* 212 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x04, 2, 129, 211  },
  /* 213 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x03, 2, 129, 212  },
  /* 214 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x02, 2, 129, 213  },
  /* 215 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x01, 2, 129, 214  },
  /* 216 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 129, 215  },
  /* 217 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x07, 2, 131, NACL_OPCODE_NULL_OFFSET  },
  /* 218 */
  { NACLi_ILLEGAL,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x06, 0, 0, 217  },
  /* 219 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x05, 2, 131, 218  },
  /* 220 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x04, 2, 131, 219  },
  /* 221 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x03, 2, 131, 220  },
  /* 222 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x02, 2, 131, 221  },
  /* 223 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x01, 2, 131, 222  },
  /* 224 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 131, 223  },
  /* 225 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x07, 1, 133, NACL_OPCODE_NULL_OFFSET  },
  /* 226 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x06, 1, 133, 225  },
  /* 227 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x05, 1, 133, 226  },
  /* 228 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x04, 1, 133, 227  },
  /* 229 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x03, 1, 133, 228  },
  /* 230 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x02, 1, 133, 229  },
  /* 231 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x01, 1, 133, 230  },
  /* 232 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 133, 231  },
  /* 233 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x07, 1, 134, NACL_OPCODE_NULL_OFFSET  },
  /* 234 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x06, 1, 135, 233  },
  /* 235 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x05, 1, 136, 234  },
  /* 236 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x04, 1, 137, 235  },
  /* 237 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x03, 1, 138, 236  },
  /* 238 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x02, 1, 138, 237  },
  /* 239 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x01, 0, 0, 238  },
  /* 240 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 133, 239  },
  /* 241 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x07, 1, 135, NACL_OPCODE_NULL_OFFSET  },
  /* 242 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x06, 0, 0, 241  },
  /* 243 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x05, 1, 137, 242  },
  /* 244 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x04, 0, 0, 243  },
  /* 245 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x03, 1, 138, 244  },
  /* 246 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x02, 1, 138, 245  },
  /* 247 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x01, 1, 138, 246  },
  /* 248 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 133, 247  },
  /* 249 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x07, 1, 139, NACL_OPCODE_NULL_OFFSET  },
  /* 250 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x06, 1, 139, 249  },
  /* 251 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x05, 1, 139, 250  },
  /* 252 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x04, 1, 139, 251  },
  /* 253 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x03, 1, 139, 252  },
  /* 254 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x02, 1, 139, 253  },
  /* 255 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x01, 1, 139, 254  },
  /* 256 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 139, 255  },
  /* 257 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x05, 0, 0, 234  },
  /* 258 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x04, 1, 137, 257  },
  /* 259 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x03, 1, 140, 258  },
  /* 260 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x02, 1, 140, 259  },
  /* 261 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x01, 1, 140, 260  },
  /* 262 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 139, 261  },
  /* 263 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x07, 1, 136, NACL_OPCODE_NULL_OFFSET  },
  /* 264 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x06, 1, 136, 263  },
  /* 265 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x05, 1, 136, 264  },
  /* 266 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x04, 1, 136, 265  },
  /* 267 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x03, 1, 136, 266  },
  /* 268 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x02, 1, 136, 267  },
  /* 269 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x01, 1, 136, 268  },
  /* 270 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 136, 269  },
  /* 271 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x06, 1, 135, 241  },
  /* 272 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x05, 1, 137, 271  },
  /* 273 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x04, 1, 137, 272  },
  /* 274 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x03, 1, 134, 273  },
  /* 275 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x02, 1, 134, 274  },
  /* 276 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x01, 1, 134, 275  },
  /* 277 */
  { NACLi_X87,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 136, 276  },
  /* 278 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(JumpInstruction) | NACL_IFLAG(PartialInstruction),
    InstDontCareJump, 0x00, 2, 45, NACL_OPCODE_NULL_OFFSET  },
  /* 279 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(AddressSize_v) | NACL_IFLAG(ConditionalJump) | NACL_IFLAG(PartialInstruction),
    InstDontCareCondJump, 0x00, 3, 141, NACL_OPCODE_NULL_OFFSET  },
  /* 280 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(AddressSize_w) | NACL_IFLAG(ConditionalJump) | NACL_IFLAG(PartialInstruction),
    InstDontCareCondJump, 0x00, 3, 144, 279  },
  /* 281 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 282 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(JumpInstruction),
    InstCall, 0x00, 3, 147, NACL_OPCODE_NULL_OFFSET  },
  /* 283 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(JumpInstruction) | NACL_IFLAG(PartialInstruction),
    InstDontCareJump, 0x00, 2, 150, NACL_OPCODE_NULL_OFFSET  },
  /* 284 */
  { NACLi_386,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 285 */
  { NACLi_386,
    NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 286 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x07, 3, 152, NACL_OPCODE_NULL_OFFSET  },
  /* 287 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x06, 3, 152, 286  },
  /* 288 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x05, 3, 152, 287  },
  /* 289 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x04, 3, 152, 288  },
  /* 290 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x03, 1, 0, 289  },
  /* 291 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x02, 1, 0, 290  },
  /* 292 */
  { NACLi_ILLEGAL,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x01, 0, 0, 291  },
  /* 293 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 47, 292  },
  /* 294 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x07, 3, 155, NACL_OPCODE_NULL_OFFSET  },
  /* 295 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x06, 3, 155, 294  },
  /* 296 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x05, 3, 155, 295  },
  /* 297 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x04, 3, 155, 296  },
  /* 298 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x03, 1, 2, 297  },
  /* 299 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x02, 1, 2, 298  },
  /* 300 */
  { NACLi_ILLEGAL,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x01, 0, 0, 299  },
  /* 301 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_z) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 38, 300  },
  /* 302 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x07, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 303 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x06, 0, 0, 302  },
  /* 304 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x05, 0, 0, 303  },
  /* 305 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x04, 0, 0, 304  },
  /* 306 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x03, 0, 0, 305  },
  /* 307 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x02, 0, 0, 306  },
  /* 308 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x01, 1, 0, 307  },
  /* 309 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 0, 308  },
  /* 310 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
    InstPush, 0x06, 2, 158, 302  },
  /* 311 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(JumpInstruction) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x05, 0, 0, 310  },
  /* 312 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(JumpInstruction) | NACL_IFLAG(PartialInstruction),
    InstDontCareJump, 0x04, 2, 160, 311  },
  /* 313 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(JumpInstruction) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x03, 0, 0, 312  },
  /* 314 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(JumpInstruction),
    InstCall, 0x02, 3, 162, 313  },
  /* 315 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x01, 1, 2, 314  },
  /* 316 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 2, 315  },
  /* 317 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x05, 0, 0, 303  },
  /* 318 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x04, 0, 0, 317  },
  /* 319 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x03, 0, 0, 318  },
  /* 320 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x02, 0, 0, 319  },
  /* 321 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x01, 0, 0, 320  },
  /* 322 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 0, 0, 321  },
  /* 323 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x07, 0, 0, 302  },
  /* 324 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstLmsw, 0x06, 1, 165, 323  },
  /* 325 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x05, 0, 0, 324  },
  /* 326 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x04, 0, 0, 325  },
  /* 327 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x03, 0, 0, 326  },
  /* 328 */
  { NACLi_SVM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x73, 0, 0, 327  },
  /* 329 */
  { NACLi_SVM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x63, 0, 0, 328  },
  /* 330 */
  { NACLi_SVM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x53, 0, 0, 329  },
  /* 331 */
  { NACLi_SVM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x43, 0, 0, 330  },
  /* 332 */
  { NACLi_SVM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x33, 0, 0, 331  },
  /* 333 */
  { NACLi_SVM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x23, 0, 0, 332  },
  /* 334 */
  { NACLi_SVM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x13, 0, 0, 333  },
  /* 335 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x03, 0, 0, 334  },
  /* 336 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x02, 0, 0, 335  },
  /* 337 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x01, 0, 0, 336  },
  /* 338 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x11, 0, 0, 337  },
  /* 339 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x01, 0, 0, 338  },
  /* 340 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x01, 0, 0, 339  },
  /* 341 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 0, 0, 340  },
  /* 342 */
  { NACLi_SYSTEM,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 343 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x07, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 344 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x06, 0, 0, 343  },
  /* 345 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x05, 0, 0, 344  },
  /* 346 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x04, 0, 0, 345  },
  /* 347 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x03, 1, 166, 346  },
  /* 348 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x02, 0, 0, 347  },
  /* 349 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x01, 1, 166, 348  },
  /* 350 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 166, 349  },
  /* 351 */
  { NACLi_3DNOW,
    NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 352 */
  { NACLi_INVALID,
    NACL_IFLAG(Opcode0F0F) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00, 3, 167, NACL_OPCODE_NULL_OFFSET  },
  /* 353 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 170, NACL_OPCODE_NULL_OFFSET  },
  /* 354 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 171, NACL_OPCODE_NULL_OFFSET  },
  /* 355 */
  { NACLi_SSE,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 172, NACL_OPCODE_NULL_OFFSET  },
  /* 356 */
  { NACLi_SSE,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 139, 355  },
  /* 357 */
  { NACLi_SSE,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 140, NACL_OPCODE_NULL_OFFSET  },
  /* 358 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 173, NACL_OPCODE_NULL_OFFSET  },
  /* 359 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x07, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 360 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x06, 0, 0, 359  },
  /* 361 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x05, 0, 0, 360  },
  /* 362 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x04, 0, 0, 361  },
  /* 363 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x03, 1, 166, 362  },
  /* 364 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x02, 1, 166, 363  },
  /* 365 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x01, 1, 166, 364  },
  /* 366 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 166, 365  },
  /* 367 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 368 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 0, 0, 367  },
  /* 369 */
  { NACLi_SYSTEM,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 370 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 174, NACL_OPCODE_NULL_OFFSET  },
  /* 371 */
  { NACLi_SSE,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 175, NACL_OPCODE_NULL_OFFSET  },
  /* 372 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 176, NACL_OPCODE_NULL_OFFSET  },
  /* 373 */
  { NACLi_RDMSR,
    NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 374 */
  { NACLi_RDTSC,
    NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 177, NACL_OPCODE_NULL_OFFSET  },
  /* 375 */
  { NACLi_SYSENTER,
    NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 376 */
  { NACLi_CMOV,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 37, NACL_OPCODE_NULL_OFFSET  },
  /* 377 */
  { NACLi_SSE,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 179, NACL_OPCODE_NULL_OFFSET  },
  /* 378 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 170, NACL_OPCODE_NULL_OFFSET  },
  /* 379 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 181, NACL_OPCODE_NULL_OFFSET  },
  /* 380 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 174, NACL_OPCODE_NULL_OFFSET  },
  /* 381 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 182, NACL_OPCODE_NULL_OFFSET  },
  /* 382 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 183, NACL_OPCODE_NULL_OFFSET  },
  /* 383 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 184, NACL_OPCODE_NULL_OFFSET  },
  /* 384 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x06, 2, 186, 302  },
  /* 385 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x05, 0, 0, 384  },
  /* 386 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x04, 2, 186, 385  },
  /* 387 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x03, 0, 0, 386  },
  /* 388 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x02, 2, 186, 387  },
  /* 389 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x01, 0, 0, 388  },
  /* 390 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00, 0, 0, 389  },
  /* 391 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x04, 0, 0, 385  },
  /* 392 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x03, 0, 0, 391  },
  /* 393 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x02, 2, 186, 392  },
  /* 394 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x01, 0, 0, 393  },
  /* 395 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00, 0, 0, 394  },
  /* 396 */
  { NACLi_MMX,
    NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 397 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 188, NACL_OPCODE_NULL_OFFSET  },
  /* 398 */
  { NACLi_MMX,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 189, NACL_OPCODE_NULL_OFFSET  },
  /* 399 */
  { NACLi_386,
    NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(ConditionalJump) | NACL_IFLAG(PartialInstruction),
    InstDontCareCondJump, 0x00, 2, 150, NACL_OPCODE_NULL_OFFSET  },
  /* 400 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 59, NACL_OPCODE_NULL_OFFSET  },
  /* 401 */
  { NACLi_386,
    NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 4, 190, NACL_OPCODE_NULL_OFFSET  },
  /* 402 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 403 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 3, 194, NACL_OPCODE_NULL_OFFSET  },
  /* 404 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 3, 197, NACL_OPCODE_NULL_OFFSET  },
  /* 405 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 406 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 3, 200, NACL_OPCODE_NULL_OFFSET  },
  /* 407 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 3, 203, NACL_OPCODE_NULL_OFFSET  },
  /* 408 */
  { NACLi_SFENCE_CLFLUSH,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x07, 1, 206, NACL_OPCODE_NULL_OFFSET  },
  /* 409 */
  { NACLi_SFENCE_CLFLUSH,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x77, 0, 0, 408  },
  /* 410 */
  { NACLi_SFENCE_CLFLUSH,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x67, 0, 0, 409  },
  /* 411 */
  { NACLi_SFENCE_CLFLUSH,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x57, 0, 0, 410  },
  /* 412 */
  { NACLi_SFENCE_CLFLUSH,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x47, 0, 0, 411  },
  /* 413 */
  { NACLi_SFENCE_CLFLUSH,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x37, 0, 0, 412  },
  /* 414 */
  { NACLi_SFENCE_CLFLUSH,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x27, 0, 0, 413  },
  /* 415 */
  { NACLi_SFENCE_CLFLUSH,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x17, 0, 0, 414  },
  /* 416 */
  { NACLi_SFENCE_CLFLUSH,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x07, 0, 0, 415  },
  /* 417 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x76, 0, 0, 416  },
  /* 418 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x66, 0, 0, 417  },
  /* 419 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x56, 0, 0, 418  },
  /* 420 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x46, 0, 0, 419  },
  /* 421 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x36, 0, 0, 420  },
  /* 422 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x26, 0, 0, 421  },
  /* 423 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x16, 0, 0, 422  },
  /* 424 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x06, 0, 0, 423  },
  /* 425 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x75, 0, 0, 424  },
  /* 426 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x65, 0, 0, 425  },
  /* 427 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x55, 0, 0, 426  },
  /* 428 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x45, 0, 0, 427  },
  /* 429 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x35, 0, 0, 428  },
  /* 430 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x25, 0, 0, 429  },
  /* 431 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x15, 0, 0, 430  },
  /* 432 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeInModRmRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x05, 0, 0, 431  },
  /* 433 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x04, 0, 0, 432  },
  /* 434 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x03, 1, 138, 433  },
  /* 435 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x02, 1, 133, 434  },
  /* 436 */
  { NACLi_FXSAVE,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x01, 0, 0, 435  },
  /* 437 */
  { NACLi_FXSAVE,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 0, 0, 436  },
  /* 438 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 6, NACL_OPCODE_NULL_OFFSET  },
  /* 439 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 3, 207, NACL_OPCODE_NULL_OFFSET  },
  /* 440 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 3, 210, NACL_OPCODE_NULL_OFFSET  },
  /* 441 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 213, NACL_OPCODE_NULL_OFFSET  },
  /* 442 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 215, NACL_OPCODE_NULL_OFFSET  },
  /* 443 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x07, 0, 0, 116  },
  /* 444 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x06, 0, 0, 443  },
  /* 445 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x05, 0, 0, 444  },
  /* 446 */
  { NACLi_386,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x04, 0, 0, 445  },
  /* 447 */
  { NACLi_386,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 37, NACL_OPCODE_NULL_OFFSET  },
  /* 448 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 217, NACL_OPCODE_NULL_OFFSET  },
  /* 449 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 219, NACL_OPCODE_NULL_OFFSET  },
  /* 450 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 221, NACL_OPCODE_NULL_OFFSET  },
  /* 451 */
  { NACLi_SSE41,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 3, 223, NACL_OPCODE_NULL_OFFSET  },
  /* 452 */
  { NACLi_CMPXCHG8B,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x01, 3, 226, 116  },
  /* 453 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 229, NACL_OPCODE_NULL_OFFSET  },
  /* 454 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 230, 453  },
  /* 455 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x01, 1, 229, NACL_OPCODE_NULL_OFFSET  },
  /* 456 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x01, 1, 230, 455  },
  /* 457 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x02, 1, 229, NACL_OPCODE_NULL_OFFSET  },
  /* 458 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x02, 1, 230, 457  },
  /* 459 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x03, 1, 229, NACL_OPCODE_NULL_OFFSET  },
  /* 460 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x03, 1, 230, 459  },
  /* 461 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x04, 1, 229, NACL_OPCODE_NULL_OFFSET  },
  /* 462 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x04, 1, 230, 461  },
  /* 463 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x05, 1, 229, NACL_OPCODE_NULL_OFFSET  },
  /* 464 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x05, 1, 230, 463  },
  /* 465 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x06, 1, 229, NACL_OPCODE_NULL_OFFSET  },
  /* 466 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x06, 1, 230, 465  },
  /* 467 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x07, 1, 229, NACL_OPCODE_NULL_OFFSET  },
  /* 468 */
  { NACLi_386,
    NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x07, 1, 230, 467  },
  /* 469 */
  { NACLi_MMX,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 223, NACL_OPCODE_NULL_OFFSET  },
  /* 470 */
  { NACLi_MMX,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 140, NACL_OPCODE_NULL_OFFSET  },
  /* 471 */
  { NACLi_MMX,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 231, NACL_OPCODE_NULL_OFFSET  },
  /* 472 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 233, NACL_OPCODE_NULL_OFFSET  },
  /* 473 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 234, NACL_OPCODE_NULL_OFFSET  },
  /* 474 */
  { NACLi_SSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 233, NACL_OPCODE_NULL_OFFSET  },
  /* 475 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 476 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 235, NACL_OPCODE_NULL_OFFSET  },
  /* 477 */
  { NACLi_SSE4A,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 140, NACL_OPCODE_NULL_OFFSET  },
  /* 478 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 236, NACL_OPCODE_NULL_OFFSET  },
  /* 479 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 238, NACL_OPCODE_NULL_OFFSET  },
  /* 480 */
  { NACLi_SSE4A,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeHasImmed2_b) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 3, 240, NACL_OPCODE_NULL_OFFSET  },
  /* 481 */
  { NACLi_SSE4A,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 243, NACL_OPCODE_NULL_OFFSET  },
  /* 482 */
  { NACLi_SSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 170, NACL_OPCODE_NULL_OFFSET  },
  /* 483 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 244, NACL_OPCODE_NULL_OFFSET  },
  /* 484 */
  { NACLi_SSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 246, NACL_OPCODE_NULL_OFFSET  },
  /* 485 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 172, NACL_OPCODE_NULL_OFFSET  },
  /* 486 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 246, NACL_OPCODE_NULL_OFFSET  },
  /* 487 */
  { NACLi_SSE3,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 247, NACL_OPCODE_NULL_OFFSET  },
  /* 488 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 176, NACL_OPCODE_NULL_OFFSET  },
  /* 489 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 248, NACL_OPCODE_NULL_OFFSET  },
  /* 490 */
  { NACLi_SSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 170, NACL_OPCODE_NULL_OFFSET  },
  /* 491 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 492 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 235, NACL_OPCODE_NULL_OFFSET  },
  /* 493 */
  { NACLi_SSE4A,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 138, NACL_OPCODE_NULL_OFFSET  },
  /* 494 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 249, NACL_OPCODE_NULL_OFFSET  },
  /* 495 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 170, NACL_OPCODE_NULL_OFFSET  },
  /* 496 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 176, NACL_OPCODE_NULL_OFFSET  },
  /* 497 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 170, NACL_OPCODE_NULL_OFFSET  },
  /* 498 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 181, NACL_OPCODE_NULL_OFFSET  },
  /* 499 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 238, NACL_OPCODE_NULL_OFFSET  },
  /* 500 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 173, NACL_OPCODE_NULL_OFFSET  },
  /* 501 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 251, NACL_OPCODE_NULL_OFFSET  },
  /* 502 */
  { NACLi_POPCNT,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 37, NACL_OPCODE_NULL_OFFSET  },
  /* 503 */
  { NACLi_LZCNT,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 37, NACL_OPCODE_NULL_OFFSET  },
  /* 504 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 252, NACL_OPCODE_NULL_OFFSET  },
  /* 505 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRep) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 224, NACL_OPCODE_NULL_OFFSET  },
  /* 506 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 246, NACL_OPCODE_NULL_OFFSET  },
  /* 507 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 254, NACL_OPCODE_NULL_OFFSET  },
  /* 508 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 139, NACL_OPCODE_NULL_OFFSET  },
  /* 509 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 140, NACL_OPCODE_NULL_OFFSET  },
  /* 510 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 173, NACL_OPCODE_NULL_OFFSET  },
  /* 511 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 174, NACL_OPCODE_NULL_OFFSET  },
  /* 512 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 175, NACL_OPCODE_NULL_OFFSET  },
  /* 513 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 233, NACL_OPCODE_NULL_OFFSET  },
  /* 514 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 255, NACL_OPCODE_NULL_OFFSET  },
  /* 515 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 516 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 170, NACL_OPCODE_NULL_OFFSET  },
  /* 517 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 181, NACL_OPCODE_NULL_OFFSET  },
  /* 518 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 183, NACL_OPCODE_NULL_OFFSET  },
  /* 519 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 257, NACL_OPCODE_NULL_OFFSET  },
  /* 520 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x07, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 521 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x06, 2, 259, 520  },
  /* 522 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x05, 0, 0, 521  },
  /* 523 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x04, 2, 259, 522  },
  /* 524 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x03, 0, 0, 523  },
  /* 525 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x02, 2, 259, 524  },
  /* 526 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x01, 0, 0, 525  },
  /* 527 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00, 0, 0, 526  },
  /* 528 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x07, 2, 259, NACL_OPCODE_NULL_OFFSET  },
  /* 529 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x06, 2, 259, 528  },
  /* 530 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x05, 0, 0, 529  },
  /* 531 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x04, 0, 0, 530  },
  /* 532 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x03, 2, 259, 531  },
  /* 533 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x02, 2, 259, 532  },
  /* 534 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x01, 0, 0, 533  },
  /* 535 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00, 0, 0, 534  },
  /* 536 */
  { NACLi_INVALID,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal),
    InstInvalid, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 537 */
  { NACLi_SSE4A,
    NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeHasImmed2_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 0, 0, 536  },
  /* 538 */
  { NACLi_SSE4A,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 243, NACL_OPCODE_NULL_OFFSET  },
  /* 539 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 188, NACL_OPCODE_NULL_OFFSET  },
  /* 540 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 251, NACL_OPCODE_NULL_OFFSET  },
  /* 541 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 261, NACL_OPCODE_NULL_OFFSET  },
  /* 542 */
  { NACLi_SSE,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 221, NACL_OPCODE_NULL_OFFSET  },
  /* 543 */
  { NACLi_SSE41,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 3, 263, NACL_OPCODE_NULL_OFFSET  },
  /* 544 */
  { NACLi_SSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 246, NACL_OPCODE_NULL_OFFSET  },
  /* 545 */
  { NACLi_SSE2,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 266, NACL_OPCODE_NULL_OFFSET  },
  /* 546 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 263, NACL_OPCODE_NULL_OFFSET  },
  /* 547 */
  { NACLi_SSE2,
    NACL_IFLAG(ModRmModIs0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 267, NACL_OPCODE_NULL_OFFSET  },
  /* 548 */
  { NACLi_E3DNOW,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 174, NACL_OPCODE_NULL_OFFSET  },
  /* 549 */
  { NACLi_3DNOW,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 174, NACL_OPCODE_NULL_OFFSET  },
  /* 550 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 174, NACL_OPCODE_NULL_OFFSET  },
  /* 551 */
  { NACLi_MOVBE,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 269, NACL_OPCODE_NULL_OFFSET  },
  /* 552 */
  { NACLi_MOVBE,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 271, NACL_OPCODE_NULL_OFFSET  },
  /* 553 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 181, NACL_OPCODE_NULL_OFFSET  },
  /* 554 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 181, NACL_OPCODE_NULL_OFFSET  },
  /* 555 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 273, NACL_OPCODE_NULL_OFFSET  },
  /* 556 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 274, NACL_OPCODE_NULL_OFFSET  },
  /* 557 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 275, NACL_OPCODE_NULL_OFFSET  },
  /* 558 */
  { NACLi_SSE41,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 247, NACL_OPCODE_NULL_OFFSET  },
  /* 559 */
  { NACLi_SSE42,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 181, NACL_OPCODE_NULL_OFFSET  },
  /* 560 */
  { NACLi_VMX,
    NACL_IFLAG(ModRmModIsnt0x3) | NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(NaClIllegal) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 561 */
  { NACLi_SSE42,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(OperandSize_b) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 276, NACL_OPCODE_NULL_OFFSET  },
  /* 562 */
  { NACLi_SSE42,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeAllowsRepne) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 278, NACL_OPCODE_NULL_OFFSET  },
  /* 563 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 184, NACL_OPCODE_NULL_OFFSET  },
  /* 564 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 257, NACL_OPCODE_NULL_OFFSET  },
  /* 565 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 252, NACL_OPCODE_NULL_OFFSET  },
  /* 566 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 244, NACL_OPCODE_NULL_OFFSET  },
  /* 567 */
  { NACLi_SSSE3,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 257, NACL_OPCODE_NULL_OFFSET  },
  /* 568 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 280, NACL_OPCODE_NULL_OFFSET  },
  /* 569 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 282, NACL_OPCODE_NULL_OFFSET  },
  /* 570 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 284, NACL_OPCODE_NULL_OFFSET  },
  /* 571 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 286, NACL_OPCODE_NULL_OFFSET  },
  /* 572 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 288, NACL_OPCODE_NULL_OFFSET  },
  /* 573 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 290, NACL_OPCODE_NULL_OFFSET  },
  /* 574 */
  { NACLi_SSE41,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 292, NACL_OPCODE_NULL_OFFSET  },
  /* 575 */
  { NACLi_SSE42,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 4, 294, NACL_OPCODE_NULL_OFFSET  },
  /* 576 */
  { NACLi_SSE42,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 5, 298, NACL_OPCODE_NULL_OFFSET  },
  /* 577 */
  { NACLi_SSE42,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 2, 257, NACL_OPCODE_NULL_OFFSET  },
  /* 578 */
  { NACLi_SSE42,
    NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeAllowsData16) | NACL_IFLAG(SizeIgnoresData16) | NACL_IFLAG(OperandSize_v) | NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 3, 303, NACL_OPCODE_NULL_OFFSET  },
  /* 579 */
  { NACLi_X87_FSINCOS,
    NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 580 */
  { NACLi_X87,
    NACL_EMPTY_IFLAGS,
    InstInvalid, 0x00, 0, 0, NACL_OPCODE_NULL_OFFSET  },
  /* 581 */
  { NACLi_X87,
    NACL_IFLAG(PartialInstruction),
    InstDontCare, 0x00, 1, 73, NACL_OPCODE_NULL_OFFSET  },
};

static const NaClPrefixOpcodeArrayOffset g_LookupTable[2543] = {
  /*     0 */ 1, 2, 3, 4, 5, 6, 7, 7, 8, 9, 
  /*    10 */ 10, 11, 12, 13, 7, 14, 15, 16, 17, 18, 
  /*    20 */ 19, 20, 7, 7, 15, 16, 17, 18, 19, 20, 
  /*    30 */ 7, 7, 21, 22, 23, 24, 25, 26, 14, 7, 
  /*    40 */ 27, 28, 29, 30, 31, 32, 14, 7, 15, 16, 
  /*    50 */ 17, 18, 19, 20, 14, 7, 33, 34, 35, 36, 
  /*    60 */ 37, 38, 14, 7, 39, 40, 41, 42, 43, 44, 
  /*    70 */ 45, 46, 39, 40, 41, 42, 43, 44, 45, 46, 
  /*    80 */ 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 
  /*    90 */ 57, 58, 59, 60, 61, 62, 64, 64, 65, 66, 
  /*   100 */ 14, 14, 14, 14, 67, 68, 69, 70, 71, 73, 
  /*   110 */ 71, 73, 74, 74, 74, 74, 74, 74, 74, 74, 
  /*   120 */ 74, 74, 74, 74, 74, 74, 74, 74, 82, 90, 
  /*   130 */ 98, 106, 33, 34, 107, 108, 109, 110, 111, 112, 
  /*   140 */ 113, 114, 115, 117, 118, 119, 120, 121, 122, 123, 
  /*   150 */ 124, 125, 127, 129, 130, 131, 64, 64, 7, 7, 
  /*   160 */ 132, 133, 134, 135, 136, 138, 139, 141, 37, 38, 
  /*   170 */ 142, 144, 145, 147, 148, 150, 151, 152, 153, 154, 
  /*   180 */ 155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 
  /*   190 */ 165, 166, 174, 182, 183, 7, 65, 65, 184, 185, 
  /*   200 */ 186, 7, 187, 188, 189, 190, 7, 192, 200, 208, 
  /*   210 */ 216, 224, 190, 190, 7, 7, 232, 240, 232, 248, 
  /*   220 */ 256, 262, 270, 277, 74, 74, 278, 280, 190, 281, 
  /*   230 */ 190, 281, 282, 283, 130, 278, 7, 284, 7, 284, 
  /*   240 */ 14, 7, 14, 14, 285, 285, 293, 301, 285, 285, 
  /*   250 */ 189, 189, 285, 285, 309, 316, 322, 341, 342, 342, 
  /*   260 */ 14, NACL_OPCODE_NULL_OFFSET, 189, NACL_OPCODE_NULL_OFFSET, 189, 189, 14, 7, 14, 350, 
  /*   270 */ 351, 352, 353, 354, 356, 357, 358, 358, 356, 357, 
  /*   280 */ 366, 367, 367, 367, 367, 367, 367, 368, 369, 369, 
  /*   290 */ 369, 369, 14, 14, 14, 14, 353, 354, 370, 371, 
  /*   300 */ 353, 353, 372, 353, 373, 374, 373, 189, 375, 375, 
  /*   310 */ 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
  /*   320 */ 376, 376, 376, 376, 376, 376, 376, 376, 376, 376, 
  /*   330 */ 376, 376, 376, 376, 376, 376, 377, 353, 353, 353, 
  /*   340 */ 353, 353, 353, 353, 353, 353, 378, 379, 353, 353, 
  /*   350 */ 353, 353, 380, 380, 380, 380, 380, 380, 380, 380, 
  /*   360 */ 381, 381, 381, 380, 14, 14, 382, 380, 383, 390, 
  /*   370 */ 390, 395, 380, 380, 380, 396, 14, 14, 14, 14, 
  /*   380 */ 14, 14, 397, 398, 399, 399, 399, 399, 399, 399, 
  /*   390 */ 399, 399, 399, 399, 399, 399, 399, 399, 399, 399, 
  /*   400 */ 400, 400, 400, 400, 400, 400, 400, 400, 400, 400, 
  /*   410 */ 400, 400, 400, 400, 400, 400, 7, 7, 401, 402, 
  /*   420 */ 403, 404, 14, 14, 7, 7, 189, 405, 406, 407, 
  /*   430 */ 437, 438, 439, 440, 65, 405, 65, 65, 441, 442, 
  /*   440 */ 14, 116, 446, 405, 447, 447, 441, 442, 107, 108, 
  /*   450 */ 448, 449, 450, 451, 448, 452, 454, 456, 458, 460, 
  /*   460 */ 462, 464, 466, 468, 14, 380, 380, 380, 380, 380, 
  /*   470 */ 14, 469, 380, 380, 380, 380, 380, 380, 380, 380, 
  /*   480 */ 380, 380, 380, 380, 380, 380, 14, 470, 380, 380, 
  /*   490 */ 380, 380, 380, 380, 380, 380, 14, 380, 380, 380, 
  /*   500 */ 380, 380, 380, 471, 380, 380, 380, 380, 380, 380, 
  /*   510 */ 380, 14, NACL_OPCODE_NULL_OFFSET, 472, 473, 474, 475, 475, 475, 475, 
  /*   520 */ 475, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   530 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 475, 475, 476, 
  /*   540 */ 477, 478, 478, 475, 475, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   550 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   560 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   570 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 475, 472, 475, 
  /*   580 */ 475, 475, 475, 475, 475, 472, 472, 472, 475, 472, 
  /*   590 */ 472, 472, 472, 475, 475, 475, 475, 475, 475, 475, 
  /*   600 */ 475, 475, 475, 475, 475, 475, 475, 475, 475, 479, 
  /*   610 */ 475, 475, 475, 475, 475, 475, 475, 480, 481, 475, 
  /*   620 */ 475, 482, 482, 475, 475, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   630 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   640 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   650 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   660 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   670 */ NACL_OPCODE_NULL_OFFSET, 475, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   680 */ NACL_OPCODE_NULL_OFFSET, 475, 475, 475, 475, 475, 475, 475, 475, NACL_OPCODE_NULL_OFFSET, 
  /*   690 */ NACL_OPCODE_NULL_OFFSET, 483, 475, 475, 475, 475, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   700 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 484, 475, 475, 475, 475, 
  /*   710 */ 475, 485, 475, 475, 475, 475, 475, 475, 475, 475, 
  /*   720 */ 475, 475, 475, 475, 475, 475, 475, 486, 475, 475, 
  /*   730 */ 475, 475, 475, 475, 475, 475, 475, 487, 475, 475, 
  /*   740 */ 475, 475, 475, 475, 475, 475, 475, 475, 475, 475, 
  /*   750 */ 475, 475, 475, NACL_OPCODE_NULL_OFFSET, 488, 489, 490, 491, 491, 491, 
  /*   760 */ 490, 491, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   770 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 491, 491, 
  /*   780 */ 492, 493, 494, 494, 491, 491, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   790 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   800 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   810 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 491, 495, 
  /*   820 */ 488, 488, 491, 491, 491, 491, 488, 488, 496, 497, 
  /*   830 */ 488, 488, 488, 488, 491, 491, 491, 491, 491, 491, 
  /*   840 */ 491, 491, 491, 491, 491, 491, 491, 491, 491, 498, 
  /*   850 */ 499, 491, 491, 491, 491, 491, 491, 491, 491, 491, 
  /*   860 */ 491, 491, 491, 491, 500, 501, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   870 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   880 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   890 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   900 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   910 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   920 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 502, 491, 491, 491, 491, 503, 491, 491, 
  /*   930 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 504, 491, 491, 491, 491, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*   940 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 491, 491, 491, 491, 
  /*   950 */ 491, 491, 505, 491, 491, 491, 491, 491, 491, 491, 
  /*   960 */ 491, 491, 491, 491, 491, 491, 491, 491, 500, 491, 
  /*   970 */ 491, 491, 491, 491, 491, 491, 491, 491, 491, 491, 
  /*   980 */ 491, 491, 491, 491, 491, 491, 491, 491, 491, 491, 
  /*   990 */ 491, 491, 491, 491, NACL_OPCODE_NULL_OFFSET, 506, 507, 508, 509, 510, 
  /*  1000 */ 510, 508, 509, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1010 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 506, 
  /*  1020 */ 507, 511, 512, 506, 506, 513, 513, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1030 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1040 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1050 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 514, 
  /*  1060 */ 506, 515, 515, 506, 506, 506, 506, 506, 506, 506, 
  /*  1070 */ 516, 506, 506, 506, 506, 510, 510, 510, 517, 517, 
  /*  1080 */ 517, 517, 517, 510, 510, 510, 517, 510, 510, 518, 
  /*  1090 */ 517, 519, 527, 527, 535, 517, 517, 517, 515, 537, 
  /*  1100 */ 538, 515, 515, 506, 506, 539, 540, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1110 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1120 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1130 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1140 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1150 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 515, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1160 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1170 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 541, 515, 542, 543, 541, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1180 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 544, 517, 517, 
  /*  1190 */ 517, 517, 517, 545, 546, 517, 517, 517, 517, 517, 
  /*  1200 */ 517, 517, 517, 517, 517, 517, 517, 517, 517, 517, 
  /*  1210 */ 512, 517, 517, 517, 517, 517, 517, 517, 517, 515, 
  /*  1220 */ 517, 517, 517, 517, 517, 517, 547, 517, 517, 517, 
  /*  1230 */ 517, 517, 517, 517, 515, NACL_OPCODE_NULL_OFFSET, 548, 549, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1240 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1250 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 548, 549, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
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
  /*  1360 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 548, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 548, NACL_OPCODE_NULL_OFFSET, 549, NACL_OPCODE_NULL_OFFSET, 
  /*  1370 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 549, NACL_OPCODE_NULL_OFFSET, 549, 549, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 549, NACL_OPCODE_NULL_OFFSET, 
  /*  1380 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 549, NACL_OPCODE_NULL_OFFSET, 549, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 549, NACL_OPCODE_NULL_OFFSET, 
  /*  1390 */ 549, 549, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 549, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 549, NACL_OPCODE_NULL_OFFSET, 
  /*  1400 */ 549, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 549, NACL_OPCODE_NULL_OFFSET, 549, 549, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1410 */ NACL_OPCODE_NULL_OFFSET, 548, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 549, NACL_OPCODE_NULL_OFFSET, 550, 550, 550, 
  /*  1420 */ 550, 550, 550, 550, 550, 550, 550, 550, 550, 14, 
  /*  1430 */ 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
  /*  1440 */ 14, 14, 14, 14, 14, 550, 550, 550, 14, 14, 
  /*  1450 */ 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
  /*  1460 */ 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
  /*  1470 */ 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
  /*  1480 */ 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
  /*  1490 */ 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
  /*  1500 */ 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
  /*  1510 */ 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
  /*  1520 */ 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
  /*  1530 */ 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
  /*  1540 */ 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
  /*  1550 */ 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
  /*  1560 */ 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
  /*  1570 */ 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
  /*  1580 */ 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
  /*  1590 */ 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
  /*  1600 */ 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
  /*  1610 */ 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
  /*  1620 */ 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
  /*  1630 */ 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
  /*  1640 */ 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
  /*  1650 */ 14, 14, 14, 14, 14, 14, 14, 551, 552, 14, 
  /*  1660 */ 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
  /*  1670 */ 14, 14, 14, 553, 553, 553, 553, 553, 553, 553, 
  /*  1680 */ 553, 553, 553, 553, 553, 515, 515, 515, 515, 554, 
  /*  1690 */ 515, 515, 515, 554, 554, 515, 554, 515, 515, 515, 
  /*  1700 */ 515, 553, 553, 553, 515, 555, 556, 557, 555, 556, 
  /*  1710 */ 555, 515, 515, 554, 554, 558, 554, 515, 515, 515, 
  /*  1720 */ 515, 555, 556, 557, 555, 556, 555, 515, 559, 554, 
  /*  1730 */ 554, 554, 554, 554, 554, 554, 554, 554, 554, 515, 
  /*  1740 */ 515, 515, 515, 515, 515, 515, 515, 515, 515, 515, 
  /*  1750 */ 515, 515, 515, 515, 515, 515, 515, 515, 515, 515, 
  /*  1760 */ 515, 515, 515, 515, 515, 515, 515, 515, 515, 515, 
  /*  1770 */ 515, 515, 515, 515, 515, 515, 515, 515, 515, 515, 
  /*  1780 */ 515, 515, 515, 515, 515, 515, 515, 515, 515, 515, 
  /*  1790 */ 515, 515, 515, 515, 515, 515, 515, 515, 515, 515, 
  /*  1800 */ 515, 560, 560, 515, 515, 515, 515, 515, 515, 515, 
  /*  1810 */ 515, 515, 515, 515, 515, 515, 515, 515, 515, 515, 
  /*  1820 */ 515, 515, 515, 515, 515, 515, 515, 515, 515, 515, 
  /*  1830 */ 515, 515, 515, 515, 515, 515, 515, 515, 515, 515, 
  /*  1840 */ 515, 515, 515, 515, 515, 515, 515, 515, 515, 515, 
  /*  1850 */ 515, 515, 515, 515, 515, 515, 515, 515, 515, 515, 
  /*  1860 */ 515, 515, 515, 515, 515, 515, 515, 515, 515, 515, 
  /*  1870 */ 515, 515, 515, 515, 515, 515, 515, 515, 515, 515, 
  /*  1880 */ 515, 515, 515, 515, 515, 515, 515, 515, 515, 515, 
  /*  1890 */ 515, 515, 515, 515, 515, 515, 515, 515, 515, 515, 
  /*  1900 */ 515, 515, 515, 515, 515, 515, 515, 515, 515, 515, 
  /*  1910 */ 515, 515, 515, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 515, 515, 515, 515, 515, 
  /*  1920 */ 515, 515, 515, 515, 515, 515, 515, 515, 515, NACL_OPCODE_NULL_OFFSET, 
  /*  1930 */ 561, 562, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 563, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 564, 564, 565, 
  /*  1940 */ 566, 564, 564, 564, 567, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 568, 
  /*  1950 */ 569, 570, 571, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1960 */ NACL_OPCODE_NULL_OFFSET, 572, 573, 574, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1970 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1980 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  1990 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 564, 564, 564, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  2000 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  2010 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 
  /*  2020 */ NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 575, 576, 577, 578, NACL_OPCODE_NULL_OFFSET, 
  /*  2030 */ NACL_OPCODE_NULL_OFFSET, 131, 131, 131, 131, 131, 131, 131, 131, 131, 
  /*  2040 */ 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 
  /*  2050 */ 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 
  /*  2060 */ 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 
  /*  2070 */ 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 
  /*  2080 */ 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 
  /*  2090 */ 131, 131, 131, 131, 131, NACL_OPCODE_NULL_OFFSET, 131, 131, 131, 131, 
  /*  2100 */ 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 
  /*  2110 */ 131, 131, 131, 14, 14, 14, 14, 14, 14, 14, 
  /*  2120 */ 14, 14, 14, 14, 14, 14, 14, 14, 131, 131, 
  /*  2130 */ 14, 14, 131, 131, 14, 14, 131, 131, 131, 131, 
  /*  2140 */ 131, 131, 131, 14, 131, 131, 131, 131, 131, 131, 
  /*  2150 */ 131, 131, 131, 131, 131, 579, 131, 131, 131, 131, 
  /*  2160 */ NACL_OPCODE_NULL_OFFSET, 131, 131, 131, 131, 131, 131, 131, 131, 131, 
  /*  2170 */ 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 
  /*  2180 */ 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 
  /*  2190 */ 131, 131, 131, 14, 14, 14, 14, 14, 14, 14, 
  /*  2200 */ 14, 14, 131, 14, 14, 14, 14, 14, 14, 14, 
  /*  2210 */ 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
  /*  2220 */ 14, 14, 14, 14, 14, NACL_OPCODE_NULL_OFFSET, 131, 131, 131, 131, 
  /*  2230 */ 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 
  /*  2240 */ 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 
  /*  2250 */ 131, 131, 131, 131, 131, 131, 131, 131, 14, 14, 
  /*  2260 */ 131, 131, 14, 14, 14, 14, 131, 131, 131, 131, 
  /*  2270 */ 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 
  /*  2280 */ 131, 131, NACL_OPCODE_NULL_OFFSET, NACL_OPCODE_NULL_OFFSET, 131, 131, 131, 131, 131, 131, 
  /*  2290 */ 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 
  /*  2300 */ 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
  /*  2310 */ 14, 14, 14, 14, 14, 14, 131, 131, 131, 131, 
  /*  2320 */ 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 
  /*  2330 */ 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 
  /*  2340 */ 131, 131, 131, 131, 131, 131, 131, 131, NACL_OPCODE_NULL_OFFSET, 131, 
  /*  2350 */ 131, 131, 131, 131, 131, 131, 131, 14, 14, 14, 
  /*  2360 */ 14, 14, 14, 14, 14, 131, 131, 131, 131, 131, 
  /*  2370 */ 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 
  /*  2380 */ 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 
  /*  2390 */ 131, 131, 131, 131, 131, 131, 131, 14, 14, 14, 
  /*  2400 */ 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
  /*  2410 */ 14, 14, 14, NACL_OPCODE_NULL_OFFSET, 131, 131, 131, 131, 131, 131, 
  /*  2420 */ 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 
  /*  2430 */ 14, 14, 14, 14, 14, 14, 14, 14, 14, 131, 
  /*  2440 */ 14, 14, 14, 14, 14, 14, 131, 131, 131, 131, 
  /*  2450 */ 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 
  /*  2460 */ 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 
  /*  2470 */ 131, 131, 131, 131, 131, 131, 131, 131, NACL_OPCODE_NULL_OFFSET, 580, 
  /*  2480 */ 580, 580, 580, 580, 580, 580, 580, 580, 580, 580, 
  /*  2490 */ 580, 580, 580, 580, 580, 580, 580, 580, 580, 580, 
  /*  2500 */ 580, 580, 580, 580, 580, 580, 580, 580, 580, 580, 
  /*  2510 */ 580, 581, 14, 14, 14, 14, 14, 14, 14, 131, 
  /*  2520 */ 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 
  /*  2530 */ 131, 131, 131, 131, 131, 14, 14, 14, 14, 14, 
  /*  2540 */ 14, 14, 14, };

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
    285,
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
    285,
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
    285,
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
    285,
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
    285,
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
    285,
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
    285,
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
    285,
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
    285,
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
    285,
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
    285,
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
    285,
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
    285,
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
    285,
    NULL,
    NULL,
  },
  /* 91 */
  { 0x90,
    285,
    NULL,
    NULL,
  },
  /* 92 */
  { 0x90,
    285,
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
    285,
    NULL,
    NULL,
  },
};
