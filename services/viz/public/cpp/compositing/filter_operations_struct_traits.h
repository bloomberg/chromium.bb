// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_FILTER_OPERATIONS_STRUCT_TRAITS_H_
#define SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_FILTER_OPERATIONS_STRUCT_TRAITS_H_

#include "cc/paint/filter_operations.h"
#include "services/viz/public/interfaces/compositing/filter_operations.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<viz::mojom::FilterOperationsDataView,
                    cc::FilterOperations> {
  static const std::vector<cc::FilterOperation>& operations(
      const cc::FilterOperations& operations) {
    return operations.operations();
  }

  static bool Read(viz::mojom::FilterOperationsDataView data,
                   cc::FilterOperations* out) {
    std::vector<cc::FilterOperation> operations;
    if (!data.ReadOperations(&operations))
      return false;
    *out = cc::FilterOperations(std::move(operations));
    return true;
  }
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_FILTER_OPERATIONS_STRUCT_TRAITS_H_
