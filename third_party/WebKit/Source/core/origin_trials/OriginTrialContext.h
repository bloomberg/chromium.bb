// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OriginTrialContext_h
#define OriginTrialContext_h

#include "core/CoreExport.h"
#include "core/dom/ExecutionContext.h"
#include "platform/Supplementable.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/StringHash.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/WebTrialTokenValidator.h"
#include "third_party/WebKit/common/origin_trials/trial_token_validator.h"

namespace blink {

class ExecutionContext;
enum class WebOriginTrialTokenStatus;
class WebTrialTokenValidator;

// The Origin Trials Framework provides limited access to experimental features,
// on a per-origin basis (origin trials). This class provides the implementation
// to check if the experimental feature should be enabled for the current
// context.  This class is not for direct use by feature implementers.
// Instead, the OriginTrials generated namespace provides a method for each
// trial to check if it is enabled. Experimental features must be defined in
// runtime_enabled_features.json5, which is used to generate origin_trials.h/cc.
//
// Origin trials are defined by string names, provided by the implementers. The
// framework does not maintain an enum or constant list for trial names.
// Instead, the name provided by the feature implementation is validated against
// any provided tokens.
//
// For more information, see https://github.com/jpchase/OriginTrials.
class CORE_EXPORT OriginTrialContext final
    : public GarbageCollectedFinalized<OriginTrialContext>,
      public Supplement<ExecutionContext> {
  USING_GARBAGE_COLLECTED_MIXIN(OriginTrialContext)
 public:
  enum CreateMode { kCreateIfNotExists, kDontCreateIfNotExists };

  OriginTrialContext(ExecutionContext&,
                     std::unique_ptr<WebTrialTokenValidator>);

  static const char* SupplementName();

  // Returns the OriginTrialContext for a specific ExecutionContext. If
  // |create| is false, this returns null if no OriginTrialContext exists
  // yet for the ExecutionContext.
  static OriginTrialContext* From(ExecutionContext*,
                                  CreateMode = kCreateIfNotExists);

  // Parses an Origin-Trial header as specified in
  // https://jpchase.github.io/OriginTrials/#header into individual tokens.
  // Returns null if the header value was malformed and could not be parsed.
  // If the header does not contain any tokens, this returns an empty vector.
  static std::unique_ptr<Vector<String>> ParseHeaderValue(
      const String& header_value);

  static void AddTokensFromHeader(ExecutionContext*,
                                  const String& header_value);
  static void AddTokens(ExecutionContext*, const Vector<String>* tokens);

  // Returns the trial tokens that are active in a specific ExecutionContext.
  // Returns null if no tokens were added to the ExecutionContext.
  static std::unique_ptr<Vector<String>> GetTokens(ExecutionContext*);

  void AddToken(const String& token);
  void AddTokens(const Vector<String>& tokens);

  // Forces a given origin-trial-enabled feature to be enabled in this context
  // and immediately adds required bindings to already initialized JS contexts.
  void AddFeature(const String& feature);

  // Returns true if the trial (and therefore the feature or features it
  // controls) should be considered enabled for the current execution context.
  bool IsTrialEnabled(const String& trial_name);

  // Installs JavaScript bindings on the relevant objects for any features which
  // should be enabled by the current set of trial tokens. This method is called
  // every time a token is added to the document (including when tokens are
  // added via script). JavaScript-exposed members will be properly visible, for
  // existing objects in the V8 context. If the V8 context is not initialized,
  // or there are no enabled features, or all enabled features are already
  // initialized, this method returns without doing anything. That is, it is
  // safe to call this method multiple times, even if no trials are newly
  // enabled.
  void InitializePendingFeatures();

  DECLARE_VIRTUAL_TRACE();

 private:
  // Validate the trial token. If valid, the trial named in the token is
  // added to the list of enabled trials. Returns true or false to indicate if
  // the token is valid.
  bool EnableTrialFromToken(const String& token);

  Vector<String> tokens_;
  HashSet<String> enabled_trials_;
  HashSet<String> installed_trials_;
  std::unique_ptr<WebTrialTokenValidator> trial_token_validator_;
};

}  // namespace blink

#endif  // OriginTrialContext_h
