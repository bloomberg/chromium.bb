/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
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

#include "modules/webaudio/DelayNode.h"
#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "modules/webaudio/AudioBasicProcessorHandler.h"
#include "modules/webaudio/DelayOptions.h"
#include "modules/webaudio/DelayProcessor.h"
#include "platform/wtf/MathExtras.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

const double kMaximumAllowedDelayTime = 180;

DelayNode::DelayNode(BaseAudioContext& context, double max_delay_time)
    : AudioNode(context),
      delay_time_(AudioParam::Create(context,
                                     kParamTypeDelayDelayTime,
                                     0.0,
                                     0.0,
                                     max_delay_time)) {
  SetHandler(AudioBasicProcessorHandler::Create(
      AudioHandler::kNodeTypeDelay, *this, context.sampleRate(),
      WTF::WrapUnique(new DelayProcessor(
          context.sampleRate(), 1, delay_time_->Handler(), max_delay_time))));

  // Initialize the handler so that AudioParams can be processed.
  Handler().Initialize();
}

DelayNode* DelayNode::Create(BaseAudioContext& context,
                             ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  // The default maximum delay time for the delay node is 1 sec.
  return Create(context, 1, exception_state);
}

DelayNode* DelayNode::Create(BaseAudioContext& context,
                             double max_delay_time,
                             ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  if (context.IsContextClosed()) {
    context.ThrowExceptionForClosedState(exception_state);
    return nullptr;
  }

  if (max_delay_time <= 0 || max_delay_time >= kMaximumAllowedDelayTime) {
    exception_state.ThrowDOMException(
        kNotSupportedError,
        ExceptionMessages::IndexOutsideRange(
            "max delay time", max_delay_time, 0.0,
            ExceptionMessages::kExclusiveBound, kMaximumAllowedDelayTime,
            ExceptionMessages::kExclusiveBound));
    return nullptr;
  }

  return new DelayNode(context, max_delay_time);
}

DelayNode* DelayNode::Create(BaseAudioContext* context,
                             const DelayOptions& options,
                             ExceptionState& exception_state) {
  // maxDelayTime has a default value specified.
  DelayNode* node = Create(*context, options.maxDelayTime(), exception_state);

  if (!node)
    return nullptr;

  node->HandleChannelOptions(options, exception_state);

  node->delayTime()->setInitialValue(options.delayTime());

  return node;
}

AudioParam* DelayNode::delayTime() {
  return delay_time_;
}

DEFINE_TRACE(DelayNode) {
  visitor->Trace(delay_time_);
  AudioNode::Trace(visitor);
}

}  // namespace blink
