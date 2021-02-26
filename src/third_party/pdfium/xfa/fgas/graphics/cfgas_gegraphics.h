// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FGAS_GRAPHICS_CFGAS_GEGRAPHICS_H_
#define XFA_FGAS_GRAPHICS_CFGAS_GEGRAPHICS_H_

#include <memory>
#include <vector>

#include "core/fxcrt/fx_system.h"
#include "core/fxge/cfx_fillrenderoptions.h"
#include "core/fxge/cfx_graphstatedata.h"
#include "third_party/base/span.h"
#include "xfa/fgas/graphics/cfgas_gecolor.h"

enum class FX_HatchStyle {
  Horizontal = 0,
  Vertical = 1,
  ForwardDiagonal = 2,
  BackwardDiagonal = 3,
  Cross = 4,
  DiagonalCross = 5
};

class CFGAS_GEPath;
class CFX_DIBBase;
class CFX_RenderDevice;

class CFGAS_GEGraphics {
 public:
  explicit CFGAS_GEGraphics(CFX_RenderDevice* renderDevice);
  ~CFGAS_GEGraphics();

  void SaveGraphState();
  void RestoreGraphState();

  CFX_RectF GetClipRect() const;
  const CFX_Matrix* GetMatrix() const;
  CFX_RenderDevice* GetRenderDevice();

  void SetLineCap(CFX_GraphStateData::LineCap lineCap);
  void SetLineDash(float dashPhase, pdfium::span<const float> dashArray);
  void SetSolidLineDash();
  void SetLineWidth(float lineWidth);
  void EnableActOnDash();
  void SetStrokeColor(const CFGAS_GEColor& color);
  void SetFillColor(const CFGAS_GEColor& color);
  void SetClipRect(const CFX_RectF& rect);
  void StrokePath(CFGAS_GEPath* path, const CFX_Matrix* matrix);
  void FillPath(CFGAS_GEPath* path,
                CFX_FillRenderOptions::FillType fill_type,
                const CFX_Matrix* matrix);
  void ConcatMatrix(const CFX_Matrix* matrix);

 private:
  struct TInfo {
    TInfo();
    explicit TInfo(const TInfo& info);
    TInfo& operator=(const TInfo& other);

    CFX_GraphStateData graphState;
    CFX_Matrix CTM;
    bool isActOnDash = false;
    CFGAS_GEColor strokeColor{nullptr};
    CFGAS_GEColor fillColor{nullptr};
  };

  void RenderDeviceStrokePath(const CFGAS_GEPath* path,
                              const CFX_Matrix* matrix);
  void RenderDeviceFillPath(const CFGAS_GEPath* path,
                            CFX_FillRenderOptions::FillType fill_type,
                            const CFX_Matrix* matrix);
  void FillPathWithPattern(const CFGAS_GEPath* path,
                           const CFX_FillRenderOptions& fill_options,
                           const CFX_Matrix& matrix);
  void FillPathWithShading(const CFGAS_GEPath* path,
                           const CFX_FillRenderOptions& fill_options,
                           const CFX_Matrix& matrix);
  void SetDIBitsWithMatrix(const RetainPtr<CFX_DIBBase>& source,
                           const CFX_Matrix& matrix);

  CFX_RenderDevice* const m_renderDevice;  // Not owned.
  TInfo m_info;
  std::vector<std::unique_ptr<TInfo>> m_infoStack;
};

#endif  // XFA_FGAS_GRAPHICS_CFGAS_GEGRAPHICS_H_
