// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webaudio/IIRFilterNode.h"

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "core/inspector/ConsoleMessage.h"
#include "modules/webaudio/AudioBasicProcessorHandler.h"
#include "modules/webaudio/BaseAudioContext.h"
#include "modules/webaudio/IIRFilterOptions.h"
#include "platform/Histogram.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

// Determine if filter is stable based on the feedback coefficients.
// We compute the reflection coefficients for the filter.  If, at any
// point, the magnitude of the reflection coefficient is greater than
// or equal to 1, the filter is declared unstable.
//
// Let A(z) be the feedback polynomial given by
//   A[n](z) = 1 + a[1]/z + a[2]/z^2 + ... + a[n]/z^n
//
// The first reflection coefficient k[n] = a[n].  Then, recursively compute
//
//   A[n-1](z) = (A[n](z) - k[n]*A[n](1/z)/z^n)/(1-k[n]^2);
//
// stopping at A[1](z).  If at any point |k[n]| >= 1, the filter is
// unstable.
static bool IsFilterStable(const Vector<double>& feedback_coef) {
  // Make a copy of the feedback coefficients
  Vector<double> coef(feedback_coef);
  int order = coef.size() - 1;

  // If necessary, normalize filter coefficients so that constant term is 1.
  if (coef[0] != 1) {
    for (int m = 1; m <= order; ++m)
      coef[m] /= coef[0];
    coef[0] = 1;
  }

  // Begin recursion, using a work array to hold intermediate results.
  Vector<double> work(order + 1);
  for (int n = order; n >= 1; --n) {
    double k = coef[n];

    if (std::fabs(k) >= 1)
      return false;

    // Note that A[n](1/z)/z^n is basically the coefficients of A[n]
    // in reverse order.
    double factor = 1 - k * k;
    for (int m = 0; m <= n; ++m)
      work[m] = (coef[m] - k * coef[n - m]) / factor;
    coef.swap(work);
  }

  return true;
}

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

  if (!IsFilterStable(feedback_coef)) {
    StringBuilder message;
    message.Append("Unstable IIRFilter with feedback coefficients: [");
    message.AppendNumber(feedback_coef[0]);
    for (size_t k = 1; k < feedback_coef.size(); ++k) {
      message.Append(", ");
      message.AppendNumber(feedback_coef[k]);
    }
    message.Append(']');

    context.GetExecutionContext()->AddConsoleMessage(ConsoleMessage::Create(
        kJSMessageSource, kWarningMessageLevel, message.ToString()));
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

void IIRFilterNode::Trace(blink::Visitor* visitor) {
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
