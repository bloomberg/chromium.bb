// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxge/dib/cfx_bitmapstorer.h"

#include <string.h>

#include <utility>

#include "core/fxcrt/span_util.h"
#include "core/fxge/dib/cfx_dibitmap.h"

CFX_BitmapStorer::CFX_BitmapStorer() = default;

CFX_BitmapStorer::~CFX_BitmapStorer() = default;

RetainPtr<CFX_DIBitmap> CFX_BitmapStorer::Detach() {
  return std::move(m_pBitmap);
}

void CFX_BitmapStorer::Replace(RetainPtr<CFX_DIBitmap>&& pBitmap) {
  m_pBitmap = std::move(pBitmap);
}

void CFX_BitmapStorer::ComposeScanline(
    int line,
    pdfium::span<const uint8_t> scanline,
    pdfium::span<const uint8_t> scan_extra_alpha) {
  pdfium::span<uint8_t> dest_buf = m_pBitmap->GetWritableScanline(line);
  if (!dest_buf.empty())
    fxcrt::spancpy(dest_buf, scanline);

  pdfium::span<uint8_t> dest_alpha_buf =
      m_pBitmap->GetWritableAlphaMaskScanline(line);
  if (!dest_alpha_buf.empty())
    fxcrt::spancpy(dest_alpha_buf, scan_extra_alpha);
}

bool CFX_BitmapStorer::SetInfo(int width,
                               int height,
                               FXDIB_Format src_format,
                               pdfium::span<const uint32_t> src_palette) {
  auto pBitmap = pdfium::MakeRetain<CFX_DIBitmap>();
  if (!pBitmap->Create(width, height, src_format))
    return false;

  if (!src_palette.empty())
    pBitmap->SetPalette(src_palette);

  m_pBitmap = std::move(pBitmap);
  return true;
}
