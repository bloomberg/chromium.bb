// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_PIN_REQUEST_WIDGET_H_
#define ASH_LOGIN_UI_PIN_REQUEST_WIDGET_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/login/ui/pin_request_view.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"

namespace views {
class Widget;
}

namespace ash {

class WindowDimmer;

enum class PinRequestReason;
enum class PinRequestViewState;

// Widget to display the Pin Request View in a standalone container.
// This widget is modal and only one instance can be created at a time. It will
// be destroyed when dismissed.
class ASH_EXPORT PinRequestWidget {
 public:
  // TestApi is used for tests to get internal implementation details.
  class ASH_EXPORT TestApi {
   public:
    explicit TestApi(PinRequestWidget* widget);
    ~TestApi();

    PinRequestView* pin_request_view();

    // Simulates that pin validation finished with the result specified in
    // |access_granted|, which dismisses the widget.
    void SimulateValidationFinished(bool access_granted);

   private:
    PinRequestWidget* const pin_request_widget_;
  };

  // Creates and shows the instance of PinRequestWidget.
  // This widget is modal and only one instance can be created at a time. It
  // will be destroyed when dismissed.
  static void Show(PinRequest request, PinRequestView::Delegate* delegate);

  // Returns the instance of PinRequestWidget or nullptr if it does not exits.
  static PinRequestWidget* Get();

  // Toggles showing an error state and updates displayed strings.
  void UpdateState(PinRequestViewState state,
                   const base::string16& title,
                   const base::string16& description);

  // Enables or disables PIN input.
  void SetPinInputEnabled(bool enabled);

  // Clears previously entered PIN from the PIN input field(s).
  void ClearInput();

  // Closes the widget.
  // |success| describes whether the validation was successful and is passed to
  // |on_pin_request_done_|.
  void Close(bool success);

 private:
  PinRequestWidget(PinRequest request, PinRequestView::Delegate* delegate);
  ~PinRequestWidget();

  // Shows the |widget_| and |dimmer_| if applicable.
  void Show();

  // Returns the associated view.
  PinRequestView* GetView();

  // Callback invoked when closing the widget.
  PinRequest::OnPinRequestDone on_pin_request_done_;

  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<WindowDimmer> dimmer_;

  base::WeakPtrFactory<PinRequestWidget> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PinRequestWidget);
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_PIN_REQUEST_WIDGET_H_
