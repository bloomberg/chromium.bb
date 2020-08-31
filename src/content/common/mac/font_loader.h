// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MAC_FONT_LOADER_H_
#define CONTENT_COMMON_MAC_FONT_LOADER_H_

#include <CoreText/CoreText.h>
#include <stdint.h>

#include <memory>

#include "base/callback_forward.h"
#include "base/mac/scoped_cftyperef.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/system/buffer.h"

namespace content {

// Provides functionality to transmit fonts over IPC.
//
// Note about font formats: .dfont (datafork suitcase) fonts are currently not
// supported by this code since CGFontCreateWithDataProvider() can't handle them
// directly.
class FontLoader {
 public:
  // Internal font load result data. Exposed here for testing.
  struct CONTENT_EXPORT ResultInternal {
    ResultInternal();
    ~ResultInternal();

    mojo::ScopedSharedBufferHandle font_data;
    uint32_t font_id = 0;
  };

  // Callback for the reporting result of LoadFont().
  // - The ScopedSharedBufferHandle points to a shared memory buffer containing
  //   the raw data for the font file.
  // - The last argument is the font_id: a unique identifier for the on-disk
  //   file we load for the font.
  using LoadedCallback =
      base::OnceCallback<void(mojo::ScopedSharedBufferHandle, uint32_t)>;

  // Load a font specified by |font| into a shared memory buffer suitable for
  // sending over IPC. On failure, zeroes and an invalid handle are reported
  // to the callback.
  CONTENT_EXPORT
  static void LoadFont(const base::string16& font_name,
                       float font_point_size,
                       LoadedCallback callback);

  // Given a shared memory buffer containing the raw data for a font file, load
  // the font and turn them into a CTFontDescriptor.
  //
  // |data| - A shared memory handle pointing to the raw data from a font file.
  // |data_size| - Size of |data|.
  //
  // On return:
  //  returns true on success, false on failure.
  //  |out| - A CTFontDescriptorRef corresponding to the designated font buffer.
  //  The caller is responsible for releasing this value via CFRelease().
  CONTENT_EXPORT
  static bool CTFontDescriptorFromBuffer(
      mojo::ScopedSharedBufferHandle font_data,
      uint32_t font_data_size,
      base::ScopedCFTypeRef<CTFontDescriptorRef>* out_descriptor);

  CONTENT_EXPORT
  static std::unique_ptr<ResultInternal> LoadFontForTesting(
      const base::string16& font_name,
      float font_point_size);
};

}  // namespace content

#endif  // CONTENT_COMMON_MAC_FONT_LOADER_H_
