// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CHROMEOS_MESSAGE_BOX_H_
#define REMOTING_HOST_CHROMEOS_MESSAGE_BOX_H_

#include "base/callback_helpers.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/threading/thread_checker.h"

namespace remoting {

// Overview:
// Shows a system modal message box with OK and cancel buttons. This class
// is not thread-safe, it must be called on the UI thread of the browser
// process. Destroy the instance to hide the message box.
class MessageBox {
 public:
  enum Result {
    OK,
    CANCEL
  };

  // ResultCallback will be invoked with Result::Cancel if the user closes the
  // MessageBox without clicking on any buttons.
  typedef base::Callback<void(Result)> ResultCallback;

  MessageBox(const base::string16& title_label,
             const base::string16& message_label,
             const base::string16& ok_label,
             const base::string16& cancel_label,
             ResultCallback result_callback);
  ~MessageBox();

 private:
  class Core;
  Core* core_;
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(MessageBox);
};

}  // namespace remoting

#endif  // REMOTING_HOST_CHROMEOS_MESSAGE_BOX_H_
