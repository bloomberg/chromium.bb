// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/test_ash_web_view.h"

#include "base/bind.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "ui/views/view.h"

namespace ash {

TestAshWebView::TestAshWebView(const AshWebView::InitParams& init_params)
    : init_params_(init_params) {}

TestAshWebView::~TestAshWebView() = default;

void TestAshWebView::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void TestAshWebView::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

gfx::NativeView TestAshWebView::GetNativeView() {
  // Not yet implemented for unittests.
  return nullptr;
}

bool TestAshWebView::GoBack() {
  // Not yet implemented for unittests.
  return false;
}

void TestAshWebView::Navigate(const GURL& url) {
  // Simulate navigation by notifying |observers_| of the expected event that
  // would normally signal navigation completion. We do this asynchronously to
  // more accurately simulate real-world conditions.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](const base::WeakPtr<TestAshWebView>& self) {
                       if (self) {
                         for (auto& observer : self->observers_)
                           observer.DidStopLoading();
                       }
                     },
                     weak_factory_.GetWeakPtr()));
}

views::View* TestAshWebView::GetInitiallyFocusedView() {
  return this;
}

void TestAshWebView::RequestFocus() {
  focused_ = true;
}

bool TestAshWebView::HasFocus() const {
  return focused_;
}

}  // namespace ash
