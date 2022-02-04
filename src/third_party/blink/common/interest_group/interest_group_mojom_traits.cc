// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/interest_group/interest_group_mojom_traits.h"

#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"

namespace mojo {

bool StructTraits<
    blink::mojom::InterestGroupAdDataView,
    blink::InterestGroup::Ad>::Read(blink::mojom::InterestGroupAdDataView data,
                                    blink::InterestGroup::Ad* out) {
  if (!data.ReadRenderUrl(&out->render_url) ||
      !data.ReadMetadata(&out->metadata)) {
    return false;
  }
  return true;
}

bool StructTraits<blink::mojom::InterestGroupDataView, blink::InterestGroup>::
    Read(blink::mojom::InterestGroupDataView data, blink::InterestGroup* out) {
  if (!data.ReadExpiry(&out->expiry) || !data.ReadOwner(&out->owner) ||
      !data.ReadName(&out->name) || !data.ReadBiddingUrl(&out->bidding_url) ||
      !data.ReadBiddingWasmHelperUrl(&out->bidding_wasm_helper_url) ||
      !data.ReadUpdateUrl(&out->update_url) ||
      !data.ReadTrustedBiddingSignalsUrl(&out->trusted_bidding_signals_url) ||
      !data.ReadTrustedBiddingSignalsKeys(&out->trusted_bidding_signals_keys) ||
      !data.ReadUserBiddingSignals(&out->user_bidding_signals) ||
      !data.ReadAds(&out->ads) || !data.ReadAdComponents(&out->ad_components)) {
    return false;
  }
  return out->IsValid();
}

}  // namespace mojo
