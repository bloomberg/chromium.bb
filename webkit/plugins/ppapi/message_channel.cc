// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/message_channel.h"

#include <cstdlib>
#include <string>

#include "base/logging.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebBindings.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginContainer.h"
#include "webkit/plugins/ppapi/npapi_glue.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/var.h"

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
      scoped_refptr<StringVar> string(StringVar::FromPPVar(var));
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
      DCHECK(false);
      VOID_TO_NPVARIANT(*result);
      return false;
    default:
      VOID_TO_NPVARIANT(*result);
      return false;
  }
  return true;
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
    PP_Var argument(Var::NPVariantToPPVar(message_channel.instance(),
                                          &args[0]));
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
MessageChannel::MessageChannelNPObject::MessageChannelNPObject() {}

MessageChannel::MessageChannelNPObject::~MessageChannelNPObject() {}

MessageChannel::MessageChannel(PluginInstance* instance)
    : instance_(instance),
      passthrough_object_(NULL),
      np_object_(NULL) {
  VOID_TO_NPVARIANT(onmessage_invoker_);

  // Now create an NPObject for receiving calls to postMessage.
  NPObject* obj = WebBindings::createObject(NULL, &message_channel_class);
  DCHECK(obj);
  np_object_ = static_cast<MessageChannel::MessageChannelNPObject*>(obj);
  np_object_->message_channel = this;
}

bool MessageChannel::EvaluateOnMessageInvoker() {
  // If we've already evaluated the function, just return.
  if (NPVARIANT_IS_OBJECT(onmessage_invoker_))
    return true;

  // This is the javascript code that we invoke.  It checks to see if onmessage
  // exists, and if so, it invokes it.
  const char invoke_onmessage_js[] =
      "(function(module_instance, message_data) {"
      "  if (module_instance &&"  // Only invoke if the instance is valid and
      "      module_instance.onmessage &&"  // has a function named onmessage.
      "      typeof(module_instance.onmessage) == 'function') {"
      "    var message_event = document.createEvent('MessageEvent');"
      "    message_event.initMessageEvent('message',"  // type
      "                                   false,"  // canBubble
      "                                   false,"  // cancelable
      "                                   message_data,"  // data
      "                                   '',"  // origin
      "                                   '',"  // lastEventId
      "                                   module_instance,"  // source
      "                                   []);"  // ports
      "    module_instance.onmessage(message_event);"
      "  }"
      "})";
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
  // Make sure we have our function for invoking a JavaScript onmessage
  // function.
  bool success = EvaluateOnMessageInvoker();
  DCHECK(success);
  if (!success)
    return;

  DCHECK(instance_);

  NPVariant result_var;
  VOID_TO_NPVARIANT(result_var);
  NPVariant npvariant_args[2];
  OBJECT_TO_NPVARIANT(instance_->container()->scriptableObjectForElement(),
                      npvariant_args[0]);
  // Convert message to an NPVariant without copying.  Note this means that
  // in-process plugins will not copy the data, so isn't really following the
  // postMessage spec in spirit.  Copying is handled in the proxy, and we don't
  // want to re-copy unnecessarily.
  //
  // TODO(dmichael):  We need to do structured clone eventually to copy a object
  // structure.  The details and PPAPI changes for this are TBD.
  if (!PPVarToNPVariantNoCopy(message_data, &npvariant_args[1]))
    return;

  WebBindings::invokeDefault(NULL,
                             NPVARIANT_TO_OBJECT(onmessage_invoker_),
                             npvariant_args,
                             sizeof(npvariant_args)/sizeof(*npvariant_args),
                             &result_var);
}

void MessageChannel::PostMessageToNative(PP_Var message_data) {
  instance_->HandleMessage(message_data);
}

MessageChannel::~MessageChannel() {
  WebBindings::releaseVariantValue(&onmessage_invoker_);
}

}  // namespace ppapi
}  // namespace webkit

