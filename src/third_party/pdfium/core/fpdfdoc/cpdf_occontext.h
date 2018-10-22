// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFDOC_CPDF_OCCONTEXT_H_
#define CORE_FPDFDOC_CPDF_OCCONTEXT_H_

#include <map>

#include "core/fxcrt/fx_string.h"
#include "core/fxcrt/retain_ptr.h"

class CPDF_Array;
class CPDF_Dictionary;
class CPDF_Document;
class CPDF_PageObject;

class CPDF_OCContext final : public Retainable {
 public:
  enum UsageType { View = 0, Design, Print, Export };

  template <typename T, typename... Args>
  friend RetainPtr<T> pdfium::MakeRetain(Args&&... args);

  bool CheckOCGVisible(const CPDF_Dictionary* pOCGDict);
  bool CheckObjectVisible(const CPDF_PageObject* pObj);

 private:
  CPDF_OCContext(CPDF_Document* pDoc, UsageType eUsageType);
  ~CPDF_OCContext() override;

  bool LoadOCGStateFromConfig(const ByteString& csConfig,
                              const CPDF_Dictionary* pOCGDict) const;
  bool LoadOCGState(const CPDF_Dictionary* pOCGDict) const;
  bool GetOCGVisible(const CPDF_Dictionary* pOCGDict);
  bool GetOCGVE(const CPDF_Array* pExpression, int nLevel);
  bool LoadOCMDState(const CPDF_Dictionary* pOCMDDict);

  UnownedPtr<CPDF_Document> const m_pDocument;
  const UsageType m_eUsageType;
  std::map<const CPDF_Dictionary*, bool> m_OCGStates;
};

#endif  // CORE_FPDFDOC_CPDF_OCCONTEXT_H_
