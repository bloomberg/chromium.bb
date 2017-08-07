// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.#ifndef WebViewBase_h

#ifndef WebLocalFrameBase_h
#define WebLocalFrameBase_h

#include "core/CoreExport.h"
#include "core/frame/WebFrameWidgetBase.h"
#include "core/loader/FrameLoaderTypes.h"
#include "public/web/WebLocalFrame.h"

namespace blink {

class ContentSettingsClient;
class FrameOwner;
class LocalFrame;
class LocalFrameView;
class Node;
class Page;
class SharedWorkerRepositoryClientImpl;
class TextCheckerClient;
class TextFinder;
class WebDevToolsAgentImpl;
class WebDevToolsFrontendImpl;
class WebFrameClient;
class WebFrameWidgetBase;
class WebTextCheckClient;
class WebViewBase;

// WebLocalFrameBase is a temporary class the provides a layer of abstraction
// for WebLocalFrameImpl. Mehtods that are declared public in WebLocalFrameImpl
// that are not overrides from WebLocalFrame will be declared pure virtual in
// WebLocalFrameBase. Classes that then have a dependency on WebLocalFrameImpl
// will then take a dependency on WebLocalFrameBase instead, so we can remove
// cyclic dependencies in web/ and move classes from web/ into core/ or
// modules.
// TODO(slangley): Remove this class once WebLocalFrameImpl is in core/.
class WebLocalFrameBase : public GarbageCollectedFinalized<WebLocalFrameBase>,
                          public WebLocalFrame {
 public:
  virtual WebViewBase* ViewImpl() const = 0;
  virtual WebFrameClient* Client() const = 0;
  virtual void SetClient(WebFrameClient*) = 0;
  virtual WebTextCheckClient* TextCheckClient() const = 0;
  virtual void SetContextMenuNode(Node*) = 0;
  virtual void ClearContextMenuNode() = 0;
  virtual LocalFrame* GetFrame() const = 0;
  virtual LocalFrameView* GetFrameView() const = 0;
  virtual void InitializeCoreFrame(Page&,
                                   FrameOwner*,
                                   const AtomicString& name) = 0;
  virtual TextFinder& EnsureTextFinder() = 0;
  virtual WebFrameWidgetBase* FrameWidget() const = 0;
  virtual void SetFrameWidget(WebFrameWidgetBase*) = 0;
  virtual WebDevToolsAgentImpl* DevToolsAgentImpl() const = 0;
  virtual void SetDevToolsFrontend(WebDevToolsFrontendImpl*) = 0;
  virtual WebDevToolsFrontendImpl* DevToolsFrontend() = 0;
  virtual void WillBeDetached() = 0;
  virtual void WillDetachParent() = 0;
  virtual void SetCoreFrame(LocalFrame*) = 0;
  virtual void DidFail(const ResourceError&,
                       bool was_provisional,
                       HistoryCommitType) = 0;
  virtual void DidFinish() = 0;
  virtual void CreateFrameView() = 0;
  virtual LocalFrame* CreateChildFrame(const AtomicString& name,
                                       HTMLFrameOwnerElement*) = 0;
  virtual ContentSettingsClient& GetContentSettingsClient() = 0;
  virtual SharedWorkerRepositoryClientImpl* SharedWorkerRepositoryClient()
      const = 0;
  virtual TextCheckerClient& GetTextCheckerClient() const = 0;
  virtual TextFinder* GetTextFinder() const = 0;
  virtual void SetInputEventsScaleForEmulation(float) = 0;

  virtual WebFrameWidgetBase* LocalRootFrameWidget() = 0;

  DEFINE_INLINE_VIRTUAL_TRACE() {}

 protected:
  explicit WebLocalFrameBase(WebTreeScopeType scope) : WebLocalFrame(scope) {}
};

DEFINE_TYPE_CASTS(WebLocalFrameBase,
                  WebFrame,
                  frame,
                  frame->IsWebLocalFrame(),
                  frame.IsWebLocalFrame());

}  // namespace blink

#endif  // WebLocalFrameBase_h
