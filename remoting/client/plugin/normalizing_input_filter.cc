// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/normalizing_input_filter.h"

#include "remoting/protocol/input_filter.h"

namespace remoting {

using protocol::InputFilter;
using protocol::InputStub;

#if !defined(OS_MACOSX) && !defined(OS_CHROMEOS)
scoped_ptr<InputFilter> CreateNormalizingInputFilter(InputStub* input_stub) {
  return scoped_ptr<InputFilter>(new InputFilter(input_stub));
}
#endif // !defined(OS_MACOSX) && !defined(OS_CHROMEOS)

}
