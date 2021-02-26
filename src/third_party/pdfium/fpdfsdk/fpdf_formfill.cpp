// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "public/fpdf_formfill.h"

#include <memory>
#include <utility>

#include "constants/form_fields.h"
#include "core/fpdfapi/page/cpdf_annotcontext.h"
#include "core/fpdfapi/page/cpdf_occontext.h"
#include "core/fpdfapi/page/cpdf_page.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fpdfapi/render/cpdf_renderoptions.h"
#include "core/fpdfdoc/cpdf_formcontrol.h"
#include "core/fpdfdoc/cpdf_formfield.h"
#include "core/fpdfdoc/cpdf_interactiveform.h"
#include "core/fxge/cfx_defaultrenderdevice.h"
#include "fpdfsdk/cpdfsdk_actionhandler.h"
#include "fpdfsdk/cpdfsdk_annot.h"
#include "fpdfsdk/cpdfsdk_baannothandler.h"
#include "fpdfsdk/cpdfsdk_formfillenvironment.h"
#include "fpdfsdk/cpdfsdk_helpers.h"
#include "fpdfsdk/cpdfsdk_interactiveform.h"
#include "fpdfsdk/cpdfsdk_pageview.h"
#include "fpdfsdk/cpdfsdk_widgethandler.h"
#include "public/fpdfview.h"

#ifdef PDF_ENABLE_XFA
#include "fpdfsdk/fpdfxfa/cpdfxfa_context.h"
#include "fpdfsdk/fpdfxfa/cpdfxfa_page.h"
#include "fpdfsdk/fpdfxfa/cpdfxfa_widgethandler.h"

static_assert(static_cast<int>(AlertButton::kDefault) ==
                  JSPLATFORM_ALERT_BUTTON_DEFAULT,
              "Default alert button types must match");
static_assert(static_cast<int>(AlertButton::kOK) == JSPLATFORM_ALERT_BUTTON_OK,
              "OK alert button types must match");
static_assert(static_cast<int>(AlertButton::kOKCancel) ==
                  JSPLATFORM_ALERT_BUTTON_OKCANCEL,
              "OKCancel alert button types must match");
static_assert(static_cast<int>(AlertButton::kYesNo) ==
                  JSPLATFORM_ALERT_BUTTON_YESNO,
              "YesNo alert button types must match");
static_assert(static_cast<int>(AlertButton::kYesNoCancel) ==
                  JSPLATFORM_ALERT_BUTTON_YESNOCANCEL,
              "YesNoCancel alert button types must match");

static_assert(static_cast<int>(AlertIcon::kDefault) ==
                  JSPLATFORM_ALERT_ICON_DEFAULT,
              "Default alert icon types must match");
static_assert(static_cast<int>(AlertIcon::kError) ==
                  JSPLATFORM_ALERT_ICON_ERROR,
              "Error alert icon types must match");
static_assert(static_cast<int>(AlertIcon::kWarning) ==
                  JSPLATFORM_ALERT_ICON_WARNING,
              "Warning alert icon types must match");
static_assert(static_cast<int>(AlertIcon::kQuestion) ==
                  JSPLATFORM_ALERT_ICON_QUESTION,
              "Question alert icon types must match");
static_assert(static_cast<int>(AlertIcon::kStatus) ==
                  JSPLATFORM_ALERT_ICON_STATUS,
              "Status alert icon types must match");
static_assert(static_cast<int>(AlertIcon::kAsterisk) ==
                  JSPLATFORM_ALERT_ICON_ASTERISK,
              "Asterisk alert icon types must match");

static_assert(static_cast<int>(AlertReturn::kOK) == JSPLATFORM_ALERT_RETURN_OK,
              "OK alert return types must match");
static_assert(static_cast<int>(AlertReturn::kCancel) ==
                  JSPLATFORM_ALERT_RETURN_CANCEL,
              "Cancel alert return types must match");
static_assert(static_cast<int>(AlertReturn::kNo) == JSPLATFORM_ALERT_RETURN_NO,
              "No alert return types must match");
static_assert(static_cast<int>(AlertReturn::kYes) ==
                  JSPLATFORM_ALERT_RETURN_YES,
              "Yes alert return types must match");

static_assert(static_cast<int>(FormType::kNone) == FORMTYPE_NONE,
              "None form types must match");
static_assert(static_cast<int>(FormType::kAcroForm) == FORMTYPE_ACRO_FORM,
              "AcroForm form types must match");
static_assert(static_cast<int>(FormType::kXFAFull) == FORMTYPE_XFA_FULL,
              "XFA full form types must match");
static_assert(static_cast<int>(FormType::kXFAForeground) ==
                  FORMTYPE_XFA_FOREGROUND,
              "XFA foreground form types must match");
#endif  // PDF_ENABLE_XFA

static_assert(static_cast<int>(FormFieldType::kUnknown) ==
                  FPDF_FORMFIELD_UNKNOWN,
              "Unknown form field types must match");
static_assert(static_cast<int>(FormFieldType::kPushButton) ==
                  FPDF_FORMFIELD_PUSHBUTTON,
              "PushButton form field types must match");
static_assert(static_cast<int>(FormFieldType::kCheckBox) ==
                  FPDF_FORMFIELD_CHECKBOX,
              "CheckBox form field types must match");
static_assert(static_cast<int>(FormFieldType::kRadioButton) ==
                  FPDF_FORMFIELD_RADIOBUTTON,
              "RadioButton form field types must match");
static_assert(static_cast<int>(FormFieldType::kComboBox) ==
                  FPDF_FORMFIELD_COMBOBOX,
              "ComboBox form field types must match");
static_assert(static_cast<int>(FormFieldType::kListBox) ==
                  FPDF_FORMFIELD_LISTBOX,
              "ListBox form field types must match");
static_assert(static_cast<int>(FormFieldType::kTextField) ==
                  FPDF_FORMFIELD_TEXTFIELD,
              "TextField form field types must match");
static_assert(static_cast<int>(FormFieldType::kSignature) ==
                  FPDF_FORMFIELD_SIGNATURE,
              "Signature form field types must match");
#ifdef PDF_ENABLE_XFA
static_assert(static_cast<int>(FormFieldType::kXFA) == FPDF_FORMFIELD_XFA,
              "XFA form field types must match");
static_assert(static_cast<int>(FormFieldType::kXFA_CheckBox) ==
                  FPDF_FORMFIELD_XFA_CHECKBOX,
              "XFA CheckBox form field types must match");
static_assert(static_cast<int>(FormFieldType::kXFA_ComboBox) ==
                  FPDF_FORMFIELD_XFA_COMBOBOX,
              "XFA ComboBox form field types must match");
static_assert(static_cast<int>(FormFieldType::kXFA_ImageField) ==
                  FPDF_FORMFIELD_XFA_IMAGEFIELD,
              "XFA ImageField form field types must match");
static_assert(static_cast<int>(FormFieldType::kXFA_ListBox) ==
                  FPDF_FORMFIELD_XFA_LISTBOX,
              "XFA ListBox form field types must match");
static_assert(static_cast<int>(FormFieldType::kXFA_PushButton) ==
                  FPDF_FORMFIELD_XFA_PUSHBUTTON,
              "XFA PushButton form field types must match");
static_assert(static_cast<int>(FormFieldType::kXFA_Signature) ==
                  FPDF_FORMFIELD_XFA_SIGNATURE,
              "XFA Signature form field types must match");
static_assert(static_cast<int>(FormFieldType::kXFA_TextField) ==
                  FPDF_FORMFIELD_XFA_TEXTFIELD,
              "XFA TextField form field types must match");
#endif  // PDF_ENABLE_XFA
static_assert(kFormFieldTypeCount == FPDF_FORMFIELD_COUNT,
              "Number of form field types must match");

static_assert(static_cast<int>(CPDF_AAction::kCloseDocument) ==
                  FPDFDOC_AACTION_WC,
              "CloseDocument action must match");
static_assert(static_cast<int>(CPDF_AAction::kSaveDocument) ==
                  FPDFDOC_AACTION_WS,
              "SaveDocument action must match");
static_assert(static_cast<int>(CPDF_AAction::kDocumentSaved) ==
                  FPDFDOC_AACTION_DS,
              "DocumentSaved action must match");
static_assert(static_cast<int>(CPDF_AAction::kPrintDocument) ==
                  FPDFDOC_AACTION_WP,
              "PrintDocument action must match");
static_assert(static_cast<int>(CPDF_AAction::kDocumentPrinted) ==
                  FPDFDOC_AACTION_DP,
              "DocumentPrinted action must match");

namespace {

CPDFSDK_PageView* FormHandleToPageView(FPDF_FORMHANDLE hHandle,
                                       FPDF_PAGE fpdf_page) {
  IPDF_Page* pPage = IPDFPageFromFPDFPage(fpdf_page);
  if (!pPage)
    return nullptr;

  CPDFSDK_FormFillEnvironment* pFormFillEnv =
      CPDFSDKFormFillEnvironmentFromFPDFFormHandle(hHandle);
  return pFormFillEnv ? pFormFillEnv->GetPageView(pPage, true) : nullptr;
}

void FFLCommon(FPDF_FORMHANDLE hHandle,
               FPDF_BITMAP bitmap,
               FPDF_RECORDER recorder,
               FPDF_PAGE fpdf_page,
               int start_x,
               int start_y,
               int size_x,
               int size_y,
               int rotate,
               int flags) {
  if (!hHandle)
    return;

  IPDF_Page* pPage = IPDFPageFromFPDFPage(fpdf_page);
  if (!pPage)
    return;

  CPDF_Document* pPDFDoc = pPage->GetDocument();
  CPDFSDK_PageView* pPageView = FormHandleToPageView(hHandle, fpdf_page);

  const FX_RECT rect(start_x, start_y, start_x + size_x, start_y + size_y);
  CFX_Matrix matrix = pPage->GetDisplayMatrix(rect, rotate);

  auto pDevice = std::make_unique<CFX_DefaultRenderDevice>();
#if defined(_SKIA_SUPPORT_)
  pDevice->AttachRecorder(static_cast<SkPictureRecorder*>(recorder));
#endif

  RetainPtr<CFX_DIBitmap> holder(CFXDIBitmapFromFPDFBitmap(bitmap));
  pDevice->Attach(holder, !!(flags & FPDF_REVERSE_BYTE_ORDER), nullptr, false);
  {
    CFX_RenderDevice::StateRestorer restorer(pDevice.get());
    pDevice->SetClip_Rect(rect);

    CPDF_RenderOptions options;
    options.GetOptions().bClearType = !!(flags & FPDF_LCD_TEXT);

    // Grayscale output
    if (flags & FPDF_GRAYSCALE)
      options.SetColorMode(CPDF_RenderOptions::kGray);

    options.SetDrawAnnots(flags & FPDF_ANNOT);
    options.SetOCContext(
        pdfium::MakeRetain<CPDF_OCContext>(pPDFDoc, CPDF_OCContext::View));

    if (pPageView)
      pPageView->PageView_OnDraw(pDevice.get(), matrix, &options, rect);
  }

#if defined(_SKIA_SUPPORT_PATHS_)
  pDevice->Flush(true);
  holder->UnPreMultiply();
#endif
}

// Returns true if formfill version is correctly set. See |version| in
// FPDF_FORMFILLINFO for details regarding correct version.
bool CheckFormfillVersion(FPDF_FORMFILLINFO* formInfo) {
  if (!formInfo || formInfo->version < 1 || formInfo->version > 2)
    return false;

#ifdef PDF_ENABLE_XFA
  if (formInfo->version != 2)
    return false;
#endif  // PDF_ENABLE_XFA

  return true;
}

}  // namespace

FPDF_EXPORT int FPDF_CALLCONV
FPDFPage_HasFormFieldAtPoint(FPDF_FORMHANDLE hHandle,
                             FPDF_PAGE page,
                             double page_x,
                             double page_y) {
  CPDF_Page* pPage = CPDFPageFromFPDFPage(page);
  if (pPage) {
    CPDFSDK_InteractiveForm* pForm = FormHandleToInteractiveForm(hHandle);
    if (!pForm)
      return -1;

    CPDF_InteractiveForm* pPDFForm = pForm->GetInteractiveForm();
    CPDF_FormControl* pFormCtrl = pPDFForm->GetControlAtPoint(
        pPage,
        CFX_PointF(static_cast<float>(page_x), static_cast<float>(page_y)),
        nullptr);
    if (!pFormCtrl)
      return -1;
    CPDF_FormField* pFormField = pFormCtrl->GetField();
    return pFormField ? static_cast<int>(pFormField->GetFieldType()) : -1;
  }

#ifdef PDF_ENABLE_XFA
  CPDFXFA_Page* pXFAPage = ToXFAPage(IPDFPageFromFPDFPage(page));
  if (pXFAPage) {
    return pXFAPage->HasFormFieldAtPoint(
        CFX_PointF(static_cast<float>(page_x), static_cast<float>(page_y)));
  }
#endif  // PDF_ENABLE_XFA

  return -1;
}

FPDF_EXPORT int FPDF_CALLCONV
FPDFPage_FormFieldZOrderAtPoint(FPDF_FORMHANDLE hHandle,
                                FPDF_PAGE page,
                                double page_x,
                                double page_y) {
  CPDFSDK_InteractiveForm* pForm = FormHandleToInteractiveForm(hHandle);
  if (!pForm)
    return -1;

  CPDF_Page* pPage = CPDFPageFromFPDFPage(page);
  if (!pPage)
    return -1;

  CPDF_InteractiveForm* pPDFForm = pForm->GetInteractiveForm();
  int z_order = -1;
  pPDFForm->GetControlAtPoint(
      pPage, CFX_PointF(static_cast<float>(page_x), static_cast<float>(page_y)),
      &z_order);
  return z_order;
}

FPDF_EXPORT FPDF_FORMHANDLE FPDF_CALLCONV
FPDFDOC_InitFormFillEnvironment(FPDF_DOCUMENT document,
                                FPDF_FORMFILLINFO* formInfo) {
  if (!CheckFormfillVersion(formInfo))
    return nullptr;

  auto* pDocument = CPDFDocumentFromFPDFDocument(document);
  if (!pDocument)
    return nullptr;

  std::unique_ptr<IPDFSDK_AnnotHandler> pXFAHandler;
#ifdef PDF_ENABLE_XFA
  CPDFXFA_Context* pContext = nullptr;
  if (!formInfo->xfa_disabled) {
    if (!pDocument->GetExtension()) {
      pDocument->SetExtension(std::make_unique<CPDFXFA_Context>(pDocument));
    }

    // If the CPDFXFA_Context has a FormFillEnvironment already then we've done
    // this and can just return the old Env. Otherwise, we'll end up setting a
    // new environment into the XFADocument and, that could get weird.
    pContext = static_cast<CPDFXFA_Context*>(pDocument->GetExtension());
    if (pContext->GetFormFillEnv()) {
      return FPDFFormHandleFromCPDFSDKFormFillEnvironment(
          pContext->GetFormFillEnv());
    }
    pXFAHandler = std::make_unique<CPDFXFA_WidgetHandler>();
  }
#endif  // PDF_ENABLE_XFA

  auto pFormFillEnv = std::make_unique<CPDFSDK_FormFillEnvironment>(
      pDocument, formInfo,
      std::make_unique<CPDFSDK_AnnotHandlerMgr>(
          std::make_unique<CPDFSDK_BAAnnotHandler>(),
          std::make_unique<CPDFSDK_WidgetHandler>(), std::move(pXFAHandler)));

#ifdef PDF_ENABLE_XFA
  if (pContext)
    pContext->SetFormFillEnv(pFormFillEnv.get());
#endif  // PDF_ENABLE_XFA

  ReportUnsupportedXFA(pDocument);

  return FPDFFormHandleFromCPDFSDKFormFillEnvironment(
      pFormFillEnv.release());  // Caller takes ownership.
}

FPDF_EXPORT void FPDF_CALLCONV
FPDFDOC_ExitFormFillEnvironment(FPDF_FORMHANDLE hHandle) {
  if (!hHandle)
    return;

  // Take back ownership of the form fill environment. This is the inverse of
  // FPDFDOC_InitFormFillEnvironment() above.
  std::unique_ptr<CPDFSDK_FormFillEnvironment> pFormFillEnv(
      CPDFSDKFormFillEnvironmentFromFPDFFormHandle(hHandle));

#ifdef PDF_ENABLE_XFA
  // Reset the focused annotations and remove the SDK document from the
  // XFA document.
  pFormFillEnv->ClearAllFocusedAnnots();
  // If the document was closed first, it's possible the XFA document
  // is now a nullptr.
  auto* pContext =
      static_cast<CPDFXFA_Context*>(pFormFillEnv->GetDocExtension());
  if (pContext)
    pContext->SetFormFillEnv(nullptr);
#endif  // PDF_ENABLE_XFA
}

FPDF_EXPORT FPDF_BOOL FPDF_CALLCONV FORM_OnMouseMove(FPDF_FORMHANDLE hHandle,
                                                     FPDF_PAGE page,
                                                     int modifier,
                                                     double page_x,
                                                     double page_y) {
  CPDFSDK_PageView* pPageView = FormHandleToPageView(hHandle, page);
  if (!pPageView)
    return false;
  return pPageView->OnMouseMove(modifier, CFX_PointF(page_x, page_y));
}

FPDF_EXPORT FPDF_BOOL FPDF_CALLCONV
FORM_OnMouseWheel(FPDF_FORMHANDLE hHandle,
                  FPDF_PAGE page,
                  int modifier,
                  const FS_POINTF* page_coord,
                  int delta_x,
                  int delta_y) {
  CPDFSDK_PageView* pPageView = FormHandleToPageView(hHandle, page);
  if (!pPageView || !page_coord)
    return false;
  return pPageView->OnMouseWheel(modifier, CFXPointFFromFSPointF(*page_coord),
                                 CFX_Vector(delta_x, delta_y));
}

FPDF_EXPORT FPDF_BOOL FPDF_CALLCONV FORM_OnFocus(FPDF_FORMHANDLE hHandle,
                                                 FPDF_PAGE page,
                                                 int modifier,
                                                 double page_x,
                                                 double page_y) {
  CPDFSDK_PageView* pPageView = FormHandleToPageView(hHandle, page);
  if (!pPageView)
    return false;
  return pPageView->OnFocus(modifier, CFX_PointF(page_x, page_y));
}

FPDF_EXPORT FPDF_BOOL FPDF_CALLCONV FORM_OnLButtonDown(FPDF_FORMHANDLE hHandle,
                                                       FPDF_PAGE page,
                                                       int modifier,
                                                       double page_x,
                                                       double page_y) {
  CPDFSDK_PageView* pPageView = FormHandleToPageView(hHandle, page);
  if (!pPageView)
    return false;
#ifdef PDF_ENABLE_CLICK_LOGGING
  fprintf(stderr, "mousedown,left,%d,%d\n", static_cast<int>(round(page_x)),
          static_cast<int>(round(page_y)));
#endif  // PDF_ENABLE_CLICK_LOGGING
  return pPageView->OnLButtonDown(modifier, CFX_PointF(page_x, page_y));
}

FPDF_EXPORT FPDF_BOOL FPDF_CALLCONV FORM_OnLButtonUp(FPDF_FORMHANDLE hHandle,
                                                     FPDF_PAGE page,
                                                     int modifier,
                                                     double page_x,
                                                     double page_y) {
  CPDFSDK_PageView* pPageView = FormHandleToPageView(hHandle, page);
  if (!pPageView)
    return false;
#ifdef PDF_ENABLE_CLICK_LOGGING
  fprintf(stderr, "mouseup,left,%d,%d\n", static_cast<int>(round(page_x)),
          static_cast<int>(round(page_y)));
#endif  // PDF_ENABLE_CLICK_LOGGING
  return pPageView->OnLButtonUp(modifier, CFX_PointF(page_x, page_y));
}

FPDF_EXPORT FPDF_BOOL FPDF_CALLCONV
FORM_OnLButtonDoubleClick(FPDF_FORMHANDLE hHandle,
                          FPDF_PAGE page,
                          int modifier,
                          double page_x,
                          double page_y) {
  CPDFSDK_PageView* pPageView = FormHandleToPageView(hHandle, page);
  if (!pPageView)
    return false;
#ifdef PDF_ENABLE_CLICK_LOGGING
  fprintf(stderr, "mousedown,doubleleft,%d,%d\n",
          static_cast<int>(round(page_x)), static_cast<int>(round(page_y)));
#endif  // PDF_ENABLE_CLICK_LOGGING
  return pPageView->OnLButtonDblClk(modifier, CFX_PointF(page_x, page_y));
}

FPDF_EXPORT FPDF_BOOL FPDF_CALLCONV FORM_OnRButtonDown(FPDF_FORMHANDLE hHandle,
                                                       FPDF_PAGE page,
                                                       int modifier,
                                                       double page_x,
                                                       double page_y) {
  CPDFSDK_PageView* pPageView = FormHandleToPageView(hHandle, page);
  if (!pPageView)
    return false;
#ifdef PDF_ENABLE_CLICK_LOGGING
  fprintf(stderr, "mousedown,right,%d,%d\n", static_cast<int>(round(page_x)),
          static_cast<int>(round(page_y)));
#endif  // PDF_ENABLE_CLICK_LOGGING
  return pPageView->OnRButtonDown(modifier, CFX_PointF(page_x, page_y));
}

FPDF_EXPORT FPDF_BOOL FPDF_CALLCONV FORM_OnRButtonUp(FPDF_FORMHANDLE hHandle,
                                                     FPDF_PAGE page,
                                                     int modifier,
                                                     double page_x,
                                                     double page_y) {
  CPDFSDK_PageView* pPageView = FormHandleToPageView(hHandle, page);
  if (!pPageView)
    return false;
#ifdef PDF_ENABLE_CLICK_LOGGING
  fprintf(stderr, "mouseup,right,%d,%d\n", static_cast<int>(round(page_x)),
          static_cast<int>(round(page_y)));
#endif  // PDF_ENABLE_CLICK_LOGGING
  return pPageView->OnRButtonUp(modifier, CFX_PointF(page_x, page_y));
}

FPDF_EXPORT FPDF_BOOL FPDF_CALLCONV FORM_OnKeyDown(FPDF_FORMHANDLE hHandle,
                                                   FPDF_PAGE page,
                                                   int nKeyCode,
                                                   int modifier) {
  CPDFSDK_PageView* pPageView = FormHandleToPageView(hHandle, page);
  if (!pPageView)
    return false;
  return pPageView->OnKeyDown(nKeyCode, modifier);
}

FPDF_EXPORT FPDF_BOOL FPDF_CALLCONV FORM_OnKeyUp(FPDF_FORMHANDLE hHandle,
                                                 FPDF_PAGE page,
                                                 int nKeyCode,
                                                 int modifier) {
  CPDFSDK_PageView* pPageView = FormHandleToPageView(hHandle, page);
  if (!pPageView)
    return false;
  return pPageView->OnKeyUp(nKeyCode, modifier);
}

FPDF_EXPORT FPDF_BOOL FPDF_CALLCONV FORM_OnChar(FPDF_FORMHANDLE hHandle,
                                                FPDF_PAGE page,
                                                int nChar,
                                                int modifier) {
  CPDFSDK_PageView* pPageView = FormHandleToPageView(hHandle, page);
  if (!pPageView)
    return false;
  return pPageView->OnChar(nChar, modifier);
}

FPDF_EXPORT unsigned long FPDF_CALLCONV
FORM_GetFocusedText(FPDF_FORMHANDLE hHandle,
                    FPDF_PAGE page,
                    void* buffer,
                    unsigned long buflen) {
  CPDFSDK_PageView* pPageView = FormHandleToPageView(hHandle, page);
  if (!pPageView)
    return 0;

  return Utf16EncodeMaybeCopyAndReturnLength(pPageView->GetFocusedFormText(),
                                             buffer, buflen);
}

FPDF_EXPORT unsigned long FPDF_CALLCONV
FORM_GetSelectedText(FPDF_FORMHANDLE hHandle,
                     FPDF_PAGE page,
                     void* buffer,
                     unsigned long buflen) {
  CPDFSDK_PageView* pPageView = FormHandleToPageView(hHandle, page);
  if (!pPageView)
    return 0;

  return Utf16EncodeMaybeCopyAndReturnLength(pPageView->GetSelectedText(),
                                             buffer, buflen);
}

FPDF_EXPORT void FPDF_CALLCONV FORM_ReplaceSelection(FPDF_FORMHANDLE hHandle,
                                                     FPDF_PAGE page,
                                                     FPDF_WIDESTRING wsText) {
  CPDFSDK_PageView* pPageView = FormHandleToPageView(hHandle, page);
  if (!pPageView)
    return;

  pPageView->ReplaceSelection(WideStringFromFPDFWideString(wsText));
}

FPDF_EXPORT FPDF_BOOL FPDF_CALLCONV FORM_SelectAllText(FPDF_FORMHANDLE hHandle,
                                                       FPDF_PAGE page) {
  CPDFSDK_PageView* pPageView = FormHandleToPageView(hHandle, page);
  return pPageView && pPageView->SelectAllText();
}

FPDF_EXPORT FPDF_BOOL FPDF_CALLCONV FORM_CanUndo(FPDF_FORMHANDLE hHandle,
                                                 FPDF_PAGE page) {
  CPDFSDK_PageView* pPageView = FormHandleToPageView(hHandle, page);
  if (!pPageView)
    return false;
  return pPageView->CanUndo();
}

FPDF_EXPORT FPDF_BOOL FPDF_CALLCONV FORM_CanRedo(FPDF_FORMHANDLE hHandle,
                                                 FPDF_PAGE page) {
  CPDFSDK_PageView* pPageView = FormHandleToPageView(hHandle, page);
  if (!pPageView)
    return false;
  return pPageView->CanRedo();
}

FPDF_EXPORT FPDF_BOOL FPDF_CALLCONV FORM_Undo(FPDF_FORMHANDLE hHandle,
                                              FPDF_PAGE page) {
  CPDFSDK_PageView* pPageView = FormHandleToPageView(hHandle, page);
  if (!pPageView)
    return false;
  return pPageView->Undo();
}

FPDF_EXPORT FPDF_BOOL FPDF_CALLCONV FORM_Redo(FPDF_FORMHANDLE hHandle,
                                              FPDF_PAGE page) {
  CPDFSDK_PageView* pPageView = FormHandleToPageView(hHandle, page);
  if (!pPageView)
    return false;
  return pPageView->Redo();
}

FPDF_EXPORT FPDF_BOOL FPDF_CALLCONV
FORM_ForceToKillFocus(FPDF_FORMHANDLE hHandle) {
  CPDFSDK_FormFillEnvironment* pFormFillEnv =
      CPDFSDKFormFillEnvironmentFromFPDFFormHandle(hHandle);
  if (!pFormFillEnv)
    return false;
  return pFormFillEnv->KillFocusAnnot(0);
}

FPDF_EXPORT FPDF_BOOL FPDF_CALLCONV
FORM_GetFocusedAnnot(FPDF_FORMHANDLE handle,
                     int* page_index,
                     FPDF_ANNOTATION* annot) {
  if (!page_index || !annot)
    return false;

  CPDFSDK_FormFillEnvironment* form_fill_env =
      CPDFSDKFormFillEnvironmentFromFPDFFormHandle(handle);
  if (!form_fill_env)
    return false;

  // Set |page_index| and |annot| to default values. This is returned when there
  // is no focused annotation.
  *page_index = -1;
  *annot = nullptr;

  CPDFSDK_Annot* cpdfsdk_annot = form_fill_env->GetFocusAnnot();
  if (!cpdfsdk_annot)
    return true;

  // TODO(crbug.com/pdfium/1482): Handle XFA case.
  if (cpdfsdk_annot->AsXFAWidget())
    return true;

  CPDFSDK_PageView* page_view = cpdfsdk_annot->GetPageView();
  if (!page_view->IsValid())
    return true;

  IPDF_Page* page = cpdfsdk_annot->GetPage();
  if (!page)
    return true;

  CPDF_Dictionary* annot_dict = cpdfsdk_annot->GetPDFAnnot()->GetAnnotDict();
  auto annot_context = std::make_unique<CPDF_AnnotContext>(annot_dict, page);

  *page_index = page_view->GetPageIndex();
  // Caller takes ownership.
  *annot = FPDFAnnotationFromCPDFAnnotContext(annot_context.release());
  return true;
}

FPDF_EXPORT FPDF_BOOL FPDF_CALLCONV
FORM_SetFocusedAnnot(FPDF_FORMHANDLE handle, FPDF_ANNOTATION annot) {
  CPDFSDK_FormFillEnvironment* form_fill_env =
      CPDFSDKFormFillEnvironmentFromFPDFFormHandle(handle);
  if (!form_fill_env)
    return false;

  CPDF_AnnotContext* annot_context = CPDFAnnotContextFromFPDFAnnotation(annot);
  if (!annot_context)
    return false;

  CPDFSDK_PageView* page_view =
      form_fill_env->GetPageView(annot_context->GetPage(), true);
  if (!page_view->IsValid())
    return false;

  CPDF_Dictionary* annot_dict = annot_context->GetAnnotDict();

  ObservedPtr<CPDFSDK_Annot> cpdfsdk_annot(
      page_view->GetAnnotByDict(annot_dict));
  if (!cpdfsdk_annot)
    return false;
  return form_fill_env->SetFocusAnnot(&cpdfsdk_annot);
}

FPDF_EXPORT void FPDF_CALLCONV FPDF_FFLDraw(FPDF_FORMHANDLE hHandle,
                                            FPDF_BITMAP bitmap,
                                            FPDF_PAGE page,
                                            int start_x,
                                            int start_y,
                                            int size_x,
                                            int size_y,
                                            int rotate,
                                            int flags) {
  FFLCommon(hHandle, bitmap, nullptr, page, start_x, start_y, size_x, size_y,
            rotate, flags);
}

#if defined(_SKIA_SUPPORT_)
FPDF_EXPORT void FPDF_CALLCONV FPDF_FFLRecord(FPDF_FORMHANDLE hHandle,
                                              FPDF_RECORDER recorder,
                                              FPDF_PAGE page,
                                              int start_x,
                                              int start_y,
                                              int size_x,
                                              int size_y,
                                              int rotate,
                                              int flags) {
  FFLCommon(hHandle, nullptr, recorder, page, start_x, start_y, size_x, size_y,
            rotate, flags);
}
#endif

FPDF_EXPORT void FPDF_CALLCONV
FPDF_SetFormFieldHighlightColor(FPDF_FORMHANDLE hHandle,
                                int fieldType,
                                unsigned long color) {
  CPDFSDK_InteractiveForm* pForm = FormHandleToInteractiveForm(hHandle);
  if (!pForm)
    return;

  Optional<FormFieldType> cast_input =
      CPDF_FormField::IntToFormFieldType(fieldType);
  if (!cast_input)
    return;

  if (cast_input.value() == FormFieldType::kUnknown)
    pForm->SetAllHighlightColors(color);
  else
    pForm->SetHighlightColor(color, cast_input.value());
}

FPDF_EXPORT void FPDF_CALLCONV
FPDF_SetFormFieldHighlightAlpha(FPDF_FORMHANDLE hHandle, unsigned char alpha) {
  if (CPDFSDK_InteractiveForm* pForm = FormHandleToInteractiveForm(hHandle))
    pForm->SetHighlightAlpha(alpha);
}

FPDF_EXPORT void FPDF_CALLCONV
FPDF_RemoveFormFieldHighlight(FPDF_FORMHANDLE hHandle) {
  if (CPDFSDK_InteractiveForm* pForm = FormHandleToInteractiveForm(hHandle))
    pForm->RemoveAllHighLights();
}

FPDF_EXPORT void FPDF_CALLCONV FORM_OnAfterLoadPage(FPDF_PAGE page,
                                                    FPDF_FORMHANDLE hHandle) {
  if (CPDFSDK_PageView* pPageView = FormHandleToPageView(hHandle, page))
    pPageView->SetValid(true);
}

FPDF_EXPORT void FPDF_CALLCONV FORM_OnBeforeClosePage(FPDF_PAGE page,
                                                      FPDF_FORMHANDLE hHandle) {
  CPDFSDK_FormFillEnvironment* pFormFillEnv =
      CPDFSDKFormFillEnvironmentFromFPDFFormHandle(hHandle);
  if (!pFormFillEnv)
    return;

  IPDF_Page* pPage = IPDFPageFromFPDFPage(page);
  if (!pPage)
    return;

  CPDFSDK_PageView* pPageView = pFormFillEnv->GetPageView(pPage, false);
  if (pPageView) {
    pPageView->SetValid(false);
    // RemovePageView() takes care of the delete for us.
    pFormFillEnv->RemovePageView(pPage);
  }
}

FPDF_EXPORT void FPDF_CALLCONV
FORM_DoDocumentJSAction(FPDF_FORMHANDLE hHandle) {
  CPDFSDK_FormFillEnvironment* pFormFillEnv =
      CPDFSDKFormFillEnvironmentFromFPDFFormHandle(hHandle);
  if (pFormFillEnv && pFormFillEnv->IsJSPlatformPresent())
    pFormFillEnv->ProcJavascriptAction();
}

FPDF_EXPORT void FPDF_CALLCONV
FORM_DoDocumentOpenAction(FPDF_FORMHANDLE hHandle) {
  CPDFSDK_FormFillEnvironment* pFormFillEnv =
      CPDFSDKFormFillEnvironmentFromFPDFFormHandle(hHandle);
  if (pFormFillEnv && pFormFillEnv->IsJSPlatformPresent())
    pFormFillEnv->ProcOpenAction();
}

FPDF_EXPORT void FPDF_CALLCONV FORM_DoDocumentAAction(FPDF_FORMHANDLE hHandle,
                                                      int aaType) {
  CPDFSDK_FormFillEnvironment* pFormFillEnv =
      CPDFSDKFormFillEnvironmentFromFPDFFormHandle(hHandle);
  if (!pFormFillEnv)
    return;

  CPDF_Document* pDoc = pFormFillEnv->GetPDFDocument();
  CPDF_Dictionary* pDict = pDoc->GetRoot();
  if (!pDict)
    return;

  CPDF_AAction aa(pDict->GetDictFor(pdfium::form_fields::kAA));
  auto type = static_cast<CPDF_AAction::AActionType>(aaType);
  if (aa.ActionExist(type)) {
    CPDF_Action action = aa.GetAction(type);
    pFormFillEnv->GetActionHandler()->DoAction_Document(action, type,
                                                        pFormFillEnv);
  }
}

FPDF_EXPORT void FPDF_CALLCONV FORM_DoPageAAction(FPDF_PAGE page,
                                                  FPDF_FORMHANDLE hHandle,
                                                  int aaType) {
  CPDFSDK_FormFillEnvironment* pFormFillEnv =
      CPDFSDKFormFillEnvironmentFromFPDFFormHandle(hHandle);
  if (!pFormFillEnv)
    return;

  IPDF_Page* pPage = IPDFPageFromFPDFPage(page);
  CPDF_Page* pPDFPage = CPDFPageFromFPDFPage(page);
  if (!pPDFPage)
    return;

  if (!pFormFillEnv->GetPageView(pPage, false))
    return;

  CPDFSDK_ActionHandler* pActionHandler = pFormFillEnv->GetActionHandler();
  CPDF_Dictionary* pPageDict = pPDFPage->GetDict();
  CPDF_AAction aa(pPageDict->GetDictFor(pdfium::form_fields::kAA));
  CPDF_AAction::AActionType type = aaType == FPDFPAGE_AACTION_OPEN
                                       ? CPDF_AAction::kOpenPage
                                       : CPDF_AAction::kClosePage;
  if (aa.ActionExist(type)) {
    CPDF_Action action = aa.GetAction(type);
    pActionHandler->DoAction_Page(action, type, pFormFillEnv);
  }
}

FPDF_EXPORT FPDF_BOOL FPDF_CALLCONV
FORM_SetIndexSelected(FPDF_FORMHANDLE hHandle,
                      FPDF_PAGE page,
                      int index,
                      FPDF_BOOL selected) {
  CPDFSDK_PageView* pPageView = FormHandleToPageView(hHandle, page);
  if (!pPageView)
    return false;
  return pPageView->SetIndexSelected(index, selected);
}

FPDF_EXPORT FPDF_BOOL FPDF_CALLCONV
FORM_IsIndexSelected(FPDF_FORMHANDLE hHandle, FPDF_PAGE page, int index) {
  CPDFSDK_PageView* pPageView = FormHandleToPageView(hHandle, page);
  if (!pPageView)
    return false;
  return pPageView->IsIndexSelected(index);
}
