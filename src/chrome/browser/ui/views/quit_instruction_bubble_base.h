// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_QUIT_INSTRUCTION_BUBBLE_BASE_H_
#define CHROME_BROWSER_UI_VIEWS_QUIT_INSTRUCTION_BUBBLE_BASE_H_

// Base class of QuitInstructionBubble necessary for unit testing
// QuitInstructionBubbleController.
class QuitInstructionBubbleBase {
 public:
  QuitInstructionBubbleBase() {}
  virtual ~QuitInstructionBubbleBase() {}

  virtual void Show() = 0;
  virtual void Hide() = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_QUIT_INSTRUCTION_BUBBLE_BASE_H_
