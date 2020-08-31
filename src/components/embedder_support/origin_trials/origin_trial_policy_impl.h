// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EMBEDDER_SUPPORT_ORIGIN_TRIALS_ORIGIN_TRIAL_POLICY_IMPL_H_
#define COMPONENTS_EMBEDDER_SUPPORT_ORIGIN_TRIALS_ORIGIN_TRIAL_POLICY_IMPL_H_

#include <set>
#include <string>

#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "third_party/blink/public/common/origin_trials/origin_trial_policy.h"

namespace embedder_support {

// This class is instantiated on the main/ui thread, but its methods can be
// accessed from any thread.
class OriginTrialPolicyImpl : public blink::OriginTrialPolicy {
 public:
  OriginTrialPolicyImpl();
  ~OriginTrialPolicyImpl() override;

  // blink::OriginTrialPolicy interface
  bool IsOriginTrialsSupported() const override;
  std::vector<base::StringPiece> GetPublicKeys() const override;
  bool IsFeatureDisabled(base::StringPiece feature) const override;
  bool IsTokenDisabled(base::StringPiece token_signature) const override;
  bool IsOriginSecure(const GURL& url) const override;

  bool SetPublicKeysFromASCIIString(const std::string& ascii_public_key);
  bool SetDisabledFeatures(const std::string& disabled_feature_list);
  bool SetDisabledTokens(const std::string& disabled_token_list);

 private:
  std::vector<std::string> public_keys_;
  std::set<std::string> disabled_features_;
  std::set<std::string> disabled_tokens_;

  DISALLOW_COPY_AND_ASSIGN(OriginTrialPolicyImpl);
};

}  // namespace embedder_support

#endif  // COMPONENTS_EMBEDDER_SUPPORT_ORIGIN_TRIALS_ORIGIN_TRIAL_POLICY_IMPL_H_
