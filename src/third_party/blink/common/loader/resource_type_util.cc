// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/loader/resource_type_util.h"

namespace blink {

bool IsResourceTypeFrame(blink::mojom::ResourceType type) {
  return type == blink::mojom::ResourceType::kMainFrame ||
         type == blink::mojom::ResourceType::kSubFrame;
}

bool IsRequestDestinationFrame(network::mojom::RequestDestination destination) {
  return destination == network::mojom::RequestDestination::kDocument ||
         destination == network::mojom::RequestDestination::kFrame ||
         destination == network::mojom::RequestDestination::kIframe;
}

}  // namespace blink
