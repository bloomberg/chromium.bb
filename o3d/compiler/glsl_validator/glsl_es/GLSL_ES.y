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
#include <string.h>

#define YYSTYPE char *

int yydebug=0;

void yyerror(const char *str)
{
	fprintf(stderr,"error: %s\n",str);
}

int yywrap()
{
	return 1;
}

main()
{
	yyparse();
}

%}

%token
  INTCONSTANT FLOATCONSTANT IDENTIFIER

  INC_OP DEC_OP LE_OP GE_OP EQ_OP NE_OP AND_OP OR_OP XOR_OP

  MUL_ASSIGN DIV_ASSIGN ADD_ASSIGN MOD_ASSIGN SUB_ASSIGN

  LEFT_PAREN RIGHT_PAREN LEFT_BRACKET RIGHT_BRACKET LEFT_BRACE RIGHT_BRACE DOT 

  COMMA COLON SEMICOLON EQUAL BANG DASH TILDE PLUS STAR SLASH PERCENT 

  LEFT_ANGLE RIGHT_ANGLE VERTICAL_BAR CARET AMPERSAND QUESTION 

  ATTRIBUTE BOOL BREAK BVEC2 BVEC3
  BVEC4 CONST CONTINUE DISCARD DO
  ELSE FALSE FLOAT FOR HIGH_PRECISION
  IF IN INOUT INT INVARIANT
  IVEC2 IVEC3 IVEC4 LOW_PRECISION MAT2
  MAT3 MAT4 MEDIUM_PRECISION OUT PRECISION
  RETURN SAMPLER2D SAMPLERCUBE STRUCT TRUE
  UNIFORM VARYING VEC2 VEC3 VEC4
  VOID WHILE

%%

/* Main entry point */
translation_unit
  : external_declaration
  | translation_unit external_declaration

/* FIXME: this requires much more work */
FIELD_SELECTION
  : IDENTIFIER
  ;

variable_identifier
  : IDENTIFIER
  ;

primary_expression
  : variable_identifier
  | INTCONSTANT
  | FLOATCONSTANT
  | TRUE | FALSE
  | primary_expression_1
  ;

primary_expression_1
  : LEFT_PAREN expression RIGHT_PAREN
  ;

postfix_expression
  : postfix_expression_1
  | postfix_expression postfix_expression_2
  ;

postfix_expression_1
  : primary_expression
  | function_call
  ;

postfix_expression_2
  : LEFT_BRACKET integer_expression RIGHT_BRACKET
  | DOT FIELD_SELECTION
  | INC_OP
  | DEC_OP
  ;

integer_expression
  : expression
  ;

function_call
  : function_call_generic
  ;

function_call_generic
  : function_call_header_with_parameters RIGHT_PAREN
  | function_call_header_no_parameters RIGHT_PAREN
  ;

function_call_header_no_parameters
  : function_call_header VOID
  | function_call_header
  ;

function_call_header_with_parameters
  : function_call_header assignment_expression
  | function_call_header_with_parameters function_call_header_with_parameters_1
  ;

function_call_header_with_parameters_1
  : COMMA assignment_expression
  ;

function_call_header
  : function_identifier LEFT_PAREN
  ;

function_identifier
  : constructor_identifier
  | IDENTIFIER
  ;

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
  : postfix_expression
  | INC_OP unary_expression
  | DEC_OP unary_expression
  | unary_operator unary_expression
  ;

/* Grammar Note:  No traditional style type casts. */

unary_operator
  : PLUS
  | DASH
  | BANG
/*| TILDE                                                        // reserved */
  ;

/* Grammar Note:  No '*' or '&' unary ops.  Pointers are not supported. */

multiplicative_expression
  : unary_expression
  | multiplicative_expression multiplicative_expression_1
  ;

multiplicative_expression_1
  : STAR unary_expression
  | SLASH unary_expression
/*| PERCENT unary_expression */
  ;

additive_expression
  : multiplicative_expression
  | additive_expression additive_expression_1
  ;

additive_expression_1
  : PLUS multiplicative_expression
  | DASH multiplicative_expression
  ;

shift_expression
  : additive_expression
/*| shift_expression LEFT_OP additive_expression                 // reserved */
/*| shift_expression RIGHT_OP additive_expression                // reserved */
  ;

relational_expression
  : shift_expression
  | relational_expression relational_expression_1
  ;

relational_expression_1
  : LEFT_ANGLE shift_expression
  | RIGHT_ANGLE shift_expression
  | LE_OP shift_expression
  | GE_OP shift_expression
  ;

equality_expression
  : relational_expression
  | equality_expression equality_expression_1
  ;

equality_expression_1
  : EQ_OP relational_expression
  | NE_OP relational_expression
  ;

and_expression
  : equality_expression
/*| and_expression AMPERSAND equality_expression                 // reserved */
  ;

exclusive_or_expression
  : and_expression
/*| exclusive_or_expression CARET and_expression                 // reserved */
  ;

inclusive_or_expression
  : exclusive_or_expression
/*| inclusive_or_expression VERTICAL_BAR exclusive_or_expression // reserved */
  ;

logical_and_expression
  : inclusive_or_expression
  | logical_and_expression logical_and_expression_1
  ;

logical_and_expression_1
  : AND_OP inclusive_or_expression
  ;

logical_xor_expression
  : logical_and_expression
  | logical_xor_expression logical_xor_expression_1
  ;

logical_xor_expression_1
  : XOR_OP logical_and_expression
  ;

logical_or_expression
  : logical_xor_expression
  | logical_or_expression logical_or_expression_1
  ;

logical_or_expression_1
  : OR_OP logical_xor_expression
  ;

conditional_expression
  : logical_or_expression 
  | logical_or_expression conditional_expression_1
  ;

/* NOTE (FIXME): difference between Mesa's grammar and GLSL ES spec;
   Mesa uses conditional_expression after the colon, GLSL ES uses assignment_expression */
conditional_expression_1
  : QUESTION expression COLON assignment_expression
  ;

assignment_expression
  : conditional_expression 
  | unary_expression assignment_operator assignment_expression
  ;

assignment_operator
  : EQUAL
  | MUL_ASSIGN
  | DIV_ASSIGN
/*| MOD_ASSIGN   // reserved */
  | ADD_ASSIGN
  | SUB_ASSIGN
/*| LEFT_ASSIGN  // reserved */
/*| RIGHT_ASSIGN // reserved */
/*| AND_ASSIGN   // reserved */
/*| XOR_ASSIGN   // reserved */
/*| OR_ASSIGN    // reserved */
  ;

expression
  : assignment_expression
  | expression expression_1
  ;

expression_1
  : COMMA assignment_expression
  ;

constant_expression
  : conditional_expression
  ;

declaration
  : function_prototype SEMICOLON
  | init_declarator_list SEMICOLON
  | PRECISION precision_qualifier type_specifier_no_prec SEMICOLON
  ;

function_prototype
  : function_declarator RIGHT_PAREN
  ;

function_declarator
  : function_header
  | function_header_with_parameters
  ;

function_header_with_parameters
  : function_header parameter_declaration
  | function_header_with_parameters function_header_with_parameters_1
  ;

function_header_with_parameters_1
  : COMMA parameter_declaration
  ;

/* NOTE: Mesa grammar differs substantially in handling of types ("space" vs. "non-space") */
function_header
  : fully_specified_type IDENTIFIER LEFT_PAREN
  ;

parameter_declarator
  : type_specifier IDENTIFIER
  | type_specifier IDENTIFIER LEFT_BRACKET constant_expression RIGHT_BRACKET
  ;

/* NOTE: difference between both Mesa and ANTLR grammars */
parameter_declaration
  : type_qualifier parameter_qualifier parameter_declarator
  | parameter_qualifier parameter_declarator
  | type_qualifier parameter_qualifier parameter_type_specifier
  | parameter_qualifier parameter_type_specifier
  ;

/* NOTE empty arm at beginning */
parameter_qualifier
  :
  | IN
  | OUT
  | INOUT
  ;

parameter_type_specifier
  : type_specifier
  | type_specifier LEFT_BRACKET constant_expression RIGHT_BRACKET
  ;

init_declarator_list
  : single_declaration
  | single_declaration init_declarator_list_1
  ;

init_declarator_list_1
  : COMMA IDENTIFIER init_declarator_list_2
  ;

init_declarator_list_2
  : init_declarator_list_3
  | init_declarator_list_4
  | /* empty */
  ;

init_declarator_list_3
  : LEFT_BRACKET constant_expression RIGHT_BRACKET
  ;

init_declarator_list_4
  : EQUAL initializer
  ;

/* NOTE: significant differences between this formulation and Mesa's */
single_declaration
  : fully_specified_type
  | fully_specified_type IDENTIFIER
  | fully_specified_type IDENTIFIER LEFT_BRACKET constant_expression RIGHT_BRACKET
  | fully_specified_type IDENTIFIER EQUAL initializer
  | INVARIANT IDENTIFIER   // Vertex only.
  ;

/* Grammar Note:  No 'enum', or 'typedef'. */

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
  : STRUCT IDENTIFIER LEFT_BRACE struct_declaration_list RIGHT_BRACE
  | STRUCT LEFT_BRACE struct_declaration_list RIGHT_BRACE
  ;

struct_declaration_list
  : struct_declaration
  | struct_declaration struct_declaration_list_1
  ;

struct_declaration_list_1
  : struct_declaration struct_declaration_list
  ;

struct_declaration
  : type_specifier struct_declarator_list SEMICOLON
  ;

/* NOTE difference with spec grammar in where recursion occurs */
struct_declarator_list
  : struct_declarator
  | struct_declarator struct_declarator_list_1
  ;

/* NOTE difference with Mesa grammar */
struct_declarator_list_1
  : COMMA struct_declarator_list
  ;

struct_declarator
  : IDENTIFIER
  | IDENTIFIER LEFT_BRACKET constant_expression RIGHT_BRACKET
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
  : LEFT_BRACE RIGHT_BRACE
  | LEFT_BRACE statement_list RIGHT_BRACE
  ;

statement_with_scope
  : compound_statement_no_new_scope
  | simple_statement
  ;

compound_statement_no_new_scope
  : LEFT_BRACE RIGHT_BRACE
  | LEFT_BRACE statement_list RIGHT_BRACE
  ;

/* FIXME: may need refactoring */
statement_list
  : statement_no_new_scope
  | statement_list statement_no_new_scope
  ;

expression_statement
  : SEMICOLON
  | expression SEMICOLON
  ;

selection_statement
  : IF LEFT_PAREN expression RIGHT_PAREN selection_rest_statement
  ;

selection_rest_statement
  : statement_with_scope ELSE statement_with_scope
  | statement_with_scope
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

conditionopt
  : condition
  | /* empty */
  ;

for_rest_statement
  : conditionopt SEMICOLON
  | conditionopt SEMICOLON expression
  ;

jump_statement
  : CONTINUE SEMICOLON
  | BREAK SEMICOLON
  | RETURN SEMICOLON
  | RETURN expression SEMICOLON
  | DISCARD SEMICOLON   // Fragment shader only.
  ;

external_declaration
  : function_definition
  | declaration
  ;

function_definition
  : function_prototype compound_statement_no_new_scope
  ;
