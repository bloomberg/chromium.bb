// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_FAKE_NOTIFIER_SETTINGS_PROVIDER_H_
#define UI_MESSAGE_CENTER_FAKE_NOTIFIER_SETTINGS_PROVIDER_H_

#include "ui/message_center/notifier_settings.h"

namespace message_center {

// A NotifierSettingsProvider that returns a configurable, fixed set of
// notifiers and records which callbacks were called. For use in tests.
class FakeNotifierSettingsProvider : public NotifierSettingsProvider {
 public:
  FakeNotifierSettingsProvider(const std::vector<Notifier*>& notifiers);
  virtual ~FakeNotifierSettingsProvider();

  virtual size_t GetNotifierGroupCount() const OVERRIDE;
  virtual const message_center::NotifierGroup& GetNotifierGroupAt(
      size_t index) const OVERRIDE;
  virtual void SwitchToNotifierGroup(size_t index) OVERRIDE;
  virtual const message_center::NotifierGroup& GetActiveNotifierGroup() const
      OVERRIDE;

  virtual void GetNotifierList(std::vector<Notifier*>* notifiers) OVERRIDE;

  virtual void SetNotifierEnabled(const Notifier& notifier,
                                  bool enabled) OVERRIDE;

  virtual void OnNotifierSettingsClosing() OVERRIDE;
  virtual void AddObserver(NotifierSettingsObserver* observer) OVERRIDE;
  virtual void RemoveObserver(NotifierSettingsObserver* observer) OVERRIDE;

  bool WasEnabled(const Notifier& notifier);
  int closed_called_count();

 private:
  std::vector<Notifier*> notifiers_;
  std::map<const Notifier*, bool> enabled_;
  const NotifierGroup notifier_group_;
  int closed_called_count_;
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_FAKE_NOTIFIER_SETTINGS_PROVIDER_H_
