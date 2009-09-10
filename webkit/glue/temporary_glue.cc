// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "webkit/api/src/TemporaryGlue.h"

#include "Page.h"
#include <wtf/Assertions.h>
#undef LOG

#include "webkit/api/public/WebFrameClient.h"
#include "webkit/glue/chrome_client_impl.h"
#include "webkit/glue/webframe_impl.h"

using WebCore::Frame;
using WebCore::Page;

namespace WebKit {

// static
WebMediaPlayer* TemporaryGlue::createWebMediaPlayer(
    WebMediaPlayerClient* client, Frame* frame) {
  WebFrameImpl* webframe = WebFrameImpl::FromFrame(frame);
  if (!webframe->client())
    return NULL;

  return webframe->client()->createMediaPlayer(webframe, client);
}

// static
void TemporaryGlue::setCursorForPlugin(
    const WebCursorInfo& cursor_info, Frame* frame) {
  Page* page = frame->page();
  if (!page)
      return;

  ChromeClientImpl* chrome_client =
      static_cast<ChromeClientImpl*>(page->chrome()->client());

  // A windowless plugin can change the cursor in response to the WM_MOUSEMOVE
  // event. We need to reflect the changed cursor in the frame view as the
  // mouse is moved in the boundaries of the windowless plugin.
  chrome_client->SetCursorForPlugin(cursor_info);
}

}  // namespace WebKit
