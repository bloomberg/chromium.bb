/*
 * Copyright (C) 2012, Google Inc. All rights reserved.
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

#include "modules/webaudio/AudioScheduledSourceNode.h"

#include <algorithm>
#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "modules/EventModules.h"
#include "modules/webaudio/BaseAudioContext.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/audio/AudioUtilities.h"
#include "platform/wtf/MathExtras.h"
#include "public/platform/TaskType.h"

namespace blink {

const double AudioScheduledSourceHandler::kUnknownTime = -1;

AudioScheduledSourceHandler::AudioScheduledSourceHandler(NodeType node_type,
                                                         AudioNode& node,
                                                         float sample_rate)
    : AudioHandler(node_type, node, sample_rate),
      start_time_(0),
      end_time_(kUnknownTime),
      playback_state_(UNSCHEDULED_STATE) {
  if (Context()->GetExecutionContext()) {
    task_runner_ = Context()->GetExecutionContext()->GetTaskRunner(
        TaskType::kMediaElementEvent);
  }
}

void AudioScheduledSourceHandler::UpdateSchedulingInfo(
    size_t quantum_frame_size,
    AudioBus* output_bus,
    size_t& quantum_frame_offset,
    size_t& non_silent_frames_to_process,
    double& start_frame_offset) {
  DCHECK(output_bus);
  if (!output_bus)
    return;

  DCHECK_EQ(quantum_frame_size,
            static_cast<size_t>(AudioUtilities::kRenderQuantumFrames));
  if (quantum_frame_size != AudioUtilities::kRenderQuantumFrames)
    return;

  double sample_rate = Context()->sampleRate();

  // quantumStartFrame     : Start frame of the current time quantum.
  // quantumEndFrame       : End frame of the current time quantum.
  // startFrame            : Start frame for this source.
  // endFrame              : End frame for this source.
  size_t quantum_start_frame = Context()->CurrentSampleFrame();
  size_t quantum_end_frame = quantum_start_frame + quantum_frame_size;
  size_t start_frame =
      AudioUtilities::TimeToSampleFrame(start_time_, sample_rate);
  size_t end_frame =
      end_time_ == kUnknownTime
          ? 0
          : AudioUtilities::TimeToSampleFrame(end_time_, sample_rate);

  // If we know the end time and it's already passed, then don't bother doing
  // any more rendering this cycle.
  if (end_time_ != kUnknownTime && end_frame <= quantum_start_frame)
    Finish();

  PlaybackState state = GetPlaybackState();

  if (state == UNSCHEDULED_STATE || state == FINISHED_STATE ||
      start_frame >= quantum_end_frame) {
    // Output silence.
    output_bus->Zero();
    non_silent_frames_to_process = 0;
    return;
  }

  // Check if it's time to start playing.
  if (state == SCHEDULED_STATE) {
    // Increment the active source count only if we're transitioning from
    // SCHEDULED_STATE to PLAYING_STATE.
    SetPlaybackState(PLAYING_STATE);
    // Determine the offset of the true start time from the starting frame.
    start_frame_offset = start_time_ * sample_rate - start_frame;
  } else {
    start_frame_offset = 0;
  }

  quantum_frame_offset =
      start_frame > quantum_start_frame ? start_frame - quantum_start_frame : 0;
  quantum_frame_offset = std::min(quantum_frame_offset,
                                  quantum_frame_size);  // clamp to valid range
  non_silent_frames_to_process = quantum_frame_size - quantum_frame_offset;

  if (!non_silent_frames_to_process) {
    // Output silence.
    output_bus->Zero();
    return;
  }

  // Handle silence before we start playing.
  // Zero any initial frames representing silence leading up to a rendering
  // start time in the middle of the quantum.
  if (quantum_frame_offset) {
    for (unsigned i = 0; i < output_bus->NumberOfChannels(); ++i)
      memset(output_bus->Channel(i)->MutableData(), 0,
             sizeof(float) * quantum_frame_offset);
  }

  // Handle silence after we're done playing.
  // If the end time is somewhere in the middle of this time quantum, then zero
  // out the frames from the end time to the very end of the quantum.
  if (end_time_ != kUnknownTime && end_frame >= quantum_start_frame &&
      end_frame < quantum_end_frame) {
    size_t zero_start_frame = end_frame - quantum_start_frame;
    size_t frames_to_zero = quantum_frame_size - zero_start_frame;

    bool is_safe = zero_start_frame < quantum_frame_size &&
                   frames_to_zero <= quantum_frame_size &&
                   zero_start_frame + frames_to_zero <= quantum_frame_size;
    DCHECK(is_safe);

    if (is_safe) {
      if (frames_to_zero > non_silent_frames_to_process)
        non_silent_frames_to_process = 0;
      else
        non_silent_frames_to_process -= frames_to_zero;

      for (unsigned i = 0; i < output_bus->NumberOfChannels(); ++i)
        memset(output_bus->Channel(i)->MutableData() + zero_start_frame, 0,
               sizeof(float) * frames_to_zero);
    }

    Finish();
  }

  return;
}

void AudioScheduledSourceHandler::Start(double when,
                                        ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  Context()->MaybeRecordStartAttempt();

  if (GetPlaybackState() != UNSCHEDULED_STATE) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      "cannot call start more than once.");
    return;
  }

  if (when < 0) {
    exception_state.ThrowRangeError(
        ExceptionMessages::IndexExceedsMinimumBound("start time", when, 0.0));
    return;
  }

  // The node is started. Add a reference to keep us alive so that audio will
  // eventually get played even if Javascript should drop all references to this
  // node. The reference will get dropped when the source has finished playing.
  Context()->NotifySourceNodeStartedProcessing(GetNode());

  // This synchronizes with process(). updateSchedulingInfo will read some of
  // the variables being set here.
  MutexLocker process_locker(process_lock_);

  // If |when| < currentTime, the source must start now according to the spec.
  // So just set startTime to currentTime in this case to start the source now.
  start_time_ = std::max(when, Context()->currentTime());

  SetPlaybackState(SCHEDULED_STATE);
}

void AudioScheduledSourceHandler::Stop(double when,
                                       ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  if (GetPlaybackState() == UNSCHEDULED_STATE) {
    exception_state.ThrowDOMException(
        kInvalidStateError, "cannot call stop without calling start first.");
    return;
  }

  if (when < 0) {
    exception_state.ThrowRangeError(
        ExceptionMessages::IndexExceedsMinimumBound("stop time", when, 0.0));
    return;
  }

  // This synchronizes with process()
  MutexLocker process_locker(process_lock_);

  // stop() can be called more than once, with the last call to stop taking
  // effect, unless the source has already stopped due to earlier calls to stop.
  // No exceptions are thrown in any case.
  when = std::max(0.0, when);
  end_time_ = when;
}

void AudioScheduledSourceHandler::FinishWithoutOnEnded() {
  if (GetPlaybackState() != FINISHED_STATE) {
    // Let the context dereference this AudioNode.
    Context()->NotifySourceNodeFinishedProcessing(this);
    SetPlaybackState(FINISHED_STATE);
  }
}

void AudioScheduledSourceHandler::Finish() {
  FinishWithoutOnEnded();

  task_runner_->PostTask(
      BLINK_FROM_HERE,
      CrossThreadBind(&AudioScheduledSourceHandler::NotifyEnded,
                      WrapRefCounted(this)));
}

void AudioScheduledSourceHandler::NotifyEnded() {
  DCHECK(IsMainThread());
  if (!Context() || !Context()->GetExecutionContext())
    return;
  if (GetNode())
    GetNode()->DispatchEvent(Event::Create(EventTypeNames::ended));
}

// ----------------------------------------------------------------

AudioScheduledSourceNode::AudioScheduledSourceNode(BaseAudioContext& context)
    : AudioNode(context) {}

AudioScheduledSourceHandler&
AudioScheduledSourceNode::GetAudioScheduledSourceHandler() const {
  return static_cast<AudioScheduledSourceHandler&>(Handler());
}

void AudioScheduledSourceNode::start(ExceptionState& exception_state) {
  start(0, exception_state);
}

void AudioScheduledSourceNode::start(double when,
                                     ExceptionState& exception_state) {
  GetAudioScheduledSourceHandler().Start(when, exception_state);
}

void AudioScheduledSourceNode::stop(ExceptionState& exception_state) {
  stop(0, exception_state);
}

void AudioScheduledSourceNode::stop(double when,
                                    ExceptionState& exception_state) {
  GetAudioScheduledSourceHandler().Stop(when, exception_state);
}

EventListener* AudioScheduledSourceNode::onended() {
  return GetAttributeEventListener(EventTypeNames::ended);
}

void AudioScheduledSourceNode::setOnended(EventListener* listener) {
  SetAttributeEventListener(EventTypeNames::ended, listener);
}

bool AudioScheduledSourceNode::HasPendingActivity() const {
  // To avoid the leak, a node should be collected regardless of its
  // playback state if the context is closed.
  if (context()->IsContextClosed())
    return false;

  // If a node is scheduled or playing, do not collect the node prematurely
  // even its reference is out of scope. Then fire onended event if assigned.
  return GetAudioScheduledSourceHandler().IsPlayingOrScheduled();
}

}  // namespace blink
