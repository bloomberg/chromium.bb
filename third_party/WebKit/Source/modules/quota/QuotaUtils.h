// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef QuotaUtils_h
#define QuotaUtils_h

#include "third_party/WebKit/public/mojom/quota/quota_dispatcher_host.mojom-blink.h"

namespace blink {

class ExecutionContext;

void ConnectToQuotaDispatcherHost(ExecutionContext*,
                                  mojom::blink::QuotaDispatcherHostRequest);

}  // namespace blink

#endif  // QuotaUtils_h
