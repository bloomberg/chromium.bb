// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/focus/focus_manager_factory.h"

#include "ui/views/focus/focus_manager.h"
#include "ui/views/focus/focus_manager_delegate.h"

namespace views {

namespace {

class DefaultFocusManagerFactory : public FocusManagerFactory {
 public:
  DefaultFocusManagerFactory() : FocusManagerFactory() {}
  ~DefaultFocusManagerFactory() override {}

 protected:
  FocusManager* CreateFocusManager(Widget* widget,
                                   bool desktop_widget) override {
    return new FocusManager(widget, nullptr /* delegate */);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DefaultFocusManagerFactory);
};

FocusManagerFactory* focus_manager_factory = nullptr;

}  // namespace

FocusManagerFactory::FocusManagerFactory() {
}

FocusManagerFactory::~FocusManagerFactory() {
}

// static
FocusManager* FocusManagerFactory::Create(Widget* widget,
                                          bool desktop_widget) {
  if (!focus_manager_factory)
    focus_manager_factory = new DefaultFocusManagerFactory();
  return focus_manager_factory->CreateFocusManager(widget, desktop_widget);
}

// static
void FocusManagerFactory::Install(FocusManagerFactory* f) {
  if (f == focus_manager_factory)
    return;
  delete focus_manager_factory;
  focus_manager_factory = f ? f : new DefaultFocusManagerFactory();
}

}  // namespace views
