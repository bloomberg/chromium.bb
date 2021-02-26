// Copyright 2020 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxge/win32/cps_printer_driver.h"

#include <stdint.h>

#include <vector>

#include "core/fxcrt/fx_system.h"
#include "core/fxcrt/retain_ptr.h"
#include "core/fxge/cfx_fillrenderoptions.h"
#include "core/fxge/cfx_pathdata.h"
#include "core/fxge/dib/cfx_imagerenderer.h"
#include "core/fxge/win32/cpsoutput.h"
#include "third_party/base/notreached.h"

CPSPrinterDriver::CPSPrinterDriver(HDC hDC,
                                   WindowsPrintMode mode,
                                   const EncoderIface* pEncoderIface)
    : m_hDC(hDC), m_PSRenderer(pEncoderIface) {
  // |mode| should be PostScript.
  ASSERT(mode == WindowsPrintMode::kModePostScript2 ||
         mode == WindowsPrintMode::kModePostScript3 ||
         mode == WindowsPrintMode::kModePostScript2PassThrough ||
         mode == WindowsPrintMode::kModePostScript3PassThrough);
  int pslevel = (mode == WindowsPrintMode::kModePostScript2 ||
                 mode == WindowsPrintMode::kModePostScript2PassThrough)
                    ? 2
                    : 3;
  CPSOutput::OutputMode output_mode =
      (mode == WindowsPrintMode::kModePostScript2 ||
       mode == WindowsPrintMode::kModePostScript3)
          ? CPSOutput::OutputMode::kGdiComment
          : CPSOutput::OutputMode::kExtEscape;

  m_HorzSize = ::GetDeviceCaps(m_hDC, HORZSIZE);
  m_VertSize = ::GetDeviceCaps(m_hDC, VERTSIZE);
  m_Width = ::GetDeviceCaps(m_hDC, HORZRES);
  m_Height = ::GetDeviceCaps(m_hDC, VERTRES);
  m_nBitsPerPixel = ::GetDeviceCaps(m_hDC, BITSPIXEL);

  m_PSRenderer.Init(pdfium::MakeRetain<CPSOutput>(m_hDC, output_mode), pslevel,
                    m_Width, m_Height);
  HRGN hRgn = ::CreateRectRgn(0, 0, 1, 1);
  if (::GetClipRgn(m_hDC, hRgn) == 1) {
    DWORD dwCount = ::GetRegionData(hRgn, 0, nullptr);
    if (dwCount) {
      std::vector<uint8_t, FxAllocAllocator<uint8_t>> buffer(dwCount);
      RGNDATA* pData = reinterpret_cast<RGNDATA*>(buffer.data());
      if (::GetRegionData(hRgn, dwCount, pData)) {
        CFX_PathData path;
        for (uint32_t i = 0; i < pData->rdh.nCount; i++) {
          RECT* pRect =
              reinterpret_cast<RECT*>(pData->Buffer + pData->rdh.nRgnSize * i);
          path.AppendRect(static_cast<float>(pRect->left),
                          static_cast<float>(pRect->bottom),
                          static_cast<float>(pRect->right),
                          static_cast<float>(pRect->top));
        }
        m_PSRenderer.SetClip_PathFill(&path, nullptr,
                                      CFX_FillRenderOptions::WindingOptions());
      }
    }
  }
  ::DeleteObject(hRgn);
}

CPSPrinterDriver::~CPSPrinterDriver() {
  m_PSRenderer.EndRendering();
}

DeviceType CPSPrinterDriver::GetDeviceType() const {
  return DeviceType::kPrinter;
}

int CPSPrinterDriver::GetDeviceCaps(int caps_id) const {
  switch (caps_id) {
    case FXDC_PIXEL_WIDTH:
      return m_Width;
    case FXDC_PIXEL_HEIGHT:
      return m_Height;
    case FXDC_BITS_PIXEL:
      return m_nBitsPerPixel;
    case FXDC_RENDER_CAPS:
      return FXRC_BIT_MASK;
    case FXDC_HORZ_SIZE:
      return m_HorzSize;
    case FXDC_VERT_SIZE:
      return m_VertSize;
    default:
      NOTREACHED();
      return 0;
  }
}

void CPSPrinterDriver::SaveState() {
  m_PSRenderer.SaveState();
}

void CPSPrinterDriver::RestoreState(bool bKeepSaved) {
  m_PSRenderer.RestoreState(bKeepSaved);
}

bool CPSPrinterDriver::SetClip_PathFill(
    const CFX_PathData* pPathData,
    const CFX_Matrix* pObject2Device,
    const CFX_FillRenderOptions& fill_options) {
  m_PSRenderer.SetClip_PathFill(pPathData, pObject2Device, fill_options);
  return true;
}

bool CPSPrinterDriver::SetClip_PathStroke(
    const CFX_PathData* pPathData,
    const CFX_Matrix* pObject2Device,
    const CFX_GraphStateData* pGraphState) {
  m_PSRenderer.SetClip_PathStroke(pPathData, pObject2Device, pGraphState);
  return true;
}

bool CPSPrinterDriver::DrawPath(const CFX_PathData* pPathData,
                                const CFX_Matrix* pObject2Device,
                                const CFX_GraphStateData* pGraphState,
                                FX_ARGB fill_color,
                                FX_ARGB stroke_color,
                                const CFX_FillRenderOptions& fill_options,
                                BlendMode blend_type) {
  if (blend_type != BlendMode::kNormal)
    return false;
  return m_PSRenderer.DrawPath(pPathData, pObject2Device, pGraphState,
                               fill_color, stroke_color, fill_options);
}

bool CPSPrinterDriver::GetClipBox(FX_RECT* pRect) {
  *pRect = m_PSRenderer.GetClipBox();
  return true;
}

bool CPSPrinterDriver::SetDIBits(const RetainPtr<CFX_DIBBase>& pBitmap,
                                 uint32_t color,
                                 const FX_RECT& src_rect,
                                 int left,
                                 int top,
                                 BlendMode blend_type) {
  if (blend_type != BlendMode::kNormal)
    return false;
  return m_PSRenderer.SetDIBits(pBitmap, color, left, top);
}

bool CPSPrinterDriver::StretchDIBits(const RetainPtr<CFX_DIBBase>& pBitmap,
                                     uint32_t color,
                                     int dest_left,
                                     int dest_top,
                                     int dest_width,
                                     int dest_height,
                                     const FX_RECT* pClipRect,
                                     const FXDIB_ResampleOptions& options,
                                     BlendMode blend_type) {
  if (blend_type != BlendMode::kNormal)
    return false;
  return m_PSRenderer.StretchDIBits(pBitmap, color, dest_left, dest_top,
                                    dest_width, dest_height, options);
}

bool CPSPrinterDriver::StartDIBits(const RetainPtr<CFX_DIBBase>& pBitmap,
                                   int bitmap_alpha,
                                   uint32_t color,
                                   const CFX_Matrix& matrix,
                                   const FXDIB_ResampleOptions& options,
                                   std::unique_ptr<CFX_ImageRenderer>* handle,
                                   BlendMode blend_type) {
  if (blend_type != BlendMode::kNormal)
    return false;

  if (bitmap_alpha < 255)
    return false;

  *handle = nullptr;
  return m_PSRenderer.DrawDIBits(pBitmap, color, matrix, options);
}

bool CPSPrinterDriver::DrawDeviceText(
    int nChars,
    const TextCharPos* pCharPos,
    CFX_Font* pFont,
    const CFX_Matrix& mtObject2Device,
    float font_size,
    uint32_t color,
    const CFX_TextRenderOptions& /*options*/) {
  return m_PSRenderer.DrawText(nChars, pCharPos, pFont, mtObject2Device,
                               font_size, color);
}
