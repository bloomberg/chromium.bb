// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/blob/blob_storage_controller.h"

#include "base/logging.h"
#include "googleurl/src/gurl.h"
#include "net/base/upload_data.h"
#include "webkit/blob/blob_data.h"

namespace webkit_blob {

BlobStorageController::BlobStorageController() {
}

BlobStorageController::~BlobStorageController() {
}

void BlobStorageController::AppendStorageItems(
    BlobData* target_blob_data, BlobData* src_blob_data,
    uint64 offset, uint64 length) {
  DCHECK(target_blob_data && src_blob_data &&
         length != static_cast<uint64>(-1));

  std::vector<BlobData::Item>::const_iterator iter =
      src_blob_data->items().begin();
  if (offset) {
    for (; iter != src_blob_data->items().end(); ++iter) {
      if (offset >= iter->length())
        offset -= iter->length();
      else
        break;
    }
  }

  for (; iter != src_blob_data->items().end() && length > 0; ++iter) {
    uint64 current_length = iter->length() - offset;
    uint64 new_length = current_length > length ? length : current_length;
    if (iter->type() == BlobData::TYPE_DATA) {
      target_blob_data->AppendData(iter->data(),
                                   static_cast<uint32>(iter->offset() + offset),
                                   static_cast<uint32>(new_length));
    } else {
      DCHECK(iter->type() == BlobData::TYPE_FILE);
      target_blob_data->AppendFile(iter->file_path(),
                                   iter->offset() + offset,
                                   new_length,
                                   iter->expected_modification_time());
    }
    length -= new_length;
    offset = 0;
  }
}

void BlobStorageController::RegisterBlobUrl(
    const GURL& url, const BlobData* blob_data) {
  scoped_refptr<BlobData> target_blob_data = new BlobData();
  target_blob_data->set_content_type(blob_data->content_type());
  target_blob_data->set_content_disposition(blob_data->content_disposition());

  // The blob data is stored in the "canonical" way. That is, it only contains a
  // list of Data and File items.
  // 1) The Data item is denoted by the raw data and the range.
  // 2) The File item is denoted by the file path, the range and the expected
  //    modification time.
  // All the Blob items in the passing blob data are resolved and expanded into
  // a set of Data and File items.

  for (std::vector<BlobData::Item>::const_iterator iter =
           blob_data->items().begin();
       iter != blob_data->items().end(); ++iter) {
    switch (iter->type()) {
      case BlobData::TYPE_DATA: {
        // WebBlobData does not allow partial data.
        DCHECK(!(iter->offset()) && iter->length() == iter->data().size());
        target_blob_data->AppendData(iter->data());
        break;
      }
      case BlobData::TYPE_FILE:
        target_blob_data->AppendFile(iter->file_path(),
                                     iter->offset(),
                                     iter->length(),
                                     iter->expected_modification_time());
        break;
      case BlobData::TYPE_BLOB: {
        BlobData* src_blob_data = GetBlobDataFromUrl(iter->blob_url());
        DCHECK(src_blob_data);
        if (src_blob_data)
          AppendStorageItems(target_blob_data.get(),
                             src_blob_data,
                             iter->offset(),
                             iter->length());
        break;
      }
    }
  }

  blob_map_[url.spec()] = target_blob_data;
}

void BlobStorageController::RegisterBlobUrlFrom(
    const GURL& url, const GURL& src_url) {
  BlobData* blob_data = GetBlobDataFromUrl(src_url);
  DCHECK(blob_data);
  if (!blob_data)
    return;

  blob_map_[url.spec()] = blob_data;
}

void BlobStorageController::UnregisterBlobUrl(const GURL& url) {
  blob_map_.erase(url.spec());
}

BlobData* BlobStorageController::GetBlobDataFromUrl(const GURL& url) {
  BlobMap::iterator found = blob_map_.find(url.spec());
  return (found != blob_map_.end()) ? found->second : NULL;
}

void BlobStorageController::ResolveBlobReferencesInUploadData(
    net::UploadData* upload_data) {
  DCHECK(upload_data);

  std::vector<net::UploadData::Element>* uploads = upload_data->elements();
  std::vector<net::UploadData::Element>::iterator iter;
  for (iter = uploads->begin(); iter != uploads->end();) {
    if (iter->type() != net::UploadData::TYPE_BLOB) {
      iter++;
      continue;
    }

    // Find the referred blob data.
    webkit_blob::BlobData* blob_data = GetBlobDataFromUrl(iter->blob_url());
    DCHECK(blob_data);
    if (!blob_data) {
      // TODO(jianli): We should probably fail uploading the data
      iter++;
      continue;
    }

    // Remove this element.
    iter = uploads->erase(iter);

    // If there is no element in the referred blob data, continue the loop.
    // Note that we should not increase iter since it already points to the one
    // after the removed element.
    if (blob_data->items().empty())
      continue;

    // Insert the elements in the referred blob data.
    // Note that we traverse from the bottom so that the elements can be
    // inserted in the original order.
    for (size_t i = blob_data->items().size(); i > 0; --i) {
      iter = uploads->insert(iter, net::UploadData::Element());

      const webkit_blob::BlobData::Item& item = blob_data->items().at(i - 1);
      switch (item.type()) {
        case webkit_blob::BlobData::TYPE_DATA:
          // TODO(jianli): Figure out how to avoid copying the data.
          iter->SetToBytes(
              &item.data().at(0) + static_cast<int>(item.offset()),
              static_cast<int>(item.length()));
          break;
        case webkit_blob::BlobData::TYPE_FILE:
          // TODO(michaeln): Ensure that any temp files survive till the
          // URLRequest is done with the upload.
          iter->SetToFilePathRange(
              item.file_path(),
              item.offset(),
              item.length(),
              item.expected_modification_time());
          break;
        default:
          NOTREACHED();
          break;
      }
    }
  }
}

}  // namespace webkit_blob
