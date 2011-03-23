// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_MESSAGE_CHANNEL_H_
#define WEBKIT_PLUGINS_PPAPI_MESSAGE_CHANNEL_H_

#include "third_party/npapi/bindings/npruntime.h"
#include "webkit/plugins/ppapi/resource.h"

struct PP_Var;

namespace webkit {
namespace ppapi {

class PluginInstance;

// MessageChannel implements bidirectional postMessage functionality, allowing
// calls from JavaScript to plugins and vice-versa. See
// PPB_Instance::PostMessage and PPP_Instance::HandleMessage for more
// information.
//
// Currently, only 1 MessageChannel can exist, to implement postMessage
// functionality for the instance interfaces.  In the future, when we create a
// MessagePort type in PPAPI, those may be implemented here as well with some
// refactoring.
//   - Separate message ports won't require the passthrough object.
//   - The message target won't be limited to instance, and should support
//     either plugin-provided or JS objects.
// TODO(dmichael):  Add support for separate MessagePorts.
class MessageChannel {
 public:
  // MessageChannelNPObject is a simple struct that adds a pointer back to a
  // MessageChannel instance.  This way, we can use an NPObject to allow
  // JavaScript interactions without forcing MessageChannel to inherit from
  // NPObject.
  struct MessageChannelNPObject : public NPObject {
    MessageChannelNPObject();
    ~MessageChannelNPObject();

    MessageChannel* message_channel;
  };

  explicit MessageChannel(PluginInstance* instance);
  ~MessageChannel();

  void PostMessageToJavaScript(PP_Var message_data);
  void PostMessageToNative(PP_Var message_data);

  // Return the NPObject* to which we should forward any calls which aren't
  // related to postMessage.  Note that this can be NULL;  it only gets set if
  // there is a scriptable 'InstanceObject' associated with this channel's
  // instance.
  NPObject* passthrough_object() {
    return passthrough_object_;
  }
  void set_passthrough_object(NPObject* passthrough) {
    passthrough_object_ = passthrough;
  }

  NPObject* np_object() { return np_object_; }

  PluginInstance* instance() {
    return instance_;
  }

 private:
  PluginInstance* instance_;

  // We pass all non-postMessage calls through to the passthrough_object_.
  // This way, a plugin can use PPB_Class or PPP_Class_Deprecated and also
  // postMessage.  This is necessary to support backwards-compatibility, and
  // also trusted plugins for which we will continue to support synchronous
  // scripting.
  NPObject* passthrough_object_;

  // The NPObject we use to expose postMessage to JavaScript.
  MessageChannelNPObject* np_object_;

  // An NPVariant referring to the JavaScript function we use to send a message
  // to a JavaScript target.
  NPVariant onmessage_invoker_;

  bool EvaluateOnMessageInvoker();

  DISALLOW_COPY_AND_ASSIGN(MessageChannel);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_MESSAGE_CHANNEL_H_

