// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_NETWORK_WRAPPED_DATA_PIPE_GETTER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_NETWORK_WRAPPED_DATA_PIPE_GETTER_H_

#include "third_party/blink/renderer/platform/platform_export.h"

#include "services/network/public/mojom/data_pipe_getter.mojom-blink.h"

namespace blink {

// Refcounted wrapper around a DataPipeGetter to allow sharing the move-only
// type. This is only needed so EncodedFormData/FormDataElement have a copy
// constructor.
class PLATFORM_EXPORT WrappedDataPipeGetter final
    : public RefCounted<WrappedDataPipeGetter> {
 public:
  explicit WrappedDataPipeGetter(
      network::mojom::blink::DataPipeGetterPtr data_pipe_getter)
      : data_pipe_getter_(std::move(data_pipe_getter)) {}
  ~WrappedDataPipeGetter() = default;

  network::mojom::blink::DataPipeGetterPtr* GetPtr() {
    return &data_pipe_getter_;
  }

 private:
  network::mojom::blink::DataPipeGetterPtr data_pipe_getter_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_NETWORK_WRAPPED_DATA_PIPE_GETTER_H_
