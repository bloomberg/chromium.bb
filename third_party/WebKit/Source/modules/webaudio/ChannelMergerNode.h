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

#ifndef ChannelMergerNode_h
#define ChannelMergerNode_h

#include "modules/webaudio/AudioNode.h"
#include "platform/wtf/RefPtr.h"

namespace blink {

class BaseAudioContext;
class ChannelMergerOptions;

class ChannelMergerHandler final : public AudioHandler {
 public:
  static RefPtr<ChannelMergerHandler> Create(AudioNode&,
                                             float sample_rate,
                                             unsigned number_of_inputs);

  void Process(size_t frames_to_process) override;
  void SetChannelCount(unsigned long, ExceptionState&) final;
  void SetChannelCountMode(const String&, ExceptionState&) final;

  // AudioNode
  double TailTime() const override { return 0; }
  double LatencyTime() const override { return 0; }

 private:
  ChannelMergerHandler(AudioNode&,
                       float sample_rate,
                       unsigned number_of_inputs);
};

class ChannelMergerNode final : public AudioNode {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static ChannelMergerNode* Create(BaseAudioContext&, ExceptionState&);
  static ChannelMergerNode* Create(BaseAudioContext&,
                                   unsigned number_of_inputs,
                                   ExceptionState&);
  static ChannelMergerNode* Create(BaseAudioContext*,
                                   const ChannelMergerOptions&,
                                   ExceptionState&);

 private:
  ChannelMergerNode(BaseAudioContext&, unsigned number_of_inputs);
};

}  // namespace blink

#endif  // ChannelMergerNode_h
