// Copyright 2015 The Chromium Authors. All rights reserved.
// // TODO(https://crbug.com/738505): Delete the validator.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/origin_trials/OriginTrialContext.h"

#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/core/v8/WindowProxy.h"
#include "bindings/core/v8/WorkerOrWorkletScriptController.h"
#include "common/origin_trials/trial_token.h"
#include "common/origin_trials/trial_token_validator.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/LocalFrame.h"
#include "core/workers/WorkerGlobalScope.h"
#include "platform/Histogram.h"
#include "platform/bindings/ConditionalFeatures.h"
#include "platform/runtime_enabled_features.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/StringBuilder.h"
#include "public/platform/Platform.h"
#include "public/platform/WebSecurityOrigin.h"
#include "public/platform/WebTrialTokenValidator.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

static EnumerationHistogram& TokenValidationResultHistogram() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      EnumerationHistogram, histogram,
      ("OriginTrials.ValidationResult",
       static_cast<int>(OriginTrialTokenStatus::kLast)));
  return histogram;
}

bool IsWhitespace(UChar chr) {
  return (chr == ' ') || (chr == '\t');
}

bool SkipWhiteSpace(const String& str, unsigned& pos) {
  unsigned len = str.length();
  while (pos < len && IsWhitespace(str[pos]))
    ++pos;
  return pos < len;
}

// Extracts a quoted or unquoted token from an HTTP header. If the token was a
// quoted string, this also removes the quotes and unescapes any escaped
// characters. Also skips all whitespace before and after the token.
String ExtractTokenOrQuotedString(const String& header_value, unsigned& pos) {
  unsigned len = header_value.length();
  String result;
  if (!SkipWhiteSpace(header_value, pos))
    return String();

  if (header_value[pos] == '\'' || header_value[pos] == '"') {
    StringBuilder out;
    // Quoted string, append characters until matching quote is found,
    // unescaping as we go.
    UChar quote = header_value[pos++];
    while (pos < len && header_value[pos] != quote) {
      if (header_value[pos] == '\\')
        pos++;
      if (pos < len)
        out.Append(header_value[pos++]);
    }
    if (pos < len)
      pos++;
    result = out.ToString();
  } else {
    // Unquoted token. Consume all characters until whitespace or comma.
    int start_pos = pos;
    while (pos < len && !IsWhitespace(header_value[pos]) &&
           header_value[pos] != ',')
      pos++;
    result = header_value.Substring(start_pos, pos - start_pos);
  }
  SkipWhiteSpace(header_value, pos);
  return result;
}

}  // namespace

OriginTrialContext::OriginTrialContext(
    ExecutionContext& context,
    std::unique_ptr<WebTrialTokenValidator> validator)
    : Supplement<ExecutionContext>(context),
      trial_token_validator_(std::move(validator)) {}

// static
const char* OriginTrialContext::SupplementName() {
  return "OriginTrialContext";
}

// static
OriginTrialContext* OriginTrialContext::From(ExecutionContext* context,
                                             CreateMode create) {
  OriginTrialContext* origin_trials = static_cast<OriginTrialContext*>(
      Supplement<ExecutionContext>::From(context, SupplementName()));
  if (!origin_trials && create == kCreateIfNotExists) {
    origin_trials = new OriginTrialContext(
        *context, Platform::Current()->TrialTokenValidator());
    Supplement<ExecutionContext>::ProvideTo(*context, SupplementName(),
                                            origin_trials);
  }
  return origin_trials;
}

// static
std::unique_ptr<Vector<String>> OriginTrialContext::ParseHeaderValue(
    const String& header_value) {
  std::unique_ptr<Vector<String>> tokens(new Vector<String>);
  unsigned pos = 0;
  unsigned len = header_value.length();
  while (pos < len) {
    String token = ExtractTokenOrQuotedString(header_value, pos);
    if (!token.IsEmpty())
      tokens->push_back(token);
    // Make sure tokens are comma-separated.
    if (pos < len && header_value[pos++] != ',')
      return nullptr;
  }
  return tokens;
}

// static
void OriginTrialContext::AddTokensFromHeader(ExecutionContext* context,
                                             const String& header_value) {
  if (header_value.IsEmpty())
    return;
  std::unique_ptr<Vector<String>> tokens(ParseHeaderValue(header_value));
  if (!tokens)
    return;
  AddTokens(context, tokens.get());
}

// static
void OriginTrialContext::AddTokens(ExecutionContext* context,
                                   const Vector<String>* tokens) {
  if (!tokens || tokens->IsEmpty())
    return;
  From(context)->AddTokens(*tokens);
}

// static
std::unique_ptr<Vector<String>> OriginTrialContext::GetTokens(
    ExecutionContext* execution_context) {
  OriginTrialContext* context = From(execution_context, kDontCreateIfNotExists);
  if (!context || context->tokens_.IsEmpty())
    return nullptr;
  return std::unique_ptr<Vector<String>>(new Vector<String>(context->tokens_));
}

void OriginTrialContext::AddToken(const String& token) {
  if (token.IsEmpty())
    return;
  tokens_.push_back(token);
  if (EnableTrialFromToken(token)) {
    // Only install pending features if the provided token is valid. Otherwise,
    // there was no change to the list of enabled features.
    InitializePendingFeatures();
  }
}

void OriginTrialContext::AddTokens(const Vector<String>& tokens) {
  if (tokens.IsEmpty())
    return;
  bool found_valid = false;
  for (const String& token : tokens) {
    if (!token.IsEmpty()) {
      tokens_.push_back(token);
      if (EnableTrialFromToken(token))
        found_valid = true;
    }
  }
  if (found_valid) {
    // Only install pending features if at least one of the provided tokens are
    // valid. Otherwise, there was no change to the list of enabled features.
    InitializePendingFeatures();
  }
}

void OriginTrialContext::InitializePendingFeatures() {
  if (!enabled_trials_.size())
    return;
  if (!GetSupplementable()->IsDocument())
    return;
  LocalFrame* frame = ToDocument(GetSupplementable())->GetFrame();
  if (!frame)
    return;
  ScriptState* script_state = ToScriptStateForMainWorld(frame);
  if (!script_state)
    return;
  if (!script_state->ContextIsValid())
    return;
  ScriptState::Scope scope(script_state);
  for (auto enabled_trial : enabled_trials_) {
    if (installed_trials_.Contains(enabled_trial))
      continue;
    InstallPendingConditionalFeature(enabled_trial, script_state);
    installed_trials_.insert(enabled_trial);
  }
}

void OriginTrialContext::AddFeature(const String& feature) {
  enabled_trials_.insert(feature);
  InitializePendingFeatures();
}

bool OriginTrialContext::IsTrialEnabled(const String& trial_name) {
  if (!RuntimeEnabledFeatures::OriginTrialsEnabled())
    return false;

  return enabled_trials_.Contains(trial_name);
}

bool OriginTrialContext::EnableTrialFromToken(const String& token) {
  DCHECK(!token.IsEmpty());

  // Origin trials are only enabled for secure origins
  if (!GetSupplementable()->IsSecureContext()) {
    TokenValidationResultHistogram().Count(
        static_cast<int>(OriginTrialTokenStatus::kInsecure));
    return false;
  }

  if (!trial_token_validator_) {
    TokenValidationResultHistogram().Count(
        static_cast<int>(OriginTrialTokenStatus::kNotSupported));
    return false;
  }

  WebSecurityOrigin origin(GetSupplementable()->GetSecurityOrigin());
  WebString trial_name;
  bool valid = false;
  OriginTrialTokenStatus token_result =
      trial_token_validator_->ValidateToken(token, origin, &trial_name);
  if (token_result == OriginTrialTokenStatus::kSuccess) {
    valid = true;
    enabled_trials_.insert(trial_name);
  }

  TokenValidationResultHistogram().Count(static_cast<int>(token_result));
  return valid;
}

DEFINE_TRACE(OriginTrialContext) {
  Supplement<ExecutionContext>::Trace(visitor);
}

}  // namespace blink
