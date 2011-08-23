// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/focus/focus_manager_factory.h"

#include "base/compiler_specific.h"
#include "views/focus/focus_manager.h"

namespace {

using views::FocusManager;

class DefaultFocusManagerFactory : public views::FocusManagerFactory {
 public:
  DefaultFocusManagerFactory() : views::FocusManagerFactory() {}
  virtual ~DefaultFocusManagerFactory() {}

 protected:
  virtual FocusManager* CreateFocusManager(views::Widget* widget) OVERRIDE {
    return new FocusManager(widget);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DefaultFocusManagerFactory);
};

views::FocusManagerFactory* focus_manager_factory = NULL;

}  // namespace

namespace views {

FocusManagerFactory::FocusManagerFactory() {
}

FocusManagerFactory::~FocusManagerFactory() {
}

// static
FocusManager* FocusManagerFactory::Create(Widget* widget) {
  if (!focus_manager_factory)
    focus_manager_factory = new DefaultFocusManagerFactory();
  return focus_manager_factory->CreateFocusManager(widget);
}

// static
void FocusManagerFactory::Install(FocusManagerFactory* f) {
  if (f == focus_manager_factory)
    return;
  delete focus_manager_factory;
  focus_manager_factory = f ? f : new DefaultFocusManagerFactory();
}

}  // namespace views
