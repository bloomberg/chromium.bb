// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_BASE_MOCK_OBJECTS_H_
#define REMOTING_BASE_BASE_MOCK_OBJECTS_H_

#include "remoting/base/capture_data.h"
#include "remoting/base/decoder.h"
#include "remoting/base/encoder.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace remoting {

class MockEncoder : public Encoder {
 public:
  MockEncoder();
  virtual ~MockEncoder();

  MOCK_METHOD3(Encode, void(
      scoped_refptr<CaptureData> capture_data,
      bool key_frame,
      DataAvailableCallback* data_available_callback));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockEncoder);
};

}  // namespace remoting

#endif  // REMOTING_BASE_BASE_MOCK_OBJECTS_H_
