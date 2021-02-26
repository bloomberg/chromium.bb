// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_DROP_DATA_UTIL_H_
#define CONTENT_BROWSER_RENDERER_HOST_DROP_DATA_UTIL_H_

#include <vector>

#include "content/common/content_export.h"
#include "content/public/common/drop_data.h"
#include "third_party/blink/public/mojom/page/drag.mojom-forward.h"

namespace content {
class NativeFileSystemManagerImpl;

CONTENT_EXPORT
blink::mojom::DragDataPtr DropDataToDragData(
    const DropData& drop_data,
    NativeFileSystemManagerImpl* native_file_system_manager,
    int child_id);

CONTENT_EXPORT
blink::mojom::DragDataPtr DropMetaDataToDragData(
    const std::vector<DropData::Metadata>& drop_meta_data);

CONTENT_EXPORT DropData
DragDataToDropData(const blink::mojom::DragData& drag_data);

}  // namespace content

#endif  // #define CONTENT_BROWSER_RENDERER_HOST_DROP_DATA_UTIL_H_
