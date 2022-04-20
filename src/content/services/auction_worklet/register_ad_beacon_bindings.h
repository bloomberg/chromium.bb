// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_REGISTER_AD_BEACON_BINDINGS_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_REGISTER_AD_BEACON_BINDINGS_H_

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "v8/include/v8-forward.h"

namespace auction_worklet {

// Class to manage bindings for setting ad beacon URLs. Expected to be
// used for a short-lived v8::Context. Allows only a single call for the ad
// beacon reporting map. On any subequent calls registerAdBeacon throws an
// exception and keeps the previous ad_beacon_map_ state. registerAdBeacon also
// throws on invalid URLs or non-HTTPS URLs in the map.
class RegisterAdBeaconBindings {
 public:
  // Add registerAdBeaconBindings object to `global_template`. The
  // RegisterAdBeaconBindings must outlive the template.
  RegisterAdBeaconBindings(AuctionV8Helper* v8_helper,
                           v8::Local<v8::ObjectTemplate> global_template);
  RegisterAdBeaconBindings(const RegisterAdBeaconBindings&) = delete;
  RegisterAdBeaconBindings& operator=(const RegisterAdBeaconBindings&) = delete;
  ~RegisterAdBeaconBindings();

  base::flat_map<std::string, GURL> TakeAdBeaconMap() {
    return std::move(ad_beacon_map_);
  }

 private:
  static void RegisterAdBeacon(const v8::FunctionCallbackInfo<v8::Value>& args);

  const raw_ptr<AuctionV8Helper> v8_helper_;

  // This is a map from the event type to the reporting url.
  base::flat_map<std::string, GURL> ad_beacon_map_;

  bool first_call_ = true;
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_REGISTER_AD_BEACON_BINDINGS_H_
