// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/can_make_payment_query.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "components/payments/core/features.h"
#include "url/gurl.h"

#include "base/logging.h"

namespace payments {
namespace {

static const char* const kBasicCardNetworks[] = {
    "amex",       "diners", "discover", "jcb",
    "mastercard", "mir",    "unionpay", "visa"};

// Performs the simple normalization of card network |method| ("visa", "amex",
// etc.) into a "basic-card" |method| with "{supportedNetworks: [method]}"
// |params|.
void SimpleNormalize(std::string* method, std::set<std::string>* params) {
  params->erase("");
  for (size_t i = 0; i < base::size(kBasicCardNetworks); ++i) {
    if (std::strcmp(kBasicCardNetworks[i], method->c_str()) == 0) {
      params->insert(base::StringPrintf("{\"supportedNetworks\":[\"%s\"]}",
                                        method->c_str()));
      method->assign("basic-card");
      return;
    }
  }
}

}  // namespace

CanMakePaymentQuery::CanMakePaymentQuery() {}

CanMakePaymentQuery::~CanMakePaymentQuery() {}

bool CanMakePaymentQuery::CanQuery(
    const GURL& top_level_origin,
    const GURL& frame_origin,
    const std::map<std::string, std::set<std::string>>& query) {
  if (base::FeatureList::IsEnabled(
          features::kWebPaymentsPerMethodCanMakePaymentQuota)) {
    bool can_query = true;
    for (const auto& method_and_params : query) {
      std::string method = method_and_params.first;
      std::set<std::string> params = method_and_params.second;
      SimpleNormalize(&method, &params);

      const std::string id =
          frame_origin.spec() + ":" + top_level_origin.spec() + ":" + method;

      auto it = per_method_queries_.find(id);
      if (it == per_method_queries_.end()) {
        auto timer = std::make_unique<base::OneShotTimer>();
        timer->Start(
            FROM_HERE, base::TimeDelta::FromMinutes(30),
            base::BindOnce(
                &CanMakePaymentQuery::ExpireQuotaForFrameOriginAndMethod,
                base::Unretained(this), id));
        timers_.insert(std::make_pair(id, std::move(timer)));
        per_method_queries_.insert(std::make_pair(id, params));
        continue;
      }

      can_query &= it->second == params;
    }

    return can_query;
  } else {
    const std::string id = frame_origin.spec() + ":" + top_level_origin.spec();

    const auto& it = queries_.find(id);
    if (it == queries_.end()) {
      auto timer = std::make_unique<base::OneShotTimer>();
      timer->Start(
          FROM_HERE, base::TimeDelta::FromMinutes(30),
          base::BindOnce(&CanMakePaymentQuery::ExpireQuotaForFrameOrigin,
                         base::Unretained(this), id));
      timers_.insert(std::make_pair(id, std::move(timer)));
      queries_.insert(std::make_pair(id, query));
      return true;
    }

    return it->second == query;
  }
}

void CanMakePaymentQuery::ExpireQuotaForFrameOrigin(const std::string& id) {
  timers_.erase(id);
  queries_.erase(id);
}

void CanMakePaymentQuery::ExpireQuotaForFrameOriginAndMethod(
    const std::string& id) {
  timers_.erase(id);
  per_method_queries_.erase(id);
}

}  // namespace payments
