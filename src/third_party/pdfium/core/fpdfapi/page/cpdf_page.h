// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_PAGE_CPDF_PAGE_H_
#define CORE_FPDFAPI_PAGE_CPDF_PAGE_H_

#include <memory>
#include <utility>

#include "core/fpdfapi/page/cpdf_pageobjectholder.h"
#include "core/fpdfapi/page/ipdf_page.h"
#include "core/fxcrt/fx_coordinates.h"
#include "core/fxcrt/observed_ptr.h"
#include "core/fxcrt/retain_ptr.h"
#include "core/fxcrt/unowned_ptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class CPDF_Dictionary;
class CPDF_Document;
class CPDF_Image;
class CPDF_Object;

class CPDF_Page final : public IPDF_Page, public CPDF_PageObjectHolder {
 public:
  // Caller implements as desired, empty here due to layering.
  class View : public Observable {};

  // Data for the render layer to attach to this page.
  class RenderContextIface {
   public:
    virtual ~RenderContextIface() = default;
  };

  // Cache for the render layer to attach to this page.
  class RenderCacheIface {
   public:
    virtual ~RenderCacheIface() = default;
    virtual void ResetBitmapForImage(const RetainPtr<CPDF_Image>& pImage) = 0;
  };

  class RenderContextClearer {
   public:
    explicit RenderContextClearer(CPDF_Page* pPage);
    ~RenderContextClearer();

   private:
    UnownedPtr<CPDF_Page> const m_pPage;
  };

  CONSTRUCT_VIA_MAKE_RETAIN;

  // IPDF_Page:
  CPDF_Page* AsPDFPage() override;
  CPDFXFA_Page* AsXFAPage() override;
  CPDF_Document* GetDocument() const override;
  float GetPageWidth() const override;
  float GetPageHeight() const override;
  CFX_Matrix GetDisplayMatrix(const FX_RECT& rect, int iRotate) const override;
  absl::optional<CFX_PointF> DeviceToPage(
      const FX_RECT& rect,
      int rotate,
      const CFX_PointF& device_point) const override;
  absl::optional<CFX_PointF> PageToDevice(
      const FX_RECT& rect,
      int rotate,
      const CFX_PointF& page_point) const override;

  // CPDF_PageObjectHolder:
  bool IsPage() const override;

  void ParseContent();
  const CFX_SizeF& GetPageSize() const { return m_PageSize; }
  const CFX_Matrix& GetPageMatrix() const { return m_PageMatrix; }
  int GetPageRotation() const;
  RenderCacheIface* GetRenderCache() const { return m_pRenderCache.get(); }
  void SetRenderCache(std::unique_ptr<RenderCacheIface> pCache) {
    m_pRenderCache = std::move(pCache);
  }

  RenderContextIface* GetRenderContext() const {
    return m_pRenderContext.get();
  }

  // `pContext` cannot be null. `SetRenderContext()` cannot be called if the
  // page already has a render context. Use `ClearRenderContext()` to reset the
  // render context.
  void SetRenderContext(std::unique_ptr<RenderContextIface> pContext);
  void ClearRenderContext();

  CPDF_Document* GetPDFDocument() const { return m_pPDFDocument.Get(); }
  View* GetView() const { return m_pView.Get(); }
  void SetView(View* pView) { m_pView.Reset(pView); }
  void UpdateDimensions();

 private:
  CPDF_Page(CPDF_Document* pDocument, CPDF_Dictionary* pPageDict);
  ~CPDF_Page() override;

  CPDF_Object* GetPageAttr(const ByteString& name) const;
  CFX_FloatRect GetBox(const ByteString& name) const;

  CFX_SizeF m_PageSize;
  CFX_Matrix m_PageMatrix;
  UnownedPtr<CPDF_Document> m_pPDFDocument;
  std::unique_ptr<RenderCacheIface> m_pRenderCache;
  std::unique_ptr<RenderContextIface> m_pRenderContext;
  ObservedPtr<View> m_pView;
};

#endif  // CORE_FPDFAPI_PAGE_CPDF_PAGE_H_
