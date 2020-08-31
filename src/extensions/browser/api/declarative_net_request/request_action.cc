// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/request_action.h"

#include <utility>

#include "extensions/browser/api/declarative_net_request/flat/extension_ruleset_generated.h"
#include "extensions/browser/api/declarative_net_request/utils.h"

namespace extensions {
namespace declarative_net_request {
namespace {

namespace dnr_api = api::declarative_net_request;

// Helper to connvert a flat::HeaderOperation to an
// api::declarative_net_request::HeaderOperation.
dnr_api::HeaderOperation ConvertFlatHeaderOperation(
    flat::HeaderOperation operation) {
  switch (operation) {
    case flat::HeaderOperation_remove:
      return dnr_api::HEADER_OPERATION_REMOVE;
  }
}

}  // namespace

RequestAction::HeaderInfo::HeaderInfo(std::string header,
                                      dnr_api::HeaderOperation operation)
    : header(std::move(header)), operation(operation) {}

RequestAction::HeaderInfo::HeaderInfo(const flat::ModifyHeaderInfo& info)
    : header(CreateString<std::string>(*info.header())),
      operation(ConvertFlatHeaderOperation(info.operation())) {}

RequestAction::RequestAction(RequestAction::Type type,
                             uint32_t rule_id,
                             uint64_t index_priority,
                             RulesetID ruleset_id,
                             const ExtensionId& extension_id)
    : type(type),
      rule_id(rule_id),
      index_priority(index_priority),
      ruleset_id(ruleset_id),
      extension_id(extension_id) {}
RequestAction::~RequestAction() = default;
RequestAction::RequestAction(RequestAction&&) = default;
RequestAction& RequestAction::operator=(RequestAction&&) = default;

RequestAction RequestAction::Clone() const {
  // Use the private copy constructor to create a copy.
  return *this;
}

RequestAction::RequestAction(const RequestAction&) = default;

base::Optional<RequestAction> GetMaxPriorityAction(
    base::Optional<RequestAction> lhs,
    base::Optional<RequestAction> rhs) {
  if (!lhs)
    return rhs;
  if (!rhs)
    return lhs;
  return lhs->index_priority >= rhs->index_priority ? std::move(lhs)
                                                    : std::move(rhs);
}

}  // namespace declarative_net_request
}  // namespace extensions
