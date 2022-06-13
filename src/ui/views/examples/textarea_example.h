// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_TEXTAREA_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_TEXTAREA_EXAMPLE_H_

#include "ui/views/examples/example_base.h"

namespace views {
namespace examples {

class VIEWS_EXAMPLES_EXPORT TextareaExample : public ExampleBase {
 public:
  TextareaExample();
  ~TextareaExample() override = default;

  // ExampleBase:
  void CreateExampleView(View* container) override;
};

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_TEXTAREA_EXAMPLE_H_
