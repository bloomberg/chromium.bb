// clang-format off
#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_XML_XPATH_GRAMMAR_GENERATED_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_XML_XPATH_GRAMMAR_GENERATED_H_
// A Bison parser, made by GNU Bison 3.4.2.

// Skeleton interface for Bison LALR(1) parsers in C++

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


/**
 ** \file third_party/blink/renderer/core/xml/xpath_grammar_generated.h
 ** Define the xpathyy::parser class.
 */

// C++ LALR(1) parser skeleton written by Akim Demaille.

// Undocumented macros, especially those whose name start with YY_,
// are private implementation details.  Do not rely on them.

#ifndef YY_YY_THIRD_PARTY_BLINK_RENDERER_CORE_XML_XPATH_GRAMMAR_GENERATED_HH_INCLUDED
# define YY_YY_THIRD_PARTY_BLINK_RENDERER_CORE_XML_XPATH_GRAMMAR_GENERATED_HH_INCLUDED
// //                    "%code requires" blocks.
#line 46 "third_party/blink/renderer/core/xml/xpath_grammar.y"


#include "third_party/blink/renderer/platform/heap/persistent.h"


#line 54 "third_party/blink/renderer/core/xml/xpath_grammar_generated.h"


# include <cstdlib> // std::abort
# include <iostream>
# include <stdexcept>
# include <string>
# include <vector>

#if defined __cplusplus
# define YY_CPLUSPLUS __cplusplus
#else
# define YY_CPLUSPLUS 199711L
#endif

// Support move semantics when possible.
#if 201103L <= YY_CPLUSPLUS
# define YY_MOVE           std::move
# define YY_MOVE_OR_COPY   move
# define YY_MOVE_REF(Type) Type&&
# define YY_RVREF(Type)    Type&&
# define YY_COPY(Type)     Type
#else
# define YY_MOVE
# define YY_MOVE_OR_COPY   copy
# define YY_MOVE_REF(Type) Type&
# define YY_RVREF(Type)    const Type&
# define YY_COPY(Type)     const Type&
#endif

// Support noexcept when possible.
#if 201103L <= YY_CPLUSPLUS
# define YY_NOEXCEPT noexcept
# define YY_NOTHROW
#else
# define YY_NOEXCEPT
# define YY_NOTHROW throw ()
#endif

// Support constexpr when possible.
#if 201703 <= YY_CPLUSPLUS
# define YY_CONSTEXPR constexpr
#else
# define YY_CONSTEXPR
#endif


#ifndef YYASSERT
# include <cassert>
# define YYASSERT assert
#endif


#ifndef YY_ATTRIBUTE
# if (defined __GNUC__                                               \
      && (2 < __GNUC__ || (__GNUC__ == 2 && 96 <= __GNUC_MINOR__)))  \
     || defined __SUNPRO_C && 0x5110 <= __SUNPRO_C
#  define YY_ATTRIBUTE(Spec) __attribute__(Spec)
# else
#  define YY_ATTRIBUTE(Spec) /* empty */
# endif
#endif

#ifndef YY_ATTRIBUTE_PURE
# define YY_ATTRIBUTE_PURE   YY_ATTRIBUTE ((__pure__))
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# define YY_ATTRIBUTE_UNUSED YY_ATTRIBUTE ((__unused__))
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(E) ((void) (E))
#else
# define YYUSE(E) /* empty */
#endif

#if defined __GNUC__ && ! defined __ICC && 407 <= __GNUC__ * 100 + __GNUC_MINOR__
/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN \
    _Pragma ("GCC diagnostic push") \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")\
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# define YY_IGNORE_MAYBE_UNINITIALIZED_END \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif

# ifndef YY_NULLPTR
#  if defined __cplusplus
#   if 201103L <= __cplusplus
#    define YY_NULLPTR nullptr
#   else
#    define YY_NULLPTR 0
#   endif
#  else
#   define YY_NULLPTR ((void*)0)
#  endif
# endif

/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif

#line 69 "third_party/blink/renderer/core/xml/xpath_grammar.y"
namespace xpathyy {
#line 170 "third_party/blink/renderer/core/xml/xpath_grammar_generated.h"




  /// A Bison parser.
  class YyParser
  {
  public:
#ifndef YYSTYPE
  /// A buffer to store and retrieve objects.
  ///
  /// Sort of a variant, but does not keep track of the nature
  /// of the stored data, since that knowledge is available
  /// via the current parser state.
  class semantic_type
  {
  public:
    /// Type of *this.
    typedef semantic_type self_type;

    /// Empty construction.
    semantic_type () YY_NOEXCEPT
      : yybuffer_ ()
    {}

    /// Construct and fill.
    template <typename T>
    semantic_type (YY_RVREF (T) t)
    {
      YYASSERT (sizeof (T) <= size);
      new (yyas_<T> ()) T (YY_MOVE (t));
    }

    /// Destruction, allowed only if empty.
    ~semantic_type () YY_NOEXCEPT
    {}

# if 201103L <= YY_CPLUSPLUS
    /// Instantiate a \a T in here from \a t.
    template <typename T, typename... U>
    T&
    emplace (U&&... u)
    {
      return *new (yyas_<T> ()) T (std::forward <U>(u)...);
    }
# else
    /// Instantiate an empty \a T in here.
    template <typename T>
    T&
    emplace ()
    {
      return *new (yyas_<T> ()) T ();
    }

    /// Instantiate a \a T in here from \a t.
    template <typename T>
    T&
    emplace (const T& t)
    {
      return *new (yyas_<T> ()) T (t);
    }
# endif

    /// Instantiate an empty \a T in here.
    /// Obsolete, use emplace.
    template <typename T>
    T&
    build ()
    {
      return emplace<T> ();
    }

    /// Instantiate a \a T in here from \a t.
    /// Obsolete, use emplace.
    template <typename T>
    T&
    build (const T& t)
    {
      return emplace<T> (t);
    }

    /// Accessor to a built \a T.
    template <typename T>
    T&
    as () YY_NOEXCEPT
    {
      return *yyas_<T> ();
    }

    /// Const accessor to a built \a T (for %printer).
    template <typename T>
    const T&
    as () const YY_NOEXCEPT
    {
      return *yyas_<T> ();
    }

    /// Swap the content with \a that, of same type.
    ///
    /// Both variants must be built beforehand, because swapping the actual
    /// data requires reading it (with as()), and this is not possible on
    /// unconstructed variants: it would require some dynamic testing, which
    /// should not be the variant's responsibility.
    /// Swapping between built and (possibly) non-built is done with
    /// self_type::move ().
    template <typename T>
    void
    swap (self_type& that) YY_NOEXCEPT
    {
      std::swap (as<T> (), that.as<T> ());
    }

    /// Move the content of \a that to this.
    ///
    /// Destroys \a that.
    template <typename T>
    void
    move (self_type& that)
    {
# if 201103L <= YY_CPLUSPLUS
      emplace<T> (std::move (that.as<T> ()));
# else
      emplace<T> ();
      swap<T> (that);
# endif
      that.destroy<T> ();
    }

# if 201103L <= YY_CPLUSPLUS
    /// Move the content of \a that to this.
    template <typename T>
    void
    move (self_type&& that)
    {
      emplace<T> (std::move (that.as<T> ()));
      that.destroy<T> ();
    }
#endif

    /// Copy the content of \a that to this.
    template <typename T>
    void
    copy (const self_type& that)
    {
      emplace<T> (that.as<T> ());
    }

    /// Destroy the stored \a T.
    template <typename T>
    void
    destroy ()
    {
      as<T> ().~T ();
    }

  private:
    /// Prohibit blind copies.
    self_type& operator= (const self_type&);
    semantic_type (const self_type&);

    /// Accessor to raw memory as \a T.
    template <typename T>
    T*
    yyas_ () YY_NOEXCEPT
    {
      void *yyp = yybuffer_.yyraw;
      return static_cast<T*> (yyp);
     }

    /// Const accessor to raw memory as \a T.
    template <typename T>
    const T*
    yyas_ () const YY_NOEXCEPT
    {
      const void *yyp = yybuffer_.yyraw;
      return static_cast<const T*> (yyp);
     }

    /// An auxiliary type to compute the largest semantic type.
    union union_type
    {
      // kNodeType
      // kPI
      // kFunctionName
      // kLiteral
      // kVariableReference
      // kNumber
      // kNameTest
      char dummy1[sizeof (String)];

      // ArgumentList
      char dummy2[sizeof (blink::Persistent<blink::HeapVector<blink::Member<blink::xpath::Expression>>>)];

      // OptionalPredicateList
      // PredicateList
      char dummy3[sizeof (blink::Persistent<blink::HeapVector<blink::Member<blink::xpath::Predicate>>>)];

      // Expr
      // Predicate
      // PrimaryExpr
      // FunctionCall
      // Argument
      // UnionExpr
      // PathExpr
      // FilterExpr
      // OrExpr
      // AndExpr
      // EqualityExpr
      // RelationalExpr
      // AdditiveExpr
      // MultiplicativeExpr
      // UnaryExpr
      char dummy4[sizeof (blink::Persistent<blink::xpath::Expression>)];

      // LocationPath
      // AbsoluteLocationPath
      // RelativeLocationPath
      char dummy5[sizeof (blink::Persistent<blink::xpath::LocationPath>)];

      // NodeTest
      char dummy6[sizeof (blink::Persistent<blink::xpath::Step::NodeTest>)];

      // Step
      // DescendantOrSelf
      // AbbreviatedStep
      char dummy7[sizeof (blink::Persistent<blink::xpath::Step>)];

      // kEqOp
      // kRelOp
      char dummy8[sizeof (blink::xpath::EqTestOp::Opcode)];

      // kMulOp
      char dummy9[sizeof (blink::xpath::NumericOp::Opcode)];

      // kAxisName
      // AxisSpecifier
      char dummy10[sizeof (blink::xpath::Step::Axis)];
    };

    /// The size of the largest semantic type.
    enum { size = sizeof (union_type) };

    /// A buffer to store semantic values.
    union
    {
      /// Strongest alignment constraints.
      long double yyalign_me;
      /// A buffer large enough to store any of the semantic values.
      char yyraw[size];
    } yybuffer_;
  };

#else
    typedef YYSTYPE semantic_type;
#endif

    /// Syntax errors thrown from user actions.
    struct syntax_error : std::runtime_error
    {
      syntax_error (const std::string& m)
        : std::runtime_error (m)
      {}

      syntax_error (const syntax_error& s)
        : std::runtime_error (s.what ())
      {}

      ~syntax_error () YY_NOEXCEPT YY_NOTHROW;
    };

    /// Tokens.
    struct token
    {
      enum yytokentype
      {
        kMulOp = 258,
        kEqOp = 259,
        kRelOp = 260,
        kPlus = 261,
        kMinus = 262,
        kOr = 263,
        kAnd = 264,
        kAxisName = 265,
        kNodeType = 266,
        kPI = 267,
        kFunctionName = 268,
        kLiteral = 269,
        kVariableReference = 270,
        kNumber = 271,
        kDotDot = 272,
        kSlashSlash = 273,
        kNameTest = 274,
        kXPathError = 275
      };
    };

    /// (External) token type, as returned by yylex.
    typedef token::yytokentype token_type;

    /// Symbol type: an internal symbol number.
    typedef int symbol_number_type;

    /// The symbol type number to denote an empty symbol.
    enum { empty_symbol = -2 };

    /// Internal symbol number for tokens (subsumed by symbol_number_type).
    typedef unsigned char token_number_type;

    /// A complete symbol.
    ///
    /// Expects its Base type to provide access to the symbol type
    /// via type_get ().
    ///
    /// Provide access to semantic value.
    template <typename Base>
    struct basic_symbol : Base
    {
      /// Alias to Base.
      typedef Base super_type;

      /// Default constructor.
      basic_symbol ()
        : value ()
      {}

#if 201103L <= YY_CPLUSPLUS
      /// Move constructor.
      basic_symbol (basic_symbol&& that);
#endif

      /// Copy constructor.
      basic_symbol (const basic_symbol& that);

      /// Constructor for valueless symbols, and symbols from each type.
#if 201103L <= YY_CPLUSPLUS
      basic_symbol (typename Base::kind_type t)
        : Base (t)
      {}
#else
      basic_symbol (typename Base::kind_type t)
        : Base (t)
      {}
#endif
#if 201103L <= YY_CPLUSPLUS
      basic_symbol (typename Base::kind_type t, String&& v)
        : Base (t)
        , value (std::move (v))
      {}
#else
      basic_symbol (typename Base::kind_type t, const String& v)
        : Base (t)
        , value (v)
      {}
#endif
#if 201103L <= YY_CPLUSPLUS
      basic_symbol (typename Base::kind_type t, blink::Persistent<blink::HeapVector<blink::Member<blink::xpath::Expression>>>&& v)
        : Base (t)
        , value (std::move (v))
      {}
#else
      basic_symbol (typename Base::kind_type t, const blink::Persistent<blink::HeapVector<blink::Member<blink::xpath::Expression>>>& v)
        : Base (t)
        , value (v)
      {}
#endif
#if 201103L <= YY_CPLUSPLUS
      basic_symbol (typename Base::kind_type t, blink::Persistent<blink::HeapVector<blink::Member<blink::xpath::Predicate>>>&& v)
        : Base (t)
        , value (std::move (v))
      {}
#else
      basic_symbol (typename Base::kind_type t, const blink::Persistent<blink::HeapVector<blink::Member<blink::xpath::Predicate>>>& v)
        : Base (t)
        , value (v)
      {}
#endif
#if 201103L <= YY_CPLUSPLUS
      basic_symbol (typename Base::kind_type t, blink::Persistent<blink::xpath::Expression>&& v)
        : Base (t)
        , value (std::move (v))
      {}
#else
      basic_symbol (typename Base::kind_type t, const blink::Persistent<blink::xpath::Expression>& v)
        : Base (t)
        , value (v)
      {}
#endif
#if 201103L <= YY_CPLUSPLUS
      basic_symbol (typename Base::kind_type t, blink::Persistent<blink::xpath::LocationPath>&& v)
        : Base (t)
        , value (std::move (v))
      {}
#else
      basic_symbol (typename Base::kind_type t, const blink::Persistent<blink::xpath::LocationPath>& v)
        : Base (t)
        , value (v)
      {}
#endif
#if 201103L <= YY_CPLUSPLUS
      basic_symbol (typename Base::kind_type t, blink::Persistent<blink::xpath::Step::NodeTest>&& v)
        : Base (t)
        , value (std::move (v))
      {}
#else
      basic_symbol (typename Base::kind_type t, const blink::Persistent<blink::xpath::Step::NodeTest>& v)
        : Base (t)
        , value (v)
      {}
#endif
#if 201103L <= YY_CPLUSPLUS
      basic_symbol (typename Base::kind_type t, blink::Persistent<blink::xpath::Step>&& v)
        : Base (t)
        , value (std::move (v))
      {}
#else
      basic_symbol (typename Base::kind_type t, const blink::Persistent<blink::xpath::Step>& v)
        : Base (t)
        , value (v)
      {}
#endif
#if 201103L <= YY_CPLUSPLUS
      basic_symbol (typename Base::kind_type t, blink::xpath::EqTestOp::Opcode&& v)
        : Base (t)
        , value (std::move (v))
      {}
#else
      basic_symbol (typename Base::kind_type t, const blink::xpath::EqTestOp::Opcode& v)
        : Base (t)
        , value (v)
      {}
#endif
#if 201103L <= YY_CPLUSPLUS
      basic_symbol (typename Base::kind_type t, blink::xpath::NumericOp::Opcode&& v)
        : Base (t)
        , value (std::move (v))
      {}
#else
      basic_symbol (typename Base::kind_type t, const blink::xpath::NumericOp::Opcode& v)
        : Base (t)
        , value (v)
      {}
#endif
#if 201103L <= YY_CPLUSPLUS
      basic_symbol (typename Base::kind_type t, blink::xpath::Step::Axis&& v)
        : Base (t)
        , value (std::move (v))
      {}
#else
      basic_symbol (typename Base::kind_type t, const blink::xpath::Step::Axis& v)
        : Base (t)
        , value (v)
      {}
#endif

      /// Destroy the symbol.
      ~basic_symbol ()
      {
        clear ();
      }

      /// Destroy contents, and record that is empty.
      void clear ()
      {
        // User destructor.
        symbol_number_type yytype = this->type_get ();
        basic_symbol<Base>& yysym = *this;
        (void) yysym;
        switch (yytype)
        {
       default:
          break;
        }

        // Type destructor.
switch (yytype)
    {
      case 11: // kNodeType
      case 12: // kPI
      case 13: // kFunctionName
      case 14: // kLiteral
      case 15: // kVariableReference
      case 16: // kNumber
      case 19: // kNameTest
        value.template destroy< String > ();
        break;

      case 45: // ArgumentList
        value.template destroy< blink::Persistent<blink::HeapVector<blink::Member<blink::xpath::Expression>>> > ();
        break;

      case 38: // OptionalPredicateList
      case 39: // PredicateList
        value.template destroy< blink::Persistent<blink::HeapVector<blink::Member<blink::xpath::Predicate>>> > ();
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
        value.template destroy< blink::Persistent<blink::xpath::Expression> > ();
        break;

      case 32: // LocationPath
      case 33: // AbsoluteLocationPath
      case 34: // RelativeLocationPath
        value.template destroy< blink::Persistent<blink::xpath::LocationPath> > ();
        break;

      case 37: // NodeTest
        value.template destroy< blink::Persistent<blink::xpath::Step::NodeTest> > ();
        break;

      case 35: // Step
      case 41: // DescendantOrSelf
      case 42: // AbbreviatedStep
        value.template destroy< blink::Persistent<blink::xpath::Step> > ();
        break;

      case 4: // kEqOp
      case 5: // kRelOp
        value.template destroy< blink::xpath::EqTestOp::Opcode > ();
        break;

      case 3: // kMulOp
        value.template destroy< blink::xpath::NumericOp::Opcode > ();
        break;

      case 10: // kAxisName
      case 36: // AxisSpecifier
        value.template destroy< blink::xpath::Step::Axis > ();
        break;

      default:
        break;
    }

        Base::clear ();
      }

      /// Whether empty.
      bool empty () const YY_NOEXCEPT;

      /// Destructive move, \a s is emptied into this.
      void move (basic_symbol& s);

      /// The semantic value.
      semantic_type value;

    private:
#if YY_CPLUSPLUS < 201103L
      /// Assignment operator.
      basic_symbol& operator= (const basic_symbol& that);
#endif
    };

    /// Type access provider for token (enum) based symbols.
    struct by_type
    {
      /// Default constructor.
      by_type ();

#if 201103L <= YY_CPLUSPLUS
      /// Move constructor.
      by_type (by_type&& that);
#endif

      /// Copy constructor.
      by_type (const by_type& that);

      /// The symbol type as needed by the constructor.
      typedef token_type kind_type;

      /// Constructor from (external) token numbers.
      by_type (kind_type t);

      /// Record that this symbol is empty.
      void clear ();

      /// Steal the symbol type from \a that.
      void move (by_type& that);

      /// The (internal) type number (corresponding to \a type).
      /// \a empty when empty.
      symbol_number_type type_get () const YY_NOEXCEPT;

      /// The token.
      token_type token () const YY_NOEXCEPT;

      /// The symbol type.
      /// \a empty_symbol when empty.
      /// An int, not token_number_type, to be able to store empty_symbol.
      int type;
    };

    /// "External" symbols: returned by the scanner.
    struct symbol_type : basic_symbol<by_type>
    {
      /// Superclass.
      typedef basic_symbol<by_type> super_type;

      /// Empty symbol.
      symbol_type () {}

      /// Constructor for valueless symbols, and symbols from each type.
#if 201103L <= YY_CPLUSPLUS
      symbol_type (int tok)
        : super_type(token_type (tok))
      {
        YYASSERT (tok == 0 || tok == token::kPlus || tok == token::kMinus || tok == token::kOr || tok == token::kAnd || tok == token::kDotDot || tok == token::kSlashSlash || tok == token::kXPathError || tok == 47 || tok == 64 || tok == 40 || tok == 41 || tok == 91 || tok == 93 || tok == 46 || tok == 44 || tok == 124);
      }
#else
      symbol_type (int tok)
        : super_type(token_type (tok))
      {
        YYASSERT (tok == 0 || tok == token::kPlus || tok == token::kMinus || tok == token::kOr || tok == token::kAnd || tok == token::kDotDot || tok == token::kSlashSlash || tok == token::kXPathError || tok == 47 || tok == 64 || tok == 40 || tok == 41 || tok == 91 || tok == 93 || tok == 46 || tok == 44 || tok == 124);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      symbol_type (int tok, String v)
        : super_type(token_type (tok), std::move (v))
      {
        YYASSERT (tok == token::kNodeType || tok == token::kPI || tok == token::kFunctionName || tok == token::kLiteral || tok == token::kVariableReference || tok == token::kNumber || tok == token::kNameTest);
      }
#else
      symbol_type (int tok, const String& v)
        : super_type(token_type (tok), v)
      {
        YYASSERT (tok == token::kNodeType || tok == token::kPI || tok == token::kFunctionName || tok == token::kLiteral || tok == token::kVariableReference || tok == token::kNumber || tok == token::kNameTest);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      symbol_type (int tok, blink::xpath::EqTestOp::Opcode v)
        : super_type(token_type (tok), std::move (v))
      {
        YYASSERT (tok == token::kEqOp || tok == token::kRelOp);
      }
#else
      symbol_type (int tok, const blink::xpath::EqTestOp::Opcode& v)
        : super_type(token_type (tok), v)
      {
        YYASSERT (tok == token::kEqOp || tok == token::kRelOp);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      symbol_type (int tok, blink::xpath::NumericOp::Opcode v)
        : super_type(token_type (tok), std::move (v))
      {
        YYASSERT (tok == token::kMulOp);
      }
#else
      symbol_type (int tok, const blink::xpath::NumericOp::Opcode& v)
        : super_type(token_type (tok), v)
      {
        YYASSERT (tok == token::kMulOp);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      symbol_type (int tok, blink::xpath::Step::Axis v)
        : super_type(token_type (tok), std::move (v))
      {
        YYASSERT (tok == token::kAxisName);
      }
#else
      symbol_type (int tok, const blink::xpath::Step::Axis& v)
        : super_type(token_type (tok), v)
      {
        YYASSERT (tok == token::kAxisName);
      }
#endif
    };

    /// Build a parser object.
    YyParser (blink::xpath::Parser* parser__yyarg);
    virtual ~YyParser ();

    /// Parse.  An alias for parse ().
    /// \returns  0 iff parsing succeeded.
    int operator() ();

    /// Parse.
    /// \returns  0 iff parsing succeeded.
    virtual int parse ();

#if YYDEBUG
    /// The current debugging stream.
    std::ostream& debug_stream () const YY_ATTRIBUTE_PURE;
    /// Set the current debugging stream.
    void set_debug_stream (std::ostream &);

    /// Type for debugging levels.
    typedef int debug_level_type;
    /// The current debugging level.
    debug_level_type debug_level () const YY_ATTRIBUTE_PURE;
    /// Set the current debugging level.
    void set_debug_level (debug_level_type l);
#endif

    /// Report a syntax error.
    /// \param msg    a description of the syntax error.
    virtual void error (const std::string& msg);

    /// Report a syntax error.
    void error (const syntax_error& err);

    // Implementation of make_symbol for each symbol type.
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_kMulOp (blink::xpath::NumericOp::Opcode v)
      {
        return symbol_type (token::kMulOp, std::move (v));
      }
#else
      static
      symbol_type
      make_kMulOp (const blink::xpath::NumericOp::Opcode& v)
      {
        return symbol_type (token::kMulOp, v);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_kEqOp (blink::xpath::EqTestOp::Opcode v)
      {
        return symbol_type (token::kEqOp, std::move (v));
      }
#else
      static
      symbol_type
      make_kEqOp (const blink::xpath::EqTestOp::Opcode& v)
      {
        return symbol_type (token::kEqOp, v);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_kRelOp (blink::xpath::EqTestOp::Opcode v)
      {
        return symbol_type (token::kRelOp, std::move (v));
      }
#else
      static
      symbol_type
      make_kRelOp (const blink::xpath::EqTestOp::Opcode& v)
      {
        return symbol_type (token::kRelOp, v);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_kPlus ()
      {
        return symbol_type (token::kPlus);
      }
#else
      static
      symbol_type
      make_kPlus ()
      {
        return symbol_type (token::kPlus);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_kMinus ()
      {
        return symbol_type (token::kMinus);
      }
#else
      static
      symbol_type
      make_kMinus ()
      {
        return symbol_type (token::kMinus);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_kOr ()
      {
        return symbol_type (token::kOr);
      }
#else
      static
      symbol_type
      make_kOr ()
      {
        return symbol_type (token::kOr);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_kAnd ()
      {
        return symbol_type (token::kAnd);
      }
#else
      static
      symbol_type
      make_kAnd ()
      {
        return symbol_type (token::kAnd);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_kAxisName (blink::xpath::Step::Axis v)
      {
        return symbol_type (token::kAxisName, std::move (v));
      }
#else
      static
      symbol_type
      make_kAxisName (const blink::xpath::Step::Axis& v)
      {
        return symbol_type (token::kAxisName, v);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_kNodeType (String v)
      {
        return symbol_type (token::kNodeType, std::move (v));
      }
#else
      static
      symbol_type
      make_kNodeType (const String& v)
      {
        return symbol_type (token::kNodeType, v);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_kPI (String v)
      {
        return symbol_type (token::kPI, std::move (v));
      }
#else
      static
      symbol_type
      make_kPI (const String& v)
      {
        return symbol_type (token::kPI, v);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_kFunctionName (String v)
      {
        return symbol_type (token::kFunctionName, std::move (v));
      }
#else
      static
      symbol_type
      make_kFunctionName (const String& v)
      {
        return symbol_type (token::kFunctionName, v);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_kLiteral (String v)
      {
        return symbol_type (token::kLiteral, std::move (v));
      }
#else
      static
      symbol_type
      make_kLiteral (const String& v)
      {
        return symbol_type (token::kLiteral, v);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_kVariableReference (String v)
      {
        return symbol_type (token::kVariableReference, std::move (v));
      }
#else
      static
      symbol_type
      make_kVariableReference (const String& v)
      {
        return symbol_type (token::kVariableReference, v);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_kNumber (String v)
      {
        return symbol_type (token::kNumber, std::move (v));
      }
#else
      static
      symbol_type
      make_kNumber (const String& v)
      {
        return symbol_type (token::kNumber, v);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_kDotDot ()
      {
        return symbol_type (token::kDotDot);
      }
#else
      static
      symbol_type
      make_kDotDot ()
      {
        return symbol_type (token::kDotDot);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_kSlashSlash ()
      {
        return symbol_type (token::kSlashSlash);
      }
#else
      static
      symbol_type
      make_kSlashSlash ()
      {
        return symbol_type (token::kSlashSlash);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_kNameTest (String v)
      {
        return symbol_type (token::kNameTest, std::move (v));
      }
#else
      static
      symbol_type
      make_kNameTest (const String& v)
      {
        return symbol_type (token::kNameTest, v);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_kXPathError ()
      {
        return symbol_type (token::kXPathError);
      }
#else
      static
      symbol_type
      make_kXPathError ()
      {
        return symbol_type (token::kXPathError);
      }
#endif


  private:
    /// This class is not copyable.
    YyParser (const YyParser&);
    YyParser& operator= (const YyParser&);

    /// State numbers.
    typedef int state_type;

    /// Generate an error message.
    /// \param yystate   the state where the error occurred.
    /// \param yyla      the lookahead token.
    virtual std::string yysyntax_error_ (state_type yystate,
                                         const symbol_type& yyla) const;

    /// Compute post-reduction state.
    /// \param yystate   the current state
    /// \param yysym     the nonterminal to push on the stack
    state_type yy_lr_goto_state_ (state_type yystate, int yysym);

    /// Whether the given \c yypact_ value indicates a defaulted state.
    /// \param yyvalue   the value to check
    static bool yy_pact_value_is_default_ (int yyvalue);

    /// Whether the given \c yytable_ value indicates a syntax error.
    /// \param yyvalue   the value to check
    static bool yy_table_value_is_error_ (int yyvalue);

    static const signed char yypact_ninf_;
    static const signed char yytable_ninf_;

    /// Convert a scanner token number \a t to a symbol number.
    static token_number_type yytranslate_ (int t);

    // Tables.
  // YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
  // STATE-NUM.
  static const signed char yypact_[];

  // YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
  // Performed when YYTABLE does not specify something else to do.  Zero
  // means the default is an error.
  static const unsigned char yydefact_[];

  // YYPGOTO[NTERM-NUM].
  static const signed char yypgoto_[];

  // YYDEFGOTO[NTERM-NUM].
  static const signed char yydefgoto_[];

  // YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
  // positive, shift that token.  If negative, reduce the rule whose
  // number is the opposite.  If YYTABLE_NINF, syntax error.
  static const unsigned char yytable_[];

  static const signed char yycheck_[];

  // YYSTOS[STATE-NUM] -- The (internal number of the) accessing
  // symbol of state STATE-NUM.
  static const unsigned char yystos_[];

  // YYR1[YYN] -- Symbol number of symbol that rule YYN derives.
  static const unsigned char yyr1_[];

  // YYR2[YYN] -- Number of symbols on the right hand side of rule YYN.
  static const unsigned char yyr2_[];


#if YYDEBUG
    /// For a symbol, its name in clear.
    static const char* const yytname_[];

  // YYRLINE[YYN] -- Source line where rule number YYN was defined.
  static const unsigned short yyrline_[];
    /// Report on the debug stream that the rule \a r is going to be reduced.
    virtual void yy_reduce_print_ (int r);
    /// Print the state stack on the debug stream.
    virtual void yystack_print_ ();

    /// Debugging level.
    int yydebug_;
    /// Debug stream.
    std::ostream* yycdebug_;

    /// \brief Display a symbol type, value and location.
    /// \param yyo    The output stream.
    /// \param yysym  The symbol.
    template <typename Base>
    void yy_print_ (std::ostream& yyo, const basic_symbol<Base>& yysym) const;
#endif

    /// \brief Reclaim the memory associated to a symbol.
    /// \param yymsg     Why this token is reclaimed.
    ///                  If null, print nothing.
    /// \param yysym     The symbol.
    template <typename Base>
    void yy_destroy_ (const char* yymsg, basic_symbol<Base>& yysym) const;

  private:
    /// Type access provider for state based symbols.
    struct by_state
    {
      /// Default constructor.
      by_state () YY_NOEXCEPT;

      /// The symbol type as needed by the constructor.
      typedef state_type kind_type;

      /// Constructor.
      by_state (kind_type s) YY_NOEXCEPT;

      /// Copy constructor.
      by_state (const by_state& that) YY_NOEXCEPT;

      /// Record that this symbol is empty.
      void clear () YY_NOEXCEPT;

      /// Steal the symbol type from \a that.
      void move (by_state& that);

      /// The (internal) type number (corresponding to \a state).
      /// \a empty_symbol when empty.
      symbol_number_type type_get () const YY_NOEXCEPT;

      /// The state number used to denote an empty symbol.
      enum { empty_state = -1 };

      /// The state.
      /// \a empty when empty.
      state_type state;
    };

    /// "Internal" symbol: element of the stack.
    struct stack_symbol_type : basic_symbol<by_state>
    {
      /// Superclass.
      typedef basic_symbol<by_state> super_type;
      /// Construct an empty symbol.
      stack_symbol_type ();
      /// Move or copy construction.
      stack_symbol_type (YY_RVREF (stack_symbol_type) that);
      /// Steal the contents from \a sym to build this.
      stack_symbol_type (state_type s, YY_MOVE_REF (symbol_type) sym);
#if YY_CPLUSPLUS < 201103L
      /// Assignment, needed by push_back by some old implementations.
      /// Moves the contents of that.
      stack_symbol_type& operator= (stack_symbol_type& that);
#endif
    };

    /// A stack with random access from its top.
    template <typename T, typename S = std::vector<T> >
    class stack
    {
    public:
      // Hide our reversed order.
      typedef typename S::reverse_iterator iterator;
      typedef typename S::const_reverse_iterator const_iterator;
      typedef typename S::size_type size_type;

      stack (size_type n = 200)
        : seq_ (n)
      {}

      /// Random access.
      ///
      /// Index 0 returns the topmost element.
      T&
      operator[] (size_type i)
      {
        return seq_[size () - 1 - i];
      }

      /// Random access.
      ///
      /// Index 0 returns the topmost element.
      T&
      operator[] (int i)
      {
        return operator[] (size_type (i));
      }

      /// Random access.
      ///
      /// Index 0 returns the topmost element.
      const T&
      operator[] (size_type i) const
      {
        return seq_[size () - 1 - i];
      }

      /// Random access.
      ///
      /// Index 0 returns the topmost element.
      const T&
      operator[] (int i) const
      {
        return operator[] (size_type (i));
      }

      /// Steal the contents of \a t.
      ///
      /// Close to move-semantics.
      void
      push (YY_MOVE_REF (T) t)
      {
        seq_.push_back (T ());
        operator[] (0).move (t);
      }

      /// Pop elements from the stack.
      void
      pop (int n = 1) YY_NOEXCEPT
      {
        for (; 0 < n; --n)
          seq_.pop_back ();
      }

      /// Pop all elements from the stack.
      void
      clear () YY_NOEXCEPT
      {
        seq_.clear ();
      }

      /// Number of elements on the stack.
      size_type
      size () const YY_NOEXCEPT
      {
        return seq_.size ();
      }

      /// Iterator on top of the stack (going downwards).
      const_iterator
      begin () const YY_NOEXCEPT
      {
        return seq_.rbegin ();
      }

      /// Bottom of the stack.
      const_iterator
      end () const YY_NOEXCEPT
      {
        return seq_.rend ();
      }

      /// Present a slice of the top of a stack.
      class slice
      {
      public:
        slice (const stack& stack, int range)
          : stack_ (stack)
          , range_ (range)
        {}

        const T&
        operator[] (int i) const
        {
          return stack_[range_ - i];
        }

      private:
        const stack& stack_;
        int range_;
      };

    private:
      stack (const stack&);
      stack& operator= (const stack&);
      /// The wrapped container.
      S seq_;
    };


    /// Stack type.
    typedef stack<stack_symbol_type> stack_type;

    /// The stack.
    stack_type yystack_;

    /// Push a new state on the stack.
    /// \param m    a debug message to display
    ///             if null, no trace is output.
    /// \param sym  the symbol
    /// \warning the contents of \a s.value is stolen.
    void yypush_ (const char* m, YY_MOVE_REF (stack_symbol_type) sym);

    /// Push a new look ahead token on the state on the stack.
    /// \param m    a debug message to display
    ///             if null, no trace is output.
    /// \param s    the state
    /// \param sym  the symbol (for its value and location).
    /// \warning the contents of \a sym.value is stolen.
    void yypush_ (const char* m, state_type s, YY_MOVE_REF (symbol_type) sym);

    /// Pop \a n symbols from the stack.
    void yypop_ (int n = 1);

    /// Constants.
    enum
    {
      yyeof_ = 0,
      yylast_ = 122,     ///< Last index in yytable_.
      yynnts_ = 27,  ///< Number of nonterminal symbols.
      yyfinal_ = 47, ///< Termination state number.
      yyterror_ = 1,
      yyerrcode_ = 256,
      yyntokens_ = 30  ///< Number of tokens.
    };


    // User arguments.
    blink::xpath::Parser* parser_;
  };


#line 69 "third_party/blink/renderer/core/xml/xpath_grammar.y"
} // xpathyy
#line 1476 "third_party/blink/renderer/core/xml/xpath_grammar_generated.h"





#endif // !YY_YY_THIRD_PARTY_BLINK_RENDERER_CORE_XML_XPATH_GRAMMAR_GENERATED_HH_INCLUDED
#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_XML_XPATH_GRAMMAR_GENERATED_H_
