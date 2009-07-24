 /*
  * Copyright 2009, Google Inc.
  * All rights reserved.
  *
  * Redistribution and use in source and binary forms, with or without
  * modification, are permitted provided that the following conditions are
  * met:
  *
  *     * Redistributions of source code must retain the above copyright
  * notice, this list of conditions and the following disclaimer.
  *     * Redistributions in binary form must reproduce the above
  * copyright notice, this list of conditions and the following disclaimer
  * in the documentation and/or other materials provided with the
  * distribution.
  *     * Neither the name of Google Inc. nor the names of its
  * contributors may be used to endorse or promote products derived from
  * this software without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  */

%{
#include <stdio.h>
#define YYSTYPE char *
#include "GLSL_ES.tab.h"
%}

EXPONENT_PART           [eE][+-]?[0-9]+

DECIMAL_CONSTANT        [1-9][0-9]*
HEXADECIMAL_CONSTANT    0[xX][0-9a-fA-F]+
OCTAL_CONSTANT          0[0-7]*

%s IN_COMMENT
%%

 /**********************************************************************/
 /* Keywords */
 /**/

attribute                      return ATTRIBUTE;
bool                           return BOOL;
break                          return BREAK;
bvec2                          return BVEC2;
bvec3                          return BVEC3;
bvec4                          return BVEC4;
const                          return CONST;
continue                       return CONTINUE;
discard                        return DISCARD;
do                             return DO;
else                           return ELSE;
false                          return FALSE;
float                          return FLOAT;
for                            return FOR;
highp                          return HIGH_PRECISION;
if                             return IF;
in                             return IN;
inout                          return INOUT;
int                            return INT;
invariant                      return INVARIANT;
ivec2                          return IVEC2;
ivec3                          return IVEC3;
ivec4                          return IVEC4;
lowp                           return LOW_PRECISION;
mat2                           return MAT2;
mat3                           return MAT3;
mat4                           return MAT4;
mediump                        return MEDIUM_PRECISION;
out                            return OUT;
precision                      return PRECISION;
return                         return RETURN;
sampler2D                      return SAMPLER2D;
samplerCube                    return SAMPLERCUBE;
struct                         return STRUCT;
true                           return TRUE;
uniform                        return UNIFORM;
varying                        return VARYING;
vec2                           return VEC2;
vec3                           return VEC3;
vec4                           return VEC4;
void                           return VOID;
while                          return WHILE;

 /**********************************************************************/
 /* Basic tokens and operators */
 /**/

\/\/.*                           /* ignore single-line comments */;

 /* Ignore C-style multi-line comments */
<INITIAL>{
"/*"              BEGIN(IN_COMMENT);
}
<IN_COMMENT>{
"*/"      BEGIN(INITIAL);
[^*\n]+   // eat comment in chunks
"*"       // eat the lone star
\n        yylineno++;
}

DECIMAL_CONSTANT               |
HEXADECIMAL_CONSTANT           |
OCTAL_CONSTANT                 yylval=strdup(yytext); return INTCONSTANT;

[0-9]+\.[0-9]*{EXPONENT_PART}? |
\.[0-9]+{EXPONENT_PART}?       yylval=strdup(yytext); return FLOATCONSTANT;

\n                             /* ignore EOL */;
[ \t\r\f]+                     /* ignore whitespace */;

\+\+                           return INC_OP;
\-\-                           return DEC_OP;
\<=                            return LE_OP;
\>=                            return GE_OP;
==                             return EQ_OP;
!=                             return NE_OP;

&&                             return AND_OP;
\|\|                           return OR_OP;
\^\^                           return XOR_OP;
\*=                            return MUL_ASSIGN;
\/=                            return DIV_ASSIGN;
\+=                            return ADD_ASSIGN;
\%=                            return MOD_ASSIGN;
-=                             return SUB_ASSIGN;

\(                             return LEFT_PAREN;
\)                             return RIGHT_PAREN;
\[                             return LEFT_BRACKET;
\]                             return RIGHT_BRACKET;
\{                             return LEFT_BRACE;
\}                             return RIGHT_BRACE;
\.                             return DOT;

,                              return COMMA;
:                              return COLON;
;                              return SEMICOLON;
=                              return EQUAL;
!                              return BANG;
\-                             return DASH;
~                              return TILDE;
\+                             return PLUS;
\*                             return STAR;
\/                             return SLASH;
\%                             return PERCENT;

\<                             return LEFT_ANGLE;
\>                             return RIGHT_ANGLE;
\|                             return VERTICAL_BAR;
\^                             return CARET;
&                              return AMPERSAND;
\?                             return QUESTION;

 /* Catch-all for user identifiers */
[a-zA-Z_][a-zA-Z0-9_]*         yylval=strdup(yytext); return IDENTIFIER;

 /**********************************************************************/
 /* Keywords reserved for future use */
 /**/

 /**************************/
 /* asm                  | */
 /* cast                 | */
 /* class                | */
 /* default              | */
 /* double               | */
 /* dvec2                | */
 /* dvec3                | */
 /* dvec4                | */
 /* enum                 | */
 /* extern               | */
 /* external             | */
 /* fixed                | */
 /* flat                 | */
 /* fvec2                | */
 /* fvec3                | */
 /* fvec4                | */
 /* goto                 | */
 /* half                 | */
 /* hvec2                | */
 /* hvec3                | */
 /* hvec4                | */
 /* inline               | */
 /* input                | */
 /* interface            | */
 /* long                 | */
 /* namespace            | */
 /* noinline             | */
 /* output               | */
 /* packed               | */
 /* public               | */
 /* sampler1D            | */
 /* sampler1DShadow      | */
 /* sampler2DRect        | */
 /* sampler2DRectShadow  | */
 /* sampler2DShadow      | */
 /* sampler3D            | */
 /* sampler3DRect        | */
 /* short                | */
 /* sizeof               | */
 /* static               | */
 /* superp               | */
 /* switch               | */
 /* template             | */
 /* this                 | */
 /* typedef              | */
 /* union                | */
 /* unsigned             | */
 /* using                | */
 /* volatile             | */
%%
