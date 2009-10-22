// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_INSPECTOR_CLIENT_IMPL_H_
#define WEBKIT_GLUE_INSPECTOR_CLIENT_IMPL_H_

#include "InspectorClient.h"
#include "InspectorController.h"
#include <wtf/OwnPtr.h>

class WebNodeHighlight;
class WebViewImpl;

class InspectorClientImpl : public WebCore::InspectorClient {
 public:
  InspectorClientImpl(WebViewImpl*);
  ~InspectorClientImpl();

  // InspectorClient
  virtual void inspectorDestroyed();

  virtual WebCore::Page* createPage();
  virtual WebCore::String localizedStringsURL();
  virtual WebCore::String hiddenPanels();
  virtual void showWindow();
  virtual void closeWindow();
  virtual bool windowVisible();

  virtual void attachWindow();
  virtual void detachWindow();

  virtual void setAttachedWindowHeight(unsigned height);

  virtual void highlight(WebCore::Node*);
  virtual void hideHighlight();

  virtual void inspectedURLChanged(const WebCore::String& newURL);

  virtual void populateSetting(
      const WebCore::String& key,
      WebCore::InspectorController::Setting&);
  virtual void storeSetting(
      const WebCore::String& key,
      const WebCore::InspectorController::Setting&);
  virtual void removeSetting(const WebCore::String& key);

  virtual void inspectorWindowObjectCleared();

 private:
  void LoadSettings();
  void SaveSettings();

  // The WebViewImpl of the page being inspected; gets passed to the constructor
  WebViewImpl* inspected_web_view_;

  typedef HashMap<WebCore::String, WebCore::InspectorController::Setting>
      SettingsMap;
  OwnPtr<SettingsMap> settings_;
};

#endif // WEBKIT_GLUE_INSPECTOR_CLIENT_IMPL_H_
