// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_PARENT_ACCESS_WIDGET_H_
#define ASH_LOGIN_UI_PARENT_ACCESS_WIDGET_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/optional.h"

class AccountId;

namespace views {
class Widget;
}

namespace ash {

// Widget to display the Parent Access View in a standalone container.
class ParentAccessWidget {
 public:
  using OnExitCallback = base::RepeatingCallback<void(bool success)>;

  // Creates and shows the widget. When |account_id| is set, the parent
  // access code is validated using the configuration for the provided account,
  // when it is empty it tries to validate the access code to any child signed
  // in the device. The |callback| is called when (a) the validation is
  // successful or (b) the back button is pressed.
  ParentAccessWidget(const base::Optional<AccountId>& account_id,
                     const OnExitCallback& callback);

  ~ParentAccessWidget();

 private:
  std::unique_ptr<views::Widget> widget_;

  DISALLOW_COPY_AND_ASSIGN(ParentAccessWidget);
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_PARENT_ACCESS_WIDGET_H_
