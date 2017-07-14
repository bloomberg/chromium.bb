/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef AudioBuffer_h
#define AudioBuffer_h

#include "core/typed_arrays/ArrayBufferViewHelpers.h"
#include "core/typed_arrays/DOMTypedArray.h"
#include "modules/ModulesExport.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/wtf/PassRefPtr.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/build_config.h"

namespace blink {

class AudioBus;
class AudioBufferOptions;
class ExceptionState;

class MODULES_EXPORT AudioBuffer final : public GarbageCollected<AudioBuffer>,
                                         public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static AudioBuffer* Create(unsigned number_of_channels,
                             size_t number_of_frames,
                             float sample_rate);
  static AudioBuffer* Create(unsigned number_of_channels,
                             size_t number_of_frames,
                             float sample_rate,
                             ExceptionState&);
  static AudioBuffer* Create(const AudioBufferOptions&, ExceptionState&);

  // Returns 0 if data is not a valid audio file.
  static AudioBuffer* CreateFromAudioFileData(const void* data,
                                              size_t data_size,
                                              bool mix_to_mono,
                                              float sample_rate);

  static AudioBuffer* CreateFromAudioBus(AudioBus*);

  // Format
  size_t length() const { return length_; }
  double duration() const {
    return length() / static_cast<double>(sampleRate());
  }
  float sampleRate() const { return sample_rate_; }

  // Channel data access
  unsigned numberOfChannels() const { return channels_.size(); }
  NotShared<DOMFloat32Array> getChannelData(unsigned channel_index,
                                            ExceptionState&);
  NotShared<DOMFloat32Array> getChannelData(unsigned channel_index);
  void copyFromChannel(NotShared<DOMFloat32Array>,
                       long channel_number,
                       ExceptionState&);
  void copyFromChannel(NotShared<DOMFloat32Array>,
                       long channel_number,
                       unsigned long start_in_channel,
                       ExceptionState&);
  void copyToChannel(NotShared<DOMFloat32Array>,
                     long channel_number,
                     ExceptionState&);
  void copyToChannel(NotShared<DOMFloat32Array>,
                     long channel_number,
                     unsigned long start_in_channel,
                     ExceptionState&);

  void Zero();

  DEFINE_INLINE_TRACE() { visitor->Trace(channels_); }

 private:
  explicit AudioBuffer(AudioBus*);

  static DOMFloat32Array* CreateFloat32ArrayOrNull(size_t length);

  AudioBuffer(unsigned number_of_channels,
              size_t number_of_frames,
              float sample_rate);
  bool CreatedSuccessfully(unsigned desired_number_of_channels) const;

  float sample_rate_;
  size_t length_;

  HeapVector<Member<DOMFloat32Array>> channels_;
};

}  // namespace blink

#endif  // AudioBuffer_h
