// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IIRFilterNode_h
#define IIRFilterNode_h

#include "core/typed_arrays/ArrayBufferViewHelpers.h"
#include "core/typed_arrays/DOMTypedArray.h"
#include "modules/webaudio/AudioBasicProcessorHandler.h"
#include "modules/webaudio/AudioNode.h"
#include "modules/webaudio/IIRProcessor.h"

namespace blink {

class BaseAudioContext;
class ExceptionState;
class IIRFilterOptions;

class IIRFilterHandler : public AudioBasicProcessorHandler {
 public:
  static scoped_refptr<IIRFilterHandler> Create(
      AudioNode&,
      float sample_rate,
      const Vector<double>& feedforward_coef,
      const Vector<double>& feedback_coef);

 private:
  IIRFilterHandler(AudioNode&,
                   float sample_rate,
                   const Vector<double>& feedforward_coef,
                   const Vector<double>& feedback_coef);
};

class IIRFilterNode : public AudioNode {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static IIRFilterNode* Create(BaseAudioContext&,
                               const Vector<double>& feedforward,
                               const Vector<double>& feedback,
                               ExceptionState&);

  static IIRFilterNode* Create(BaseAudioContext*,
                               const IIRFilterOptions&,
                               ExceptionState&);

  virtual void Trace(blink::Visitor*);

  // Get the magnitude and phase response of the filter at the given
  // set of frequencies (in Hz). The phase response is in radians.
  void getFrequencyResponse(NotShared<const DOMFloat32Array> frequency_hz,
                            NotShared<DOMFloat32Array> mag_response,
                            NotShared<DOMFloat32Array> phase_response,
                            ExceptionState&);

 private:
  IIRFilterNode(BaseAudioContext&,
                const Vector<double>& denominator,
                const Vector<double>& numerator);

  IIRProcessor* GetIIRFilterProcessor() const;
};

}  // namespace blink

#endif  // IIRFilterNode_h
