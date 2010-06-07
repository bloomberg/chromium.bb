// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_JINGLE_GLUE_MOCK_OBJECTS_H_
#define REMOTING_JINGLE_GLUE_MOCK_OBJECTS_H_

#include "remoting/jingle_glue/jingle_channel.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace remoting {

class MockJingleChannel : public JingleChannel {
 public:
  MockJingleChannel() {}

  MOCK_METHOD1(Write, void(scoped_refptr<media::DataBuffer> data));
  MOCK_METHOD0(Close, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockJingleChannel);
};

}  // namespace remoting

#endif  // REMOTING_JINGLE_GLUE_MOCK_OBJECTS_H_
