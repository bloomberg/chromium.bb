// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_COPY_OUTPUT_REQUEST_STRUCT_TRAITS_H_
#define SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_COPY_OUTPUT_REQUEST_STRUCT_TRAITS_H_

#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "mojo/common/common_custom_types_struct_traits.h"
#include "services/viz/public/cpp/compositing/texture_mailbox_struct_traits.h"
#include "services/viz/public/interfaces/compositing/copy_output_request.mojom.h"
#include "ui/gfx/geometry/mojo/geometry_struct_traits.h"

namespace mojo {

template <>
struct StructTraits<viz::mojom::CopyOutputRequestDataView,
                    std::unique_ptr<viz::CopyOutputRequest>> {
  static viz::CopyOutputRequest::ResultFormat result_format(
      const std::unique_ptr<viz::CopyOutputRequest>& request) {
    return request->result_format();
  }

  static const base::Optional<base::UnguessableToken>& source(
      const std::unique_ptr<viz::CopyOutputRequest>& request) {
    return request->source_;
  }

  static const base::Optional<gfx::Rect>& area(
      const std::unique_ptr<viz::CopyOutputRequest>& request) {
    return request->area_;
  }

  static const base::Optional<viz::TextureMailbox>& texture_mailbox(
      const std::unique_ptr<viz::CopyOutputRequest>& request) {
    return request->texture_mailbox_;
  }

  static viz::mojom::CopyOutputResultSenderPtr result_sender(
      const std::unique_ptr<viz::CopyOutputRequest>& request);

  static bool Read(viz::mojom::CopyOutputRequestDataView data,
                   std::unique_ptr<viz::CopyOutputRequest>* out_p);
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_COPY_OUTPUT_REQUEST_STRUCT_TRAITS_H_
