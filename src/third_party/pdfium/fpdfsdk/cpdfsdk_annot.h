// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FPDFSDK_CPDFSDK_ANNOT_H_
#define FPDFSDK_CPDFSDK_ANNOT_H_

#include "core/fpdfdoc/cpdf_aaction.h"
#include "core/fpdfdoc/cpdf_annot.h"
#include "core/fpdfdoc/cpdf_defaultappearance.h"
#include "core/fxcrt/observed_ptr.h"
#include "core/fxcrt/unowned_ptr.h"

class CFX_Matrix;
class CFX_RenderDevice;
class CPDF_Page;
class CPDF_RenderOptions;
class CPDFSDK_BAAnnot;
class CPDFSDK_PageView;
class CPDFXFA_Widget;
class IPDF_Page;

class CPDFSDK_Annot : public Observable {
 public:
  explicit CPDFSDK_Annot(CPDFSDK_PageView* pPageView);
  virtual ~CPDFSDK_Annot();

  virtual CPDFSDK_BAAnnot* AsBAAnnot();
  virtual CPDFXFA_Widget* AsXFAWidget();

  virtual int GetLayoutOrder() const;
  virtual CPDF_Annot* GetPDFAnnot() const;
  virtual CPDF_Annot::Subtype GetAnnotSubtype() const;
  virtual bool IsSignatureWidget() const;
  virtual CFX_FloatRect GetRect() const;
  virtual void SetRect(const CFX_FloatRect& rect);

  // Three cases: PDF page only, XFA page only, or XFA page backed by PDF page.
  IPDF_Page* GetPage();     // Returns XFA Page if possible, else PDF page.
  CPDF_Page* GetPDFPage();  // Returns PDF page or nullptr.
  IPDF_Page* GetXFAPage();  // Returns XFA page or nullptr.

  CPDFSDK_PageView* GetPageView() const { return m_pPageView.Get(); }

 protected:
  UnownedPtr<CPDFSDK_PageView> const m_pPageView;
};

inline CPDFSDK_BAAnnot* ToBAAnnot(CPDFSDK_Annot* pAnnot) {
  return pAnnot ? pAnnot->AsBAAnnot() : nullptr;
}

inline CPDFXFA_Widget* ToXFAWidget(CPDFSDK_Annot* pAnnot) {
  return pAnnot ? pAnnot->AsXFAWidget() : nullptr;
}

#endif  // FPDFSDK_CPDFSDK_ANNOT_H_
