// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CLIPBOARD_EXTENSION_HELPER_CHROMEOS_H_
#define CHROME_BROWSER_EXTENSIONS_CLIPBOARD_EXTENSION_HELPER_CHROMEOS_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "extensions/browser/api/clipboard/clipboard_api.h"
#include "extensions/common/api/clipboard.h"

class SkBitmap;

namespace extensions {

// A helper class for decoding the image data and saving decoded image data on
// clipboard, called from clipboard extension API.
class ClipboardExtensionHelper {
 public:
  ClipboardExtensionHelper();
  ~ClipboardExtensionHelper();

  // Decodes and saves the image data on clipboard. Must run on UI thread.
  void DecodeAndSaveImageData(
      const std::vector<char>& data,
      api::clipboard::ImageType type,
      AdditionalDataItemList additional_items,
      const base::Closure& success_callback,
      const base::Callback<void(const std::string&)>& error_callback);

 private:
  // A class to decode PNG and JPEG file.
  class ClipboardImageDataDecoder;

  // Handles decoded image data.
  void OnImageDecoded(const SkBitmap& bitmap);
  // Handles image decoding failure case.
  void OnImageDecodeFailure();
  // Handles image decoding request cancelation case.
  void OnImageDecodeCancel();

  std::unique_ptr<ClipboardImageDataDecoder> clipboard_image_data_decoder_;
  base::Closure image_save_success_callback_;
  base::Callback<void(const std::string&)> image_save_error_callback_;
  AdditionalDataItemList additonal_items_;

  DISALLOW_COPY_AND_ASSIGN(ClipboardExtensionHelper);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_CLIPBOARD_EXTENSION_HELPER_CHROMEOS_H_
