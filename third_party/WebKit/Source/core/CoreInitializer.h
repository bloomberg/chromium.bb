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

class HTMLMediaElement;
class InspectedFrames;
class InspectorDOMAgent;
class InspectorSession;
class LocalFrame;
class MediaControls;
class Page;
class ShadowRoot;
class WorkerClients;

class CORE_EXPORT CoreInitializer {
  USING_FAST_MALLOC(CoreInitializer);
  WTF_MAKE_NONCOPYABLE(CoreInitializer);

 public:
  virtual ~CoreInitializer() {}

  // Should be called by clients before trying to create Frames.
  virtual void Initialize();

  // Callbacks stored in CoreInitializer and set by ModulesInitializer to bypass
  // the inverted dependency from core/ to modules/.
  using LocalFrameCallback = void (*)(LocalFrame&);
  static void CallModulesLocalFrameInit(LocalFrame& frame) {
    instance_->local_frame_init_callback_(frame);
  }
  static void CallModulesInstallSupplements(LocalFrame& frame) {
    instance_->chrome_client_install_supplements_callback_(frame);
  }
  using WorkerClientsCallback = void (*)(WorkerClients&);
  static void CallModulesProvideLocalFileSystem(WorkerClients& worker_clients) {
    instance_->worker_clients_local_file_system_callback_(worker_clients);
  }
  static void CallModulesProvideIndexedDB(WorkerClients& worker_clients) {
    instance_->worker_clients_indexed_db_callback_(worker_clients);
  }
  using MediaControlsFactory = MediaControls* (*)(HTMLMediaElement&,
                                                  ShadowRoot&);
  static MediaControls* CallModulesMediaControlsFactory(
      HTMLMediaElement& media_element,
      ShadowRoot& shadow_root) {
    return instance_->media_controls_factory_(media_element, shadow_root);
  }
  using InspectorAgentSessionInitCallback = void (*)(InspectorSession*,
                                                     bool,
                                                     InspectorDOMAgent*,
                                                     InspectedFrames*,
                                                     Page*);
  static void CallModulesInspectorAgentSessionInitCallback(
      InspectorSession* session,
      bool allow_view_agents,
      InspectorDOMAgent* dom_agent,
      InspectedFrames* inspected_frames,
      Page* page) {
    instance_->inspector_agent_session_init_callback_(
        session, allow_view_agents, dom_agent, inspected_frames, page);
  }

 protected:
  // CoreInitializer is only instantiated by subclass ModulesInitializer.
  CoreInitializer() {}
  LocalFrameCallback local_frame_init_callback_;
  LocalFrameCallback chrome_client_install_supplements_callback_;
  WorkerClientsCallback worker_clients_local_file_system_callback_;
  WorkerClientsCallback worker_clients_indexed_db_callback_;
  MediaControlsFactory media_controls_factory_;
  InspectorAgentSessionInitCallback inspector_agent_session_init_callback_;

 private:
  static CoreInitializer* instance_;
  void RegisterEventFactory();
};

}  // namespace blink

#endif  // CoreInitializer_h
