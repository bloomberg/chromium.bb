// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/test/fakes/fake_web_frame.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/json/json_writer.h"
#include "base/task/post_task.h"
#include "base/values.h"
#include "ios/web/public/thread/web_task_traits.h"

namespace web {

// FakeMainWebFrame
FakeMainWebFrame::FakeMainWebFrame(GURL security_origin)
    : FakeWebFrame(kMainFakeFrameId, /*is_main_frame=*/true, security_origin) {}

FakeMainWebFrame::~FakeMainWebFrame() {}

// FakeChildWebFrame
FakeChildWebFrame::FakeChildWebFrame(GURL security_origin)
    : FakeWebFrame(kChildFakeFrameId,
                   /*is_main_frame=*/false,
                   security_origin) {}

FakeChildWebFrame::~FakeChildWebFrame() {}

}  // namespace web
