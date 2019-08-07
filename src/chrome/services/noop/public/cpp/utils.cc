// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/noop/public/cpp/utils.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "chrome/services/noop/public/mojom/noop.mojom.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"

namespace chrome {
namespace {

// If enabled, starts a service that does nothing. This will be used to analyze
// memory usage of starting an extra process.
const base::Feature kNoopService{"NoopService",
                                 base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace

void MaybeStartNoopService() {
  if (!IsNoopServiceEnabled())
    return;

  static mojom::NoopPtr* noop_ptr = new mojom::NoopPtr;
  if (!noop_ptr->is_bound() || noop_ptr->encountered_error()) {
    content::ServiceManagerConnection::GetForProcess()
        ->GetConnector()
        ->BindInterface(mojom::kNoopServiceName, noop_ptr);
    noop_ptr->set_connection_error_handler(
        base::BindOnce(&MaybeStartNoopService));
  }
}

bool IsNoopServiceEnabled() {
  return base::FeatureList::IsEnabled(kNoopService);
}

}  // namespace chrome
