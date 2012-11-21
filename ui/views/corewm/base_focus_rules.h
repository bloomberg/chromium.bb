// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COREWM_BASE_FOCUS_RULES_H_
#define UI_VIEWS_COREWM_BASE_FOCUS_RULES_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/views/corewm/focus_rules.h"

namespace views {
namespace corewm {

// A set of basic focus and activation rules. Specializations should most likely
// subclass this and call up to these methods rather than reimplementing them.
class VIEWS_EXPORT BaseFocusRules : public FocusRules {
 public:
  BaseFocusRules();
  virtual ~BaseFocusRules();

 protected:
  // Overridden from FocusRules:
  virtual bool CanActivateWindow(aura::Window* window) OVERRIDE;
  virtual bool CanFocusWindow(aura::Window* window) OVERRIDE;
  virtual aura::Window* GetActivatableWindow(aura::Window* window) OVERRIDE;
  virtual aura::Window* GetFocusableWindow(aura::Window* window) OVERRIDE;
  virtual aura::Window* GetNextActivatableWindow(aura::Window* ignore) OVERRIDE;
  virtual aura::Window* GetNextFocusableWindow(aura::Window* ignore) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(BaseFocusRules);
};

}  // namespace corewm
}  // namespace views

#endif  // UI_VIEWS_COREWM_BASE_FOCUS_RULES_H_
