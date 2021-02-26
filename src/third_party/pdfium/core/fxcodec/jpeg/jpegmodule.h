// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXCODEC_JPEG_JPEGMODULE_H_
#define CORE_FXCODEC_JPEG_JPEGMODULE_H_

#include <memory>

#include "build/build_config.h"
#include "third_party/base/optional.h"
#include "third_party/base/span.h"

#if defined(OS_WIN)
#include "core/fxcrt/retain_ptr.h"
#endif

class CFX_DIBBase;

namespace fxcodec {

class ScanlineDecoder;

class JpegModule {
 public:
  struct JpegImageInfo {
    int width;
    int height;
    int num_components;
    int bits_per_components;
    bool color_transform;
  };

  static std::unique_ptr<ScanlineDecoder> CreateDecoder(
      pdfium::span<const uint8_t> src_span,
      int width,
      int height,
      int nComps,
      bool ColorTransform);

  static Optional<JpegImageInfo> LoadInfo(pdfium::span<const uint8_t> src_span);

#if defined(OS_WIN)
  static bool JpegEncode(const RetainPtr<CFX_DIBBase>& pSource,
                         uint8_t** dest_buf,
                         size_t* dest_size);
#endif  // defined(OS_WIN)

  JpegModule() = delete;
  JpegModule(const JpegModule&) = delete;
  JpegModule& operator=(const JpegModule&) = delete;
};

}  // namespace fxcodec

using JpegModule = fxcodec::JpegModule;

#endif  // CORE_FXCODEC_JPEG_JPEGMODULE_H_
