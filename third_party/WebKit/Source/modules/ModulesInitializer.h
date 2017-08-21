// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ModulesInitializer_h
#define ModulesInitializer_h

#include "core/CoreInitializer.h"
#include "modules/ModulesExport.h"

namespace blink {

class MODULES_EXPORT ModulesInitializer : public CoreInitializer {
 public:
  void Initialize() override;

 private:
  void InitLocalFrame(LocalFrame&) const override;
  void InstallSupplements(LocalFrame&) const override;
  void ProvideLocalFileSystemToWorker(WorkerClients&) const override;
  void ProvideIndexedDBClientToWorker(WorkerClients&) const override;
  MediaControls* CreateMediaControls(HTMLMediaElement&,
                                     ShadowRoot&) const override;
  void InitInspectorAgentSession(InspectorSession*,
                                 bool,
                                 InspectorDOMAgent*,
                                 InspectedFrames*,
                                 Page*) const override;
  LinkResource* CreateServiceWorkerLinkResource(
      HTMLLinkElement*) const override;
  void OnClearWindowObjectInMainWorld(Document&,
                                      const Settings&) const override;
  std::unique_ptr<WebMediaPlayer> CreateWebMediaPlayer(
      WebFrameClient*,
      HTMLMediaElement&,
      const WebMediaPlayerSource&,
      WebMediaPlayerClient*) const override;
  WebRemotePlaybackClient* CreateWebRemotePlaybackClient(
      HTMLMediaElement&) const override;

  void ProvideCredentialManagerClient(Page&, WebCredentialManagerClient*)
      const override;
  void ProvideModulesToPage(Page&, WebViewClient*) const override;
  void ForceNextWebGLContextCreationToFail() const override;

  void CollectAllGarbageForAnimationWorklet() const override;
};

}  // namespace blink

#endif  // ModulesInitializer_h
