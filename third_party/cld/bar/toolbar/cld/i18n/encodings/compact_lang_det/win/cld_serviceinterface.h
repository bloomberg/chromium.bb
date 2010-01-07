// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BAR_TOOLBAR_CLD_I18N_ENCODINGS_COMPACT_LANG_DET_WIN_CLD_SERVICEINTERFACE_H_
#define BAR_TOOLBAR_CLD_I18N_ENCODINGS_COMPACT_LANG_DET_WIN_CLD_SERVICEINTERFACE_H_

#include <tchar.h>
#include <windows.h>
#include <bar/common/refobject.h>

// Abstract interface for Compact language Detection library functionality.
class CldServiceInterface : public RefObject {
 public:
  // Clears everything, no need to call Unload() explicitly.
  virtual ~CldServiceInterface() {}

  // Loads CLD resources and modifies CLD internal data structures to use
  // loaded tables and parameters.
  // Call this function before trying to detect a language.  DetectLanguage()
  // will return " invalid_language_code" if Load() was not called at all
  // or failed for some reason.
  virtual DWORD Load(const TCHAR* component_file_name) = 0;
  // Returns true if it was loaded successfully.
  virtual bool IsLoaded() const = 0;
  // Unloads all CLD resources and reverts CLD internal data structures to
  // the default state (in which it is not capable to detect any language).
  virtual void Unload() = 0;

  // Detects a language of the UTF-16 encoded zero-terminated text.
  // [in] text - UTF-16 encoded text to detect a language of.
  // [in] is_plain_text - true if plain text, false otherwise (e.g. HTML).
  // [out] is_reliable - true, if returned language was detected reliably.
  //     See compact_lang_det.h for details.
  // [out] num_languages - set to the number of languages detected on the page.
  //     Language counts only if it's detected in more than 20% of the text.
  // [out, optional] error_code - set to 0 in case of success, Windows
  //     GetLastError() code otherwise.  Pass NULL, if not interested in errors.
  // See bar/toolbar/cld/i18n/encodings/compact_lang_det/compact_lang_det.h,
  //     CompactLangDet::DetectLanguage() description for other input parameters
  //     description.
  // Returns: ISO-639-1 code for the language.
  //     Returns " invalid_language_code" in case of any error.
  //     See googleclient/bar/toolbar/cld/i18n/languages/internal/languages.cc
  //     for details.
  virtual const char* DetectLanguage(const WCHAR* text, bool is_plain_text,
                                     bool* is_reliable, int* num_languages,
                                     DWORD* error_code) = 0;
};

#endif  // BAR_TOOLBAR_CLD_I18N_ENCODINGS_COMPACT_LANG_DET_WIN_CLD_SERVICEINTERFACE_H_
