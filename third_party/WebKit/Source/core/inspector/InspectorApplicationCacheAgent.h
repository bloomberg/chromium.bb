/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#ifndef InspectorApplicationCacheAgent_h
#define InspectorApplicationCacheAgent_h

#include "core/CoreExport.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "core/inspector/protocol/ApplicationCache.h"
#include "core/loader/appcache/ApplicationCacheHost.h"
#include "platform/wtf/Noncopyable.h"

namespace blink {

class LocalFrame;
class InspectedFrames;

class CORE_EXPORT InspectorApplicationCacheAgent final
    : public InspectorBaseAgent<protocol::ApplicationCache::Metainfo> {
  WTF_MAKE_NONCOPYABLE(InspectorApplicationCacheAgent);

 public:
  static InspectorApplicationCacheAgent* Create(
      InspectedFrames* inspected_frames) {
    return new InspectorApplicationCacheAgent(inspected_frames);
  }
  ~InspectorApplicationCacheAgent() override {}
  virtual void Trace(blink::Visitor*);

  // InspectorBaseAgent
  void Restore() override;
  protocol::Response disable() override;

  // InspectorInstrumentation API
  void UpdateApplicationCacheStatus(LocalFrame*);
  void NetworkStateChanged(LocalFrame*, bool online);

  // ApplicationCache API for frontend
  protocol::Response getFramesWithManifests(
      std::unique_ptr<
          protocol::Array<protocol::ApplicationCache::FrameWithManifest>>*
          frame_ids) override;
  protocol::Response enable() override;
  protocol::Response getManifestForFrame(const String& frame_id,
                                         String* manifest_url) override;
  protocol::Response getApplicationCacheForFrame(
      const String& frame_id,
      std::unique_ptr<protocol::ApplicationCache::ApplicationCache>*) override;

 private:
  explicit InspectorApplicationCacheAgent(InspectedFrames*);

  std::unique_ptr<protocol::ApplicationCache::ApplicationCache>
  BuildObjectForApplicationCache(const ApplicationCacheHost::ResourceInfoList&,
                                 const ApplicationCacheHost::CacheInfo&);
  std::unique_ptr<
      protocol::Array<protocol::ApplicationCache::ApplicationCacheResource>>
  BuildArrayForApplicationCacheResources(
      const ApplicationCacheHost::ResourceInfoList&);
  std::unique_ptr<protocol::ApplicationCache::ApplicationCacheResource>
  BuildObjectForApplicationCacheResource(
      const ApplicationCacheHost::ResourceInfo&);

  protocol::Response AssertFrameWithDocumentLoader(String frame_id,
                                                   DocumentLoader*&);

  Member<InspectedFrames> inspected_frames_;
};

}  // namespace blink

#endif  // InspectorApplicationCacheAgent_h
