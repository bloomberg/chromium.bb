// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printing_context.h"

#include "base/logging.h"

namespace printing {

#if !defined(OS_CHROMEOS)
// static
PrintingContext* PrintingContext::Create(const std::string& app_locale) {
  // TODO(saintlou): This a stub to allow us to build under Aura.
  // See issue: http://crbug.com/99282
  NOTIMPLEMENTED();
  return NULL;
}
#endif

}  // namespace printing
