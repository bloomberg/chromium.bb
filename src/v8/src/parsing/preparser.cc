// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>

#include "src/allocation.h"
#include "src/base/logging.h"
#include "src/conversions-inl.h"
#include "src/conversions.h"
#include "src/globals.h"
#include "src/parsing/parser-base.h"
#include "src/parsing/preparsed-scope-data.h"
#include "src/parsing/preparser.h"
#include "src/unicode.h"
#include "src/utils.h"

namespace v8 {
namespace internal {

namespace {

PreParserIdentifier GetSymbolHelper(Scanner* scanner,
                                    const AstRawString* string,
                                    AstValueFactory* avf) {
  // These symbols require slightly different treatement:
  // - regular keywords (async, await, etc.; treated in 1st switch.)
  // - 'contextual' keywords (and may contain escaped; treated in 2nd switch.)
  // - 'contextual' keywords, but may not be escaped (3rd switch).
  switch (scanner->current_token()) {
    case Token::AWAIT:
      return PreParserIdentifier::Await();
    case Token::ASYNC:
      return PreParserIdentifier::Async();
    case Token::PRIVATE_NAME:
      return PreParserIdentifier::PrivateName();
    default:
      break;
  }
  if (string == avf->constructor_string()) {
    return PreParserIdentifier::Constructor();
  }
  if (string == avf->name_string()) {
    return PreParserIdentifier::Name();
  }
  if (scanner->literal_contains_escapes()) {
    return PreParserIdentifier::Default();
  }
  if (string == avf->eval_string()) {
    return PreParserIdentifier::Eval();
  }
  if (string == avf->arguments_string()) {
    return PreParserIdentifier::Arguments();
  }
  return PreParserIdentifier::Default();
}

}  // unnamed namespace

PreParserIdentifier PreParser::GetSymbol() const {
  const AstRawString* result = scanner()->CurrentSymbol(ast_value_factory());
  PreParserIdentifier symbol =
      GetSymbolHelper(scanner(), result, ast_value_factory());
  DCHECK_NOT_NULL(result);
  symbol.string_ = result;
  return symbol;
}

PreParser::PreParseResult PreParser::PreParseProgram() {
  DCHECK_NULL(scope_);
  DeclarationScope* scope = NewScriptScope();
#ifdef DEBUG
  scope->set_is_being_lazily_parsed(true);
#endif

  // ModuleDeclarationInstantiation for Source Text Module Records creates a
  // new Module Environment Record whose outer lexical environment record is
  // the global scope.
  if (parsing_module_) scope = NewModuleScope(scope);

  FunctionState top_scope(&function_state_, &scope_, scope);
  original_scope_ = scope_;
  int start_position = peek_position();
  PreParserScopedStatementList body(pointer_buffer());
  ParseStatementList(&body, Token::EOS);
  original_scope_ = nullptr;
  if (stack_overflow()) return kPreParseStackOverflow;
  if (is_strict(language_mode())) {
    CheckStrictOctalLiteral(start_position, scanner()->location().end_pos);
  }
  return kPreParseSuccess;
}

PreParser::PreParseResult PreParser::PreParseFunction(
    const AstRawString* function_name, FunctionKind kind,
    FunctionLiteral::FunctionType function_type,
    DeclarationScope* function_scope, bool may_abort, int* use_counts,
    ProducedPreParsedScopeData** produced_preparsed_scope_data, int script_id) {
  DCHECK_EQ(FUNCTION_SCOPE, function_scope->scope_type());
  use_counts_ = use_counts;
  set_script_id(script_id);
#ifdef DEBUG
  function_scope->set_is_being_lazily_parsed(true);
#endif

  // Start collecting data for a new function which might contain skippable
  // functions.
  std::unique_ptr<PreParsedScopeDataBuilder::DataGatheringScope>
      preparsed_scope_data_builder_scope;
  if (!IsArrowFunction(kind)) {
    preparsed_scope_data_builder_scope.reset(
        new PreParsedScopeDataBuilder::DataGatheringScope(function_scope,
                                                          this));
  }

  // In the preparser, we use the function literal ids to count how many
  // FunctionLiterals were encountered. The PreParser doesn't actually persist
  // FunctionLiterals, so there IDs don't matter.
  ResetFunctionLiteralId();

  // The caller passes the function_scope which is not yet inserted into the
  // scope stack. All scopes above the function_scope are ignored by the
  // PreParser.
  DCHECK_NULL(function_state_);
  DCHECK_NULL(scope_);
  FunctionState function_state(&function_state_, &scope_, function_scope);

  PreParserFormalParameters formals(function_scope);
  std::unique_ptr<ExpressionClassifier> formals_classifier;

  // Parse non-arrow function parameters. For arrow functions, the parameters
  // have already been parsed.
  if (!IsArrowFunction(kind)) {
    formals_classifier.reset(new ExpressionClassifier(this));
    // We return kPreParseSuccess in failure cases too - errors are retrieved
    // separately by Parser::SkipLazyFunctionBody.
    ParseFormalParameterList(&formals);
    Expect(Token::RPAREN);
    int formals_end_position = scanner()->location().end_pos;

    CheckArityRestrictions(formals.arity, kind, formals.has_rest,
                           function_scope->start_position(),
                           formals_end_position);
  }

  Expect(Token::LBRACE);
  DeclarationScope* inner_scope = function_scope;
  LazyParsingResult result;

  if (!formals.is_simple) {
    inner_scope = NewVarblockScope();
    inner_scope->set_start_position(position());
  }

  {
    BlockState block_state(&scope_, inner_scope);
    result = ParseStatementListAndLogFunction(&formals, may_abort);
  }

  bool allow_duplicate_parameters = false;

  if (formals.is_simple) {
    if (is_sloppy(function_scope->language_mode())) {
      function_scope->HoistSloppyBlockFunctions(nullptr);
    }

    allow_duplicate_parameters = is_sloppy(function_scope->language_mode()) &&
                                 !IsConciseMethod(kind) &&
                                 !IsArrowFunction(kind);
  } else {
    BuildParameterInitializationBlock(formals);

    if (is_sloppy(inner_scope->language_mode())) {
      inner_scope->HoistSloppyBlockFunctions(nullptr);
    }

    SetLanguageMode(function_scope, inner_scope->language_mode());
    inner_scope->set_end_position(scanner()->peek_location().end_pos);
    inner_scope->FinalizeBlockScope();
  }

  use_counts_ = nullptr;

  if (stack_overflow()) {
    return kPreParseStackOverflow;
  } else if (pending_error_handler()->has_error_unidentifiable_by_preparser()) {
    return kPreParseNotIdentifiableError;
  } else if (has_error()) {
    DCHECK(pending_error_handler()->has_pending_error());
  } else if (result == kLazyParsingAborted) {
    DCHECK(!pending_error_handler()->has_error_unidentifiable_by_preparser());
    return kPreParseAbort;
  } else {
    DCHECK_EQ(Token::RBRACE, scanner()->peek());
    DCHECK(result == kLazyParsingComplete);

    if (!IsArrowFunction(kind)) {
      // Validate parameter names. We can do this only after parsing the
      // function, since the function can declare itself strict.
      ValidateFormalParameters(language_mode(), formals,
                               allow_duplicate_parameters);
      if (has_error()) {
        if (pending_error_handler()->has_error_unidentifiable_by_preparser()) {
          return kPreParseNotIdentifiableError;
        } else {
          return kPreParseSuccess;
        }
      }

      // Declare arguments after parsing the function since lexical
      // 'arguments' masks the arguments object. Declare arguments before
      // declaring the function var since the arguments object masks 'function
      // arguments'.
      function_scope->DeclareArguments(ast_value_factory());

      DeclareFunctionNameVar(function_name, function_type, function_scope);

      *produced_preparsed_scope_data = ProducedPreParsedScopeData::For(
          preparsed_scope_data_builder_, main_zone());
    }

    if (pending_error_handler()->has_error_unidentifiable_by_preparser()) {
      return kPreParseNotIdentifiableError;
    }

    if (is_strict(function_scope->language_mode())) {
      int end_pos = scanner()->location().end_pos;
      CheckStrictOctalLiteral(function_scope->start_position(), end_pos);
    }
  }

  DCHECK(!pending_error_handler()->has_error_unidentifiable_by_preparser());
  return kPreParseSuccess;
}


// Preparsing checks a JavaScript program and emits preparse-data that helps
// a later parsing to be faster.
// See preparser-data.h for the data.

// The PreParser checks that the syntax follows the grammar for JavaScript,
// and collects some information about the program along the way.
// The grammar check is only performed in order to understand the program
// sufficiently to deduce some information about it, that can be used
// to speed up later parsing. Finding errors is not the goal of pre-parsing,
// rather it is to speed up properly written and correct programs.
// That means that contextual checks (like a label being declared where
// it is used) are generally omitted.

PreParser::Expression PreParser::ParseFunctionLiteral(
    Identifier function_name, Scanner::Location function_name_location,
    FunctionNameValidity function_name_validity, FunctionKind kind,
    int function_token_pos, FunctionLiteral::FunctionType function_type,
    LanguageMode language_mode,
    ZonePtrList<const AstRawString>* arguments_for_wrapped_function) {
  // Wrapped functions are not parsed in the preparser.
  DCHECK_NULL(arguments_for_wrapped_function);
  DCHECK_NE(FunctionLiteral::kWrapped, function_type);
  // Function ::
  //   '(' FormalParameterList? ')' '{' FunctionBody '}'
  const RuntimeCallCounterId counters[2] = {
      RuntimeCallCounterId::kPreParseBackgroundWithVariableResolution,
      RuntimeCallCounterId::kPreParseWithVariableResolution};
  RuntimeCallTimerScope runtime_timer(runtime_call_stats_,
                                      counters[parsing_on_main_thread_]);

  base::ElapsedTimer timer;
  if (V8_UNLIKELY(FLAG_log_function_events)) timer.Start();

  DeclarationScope* function_scope = NewFunctionScope(kind);
  function_scope->SetLanguageMode(language_mode);
  int func_id = GetNextFunctionLiteralId();
  bool skippable_function = false;

  // Start collecting data for a new function which might contain skippable
  // functions.
  {
    std::unique_ptr<PreParsedScopeDataBuilder::DataGatheringScope>
        preparsed_scope_data_builder_scope;
    if (!function_state_->next_function_is_likely_called() &&
        preparsed_scope_data_builder_ != nullptr) {
      skippable_function = true;
      preparsed_scope_data_builder_scope.reset(
          new PreParsedScopeDataBuilder::DataGatheringScope(function_scope,
                                                            this));
    }

    FunctionState function_state(&function_state_, &scope_, function_scope);
    ExpressionClassifier formals_classifier(this);

    Expect(Token::LPAREN);
    int start_position = position();
    function_scope->set_start_position(start_position);
    PreParserFormalParameters formals(function_scope);
    ParseFormalParameterList(&formals);
    Expect(Token::RPAREN);
    int formals_end_position = scanner()->location().end_pos;

    CheckArityRestrictions(formals.arity, kind, formals.has_rest,
                           start_position, formals_end_position);

    Expect(Token::LBRACE);

    // Parse function body.
    PreParserScopedStatementList body(pointer_buffer());
    int pos = function_token_pos == kNoSourcePosition ? peek_position()
                                                      : function_token_pos;
    AcceptINScope scope(this, true);
    ParseFunctionBody(&body, function_name, pos, formals, kind, function_type,
                      FunctionBodyType::kBlock);

    // Parsing the body may change the language mode in our scope.
    language_mode = function_scope->language_mode();

    if (is_sloppy(language_mode)) {
      function_scope->HoistSloppyBlockFunctions(nullptr);
    }

    // Validate name and parameter names. We can do this only after parsing the
    // function, since the function can declare itself strict.
    CheckFunctionName(language_mode, function_name, function_name_validity,
                      function_name_location);

    if (is_strict(language_mode)) {
      CheckStrictOctalLiteral(start_position, end_position());
    }
  }

  if (skippable_function) {
    preparsed_scope_data_builder_->AddSkippableFunction(
        function_scope->start_position(), end_position(),
        function_scope->num_parameters(), GetLastFunctionLiteralId() - func_id,
        function_scope->language_mode(), function_scope->NeedsHomeObject());
  }

  if (V8_UNLIKELY(FLAG_log_function_events)) {
    double ms = timer.Elapsed().InMillisecondsF();
    const char* event_name = "preparse-resolution";
    // We might not always get a function name here. However, it can be easily
    // reconstructed from the script id and the byte range in the log processor.
    const char* name = "";
    size_t name_byte_length = 0;
    const AstRawString* string = function_name.string_;
    if (string != nullptr) {
      name = reinterpret_cast<const char*>(string->raw_data());
      name_byte_length = string->byte_length();
    }
    logger_->FunctionEvent(
        event_name, script_id(), ms, function_scope->start_position(),
        function_scope->end_position(), name, name_byte_length);
  }

  return Expression::Default();
}

PreParser::LazyParsingResult PreParser::ParseStatementListAndLogFunction(
    PreParserFormalParameters* formals, bool may_abort) {
  PreParserScopedStatementList body(pointer_buffer());
  LazyParsingResult result =
      ParseStatementList(&body, Token::RBRACE, may_abort);
  if (result == kLazyParsingAborted) return result;

  // Position right after terminal '}'.
  DCHECK_IMPLIES(!has_error(), scanner()->peek() == Token::RBRACE);
  int body_end = scanner()->peek_location().end_pos;
  DCHECK_EQ(this->scope()->is_function_scope(), formals->is_simple);
  log_.LogFunction(body_end, formals->num_parameters(),
                   GetLastFunctionLiteralId());
  return kLazyParsingComplete;
}

PreParserStatement PreParser::BuildParameterInitializationBlock(
    const PreParserFormalParameters& parameters) {
  DCHECK(!parameters.is_simple);
  DCHECK(scope()->is_function_scope());
  if (scope()->AsDeclarationScope()->calls_sloppy_eval() &&
      preparsed_scope_data_builder_ != nullptr) {
    // We cannot replicate the Scope structure constructed by the Parser,
    // because we've lost information whether each individual parameter was
    // simple or not. Give up trying to produce data to skip inner functions.
    if (preparsed_scope_data_builder_->parent() != nullptr) {
      // Lazy parsing started before the current function; the function which
      // cannot contain skippable functions is the parent function. (Its inner
      // functions cannot either; they are implicitly bailed out.)
      preparsed_scope_data_builder_->parent()->Bailout();
    } else {
      // Lazy parsing started at the current function; it cannot contain
      // skippable functions.
      preparsed_scope_data_builder_->Bailout();
    }
  }

  return PreParserStatement::Default();
}

bool PreParser::IdentifierEquals(const PreParserIdentifier& identifier,
                                 const AstRawString* other) {
  return identifier.string_ == other;
}

PreParserExpression PreParser::ExpressionFromIdentifier(
    const PreParserIdentifier& name, int start_position, InferName infer) {
  VariableProxy* proxy = nullptr;
  DCHECK_EQ(name.string_ == nullptr, has_error());
  if (name.string_ == nullptr) return PreParserExpression::Default();
  proxy = scope()->NewUnresolved(factory()->ast_node_factory(), name.string_,
                                 start_position, NORMAL_VARIABLE);
  return PreParserExpression::FromIdentifier(name, proxy, zone());
}

void PreParser::DeclareAndInitializeVariables(
    PreParserStatement block,
    const DeclarationDescriptor* declaration_descriptor,
    const DeclarationParsingResult::Declaration* declaration,
    ZonePtrList<const AstRawString>* names) {
  if (declaration->pattern.variables_ != nullptr) {
    for (auto variable : *(declaration->pattern.variables_)) {
      declaration_descriptor->scope->DeleteUnresolved(variable);
      Variable* var = scope()->DeclareVariableName(
          variable->raw_name(), declaration_descriptor->mode);
      MarkLoopVariableAsAssigned(declaration_descriptor->scope, var,
                                 declaration_descriptor->declaration_kind);
      // This is only necessary if there is an initializer, but we don't have
      // that information here.  Consequently, the preparser sometimes says
      // maybe-assigned where the parser (correctly) says never-assigned.
      if (names) {
        names->Add(variable->raw_name(), zone());
      }
    }
  }
}

}  // namespace internal
}  // namespace v8
