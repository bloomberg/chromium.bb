// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_ORIGIN_POLICY_ORIGIN_POLICY_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_ORIGIN_POLICY_ORIGIN_POLICY_H_

#include <string>

#include "base/strings/string_piece.h"
#include "third_party/blink/common/common_export.h"

namespace blink {

class OriginPolicyParser;

class BLINK_COMMON_EXPORT OriginPolicy {
 public:
  ~OriginPolicy();

  // Create & parse the manifest.
  static std::unique_ptr<OriginPolicy> From(base::StringPiece);

  base::StringPiece GetContentSecurityPolicy() const;

 private:
  friend class OriginPolicyParser;

  OriginPolicy();

  std::string csp_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_ORIGIN_POLICY_ORIGIN_POLICY_H_
