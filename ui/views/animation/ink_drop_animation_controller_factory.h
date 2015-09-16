// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_INK_DROP_ANIMATION_CONTROLLER_FACTORY_H_
#define UI_VIEWS_ANIMATION_INK_DROP_ANIMATION_CONTROLLER_FACTORY_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "ui/views/views_export.h"

namespace views {
class InkDropAnimationController;
class InkDropHost;

// A factory to create InkDropAnimationController. A different
// InkDropAnimationController type will be created based on whether or not
// material design is enabled.
class VIEWS_EXPORT InkDropAnimationControllerFactory {
 public:
  // Creates a new InkDropAnimationController that will add/remove an
  // InkDropAnimation's ui::Layer to/from the |ink_drop_host| when the animation
  // is active/inactive.
  static scoped_ptr<InkDropAnimationController>
  CreateInkDropAnimationController(InkDropHost* ink_drop_host);

 private:
  InkDropAnimationControllerFactory();
  ~InkDropAnimationControllerFactory();

  DISALLOW_COPY_AND_ASSIGN(InkDropAnimationControllerFactory);
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_INK_DROP_ANIMATION_CONTROLLER_FACTORY_H_
