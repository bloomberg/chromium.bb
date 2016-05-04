// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_INK_DROP_FACTORY_H_
#define UI_VIEWS_ANIMATION_INK_DROP_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "ui/views/views_export.h"

namespace views {
class InkDrop;
class InkDropHost;

// A factory to create an InkDrop. A different InkDrop type will be created
// based on whether or not material design is enabled.
class VIEWS_EXPORT InkDropFactory {
 public:
  // Creates a new InkDrop that will add/remove an InkDropRipple's ui::Layer
  // to/from the |ink_drop_host| when the animation is active/inactive.
  static std::unique_ptr<InkDrop> CreateInkDrop(InkDropHost* ink_drop_host);

 private:
  InkDropFactory();
  ~InkDropFactory();

  DISALLOW_COPY_AND_ASSIGN(InkDropFactory);
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_INK_DROP_FACTORY_H_
