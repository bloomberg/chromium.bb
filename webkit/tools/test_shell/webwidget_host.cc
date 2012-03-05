// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/tools/test_shell/webwidget_host.h"

#include "base/bind.h"
#include "base/message_loop.h"

void WebWidgetHost::ScheduleAnimation() {
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&WebWidgetHost::ScheduleComposite, weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(10));
}
