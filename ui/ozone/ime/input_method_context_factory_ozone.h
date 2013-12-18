// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_IME_INPUT_METHOD_CONTEXT_FACTORY_OZONE_H_
#define UI_OZONE_IME_INPUT_METHOD_CONTEXT_FACTORY_OZONE_H_

#include "ui/base/ime/linux/linux_input_method_context_factory.h"
#include "ui/ozone/ozone_export.h"

namespace ui {
// TODO(kalyan): Move this class to ui/ime, once ui/base/ime is moved to ui/ime.
// An interface that lets different Ozone platforms override the
// CreateInputMethodContext function declared here to return native input method
// contexts.
class OZONE_EXPORT InputMethodContextFactoryOzone :
    public LinuxInputMethodContextFactory  {
 public:
  InputMethodContextFactoryOzone();
  virtual ~InputMethodContextFactoryOzone();

  // By default this returns a minimal input method context.
  virtual scoped_ptr<LinuxInputMethodContext> CreateInputMethodContext(
      LinuxInputMethodContextDelegate* delegate) const OVERRIDE;
};

}  // namespace ui

#endif  // UI_OZONE_IME_INPUT_METHOD_CONTEXT_FACTORY_OZONE_H_
