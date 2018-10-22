// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_QUIT_INSTRUCTION_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_QUIT_INSTRUCTION_BUBBLE_CONTROLLER_H_

#include <memory>

#include "base/macros.h"
#include "base/timer/timer.h"
#include "ui/events/event_handler.h"

class QuitInstructionBubbleBase;

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

// Manages showing and hiding the quit instruction bubble.  The singleton
// instance of this class is added as a PreTargetHandler for each browser
// window.
class QuitInstructionBubbleController : public ui::EventHandler {
 public:
  static QuitInstructionBubbleController* GetInstance();

  ~QuitInstructionBubbleController() override;

  // ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;

 protected:
  QuitInstructionBubbleController(
      std::unique_ptr<QuitInstructionBubbleBase> bubble,
      std::unique_ptr<base::OneShotTimer> hide_timer);

 private:
  friend struct base::DefaultSingletonTraits<QuitInstructionBubbleController>;

  QuitInstructionBubbleController();

  void OnTimerElapsed();

  std::unique_ptr<QuitInstructionBubbleBase> const view_;

  std::unique_ptr<base::OneShotTimer> hide_timer_;

  DISALLOW_COPY_AND_ASSIGN(QuitInstructionBubbleController);
};

#endif  // CHROME_BROWSER_UI_VIEWS_QUIT_INSTRUCTION_BUBBLE_CONTROLLER_H_
