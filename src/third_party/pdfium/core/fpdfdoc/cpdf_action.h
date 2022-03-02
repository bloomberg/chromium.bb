// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFDOC_CPDF_ACTION_H_
#define CORE_FPDFDOC_CPDF_ACTION_H_

#include <vector>

#include "core/fpdfdoc/cpdf_dest.h"
#include "core/fxcrt/fx_string.h"
#include "core/fxcrt/retain_ptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class CPDF_Dictionary;
class CPDF_Document;
class CPDF_Object;

class CPDF_Action {
 public:
  enum class Type {
    kUnknown = 0,
    kGoTo,
    kGoToR,
    kGoToE,
    kLaunch,
    kThread,
    kURI,
    kSound,
    kMovie,
    kHide,
    kNamed,
    kSubmitForm,
    kResetForm,
    kImportData,
    kJavaScript,
    kSetOCGState,
    kRendition,
    kTrans,
    kGoTo3DView,
    kLast = kGoTo3DView
  };

  explicit CPDF_Action(const CPDF_Dictionary* pDict);
  CPDF_Action(const CPDF_Action& that);
  ~CPDF_Action();

  const CPDF_Dictionary* GetDict() const { return m_pDict.Get(); }

  Type GetType() const;
  CPDF_Dest GetDest(CPDF_Document* pDoc) const;
  WideString GetFilePath() const;
  ByteString GetURI(const CPDF_Document* pDoc) const;
  bool GetHideStatus() const;
  ByteString GetNamedAction() const;
  uint32_t GetFlags() const;

  std::vector<const CPDF_Object*> GetAllFields() const;

  // Differentiates between empty JS entry and no JS entry.
  absl::optional<WideString> MaybeGetJavaScript() const;

  // Returns empty string for empty JS entry and no JS entry.
  WideString GetJavaScript() const;

  size_t GetSubActionsCount() const;
  CPDF_Action GetSubAction(size_t iIndex) const;

 private:
  const CPDF_Object* GetJavaScriptObject() const;

  RetainPtr<const CPDF_Dictionary> const m_pDict;
};

#endif  // CORE_FPDFDOC_CPDF_ACTION_H_
