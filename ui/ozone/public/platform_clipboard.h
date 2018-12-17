// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_PLATFORM_CLIPBOARD_H_
#define UI_OZONE_PUBLIC_PLATFORM_CLIPBOARD_H_

#include <string>
#include <unordered_map>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "ui/ozone/ozone_base_export.h"

namespace ui {

// PlatformClipboard is an interface that allows Ozone backends to exchange
// data with other applications on the host system. The most familiar use for
// it is handling copy and paste operations.
//
class OZONE_BASE_EXPORT PlatformClipboard {
 public:
  // DataMap is a map from "mime type" to associated data, whereas
  // the data can be organized differently for each mime type.
  using Data = std::vector<uint8_t>;
  using DataMap = std::unordered_map<std::string, Data>;

  // Offers a given clipboard data 'data_map' to the host system clipboard.
  //
  // It is common that host clipboard implementations simply get offered
  // the set of mime types available for the data being shared. In such cases,
  // the actual clipboard data is only 'transferred' to the consuming
  // application asynchronously, upon an explicit request for data given a
  // specific mime type. This is the case of Wayland compositors and MacOS
  // (NSPasteboard), for example.
  //
  // The invoker assumes the Ozone implementation will not free |DataMap|
  // before |OfferDataClosure| is called.
  //
  // OfferDataClosure should be invoked when the host clipboard implementation
  // acknowledges that the "offer to clipboard" operation is performed.
  using OfferDataClosure = base::OnceCallback<void()>;
  virtual void OfferClipboardData(const DataMap& data_map,
                                  OfferDataClosure callback) = 0;

  // Reads data from host system clipboard given mime type. The data is
  // stored in 'data_map'.
  //
  // RequestDataClosure is invoked to acknowledge that the requested clipboard
  // data has been read and stored into 'data_map'.
  using RequestDataClosure =
      base::OnceCallback<void(const base::Optional<std::vector<uint8_t>>&)>;
  virtual void RequestClipboardData(const std::string& mime_type,
                                    DataMap* data_map,
                                    RequestDataClosure callback) = 0;

  // Gets the mime types of the data available for clipboard operations
  // in the host system clipboard.
  //
  // GetMimeTypesClosure is invoked when the mime types available for clipboard
  // operations are known.
  using GetMimeTypesClosure =
      base::OnceCallback<void(const std::vector<std::string>&)>;
  virtual void GetAvailableMimeTypes(GetMimeTypesClosure callback) = 0;

  // Returns true if the current application writing data to the host clipboard
  // data is this one; false otherwise.
  //
  // It can be relevant to know this information in case the client wants to
  // caches the clipboard data, and wants to know if it is possible to use
  // the cached data in order to reply faster to read-clipboard operations.
  virtual bool IsSelectionOwner() = 0;
};

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_PLATFORM_CLIPBOARD_H_
