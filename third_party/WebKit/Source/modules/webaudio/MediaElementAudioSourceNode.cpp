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

#include "modules/webaudio/MediaElementAudioSourceNode.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/html/media/HTMLMediaElement.h"
#include "core/inspector/ConsoleMessage.h"
#include "modules/webaudio/AudioNodeOutput.h"
#include "modules/webaudio/BaseAudioContext.h"
#include "modules/webaudio/MediaElementAudioSourceOptions.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/audio/AudioUtilities.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/Locker.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

MediaElementAudioSourceHandler::MediaElementAudioSourceHandler(
    AudioNode& node,
    HTMLMediaElement& media_element)
    : AudioHandler(kNodeTypeMediaElementAudioSource,
                   node,
                   node.context()->sampleRate()),
      media_element_(media_element),
      source_number_of_channels_(0),
      source_sample_rate_(0),
      passes_current_src_cors_access_check_(
          PassesCurrentSrcCORSAccessCheck(media_element.currentSrc())),
      maybe_print_cors_message_(!passes_current_src_cors_access_check_),
      current_src_string_(media_element.currentSrc().GetString()) {
  DCHECK(IsMainThread());
  // Default to stereo. This could change depending on what the media element
  // .src is set to.
  AddOutput(2);

  Initialize();
}

RefPtr<MediaElementAudioSourceHandler> MediaElementAudioSourceHandler::Create(
    AudioNode& node,
    HTMLMediaElement& media_element) {
  return WTF::AdoptRef(new MediaElementAudioSourceHandler(node, media_element));
}

MediaElementAudioSourceHandler::~MediaElementAudioSourceHandler() {
  Uninitialize();
}

HTMLMediaElement* MediaElementAudioSourceHandler::MediaElement() const {
  return media_element_.Get();
}

void MediaElementAudioSourceHandler::Dispose() {
  media_element_->SetAudioSourceNode(nullptr);
  AudioHandler::Dispose();
}

void MediaElementAudioSourceHandler::SetFormat(size_t number_of_channels,
                                               float source_sample_rate) {
  if (number_of_channels != source_number_of_channels_ ||
      source_sample_rate != source_sample_rate_) {
    if (!number_of_channels ||
        number_of_channels > BaseAudioContext::MaxNumberOfChannels() ||
        !AudioUtilities::IsValidAudioBufferSampleRate(source_sample_rate)) {
      // process() will generate silence for these uninitialized values.
      DLOG(ERROR) << "setFormat(" << number_of_channels << ", "
                  << source_sample_rate << ") - unhandled format change";
      // Synchronize with process().
      Locker<MediaElementAudioSourceHandler> locker(*this);
      source_number_of_channels_ = 0;
      source_sample_rate_ = 0;
      return;
    }

    // Synchronize with process() to protect m_sourceNumberOfChannels,
    // m_sourceSampleRate, and m_multiChannelResampler.
    Locker<MediaElementAudioSourceHandler> locker(*this);

    source_number_of_channels_ = number_of_channels;
    source_sample_rate_ = source_sample_rate;

    if (source_sample_rate != Context()->sampleRate()) {
      double scale_factor = source_sample_rate / Context()->sampleRate();
      multi_channel_resampler_ = WTF::MakeUnique<MultiChannelResampler>(
          scale_factor, number_of_channels);
    } else {
      // Bypass resampling.
      multi_channel_resampler_.reset();
    }

    {
      // The context must be locked when changing the number of output channels.
      BaseAudioContext::GraphAutoLocker context_locker(Context());

      // Do any necesssary re-configuration to the output's number of channels.
      Output(0).SetNumberOfChannels(number_of_channels);
    }
  }
}

bool MediaElementAudioSourceHandler::PassesCORSAccessCheck() {
  DCHECK(MediaElement());

  return (MediaElement()->GetWebMediaPlayer() &&
          MediaElement()->GetWebMediaPlayer()->DidPassCORSAccessCheck()) ||
         passes_current_src_cors_access_check_;
}

void MediaElementAudioSourceHandler::OnCurrentSrcChanged(
    const KURL& current_src) {
  DCHECK(IsMainThread());

  // Synchronize with process().
  Locker<MediaElementAudioSourceHandler> locker(*this);

  passes_current_src_cors_access_check_ =
      PassesCurrentSrcCORSAccessCheck(current_src);

  // Make a note if we need to print a console message and save the |curentSrc|
  // for use in the message.  Need to wait until later to print the message in
  // case HTMLMediaElement allows access.
  maybe_print_cors_message_ = !passes_current_src_cors_access_check_;
  current_src_string_ = current_src.GetString();
}

bool MediaElementAudioSourceHandler::PassesCurrentSrcCORSAccessCheck(
    const KURL& current_src) {
  DCHECK(IsMainThread());
  return Context()->GetSecurityOrigin() &&
         Context()->GetSecurityOrigin()->CanRequest(current_src);
}

void MediaElementAudioSourceHandler::PrintCORSMessage(const String& message) {
  if (Context()->GetExecutionContext()) {
    Context()->GetExecutionContext()->AddConsoleMessage(
        ConsoleMessage::Create(kSecurityMessageSource, kInfoMessageLevel,
                               "MediaElementAudioSource outputs zeroes due to "
                               "CORS access restrictions for " +
                                   message));
  }
}

void MediaElementAudioSourceHandler::Process(size_t number_of_frames) {
  AudioBus* output_bus = Output(0).Bus();

  // Use a tryLock() to avoid contention in the real-time audio thread.
  // If we fail to acquire the lock then the HTMLMediaElement must be in the
  // middle of reconfiguring its playback engine, so we output silence in this
  // case.
  MutexTryLocker try_locker(process_lock_);
  if (try_locker.Locked()) {
    if (!MediaElement() || !source_number_of_channels_ ||
        !source_sample_rate_) {
      output_bus->Zero();
      return;
    }
    AudioSourceProvider& provider = MediaElement()->GetAudioSourceProvider();
    // Grab data from the provider so that the element continues to make
    // progress, even if we're going to output silence anyway.
    if (multi_channel_resampler_.get()) {
      DCHECK_NE(source_sample_rate_, Context()->sampleRate());
      multi_channel_resampler_->Process(&provider, output_bus,
                                        number_of_frames);
    } else {
      // Bypass the resampler completely if the source is at the context's
      // sample-rate.
      DCHECK_EQ(source_sample_rate_, Context()->sampleRate());
      provider.ProvideInput(output_bus, number_of_frames);
    }
    // Output silence if we don't have access to the element.
    if (!PassesCORSAccessCheck()) {
      if (maybe_print_cors_message_) {
        // Print a CORS message, but just once for each change in the current
        // media element source, and only if we have a document to print to.
        maybe_print_cors_message_ = false;
        if (Context()->GetExecutionContext()) {
          TaskRunnerHelper::Get(TaskType::kMediaElementEvent,
                                Context()->GetExecutionContext())
              ->PostTask(BLINK_FROM_HERE,
                         CrossThreadBind(
                             &MediaElementAudioSourceHandler::PrintCORSMessage,
                             WrapRefPtr(this), current_src_string_));
        }
      }
      output_bus->Zero();
    }
  } else {
    // We failed to acquire the lock.
    output_bus->Zero();
  }
}

void MediaElementAudioSourceHandler::lock() {
  process_lock_.lock();
}

void MediaElementAudioSourceHandler::unlock() {
  process_lock_.unlock();
}

// ----------------------------------------------------------------

MediaElementAudioSourceNode::MediaElementAudioSourceNode(
    BaseAudioContext& context,
    HTMLMediaElement& media_element)
    : AudioNode(context) {
  SetHandler(MediaElementAudioSourceHandler::Create(*this, media_element));
}

MediaElementAudioSourceNode* MediaElementAudioSourceNode::Create(
    BaseAudioContext& context,
    HTMLMediaElement& media_element,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  if (context.IsContextClosed()) {
    context.ThrowExceptionForClosedState(exception_state);
    return nullptr;
  }

  // First check if this media element already has a source node.
  if (media_element.AudioSourceNode()) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      "HTMLMediaElement already connected "
                                      "previously to a different "
                                      "MediaElementSourceNode.");
    return nullptr;
  }

  MediaElementAudioSourceNode* node =
      new MediaElementAudioSourceNode(context, media_element);

  if (node) {
    media_element.SetAudioSourceNode(node);
    // context keeps reference until node is disconnected
    context.NotifySourceNodeStartedProcessing(node);
  }

  return node;
}

MediaElementAudioSourceNode* MediaElementAudioSourceNode::Create(
    BaseAudioContext* context,
    const MediaElementAudioSourceOptions& options,
    ExceptionState& exception_state) {
  if (!options.hasMediaElement()) {
    exception_state.ThrowDOMException(kNotFoundError,
                                      "mediaElement member is required.");
    return nullptr;
  }

  return Create(*context, *options.mediaElement(), exception_state);
}

void MediaElementAudioSourceNode::Trace(blink::Visitor* visitor) {
  AudioSourceProviderClient::Trace(visitor);
  AudioNode::Trace(visitor);
}

MediaElementAudioSourceHandler&
MediaElementAudioSourceNode::GetMediaElementAudioSourceHandler() const {
  return static_cast<MediaElementAudioSourceHandler&>(Handler());
}

HTMLMediaElement* MediaElementAudioSourceNode::mediaElement() const {
  return GetMediaElementAudioSourceHandler().MediaElement();
}

void MediaElementAudioSourceNode::SetFormat(size_t number_of_channels,
                                            float sample_rate) {
  GetMediaElementAudioSourceHandler().SetFormat(number_of_channels,
                                                sample_rate);
}

void MediaElementAudioSourceNode::OnCurrentSrcChanged(const KURL& current_src) {
  GetMediaElementAudioSourceHandler().OnCurrentSrcChanged(current_src);
}

void MediaElementAudioSourceNode::lock() {
  GetMediaElementAudioSourceHandler().lock();
}

void MediaElementAudioSourceNode::unlock() {
  GetMediaElementAudioSourceHandler().unlock();
}

}  // namespace blink
