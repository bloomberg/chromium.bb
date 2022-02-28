// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxge/dib/cfx_bitmapcomposer.h"

#include <string.h>

#include "core/fxcrt/fx_coordinates.h"
#include "core/fxcrt/fx_safe_types.h"
#include "core/fxge/cfx_cliprgn.h"
#include "core/fxge/dib/cfx_dibitmap.h"

CFX_BitmapComposer::CFX_BitmapComposer() = default;

CFX_BitmapComposer::~CFX_BitmapComposer() = default;

void CFX_BitmapComposer::Compose(const RetainPtr<CFX_DIBitmap>& pDest,
                                 const CFX_ClipRgn* pClipRgn,
                                 int bitmap_alpha,
                                 uint32_t mask_color,
                                 const FX_RECT& dest_rect,
                                 bool bVertical,
                                 bool bFlipX,
                                 bool bFlipY,
                                 bool bRgbByteOrder,
                                 BlendMode blend_mode) {
  m_pBitmap = pDest;
  m_pClipRgn = pClipRgn;
  m_DestLeft = dest_rect.left;
  m_DestTop = dest_rect.top;
  m_DestWidth = dest_rect.Width();
  m_DestHeight = dest_rect.Height();
  m_BitmapAlpha = bitmap_alpha;
  m_MaskColor = mask_color;
  m_pClipMask = nullptr;
  if (pClipRgn && pClipRgn->GetType() != CFX_ClipRgn::kRectI)
    m_pClipMask = pClipRgn->GetMask();
  m_bVertical = bVertical;
  m_bFlipX = bFlipX;
  m_bFlipY = bFlipY;
  m_bRgbByteOrder = bRgbByteOrder;
  m_BlendMode = blend_mode;
}

bool CFX_BitmapComposer::SetInfo(int width,
                                 int height,
                                 FXDIB_Format src_format,
                                 pdfium::span<const uint32_t> src_palette) {
  m_SrcFormat = src_format;
  if (!m_Compositor.Init(m_pBitmap->GetFormat(), src_format, width, src_palette,
                         m_MaskColor, m_BlendMode,
                         m_pClipMask != nullptr || (m_BitmapAlpha < 255),
                         m_bRgbByteOrder)) {
    return false;
  }
  if (m_bVertical) {
    m_pScanlineV.resize(m_pBitmap->GetBPP() / 8 * width + 4);
    m_pClipScanV.resize(m_pBitmap->GetHeight());
    if (m_pBitmap->HasAlphaMask())
      m_pScanlineAlphaV.resize(width + 4);
  }
  if (m_BitmapAlpha < 255) {
    m_pAddClipScan.resize(m_bVertical ? m_pBitmap->GetHeight()
                                      : m_pBitmap->GetWidth());
  }
  return true;
}

void CFX_BitmapComposer::DoCompose(pdfium::span<uint8_t> dest_scan,
                                   pdfium::span<const uint8_t> src_scan,
                                   int dest_width,
                                   pdfium::span<const uint8_t> clip_scan,
                                   pdfium::span<const uint8_t> src_extra_alpha,
                                   pdfium::span<uint8_t> dst_extra_alpha) {
  if (m_BitmapAlpha < 255) {
    if (!clip_scan.empty()) {
      for (int i = 0; i < dest_width; ++i)
        m_pAddClipScan[i] = clip_scan[i] * m_BitmapAlpha / 255;
    } else {
      memset(m_pAddClipScan.data(), m_BitmapAlpha, dest_width);
    }
    clip_scan = m_pAddClipScan;
  }
  if (m_SrcFormat == FXDIB_Format::k8bppMask) {
    m_Compositor.CompositeByteMaskLine(dest_scan, src_scan, dest_width,
                                       clip_scan, dst_extra_alpha);
  } else if (GetBppFromFormat(m_SrcFormat) == 8) {
    m_Compositor.CompositePalBitmapLine(dest_scan, src_scan, 0, dest_width,
                                        clip_scan, src_extra_alpha,
                                        dst_extra_alpha);
  } else {
    m_Compositor.CompositeRgbBitmapLine(dest_scan, src_scan, dest_width,
                                        clip_scan, src_extra_alpha,
                                        dst_extra_alpha);
  }
}

void CFX_BitmapComposer::ComposeScanline(
    int line,
    pdfium::span<const uint8_t> scanline,
    pdfium::span<const uint8_t> scan_extra_alpha) {
  if (m_bVertical) {
    ComposeScanlineV(line, scanline, scan_extra_alpha);
    return;
  }
  pdfium::span<const uint8_t> clip_scan;
  if (m_pClipMask) {
    clip_scan =
        m_pClipMask
            ->GetWritableScanline(m_DestTop + line - m_pClipRgn->GetBox().top)
            .subspan(m_DestLeft - m_pClipRgn->GetBox().left);
  }
  pdfium::span<uint8_t> dest_scan =
      m_pBitmap->GetWritableScanline(line + m_DestTop);
  if (!dest_scan.empty()) {
    FX_SAFE_UINT32 offset = m_DestLeft;
    offset *= m_pBitmap->GetBPP();
    offset /= 8;
    if (!offset.IsValid())
      return;

    dest_scan = dest_scan.subspan(offset.ValueOrDie());
  }
  pdfium::span<uint8_t> dest_alpha_scan =
      m_pBitmap->GetWritableAlphaMaskScanline(line + m_DestTop);
  if (!dest_alpha_scan.empty())
    dest_alpha_scan = dest_alpha_scan.subspan(m_DestLeft);

  DoCompose(dest_scan, scanline, m_DestWidth, clip_scan, scan_extra_alpha,
            dest_alpha_scan);
}

void CFX_BitmapComposer::ComposeScanlineV(
    int line,
    pdfium::span<const uint8_t> scanline,
    pdfium::span<const uint8_t> scan_extra_alpha) {
  int Bpp = m_pBitmap->GetBPP() / 8;
  int dest_pitch = m_pBitmap->GetPitch();
  int dest_alpha_pitch = m_pBitmap->GetAlphaMaskPitch();
  int dest_x = m_DestLeft + (m_bFlipX ? (m_DestWidth - line - 1) : line);
  uint8_t* dest_buf = m_pBitmap->GetBuffer();
  if (dest_buf) {
    dest_buf += dest_x * Bpp + m_DestTop * dest_pitch;
    if (m_bFlipY)
      dest_buf += dest_pitch * (m_DestHeight - 1);
  }
  uint8_t* dest_alpha_buf = m_pBitmap->GetAlphaMaskBuffer();
  if (dest_alpha_buf) {
    dest_alpha_buf += dest_x + m_DestTop * dest_alpha_pitch;
    if (m_bFlipY)
      dest_alpha_buf += dest_alpha_pitch * (m_DestHeight - 1);
  }
  int y_step = dest_pitch;
  int y_alpha_step = dest_alpha_pitch;
  if (m_bFlipY) {
    y_step = -y_step;
    y_alpha_step = -y_alpha_step;
  }
  uint8_t* src_scan = m_pScanlineV.data();
  uint8_t* dest_scan = dest_buf;
  for (int i = 0; i < m_DestHeight; ++i) {
    for (int j = 0; j < Bpp; ++j)
      *src_scan++ = dest_scan[j];
    dest_scan += y_step;
  }
  uint8_t* src_alpha_scan = m_pScanlineAlphaV.data();
  uint8_t* dest_alpha_scan = dest_alpha_buf;
  if (dest_alpha_scan) {
    for (int i = 0; i < m_DestHeight; ++i) {
      *src_alpha_scan++ = *dest_alpha_scan;
      dest_alpha_scan += y_alpha_step;
    }
  }
  pdfium::span<uint8_t> clip_scan;
  if (m_pClipMask) {
    clip_scan = m_pClipScanV;
    int clip_pitch = m_pClipMask->GetPitch();
    const uint8_t* src_clip =
        m_pClipMask->GetScanline(m_DestTop - m_pClipRgn->GetBox().top)
            .subspan(dest_x - m_pClipRgn->GetBox().left)
            .data();
    if (m_bFlipY) {
      src_clip += clip_pitch * (m_DestHeight - 1);
      clip_pitch = -clip_pitch;
    }
    for (int i = 0; i < m_DestHeight; ++i) {
      clip_scan[i] = *src_clip;
      src_clip += clip_pitch;
    }
  }
  DoCompose(m_pScanlineV, scanline, m_DestHeight, clip_scan, scan_extra_alpha,
            m_pScanlineAlphaV);
  src_scan = m_pScanlineV.data();
  dest_scan = dest_buf;
  for (int i = 0; i < m_DestHeight; ++i) {
    for (int j = 0; j < Bpp; ++j)
      dest_scan[j] = *src_scan++;
    dest_scan += y_step;
  }
  src_alpha_scan = m_pScanlineAlphaV.data();
  dest_alpha_scan = dest_alpha_buf;
  if (!dest_alpha_scan)
    return;
  for (int i = 0; i < m_DestHeight; ++i) {
    *dest_alpha_scan = *src_alpha_scan++;
    dest_alpha_scan += y_alpha_step;
  }
}
