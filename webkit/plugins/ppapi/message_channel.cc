// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/message_channel.h"

#include <cstdlib>
#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "ppapi/shared_impl/var.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebBindings.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDOMMessageEvent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNode.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginContainer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSerializedScriptValue.h"
#include "v8/include/v8.h"
#include "webkit/plugins/ppapi/npapi_glue.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"

using ppapi::StringVar;
using WebKit::WebBindings;
using WebKit::WebElement;
using WebKit::WebDOMEvent;
using WebKit::WebDOMMessageEvent;
using WebKit::WebPluginContainer;
using WebKit::WebSerializedScriptValue;

namespace webkit {

namespace ppapi {

namespace {

const char kPostMessage[] = "postMessage";

// Helper function to get the MessageChannel that is associated with an
// NPObject*.
MessageChannel& ToMessageChannel(NPObject* object) {
  return *(static_cast<MessageChannel::MessageChannelNPObject*>(object)->
      message_channel);
}

// Helper function to determine if a given identifier is equal to kPostMessage.
bool IdentifierIsPostMessage(NPIdentifier identifier) {
  return WebBindings::getStringIdentifier(kPostMessage) == identifier;
}

// Converts the given PP_Var to a v8::Value, returning true on success.
// False means that the given variant is invalid. In this case, |result| will
// be set to an empty handle.
bool PPVarToV8Value(PP_Var var, v8::Handle<v8::Value>* result) {
  switch (var.type) {
    case PP_VARTYPE_UNDEFINED:
      *result = v8::Undefined();
      break;
    case PP_VARTYPE_NULL:
      *result = v8::Null();
      break;
    case PP_VARTYPE_BOOL:
      *result = (var.value.as_bool == PP_TRUE) ? v8::True() : v8::False();
      break;
    case PP_VARTYPE_INT32:
      *result = v8::Integer::New(var.value.as_int);
      break;
    case PP_VARTYPE_DOUBLE:
      *result = v8::Number::New(var.value.as_double);
      break;
    case PP_VARTYPE_STRING: {
      StringVar* string = StringVar::FromPPVar(var);
      if (!string) {
        result->Clear();
        return false;
      }
      const std::string& value = string->value();
      // TODO(dmichael): We should consider caching the V8 string in the host-
      // side StringVar, so that we only have to convert/copy once if a
      // string is sent more than once.
      *result = v8::String::New(value.c_str(), value.size());
      break;
    }
    case PP_VARTYPE_OBJECT:
      // Objects are not currently supported.
      NOTIMPLEMENTED();
      result->Clear();
      return false;
    default:
      result->Clear();
      return false;
  }
  return true;
}

// Copy a PP_Var in to a PP_Var that is appropriate for sending via postMessage.
// This currently just copies the value.  For a string Var, the result is a
// PP_Var with the a copy of |var|'s string contents and a reference count of 1.
//
// TODO(dmichael):  We need to do structured clone eventually to copy a object
// structure.  The details and PPAPI changes for this are TBD.
PP_Var CopyPPVar(const PP_Var& var) {
  if (var.type == PP_VARTYPE_OBJECT) {
    // Objects are not currently supported.
    NOTIMPLEMENTED();
    return PP_MakeUndefined();
  } else if (var.type == PP_VARTYPE_STRING) {
    StringVar* string = StringVar::FromPPVar(var);
    if (!string)
      return PP_MakeUndefined();
    return StringVar::StringToPPVar(string->pp_module(), string->value());
  } else {
    return var;
  }
}

//------------------------------------------------------------------------------
// Implementations of NPClass functions.  These are here to:
// - Implement postMessage behavior.
// - Forward calls to the 'passthrough' object to allow backwards-compatibility
//   with GetInstanceObject() objects.
//------------------------------------------------------------------------------
NPObject* MessageChannelAllocate(NPP npp, NPClass* the_class) {
  return new MessageChannel::MessageChannelNPObject;
}

void MessageChannelDeallocate(NPObject* object) {
  MessageChannel::MessageChannelNPObject* instance =
      static_cast<MessageChannel::MessageChannelNPObject*>(object);
  delete instance;
}

bool MessageChannelHasMethod(NPObject* np_obj, NPIdentifier name) {
  if (!np_obj)
    return false;

  // We only handle a function called postMessage.
  if (IdentifierIsPostMessage(name))
    return true;

  // Other method names we will pass to the passthrough object, if we have one.
  NPObject* passthrough = ToMessageChannel(np_obj).passthrough_object();
  if (passthrough)
    return WebBindings::hasMethod(NULL, passthrough, name);
  return false;
}

bool MessageChannelInvoke(NPObject* np_obj, NPIdentifier name,
                          const NPVariant* args, uint32 arg_count,
                          NPVariant* result) {
  if (!np_obj)
    return false;

  // We only handle a function called postMessage.
  if (IdentifierIsPostMessage(name) && (arg_count == 1)) {
    MessageChannel& message_channel(ToMessageChannel(np_obj));
    PP_Var argument(NPVariantToPPVar(message_channel.instance(), &args[0]));
    message_channel.PostMessageToNative(argument);
    return true;
  }
  // Other method calls we will pass to the passthrough object, if we have one.
  NPObject* passthrough = ToMessageChannel(np_obj).passthrough_object();
  if (passthrough) {
    return WebBindings::invoke(NULL, passthrough, name, args, arg_count,
                               result);
  }
  return false;
}

bool MessageChannelInvokeDefault(NPObject* np_obj,
                                 const NPVariant* args,
                                 uint32 arg_count,
                                 NPVariant* result) {
  if (!np_obj)
    return false;

  // Invoke on the passthrough object, if we have one.
  NPObject* passthrough = ToMessageChannel(np_obj).passthrough_object();
  if (passthrough) {
    return WebBindings::invokeDefault(NULL, passthrough, args, arg_count,
                                      result);
  }
  return false;
}

bool MessageChannelHasProperty(NPObject* np_obj, NPIdentifier name) {
  if (!np_obj)
    return false;

  // Invoke on the passthrough object, if we have one.
  NPObject* passthrough = ToMessageChannel(np_obj).passthrough_object();
  if (passthrough)
    return WebBindings::hasProperty(NULL, passthrough, name);
  return false;
}

bool MessageChannelGetProperty(NPObject* np_obj, NPIdentifier name,
                               NPVariant* result) {
  if (!np_obj)
    return false;

  // Don't allow getting the postMessage function.
  if (IdentifierIsPostMessage(name))
    return false;

  // Invoke on the passthrough object, if we have one.
  NPObject* passthrough = ToMessageChannel(np_obj).passthrough_object();
  if (passthrough)
    return WebBindings::getProperty(NULL, passthrough, name, result);
  return false;
}

bool MessageChannelSetProperty(NPObject* np_obj, NPIdentifier name,
                               const NPVariant* variant) {
  if (!np_obj)
    return false;

  // Don't allow setting the postMessage function.
  if (IdentifierIsPostMessage(name))
    return false;

  // Invoke on the passthrough object, if we have one.
  NPObject* passthrough = ToMessageChannel(np_obj).passthrough_object();
  if (passthrough)
    return WebBindings::setProperty(NULL, passthrough, name, variant);
  return false;
}

bool MessageChannelEnumerate(NPObject *np_obj, NPIdentifier **value,
                             uint32_t *count) {
  if (!np_obj)
    return false;

  // Invoke on the passthrough object, if we have one, to enumerate its
  // properties.
  NPObject* passthrough = ToMessageChannel(np_obj).passthrough_object();
  if (passthrough) {
    bool success = WebBindings::enumerate(NULL, passthrough, value, count);
    if (success) {
      // Add postMessage to the list and return it.
      if (std::numeric_limits<size_t>::max() / sizeof(NPIdentifier) <=
          (*count + 1))
        return false;
      NPIdentifier* new_array = static_cast<NPIdentifier*>(
          std::malloc(sizeof(NPIdentifier) * (*count + 1)));
      std::memcpy(new_array, *value, sizeof(NPIdentifier)*(*count));
      new_array[*count] = WebBindings::getStringIdentifier(kPostMessage);
      std::free(*value);
      *value = new_array;
      ++(*count);
      return true;
    }
  }

  // Otherwise, build an array that includes only postMessage.
  *value = static_cast<NPIdentifier*>(malloc(sizeof(NPIdentifier)));
  (*value)[0] = WebBindings::getStringIdentifier(kPostMessage);
  *count = 1;
  return true;
}

NPClass message_channel_class = {
  NP_CLASS_STRUCT_VERSION,
  &MessageChannelAllocate,
  &MessageChannelDeallocate,
  NULL,
  &MessageChannelHasMethod,
  &MessageChannelInvoke,
  &MessageChannelInvokeDefault,
  &MessageChannelHasProperty,
  &MessageChannelGetProperty,
  &MessageChannelSetProperty,
  NULL,
  &MessageChannelEnumerate,
};

}  // namespace

// MessageChannel --------------------------------------------------------------
MessageChannel::MessageChannelNPObject::MessageChannelNPObject()
    : message_channel(NULL) {
}

MessageChannel::MessageChannelNPObject::~MessageChannelNPObject() {}

MessageChannel::MessageChannel(PluginInstance* instance)
    : instance_(instance),
      passthrough_object_(NULL),
      np_object_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  // Now create an NPObject for receiving calls to postMessage. This sets the
  // reference count to 1.  We release it in the destructor.
  NPObject* obj = WebBindings::createObject(NULL, &message_channel_class);
  DCHECK(obj);
  np_object_ = static_cast<MessageChannel::MessageChannelNPObject*>(obj);
  np_object_->message_channel = this;
}

void MessageChannel::PostMessageToJavaScript(PP_Var message_data) {
  // Serialize the message data.
  v8::HandleScope scope;
  v8::Handle<v8::Value> v8_val;
  if (!PPVarToV8Value(message_data, &v8_val)) {
    NOTREACHED();
    return;
  }

  WebSerializedScriptValue serialized_val =
      WebSerializedScriptValue::serialize(v8_val);

  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&MessageChannel::PostMessageToJavaScriptImpl,
                 weak_ptr_factory_.GetWeakPtr(),
                 serialized_val));
}

void MessageChannel::PostMessageToJavaScriptImpl(
    const WebSerializedScriptValue& message_data) {
  DCHECK(instance_);

  WebPluginContainer* container = instance_->container();
  // It's possible that container() is NULL if the plugin has been removed from
  // the DOM (but the PluginInstance is not destroyed yet).
  if (!container)
    return;

  WebDOMEvent event =
      container->element().document().createEvent("MessageEvent");
  WebDOMMessageEvent msg_event = event.to<WebDOMMessageEvent>();
  msg_event.initMessageEvent("message",  // type
                             false,  // canBubble
                             false,  // cancelable
                             message_data,  // data
                             "",  // origin [*]
                             NULL,  // source [*]
                             "");  // lastEventId
  // [*] Note that the |origin| is only specified for cross-document and server-
  //     sent messages, while |source| is only specified for cross-document
  //     messages:
  //      http://www.whatwg.org/specs/web-apps/current-work/multipage/comms.html
  //     This currently behaves like Web Workers. On Firefox, Chrome, and Safari
  //     at least, postMessage on Workers does not provide the origin or source.
  //     TODO(dmichael):  Add origin if we change to a more iframe-like origin
  //                      policy (see crbug.com/81537)

  container->element().dispatchEvent(msg_event);
}

void MessageChannel::PostMessageToNative(PP_Var message_data) {
  // Make a copy of the message data for the Task we will run.
  PP_Var var_copy(CopyPPVar(message_data));

  MessageLoop::current()->PostTask(FROM_HERE,
      base::Bind(&MessageChannel::PostMessageToNativeImpl,
                 weak_ptr_factory_.GetWeakPtr(),
                 var_copy));
}

void MessageChannel::PostMessageToNativeImpl(PP_Var message_data) {
  instance_->HandleMessage(message_data);
}

MessageChannel::~MessageChannel() {
  WebBindings::releaseObject(np_object_);
  if (passthrough_object_)
    WebBindings::releaseObject(passthrough_object_);
}

void MessageChannel::SetPassthroughObject(NPObject* passthrough) {
  // Retain the passthrough object; We need to ensure it lives as long as this
  // MessageChannel.
  WebBindings::retainObject(passthrough);

  // If we had a passthrough set already, release it. Note that we retain the
  // incoming passthrough object first, so that we behave correctly if anyone
  // invokes:
  //   SetPassthroughObject(passthrough_object());
  if (passthrough_object_)
    WebBindings::releaseObject(passthrough_object_);

  passthrough_object_ = passthrough;
}

}  // namespace ppapi
}  // namespace webkit

