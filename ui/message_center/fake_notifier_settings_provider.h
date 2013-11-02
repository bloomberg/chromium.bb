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
  FakeNotifierSettingsProvider();
  explicit FakeNotifierSettingsProvider(
      const std::vector<Notifier*>& notifiers);
  virtual ~FakeNotifierSettingsProvider();

  virtual size_t GetNotifierGroupCount() const OVERRIDE;
  virtual const message_center::NotifierGroup& GetNotifierGroupAt(
      size_t index) const OVERRIDE;
  virtual bool IsNotifierGroupActiveAt(size_t index) const OVERRIDE;
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

  void AddGroup(NotifierGroup* group, const std::vector<Notifier*>& notifiers);

 private:
  struct NotifierGroupItem {
    NotifierGroup* group;
    std::vector<Notifier*> notifiers;

    NotifierGroupItem();
    ~NotifierGroupItem();
  };

  std::map<const Notifier*, bool> enabled_;
  std::vector<NotifierGroupItem> items_;
  int closed_called_count_;
  size_t active_item_index_;
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_FAKE_NOTIFIER_SETTINGS_PROVIDER_H_
