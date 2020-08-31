// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfdoc/cpdf_aaction.h"

#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fxcrt/fx_memory.h"

namespace {

constexpr const char* kAATypes[] = {
    "E",   // kCursorEnter
    "X",   // kCursorExit
    "D",   // kButtonDown
    "U",   // kButtonUp
    "Fo",  // kGetFocus
    "Bl",  // kLoseFocus
    "PO",  // kPageOpen
    "PC",  // kPageClose
    "PV",  // kPageVisible
    "PI",  // kPageInvisible
    "O",   // kOpenPage
    "C",   // kClosePage
    "K",   // kKeyStroke
    "F",   // kFormat
    "V",   // kValidate
    "C",   // kCalculate
    "WC",  // kCloseDocument
    "WS",  // kSaveDocument
    "DS",  // kDocumentSaved
    "WP",  // kPrintDocument
    "DP",  // kDocumentPrinted
};

// |kAATypes| should have one less element than enum AActionType due to
// |kDocumentOpen|, which is an artificial type.
static_assert(FX_ArraySize(kAATypes) == CPDF_AAction::kNumberOfActions - 1,
              "kAATypes count mismatch");

}  // namespace

CPDF_AAction::CPDF_AAction(const CPDF_Dictionary* pDict) : m_pDict(pDict) {}

CPDF_AAction::CPDF_AAction(const CPDF_AAction& that) = default;

CPDF_AAction::~CPDF_AAction() = default;

bool CPDF_AAction::ActionExist(AActionType eType) const {
  return m_pDict && m_pDict->KeyExist(kAATypes[eType]);
}

CPDF_Action CPDF_AAction::GetAction(AActionType eType) const {
  return CPDF_Action(m_pDict ? m_pDict->GetDictFor(kAATypes[eType]) : nullptr);
}

// static
bool CPDF_AAction::IsUserInput(AActionType type) {
  switch (type) {
    case kButtonUp:
    case kButtonDown:
    case kKeyStroke:
      return true;
    default:
      return false;
  }
}
