// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_service.h"

#include <atlbase.h>
#include <atlcore.h>
#include <tchar.h>
#include <windows.h>

#include "bar/toolbar/cld/i18n/encodings/compact_lang_det/compact_lang_det.h"
#include "bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_dynamicstate.h"
#include "bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_loadpolicyinterface.h"
#include "bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_macros.h"
#include "bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_scopedptr.h"
#include "bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_tables.h"
#include "bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_unicodetext.h"

using ATL::CComCriticalSection;
using ATL::CComCritSecLock;

CldService::CldService(CldLoadPolicyInterface* load_policy)
    : load_policy_(load_policy) {
}

CldService::~CldService() {
  Unload();
}


DWORD CldService::Load(const TCHAR* component_file_name) {
  CComCritSecLock<CComCriticalSection> lock(guard_);
  return LoadState(component_file_name, &state_);
}


DWORD CldService::LoadState(const TCHAR* component_file_name,
                            scoped_ptr<CldDynamicState>* state) const {
  _ASSERT(state);
  if (!state)
    return ERROR_INVALID_PARAMETER;

  // Load resource DLL.
  SAFE_HMODULE resource_library(
      ::LoadLibraryEx(component_file_name, NULL, LOAD_LIBRARY_AS_DATAFILE));
  if (!resource_library.get())
    return ::GetLastError();

  // Load resources.
  scoped_ptr<CldTables> tables(new CldTables);
  if (!tables.get())
    return ERROR_OUTOFMEMORY;

  RETURN_IF_ERROR(
      tables->Load(resource_library.release()));

  // Set up CLD library.
  scoped_ptr<CldDynamicState> new_state(new CldDynamicState);
  if (!new_state.get())
    return ERROR_OUTOFMEMORY;

  // Pass tables's ownership to new_state.
  RETURN_IF_ERROR(
      new_state->Load(tables.release()));

  // Put new state in place.  Note that CldDynamicState manipulates global
  // memory and does not fully encapsulate the state.
  swap(*state, new_state);

  return ERROR_SUCCESS;
}


bool CldService::IsLoaded() const {
  CComCritSecLock<CComCriticalSection> lock(guard_);
  return NULL != state_.get();
}


void CldService::Unload() {
  CComCritSecLock<CComCriticalSection> lock(guard_);
  state_.reset();
}


const char* CldService::DetectLanguage(const WCHAR* text, bool is_plain_text,
                                       bool* is_reliable, int* num_languages,
                                       DWORD* error_code) {
  CComCritSecLock<CComCriticalSection> lock(guard_);
  Language language = NUM_LANGUAGES;

  bool ready = IsLoaded();
  if (!ready) {
    // Try to load CLD tables; do not complain to user if something goes wrong.
    // Worst that could happen is that automatic translation won't kick in,
    // but user can still explicitly request a translation.
    ready = NULL != load_policy_.get() &&
            ERROR_SUCCESS == load_policy_->Ready(this);
  }

  if (ready) {
    language = DetectLanguageOfUnicodeText(state_->detection_tables(),
                                           text, is_plain_text, is_reliable,
                                           num_languages, error_code);
  }

  // Use LanguageCodeWithDialects() instead of LanguageCode() to properly handle
  // Chinese return variations, i.e. return "zh-CN" or "zh-TW", not just "zh".
  return LanguageCodeWithDialects(language);
}
