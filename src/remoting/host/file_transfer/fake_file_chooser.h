// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_FILE_TRANSFER_FAKE_FILE_CHOOSER_H_
#define REMOTING_HOST_FILE_TRANSFER_FAKE_FILE_CHOOSER_H_

#include "base/macros.h"
#include "remoting/host/file_transfer/file_chooser.h"

namespace remoting {

class FakeFileChooser : public FileChooser {
 public:
  explicit FakeFileChooser(ResultCallback callback);

  ~FakeFileChooser() override;

  // FileChooser implementation.
  void Show() override;

  // The result that usages of FakeFileChooser should return.
  static void SetResult(FileChooser::Result result);

 private:
  ResultCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(FakeFileChooser);
};

}  // namespace remoting

#endif  // REMOTING_HOST_FILE_TRANSFER_FAKE_FILE_CHOOSER_H_
