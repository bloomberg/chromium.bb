// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDFIUM_PDFIUM_FORM_FILLER_H_
#define PDF_PDFIUM_PDFIUM_FORM_FILLER_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "third_party/pdfium/public/fpdf_formfill.h"
#include "third_party/pdfium/public/fpdfview.h"

namespace chrome_pdf {

class PDFiumEngine;

class PDFiumFormFiller : public FPDF_FORMFILLINFO, public IPDF_JSPLATFORM {
 public:
  PDFiumFormFiller(PDFiumEngine* engine, bool enable_javascript);
  PDFiumFormFiller(const PDFiumFormFiller&) = delete;
  PDFiumFormFiller& operator=(const PDFiumFormFiller&) = delete;
  ~PDFiumFormFiller();

 private:
  friend class FormFillerTest;

  // FPDF_FORMFILLINFO callbacks.
  static void Form_Invalidate(FPDF_FORMFILLINFO* param,
                              FPDF_PAGE page,
                              double left,
                              double top,
                              double right,
                              double bottom);
  static void Form_OutputSelectedRect(FPDF_FORMFILLINFO* param,
                                      FPDF_PAGE page,
                                      double left,
                                      double top,
                                      double right,
                                      double bottom);
  static void Form_SetCursor(FPDF_FORMFILLINFO* param, int cursor_type);
  static int Form_SetTimer(FPDF_FORMFILLINFO* param,
                           int elapse,
                           TimerCallback timer_func);
  static void Form_KillTimer(FPDF_FORMFILLINFO* param, int timer_id);
  static FPDF_SYSTEMTIME Form_GetLocalTime(FPDF_FORMFILLINFO* param);
  static void Form_OnChange(FPDF_FORMFILLINFO* param);
  static FPDF_PAGE Form_GetPage(FPDF_FORMFILLINFO* param,
                                FPDF_DOCUMENT document,
                                int page_index);
  static FPDF_PAGE Form_GetCurrentPage(FPDF_FORMFILLINFO* param,
                                       FPDF_DOCUMENT document);
  static int Form_GetRotation(FPDF_FORMFILLINFO* param, FPDF_PAGE page);
  static void Form_ExecuteNamedAction(FPDF_FORMFILLINFO* param,
                                      FPDF_BYTESTRING named_action);
  static void Form_SetTextFieldFocus(FPDF_FORMFILLINFO* param,
                                     FPDF_WIDESTRING value,
                                     FPDF_DWORD valueLen,
                                     FPDF_BOOL is_focus);
  static void Form_OnFocusChange(FPDF_FORMFILLINFO* param,
                                 FPDF_ANNOTATION annot,
                                 int page_index);
  static void Form_DoURIAction(FPDF_FORMFILLINFO* param, FPDF_BYTESTRING uri);
  static void Form_DoGoToAction(FPDF_FORMFILLINFO* param,
                                int page_index,
                                int zoom_mode,
                                float* position_array,
                                int size_of_array);
  static void Form_DoURIActionWithKeyboardModifier(FPDF_FORMFILLINFO* param,
                                                   FPDF_BYTESTRING uri,
                                                   int modifiers);

#if defined(PDF_ENABLE_XFA)
  static void Form_EmailTo(FPDF_FORMFILLINFO* param,
                           FPDF_FILEHANDLER* file_handler,
                           FPDF_WIDESTRING to,
                           FPDF_WIDESTRING subject,
                           FPDF_WIDESTRING cc,
                           FPDF_WIDESTRING bcc,
                           FPDF_WIDESTRING message);
  static void Form_DisplayCaret(FPDF_FORMFILLINFO* param,
                                FPDF_PAGE page,
                                FPDF_BOOL visible,
                                double left,
                                double top,
                                double right,
                                double bottom);
  static void Form_SetCurrentPage(FPDF_FORMFILLINFO* param,
                                  FPDF_DOCUMENT document,
                                  int page);
  static int Form_GetCurrentPageIndex(FPDF_FORMFILLINFO* param,
                                      FPDF_DOCUMENT document);
  static void Form_GetPageViewRect(FPDF_FORMFILLINFO* param,
                                   FPDF_PAGE page,
                                   double* left,
                                   double* top,
                                   double* right,
                                   double* bottom);
  static int Form_GetPlatform(FPDF_FORMFILLINFO* param,
                              void* platform,
                              int length);
  static void Form_PageEvent(FPDF_FORMFILLINFO* param,
                             int page_count,
                             unsigned long event_type);
  static FPDF_BOOL Form_PopupMenu(FPDF_FORMFILLINFO* param,
                                  FPDF_PAGE page,
                                  FPDF_WIDGET widget,
                                  int menu_flag,
                                  float x,
                                  float y);
  static FPDF_BOOL Form_PostRequestURL(FPDF_FORMFILLINFO* param,
                                       FPDF_WIDESTRING url,
                                       FPDF_WIDESTRING data,
                                       FPDF_WIDESTRING content_type,
                                       FPDF_WIDESTRING encode,
                                       FPDF_WIDESTRING header,
                                       FPDF_BSTR* response);
  static FPDF_BOOL Form_PutRequestURL(FPDF_FORMFILLINFO* param,
                                      FPDF_WIDESTRING url,
                                      FPDF_WIDESTRING data,
                                      FPDF_WIDESTRING encode);
  static void Form_UploadTo(FPDF_FORMFILLINFO* param,
                            FPDF_FILEHANDLER* file_handler,
                            int file_flag,
                            FPDF_WIDESTRING dest);
  static FPDF_FILEHANDLER* Form_DownloadFromURL(FPDF_FORMFILLINFO* param,
                                                FPDF_WIDESTRING url);
  static FPDF_FILEHANDLER* Form_OpenFile(FPDF_FORMFILLINFO* param,
                                         int file_flag,
                                         FPDF_WIDESTRING url,
                                         const char* mode);
  static void Form_GotoURL(FPDF_FORMFILLINFO* param,
                           FPDF_DOCUMENT document,
                           FPDF_WIDESTRING url);
  static int Form_GetLanguage(FPDF_FORMFILLINFO* param,
                              void* language,
                              int length);
#endif  // defined(PDF_ENABLE_XFA)

  // IPDF_JSPLATFORM callbacks.
  static int Form_Alert(IPDF_JSPLATFORM* param,
                        FPDF_WIDESTRING message,
                        FPDF_WIDESTRING title,
                        int type,
                        int icon);
  static void Form_Beep(IPDF_JSPLATFORM* param, int type);
  static int Form_Response(IPDF_JSPLATFORM* param,
                           FPDF_WIDESTRING question,
                           FPDF_WIDESTRING title,
                           FPDF_WIDESTRING default_response,
                           FPDF_WIDESTRING label,
                           FPDF_BOOL password,
                           void* response,
                           int length);
  static int Form_GetFilePath(IPDF_JSPLATFORM* param,
                              void* file_path,
                              int length);
  static void Form_Mail(IPDF_JSPLATFORM* param,
                        void* mail_data,
                        int length,
                        FPDF_BOOL ui,
                        FPDF_WIDESTRING to,
                        FPDF_WIDESTRING subject,
                        FPDF_WIDESTRING cc,
                        FPDF_WIDESTRING bcc,
                        FPDF_WIDESTRING message);
  static void Form_Print(IPDF_JSPLATFORM* param,
                         FPDF_BOOL ui,
                         int start,
                         int end,
                         FPDF_BOOL silent,
                         FPDF_BOOL shrink_to_fit,
                         FPDF_BOOL print_as_image,
                         FPDF_BOOL reverse,
                         FPDF_BOOL annotations);
  static void Form_SubmitForm(IPDF_JSPLATFORM* param,
                              void* form_data,
                              int length,
                              FPDF_WIDESTRING url);
  static void Form_GotoPage(IPDF_JSPLATFORM* param, int page_number);

  static PDFiumEngine* GetEngine(FPDF_FORMFILLINFO* info);
  static PDFiumEngine* GetEngine(IPDF_JSPLATFORM* platform);

  int SetTimer(const base::TimeDelta& delay, TimerCallback timer_func);
  void KillTimer(int timer_id);

  PDFiumEngine* const engine_;

  std::map<int, std::unique_ptr<base::RepeatingTimer>> timers_;
};

}  // namespace chrome_pdf

#endif  // PDF_PDFIUM_PDFIUM_FORM_FILLER_H_
