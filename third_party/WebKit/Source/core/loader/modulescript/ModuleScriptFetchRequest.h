// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_MODULESCRIPT_MODULE_SCRIPT_FETCH_REQUEST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_MODULESCRIPT_MODULE_SCRIPT_FETCH_REQUEST_H_

#include "third_party/blink/renderer/platform/loader/fetch/script_fetch_options.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/referrer.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// A ModuleScriptFetchRequest essentially serves as a "parameter object" for
// Modulator::Fetch{Tree,Single,NewSingle}.
class ModuleScriptFetchRequest final {
  STACK_ALLOCATED();

 public:
  ModuleScriptFetchRequest(const KURL& url,
                           ReferrerPolicy referrer_policy,
                           const ScriptFetchOptions& options)
      : ModuleScriptFetchRequest(url,
                                 options,
                                 Referrer::NoReferrer(),
                                 referrer_policy,
                                 TextPosition::MinimumPosition()) {}
  ~ModuleScriptFetchRequest() = default;

  const KURL& Url() const { return url_; }
  const ScriptFetchOptions& Options() const { return options_; }
  const AtomicString& GetReferrer() const { return referrer_; }
  ReferrerPolicy GetReferrerPolicy() const { return referrer_policy_; }
  const TextPosition& GetReferrerPosition() const { return referrer_position_; }

 private:
  // Referrer is set only for internal module script fetch algorithms triggered
  // from ModuleTreeLinker to fetch descendant module scripts.
  friend class ModuleTreeLinker;
  ModuleScriptFetchRequest(const KURL& url,
                           const ScriptFetchOptions& options,
                           const String& referrer,
                           ReferrerPolicy referrer_policy,
                           const TextPosition& referrer_position)
      : url_(url),
        options_(options),
        referrer_(referrer),
        referrer_policy_(referrer_policy),
        referrer_position_(referrer_position) {}

  const KURL url_;
  const ScriptFetchOptions options_;
  const AtomicString referrer_;
  const ReferrerPolicy referrer_policy_;
  const TextPosition referrer_position_;
};

}  // namespace blink

#endif
