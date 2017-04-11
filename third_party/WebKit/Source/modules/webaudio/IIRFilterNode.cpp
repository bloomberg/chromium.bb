// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webaudio/IIRFilterNode.h"

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "modules/webaudio/AudioBasicProcessorHandler.h"
#include "modules/webaudio/BaseAudioContext.h"
#include "modules/webaudio/IIRFilterOptions.h"
#include "platform/Histogram.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

IIRFilterNode::IIRFilterNode(BaseAudioContext& context,
                             const Vector<double>& feedforward_coef,
                             const Vector<double>& feedback_coef)
    : AudioNode(context) {
  SetHandler(AudioBasicProcessorHandler::Create(
      AudioHandler::kNodeTypeIIRFilter, *this, context.sampleRate(),
      WTF::WrapUnique(new IIRProcessor(context.sampleRate(), 1,
                                       feedforward_coef, feedback_coef))));

  // Histogram of the IIRFilter order.  createIIRFilter ensures that the length
  // of |feedbackCoef| is in the range [1, IIRFilter::kMaxOrder + 1].  The order
  // is one less than the length of this vector.
  DEFINE_STATIC_LOCAL(SparseHistogram, filter_order_histogram,
                      ("WebAudio.IIRFilterNode.Order"));

  filter_order_histogram.Sample(feedback_coef.size() - 1);
}

IIRFilterNode* IIRFilterNode::Create(BaseAudioContext& context,
                                     const Vector<double>& feedforward_coef,
                                     const Vector<double>& feedback_coef,
                                     ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  if (context.IsContextClosed()) {
    context.ThrowExceptionForClosedState(exception_state);
    return nullptr;
  }

  if (feedback_coef.size() == 0 ||
      (feedback_coef.size() > IIRFilter::kMaxOrder + 1)) {
    exception_state.ThrowDOMException(
        kNotSupportedError,
        ExceptionMessages::IndexOutsideRange<size_t>(
            "number of feedback coefficients", feedback_coef.size(), 1,
            ExceptionMessages::kInclusiveBound, IIRFilter::kMaxOrder + 1,
            ExceptionMessages::kInclusiveBound));
    return nullptr;
  }

  if (feedforward_coef.size() == 0 ||
      (feedforward_coef.size() > IIRFilter::kMaxOrder + 1)) {
    exception_state.ThrowDOMException(
        kNotSupportedError,
        ExceptionMessages::IndexOutsideRange<size_t>(
            "number of feedforward coefficients", feedforward_coef.size(), 1,
            ExceptionMessages::kInclusiveBound, IIRFilter::kMaxOrder + 1,
            ExceptionMessages::kInclusiveBound));
    return nullptr;
  }

  if (feedback_coef[0] == 0) {
    exception_state.ThrowDOMException(
        kInvalidStateError, "First feedback coefficient cannot be zero.");
    return nullptr;
  }

  bool has_non_zero_coef = false;

  for (size_t k = 0; k < feedforward_coef.size(); ++k) {
    if (feedforward_coef[k] != 0) {
      has_non_zero_coef = true;
      break;
    }
  }

  if (!has_non_zero_coef) {
    exception_state.ThrowDOMException(
        kInvalidStateError,
        "At least one feedforward coefficient must be non-zero.");
    return nullptr;
  }

  return new IIRFilterNode(context, feedforward_coef, feedback_coef);
}

IIRFilterNode* IIRFilterNode::Create(BaseAudioContext* context,
                                     const IIRFilterOptions& options,
                                     ExceptionState& exception_state) {
  if (!options.hasFeedforward()) {
    exception_state.ThrowDOMException(
        kNotFoundError, "IIRFilterOptions: feedforward is required.");
    return nullptr;
  }

  if (!options.hasFeedback()) {
    exception_state.ThrowDOMException(
        kNotFoundError, "IIRFilterOptions: feedback is required.");
    return nullptr;
  }

  IIRFilterNode* node = Create(*context, options.feedforward(),
                               options.feedback(), exception_state);

  if (!node)
    return nullptr;

  node->HandleChannelOptions(options, exception_state);

  return node;
}

DEFINE_TRACE(IIRFilterNode) {
  AudioNode::Trace(visitor);
}

IIRProcessor* IIRFilterNode::IirProcessor() const {
  return static_cast<IIRProcessor*>(
      static_cast<AudioBasicProcessorHandler&>(Handler()).Processor());
}

void IIRFilterNode::getFrequencyResponse(
    NotShared<const DOMFloat32Array> frequency_hz,
    NotShared<DOMFloat32Array> mag_response,
    NotShared<DOMFloat32Array> phase_response,
    ExceptionState& exception_state) {
  if (!frequency_hz.View()) {
    exception_state.ThrowDOMException(kNotSupportedError,
                                      "frequencyHz array cannot be null");
    return;
  }

  if (!mag_response.View()) {
    exception_state.ThrowDOMException(kNotSupportedError,
                                      "magResponse array cannot be null");
    return;
  }

  if (!phase_response.View()) {
    exception_state.ThrowDOMException(kNotSupportedError,
                                      "phaseResponse array cannot be null");
    return;
  }

  unsigned frequency_hz_length = frequency_hz.View()->length();

  if (mag_response.View()->length() < frequency_hz_length) {
    exception_state.ThrowDOMException(
        kNotSupportedError,
        ExceptionMessages::IndexExceedsMinimumBound(
            "magResponse length", mag_response.View()->length(),
            frequency_hz_length));
    return;
  }

  if (phase_response.View()->length() < frequency_hz_length) {
    exception_state.ThrowDOMException(
        kNotSupportedError,
        ExceptionMessages::IndexExceedsMinimumBound(
            "phaseResponse length", phase_response.View()->length(),
            frequency_hz_length));
    return;
  }

  IirProcessor()->GetFrequencyResponse(
      frequency_hz_length, frequency_hz.View()->Data(),
      mag_response.View()->Data(), phase_response.View()->Data());
}

}  // namespace blink
