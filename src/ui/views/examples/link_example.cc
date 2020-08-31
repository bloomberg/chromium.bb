// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/link_example.h"

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/link.h"
#include "ui/views/examples/examples_window.h"
#include "ui/views/examples/grit/views_examples_resources.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"

using l10n_util::GetStringUTF16;
using l10n_util::GetStringUTF8;

namespace views {
namespace examples {

LinkExample::LinkExample()
    : ExampleBase(GetStringUTF8(IDS_LINK_SELECT_LABEL).c_str()) {}

LinkExample::~LinkExample() = default;

void LinkExample::CreateExampleView(View* container) {
  auto link =
      std::make_unique<Link>(GetStringUTF16(IDS_LINK_CLICK_PROMPT_LABEL));
  link->set_callback(base::BindRepeating(
      &LogStatus, GetStringUTF8(IDS_LINK_CLICK_CONFIRMED_LABEL)));

  container->SetLayoutManager(std::make_unique<FillLayout>());
  container->AddChildView(std::move(link));
}

}  // namespace examples
}  // namespace views
