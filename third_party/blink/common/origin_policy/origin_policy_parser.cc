// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/common/origin_policy/origin_policy_parser.h"

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"

namespace blink {

OriginPolicyParser::OriginPolicyParser() {}
OriginPolicyParser::~OriginPolicyParser() {}

std::unique_ptr<OriginPolicy> OriginPolicyParser::Parse(
    base::StringPiece text) {
  if (text.empty())
    return nullptr;

  std::unique_ptr<base::Value> json = base::JSONReader::Read(text);
  if (!json || !json->is_dict())
    return nullptr;

  auto* headers = json->FindKey("headers");
  if (!headers || !headers->is_list())
    return nullptr;

  std::unique_ptr<OriginPolicy> manifest = base::WrapUnique(new OriginPolicy);
  for (auto& header : headers->GetList()) {
    if (!header.is_dict())
      continue;
    auto* name = header.FindKey("name");
    auto* value = header.FindKey("value");
    if (name && name->is_string() && value && value->is_string() &&
        name->GetString() == "Content-Security-Policy") {
      manifest->csp_ = value->GetString();
    }
  }

  return manifest;
}

}  // namespace blink
