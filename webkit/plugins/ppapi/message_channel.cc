// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/message_channel.h"

#include <cstdlib>
#include <string>

#include "base/logging.h"
#include "base/message_loop.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebBindings.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginContainer.h"
#include "ppapi/shared_impl/var.h"
#include "webkit/plugins/ppapi/npapi_glue.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"

using ppapi::StringVar;
using WebKit::WebBindings;

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

// Converts the given PP_Var to an NPVariant, returning true on success.
// False means that the given variant is invalid. In this case, the result
// NPVariant will be set to a void one.
//
// The contents of the PP_Var will NOT be copied, so you need to ensure that
// the PP_Var remains valid while the resultant NPVariant is in use.
//
// Note:  This is largely copied from var.cc so that we don't depend on code
//        which will be removed.  TODO(dmichael) remove this comment when var
//        is removed.
bool PPVarToNPVariantNoCopy(PP_Var var, NPVariant* result) {
  switch (var.type) {
    case PP_VARTYPE_UNDEFINED:
      VOID_TO_NPVARIANT(*result);
      break;
    case PP_VARTYPE_NULL:
      NULL_TO_NPVARIANT(*result);
      break;
    case PP_VARTYPE_BOOL:
      BOOLEAN_TO_NPVARIANT(var.value.as_bool, *result);
      break;
    case PP_VARTYPE_INT32:
      INT32_TO_NPVARIANT(var.value.as_int, *result);
      break;
    case PP_VARTYPE_DOUBLE:
      DOUBLE_TO_NPVARIANT(var.value.as_double, *result);
      break;
    case PP_VARTYPE_STRING: {
      StringVar* string = StringVar::FromPPVar(var);
      if (!string) {
        VOID_TO_NPVARIANT(*result);
        return false;
      }
      const std::string& value = string->value();
      STRINGN_TO_NPVARIANT(value.c_str(), value.size(), *result);
      break;
    }
    case PP_VARTYPE_OBJECT:
      // Objects are not currently supported.
      NOTIMPLEMENTED();
      VOID_TO_NPVARIANT(*result);
      return false;
    default:
      VOID_TO_NPVARIANT(*result);
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
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {
  VOID_TO_NPVARIANT(onmessage_invoker_);

  // Now create an NPObject for receiving calls to postMessage. This sets the
  // reference count to 1.  We release it in the destructor.
  NPObject* obj = WebBindings::createObject(NULL, &message_channel_class);
  DCHECK(obj);
  np_object_ = static_cast<MessageChannel::MessageChannelNPObject*>(obj);
  np_object_->message_channel = this;
}

bool MessageChannel::EvaluateOnMessageInvoker() {
  // If we've already evaluated the function, just return.
  if (NPVARIANT_IS_OBJECT(onmessage_invoker_))
    return true;

  // This is the javascript code that we invoke to create and dispatch a
  // message event.
  const char invoke_onmessage_js[] =
      "(function(window, module_instance, message_data) {"
      "  if (module_instance) {"
      "    var message_event = window.document.createEvent('MessageEvent');"
      "    message_event.initMessageEvent('message',"  // type
      "                                   false,"  // canBubble
      "                                   false,"  // cancelable
      "                                   message_data,"  // data
      "                                   '',"  // origin [*]
      "                                   '',"  // lastEventId
      "                                   null,"  // source [*]
      "                                   []);"  // ports
      "    module_instance.dispatchEvent(message_event);"
      "  }"
      "})";
  // [*] Note that the |origin| is only specified for cross-document and server-
  //     sent messages, while |source| is only specified for cross-document
  //     messages:
  //      http://www.whatwg.org/specs/web-apps/current-work/multipage/comms.html
  //     This currently behaves like Web Workers. On Firefox, Chrome, and Safari
  //     at least, postMessage on Workers does not provide the origin or source.
  //     TODO(dmichael):  Add origin if we change to a more iframe-like origin
  //                      policy (see crbug.com/81537)

  NPString function_string = { invoke_onmessage_js,
                               sizeof(invoke_onmessage_js)-1 };
  // Get the current frame to pass to the evaluate function.
  WebKit::WebFrame* frame =
      instance_->container()->element().document().frame();
  // Evaluate the function and obtain an NPVariant pointing to it.
  if (!WebBindings::evaluate(NULL, frame->windowObject(), &function_string,
                             &onmessage_invoker_)) {
    // If it fails, do nothing.
    return false;
  }
  DCHECK(NPVARIANT_IS_OBJECT(onmessage_invoker_));
  return true;
}

void MessageChannel::PostMessageToJavaScript(PP_Var message_data) {
  // Make a copy of the message data for the Task we will run.
  PP_Var var_copy(CopyPPVar(message_data));

  MessageLoop::current()->PostTask(
      FROM_HERE,
      method_factory_.NewRunnableMethod(
          &MessageChannel::PostMessageToJavaScriptImpl,
          var_copy));
}

void MessageChannel::PostMessageToJavaScriptImpl(PP_Var message_data) {
  // Make sure we have our function for invoking onmessage on JavaScript.
  bool success = EvaluateOnMessageInvoker();
  DCHECK(success);
  if (!success)
    return;

  DCHECK(instance_);

  NPVariant result_var;
  VOID_TO_NPVARIANT(result_var);
  NPVariant npvariant_args[3];
  // Get the frame so we can get the window object.
  WebKit::WebFrame* frame =
      instance_->container()->element().document().frame();
  if (!frame)
    return;

  OBJECT_TO_NPVARIANT(frame->windowObject(), npvariant_args[0]);
  OBJECT_TO_NPVARIANT(instance_->container()->scriptableObjectForElement(),
                      npvariant_args[1]);
  // Convert message to an NPVariant without copying. At this point, the data
  // has already been copied.
  if (!PPVarToNPVariantNoCopy(message_data, &npvariant_args[2])) {
    // We couldn't create an NPVariant, so we can't invoke the method.  Thus,
    // WebBindings::invokeDefault does not take ownership of these variants, so
    // we must release our references to them explicitly.
    WebBindings::releaseVariantValue(&npvariant_args[0]);
    WebBindings::releaseVariantValue(&npvariant_args[1]);
    return;
  }

  WebBindings::invokeDefault(NULL,
                             NPVARIANT_TO_OBJECT(onmessage_invoker_),
                             npvariant_args,
                             sizeof(npvariant_args)/sizeof(*npvariant_args),
                             &result_var);
}

void MessageChannel::PostMessageToNative(PP_Var message_data) {
  // Make a copy of the message data for the Task we will run.
  PP_Var var_copy(CopyPPVar(message_data));

  MessageLoop::current()->PostTask(FROM_HERE,
      method_factory_.NewRunnableMethod(
          &MessageChannel::PostMessageToNativeImpl,
          var_copy));
}

void MessageChannel::PostMessageToNativeImpl(PP_Var message_data) {
  instance_->HandleMessage(message_data);
}

MessageChannel::~MessageChannel() {
  WebBindings::releaseObject(np_object_);
  if (passthrough_object_)
    WebBindings::releaseObject(passthrough_object_);
  WebBindings::releaseVariantValue(&onmessage_invoker_);
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

