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

// This file contains the ANTLR grammar for parsing GLSL ES into an Abstract
// Syntax Tree (AST).

grammar GLSL_ES;

options {
    language = Java;
 }

@lexer::header  { package glsl_es; }
@parser::header { package glsl_es; }

/* Main entry point */
translation_unit
  : ( external_declaration )* EOF
  ;

variable_identifier
  : IDENTIFIER
  ;

primary_expression
  : INTCONSTANT
  | FLOATCONSTANT
  | BOOLCONSTANT
  | variable_identifier
  | LEFT_PAREN expression RIGHT_PAREN
  ;

postfix_expression
  : primary_expression_or_function_call
    ( LEFT_BRACKET integer_expression RIGHT_BRACKET
      | DOT field_selection
      | INC_OP
      | DEC_OP
    )*
  ;

primary_expression_or_function_call
  : ( INTCONSTANT ) => primary_expression
  | ( FLOATCONSTANT ) => primary_expression
  | ( BOOLCONSTANT ) => primary_expression
  | ( LEFT_PAREN ) => primary_expression
  | ( function_call_header ) => function_call
  | primary_expression
  ;

integer_expression
  : expression
  ;

function_call
  : function_call_generic
  ;

function_call_generic
  : function_call_header
    (
        (VOID)?
      | assignment_expression (COMMA assignment_expression)*
    )
    RIGHT_PAREN
  ;

function_call_header
  : function_identifier LEFT_PAREN
  ;

function_identifier
  : constructor_identifier
  | IDENTIFIER
  ;

// Grammar Note: Constructors look like functions, but lexical analysis recognized most of them as 
// keywords.
//
// FIXME: do we need to register declared struct types in a dictionary
// and look them up in order to be able to handle the TYPE_NAME
// constructor identifier type?

constructor_identifier
  : FLOAT
  | INT
  | BOOL
  | VEC2
  | VEC3
  | VEC4
  | BVEC2
  | BVEC3
  | BVEC4
  | IVEC2
  | IVEC3
  | IVEC4
  | MAT2
  | MAT3
  | MAT4
//  | TYPE_NAME
  ;

unary_expression
  : (INC_OP | DEC_OP | unary_operator)* postfix_expression
  ;

// Grammar Note:  No traditional style type casts. 

unary_operator
  : PLUS
  | DASH
  | BANG
//| TILDE   // reserved
  ;

// Grammar Note:  No '*' or '&' unary ops.  Pointers are not supported. 

multiplicative_expression
  : unary_expression ((STAR | SLASH) unary_expression)*
//| multiplicative_expression PERCENT unary_expression   // reserved
  ;

additive_expression
  : multiplicative_expression ((PLUS | DASH) multiplicative_expression)*
  ;

shift_expression
  : additive_expression
//| shift_expression LEFT_OP additive_expression         // reserved
//| shift_expression RIGHT_OP additive_expression        // reserved
  ;

relational_expression
  : shift_expression ((LEFT_ANGLE | RIGHT_ANGLE | LE_OP | GE_OP) shift_expression)*
  ;

equality_expression
  : relational_expression ((EQ_OP | NE_OP) relational_expression)*
  ;

and_expression
  : equality_expression
//| and_expression AMPERSAND equality_expression         // reserved
  ;

exclusive_or_expression
  : and_expression
//| exclusive_or_expression CARET and_expression         // reserved
  ;

inclusive_or_expression
  : exclusive_or_expression
//| inclusive_or_expression VERTICAL_BAR exclusive_or_expression  // reserved
  ;

logical_and_expression
  : inclusive_or_expression (AND_OP inclusive_or_expression)*
  ;

logical_xor_expression
  : logical_and_expression (XOR_OP logical_and_expression)*
  ;

logical_or_expression
  : logical_xor_expression (OR_OP logical_xor_expression)*
  ;

conditional_expression
  : logical_or_expression (QUESTION expression COLON assignment_expression)?
  ;

assignment_expression
  : (unary_expression assignment_operator) => unary_expression assignment_operator assignment_expression
  | conditional_expression
  ;

assignment_operator
  : EQUAL
  | MUL_ASSIGN
  | DIV_ASSIGN
//| MOD_ASSIGN   // reserved
  | ADD_ASSIGN
  | SUB_ASSIGN
//| LEFT_ASSIGN  // reserved
//| RIGHT_ASSIGN // reserved
//| AND_ASSIGN   // reserved
//| XOR_ASSIGN   // reserved
//| OR_ASSIGN    // reserved
  ;

expression
  : assignment_expression (COMMA assignment_expression)*
  ;

constant_expression
  : conditional_expression
  ;

declaration
  : (function_header) => function_prototype SEMICOLON
  | init_declarator_list SEMICOLON
  | PRECISION precision_qualifier type_specifier_no_prec SEMICOLON
  ;

function_prototype
  : function_declarator RIGHT_PAREN
  ;

function_declarator
  : function_header (parameter_declaration (COMMA parameter_declaration)* )?
  ;

function_header
  : fully_specified_type IDENTIFIER LEFT_PAREN
  ;

parameter_declaration
  : (type_qualifier (parameter_qualifier)? )?
    ( type_specifier
      // parameter_declarator
      (IDENTIFIER)?
      // parameter_type_specifier
      (LEFT_BRACKET constant_expression RIGHT_BRACKET)?
    )
  ;

// NOTE: this originally had "empty" as one of the arms in the grammar

parameter_qualifier
  : IN
  | OUT
  | INOUT
  ;

init_declarator_list
  : single_declaration (init_declarator_list_1)*
  ;

init_declarator_list_1
  : COMMA IDENTIFIER (init_declarator_list_2)?
  ;

init_declarator_list_2
  : LEFT_BRACKET constant_expression RIGHT_BRACKET
  | EQUAL initializer
  ;

single_declaration
  : fully_specified_type
    ( IDENTIFIER
      (   LEFT_BRACKET constant_expression RIGHT_BRACKET
        | EQUAL initializer
      ) ?
    ) ?
  | INVARIANT IDENTIFIER   // Vertex only.
  ;

// Grammar Note:  No 'enum', or 'typedef'. 

fully_specified_type
  : type_specifier
  | type_qualifier type_specifier
  ;

type_qualifier
  : CONST
  | ATTRIBUTE   // Vertex only.
  | VARYING
  | INVARIANT VARYING
  | UNIFORM
  ;

type_specifier
  : type_specifier_no_prec
  | precision_qualifier type_specifier_no_prec
  ;

type_specifier_no_prec
  : VOID
  | FLOAT
  | INT
  | BOOL
  | VEC2
  | VEC3
  | VEC4
  | BVEC2
  | BVEC3
  | BVEC4
  | IVEC2
  | IVEC3
  | IVEC4
  | MAT2
  | MAT3
  | MAT4
  | SAMPLER2D
  | SAMPLERCUBE
  | struct_specifier
//  | TYPE_NAME
  ;

precision_qualifier
  : HIGH_PRECISION
  | MEDIUM_PRECISION
  | LOW_PRECISION
  ;

struct_specifier
  : STRUCT (IDENTIFIER)? LEFT_BRACE struct_declaration_list RIGHT_BRACE
  ;

struct_declaration_list
  : (struct_declaration)+
  ;

struct_declaration
  : type_specifier struct_declarator_list SEMICOLON
  ;

struct_declarator_list
  : struct_declarator (COMMA struct_declarator)*
  ;

struct_declarator
  : IDENTIFIER (LEFT_BRACKET constant_expression RIGHT_BRACKET)?
  ;

initializer
  : assignment_expression
  ;

declaration_statement
  : declaration
  ;

statement_no_new_scope
  : compound_statement_with_scope
  | simple_statement
  ;

simple_statement
  : declaration_statement
  | expression_statement
  | selection_statement
  | iteration_statement
  | jump_statement
  ;

compound_statement_with_scope
  : LEFT_BRACE (statement_list)? RIGHT_BRACE
  ;

statement_with_scope
  : compound_statement_no_new_scope
  | simple_statement
  ;

compound_statement_no_new_scope
  : LEFT_BRACE (statement_list)? RIGHT_BRACE
  ;

statement_list
  : (statement_no_new_scope)+
  ;

expression_statement
  : (expression)? SEMICOLON
  ;

selection_statement
options { backtrack=true; }
  : IF LEFT_PAREN expression RIGHT_PAREN statement_with_scope ELSE statement_with_scope
  | IF LEFT_PAREN expression RIGHT_PAREN statement_with_scope
  ;

condition
  : expression
  | fully_specified_type IDENTIFIER EQUAL initializer
  ;

iteration_statement
  : WHILE LEFT_PAREN condition RIGHT_PAREN statement_no_new_scope
  | DO statement_with_scope WHILE LEFT_PAREN expression RIGHT_PAREN SEMICOLON
  | FOR LEFT_PAREN for_init_statement for_rest_statement RIGHT_PAREN statement_no_new_scope
  ;

for_init_statement
  : expression_statement
  | declaration_statement
  ;

for_rest_statement
  : (condition)? SEMICOLON (expression)?
  ;

jump_statement
  : CONTINUE SEMICOLON
  | BREAK SEMICOLON
  | RETURN (expression)? SEMICOLON
  | DISCARD SEMICOLON   // Fragment shader only.
  ;

external_declaration
  : (function_header) => function_definition
  | declaration
  ;

function_definition
  : function_prototype compound_statement_no_new_scope
  ;

// ----------------------------------------------------------------------
// Keywords

ATTRIBUTE        : 'attribute';
BOOL             : 'bool';
BREAK            : 'break';
BVEC2            : 'bvec2';
BVEC3            : 'bvec3';
BVEC4            : 'bvec4';
CONST            : 'const';
CONTINUE         : 'continue';
DISCARD          : 'discard';
DO               : 'do';
ELSE             : 'else';
FALSE            : 'false';
FLOAT            : 'float';
FOR              : 'for';
HIGH_PRECISION   : 'highp';
IF               : 'if';
IN               : 'in';
INOUT            : 'inout';
INT              : 'int';
INVARIANT        : 'invariant';
IVEC2            : 'ivec2';
IVEC3            : 'ivec3';
IVEC4            : 'ivec4';
LOW_PRECISION    : 'lowp';
MAT2             : 'mat2';
MAT3             : 'mat3';
MAT4             : 'mat4';
MEDIUM_PRECISION : 'mediump';
OUT              : 'out';
PRECISION        : 'precision';
RETURN           : 'return';
SAMPLER2D        : 'sampler2D';
SAMPLERCUBE      : 'samplerCube';
STRUCT           : 'struct'; 
TRUE             : 'true';
UNIFORM          : 'uniform';
VARYING          : 'varying';
VEC2             : 'vec2';
VEC3             : 'vec3';
VEC4             : 'vec4';
VOID             : 'void';
WHILE            : 'while';

IDENTIFIER
  : ('a'..'z'|'A'..'Z'|'_')('a'..'z'|'A'..'Z'|'_'|'0'..'9')*
  ;

/*
// FIXME: it isn't clear whether we need to support the TYPE_NAME
// token type; that may only be needed if typedef is supported
TYPE_NAME
  : IDENTIFIER
  ;
*/

// NOTE difference in handling of leading minus sign compared to HLSL
// grammar

fragment EXPONENT_PART : ('e'|'E') (PLUS | DASH)? ('0'..'9')+ ;

FLOATCONSTANT
  : ('0'..'9')+ '.' ('0'..'9')* (EXPONENT_PART)?
  | '.' ('0'..'9')+ (EXPONENT_PART)?
  ;

fragment DECIMAL_CONSTANT
  : ('1'..'9')('0'..'9')*
  ;

fragment OCTAL_CONSTANT
  : '0' ('0'..'7')*
  ;

fragment HEXADECIMAL_CONSTANT
  : '0' ('x'|'X') HEXDIGIT+
  ;

fragment HEXDIGIT
  : ('0'..'9'|'a'..'f'|'A'..'F')
  ;

INTCONSTANT
  : DECIMAL_CONSTANT
  | OCTAL_CONSTANT
  | HEXADECIMAL_CONSTANT
  ;

fragment BOOLCONSTANT
  : TRUE
  | FALSE
  ;

// FIXME: this needs much more work
field_selection
  : IDENTIFIER
  ;

//LEFT_OP  : '<<';      - reserved
//RIGHT_OP : '>>';      - reserved

INC_OP           : '++';
DEC_OP           : '--';
LE_OP            : '<=';
GE_OP            : '>=';
EQ_OP            : '==';
NE_OP            : '!=';

AND_OP           : '&&';
OR_OP            : '||';
XOR_OP           : '^^';
MUL_ASSIGN       : '*=';
DIV_ASSIGN       : '/=';
ADD_ASSIGN       : '+=';
MOD_ASSIGN       : '%=';
// LEFT_ASSIGN   : '<<=';  - reserved
// RIGHT_ASSIGN  : '>>=';  - reserved
// AND_ASSIGN    : '&=';   - reserved
// XOR_ASSIGN    : '^=';   - reserved
// OR_ASSIGN     : '|=';   - reserved
SUB_ASSIGN       : '-=';

LEFT_PAREN       : '(';
RIGHT_PAREN      : ')';
LEFT_BRACKET     : '[';
RIGHT_BRACKET    : ']';
LEFT_BRACE       : '{';
RIGHT_BRACE      : '}';
DOT              : '.';

COMMA            : ',';
COLON            : ':';
EQUAL            : '=';
SEMICOLON        : ';';
BANG             : '!';
DASH             : '-';
TILDE            : '~';
PLUS             : '+';
STAR             : '*';
SLASH            : '/';
PERCENT          : '%';

LEFT_ANGLE       : '<';
RIGHT_ANGLE      : '>';
VERTICAL_BAR     : '|';
CARET            : '^';
AMPERSAND        : '&';
QUESTION         : '?';

// ----------------------------------------------------------------------
// skipped elements

WHITESPACE
  : ( ' ' | '\t' | '\f' | '\r' | '\n' )
  { $channel = HIDDEN; }
  ;

COMMENT
  : '//' (~('\n'|'\r'))*
  { $channel = HIDDEN; }
  ;

MULTILINE_COMMENT
  : '/*' ( options {greedy=false;} : . )* '*/'
  { $channel = HIDDEN; }
  ;

// ----------------------------------------------------------------------
// Keywords reserved for future use

//RESERVED_KEYWORDS
//  : 'asm'
//  | 'cast'
//  | 'class'
//  | 'default'
//  | 'double'
//  | 'dvec2'
//  | 'dvec3'
//  | 'dvec4'
//  | 'enum'
//  | 'extern'
//  | 'external'
//  | 'fixed'
//  | 'flat'
//  | 'fvec2'
//  | 'fvec3'
//  | 'fvec4'
//  | 'goto'
//  | 'half'
//  | 'hvec2'
//  | 'hvec3'
//  | 'hvec4'
//  | 'inline'
//  | 'input'
//  | 'interface'
//  | 'long'
//  | 'namespace'
//  | 'noinline'
//  | 'output'
//  | 'packed'
//  | 'public'
//  | 'sampler1D'
//  | 'sampler1DShadow'
//  | 'sampler2DRect'
//  | 'sampler2DRectShadow'
//  | 'sampler2DShadow'
//  | 'sampler3D'
//  | 'sampler3DRect'
//  | 'short'
//  | 'sizeof'
//  | 'static'
//  | 'superp'
//  | 'switch'
//  | 'template'
//  | 'this'
//  | 'typedef'
//  | 'union'
//  | 'unsigned'
//  | 'using'
//  | 'volatile'
//  ; 
