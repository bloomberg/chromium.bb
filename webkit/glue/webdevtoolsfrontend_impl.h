// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBDEVTOOLSFRONTEND_IMPL_H_
#define WEBKIT_GLUE_WEBDEVTOOLSFRONTEND_IMPL_H_

#include <string>

#include "ContextMenuProvider.h"

#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>
#include <wtf/OwnPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

#include "v8.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDevToolsFrontend.h"
#include "webkit/glue/devtools/devtools_rpc.h"

namespace WebCore {
class ContextMenuItem;
class Node;
class Page;
class String;
}

namespace WebKit {
class WebViewImpl;
struct WebDevToolsMessageData;
}

class JsDebuggerAgentBoundObj;
class JsNetAgentBoundObj;
class JsProfilerAgentBoundObj;
class JsToolsAgentBoundObj;
class ToolsAgentNativeDelegateImpl;
class WebDevToolsClientDelegate;

class WebDevToolsFrontendImpl : public WebKit::WebDevToolsFrontend,
                                public DevToolsRpc::Delegate,
                                public Noncopyable {
 public:
  WebDevToolsFrontendImpl(
      WebKit::WebViewImpl* web_view_impl,
      WebKit::WebDevToolsFrontendClient* client,
      const String& application_locale);
  virtual ~WebDevToolsFrontendImpl();

  // DevToolsRpc::Delegate implementation.
  virtual void SendRpcMessage(const WebKit::WebDevToolsMessageData& data);

  // WebDevToolsFrontend implementation.
  virtual void dispatchMessageFromAgent(const WebKit::WebString& class_name,
                                        const WebKit::WebString& method_name,
                                        const WebKit::WebString& param1,
                                        const WebKit::WebString& param2,
                                        const WebKit::WebString& param3);
  virtual void dispatchMessageFromAgent(
      const WebKit::WebDevToolsMessageData& data);

 private:
  class MenuProvider : public WebCore::ContextMenuProvider {
   public:
    static PassRefPtr<MenuProvider> create(
        WebDevToolsFrontendImpl* frontend_host,
        const Vector<WebCore::ContextMenuItem*>& items) {
      return adoptRef(new MenuProvider(frontend_host, items));
    }

    virtual ~MenuProvider() {
      contextMenuCleared();
    }

    void disconnect() {
      frontend_host_ = NULL;
    }

    virtual void populateContextMenu(WebCore::ContextMenu* menu) {
      for (size_t i = 0; i < items_.size(); ++i)
        menu->appendItem(*items_[i]);
    }

    virtual void contextMenuItemSelected(WebCore::ContextMenuItem* item) {
      if (frontend_host_)
        frontend_host_->ContextMenuItemSelected(item);
    }

    virtual void contextMenuCleared() {
      if (frontend_host_)
        frontend_host_->ContextMenuCleared();
      deleteAllValues(items_);
      items_.clear();
    }

    private:
     MenuProvider(WebDevToolsFrontendImpl* frontend_host,
                  const Vector<WebCore::ContextMenuItem*>& items)
         : frontend_host_(frontend_host),
           items_(items) { }
     WebDevToolsFrontendImpl* frontend_host_;
     Vector<WebCore::ContextMenuItem*> items_;
  };

  void AddResourceSourceToFrame(int resource_id,
                                String mime_type,
                                WebCore::Node* frame);

  void ExecuteScript(const Vector<String>& v);
  void DispatchOnWebInspector(const String& method, const String& param);

  // friend class MenuSelectionHandler;
  void ContextMenuItemSelected(WebCore::ContextMenuItem* menu_item);
  void ContextMenuCleared();

  static v8::Handle<v8::Value> JsReset(const v8::Arguments& args);
  static v8::Handle<v8::Value> JsAddSourceToFrame(const v8::Arguments& args);
  static v8::Handle<v8::Value> JsAddResourceSourceToFrame(
      const v8::Arguments& args);
  static v8::Handle<v8::Value> JsLoaded(const v8::Arguments& args);
  static v8::Handle<v8::Value> JsPlatform(const v8::Arguments& args);
  static v8::Handle<v8::Value> JsPort(const v8::Arguments& args);

  static v8::Handle<v8::Value> JsActivateWindow(const v8::Arguments& args);
  static v8::Handle<v8::Value> JsCloseWindow(const v8::Arguments& args);
  static v8::Handle<v8::Value> JsDockWindow(const v8::Arguments& args);
  static v8::Handle<v8::Value> JsUndockWindow(const v8::Arguments& args);
  static v8::Handle<v8::Value> JsLocalizedStringsURL(
      const v8::Arguments& args);
  static v8::Handle<v8::Value> JsHiddenPanels(
      const v8::Arguments& args);
  static v8::Handle<v8::Value> JsDebuggerCommand(
      const v8::Arguments& args);
  static v8::Handle<v8::Value> JsSetting(
      const v8::Arguments& args);
  static v8::Handle<v8::Value> JsSetSetting(
      const v8::Arguments& args);
  static v8::Handle<v8::Value> JsDebuggerPauseScript(
      const v8::Arguments& args);
  static v8::Handle<v8::Value> JsWindowUnloading(
      const v8::Arguments& args);
  static v8::Handle<v8::Value> JsShowContextMenu(
      const v8::Arguments& args);

  WebKit::WebViewImpl* web_view_impl_;
  WebKit::WebDevToolsFrontendClient* client_;
  String application_locale_;
  OwnPtr<JsDebuggerAgentBoundObj> debugger_agent_obj_;
  OwnPtr<JsProfilerAgentBoundObj> profiler_agent_obj_;
  OwnPtr<JsToolsAgentBoundObj> tools_agent_obj_;
  bool loaded_;
  Vector<Vector<String> > pending_incoming_messages_;
  OwnPtr<ToolsAgentNativeDelegateImpl> tools_agent_native_delegate_impl_;
  RefPtr<MenuProvider> menu_provider_;
};

#endif  // WEBKIT_GLUE_WEBDEVTOOLSFRONTEND_IMPL_H_
