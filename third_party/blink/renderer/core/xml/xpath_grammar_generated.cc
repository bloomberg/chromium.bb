// clang-format off
// A Bison parser, made by GNU Bison 3.4.2.

// Skeleton implementation for Bison LALR(1) parsers in C++

// Copyright (C) 2002-2015, 2018-2019 Free Software Foundation, Inc.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

// As a special exception, you may create a larger work that contains
// part or all of the Bison parser skeleton and distribute that work
// under terms of your choice, so long as that work isn't itself a
// parser generator using the skeleton or a modified version thereof
// as a parser skeleton.  Alternatively, if you modify or redistribute
// the parser skeleton itself, you may (at your option) remove this
// special exception, which will cause the skeleton and the resulting
// Bison output files to be licensed under the GNU General Public
// License without this special exception.

// This special exception was added by the Free Software Foundation in
// version 2.2 of Bison.

// Undocumented macros, especially those whose name start with YY_,
// are private implementation details.  Do not rely on them.



// First part of user prologue.
#line 52 "third_party/blink/renderer/core/xml/xpath_grammar.y"


#include "third_party/blink/renderer/core/xml/xpath_functions.h"
#include "third_party/blink/renderer/core/xml/xpath_ns_resolver.h"
#include "third_party/blink/renderer/core/xml/xpath_parser.h"
#include "third_party/blink/renderer/core/xml/xpath_path.h"
#include "third_party/blink/renderer/core/xml/xpath_predicate.h"
#include "third_party/blink/renderer/core/xml/xpath_step.h"
#include "third_party/blink/renderer/core/xml/xpath_variable_reference.h"

#define YYENABLE_NLS 0
#define YY_EXCEPTIONS 0
#define YYDEBUG 0

using blink::xpath::Step;

#line 57 "third_party/blink/renderer/core/xml/xpath_grammar_generated.cc"


#include "xpath_grammar_generated.h"


// Unqualified %code blocks.
#line 113 "third_party/blink/renderer/core/xml/xpath_grammar.y"


static int yylex(xpathyy::YyParser::semantic_type* yylval) {
  return blink::xpath::Parser::Current()->Lex(yylval);
}

namespace xpathyy {
void YyParser::error(const std::string&) { }
}


#line 76 "third_party/blink/renderer/core/xml/xpath_grammar_generated.cc"


#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> // FIXME: INFRINGES ON USER NAME SPACE.
#   define YY_(msgid) dgettext ("bison-runtime", msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(msgid) msgid
# endif
#endif

// Whether we are compiled with exception support.
#ifndef YY_EXCEPTIONS
# if defined __GNUC__ && !defined __EXCEPTIONS
#  define YY_EXCEPTIONS 0
# else
#  define YY_EXCEPTIONS 1
# endif
#endif



// Enable debugging if requested.
#if YYDEBUG

// A pseudo ostream that takes yydebug_ into account.
# define YYCDEBUG if (yydebug_) (*yycdebug_)

# define YY_SYMBOL_PRINT(Title, Symbol)         \
  do {                                          \
    if (yydebug_)                               \
    {                                           \
      *yycdebug_ << Title << ' ';               \
      yy_print_ (*yycdebug_, Symbol);           \
      *yycdebug_ << '\n';                       \
    }                                           \
  } while (false)

# define YY_REDUCE_PRINT(Rule)          \
  do {                                  \
    if (yydebug_)                       \
      yy_reduce_print_ (Rule);          \
  } while (false)

# define YY_STACK_PRINT()               \
  do {                                  \
    if (yydebug_)                       \
      yystack_print_ ();                \
  } while (false)

#else // !YYDEBUG

# define YYCDEBUG if (false) std::cerr
# define YY_SYMBOL_PRINT(Title, Symbol)  YYUSE (Symbol)
# define YY_REDUCE_PRINT(Rule)           static_cast<void> (0)
# define YY_STACK_PRINT()                static_cast<void> (0)

#endif // !YYDEBUG

#define yyerrok         (yyerrstatus_ = 0)
#define yyclearin       (yyla.clear ())

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab
#define YYRECOVERING()  (!!yyerrstatus_)

#line 69 "third_party/blink/renderer/core/xml/xpath_grammar.y"
namespace xpathyy {
#line 149 "third_party/blink/renderer/core/xml/xpath_grammar_generated.cc"


  /// Build a parser object.
  YyParser::YyParser (blink::xpath::Parser* parser__yyarg)
    :
#if YYDEBUG
      yydebug_ (false),
      yycdebug_ (&std::cerr),
#endif
      parser_ (parser__yyarg)
  {}

  YyParser::~YyParser ()
  {}

  YyParser::syntax_error::~syntax_error () YY_NOEXCEPT YY_NOTHROW
  {}

  /*---------------.
  | Symbol types.  |
  `---------------*/

  // basic_symbol.
#if 201103L <= YY_CPLUSPLUS
  template <typename Base>
  YyParser::basic_symbol<Base>::basic_symbol (basic_symbol&& that)
    : Base (std::move (that))
    , value ()
  {
    switch (this->type_get ())
    {
      case 11: // kNodeType
      case 12: // kPI
      case 13: // kFunctionName
      case 14: // kLiteral
      case 15: // kVariableReference
      case 16: // kNumber
      case 19: // kNameTest
        value.move< String > (std::move (that.value));
        break;

      case 45: // ArgumentList
        value.move< blink::Persistent<blink::HeapVector<blink::Member<blink::xpath::Expression>>> > (std::move (that.value));
        break;

      case 38: // OptionalPredicateList
      case 39: // PredicateList
        value.move< blink::Persistent<blink::HeapVector<blink::Member<blink::xpath::Predicate>>> > (std::move (that.value));
        break;

      case 31: // Expr
      case 40: // Predicate
      case 43: // PrimaryExpr
      case 44: // FunctionCall
      case 46: // Argument
      case 47: // UnionExpr
      case 48: // PathExpr
      case 49: // FilterExpr
      case 50: // OrExpr
      case 51: // AndExpr
      case 52: // EqualityExpr
      case 53: // RelationalExpr
      case 54: // AdditiveExpr
      case 55: // MultiplicativeExpr
      case 56: // UnaryExpr
        value.move< blink::Persistent<blink::xpath::Expression> > (std::move (that.value));
        break;

      case 32: // LocationPath
      case 33: // AbsoluteLocationPath
      case 34: // RelativeLocationPath
        value.move< blink::Persistent<blink::xpath::LocationPath> > (std::move (that.value));
        break;

      case 37: // NodeTest
        value.move< blink::Persistent<blink::xpath::Step::NodeTest> > (std::move (that.value));
        break;

      case 35: // Step
      case 41: // DescendantOrSelf
      case 42: // AbbreviatedStep
        value.move< blink::Persistent<blink::xpath::Step> > (std::move (that.value));
        break;

      case 4: // kEqOp
      case 5: // kRelOp
        value.move< blink::xpath::EqTestOp::Opcode > (std::move (that.value));
        break;

      case 3: // kMulOp
        value.move< blink::xpath::NumericOp::Opcode > (std::move (that.value));
        break;

      case 10: // kAxisName
      case 36: // AxisSpecifier
        value.move< blink::xpath::Step::Axis > (std::move (that.value));
        break;

      default:
        break;
    }

  }
#endif

  template <typename Base>
  YyParser::basic_symbol<Base>::basic_symbol (const basic_symbol& that)
    : Base (that)
    , value ()
  {
    switch (this->type_get ())
    {
      case 11: // kNodeType
      case 12: // kPI
      case 13: // kFunctionName
      case 14: // kLiteral
      case 15: // kVariableReference
      case 16: // kNumber
      case 19: // kNameTest
        value.copy< String > (YY_MOVE (that.value));
        break;

      case 45: // ArgumentList
        value.copy< blink::Persistent<blink::HeapVector<blink::Member<blink::xpath::Expression>>> > (YY_MOVE (that.value));
        break;

      case 38: // OptionalPredicateList
      case 39: // PredicateList
        value.copy< blink::Persistent<blink::HeapVector<blink::Member<blink::xpath::Predicate>>> > (YY_MOVE (that.value));
        break;

      case 31: // Expr
      case 40: // Predicate
      case 43: // PrimaryExpr
      case 44: // FunctionCall
      case 46: // Argument
      case 47: // UnionExpr
      case 48: // PathExpr
      case 49: // FilterExpr
      case 50: // OrExpr
      case 51: // AndExpr
      case 52: // EqualityExpr
      case 53: // RelationalExpr
      case 54: // AdditiveExpr
      case 55: // MultiplicativeExpr
      case 56: // UnaryExpr
        value.copy< blink::Persistent<blink::xpath::Expression> > (YY_MOVE (that.value));
        break;

      case 32: // LocationPath
      case 33: // AbsoluteLocationPath
      case 34: // RelativeLocationPath
        value.copy< blink::Persistent<blink::xpath::LocationPath> > (YY_MOVE (that.value));
        break;

      case 37: // NodeTest
        value.copy< blink::Persistent<blink::xpath::Step::NodeTest> > (YY_MOVE (that.value));
        break;

      case 35: // Step
      case 41: // DescendantOrSelf
      case 42: // AbbreviatedStep
        value.copy< blink::Persistent<blink::xpath::Step> > (YY_MOVE (that.value));
        break;

      case 4: // kEqOp
      case 5: // kRelOp
        value.copy< blink::xpath::EqTestOp::Opcode > (YY_MOVE (that.value));
        break;

      case 3: // kMulOp
        value.copy< blink::xpath::NumericOp::Opcode > (YY_MOVE (that.value));
        break;

      case 10: // kAxisName
      case 36: // AxisSpecifier
        value.copy< blink::xpath::Step::Axis > (YY_MOVE (that.value));
        break;

      default:
        break;
    }

  }



  template <typename Base>
  bool
  YyParser::basic_symbol<Base>::empty () const YY_NOEXCEPT
  {
    return Base::type_get () == empty_symbol;
  }

  template <typename Base>
  void
  YyParser::basic_symbol<Base>::move (basic_symbol& s)
  {
    super_type::move (s);
    switch (this->type_get ())
    {
      case 11: // kNodeType
      case 12: // kPI
      case 13: // kFunctionName
      case 14: // kLiteral
      case 15: // kVariableReference
      case 16: // kNumber
      case 19: // kNameTest
        value.move< String > (YY_MOVE (s.value));
        break;

      case 45: // ArgumentList
        value.move< blink::Persistent<blink::HeapVector<blink::Member<blink::xpath::Expression>>> > (YY_MOVE (s.value));
        break;

      case 38: // OptionalPredicateList
      case 39: // PredicateList
        value.move< blink::Persistent<blink::HeapVector<blink::Member<blink::xpath::Predicate>>> > (YY_MOVE (s.value));
        break;

      case 31: // Expr
      case 40: // Predicate
      case 43: // PrimaryExpr
      case 44: // FunctionCall
      case 46: // Argument
      case 47: // UnionExpr
      case 48: // PathExpr
      case 49: // FilterExpr
      case 50: // OrExpr
      case 51: // AndExpr
      case 52: // EqualityExpr
      case 53: // RelationalExpr
      case 54: // AdditiveExpr
      case 55: // MultiplicativeExpr
      case 56: // UnaryExpr
        value.move< blink::Persistent<blink::xpath::Expression> > (YY_MOVE (s.value));
        break;

      case 32: // LocationPath
      case 33: // AbsoluteLocationPath
      case 34: // RelativeLocationPath
        value.move< blink::Persistent<blink::xpath::LocationPath> > (YY_MOVE (s.value));
        break;

      case 37: // NodeTest
        value.move< blink::Persistent<blink::xpath::Step::NodeTest> > (YY_MOVE (s.value));
        break;

      case 35: // Step
      case 41: // DescendantOrSelf
      case 42: // AbbreviatedStep
        value.move< blink::Persistent<blink::xpath::Step> > (YY_MOVE (s.value));
        break;

      case 4: // kEqOp
      case 5: // kRelOp
        value.move< blink::xpath::EqTestOp::Opcode > (YY_MOVE (s.value));
        break;

      case 3: // kMulOp
        value.move< blink::xpath::NumericOp::Opcode > (YY_MOVE (s.value));
        break;

      case 10: // kAxisName
      case 36: // AxisSpecifier
        value.move< blink::xpath::Step::Axis > (YY_MOVE (s.value));
        break;

      default:
        break;
    }

  }

  // by_type.
  YyParser::by_type::by_type ()
    : type (empty_symbol)
  {}

#if 201103L <= YY_CPLUSPLUS
  YyParser::by_type::by_type (by_type&& that)
    : type (that.type)
  {
    that.clear ();
  }
#endif

  YyParser::by_type::by_type (const by_type& that)
    : type (that.type)
  {}

  YyParser::by_type::by_type (token_type t)
    : type (yytranslate_ (t))
  {}

  void
  YyParser::by_type::clear ()
  {
    type = empty_symbol;
  }

  void
  YyParser::by_type::move (by_type& that)
  {
    type = that.type;
    that.clear ();
  }

  int
  YyParser::by_type::type_get () const YY_NOEXCEPT
  {
    return type;
  }


  // by_state.
  YyParser::by_state::by_state () YY_NOEXCEPT
    : state (empty_state)
  {}

  YyParser::by_state::by_state (const by_state& that) YY_NOEXCEPT
    : state (that.state)
  {}

  void
  YyParser::by_state::clear () YY_NOEXCEPT
  {
    state = empty_state;
  }

  void
  YyParser::by_state::move (by_state& that)
  {
    state = that.state;
    that.clear ();
  }

  YyParser::by_state::by_state (state_type s) YY_NOEXCEPT
    : state (s)
  {}

  YyParser::symbol_number_type
  YyParser::by_state::type_get () const YY_NOEXCEPT
  {
    if (state == empty_state)
      return empty_symbol;
    else
      return yystos_[state];
  }

  YyParser::stack_symbol_type::stack_symbol_type ()
  {}

  YyParser::stack_symbol_type::stack_symbol_type (YY_RVREF (stack_symbol_type) that)
    : super_type (YY_MOVE (that.state))
  {
    switch (that.type_get ())
    {
      case 11: // kNodeType
      case 12: // kPI
      case 13: // kFunctionName
      case 14: // kLiteral
      case 15: // kVariableReference
      case 16: // kNumber
      case 19: // kNameTest
        value.YY_MOVE_OR_COPY< String > (YY_MOVE (that.value));
        break;

      case 45: // ArgumentList
        value.YY_MOVE_OR_COPY< blink::Persistent<blink::HeapVector<blink::Member<blink::xpath::Expression>>> > (YY_MOVE (that.value));
        break;

      case 38: // OptionalPredicateList
      case 39: // PredicateList
        value.YY_MOVE_OR_COPY< blink::Persistent<blink::HeapVector<blink::Member<blink::xpath::Predicate>>> > (YY_MOVE (that.value));
        break;

      case 31: // Expr
      case 40: // Predicate
      case 43: // PrimaryExpr
      case 44: // FunctionCall
      case 46: // Argument
      case 47: // UnionExpr
      case 48: // PathExpr
      case 49: // FilterExpr
      case 50: // OrExpr
      case 51: // AndExpr
      case 52: // EqualityExpr
      case 53: // RelationalExpr
      case 54: // AdditiveExpr
      case 55: // MultiplicativeExpr
      case 56: // UnaryExpr
        value.YY_MOVE_OR_COPY< blink::Persistent<blink::xpath::Expression> > (YY_MOVE (that.value));
        break;

      case 32: // LocationPath
      case 33: // AbsoluteLocationPath
      case 34: // RelativeLocationPath
        value.YY_MOVE_OR_COPY< blink::Persistent<blink::xpath::LocationPath> > (YY_MOVE (that.value));
        break;

      case 37: // NodeTest
        value.YY_MOVE_OR_COPY< blink::Persistent<blink::xpath::Step::NodeTest> > (YY_MOVE (that.value));
        break;

      case 35: // Step
      case 41: // DescendantOrSelf
      case 42: // AbbreviatedStep
        value.YY_MOVE_OR_COPY< blink::Persistent<blink::xpath::Step> > (YY_MOVE (that.value));
        break;

      case 4: // kEqOp
      case 5: // kRelOp
        value.YY_MOVE_OR_COPY< blink::xpath::EqTestOp::Opcode > (YY_MOVE (that.value));
        break;

      case 3: // kMulOp
        value.YY_MOVE_OR_COPY< blink::xpath::NumericOp::Opcode > (YY_MOVE (that.value));
        break;

      case 10: // kAxisName
      case 36: // AxisSpecifier
        value.YY_MOVE_OR_COPY< blink::xpath::Step::Axis > (YY_MOVE (that.value));
        break;

      default:
        break;
    }

#if 201103L <= YY_CPLUSPLUS
    // that is emptied.
    that.state = empty_state;
#endif
  }

  YyParser::stack_symbol_type::stack_symbol_type (state_type s, YY_MOVE_REF (symbol_type) that)
    : super_type (s)
  {
    switch (that.type_get ())
    {
      case 11: // kNodeType
      case 12: // kPI
      case 13: // kFunctionName
      case 14: // kLiteral
      case 15: // kVariableReference
      case 16: // kNumber
      case 19: // kNameTest
        value.move< String > (YY_MOVE (that.value));
        break;

      case 45: // ArgumentList
        value.move< blink::Persistent<blink::HeapVector<blink::Member<blink::xpath::Expression>>> > (YY_MOVE (that.value));
        break;

      case 38: // OptionalPredicateList
      case 39: // PredicateList
        value.move< blink::Persistent<blink::HeapVector<blink::Member<blink::xpath::Predicate>>> > (YY_MOVE (that.value));
        break;

      case 31: // Expr
      case 40: // Predicate
      case 43: // PrimaryExpr
      case 44: // FunctionCall
      case 46: // Argument
      case 47: // UnionExpr
      case 48: // PathExpr
      case 49: // FilterExpr
      case 50: // OrExpr
      case 51: // AndExpr
      case 52: // EqualityExpr
      case 53: // RelationalExpr
      case 54: // AdditiveExpr
      case 55: // MultiplicativeExpr
      case 56: // UnaryExpr
        value.move< blink::Persistent<blink::xpath::Expression> > (YY_MOVE (that.value));
        break;

      case 32: // LocationPath
      case 33: // AbsoluteLocationPath
      case 34: // RelativeLocationPath
        value.move< blink::Persistent<blink::xpath::LocationPath> > (YY_MOVE (that.value));
        break;

      case 37: // NodeTest
        value.move< blink::Persistent<blink::xpath::Step::NodeTest> > (YY_MOVE (that.value));
        break;

      case 35: // Step
      case 41: // DescendantOrSelf
      case 42: // AbbreviatedStep
        value.move< blink::Persistent<blink::xpath::Step> > (YY_MOVE (that.value));
        break;

      case 4: // kEqOp
      case 5: // kRelOp
        value.move< blink::xpath::EqTestOp::Opcode > (YY_MOVE (that.value));
        break;

      case 3: // kMulOp
        value.move< blink::xpath::NumericOp::Opcode > (YY_MOVE (that.value));
        break;

      case 10: // kAxisName
      case 36: // AxisSpecifier
        value.move< blink::xpath::Step::Axis > (YY_MOVE (that.value));
        break;

      default:
        break;
    }

    // that is emptied.
    that.type = empty_symbol;
  }

#if YY_CPLUSPLUS < 201103L
  YyParser::stack_symbol_type&
  YyParser::stack_symbol_type::operator= (stack_symbol_type& that)
  {
    state = that.state;
    switch (that.type_get ())
    {
      case 11: // kNodeType
      case 12: // kPI
      case 13: // kFunctionName
      case 14: // kLiteral
      case 15: // kVariableReference
      case 16: // kNumber
      case 19: // kNameTest
        value.move< String > (that.value);
        break;

      case 45: // ArgumentList
        value.move< blink::Persistent<blink::HeapVector<blink::Member<blink::xpath::Expression>>> > (that.value);
        break;

      case 38: // OptionalPredicateList
      case 39: // PredicateList
        value.move< blink::Persistent<blink::HeapVector<blink::Member<blink::xpath::Predicate>>> > (that.value);
        break;

      case 31: // Expr
      case 40: // Predicate
      case 43: // PrimaryExpr
      case 44: // FunctionCall
      case 46: // Argument
      case 47: // UnionExpr
      case 48: // PathExpr
      case 49: // FilterExpr
      case 50: // OrExpr
      case 51: // AndExpr
      case 52: // EqualityExpr
      case 53: // RelationalExpr
      case 54: // AdditiveExpr
      case 55: // MultiplicativeExpr
      case 56: // UnaryExpr
        value.move< blink::Persistent<blink::xpath::Expression> > (that.value);
        break;

      case 32: // LocationPath
      case 33: // AbsoluteLocationPath
      case 34: // RelativeLocationPath
        value.move< blink::Persistent<blink::xpath::LocationPath> > (that.value);
        break;

      case 37: // NodeTest
        value.move< blink::Persistent<blink::xpath::Step::NodeTest> > (that.value);
        break;

      case 35: // Step
      case 41: // DescendantOrSelf
      case 42: // AbbreviatedStep
        value.move< blink::Persistent<blink::xpath::Step> > (that.value);
        break;

      case 4: // kEqOp
      case 5: // kRelOp
        value.move< blink::xpath::EqTestOp::Opcode > (that.value);
        break;

      case 3: // kMulOp
        value.move< blink::xpath::NumericOp::Opcode > (that.value);
        break;

      case 10: // kAxisName
      case 36: // AxisSpecifier
        value.move< blink::xpath::Step::Axis > (that.value);
        break;

      default:
        break;
    }

    // that is emptied.
    that.state = empty_state;
    return *this;
  }
#endif

  template <typename Base>
  void
  YyParser::yy_destroy_ (const char* yymsg, basic_symbol<Base>& yysym) const
  {
    if (yymsg)
      YY_SYMBOL_PRINT (yymsg, yysym);
  }

#if YYDEBUG
  template <typename Base>
  void
  YyParser::yy_print_ (std::ostream& yyo,
                                     const basic_symbol<Base>& yysym) const
  {
    std::ostream& yyoutput = yyo;
    YYUSE (yyoutput);
    symbol_number_type yytype = yysym.type_get ();
#if defined __GNUC__ && ! defined __clang__ && ! defined __ICC && __GNUC__ * 100 + __GNUC_MINOR__ <= 408
    // Avoid a (spurious) G++ 4.8 warning about "array subscript is
    // below array bounds".
    if (yysym.empty ())
      std::abort ();
#endif
    yyo << (yytype < yyntokens_ ? "token" : "nterm")
        << ' ' << yytname_[yytype] << " (";
    YYUSE (yytype);
    yyo << ')';
  }
#endif

  void
  YyParser::yypush_ (const char* m, YY_MOVE_REF (stack_symbol_type) sym)
  {
    if (m)
      YY_SYMBOL_PRINT (m, sym);
    yystack_.push (YY_MOVE (sym));
  }

  void
  YyParser::yypush_ (const char* m, state_type s, YY_MOVE_REF (symbol_type) sym)
  {
#if 201103L <= YY_CPLUSPLUS
    yypush_ (m, stack_symbol_type (s, std::move (sym)));
#else
    stack_symbol_type ss (s, sym);
    yypush_ (m, ss);
#endif
  }

  void
  YyParser::yypop_ (int n)
  {
    yystack_.pop (n);
  }

#if YYDEBUG
  std::ostream&
  YyParser::debug_stream () const
  {
    return *yycdebug_;
  }

  void
  YyParser::set_debug_stream (std::ostream& o)
  {
    yycdebug_ = &o;
  }


  YyParser::debug_level_type
  YyParser::debug_level () const
  {
    return yydebug_;
  }

  void
  YyParser::set_debug_level (debug_level_type l)
  {
    yydebug_ = l;
  }
#endif // YYDEBUG

  YyParser::state_type
  YyParser::yy_lr_goto_state_ (state_type yystate, int yysym)
  {
    int yyr = yypgoto_[yysym - yyntokens_] + yystate;
    if (0 <= yyr && yyr <= yylast_ && yycheck_[yyr] == yystate)
      return yytable_[yyr];
    else
      return yydefgoto_[yysym - yyntokens_];
  }

  bool
  YyParser::yy_pact_value_is_default_ (int yyvalue)
  {
    return yyvalue == yypact_ninf_;
  }

  bool
  YyParser::yy_table_value_is_error_ (int yyvalue)
  {
    return yyvalue == yytable_ninf_;
  }

  int
  YyParser::operator() ()
  {
    return parse ();
  }

  int
  YyParser::parse ()
  {
    // State.
    int yyn;
    /// Length of the RHS of the rule being reduced.
    int yylen = 0;

    // Error handling.
    int yynerrs_ = 0;
    int yyerrstatus_ = 0;

    /// The lookahead symbol.
    symbol_type yyla;

    /// The return value of parse ().
    int yyresult;

#if YY_EXCEPTIONS
    try
#endif // YY_EXCEPTIONS
      {
    YYCDEBUG << "Starting parse\n";


    /* Initialize the stack.  The initial state will be set in
       yynewstate, since the latter expects the semantical and the
       location values to have been already stored, initialize these
       stacks with a primary value.  */
    yystack_.clear ();
    yypush_ (YY_NULLPTR, 0, YY_MOVE (yyla));

  /*-----------------------------------------------.
  | yynewstate -- push a new symbol on the stack.  |
  `-----------------------------------------------*/
  yynewstate:
    YYCDEBUG << "Entering state " << yystack_[0].state << '\n';

    // Accept?
    if (yystack_[0].state == yyfinal_)
      YYACCEPT;

    goto yybackup;


  /*-----------.
  | yybackup.  |
  `-----------*/
  yybackup:
    // Try to take a decision without lookahead.
    yyn = yypact_[yystack_[0].state];
    if (yy_pact_value_is_default_ (yyn))
      goto yydefault;

    // Read a lookahead token.
    if (yyla.empty ())
      {
        YYCDEBUG << "Reading a token: ";
#if YY_EXCEPTIONS
        try
#endif // YY_EXCEPTIONS
          {
            yyla.type = yytranslate_ (yylex (&yyla.value));
          }
#if YY_EXCEPTIONS
        catch (const syntax_error& yyexc)
          {
            YYCDEBUG << "Caught exception: " << yyexc.what() << '\n';
            error (yyexc);
            goto yyerrlab1;
          }
#endif // YY_EXCEPTIONS
      }
    YY_SYMBOL_PRINT ("Next token is", yyla);

    /* If the proper action on seeing token YYLA.TYPE is to reduce or
       to detect an error, take that action.  */
    yyn += yyla.type_get ();
    if (yyn < 0 || yylast_ < yyn || yycheck_[yyn] != yyla.type_get ())
      goto yydefault;

    // Reduce or error.
    yyn = yytable_[yyn];
    if (yyn <= 0)
      {
        if (yy_table_value_is_error_ (yyn))
          goto yyerrlab;
        yyn = -yyn;
        goto yyreduce;
      }

    // Count tokens shifted since error; after three, turn off error status.
    if (yyerrstatus_)
      --yyerrstatus_;

    // Shift the lookahead token.
    yypush_ ("Shifting", yyn, YY_MOVE (yyla));
    goto yynewstate;


  /*-----------------------------------------------------------.
  | yydefault -- do the default action for the current state.  |
  `-----------------------------------------------------------*/
  yydefault:
    yyn = yydefact_[yystack_[0].state];
    if (yyn == 0)
      goto yyerrlab;
    goto yyreduce;


  /*-----------------------------.
  | yyreduce -- do a reduction.  |
  `-----------------------------*/
  yyreduce:
    yylen = yyr2_[yyn];
    {
      stack_symbol_type yylhs;
      yylhs.state = yy_lr_goto_state_ (yystack_[yylen].state, yyr1_[yyn]);
      /* Variants are always initialized to an empty instance of the
         correct type. The default '$$ = $1' action is NOT applied
         when using variants.  */
      switch (yyr1_[yyn])
    {
      case 11: // kNodeType
      case 12: // kPI
      case 13: // kFunctionName
      case 14: // kLiteral
      case 15: // kVariableReference
      case 16: // kNumber
      case 19: // kNameTest
        yylhs.value.emplace< String > ();
        break;

      case 45: // ArgumentList
        yylhs.value.emplace< blink::Persistent<blink::HeapVector<blink::Member<blink::xpath::Expression>>> > ();
        break;

      case 38: // OptionalPredicateList
      case 39: // PredicateList
        yylhs.value.emplace< blink::Persistent<blink::HeapVector<blink::Member<blink::xpath::Predicate>>> > ();
        break;

      case 31: // Expr
      case 40: // Predicate
      case 43: // PrimaryExpr
      case 44: // FunctionCall
      case 46: // Argument
      case 47: // UnionExpr
      case 48: // PathExpr
      case 49: // FilterExpr
      case 50: // OrExpr
      case 51: // AndExpr
      case 52: // EqualityExpr
      case 53: // RelationalExpr
      case 54: // AdditiveExpr
      case 55: // MultiplicativeExpr
      case 56: // UnaryExpr
        yylhs.value.emplace< blink::Persistent<blink::xpath::Expression> > ();
        break;

      case 32: // LocationPath
      case 33: // AbsoluteLocationPath
      case 34: // RelativeLocationPath
        yylhs.value.emplace< blink::Persistent<blink::xpath::LocationPath> > ();
        break;

      case 37: // NodeTest
        yylhs.value.emplace< blink::Persistent<blink::xpath::Step::NodeTest> > ();
        break;

      case 35: // Step
      case 41: // DescendantOrSelf
      case 42: // AbbreviatedStep
        yylhs.value.emplace< blink::Persistent<blink::xpath::Step> > ();
        break;

      case 4: // kEqOp
      case 5: // kRelOp
        yylhs.value.emplace< blink::xpath::EqTestOp::Opcode > ();
        break;

      case 3: // kMulOp
        yylhs.value.emplace< blink::xpath::NumericOp::Opcode > ();
        break;

      case 10: // kAxisName
      case 36: // AxisSpecifier
        yylhs.value.emplace< blink::xpath::Step::Axis > ();
        break;

      default:
        break;
    }



      // Perform the reduction.
      YY_REDUCE_PRINT (yyn);
#if YY_EXCEPTIONS
      try
#endif // YY_EXCEPTIONS
        {
          switch (yyn)
            {
  case 2:
#line 129 "third_party/blink/renderer/core/xml/xpath_grammar.y"
    {
      parser_->top_expr_ = yystack_[0].value.as < blink::Persistent<blink::xpath::Expression> > ();
      yylhs.value.as < blink::Persistent<blink::xpath::Expression> > () = yystack_[0].value.as < blink::Persistent<blink::xpath::Expression> > ();
    }
#line 1069 "third_party/blink/renderer/core/xml/xpath_grammar_generated.cc"
    break;

  case 3:
#line 137 "third_party/blink/renderer/core/xml/xpath_grammar.y"
    {
      yylhs.value.as < blink::Persistent<blink::xpath::LocationPath> > () = yystack_[0].value.as < blink::Persistent<blink::xpath::LocationPath> > ();
      yylhs.value.as < blink::Persistent<blink::xpath::LocationPath> > ()->SetAbsolute(false);
    }
#line 1078 "third_party/blink/renderer/core/xml/xpath_grammar_generated.cc"
    break;

  case 4:
#line 143 "third_party/blink/renderer/core/xml/xpath_grammar.y"
    {
      yylhs.value.as < blink::Persistent<blink::xpath::LocationPath> > () = yystack_[0].value.as < blink::Persistent<blink::xpath::LocationPath> > ();
      yylhs.value.as < blink::Persistent<blink::xpath::LocationPath> > ()->SetAbsolute(true);
    }
#line 1087 "third_party/blink/renderer/core/xml/xpath_grammar_generated.cc"
    break;

  case 5:
#line 151 "third_party/blink/renderer/core/xml/xpath_grammar.y"
    {
      yylhs.value.as < blink::Persistent<blink::xpath::LocationPath> > () = blink::MakeGarbageCollected<blink::xpath::LocationPath>();
    }
#line 1095 "third_party/blink/renderer/core/xml/xpath_grammar_generated.cc"
    break;

  case 6:
#line 156 "third_party/blink/renderer/core/xml/xpath_grammar.y"
    {
      yylhs.value.as < blink::Persistent<blink::xpath::LocationPath> > () = yystack_[0].value.as < blink::Persistent<blink::xpath::LocationPath> > ();
    }
#line 1103 "third_party/blink/renderer/core/xml/xpath_grammar_generated.cc"
    break;

  case 7:
#line 161 "third_party/blink/renderer/core/xml/xpath_grammar.y"
    {
      yylhs.value.as < blink::Persistent<blink::xpath::LocationPath> > () = yystack_[0].value.as < blink::Persistent<blink::xpath::LocationPath> > ();
      yylhs.value.as < blink::Persistent<blink::xpath::LocationPath> > ()->InsertFirstStep(yystack_[1].value.as < blink::Persistent<blink::xpath::Step> > ());
    }
#line 1112 "third_party/blink/renderer/core/xml/xpath_grammar_generated.cc"
    break;

  case 8:
#line 169 "third_party/blink/renderer/core/xml/xpath_grammar.y"
    {
      yylhs.value.as < blink::Persistent<blink::xpath::LocationPath> > () = blink::MakeGarbageCollected<blink::xpath::LocationPath>();
      yylhs.value.as < blink::Persistent<blink::xpath::LocationPath> > ()->AppendStep(yystack_[0].value.as < blink::Persistent<blink::xpath::Step> > ());
    }
#line 1121 "third_party/blink/renderer/core/xml/xpath_grammar_generated.cc"
    break;

  case 9:
#line 175 "third_party/blink/renderer/core/xml/xpath_grammar.y"
    {
      yylhs.value.as < blink::Persistent<blink::xpath::LocationPath> > () = yystack_[2].value.as < blink::Persistent<blink::xpath::LocationPath> > ();
      yylhs.value.as < blink::Persistent<blink::xpath::LocationPath> > ()->AppendStep(yystack_[0].value.as < blink::Persistent<blink::xpath::Step> > ());
    }
#line 1130 "third_party/blink/renderer/core/xml/xpath_grammar_generated.cc"
    break;

  case 10:
#line 181 "third_party/blink/renderer/core/xml/xpath_grammar.y"
    {
      yylhs.value.as < blink::Persistent<blink::xpath::LocationPath> > () = yystack_[2].value.as < blink::Persistent<blink::xpath::LocationPath> > ();
      yylhs.value.as < blink::Persistent<blink::xpath::LocationPath> > ()->AppendStep(yystack_[1].value.as < blink::Persistent<blink::xpath::Step> > ());
      yylhs.value.as < blink::Persistent<blink::xpath::LocationPath> > ()->AppendStep(yystack_[0].value.as < blink::Persistent<blink::xpath::Step> > ());
    }
#line 1140 "third_party/blink/renderer/core/xml/xpath_grammar_generated.cc"
    break;

  case 11:
#line 190 "third_party/blink/renderer/core/xml/xpath_grammar.y"
    {
      if (yystack_[0].value.as < blink::Persistent<blink::HeapVector<blink::Member<blink::xpath::Predicate>>> > ())
        yylhs.value.as < blink::Persistent<blink::xpath::Step> > () = blink::MakeGarbageCollected<Step>(Step::kChildAxis, *yystack_[1].value.as < blink::Persistent<blink::xpath::Step::NodeTest> > (), *yystack_[0].value.as < blink::Persistent<blink::HeapVector<blink::Member<blink::xpath::Predicate>>> > ());
      else
        yylhs.value.as < blink::Persistent<blink::xpath::Step> > () = blink::MakeGarbageCollected<Step>(Step::kChildAxis, *yystack_[1].value.as < blink::Persistent<blink::xpath::Step::NodeTest> > ());
    }
#line 1151 "third_party/blink/renderer/core/xml/xpath_grammar_generated.cc"
    break;

  case 12:
#line 198 "third_party/blink/renderer/core/xml/xpath_grammar.y"
    {
      AtomicString local_name;
      AtomicString namespace_uri;
      if (!parser_->ExpandQName(yystack_[1].value.as < String > (), local_name, namespace_uri)) {
        parser_->got_namespace_error_ = true;
        YYABORT;
      }

      if (yystack_[0].value.as < blink::Persistent<blink::HeapVector<blink::Member<blink::xpath::Predicate>>> > ())
        yylhs.value.as < blink::Persistent<blink::xpath::Step> > () = blink::MakeGarbageCollected<Step>(Step::kChildAxis, Step::NodeTest(Step::NodeTest::kNameTest, local_name, namespace_uri), *yystack_[0].value.as < blink::Persistent<blink::HeapVector<blink::Member<blink::xpath::Predicate>>> > ());
      else
        yylhs.value.as < blink::Persistent<blink::xpath::Step> > () = blink::MakeGarbageCollected<Step>(Step::kChildAxis, Step::NodeTest(Step::NodeTest::kNameTest, local_name, namespace_uri));
    }
#line 1169 "third_party/blink/renderer/core/xml/xpath_grammar_generated.cc"
    break;

  case 13:
#line 213 "third_party/blink/renderer/core/xml/xpath_grammar.y"
    {
      if (yystack_[0].value.as < blink::Persistent<blink::HeapVector<blink::Member<blink::xpath::Predicate>>> > ())
        yylhs.value.as < blink::Persistent<blink::xpath::Step> > () = blink::MakeGarbageCollected<Step>(yystack_[2].value.as < blink::xpath::Step::Axis > (), *yystack_[1].value.as < blink::Persistent<blink::xpath::Step::NodeTest> > (), *yystack_[0].value.as < blink::Persistent<blink::HeapVector<blink::Member<blink::xpath::Predicate>>> > ());
      else
        yylhs.value.as < blink::Persistent<blink::xpath::Step> > () = blink::MakeGarbageCollected<Step>(yystack_[2].value.as < blink::xpath::Step::Axis > (), *yystack_[1].value.as < blink::Persistent<blink::xpath::Step::NodeTest> > ());
    }
#line 1180 "third_party/blink/renderer/core/xml/xpath_grammar_generated.cc"
    break;

  case 14:
#line 221 "third_party/blink/renderer/core/xml/xpath_grammar.y"
    {
      AtomicString local_name;
      AtomicString namespace_uri;
      if (!parser_->ExpandQName(yystack_[1].value.as < String > (), local_name, namespace_uri)) {
        parser_->got_namespace_error_ = true;
        YYABORT;
      }

      if (yystack_[0].value.as < blink::Persistent<blink::HeapVector<blink::Member<blink::xpath::Predicate>>> > ())
        yylhs.value.as < blink::Persistent<blink::xpath::Step> > () = blink::MakeGarbageCollected<Step>(yystack_[2].value.as < blink::xpath::Step::Axis > (), Step::NodeTest(Step::NodeTest::kNameTest, local_name, namespace_uri), *yystack_[0].value.as < blink::Persistent<blink::HeapVector<blink::Member<blink::xpath::Predicate>>> > ());
      else
        yylhs.value.as < blink::Persistent<blink::xpath::Step> > () = blink::MakeGarbageCollected<Step>(yystack_[2].value.as < blink::xpath::Step::Axis > (), Step::NodeTest(Step::NodeTest::kNameTest, local_name, namespace_uri));
    }
#line 1198 "third_party/blink/renderer/core/xml/xpath_grammar_generated.cc"
    break;

  case 15:
#line 235 "third_party/blink/renderer/core/xml/xpath_grammar.y"
    { yylhs.value.as < blink::Persistent<blink::xpath::Step> > () = yystack_[0].value.as < blink::Persistent<blink::xpath::Step> > (); }
#line 1204 "third_party/blink/renderer/core/xml/xpath_grammar_generated.cc"
    break;

  case 16:
#line 239 "third_party/blink/renderer/core/xml/xpath_grammar.y"
    { yylhs.value.as < blink::xpath::Step::Axis > () = yystack_[0].value.as < blink::xpath::Step::Axis > (); }
#line 1210 "third_party/blink/renderer/core/xml/xpath_grammar_generated.cc"
    break;

  case 17:
#line 242 "third_party/blink/renderer/core/xml/xpath_grammar.y"
    {
      yylhs.value.as < blink::xpath::Step::Axis > () = Step::kAttributeAxis;
    }
#line 1218 "third_party/blink/renderer/core/xml/xpath_grammar_generated.cc"
    break;

  case 18:
#line 249 "third_party/blink/renderer/core/xml/xpath_grammar.y"
    {
      if (yystack_[2].value.as < String > () == "node")
        yylhs.value.as < blink::Persistent<blink::xpath::Step::NodeTest> > () = blink::MakeGarbageCollected<Step::NodeTest>(Step::NodeTest::kAnyNodeTest);
      else if (yystack_[2].value.as < String > () == "text")
        yylhs.value.as < blink::Persistent<blink::xpath::Step::NodeTest> > () = blink::MakeGarbageCollected<Step::NodeTest>(Step::NodeTest::kTextNodeTest);
      else if (yystack_[2].value.as < String > () == "comment")
        yylhs.value.as < blink::Persistent<blink::xpath::Step::NodeTest> > () = blink::MakeGarbageCollected<Step::NodeTest>(Step::NodeTest::kCommentNodeTest);
    }
#line 1231 "third_party/blink/renderer/core/xml/xpath_grammar_generated.cc"
    break;

  case 19:
#line 259 "third_party/blink/renderer/core/xml/xpath_grammar.y"
    {
      yylhs.value.as < blink::Persistent<blink::xpath::Step::NodeTest> > () = blink::MakeGarbageCollected<Step::NodeTest>(Step::NodeTest::kProcessingInstructionNodeTest);
    }
#line 1239 "third_party/blink/renderer/core/xml/xpath_grammar_generated.cc"
    break;

  case 20:
#line 264 "third_party/blink/renderer/core/xml/xpath_grammar.y"
    {
      yylhs.value.as < blink::Persistent<blink::xpath::Step::NodeTest> > () = blink::MakeGarbageCollected<Step::NodeTest>(Step::NodeTest::kProcessingInstructionNodeTest, yystack_[1].value.as < String > ().StripWhiteSpace());
    }
#line 1247 "third_party/blink/renderer/core/xml/xpath_grammar_generated.cc"
    break;

  case 21:
#line 271 "third_party/blink/renderer/core/xml/xpath_grammar.y"
    {
      yylhs.value.as < blink::Persistent<blink::HeapVector<blink::Member<blink::xpath::Predicate>>> > () = 0;
    }
#line 1255 "third_party/blink/renderer/core/xml/xpath_grammar_generated.cc"
    break;

  case 22:
#line 276 "third_party/blink/renderer/core/xml/xpath_grammar.y"
    {
      yylhs.value.as < blink::Persistent<blink::HeapVector<blink::Member<blink::xpath::Predicate>>> > () = yystack_[0].value.as < blink::Persistent<blink::HeapVector<blink::Member<blink::xpath::Predicate>>> > ();
    }
#line 1263 "third_party/blink/renderer/core/xml/xpath_grammar_generated.cc"
    break;

  case 23:
#line 283 "third_party/blink/renderer/core/xml/xpath_grammar.y"
    {
      yylhs.value.as < blink::Persistent<blink::HeapVector<blink::Member<blink::xpath::Predicate>>> > () = blink::MakeGarbageCollected<blink::HeapVector<blink::Member<blink::xpath::Predicate>>>();
      yylhs.value.as < blink::Persistent<blink::HeapVector<blink::Member<blink::xpath::Predicate>>> > ()->push_back(blink::MakeGarbageCollected<blink::xpath::Predicate>(yystack_[0].value.as < blink::Persistent<blink::xpath::Expression> > ()));
    }
#line 1272 "third_party/blink/renderer/core/xml/xpath_grammar_generated.cc"
    break;

  case 24:
#line 289 "third_party/blink/renderer/core/xml/xpath_grammar.y"
    {
      yylhs.value.as < blink::Persistent<blink::HeapVector<blink::Member<blink::xpath::Predicate>>> > () = yystack_[1].value.as < blink::Persistent<blink::HeapVector<blink::Member<blink::xpath::Predicate>>> > ();
      yylhs.value.as < blink::Persistent<blink::HeapVector<blink::Member<blink::xpath::Predicate>>> > ()->push_back(blink::MakeGarbageCollected<blink::xpath::Predicate>(yystack_[0].value.as < blink::Persistent<blink::xpath::Expression> > ()));
    }
#line 1281 "third_party/blink/renderer/core/xml/xpath_grammar_generated.cc"
    break;

  case 25:
#line 297 "third_party/blink/renderer/core/xml/xpath_grammar.y"
    {
      yylhs.value.as < blink::Persistent<blink::xpath::Expression> > () = yystack_[1].value.as < blink::Persistent<blink::xpath::Expression> > ();
    }
#line 1289 "third_party/blink/renderer/core/xml/xpath_grammar_generated.cc"
    break;

  case 26:
#line 304 "third_party/blink/renderer/core/xml/xpath_grammar.y"
    {
      yylhs.value.as < blink::Persistent<blink::xpath::Step> > () = blink::MakeGarbageCollected<Step>(Step::kDescendantOrSelfAxis, Step::NodeTest(Step::NodeTest::kAnyNodeTest));
    }
#line 1297 "third_party/blink/renderer/core/xml/xpath_grammar_generated.cc"
    break;

  case 27:
#line 311 "third_party/blink/renderer/core/xml/xpath_grammar.y"
    {
      yylhs.value.as < blink::Persistent<blink::xpath::Step> > () = blink::MakeGarbageCollected<Step>(Step::kSelfAxis, Step::NodeTest(Step::NodeTest::kAnyNodeTest));
    }
#line 1305 "third_party/blink/renderer/core/xml/xpath_grammar_generated.cc"
    break;

  case 28:
#line 316 "third_party/blink/renderer/core/xml/xpath_grammar.y"
    {
      yylhs.value.as < blink::Persistent<blink::xpath::Step> > () = blink::MakeGarbageCollected<Step>(Step::kParentAxis, Step::NodeTest(Step::NodeTest::kAnyNodeTest));
    }
#line 1313 "third_party/blink/renderer/core/xml/xpath_grammar_generated.cc"
    break;

  case 29:
#line 323 "third_party/blink/renderer/core/xml/xpath_grammar.y"
    {
      yylhs.value.as < blink::Persistent<blink::xpath::Expression> > () = blink::MakeGarbageCollected<blink::xpath::VariableReference>(yystack_[0].value.as < String > ());
    }
#line 1321 "third_party/blink/renderer/core/xml/xpath_grammar_generated.cc"
    break;

  case 30:
#line 328 "third_party/blink/renderer/core/xml/xpath_grammar.y"
    {
      yylhs.value.as < blink::Persistent<blink::xpath::Expression> > () = yystack_[1].value.as < blink::Persistent<blink::xpath::Expression> > ();
    }
#line 1329 "third_party/blink/renderer/core/xml/xpath_grammar_generated.cc"
    break;

  case 31:
#line 333 "third_party/blink/renderer/core/xml/xpath_grammar.y"
    {
      yylhs.value.as < blink::Persistent<blink::xpath::Expression> > () = blink::MakeGarbageCollected<blink::xpath::StringExpression>(yystack_[0].value.as < String > ());
    }
#line 1337 "third_party/blink/renderer/core/xml/xpath_grammar_generated.cc"
    break;

  case 32:
#line 338 "third_party/blink/renderer/core/xml/xpath_grammar.y"
    {
      yylhs.value.as < blink::Persistent<blink::xpath::Expression> > () = blink::MakeGarbageCollected<blink::xpath::Number>(yystack_[0].value.as < String > ().ToDouble());
    }
#line 1345 "third_party/blink/renderer/core/xml/xpath_grammar_generated.cc"
    break;

  case 33:
#line 342 "third_party/blink/renderer/core/xml/xpath_grammar.y"
    { yylhs.value.as < blink::Persistent<blink::xpath::Expression> > () = yystack_[0].value.as < blink::Persistent<blink::xpath::Expression> > (); }
#line 1351 "third_party/blink/renderer/core/xml/xpath_grammar_generated.cc"
    break;

  case 34:
#line 347 "third_party/blink/renderer/core/xml/xpath_grammar.y"
    {
      yylhs.value.as < blink::Persistent<blink::xpath::Expression> > () = blink::xpath::CreateFunction(yystack_[2].value.as < String > ());
      if (!yylhs.value.as < blink::Persistent<blink::xpath::Expression> > ())
        YYABORT;
    }
#line 1361 "third_party/blink/renderer/core/xml/xpath_grammar_generated.cc"
    break;

  case 35:
#line 354 "third_party/blink/renderer/core/xml/xpath_grammar.y"
    {
      yylhs.value.as < blink::Persistent<blink::xpath::Expression> > () = blink::xpath::CreateFunction(yystack_[3].value.as < String > (), *yystack_[1].value.as < blink::Persistent<blink::HeapVector<blink::Member<blink::xpath::Expression>>> > ());
      if (!yylhs.value.as < blink::Persistent<blink::xpath::Expression> > ())
        YYABORT;
    }
#line 1371 "third_party/blink/renderer/core/xml/xpath_grammar_generated.cc"
    break;

  case 36:
#line 363 "third_party/blink/renderer/core/xml/xpath_grammar.y"
    {
      yylhs.value.as < blink::Persistent<blink::HeapVector<blink::Member<blink::xpath::Expression>>> > () = blink::MakeGarbageCollected<blink::HeapVector<blink::Member<blink::xpath::Expression>>>();
      yylhs.value.as < blink::Persistent<blink::HeapVector<blink::Member<blink::xpath::Expression>>> > ()->push_back(yystack_[0].value.as < blink::Persistent<blink::xpath::Expression> > ());
    }
#line 1380 "third_party/blink/renderer/core/xml/xpath_grammar_generated.cc"
    break;

  case 37:
#line 369 "third_party/blink/renderer/core/xml/xpath_grammar.y"
    {
      yylhs.value.as < blink::Persistent<blink::HeapVector<blink::Member<blink::xpath::Expression>>> > () = yystack_[2].value.as < blink::Persistent<blink::HeapVector<blink::Member<blink::xpath::Expression>>> > ();
      yylhs.value.as < blink::Persistent<blink::HeapVector<blink::Member<blink::xpath::Expression>>> > ()->push_back(yystack_[0].value.as < blink::Persistent<blink::xpath::Expression> > ());
    }
#line 1389 "third_party/blink/renderer/core/xml/xpath_grammar_generated.cc"
    break;

  case 38:
#line 376 "third_party/blink/renderer/core/xml/xpath_grammar.y"
    { yylhs.value.as < blink::Persistent<blink::xpath::Expression> > () = yystack_[0].value.as < blink::Persistent<blink::xpath::Expression> > (); }
#line 1395 "third_party/blink/renderer/core/xml/xpath_grammar_generated.cc"
    break;

  case 39:
#line 380 "third_party/blink/renderer/core/xml/xpath_grammar.y"
    { yylhs.value.as < blink::Persistent<blink::xpath::Expression> > () = yystack_[0].value.as < blink::Persistent<blink::xpath::Expression> > (); }
#line 1401 "third_party/blink/renderer/core/xml/xpath_grammar_generated.cc"
    break;

  case 40:
#line 383 "third_party/blink/renderer/core/xml/xpath_grammar.y"
    {
      yylhs.value.as < blink::Persistent<blink::xpath::Expression> > () = blink::MakeGarbageCollected<blink::xpath::Union>();
      yylhs.value.as < blink::Persistent<blink::xpath::Expression> > ()->AddSubExpression(yystack_[2].value.as < blink::Persistent<blink::xpath::Expression> > ());
      yylhs.value.as < blink::Persistent<blink::xpath::Expression> > ()->AddSubExpression(yystack_[0].value.as < blink::Persistent<blink::xpath::Expression> > ());
    }
#line 1411 "third_party/blink/renderer/core/xml/xpath_grammar_generated.cc"
    break;

  case 41:
#line 392 "third_party/blink/renderer/core/xml/xpath_grammar.y"
    {
      yylhs.value.as < blink::Persistent<blink::xpath::Expression> > () = yystack_[0].value.as < blink::Persistent<blink::xpath::LocationPath> > ();
    }
#line 1419 "third_party/blink/renderer/core/xml/xpath_grammar_generated.cc"
    break;

  case 42:
#line 396 "third_party/blink/renderer/core/xml/xpath_grammar.y"
    { yylhs.value.as < blink::Persistent<blink::xpath::Expression> > () = yystack_[0].value.as < blink::Persistent<blink::xpath::Expression> > (); }
#line 1425 "third_party/blink/renderer/core/xml/xpath_grammar_generated.cc"
    break;

  case 43:
#line 399 "third_party/blink/renderer/core/xml/xpath_grammar.y"
    {
      yystack_[0].value.as < blink::Persistent<blink::xpath::LocationPath> > ()->SetAbsolute(true);
      yylhs.value.as < blink::Persistent<blink::xpath::Expression> > () = blink::MakeGarbageCollected<blink::xpath::Path>(yystack_[2].value.as < blink::Persistent<blink::xpath::Expression> > (), yystack_[0].value.as < blink::Persistent<blink::xpath::LocationPath> > ());
    }
#line 1434 "third_party/blink/renderer/core/xml/xpath_grammar_generated.cc"
    break;

  case 44:
#line 405 "third_party/blink/renderer/core/xml/xpath_grammar.y"
    {
      yystack_[0].value.as < blink::Persistent<blink::xpath::LocationPath> > ()->InsertFirstStep(yystack_[1].value.as < blink::Persistent<blink::xpath::Step> > ());
      yystack_[0].value.as < blink::Persistent<blink::xpath::LocationPath> > ()->SetAbsolute(true);
      yylhs.value.as < blink::Persistent<blink::xpath::Expression> > () = blink::MakeGarbageCollected<blink::xpath::Path>(yystack_[2].value.as < blink::Persistent<blink::xpath::Expression> > (), yystack_[0].value.as < blink::Persistent<blink::xpath::LocationPath> > ());
    }
#line 1444 "third_party/blink/renderer/core/xml/xpath_grammar_generated.cc"
    break;

  case 45:
#line 413 "third_party/blink/renderer/core/xml/xpath_grammar.y"
    { yylhs.value.as < blink::Persistent<blink::xpath::Expression> > () = yystack_[0].value.as < blink::Persistent<blink::xpath::Expression> > (); }
#line 1450 "third_party/blink/renderer/core/xml/xpath_grammar_generated.cc"
    break;

  case 46:
#line 416 "third_party/blink/renderer/core/xml/xpath_grammar.y"
    {
      yylhs.value.as < blink::Persistent<blink::xpath::Expression> > () = blink::MakeGarbageCollected<blink::xpath::Filter>(yystack_[1].value.as < blink::Persistent<blink::xpath::Expression> > (), *yystack_[0].value.as < blink::Persistent<blink::HeapVector<blink::Member<blink::xpath::Predicate>>> > ());
    }
#line 1458 "third_party/blink/renderer/core/xml/xpath_grammar_generated.cc"
    break;

  case 47:
#line 422 "third_party/blink/renderer/core/xml/xpath_grammar.y"
    { yylhs.value.as < blink::Persistent<blink::xpath::Expression> > () = yystack_[0].value.as < blink::Persistent<blink::xpath::Expression> > (); }
#line 1464 "third_party/blink/renderer/core/xml/xpath_grammar_generated.cc"
    break;

  case 48:
#line 425 "third_party/blink/renderer/core/xml/xpath_grammar.y"
    {
      yylhs.value.as < blink::Persistent<blink::xpath::Expression> > () = blink::MakeGarbageCollected<blink::xpath::LogicalOp>(blink::xpath::LogicalOp::kOP_Or, yystack_[2].value.as < blink::Persistent<blink::xpath::Expression> > (), yystack_[0].value.as < blink::Persistent<blink::xpath::Expression> > ());
    }
#line 1472 "third_party/blink/renderer/core/xml/xpath_grammar_generated.cc"
    break;

  case 49:
#line 431 "third_party/blink/renderer/core/xml/xpath_grammar.y"
    { yylhs.value.as < blink::Persistent<blink::xpath::Expression> > () = yystack_[0].value.as < blink::Persistent<blink::xpath::Expression> > (); }
#line 1478 "third_party/blink/renderer/core/xml/xpath_grammar_generated.cc"
    break;

  case 50:
#line 434 "third_party/blink/renderer/core/xml/xpath_grammar.y"
    {
      yylhs.value.as < blink::Persistent<blink::xpath::Expression> > () = blink::MakeGarbageCollected<blink::xpath::LogicalOp>(blink::xpath::LogicalOp::kOP_And, yystack_[2].value.as < blink::Persistent<blink::xpath::Expression> > (), yystack_[0].value.as < blink::Persistent<blink::xpath::Expression> > ());
    }
#line 1486 "third_party/blink/renderer/core/xml/xpath_grammar_generated.cc"
    break;

  case 51:
#line 440 "third_party/blink/renderer/core/xml/xpath_grammar.y"
    { yylhs.value.as < blink::Persistent<blink::xpath::Expression> > () = yystack_[0].value.as < blink::Persistent<blink::xpath::Expression> > (); }
#line 1492 "third_party/blink/renderer/core/xml/xpath_grammar_generated.cc"
    break;

  case 52:
#line 443 "third_party/blink/renderer/core/xml/xpath_grammar.y"
    {
      yylhs.value.as < blink::Persistent<blink::xpath::Expression> > () = blink::MakeGarbageCollected<blink::xpath::EqTestOp>(yystack_[1].value.as < blink::xpath::EqTestOp::Opcode > (), yystack_[2].value.as < blink::Persistent<blink::xpath::Expression> > (), yystack_[0].value.as < blink::Persistent<blink::xpath::Expression> > ());
    }
#line 1500 "third_party/blink/renderer/core/xml/xpath_grammar_generated.cc"
    break;

  case 53:
#line 449 "third_party/blink/renderer/core/xml/xpath_grammar.y"
    { yylhs.value.as < blink::Persistent<blink::xpath::Expression> > () = yystack_[0].value.as < blink::Persistent<blink::xpath::Expression> > (); }
#line 1506 "third_party/blink/renderer/core/xml/xpath_grammar_generated.cc"
    break;

  case 54:
#line 452 "third_party/blink/renderer/core/xml/xpath_grammar.y"
    {
      yylhs.value.as < blink::Persistent<blink::xpath::Expression> > () = blink::MakeGarbageCollected<blink::xpath::EqTestOp>(yystack_[1].value.as < blink::xpath::EqTestOp::Opcode > (), yystack_[2].value.as < blink::Persistent<blink::xpath::Expression> > (), yystack_[0].value.as < blink::Persistent<blink::xpath::Expression> > ());
    }
#line 1514 "third_party/blink/renderer/core/xml/xpath_grammar_generated.cc"
    break;

  case 55:
#line 458 "third_party/blink/renderer/core/xml/xpath_grammar.y"
    { yylhs.value.as < blink::Persistent<blink::xpath::Expression> > () = yystack_[0].value.as < blink::Persistent<blink::xpath::Expression> > (); }
#line 1520 "third_party/blink/renderer/core/xml/xpath_grammar_generated.cc"
    break;

  case 56:
#line 461 "third_party/blink/renderer/core/xml/xpath_grammar.y"
    {
      yylhs.value.as < blink::Persistent<blink::xpath::Expression> > () = blink::MakeGarbageCollected<blink::xpath::NumericOp>(blink::xpath::NumericOp::kOP_Add, yystack_[2].value.as < blink::Persistent<blink::xpath::Expression> > (), yystack_[0].value.as < blink::Persistent<blink::xpath::Expression> > ());
    }
#line 1528 "third_party/blink/renderer/core/xml/xpath_grammar_generated.cc"
    break;

  case 57:
#line 466 "third_party/blink/renderer/core/xml/xpath_grammar.y"
    {
      yylhs.value.as < blink::Persistent<blink::xpath::Expression> > () = blink::MakeGarbageCollected<blink::xpath::NumericOp>(blink::xpath::NumericOp::kOP_Sub, yystack_[2].value.as < blink::Persistent<blink::xpath::Expression> > (), yystack_[0].value.as < blink::Persistent<blink::xpath::Expression> > ());
    }
#line 1536 "third_party/blink/renderer/core/xml/xpath_grammar_generated.cc"
    break;

  case 58:
#line 472 "third_party/blink/renderer/core/xml/xpath_grammar.y"
    { yylhs.value.as < blink::Persistent<blink::xpath::Expression> > () = yystack_[0].value.as < blink::Persistent<blink::xpath::Expression> > (); }
#line 1542 "third_party/blink/renderer/core/xml/xpath_grammar_generated.cc"
    break;

  case 59:
#line 475 "third_party/blink/renderer/core/xml/xpath_grammar.y"
    {
      yylhs.value.as < blink::Persistent<blink::xpath::Expression> > () = blink::MakeGarbageCollected<blink::xpath::NumericOp>(yystack_[1].value.as < blink::xpath::NumericOp::Opcode > (), yystack_[2].value.as < blink::Persistent<blink::xpath::Expression> > (), yystack_[0].value.as < blink::Persistent<blink::xpath::Expression> > ());
    }
#line 1550 "third_party/blink/renderer/core/xml/xpath_grammar_generated.cc"
    break;

  case 60:
#line 481 "third_party/blink/renderer/core/xml/xpath_grammar.y"
    { yylhs.value.as < blink::Persistent<blink::xpath::Expression> > () = yystack_[0].value.as < blink::Persistent<blink::xpath::Expression> > (); }
#line 1556 "third_party/blink/renderer/core/xml/xpath_grammar_generated.cc"
    break;

  case 61:
#line 484 "third_party/blink/renderer/core/xml/xpath_grammar.y"
    {
      yylhs.value.as < blink::Persistent<blink::xpath::Expression> > () = blink::MakeGarbageCollected<blink::xpath::Negative>();
      yylhs.value.as < blink::Persistent<blink::xpath::Expression> > ()->AddSubExpression(yystack_[0].value.as < blink::Persistent<blink::xpath::Expression> > ());
    }
#line 1565 "third_party/blink/renderer/core/xml/xpath_grammar_generated.cc"
    break;


#line 1569 "third_party/blink/renderer/core/xml/xpath_grammar_generated.cc"

            default:
              break;
            }
        }
#if YY_EXCEPTIONS
      catch (const syntax_error& yyexc)
        {
          YYCDEBUG << "Caught exception: " << yyexc.what() << '\n';
          error (yyexc);
          YYERROR;
        }
#endif // YY_EXCEPTIONS
      YY_SYMBOL_PRINT ("-> $$ =", yylhs);
      yypop_ (yylen);
      yylen = 0;
      YY_STACK_PRINT ();

      // Shift the result of the reduction.
      yypush_ (YY_NULLPTR, YY_MOVE (yylhs));
    }
    goto yynewstate;


  /*--------------------------------------.
  | yyerrlab -- here on detecting error.  |
  `--------------------------------------*/
  yyerrlab:
    // If not already recovering from an error, report this error.
    if (!yyerrstatus_)
      {
        ++yynerrs_;
        error (yysyntax_error_ (yystack_[0].state, yyla));
      }


    if (yyerrstatus_ == 3)
      {
        /* If just tried and failed to reuse lookahead token after an
           error, discard it.  */

        // Return failure if at end of input.
        if (yyla.type_get () == yyeof_)
          YYABORT;
        else if (!yyla.empty ())
          {
            yy_destroy_ ("Error: discarding", yyla);
            yyla.clear ();
          }
      }

    // Else will try to reuse lookahead token after shifting the error token.
    goto yyerrlab1;


  /*---------------------------------------------------.
  | yyerrorlab -- error raised explicitly by YYERROR.  |
  `---------------------------------------------------*/
  yyerrorlab:
    /* Pacify compilers when the user code never invokes YYERROR and
       the label yyerrorlab therefore never appears in user code.  */
    if (false)
      YYERROR;

    /* Do not reclaim the symbols of the rule whose action triggered
       this YYERROR.  */
    yypop_ (yylen);
    yylen = 0;
    goto yyerrlab1;


  /*-------------------------------------------------------------.
  | yyerrlab1 -- common code for both syntax error and YYERROR.  |
  `-------------------------------------------------------------*/
  yyerrlab1:
    yyerrstatus_ = 3;   // Each real token shifted decrements this.
    {
      stack_symbol_type error_token;
      for (;;)
        {
          yyn = yypact_[yystack_[0].state];
          if (!yy_pact_value_is_default_ (yyn))
            {
              yyn += yyterror_;
              if (0 <= yyn && yyn <= yylast_ && yycheck_[yyn] == yyterror_)
                {
                  yyn = yytable_[yyn];
                  if (0 < yyn)
                    break;
                }
            }

          // Pop the current state because it cannot handle the error token.
          if (yystack_.size () == 1)
            YYABORT;

          yy_destroy_ ("Error: popping", yystack_[0]);
          yypop_ ();
          YY_STACK_PRINT ();
        }


      // Shift the error token.
      error_token.state = yyn;
      yypush_ ("Shifting", YY_MOVE (error_token));
    }
    goto yynewstate;


  /*-------------------------------------.
  | yyacceptlab -- YYACCEPT comes here.  |
  `-------------------------------------*/
  yyacceptlab:
    yyresult = 0;
    goto yyreturn;


  /*-----------------------------------.
  | yyabortlab -- YYABORT comes here.  |
  `-----------------------------------*/
  yyabortlab:
    yyresult = 1;
    goto yyreturn;


  /*-----------------------------------------------------.
  | yyreturn -- parsing is finished, return the result.  |
  `-----------------------------------------------------*/
  yyreturn:
    if (!yyla.empty ())
      yy_destroy_ ("Cleanup: discarding lookahead", yyla);

    /* Do not reclaim the symbols of the rule whose action triggered
       this YYABORT or YYACCEPT.  */
    yypop_ (yylen);
    while (1 < yystack_.size ())
      {
        yy_destroy_ ("Cleanup: popping", yystack_[0]);
        yypop_ ();
      }

    return yyresult;
  }
#if YY_EXCEPTIONS
    catch (...)
      {
        YYCDEBUG << "Exception caught: cleaning lookahead and stack\n";
        // Do not try to display the values of the reclaimed symbols,
        // as their printers might throw an exception.
        if (!yyla.empty ())
          yy_destroy_ (YY_NULLPTR, yyla);

        while (1 < yystack_.size ())
          {
            yy_destroy_ (YY_NULLPTR, yystack_[0]);
            yypop_ ();
          }
        throw;
      }
#endif // YY_EXCEPTIONS
  }

  void
  YyParser::error (const syntax_error& yyexc)
  {
    error (yyexc.what ());
  }

  // Generate an error message.
  std::string
  YyParser::yysyntax_error_ (state_type, const symbol_type&) const
  {
    return YY_("syntax error");
  }


  const signed char YyParser::yypact_ninf_ = -44;

  const signed char YyParser::yytable_ninf_ = -1;

  const signed char
  YyParser::yypact_[] =
  {
      77,    77,   -44,    -9,    -4,    18,   -44,   -44,   -44,   -44,
     -44,    19,    -2,   -44,    77,   -44,    36,   -44,   -44,    13,
     -44,    11,    19,    -2,   -44,    19,   -44,    21,   -44,    17,
      34,    38,    44,    46,    20,    49,   -44,   -44,    25,    -3,
      59,    77,   -44,    19,   -44,    13,    29,   -44,    -2,    -2,
      19,    19,   -44,    13,    19,    95,    -2,    -2,    77,    77,
      77,    77,    77,    77,    77,   -44,    30,   -44,   -44,   -44,
       0,   -44,    31,   -44,   -44,   -44,   -44,   -44,   -44,   -44,
      13,    13,    38,    44,    46,    20,    49,    49,   -44,   -44,
     -44,    77,   -44,   -44
  };

  const unsigned char
  YyParser::yydefact_[] =
  {
       0,     0,    16,     0,     0,     0,    31,    29,    32,    28,
      26,    21,     5,    17,     0,    27,     0,    41,     4,     3,
       8,     0,    21,     0,    15,    45,    33,    60,    39,    42,
       2,    47,    49,    51,    53,    55,    58,    61,     0,     0,
       0,     0,    12,    22,    23,     6,     0,     1,     0,     0,
      21,    21,    11,     7,    46,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    18,     0,    19,    34,    38,
       0,    36,     0,    24,    30,     9,    10,    14,    13,    40,
      43,    44,    48,    50,    52,    54,    56,    57,    59,    20,
      35,     0,    25,    37
  };

  const signed char
  YyParser::yypgoto_[] =
  {
     -44,     2,   -44,   -44,   -11,   -43,   -44,    35,   -18,    33,
     -36,   -16,   -44,   -44,   -44,   -44,   -32,   -44,     5,   -44,
     -44,     3,     8,    -5,     1,   -23,    -1
  };

  const signed char
  YyParser::yydefgoto_[] =
  {
      -1,    69,    17,    18,    19,    20,    21,    22,    42,    43,
      44,    23,    24,    25,    26,    70,    71,    27,    28,    29,
      30,    31,    32,    33,    34,    35,    36
  };

  const unsigned char
  YyParser::yytable_[] =
  {
      37,    45,    16,    49,    52,    75,    76,    73,     2,     3,
       4,    66,    53,    57,    38,     9,    46,    11,    73,    39,
      13,    67,     3,     4,    90,    15,    62,    63,    91,    49,
      50,    10,    77,    78,    48,    10,    47,    49,    56,    86,
      87,    40,    58,    72,    41,    80,    81,    59,    60,    65,
      55,    61,    64,    74,    89,    84,    51,    92,    54,    93,
      79,    82,    85,    88,    49,    49,     1,    83,     0,     2,
       3,     4,     5,     6,     7,     8,     9,    10,    11,     0,
      12,    13,    14,    68,     1,     0,    15,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,     0,    12,    13,
      14,     0,     0,     0,    15,     2,     3,     4,     5,     6,
       7,     8,     9,    10,    11,     0,    12,    13,    14,     0,
       0,     0,    15
  };

  const signed char
  YyParser::yycheck_[] =
  {
       1,    12,     0,    19,    22,    48,    49,    43,    10,    11,
      12,    14,    23,    29,    23,    17,    14,    19,    54,    23,
      22,    24,    11,    12,    24,    27,     6,     7,    28,    45,
      19,    18,    50,    51,    21,    18,     0,    53,    21,    62,
      63,    23,     8,    41,    25,    56,    57,     9,     4,    24,
      29,     5,     3,    24,    24,    60,    21,    26,    25,    91,
      55,    58,    61,    64,    80,    81,     7,    59,    -1,    10,
      11,    12,    13,    14,    15,    16,    17,    18,    19,    -1,
      21,    22,    23,    24,     7,    -1,    27,    10,    11,    12,
      13,    14,    15,    16,    17,    18,    19,    -1,    21,    22,
      23,    -1,    -1,    -1,    27,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    -1,    21,    22,    23,    -1,
      -1,    -1,    27
  };

  const unsigned char
  YyParser::yystos_[] =
  {
       0,     7,    10,    11,    12,    13,    14,    15,    16,    17,
      18,    19,    21,    22,    23,    27,    31,    32,    33,    34,
      35,    36,    37,    41,    42,    43,    44,    47,    48,    49,
      50,    51,    52,    53,    54,    55,    56,    56,    23,    23,
      23,    25,    38,    39,    40,    34,    31,     0,    21,    41,
      19,    37,    38,    34,    39,    29,    21,    41,     8,     9,
       4,     5,     6,     7,     3,    24,    14,    24,    24,    31,
      45,    46,    31,    40,    24,    35,    35,    38,    38,    48,
      34,    34,    51,    52,    53,    54,    55,    55,    56,    24,
      24,    28,    26,    46
  };

  const unsigned char
  YyParser::yyr1_[] =
  {
       0,    30,    31,    32,    32,    33,    33,    33,    34,    34,
      34,    35,    35,    35,    35,    35,    36,    36,    37,    37,
      37,    38,    38,    39,    39,    40,    41,    42,    42,    43,
      43,    43,    43,    43,    44,    44,    45,    45,    46,    47,
      47,    48,    48,    48,    48,    49,    49,    50,    50,    51,
      51,    52,    52,    53,    53,    54,    54,    54,    55,    55,
      56,    56
  };

  const unsigned char
  YyParser::yyr2_[] =
  {
       0,     2,     1,     1,     1,     1,     2,     2,     1,     3,
       3,     2,     2,     3,     3,     1,     1,     1,     3,     3,
       4,     0,     1,     1,     2,     3,     1,     1,     1,     1,
       3,     1,     1,     1,     3,     4,     1,     3,     1,     1,
       3,     1,     1,     3,     3,     1,     2,     1,     3,     1,
       3,     1,     3,     1,     3,     1,     3,     3,     1,     3,
       1,     2
  };


#if YYDEBUG
  // YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
  // First, the terminals, then, starting at \a yyntokens_, nonterminals.
  const char*
  const YyParser::yytname_[] =
  {
  "$end", "error", "$undefined", "kMulOp", "kEqOp", "kRelOp", "kPlus",
  "kMinus", "kOr", "kAnd", "kAxisName", "kNodeType", "kPI",
  "kFunctionName", "kLiteral", "kVariableReference", "kNumber", "kDotDot",
  "kSlashSlash", "kNameTest", "kXPathError", "'/'", "'@'", "'('", "')'",
  "'['", "']'", "'.'", "','", "'|'", "$accept", "Expr", "LocationPath",
  "AbsoluteLocationPath", "RelativeLocationPath", "Step", "AxisSpecifier",
  "NodeTest", "OptionalPredicateList", "PredicateList", "Predicate",
  "DescendantOrSelf", "AbbreviatedStep", "PrimaryExpr", "FunctionCall",
  "ArgumentList", "Argument", "UnionExpr", "PathExpr", "FilterExpr",
  "OrExpr", "AndExpr", "EqualityExpr", "RelationalExpr", "AdditiveExpr",
  "MultiplicativeExpr", "UnaryExpr", YY_NULLPTR
  };


  const unsigned short
  YyParser::yyrline_[] =
  {
       0,   128,   128,   136,   142,   150,   155,   160,   168,   174,
     180,   189,   197,   212,   220,   235,   239,   241,   248,   258,
     263,   271,   275,   282,   288,   296,   303,   310,   315,   322,
     327,   332,   337,   342,   346,   353,   362,   368,   376,   380,
     382,   391,   396,   398,   404,   413,   415,   422,   424,   431,
     433,   440,   442,   449,   451,   458,   460,   465,   472,   474,
     481,   483
  };

  // Print the state stack on the debug stream.
  void
  YyParser::yystack_print_ ()
  {
    *yycdebug_ << "Stack now";
    for (stack_type::const_iterator
           i = yystack_.begin (),
           i_end = yystack_.end ();
         i != i_end; ++i)
      *yycdebug_ << ' ' << i->state;
    *yycdebug_ << '\n';
  }

  // Report on the debug stream that the rule \a yyrule is going to be reduced.
  void
  YyParser::yy_reduce_print_ (int yyrule)
  {
    unsigned yylno = yyrline_[yyrule];
    int yynrhs = yyr2_[yyrule];
    // Print the symbols being reduced, and their result.
    *yycdebug_ << "Reducing stack by rule " << yyrule - 1
               << " (line " << yylno << "):\n";
    // The symbols being reduced.
    for (int yyi = 0; yyi < yynrhs; yyi++)
      YY_SYMBOL_PRINT ("   $" << yyi + 1 << " =",
                       yystack_[(yynrhs) - (yyi + 1)]);
  }
#endif // YYDEBUG

  YyParser::token_number_type
  YyParser::yytranslate_ (int t)
  {
    // YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to
    // TOKEN-NUM as returned by yylex.
    static
    const token_number_type
    translate_table[] =
    {
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
      23,    24,     2,     2,    28,     2,    27,    21,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,    22,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    25,     2,    26,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,    29,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20
    };
    const unsigned user_token_number_max_ = 275;
    const token_number_type undef_token_ = 2;

    if (static_cast<int> (t) <= yyeof_)
      return yyeof_;
    else if (static_cast<unsigned> (t) <= user_token_number_max_)
      return translate_table[t];
    else
      return undef_token_;
  }

#line 69 "third_party/blink/renderer/core/xml/xpath_grammar.y"
} // xpathyy
#line 1984 "third_party/blink/renderer/core/xml/xpath_grammar_generated.cc"

#line 490 "third_party/blink/renderer/core/xml/xpath_grammar.y"

