// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/debug_utils.h"

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "views/view.h"

#ifndef NDEBUG
#include <iostream>
#endif

namespace views {

#ifndef NDEBUG

namespace {
void PrintViewHierarchyImp(const View* view, int indent) {
  std::wostringstream buf;
  int ind = indent;
  while (ind-- > 0)
    buf << L' ';
  buf << UTF8ToWide(view->GetClassName());
  buf << L' ';
  buf << view->id();
  buf << L' ';
  buf << view->x() << L"," << view->y() << L",";
  buf << view->bounds().right() << L"," << view->bounds().bottom();
  buf << L' ';
  buf << view;

  VLOG(1) << buf.str();
  std::cout << buf.str() << std::endl;

  for (int i = 0, count = view->child_count(); i < count; ++i)
    PrintViewHierarchyImp(view->GetChildViewAt(i), indent + 2);
}

void PrintFocusHierarchyImp(const View* view, int indent) {
  std::wostringstream buf;
  int ind = indent;
  while (ind-- > 0)
    buf << L' ';
  buf << UTF8ToWide(view->GetClassName());
  buf << L' ';
  buf << view->id();
  buf << L' ';
  buf << view->GetClassName().c_str();
  buf << L' ';
  buf << view;

  VLOG(1) << buf.str();
  std::cout << buf.str() << std::endl;

  if (view->child_count() > 0)
    PrintFocusHierarchyImp(view->GetChildViewAt(0), indent + 2);

  const View* next_focusable = view->GetNextFocusableView();
  if (next_focusable)
    PrintFocusHierarchyImp(next_focusable, indent);
}
}  // namespace

void PrintViewHierarchy(const View* view) {
  PrintViewHierarchyImp(view, 0);
}

void PrintFocusHierarchy(const View* view) {
  PrintFocusHierarchyImp(view, 0);
}

}  // namespace views

#endif  // NDEBUG
