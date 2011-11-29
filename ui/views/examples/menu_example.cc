// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/menu_example.h"

#include <set>

#include "base/utf_string_conversions.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/controls/menu/menu_2.h"
#include "ui/views/controls/menu/view_menu_delegate.h"
#include "ui/views/layout/fill_layout.h"
#include "views/view.h"

namespace views {
namespace examples {

namespace {

class ExampleMenuModel : public ui::SimpleMenuModel,
                         public ui::SimpleMenuModel::Delegate {
 public:
  ExampleMenuModel();

  void RunMenuAt(const gfx::Point& point);

  // Overridden from ui::SimpleMenuModel::Delegate:
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE;
  virtual void ExecuteCommand(int command_id) OVERRIDE;

 private:
  enum {
    kGroupMakeDecision,
  };

  enum {
    kCommandDoSomething,
    kCommandSelectAscii,
    kCommandSelectUtf8,
    kCommandSelectUtf16,
    kCommandCheckApple,
    kCommandCheckOrange,
    kCommandCheckKiwi,
    kCommandGoHome,
  };

  scoped_ptr<Menu2> menu_;
  scoped_ptr<ui::SimpleMenuModel> submenu_;
  std::set<int> checked_fruits_;
  int current_encoding_command_id_;

  DISALLOW_COPY_AND_ASSIGN(ExampleMenuModel);
};

class ExampleMenuButton : public MenuButton, public ViewMenuDelegate {
 public:
  ExampleMenuButton(const string16& test, bool show_menu_marker);
  virtual ~ExampleMenuButton();

 private:
  // Overridden from ViewMenuDelegate:
  virtual void RunMenu(View* source, const gfx::Point& point) OVERRIDE;

  scoped_ptr<ExampleMenuModel> menu_model_;
  DISALLOW_COPY_AND_ASSIGN(ExampleMenuButton);
};

// ExampleMenuModel ---------------------------------------------------------

ExampleMenuModel::ExampleMenuModel()
    : ALLOW_THIS_IN_INITIALIZER_LIST(ui::SimpleMenuModel(this)),
      current_encoding_command_id_(kCommandSelectAscii) {
  AddItem(kCommandDoSomething, WideToUTF16(L"Do Something"));
  AddSeparator();
  AddRadioItem(kCommandSelectAscii,
               WideToUTF16(L"ASCII"), kGroupMakeDecision);
  AddRadioItem(kCommandSelectUtf8,
               WideToUTF16(L"UTF-8"), kGroupMakeDecision);
  AddRadioItem(kCommandSelectUtf16,
               WideToUTF16(L"UTF-16"), kGroupMakeDecision);
  AddSeparator();
  AddCheckItem(kCommandCheckApple, WideToUTF16(L"Apple"));
  AddCheckItem(kCommandCheckOrange, WideToUTF16(L"Orange"));
  AddCheckItem(kCommandCheckKiwi, WideToUTF16(L"Kiwi"));
  AddSeparator();
  AddItem(kCommandGoHome, WideToUTF16(L"Go Home"));

  submenu_.reset(new ui::SimpleMenuModel(this));
  submenu_->AddItem(kCommandDoSomething, WideToUTF16(L"Do Something 2"));
  AddSubMenu(0, ASCIIToUTF16("Submenu"), submenu_.get());
  menu_.reset(new Menu2(this));
}

void ExampleMenuModel::RunMenuAt(const gfx::Point& point) {
  menu_->RunMenuAt(point, Menu2::ALIGN_TOPRIGHT);
}

bool ExampleMenuModel::IsCommandIdChecked(int command_id) const {
  // Radio items.
  if (command_id == current_encoding_command_id_)
    return true;

  // Check items.
  if (checked_fruits_.find(command_id) != checked_fruits_.end())
    return true;

  return false;
}

bool ExampleMenuModel::IsCommandIdEnabled(int command_id) const {
  // All commands are enabled except for kCommandGoHome.
  return command_id != kCommandGoHome;
}

bool ExampleMenuModel::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) {
  // We don't use this in the example.
  return false;
}

void ExampleMenuModel::ExecuteCommand(int command_id) {
  switch (command_id) {
    case kCommandDoSomething: {
      LOG(INFO) << "Done something";
      break;
    }

    // Radio items.
    case kCommandSelectAscii: {
      current_encoding_command_id_ = kCommandSelectAscii;
      LOG(INFO) << "Selected ASCII";
      break;
    }
    case kCommandSelectUtf8: {
      current_encoding_command_id_ = kCommandSelectUtf8;
      LOG(INFO) << "Selected UTF-8";
      break;
    }
    case kCommandSelectUtf16: {
      current_encoding_command_id_ = kCommandSelectUtf16;
      LOG(INFO) << "Selected UTF-16";
      break;
    }

    // Check items.
    case kCommandCheckApple:
    case kCommandCheckOrange:
    case kCommandCheckKiwi: {
      // Print what fruit is checked.
      const char* checked_fruit = "";
      if (command_id == kCommandCheckApple) {
        checked_fruit = "Apple";
      } else if (command_id == kCommandCheckOrange) {
        checked_fruit = "Orange";
      } else if (command_id == kCommandCheckKiwi) {
        checked_fruit = "Kiwi";
      }
      LOG(INFO) << "Checked " << checked_fruit;

      // Update the check status.
      std::set<int>::iterator iter = checked_fruits_.find(command_id);
      if (iter == checked_fruits_.end())
        checked_fruits_.insert(command_id);
      else
        checked_fruits_.erase(iter);
      break;
    }
  }
}

// ExampleMenuButton -----------------------------------------------------------

ExampleMenuButton::ExampleMenuButton(const string16& test,
                                     bool show_menu_marker)
    : ALLOW_THIS_IN_INITIALIZER_LIST(
          MenuButton(NULL, test, this, show_menu_marker)) {
}

ExampleMenuButton::~ExampleMenuButton() {
}

void ExampleMenuButton::RunMenu(View* source, const gfx::Point& point) {
  if (!menu_model_.get())
    menu_model_.reset(new ExampleMenuModel);

  menu_model_->RunMenuAt(point);
}

}  // namespace

MenuExample::MenuExample() : ExampleBase("Menu") {
}

MenuExample::~MenuExample() {
}

void MenuExample::CreateExampleView(View* container) {
  // Menu2 is not a sub class of View, hence we cannot directly
  // add to the container. Instead, we add a button to open a menu.
  const bool show_menu_marker = true;
  ExampleMenuButton* menu_button = new ExampleMenuButton(
      ASCIIToUTF16("Open a menu"), show_menu_marker);
  container->SetLayoutManager(new FillLayout);
  container->AddChildView(menu_button);
}

}  // namespace examples
}  // namespace views
