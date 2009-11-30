// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_DEVTOOLS_DEBUGGER_AGENT_IMPL_H_
#define WEBKIT_GLUE_DEVTOOLS_DEBUGGER_AGENT_IMPL_H_

#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>

#include "v8.h"
#include "webkit/glue/devtools/debugger_agent.h"

class WebDevToolsAgentImpl;

namespace WebCore {
class Document;
class Node;
class Page;
class String;
}

namespace WebKit {
class WebViewImpl;
}

class DebuggerAgentImpl : public DebuggerAgent {
 public:
  // Creates utility context with injected js agent.
  static void CreateUtilityContext(WebCore::Frame* frame,
                                   v8::Persistent<v8::Context>* context);

  DebuggerAgentImpl(WebKit::WebViewImpl* web_view_impl,
                    DebuggerAgentDelegate* delegate,
                    WebDevToolsAgentImpl* webdevtools_agent);
  virtual ~DebuggerAgentImpl();

  // DebuggerAgent implementation.
  virtual void GetContextId();

  virtual void StartProfiling(int flags);

  virtual void StopProfiling(int flags);

  virtual void GetActiveProfilerModules();

  virtual void GetNextLogLines();

  void DebuggerOutput(const WebCore::String& out);

  void set_auto_continue_on_exception(bool auto_continue) {
    auto_continue_on_exception_ = auto_continue;
  }

  bool auto_continue_on_exception() { return auto_continue_on_exception_; }

  // Executes function with the given name in the utility context. Passes node
  // and json args as parameters. Note that the function called must be
  // implemented in the inject_dispatch.js file.
  WebCore::String ExecuteUtilityFunction(
      v8::Handle<v8::Context> context,
      int call_id,
      const char* object,
      const WebCore::String& function_name,
      const WebCore::String& json_args,
      bool async,
      WebCore::String* exception);

  // Executes a no-op function in the utility context. We don't use
  // ExecuteUtilityFunction for that to avoid script evaluation leading to
  // undesirable AfterCompile events.
  void ExecuteVoidJavaScript(v8::Handle<v8::Context> context);

  WebCore::Page* GetPage();
  WebDevToolsAgentImpl* webdevtools_agent() { return webdevtools_agent_; };

  WebKit::WebViewImpl* web_view() { return web_view_impl_; }

 private:
  WebKit::WebViewImpl* web_view_impl_;
  DebuggerAgentDelegate* delegate_;
  WebDevToolsAgentImpl* webdevtools_agent_;
  int profiler_log_position_;
  bool auto_continue_on_exception_;
};

#endif  // WEBKIT_GLUE_DEVTOOLS_DEBUGGER_AGENT_IMPL_H_
