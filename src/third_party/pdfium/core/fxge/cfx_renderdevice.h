// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXGE_CFX_RENDERDEVICE_H_
#define CORE_FXGE_CFX_RENDERDEVICE_H_

#include <memory>
#include <vector>

#include "build/build_config.h"
#include "core/fxcrt/fx_coordinates.h"
#include "core/fxcrt/unowned_ptr.h"
#include "core/fxge/fx_dib.h"
#include "core/fxge/render_defines.h"
#include "core/fxge/renderdevicedriver_iface.h"

class CFX_DIBBase;
class CFX_DIBitmap;
class CFX_Font;
class CFX_GraphStateData;
class CFX_ImageRenderer;
class CFX_PathData;
class PauseIndicatorIface;
class TextCharPos;
struct CFX_Color;

enum class BorderStyle { SOLID, DASH, BEVELED, INSET, UNDERLINE };

class CFX_RenderDevice {
 public:
  class StateRestorer {
   public:
    explicit StateRestorer(CFX_RenderDevice* pDevice);
    ~StateRestorer();

   private:
    UnownedPtr<CFX_RenderDevice> m_pDevice;
  };

  CFX_RenderDevice();
  virtual ~CFX_RenderDevice();

  static CFX_Matrix GetFlipMatrix(float width,
                                  float height,
                                  float left,
                                  float top);

  void SetDeviceDriver(std::unique_ptr<RenderDeviceDriverIface> pDriver);
  RenderDeviceDriverIface* GetDeviceDriver() const {
    return m_pDeviceDriver.get();
  }

  void SaveState();
  void RestoreState(bool bKeepSaved);

  int GetWidth() const { return m_Width; }
  int GetHeight() const { return m_Height; }
  DeviceType GetDeviceType() const { return m_DeviceType; }
  int GetRenderCaps() const { return m_RenderCaps; }
  int GetDeviceCaps(int id) const;
  RetainPtr<CFX_DIBitmap> GetBitmap() const;
  void SetBitmap(const RetainPtr<CFX_DIBitmap>& pBitmap);
  bool CreateCompatibleBitmap(const RetainPtr<CFX_DIBitmap>& pDIB,
                              int width,
                              int height) const;
  const FX_RECT& GetClipBox() const { return m_ClipBox; }
  bool SetClip_Rect(const FX_RECT& pRect);
  bool SetClip_PathFill(const CFX_PathData* pPathData,
                        const CFX_Matrix* pObject2Device,
                        int fill_mode);
  bool SetClip_PathStroke(const CFX_PathData* pPathData,
                          const CFX_Matrix* pObject2Device,
                          const CFX_GraphStateData* pGraphState);
  bool DrawPath(const CFX_PathData* pPathData,
                const CFX_Matrix* pObject2Device,
                const CFX_GraphStateData* pGraphState,
                uint32_t fill_color,
                uint32_t stroke_color,
                int fill_mode) {
    return DrawPathWithBlend(pPathData, pObject2Device, pGraphState, fill_color,
                             stroke_color, fill_mode, BlendMode::kNormal);
  }
  bool DrawPathWithBlend(const CFX_PathData* pPathData,
                         const CFX_Matrix* pObject2Device,
                         const CFX_GraphStateData* pGraphState,
                         uint32_t fill_color,
                         uint32_t stroke_color,
                         int fill_mode,
                         BlendMode blend_type);
  bool FillRect(const FX_RECT& rect, uint32_t color) {
    return FillRectWithBlend(rect, color, BlendMode::kNormal);
  }

  RetainPtr<CFX_DIBitmap> GetBackDrop();
  bool GetDIBits(const RetainPtr<CFX_DIBitmap>& pBitmap, int left, int top);
  bool SetDIBits(const RetainPtr<CFX_DIBBase>& pBitmap, int left, int top) {
    return SetDIBitsWithBlend(pBitmap, left, top, BlendMode::kNormal);
  }
  bool SetDIBitsWithBlend(const RetainPtr<CFX_DIBBase>& pBitmap,
                          int left,
                          int top,
                          BlendMode blend_mode);
  bool StretchDIBits(const RetainPtr<CFX_DIBBase>& pBitmap,
                     int left,
                     int top,
                     int dest_width,
                     int dest_height) {
    return StretchDIBitsWithFlagsAndBlend(pBitmap, left, top, dest_width,
                                          dest_height, FXDIB_ResampleOptions(),
                                          BlendMode::kNormal);
  }
  bool StretchDIBitsWithFlagsAndBlend(const RetainPtr<CFX_DIBBase>& pBitmap,
                                      int left,
                                      int top,
                                      int dest_width,
                                      int dest_height,
                                      const FXDIB_ResampleOptions& options,
                                      BlendMode blend_mode);
  bool SetBitMask(const RetainPtr<CFX_DIBBase>& pBitmap,
                  int left,
                  int top,
                  uint32_t argb);
  bool StretchBitMask(const RetainPtr<CFX_DIBBase>& pBitmap,
                      int left,
                      int top,
                      int dest_width,
                      int dest_height,
                      uint32_t color);
  bool StretchBitMaskWithFlags(const RetainPtr<CFX_DIBBase>& pBitmap,
                               int left,
                               int top,
                               int dest_width,
                               int dest_height,
                               uint32_t argb,
                               const FXDIB_ResampleOptions& options);
  bool StartDIBits(const RetainPtr<CFX_DIBBase>& pBitmap,
                   int bitmap_alpha,
                   uint32_t color,
                   const CFX_Matrix& matrix,
                   const FXDIB_ResampleOptions& options,
                   std::unique_ptr<CFX_ImageRenderer>* handle) {
    return StartDIBitsWithBlend(pBitmap, bitmap_alpha, color, matrix, options,
                                handle, BlendMode::kNormal);
  }
  bool StartDIBitsWithBlend(const RetainPtr<CFX_DIBBase>& pBitmap,
                            int bitmap_alpha,
                            uint32_t argb,
                            const CFX_Matrix& matrix,
                            const FXDIB_ResampleOptions& options,
                            std::unique_ptr<CFX_ImageRenderer>* handle,
                            BlendMode blend_mode);
  bool ContinueDIBits(CFX_ImageRenderer* handle, PauseIndicatorIface* pPause);

  bool DrawNormalText(int nChars,
                      const TextCharPos* pCharPos,
                      CFX_Font* pFont,
                      float font_size,
                      const CFX_Matrix& mtText2Device,
                      uint32_t fill_color,
                      uint32_t text_flags);
  bool DrawTextPath(int nChars,
                    const TextCharPos* pCharPos,
                    CFX_Font* pFont,
                    float font_size,
                    const CFX_Matrix& mtText2User,
                    const CFX_Matrix* pUser2Device,
                    const CFX_GraphStateData* pGraphState,
                    uint32_t fill_color,
                    uint32_t stroke_color,
                    CFX_PathData* pClippingPath,
                    int nFlag);

  void DrawFillRect(const CFX_Matrix* pUser2Device,
                    const CFX_FloatRect& rect,
                    const CFX_Color& color,
                    int32_t nTransparency);
  void DrawFillRect(const CFX_Matrix* pUser2Device,
                    const CFX_FloatRect& rect,
                    const FX_COLORREF& color);
  void DrawStrokeRect(const CFX_Matrix& mtUser2Device,
                      const CFX_FloatRect& rect,
                      const FX_COLORREF& color,
                      float fWidth);
  void DrawStrokeLine(const CFX_Matrix* pUser2Device,
                      const CFX_PointF& ptMoveTo,
                      const CFX_PointF& ptLineTo,
                      const FX_COLORREF& color,
                      float fWidth);
  void DrawBorder(const CFX_Matrix* pUser2Device,
                  const CFX_FloatRect& rect,
                  float fWidth,
                  const CFX_Color& color,
                  const CFX_Color& crLeftTop,
                  const CFX_Color& crRightBottom,
                  BorderStyle nStyle,
                  int32_t nTransparency);
  void DrawFillArea(const CFX_Matrix& mtUser2Device,
                    const std::vector<CFX_PointF>& points,
                    const FX_COLORREF& color);
  void DrawShadow(const CFX_Matrix& mtUser2Device,
                  bool bVertical,
                  bool bHorizontal,
                  const CFX_FloatRect& rect,
                  int32_t nTransparency,
                  int32_t nStartGray,
                  int32_t nEndGray);

#ifdef _SKIA_SUPPORT_
  virtual void DebugVerifyBitmapIsPreMultiplied() const;
  virtual bool SetBitsWithMask(const RetainPtr<CFX_DIBBase>& pBitmap,
                               const RetainPtr<CFX_DIBBase>& pMask,
                               int left,
                               int top,
                               int bitmap_alpha,
                               BlendMode blend_type);
#endif
#if defined _SKIA_SUPPORT_ || defined _SKIA_SUPPORT_PATHS_
  void Flush(bool release);
#endif

 private:
  void InitDeviceInfo();
  void UpdateClipBox();
  bool DrawFillStrokePath(const CFX_PathData* pPathData,
                          const CFX_Matrix* pObject2Device,
                          const CFX_GraphStateData* pGraphState,
                          uint32_t fill_color,
                          uint32_t stroke_color,
                          int fill_mode,
                          BlendMode blend_type);
  bool DrawCosmeticLine(const CFX_PointF& ptMoveTo,
                        const CFX_PointF& ptLineTo,
                        uint32_t color,
                        int fill_mode,
                        BlendMode blend_type);
  bool FillRectWithBlend(const FX_RECT& rect,
                         uint32_t color,
                         BlendMode blend_type);

  RetainPtr<CFX_DIBitmap> m_pBitmap;
  int m_Width = 0;
  int m_Height = 0;
  int m_bpp = 0;
  int m_RenderCaps = 0;
  DeviceType m_DeviceType = DeviceType::kUnknown;
  FX_RECT m_ClipBox;
  std::unique_ptr<RenderDeviceDriverIface> m_pDeviceDriver;
};

#endif  // CORE_FXGE_CFX_RENDERDEVICE_H_
