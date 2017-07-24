/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CoreInitializer_h
#define CoreInitializer_h

#include "core/CoreExport.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class HTMLLinkElement;
class HTMLMediaElement;
class InspectedFrames;
class InspectorDOMAgent;
class InspectorSession;
class LinkResource;
class LocalFrame;
class MediaControls;
class Page;
class ShadowRoot;
class WorkerClients;

class CORE_EXPORT CoreInitializer {
  USING_FAST_MALLOC(CoreInitializer);
  WTF_MAKE_NONCOPYABLE(CoreInitializer);

 public:
  // Initialize must be called before GetInstance.
  static CoreInitializer& GetInstance() {
    DCHECK(instance_);
    return *instance_;
  }

  virtual ~CoreInitializer() {}

  // Should be called by clients before trying to create Frames.
  virtual void Initialize();

  // Methods defined in CoreInitializer and implemented by ModulesInitializer to
  // bypass the inverted dependency from core/ to modules/.
  // Mojo Interfaces registered with LocalFrame
  virtual void InitLocalFrame(LocalFrame&) const = 0;
  // Supplements installed on a frame using ChromeClient
  virtual void InstallSupplements(LocalFrame&) const = 0;
  virtual void ProvideLocalFileSystemToWorker(WorkerClients&) const = 0;
  virtual void ProvideIndexedDBClientToWorker(WorkerClients&) const = 0;
  virtual MediaControls* CreateMediaControls(HTMLMediaElement&,
                                             ShadowRoot&) const = 0;
  // Session Initializers for Inspector Agents in modules/
  // These methods typically create agents and append them to a session.
  // TODO(nverne): remove this and restore to WebDevToolsAgentImpl once that
  // class is a controller/ crbug:731490
  virtual void InitInspectorAgentSession(InspectorSession*,
                                         bool,
                                         InspectorDOMAgent*,
                                         InspectedFrames*,
                                         Page*) const = 0;
  virtual LinkResource* CreateServiceWorkerLinkResource(
      HTMLLinkElement*) const = 0;

 protected:
  // CoreInitializer is only instantiated by subclass ModulesInitializer.
  CoreInitializer() {}

 private:
  static CoreInitializer* instance_;
  void RegisterEventFactory();
};

}  // namespace blink

#endif  // CoreInitializer_h
