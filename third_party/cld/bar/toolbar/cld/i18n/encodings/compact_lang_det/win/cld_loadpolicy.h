// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BAR_TOOLBAR_CLD_I18N_ENCODINGS_COMPACT_LANG_DET_WIN_CLD_LOADPOLICY_H_
#define BAR_TOOLBAR_CLD_I18N_ENCODINGS_COMPACT_LANG_DET_WIN_CLD_LOADPOLICY_H_

#include "bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_loadpolicyinterface.h"


// Default implementation of CldService loading policy. Instance
// of this class is passed as a parameter to CldService constructor
// (CldService instance assumes its ownership). It's not intended to be used
// in any other way.
// It is not thread safe. Calling Ready() simultaneously from different threads
// will yield unpredictable results.
class CldLoadPolicy : public CldLoadPolicyInterface {
 public:
  // Creates an empty CldLoadPolicy instance.
  CldLoadPolicy();
  virtual ~CldLoadPolicy();

  // CldLoadPolicyInterface implementation.
  virtual DWORD Ready(CldServiceInterface* cld_service);

 private:
  // DISALLOW_COPY_AND_ASSIGN(CldLoadPolicy) equivalent.
  CldLoadPolicy(const CldLoadPolicy&);
  CldLoadPolicy& operator=(const CldLoadPolicy&);
};


#endif  // BAR_TOOLBAR_CLD_I18N_ENCODINGS_COMPACT_LANG_DET_WIN_CLD_LOADPOLICY_H_
