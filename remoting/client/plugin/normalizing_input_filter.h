// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_PLUGIN_NORMALIZING_INPUT_FILTER_H_
#define REMOTING_CLIENT_PLUGIN_NORMALIZING_INPUT_FILTER_H_

#include "base/memory/scoped_ptr.h"
#include "remoting/protocol/input_filter.h"

namespace remoting {

// Returns an InputFilter which re-writes input events to work around
// platform-specific behaviours. If no re-writing is required then a
// pass-through InputFilter is returned.
scoped_ptr<protocol::InputFilter> CreateNormalizingInputFilter(
    protocol::InputStub* input_stub);

}  // namespace remoting

#endif  // REMOTING_CLIENT_PLUGIN_NORMALIZING_INPUT_FILTER_H_
