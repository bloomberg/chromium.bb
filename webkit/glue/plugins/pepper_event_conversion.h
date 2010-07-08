// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PEPPER_EVENT_H_
#define WEBKIT_GLUE_PLUGINS_PEPPER_EVENT_H_

typedef struct _pp_Event PP_Event;

namespace WebKit {
class WebInputEvent;
}

namespace pepper {

// Creates a PP_Event from the given WebInputEvent.  If it fails, returns NULL.
// The caller owns the created object on success.
PP_Event* CreatePP_Event(const WebKit::WebInputEvent& event);

// Creates a WebInputEvent from the given PP_Event.  If it fails, returns NULL.
// The caller owns the created object on success.
WebKit::WebInputEvent* CreateWebInputEvent(const PP_Event& event);

}  // namespace pepper

#endif  // WEBKIT_GLUE_PLUGINS_PEPPER_EVENT_H_
