// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_PAINT_FILTER_STRUCT_TRAITS_H_
#define SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_PAINT_FILTER_STRUCT_TRAITS_H_

#include <vector>

#include "cc/paint/paint_filter.h"
#include "cc/paint/paint_op_reader.h"
#include "cc/paint/paint_op_writer.h"
#include "services/viz/public/interfaces/compositing/paint_filter.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<viz::mojom::PaintFilterDataView, sk_sp<cc::PaintFilter>> {
  static base::Optional<std::vector<uint8_t>> data(
      const sk_sp<cc::PaintFilter>& filter) {
    std::vector<uint8_t> memory;
    memory.resize(cc::PaintOpWriter::HeaderBytes() +
                  cc::PaintFilter::GetFilterSize(filter.get()));
    cc::PaintOpWriter writer(memory.data(), memory.size(), nullptr, nullptr,
                             true /* enable_security_constraints */);
    writer.Write(filter.get());

    if (writer.size() == 0)
      return base::nullopt;

    memory.resize(writer.size());
    return memory;
  }

  static bool Read(viz::mojom::PaintFilterDataView data,
                   sk_sp<cc::PaintFilter>* out) {
    base::Optional<std::vector<uint8_t>> buffer;
    if (!data.ReadData(&buffer))
      return false;

    if (!buffer) {
      // We may fail to serialize the filter if it doesn't fit in kBufferSize
      // above, use an empty filter instead of rejecting the message.
      *out = nullptr;
      return true;
    }

    cc::PaintOpReader reader(buffer->data(), buffer->size(), nullptr,
                             true /* enable_security_constraints */);
    sk_sp<cc::PaintFilter> filter;
    reader.Read(&filter);
    if (!reader.valid()) {
      *out = nullptr;
      return false;
    }

    // We must have consumed all bytes writen when reading this filter.
    if (reader.remaining_bytes() != 0u) {
      *out = nullptr;
      return false;
    }

    *out = std::move(filter);
    return true;
  }
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_PAINT_FILTER_STRUCT_TRAITS_H_
