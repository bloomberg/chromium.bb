/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_DRAG_DATA_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_DRAG_DATA_H_

#include "base/memory/scoped_refptr.h"
#include "services/network/public/mojom/referrer_policy.mojom-shared.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_data_transfer_token.mojom-shared.h"
#include "third_party/blink/public/platform/cross_variant_mojo_util.h"
#include "third_party/blink/public/platform/web_blob_info.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_vector.h"

namespace blink {
template <typename T>
class WebVector;

using FileSystemAccessDropData =
    base::RefCountedData<blink::CrossVariantMojoRemote<
        mojom::FileSystemAccessDataTransferTokenInterfaceBase>>;

// Holds data that may be exchanged through a drag-n-drop operation. It is
// inexpensive to copy a WebDragData object.
class WebDragData {
 public:
  struct Item {
    enum StorageType {
      // String data with an associated MIME type. Depending on the MIME type,
      // there may be optional metadata attributes as well.
      kStorageTypeString,
      // Stores the name of one file being dragged into the renderer.
      kStorageTypeFilename,
      // An image being dragged out of the renderer. Contains a buffer holding
      // the image data as well as the suggested name for saving the image to.
      kStorageTypeBinaryData,
      // Stores the filesystem URL of one file being dragged into the renderer.
      kStorageTypeFileSystemFile,
    };

    StorageType storage_type;

    // TODO(dcheng): This should probably be a union.
    // Only valid when storage_type == kStorageTypeString.
    WebString string_type;
    WebString string_data;

    // Title associated with a link when string_type == "text/uri-list".
    WebString title;

    // Only valid when string_type == "text/html". Stores the base URL for the
    // contained markup.
    WebURL base_url;

    // Only valid when storage_type == kStorageTypeFilename.
    WebString filename_data;
    WebString display_name_data;
    scoped_refptr<FileSystemAccessDropData> file_system_access_entry;

    // Only valid when storage_type == kStorageTypeBinaryData.
    WebData binary_data;
    bool binary_data_image_accessible;
    WebURL binary_data_source_url;
    WebString binary_data_filename_extension;
    WebString binary_data_content_disposition;

    // Only valid when storage_type == kStorageTypeFileSystemFile.
    WebURL file_system_url;
    int64_t file_system_file_size;
    WebString file_system_id;
    WebBlobInfo file_system_blob_info;
  };

  WebDragData() = default;

  WebDragData(const WebDragData& object) = default;

  WebDragData& operator=(const WebDragData& object) = default;

  ~WebDragData() = default;

  WebVector<Item> Items() const { return item_list_; }

  BLINK_PLATFORM_EXPORT void SetItems(WebVector<Item> item_list);
  // FIXME: SetItems is slow because SetItems copies WebVector.
  // Instead, use SwapItems.
  void SwapItems(WebVector<Item>& item_list) { item_list_.Swap(item_list); }

  BLINK_PLATFORM_EXPORT void AddItem(const Item&);

  WebString FilesystemId() const { return filesystem_id_; }

  void SetFilesystemId(const WebString& filesystem_id) {
    // The ID is an opaque string, given by and validated by chromium port.
    filesystem_id_ = filesystem_id;
  }

  network::mojom::ReferrerPolicy ReferrerPolicy() const {
    return referrer_policy_;
  }

  void SetReferrerPolicy(network::mojom::ReferrerPolicy referrer_policy) {
    referrer_policy_ = referrer_policy;
  }

 private:
  WebVector<Item> item_list_;
  WebString filesystem_id_;

  // Used for items where string_type == "downloadurl". Stores the referrer
  // policy for usage when dragging a link out of the webview results in a
  // download.
  network::mojom::ReferrerPolicy referrer_policy_ =
      network::mojom::ReferrerPolicy::kDefault;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_DRAG_DATA_H_
