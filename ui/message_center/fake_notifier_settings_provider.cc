// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/fake_notifier_settings_provider.h"

namespace message_center {

FakeNotifierSettingsProvider::FakeNotifierSettingsProvider(
    const std::vector<Notifier*>& notifiers)
    : notifiers_(notifiers), closed_called_count_(0) {}

FakeNotifierSettingsProvider::~FakeNotifierSettingsProvider() {}

void FakeNotifierSettingsProvider::GetNotifierList(
    std::vector<Notifier*>* notifiers) {
  notifiers->clear();
  for (size_t i = 0; i < notifiers_.size(); ++i)
    notifiers->push_back(notifiers_[i]);
}

void FakeNotifierSettingsProvider::SetNotifierEnabled(const Notifier& notifier,
                                                      bool enabled) {
  enabled_[&notifier] = enabled;
}

void FakeNotifierSettingsProvider::OnNotifierSettingsClosing() {
  closed_called_count_++;
}

void FakeNotifierSettingsProvider::AddObserver(
    NotifierSettingsObserver* observer) {
}

void FakeNotifierSettingsProvider::RemoveObserver(
    NotifierSettingsObserver* observer) {
}

bool FakeNotifierSettingsProvider::WasEnabled(const Notifier& notifier) {
  return enabled_[&notifier];
}

int FakeNotifierSettingsProvider::closed_called_count() {
  return closed_called_count_;
}

}  // namespace message_center
