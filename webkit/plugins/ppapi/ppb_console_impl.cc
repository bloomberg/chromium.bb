// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_console_impl.h"

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "ppapi/c/dev/ppb_console_dev.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebConsoleMessage.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginContainer.h"
#include "webkit/plugins/ppapi/plugin_module.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/resource_tracker.h"
#include "webkit/plugins/ppapi/var.h"

using WebKit::WebConsoleMessage;
using WebKit::WebString;

namespace webkit {
namespace ppapi {

namespace {

void LogWithSource(PP_Instance instance_id,
                   PP_LogLevel_Dev level,
                   PP_Var source,
                   PP_Var value) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(instance_id);
  if (!instance)
    return;

  // Convert the log level, defaulting to error.
  WebConsoleMessage::Level web_level;
  switch (level) {
    case PP_LOGLEVEL_TIP:
      web_level = WebConsoleMessage::LevelTip;
      break;
    case PP_LOGLEVEL_LOG:
      web_level = WebConsoleMessage::LevelLog;
      break;
    case PP_LOGLEVEL_WARNING:
      web_level = WebConsoleMessage::LevelWarning;
      break;
    case PP_LOGLEVEL_ERROR:
    default:
      web_level = WebConsoleMessage::LevelError;
      break;
  }

  // Format is the "<source>: <value>". The source defaults to the module name
  // if the source isn't a string or is empty.
  std::string message;
  if (source.type == PP_VARTYPE_STRING)
    message = Var::PPVarToLogString(source);
  if (message.empty())
    message = instance->module()->name();
  message.append(": ");
  message.append(Var::PPVarToLogString(value));

  instance->container()->element().document().frame()->addMessageToConsole(
      WebConsoleMessage(web_level, WebString(UTF8ToUTF16(message))));
}

void Log(PP_Instance instance, PP_LogLevel_Dev level, PP_Var value) {
  LogWithSource(instance, level, PP_MakeUndefined(), value);
}

const PPB_Console_Dev ppb_console = {
  &Log,
  &LogWithSource
};

}  // namespace

// static
const struct PPB_Console_Dev* PPB_Console_Impl::GetInterface() {
  return &ppb_console;
}

}  // namespace ppapi
}  // namespace webkit

