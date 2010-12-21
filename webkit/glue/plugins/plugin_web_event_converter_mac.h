// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGIN_PLUGIN_WEB_EVENT_CONVERTER_MAC_H_
#define WEBKIT_GLUE_PLUGIN_PLUGIN_WEB_EVENT_CONVERTER_MAC_H_

#include "third_party/npapi/bindings/npapi.h"

namespace WebKit {
class WebInputEvent;
class WebKeyboardEvent;
class WebMouseEvent;
class WebMouseWheelEvent;
}

// Utility class to translating WebInputEvent structs to equivalent structures
// suitable for sending to Mac plugins (via NPP_HandleEvent).
class PluginWebEventConverter {
 public:
  PluginWebEventConverter() {}
  virtual ~PluginWebEventConverter() {}

  // Initializes a converter for the given web event. Returns false if the event
  // could not be converted.
  virtual bool InitWithEvent(const WebKit::WebInputEvent& web_event);

  // Returns a pointer to a plugin event--suitable for passing to
  // NPP_HandleEvent--corresponding to the the web event this converter was
  // created with. The pointer is valid only as long as this object is.
  // Returns NULL iff InitWithEvent returned false.
  virtual void* plugin_event() = 0;

protected:
  // To be overridden by subclasses to store a converted plugin representation
  // of the given web event, suitable for returning from plugin_event.
  // Returns true if the event was successfully converted.
  virtual bool ConvertKeyboardEvent(
      const WebKit::WebKeyboardEvent& web_event) = 0;
  virtual bool ConvertMouseEvent(const WebKit::WebMouseEvent& web_event) = 0;
  virtual bool ConvertMouseWheelEvent(
      const WebKit::WebMouseWheelEvent& web_event) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(PluginWebEventConverter);
};

// Factory for generating PluginWebEventConverter objects by event model.
class PluginWebEventConverterFactory {
 public:
  // Returns a new PluginWebEventConverter corresponding to the given plugin
  // event model.
  static PluginWebEventConverter*
      CreateConverterForModel(NPEventModel event_model);

 private:
  DISALLOW_COPY_AND_ASSIGN(PluginWebEventConverterFactory);
};

#endif  // WEBKIT_GLUE_PLUGIN_PLUGIN_WEB_EVENT_CONVERTER_MAC_H_
