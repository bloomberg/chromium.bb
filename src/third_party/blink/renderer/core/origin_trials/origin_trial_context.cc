// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"

#include "base/time/time.h"
#include "third_party/blink/public/common/origin_trials/trial_token.h"
#include "third_party/blink/public/common/origin_trials/trial_token_validator.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/window_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trials.h"
#include "third_party/blink/renderer/core/workers/worklet_global_scope.h"
#include "third_party/blink/renderer/platform/bindings/origin_trial_features.h"
#include "third_party/blink/renderer/platform/histogram.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
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
    std::unique_ptr<TrialTokenValidator> validator)
    : Supplement<ExecutionContext>(context),
      trial_token_validator_(std::move(validator)) {}

// static
const char OriginTrialContext::kSupplementName[] = "OriginTrialContext";

// static
const OriginTrialContext* OriginTrialContext::From(
    const ExecutionContext* context) {
  return Supplement<ExecutionContext>::From<OriginTrialContext>(context);
}

// static
OriginTrialContext* OriginTrialContext::FromOrCreate(
    ExecutionContext* context) {
  OriginTrialContext* origin_trials =
      Supplement<ExecutionContext>::From<OriginTrialContext>(context);
  if (!origin_trials) {
    origin_trials = MakeGarbageCollected<OriginTrialContext>(
        *context, TrialTokenValidator::Policy()
                      ? std::make_unique<TrialTokenValidator>()
                      : nullptr);
    Supplement<ExecutionContext>::ProvideTo(*context, origin_trials);
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
  FromOrCreate(context)->AddTokens(*tokens);
}

// static
std::unique_ptr<Vector<String>> OriginTrialContext::GetTokens(
    ExecutionContext* execution_context) {
  const OriginTrialContext* context = From(execution_context);
  if (!context || context->tokens_.IsEmpty())
    return nullptr;
  return std::make_unique<Vector<String>>(context->tokens_);
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
  if (!enabled_features_.size())
    return;
  auto* document = DynamicTo<Document>(GetSupplementable());
  if (!document)
    return;
  LocalFrame* frame = document->GetFrame();
  if (!frame)
    return;
  ScriptState* script_state = ToScriptStateForMainWorld(frame);
  if (!script_state)
    return;
  if (!script_state->ContextIsValid())
    return;
  ScriptState::Scope scope(script_state);
  for (OriginTrialFeature enabled_feature : enabled_features_) {
    if (installed_features_.Contains(enabled_feature))
      continue;
    InstallPendingOriginTrialFeature(enabled_feature, script_state);
    installed_features_.insert(enabled_feature);
  }
}

void OriginTrialContext::AddFeature(OriginTrialFeature feature) {
  enabled_features_.insert(feature);
  InitializePendingFeatures();
}

bool OriginTrialContext::IsFeatureEnabled(OriginTrialFeature feature) const {
  if (!RuntimeEnabledFeatures::OriginTrialsEnabled())
    return false;

  if (enabled_features_.Contains(feature))
    return true;

  // HTML imports do not have a browsing context, see:
  //  - Spec: https://w3c.github.io/webcomponents/spec/imports/#terminology
  //  - Spec issue: https://github.com/w3c/webcomponents/issues/197
  // For the purposes of origin trials, we consider imported documents to be
  // part of the master document. Thus, check if the trial is enabled in the
  // master document and use that result.
  auto* document = DynamicTo<Document>(GetSupplementable());
  if (!document || !document->IsHTMLImport())
    return false;

  const OriginTrialContext* context =
      OriginTrialContext::From(&document->MasterDocument());
  if (!context)
    return false;
  return context->IsFeatureEnabled(feature);
}

bool OriginTrialContext::EnableTrialFromToken(const String& token) {
  DCHECK(!token.IsEmpty());

  // Origin trials are only enabled for secure origins
  //  - For worklets, they are currently spec'd to not be secure, given their
  //    scope has unique origin:
  //    https://drafts.css-houdini.org/worklets/#script-settings-for-worklets
  //  - For the purpose of origin trials, we consider worklets as running in the
  //    same context as the originating document. Thus, the special logic here
  //    to validate the token against the document context.
  bool is_secure = false;
  ExecutionContext* context = GetSupplementable();
  if (auto* scope = DynamicTo<WorkletGlobalScope>(context)) {
    is_secure = scope->DocumentSecureContext();
  } else {
    is_secure = context->IsSecureContext();
  }
  if (!is_secure) {
    TokenValidationResultHistogram().Count(
        static_cast<int>(OriginTrialTokenStatus::kInsecure));
    return false;
  }

  if (!trial_token_validator_) {
    TokenValidationResultHistogram().Count(
        static_cast<int>(OriginTrialTokenStatus::kNotSupported));
    return false;
  }

  const SecurityOrigin* origin;
  if (auto* scope = DynamicTo<WorkletGlobalScope>(context))
    origin = scope->DocumentSecurityOrigin();
  else
    origin = context->GetSecurityOrigin();

  bool valid = false;
  StringUTF8Adaptor token_string(token);
  std::string trial_name_str;
  OriginTrialTokenStatus token_result = trial_token_validator_->ValidateToken(
      token_string.AsStringPiece(), origin->ToUrlOrigin(), &trial_name_str,
      base::Time::Now());
  if (token_result == OriginTrialTokenStatus::kSuccess) {
    String trial_name =
        String::FromUTF8(trial_name_str.data(), trial_name_str.size());
    if (origin_trials::IsTrialValid(trial_name)) {
      for (OriginTrialFeature feature :
           origin_trials::FeaturesForTrial(trial_name)) {
        if (origin_trials::FeatureEnabledForOS(feature)) {
          valid = true;
          enabled_features_.insert(feature);
          // Also enable any features implied by this feature.
          for (OriginTrialFeature implied_feature :
               origin_trials::GetImpliedFeatures(feature)) {
            enabled_features_.insert(implied_feature);
          }
        }
      }
    }
  }

  TokenValidationResultHistogram().Count(static_cast<int>(token_result));
  return valid;
}

void OriginTrialContext::Trace(blink::Visitor* visitor) {
  Supplement<ExecutionContext>::Trace(visitor);
}

}  // namespace blink
