// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/scoped_views_test_helper.h"

#include "base/message_loop/message_loop.h"
#include "ui/base/ime/input_method_initializer.h"
#include "ui/compositor/test/context_factories_for_test.h"
#include "ui/views/test/test_views_delegate.h"
#include "ui/views/test/views_test_helper.h"

namespace views {

ScopedViewsTestHelper::ScopedViewsTestHelper()
    : ScopedViewsTestHelper(make_scoped_ptr(new TestViewsDelegate)) {
}

ScopedViewsTestHelper::ScopedViewsTestHelper(
    scoped_ptr<TestViewsDelegate> views_delegate)
    : views_delegate_(views_delegate.Pass()) {
  // The ContextFactory must exist before any Compositors are created.
  bool enable_pixel_output = false;
  ui::ContextFactory* context_factory =
      ui::InitializeContextFactoryForTests(enable_pixel_output);
  views_delegate_->set_context_factory(context_factory);

  test_helper_.reset(ViewsTestHelper::Create(base::MessageLoopForUI::current(),
                                             context_factory));
  test_helper_->SetUp();

  ui::InitializeInputMethodForTesting();
}

ScopedViewsTestHelper::~ScopedViewsTestHelper() {
  ui::ShutdownInputMethodForTesting();
  test_helper_->TearDown();
  test_helper_.reset();

  ui::TerminateContextFactoryForTests();
  views_delegate_.reset();
}

gfx::NativeWindow ScopedViewsTestHelper::GetContext() {
  return test_helper_->GetContext();
}

}  // namespace views
