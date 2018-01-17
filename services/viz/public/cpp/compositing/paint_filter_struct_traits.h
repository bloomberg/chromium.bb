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

struct PaintFilterBuffer {
  PaintFilterBuffer();
  PaintFilterBuffer(const PaintFilterBuffer& other);
  ~PaintFilterBuffer();
  std::vector<uint8_t> data;
};

template <>
struct ArrayTraits<PaintFilterBuffer> {
  using Element = uint8_t;
  static size_t GetSize(const PaintFilterBuffer& b);
  static uint8_t* GetData(PaintFilterBuffer& b);
  static const uint8_t* GetData(const PaintFilterBuffer& b);
  static uint8_t& GetAt(PaintFilterBuffer& b, size_t i);
  static const uint8_t& GetAt(const PaintFilterBuffer& b, size_t i);
  static bool Resize(PaintFilterBuffer& b, size_t size);
};

template <>
struct StructTraits<viz::mojom::PaintFilterDataView, sk_sp<cc::PaintFilter>> {
  static PaintFilterBuffer data(const sk_sp<cc::PaintFilter>& filter) {
    PaintFilterBuffer buffer;
    static const size_t kBufferSize = 8 * 1024;
    buffer.data.resize(kBufferSize);
    cc::PaintOpWriter writer(buffer.data.data(), kBufferSize, nullptr, nullptr,
                             true /* enable_security_constraints */);
    writer.Write(filter.get());
    if (writer.size() == 0)
      buffer.data.clear();
    else
      buffer.data.resize(writer.size());
    return buffer;
  }

  static bool Read(viz::mojom::PaintFilterDataView data,
                   sk_sp<cc::PaintFilter>* out) {
    PaintFilterBuffer buffer;
    if (!data.ReadData(&buffer))
      return false;

    cc::PaintOpReader reader(buffer.data.data(), buffer.data.size(), nullptr,
                             true /* enable_security_constraints */);
    sk_sp<cc::PaintFilter> filter;
    reader.Read(&filter);
    if (!filter) {
      *out = nullptr;
      return false;
    }

    *out = std::move(filter);
    return true;
  }
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_PAINT_FILTER_STRUCT_TRAITS_H_
