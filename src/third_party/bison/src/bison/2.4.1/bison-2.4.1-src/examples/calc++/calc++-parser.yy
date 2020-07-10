#line 8420 "../../doc/bison.texinfo"
%skeleton "lalr1.cc"                          /*  -*- C++ -*- */
%require "2.4.1"
%defines
%define parser_class_name "calcxx_parser"
#line 8438 "../../doc/bison.texinfo"
%code requires {
# include <string>
class calcxx_driver;
}
#line 8451 "../../doc/bison.texinfo"
// The parsing context.
%parse-param { calcxx_driver& driver }
%lex-param   { calcxx_driver& driver }
#line 8464 "../../doc/bison.texinfo"
%locations
%initial-action
{
  // Initialize the initial location.
  @$.begin.filename = @$.end.filename = &driver.file;
};
#line 8478 "../../doc/bison.texinfo"
%debug
%error-verbose
#line 8488 "../../doc/bison.texinfo"
// Symbols.
%union
{
  int          ival;
  std::string *sval;
};
#line 8503 "../../doc/bison.texinfo"
%code {
# include "calc++-driver.hh"
}
#line 8518 "../../doc/bison.texinfo"
%token        END      0 "end of file"
%token        ASSIGN     ":="
%token <sval> IDENTIFIER "identifier"
%token <ival> NUMBER     "number"
%type  <ival> exp
#line 8532 "../../doc/bison.texinfo"
%printer    { debug_stream () << *$$; } "identifier"
%destructor { delete $$; } "identifier"

%printer    { debug_stream () << $$; } <ival>
#line 8543 "../../doc/bison.texinfo"
%%
%start unit;
unit: assignments exp  { driver.result = $2; };

assignments: assignments assignment {}
           | /* Nothing.  */        {};

assignment:
     "identifier" ":=" exp
       { driver.variables[*$1] = $3; delete $1; };

%left '+' '-';
%left '*' '/';
exp: exp '+' exp   { $$ = $1 + $3; }
   | exp '-' exp   { $$ = $1 - $3; }
   | exp '*' exp   { $$ = $1 * $3; }
   | exp '/' exp   { $$ = $1 / $3; }
   | "identifier"  { $$ = driver.variables[*$1]; delete $1; }
   | "number"      { $$ = $1; };
%%
#line 8571 "../../doc/bison.texinfo"
void
yy::calcxx_parser::error (const yy::calcxx_parser::location_type& l,
                          const std::string& m)
{
  driver.error (l, m);
}
