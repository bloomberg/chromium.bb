// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_C_PRIVATE_PPB_PDF_H_
#define PPAPI_C_PRIVATE_PPB_PDF_H_

#include <stdint.h>

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/private/pp_private_font_charset.h"

#define PPB_PDF_INTERFACE "PPB_PDF;1"

typedef enum {
  PP_PDFFEATURE_HIDPI = 0,
  PP_PDFFEATURE_PRINTING = 1
} PP_PDFFeature;

struct PP_PrivateFontFileDescription {
  const char* face;
  uint32_t weight;
  bool italic;
};

struct PP_PrivateFindResult {
  int start_index;
  int length;
};

struct PP_PrivateAccessibilityViewportInfo {
  double zoom;
  struct PP_Point scroll;
  struct PP_Point offset;
  uint32_t selection_start_page_index;
  uint32_t selection_start_char_index;
  uint32_t selection_end_page_index;
  uint32_t selection_end_char_index;
};

struct PP_PrivateAccessibilityDocInfo {
  uint32_t page_count;
  PP_Bool text_accessible;
  PP_Bool text_copyable;
};

typedef enum {
  PP_PRIVATEDIRECTION_NONE = 0,
  PP_PRIVATEDIRECTION_LTR = 1,
  PP_PRIVATEDIRECTION_RTL = 2,
  PP_PRIVATEDIRECTION_TTB = 3,
  PP_PRIVATEDIRECTION_BTT = 4,
  PP_PRIVATEDIRECTION_LAST = PP_PRIVATEDIRECTION_BTT
} PP_PrivateDirection;

struct PP_PrivateAccessibilityPageInfo {
  uint32_t page_index;
  struct PP_Rect bounds;
  uint32_t text_run_count;
  uint32_t char_count;
  uint32_t link_count;
  uint32_t image_count;
};

struct PP_PrivateAccessibilityTextRunInfo {
  uint32_t len;
  double font_size;
  struct PP_FloatRect bounds;
  PP_PrivateDirection direction;
};

struct PP_PrivateAccessibilityCharInfo {
  uint32_t unicode_character;
  double char_width;
};

// This holds the link information provided by the PDF and will be used in
// accessibility to provide the link information.
struct PP_PrivateAccessibilityLinkInfo {
  // URL of the link.
  const char* url;
  uint32_t url_length;
  // Index of the link in the page. This will be used to identify the link on
  // which action has to be performed in the page.
  uint32_t index_in_page;
  // Link can either be part of the page text or not. If the link is part of the
  // page text, then |text_run_index| denotes the text run which contains the
  // start_index of the link and the |text_run_count| equals the number of text
  // runs the link spans in the page text. If the link is not part of the page
  // text then |text_run_count| should be 0 and the |text_run_index| should
  // contain the nearest char index to the bounding rectangle of the link.
  uint32_t text_run_index;
  uint32_t text_run_count;
  // Bounding box of the link.
  struct PP_FloatRect bounds;
};

// This holds the image information provided by the PDF and will be used in
// accessibility to provide the image information.
struct PP_PrivateAccessibilityImageInfo {
  // Alternate text for the image provided by PDF.
  const char* alt_text;
  uint32_t alt_text_length;
  // We anchor the image to a char index, this denotes the text run before
  // which the image should be inserted in the accessibility tree. The text run
  // at this index should contain the anchor char index.
  uint32_t text_run_index;
  // Bounding box of the image.
  struct PP_FloatRect bounds;
};

struct PPB_PDF {
  // Returns a resource identifying a font file corresponding to the given font
  // request after applying the browser-specific fallback.
  //
  // Currently Linux-only.
  PP_Resource (*GetFontFileWithFallback)(
      PP_Instance instance,
      const struct PP_BrowserFont_Trusted_Description* description,
      PP_PrivateFontCharset charset);

  // Given a resource previously returned by GetFontFileWithFallback, returns
  // a pointer to the requested font table. Linux only.
  bool (*GetFontTableForPrivateFontFile)(PP_Resource font_file,
                                         uint32_t table,
                                         void* output,
                                         uint32_t* output_length);

  // Search the given string using ICU.  Use PPB_Core's MemFree on results when
  // done.
  void (*SearchString)(PP_Instance instance,
                       const unsigned short* string,
                       const unsigned short* term,
                       bool case_sensitive,
                       struct PP_PrivateFindResult** results,
                       uint32_t* count);

  // Since WebFrame doesn't know about PPAPI requests, it'll think the page has
  // finished loading even if there are outstanding requests by the plugin.
  // Take this out once WebFrame knows about requests by PPAPI plugins.
  void (*DidStartLoading)(PP_Instance instance);
  void (*DidStopLoading)(PP_Instance instance);

  // Sets content restriction for a full-page plugin (i.e. can't copy/print).
  // The value is a bitfield of ContentRestriction enums.
  void (*SetContentRestriction)(PP_Instance instance, int restrictions);

  // Notifies the browser that the given action has been performed.
  void (*UserMetricsRecordAction)(PP_Instance instance, struct PP_Var action);

  // Notifies the browser that the PDF has an unsupported feature.
  void (*HasUnsupportedFeature)(PP_Instance instance);

  // Invoke SaveAs... dialog, similar to the right-click or wrench menu.
  void (*SaveAs)(PP_Instance instance);

  // Invoke Print dialog for plugin.
  void (*Print)(PP_Instance instance);

  PP_Bool(*IsFeatureEnabled)(PP_Instance instance, PP_PDFFeature feature);

  // Sets the selected text of the plugin.
  void(*SetSelectedText)(PP_Instance instance, const char* selected_text);

  // Sets the link currently under the cursor.
  void (*SetLinkUnderCursor)(PP_Instance instance, const char* url);

  // Gets pointers to both the mmap'd V8 snapshot files and their sizes.
  // This is needed when loading V8's initial snapshot from external files.
  void (*GetV8ExternalSnapshotData)(PP_Instance instance,
                                    const char** natives_data_out,
                                    int* natives_size_out,
                                    const char** snapshot_data_out,
                                    int* snapshot_size_out);

  // Sends information about the viewport to the renderer for accessibility
  // support.
  void (*SetAccessibilityViewportInfo)(
      PP_Instance instance,
      struct PP_PrivateAccessibilityViewportInfo* viewport_info);

  // Sends information about the PDF document to the renderer for accessibility
  // support.
  void (*SetAccessibilityDocInfo)(
      PP_Instance instance,
      struct PP_PrivateAccessibilityDocInfo* doc_info);

  // Sends information about one page in a PDF document to the renderer for
  // accessibility support.
  void (*SetAccessibilityPageInfo)(
      PP_Instance instance,
      struct PP_PrivateAccessibilityPageInfo* page_info,
      struct PP_PrivateAccessibilityTextRunInfo text_runs[],
      struct PP_PrivateAccessibilityCharInfo chars[],
      struct PP_PrivateAccessibilityLinkInfo links[],
      struct PP_PrivateAccessibilityImageInfo images[]);

  // Sends information about the PDF's URL and the embedder's URL.
  void (*SetCrashData)(PP_Instance instance,
                       const char* pdf_url,
                       const char* top_level_url);

  // Sets the current selection bounding edges.
  void (*SelectionChanged)(PP_Instance instance,
                           const struct PP_FloatPoint* left,
                           int32_t left_height,
                           const struct PP_FloatPoint* right,
                           int32_t right_height);

  // Sets whether the PDF viewer can handle save commands internally.
  void (*SetPluginCanSave)(PP_Instance instance, bool can_save);

  // Displays an alert dialog.
  void (*ShowAlertDialog)(PP_Instance instance, const char* message);

  // Displays a confirmation dialog. This method is synchronous.
  bool (*ShowConfirmDialog)(PP_Instance instance, const char* message);

  // Displays a prompt dialog. This method is synchronous.
  struct PP_Var (*ShowPromptDialog)(PP_Instance instance,
                                    const char* message,
                                    const char* default_answer);
};

#endif  // PPAPI_C_PRIVATE_PPB_PDF_H_
