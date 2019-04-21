// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/observer_list.h"
#include "ui/chromeos/ime/input_method_menu_item.h"
#include "ui/chromeos/ui_chromeos_export.h"

#ifndef UI_CHROMEOS_IME_INPUT_METHOD_MENU_MANAGER_H_
#define UI_CHROMEOS_IME_INPUT_METHOD_MENU_MANAGER_H_

namespace base {
template <typename Type>
struct DefaultSingletonTraits;
}  // namespace base

namespace ui {
namespace ime {

class UI_CHROMEOS_EXPORT InputMethodMenuManager {
public:
  class Observer {
   public:
    virtual ~Observer() {}

    // Called when the list of menu items is changed.
    virtual void InputMethodMenuItemChanged(
        InputMethodMenuManager* manager) = 0;
  };

  ~InputMethodMenuManager();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Obtains the singleton instance.
  static InputMethodMenuManager* GetInstance();

  // Sets the list of input method menu items. The list could be empty().
  void SetCurrentInputMethodMenuItemList(
      const InputMethodMenuItemList& menu_list);

  // Gets the list of input method menu items. The list could be empty().
  InputMethodMenuItemList GetCurrentInputMethodMenuItemList() const;

  // True if the key exists in the menu_list_.
  bool HasInputMethodMenuItemForKey(const std::string& key) const;

 private:
  InputMethodMenuManager();

  // For Singleton to be able to construct an instance.
  friend struct base::DefaultSingletonTraits<InputMethodMenuManager>;

  // Menu item list of the input method.  This is set by extension IMEs.
  InputMethodMenuItemList menu_list_;

  // Observers who will be notified when menu changes.
  base::ObserverList<Observer>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodMenuManager);
};

}  // namespace ime
}  // namespace ui

#endif // UI_CHROMEOS_IME_INPUT_METHOD_MENU_MANAGER_H_
