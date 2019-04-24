// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/mojom/data_source_descriptor_mojom_traits.h"

#include <utility>

namespace mojo {
bool StructTraits<tracing::mojom::DataSourceRegistrationDataView,
                  perfetto::DataSourceDescriptor>::
    Read(tracing::mojom::DataSourceRegistrationDataView data,
         perfetto::DataSourceDescriptor* out) {
  std::string name;
  if (!data.ReadName(&name)) {
    return false;
  }
  out->set_name(name);
  out->set_will_notify_on_stop(data.will_notify_on_stop());
  return true;
}
}  // namespace mojo
