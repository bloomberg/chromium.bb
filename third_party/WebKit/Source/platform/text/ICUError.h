// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ICUError_h
#define ICUError_h

#include "platform/PlatformExport.h"
#include "wtf/Allocator.h"
#include "wtf/Assertions.h"
#include <unicode/utypes.h>

namespace blink {

// ICUError provides the unified way to handle ICU errors in Blink.
class PLATFORM_EXPORT ICUError {
  STACK_ALLOCATED();

 public:
  ~ICUError() { crashIfCritical(); }

  UErrorCode* operator&() { return &m_error; }
  operator UErrorCode() const { return m_error; }

  // Crash the renderer in the appropriate way if critical failure occurred.
  void crashIfCritical();

 private:
  UErrorCode m_error = U_ZERO_ERROR;

  void handleFailure();
};

inline void ICUError::crashIfCritical() {
  if (U_FAILURE(m_error))
    handleFailure();
}

}  // namespace blink

#endif  // ICUError_h
