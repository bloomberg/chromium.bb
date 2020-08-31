// Copyright 2019 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/fpdf_javascript.h"

#include <memory>

#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fpdfdoc/cpdf_action.h"
#include "core/fpdfdoc/cpdf_nametree.h"
#include "fpdfsdk/cpdfsdk_helpers.h"
#include "third_party/base/ptr_util.h"

struct CPDF_JavaScript {
  WideString name;
  WideString script;
};

FPDF_EXPORT int FPDF_CALLCONV
FPDFDoc_GetJavaScriptActionCount(FPDF_DOCUMENT document) {
  CPDF_Document* doc = CPDFDocumentFromFPDFDocument(document);
  if (!doc)
    return -1;

  auto name_tree = CPDF_NameTree::Create(doc, "JavaScript");
  return name_tree ? name_tree->GetCount() : 0;
}

FPDF_EXPORT FPDF_JAVASCRIPT_ACTION FPDF_CALLCONV
FPDFDoc_GetJavaScriptAction(FPDF_DOCUMENT document, int index) {
  CPDF_Document* doc = CPDFDocumentFromFPDFDocument(document);
  if (!doc || index < 0)
    return nullptr;

  auto name_tree = CPDF_NameTree::Create(doc, "JavaScript");
  if (!name_tree || static_cast<size_t>(index) >= name_tree->GetCount())
    return nullptr;

  WideString name;
  CPDF_Dictionary* obj =
      ToDictionary(name_tree->LookupValueAndName(index, &name));
  if (!obj)
    return nullptr;

  // Validate |obj|. Type is optional, but must be valid if present.
  CPDF_Action action(obj);
  if (action.GetType() != CPDF_Action::JavaScript)
    return nullptr;

  Optional<WideString> script = action.MaybeGetJavaScript();
  if (!script.has_value())
    return nullptr;

  auto js = pdfium::MakeUnique<CPDF_JavaScript>();
  js->name = name;
  js->script = script.value();
  return FPDFJavaScriptActionFromCPDFJavaScriptAction(js.release());
}

FPDF_EXPORT void FPDF_CALLCONV
FPDFDoc_CloseJavaScriptAction(FPDF_JAVASCRIPT_ACTION javascript) {
  // Take object back across API and destroy it.
  std::unique_ptr<CPDF_JavaScript>(
      CPDFJavaScriptActionFromFPDFJavaScriptAction(javascript));
}

FPDF_EXPORT unsigned long FPDF_CALLCONV
FPDFJavaScriptAction_GetName(FPDF_JAVASCRIPT_ACTION javascript,
                             FPDF_WCHAR* buffer,
                             unsigned long buflen) {
  CPDF_JavaScript* js =
      CPDFJavaScriptActionFromFPDFJavaScriptAction(javascript);
  if (!js)
    return 0;
  return Utf16EncodeMaybeCopyAndReturnLength(js->name, buffer, buflen);
}

FPDF_EXPORT unsigned long FPDF_CALLCONV
FPDFJavaScriptAction_GetScript(FPDF_JAVASCRIPT_ACTION javascript,
                               FPDF_WCHAR* buffer,
                               unsigned long buflen) {
  CPDF_JavaScript* js =
      CPDFJavaScriptActionFromFPDFJavaScriptAction(javascript);
  if (!js)
    return 0;
  return Utf16EncodeMaybeCopyAndReturnLength(js->script, buffer, buflen);
}
