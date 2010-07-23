// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_MOCK_OBJECTS_H_
#define REMOTING_CLIENT_MOCK_OBJECTS_H_

#include "base/ref_counted.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace remoting {

class MockDecodeDoneHandler :
      public base::RefCountedThreadSafe<MockDecodeDoneHandler> {
 public:
  MockDecodeDoneHandler() {}

  MOCK_METHOD0(PartialDecodeDone, void());
  MOCK_METHOD0(DecodeDone, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDecodeDoneHandler);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_MOCK_OBJECTS_H_
