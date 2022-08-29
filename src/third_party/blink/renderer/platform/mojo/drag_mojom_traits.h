// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_MOJO_DRAG_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_MOJO_DRAG_MOJOM_TRAITS_H_

#include <stdint.h>

#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/base/file_path_mojom_traits.h"
#include "mojo/public/cpp/bindings/array_traits_web_vector.h"
#include "mojo/public/cpp/bindings/string_traits_wtf.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "mojo/public/cpp/bindings/union_traits.h"
#include "services/network/public/mojom/referrer_policy.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/data_transfer/data_transfer.mojom-shared.h"
#include "third_party/blink/public/mojom/drag/drag.mojom-shared.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_data_transfer_token.mojom-blink.h"
#include "third_party/blink/public/platform/web_drag_data.h"
#include "third_party/blink/renderer/platform/blob/serialized_blob_mojom_traits.h"
#include "third_party/blink/renderer/platform/mojo/kurl_mojom_traits.h"
#include "third_party/blink/renderer/platform/mojo/string16_mojom_traits.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {
template <typename T>
class WebVector;
}

namespace mojo {

template <>
struct PLATFORM_EXPORT StructTraits<blink::mojom::DragItemStringDataView,
                                    blink::WebDragData::Item> {
  static WTF::String string_type(blink::WebDragData::Item item);
  static WTF::String string_data(const blink::WebDragData::Item& item);
  static WTF::String title(const blink::WebDragData::Item& item);
  static absl::optional<blink::KURL> base_url(
      const blink::WebDragData::Item& item);
  static bool Read(blink::mojom::DragItemStringDataView data,
                   blink::WebDragData::Item* out);
};

template <>
struct PLATFORM_EXPORT StructTraits<blink::mojom::DataTransferFileDataView,
                                    blink::WebDragData::Item> {
  static base::FilePath path(const blink::WebDragData::Item& item);
  static base::FilePath display_name(const blink::WebDragData::Item& item);
  static mojo::PendingRemote<
      blink::mojom::blink::FileSystemAccessDataTransferToken>
  file_system_access_token(const blink::WebDragData::Item& item);
  static bool Read(blink::mojom::DataTransferFileDataView data,
                   blink::WebDragData::Item* out);
};

template <>
struct PLATFORM_EXPORT StructTraits<blink::mojom::DragItemBinaryDataView,
                                    blink::WebDragData::Item> {
  static mojo_base::BigBuffer data(const blink::WebDragData::Item& item);
  static bool is_image_accessible(const blink::WebDragData::Item& item);
  static blink::KURL source_url(const blink::WebDragData::Item& item);
  static base::FilePath filename_extension(
      const blink::WebDragData::Item& item);
  static WTF::String content_disposition(const blink::WebDragData::Item& item);
  static bool Read(blink::mojom::DragItemBinaryDataView data,
                   blink::WebDragData::Item* out);
};

template <>
struct PLATFORM_EXPORT
    StructTraits<blink::mojom::DragItemFileSystemFileDataView,
                 blink::WebDragData::Item> {
  static blink::KURL url(const blink::WebDragData::Item& item);
  static int64_t size(const blink::WebDragData::Item& item);
  static WTF::String file_system_id(const blink::WebDragData::Item& item);
  static scoped_refptr<blink::BlobDataHandle> serialized_blob(
      const blink::WebDragData::Item& item);
  static bool Read(blink::mojom::DragItemFileSystemFileDataView data,
                   blink::WebDragData::Item* out);
};

template <>
struct PLATFORM_EXPORT
    UnionTraits<blink::mojom::DragItemDataView, blink::WebDragData::Item> {
  static const blink::WebDragData::Item& string(
      const blink::WebDragData::Item& item) {
    return item;
  }
  static const blink::WebDragData::Item& file(
      const blink::WebDragData::Item& item) {
    return item;
  }
  static const blink::WebDragData::Item& binary(
      const blink::WebDragData::Item& item) {
    return item;
  }
  static const blink::WebDragData::Item& file_system_file(
      const blink::WebDragData::Item& item) {
    return item;
  }
  static bool Read(blink::mojom::DragItemDataView data,
                   blink::WebDragData::Item* out);
  static blink::mojom::DragItemDataView::Tag GetTag(
      const blink::WebDragData::Item& item);
};

template <>
struct PLATFORM_EXPORT
    StructTraits<blink::mojom::DragDataDataView, blink::WebDragData> {
  static blink::WebVector<blink::WebDragData::Item> items(
      const blink::WebDragData& drag_data);
  static WTF::String file_system_id(const blink::WebDragData& drag_data);
  static network::mojom::ReferrerPolicy referrer_policy(
      const blink::WebDragData& drag_data);
  static bool Read(blink::mojom::DragDataDataView data,
                   blink::WebDragData* out);
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_MOJO_DRAG_MOJOM_TRAITS_H_
