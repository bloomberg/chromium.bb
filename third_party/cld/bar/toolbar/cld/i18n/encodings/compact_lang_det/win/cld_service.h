// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BAR_TOOLBAR_CLD_I18N_ENCODINGS_COMPACT_LANG_DET_WIN_CLD_SERVICE_H_
#define BAR_TOOLBAR_CLD_I18N_ENCODINGS_COMPACT_LANG_DET_WIN_CLD_SERVICE_H_

#include <atlcore.h>
#include <tchar.h>
#include <windows.h>

#include "bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_serviceinterface.h"
#include "bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_scoped_ptr.h"


class CldDynamicState;
class CldLoadPolicyInterface;


// Provides an access to Compact language Detection library functionality.
// Usage:
//     CldService instance(new CldLoadPolicy);
//     ...
//     const char* language = instance.DetectLanguage(...);
//     ...
//     instance.Unload();  or just let the instance's destructor to do the job.
//
// Calling DetectLanguage for unsuccessfully loaded instance won't crash or
// report an error, it will just return the invalid language code (see comment
// for DetectLanguage function below).
// It is thread safe.
class CldService : public CldServiceInterface {
 public:
  // Creates an empty CldService instance.  One should call Load() before
  // calling DetectLanguage().
  // [in] load_policy - component loading policy. Pass NULL if loading policy
  //     is managed externally. Takes an ownership of load_policy.
  explicit CldService(CldLoadPolicyInterface* load_policy);
  // Clears everything, no need to call Unload() explicitly.
  virtual ~CldService();

  // CldServiceInterface implemenation.

  // Usually called by load_policy.
  virtual DWORD Load(const TCHAR* component_file_name);
  virtual bool IsLoaded() const;
  virtual void Unload();

  virtual const char* DetectLanguage(const WCHAR* text, bool is_plain_text,
                                     bool* is_reliable, int* num_languages,
                                     DWORD* error_code);

 private:
  // Loads a new copy of data from the specified DLL.
  // [in] component_file_name - file name of the DLL with CLD resources.
  // [out] state - receives a new copy of data.
  // Returns: 0 in case of success, Windows GetLastError() code otherwise.
  DWORD LoadState(const TCHAR* component_file_name,
                  scoped_ptr<CldDynamicState>* state) const;

  // CLD loading policy.
  scoped_ptr<CldLoadPolicyInterface> load_policy_;
  // Loaded CLD resources and internal data structures.
  scoped_ptr<CldDynamicState> state_;
  // Critical section to protect threaded access.
  mutable ATL::CComAutoCriticalSection guard_;


  // DISALLOW_COPY_AND_ASSIGN(CldService) equivalent.
  CldService(const CldService&);
  CldService& operator=(const CldService&);
};


#endif  // BAR_TOOLBAR_CLD_I18N_ENCODINGS_COMPACT_LANG_DET_WIN_CLD_SERVICE_H_
