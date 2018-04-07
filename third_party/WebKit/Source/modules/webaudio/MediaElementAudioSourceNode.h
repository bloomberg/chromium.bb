/*
 * Copyright (C) 2011, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_MEDIA_ELEMENT_AUDIO_SOURCE_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_MEDIA_ELEMENT_AUDIO_SOURCE_NODE_H_

#include <memory>
#include "base/memory/scoped_refptr.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_annotations.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node.h"
#include "third_party/blink/renderer/platform/audio/audio_source_provider_client.h"
#include "third_party/blink/renderer/platform/audio/multi_channel_resampler.h"
#include "third_party/blink/renderer/platform/wtf/threading_primitives.h"

namespace blink {

class BaseAudioContext;
class HTMLMediaElement;
class MediaElementAudioSourceOptions;

class MediaElementAudioSourceHandler final : public AudioHandler {
 public:
  static scoped_refptr<MediaElementAudioSourceHandler> Create(
      AudioNode&,
      HTMLMediaElement&);
  ~MediaElementAudioSourceHandler() override;

  HTMLMediaElement* MediaElement() const;

  // AudioHandler
  void Dispose() override;
  void Process(size_t frames_to_process) override;

  // AudioNode
  double TailTime() const override { return 0; }
  double LatencyTime() const override { return 0; }

  // Helpers for AudioSourceProviderClient implementation of
  // MediaElementAudioSourceNode.
  void SetFormat(size_t number_of_channels, float sample_rate);
  void OnCurrentSrcChanged(const KURL& current_src);
  void lock() EXCLUSIVE_LOCK_FUNCTION(GetProcessLock());
  void unlock() UNLOCK_FUNCTION(GetProcessLock());

  // For thread safety analysis only.  Does not actually return mu.
  Mutex* GetProcessLock() LOCK_RETURNED(process_lock_) {
    NOTREACHED();
    return nullptr;
  }

  bool RequiresTailProcessing() const final { return false; }

 private:
  MediaElementAudioSourceHandler(AudioNode&, HTMLMediaElement&);
  // As an audio source, we will never propagate silence.
  bool PropagatesSilence() const override { return false; }

  // Must be called only on the audio thread.
  bool PassesCORSAccessCheck();

  // Must be called only on the main thread.
  bool PassesCurrentSrcCORSAccessCheck(const KURL& current_src);

  // Print warning if CORS restrictions cause MediaElementAudioSource to output
  // zeroes.
  void PrintCORSMessage(const String& message);

  // This Persistent doesn't make a reference cycle. The reference from
  // HTMLMediaElement to AudioSourceProvideClient, which
  // MediaElementAudioSourceNode implements, is weak.
  //
  // It is accessed by both audio and main thread. TODO: we really should
  // try to minimize or avoid the audio thread touching this element.
  CrossThreadPersistent<HTMLMediaElement> media_element_;
  Mutex process_lock_;

  unsigned source_number_of_channels_;
  double source_sample_rate_;

  std::unique_ptr<MultiChannelResampler> multi_channel_resampler_;

  // |m_passesCurrentSrcCORSAccessCheck| holds the value of
  // context()->getSecurityOrigin() &&
  // context()->getSecurityOrigin()->canRequest(mediaElement()->currentSrc()),
  // updated in the ctor and onCurrentSrcChanged() on the main thread and used
  // in passesCORSAccessCheck() on the audio thread, protected by
  // |m_processLock|.
  bool passes_current_src_cors_access_check_;

  // Indicates if we need to print a CORS message if the current source has
  // changed and we have no access to it. Must be protected by |m_processLock|.
  bool maybe_print_cors_message_;

  // The value of mediaElement()->currentSrc().string() in the ctor and
  // onCurrentSrcChanged().  Protected by |m_processLock|.
  String current_src_string_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

class MediaElementAudioSourceNode final : public AudioNode,
                                          public AudioSourceProviderClient {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(MediaElementAudioSourceNode);

 public:
  static MediaElementAudioSourceNode* Create(BaseAudioContext&,
                                             HTMLMediaElement&,
                                             ExceptionState&);
  static MediaElementAudioSourceNode* Create(
      BaseAudioContext*,
      const MediaElementAudioSourceOptions&,
      ExceptionState&);

  virtual void Trace(blink::Visitor*);
  MediaElementAudioSourceHandler& GetMediaElementAudioSourceHandler() const;

  HTMLMediaElement* mediaElement() const;

  // AudioSourceProviderClient functions:
  void SetFormat(size_t number_of_channels, float sample_rate) override;
  void OnCurrentSrcChanged(const KURL& current_src) override;
  void lock() override EXCLUSIVE_LOCK_FUNCTION(
      GetMediaElementAudioSourceHandler().GetProcessLock());
  void unlock() override
      UNLOCK_FUNCTION(GetMediaElementAudioSourceHandler().GetProcessLock());

 private:
  MediaElementAudioSourceNode(BaseAudioContext&, HTMLMediaElement&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_MEDIA_ELEMENT_AUDIO_SOURCE_NODE_H_
