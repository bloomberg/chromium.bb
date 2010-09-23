// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PEPPER_EVENT_H_
#define WEBKIT_GLUE_PLUGINS_PEPPER_EVENT_H_

#include <vector>

struct PP_InputEvent;

namespace WebKit {
class WebInputEvent;
}

namespace pepper {

// Converts the given WebKit event to one or possibly multiple PP_InputEvents.
// The generated events will be filled into the given vector. On failure, no
// events will ge generated and the vector will be empty.
void CreatePPEvent(const WebKit::WebInputEvent& event,
                   std::vector<PP_InputEvent>* pp_events);

// Creates a WebInputEvent from the given PP_InputEvent.  If it fails, returns
// NULL.  The caller owns the created object on success.
WebKit::WebInputEvent* CreateWebInputEvent(const PP_InputEvent& event);

}  // namespace pepper

#endif  // WEBKIT_GLUE_PLUGINS_PEPPER_EVENT_H_
