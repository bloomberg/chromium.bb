// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AudioWorkletProcessor_h
#define AudioWorkletProcessor_h

#include "modules/ModulesExport.h"
#include "platform/audio/AudioArray.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/bindings/TraceWrapperV8Reference.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/text/WTFString.h"
#include "v8/include/v8.h"

namespace blink {

class AudioBus;
class AudioWorkletGlobalScope;
class AudioWorkletProcessorDefinition;

// AudioWorkletProcessor class represents the active instance created from
// AudioWorkletProcessorDefinition. |AudioWorkletNodeHandler| invokes
// process() method in this object upon graph rendering.
//
// This is constructed and destroyed on a worker thread, and all methods also
// must be called on the worker thread.
class MODULES_EXPORT AudioWorkletProcessor : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static AudioWorkletProcessor* Create(AudioWorkletGlobalScope*,
                                       const String& name);
  virtual ~AudioWorkletProcessor();

  void SetInstance(v8::Isolate*, v8::Local<v8::Object> instance);

  v8::Local<v8::Object> InstanceLocal(v8::Isolate*);

  // |AudioWorkletHandler| invokes this method to process audio.
  bool Process(
      Vector<AudioBus*>* input_buses,
      Vector<AudioBus*>* output_buses,
      HashMap<String, std::unique_ptr<AudioFloatArray>>* param_value_map,
      double current_time);

  const String& Name() const { return name_; }

  // Mark this processor as "not runnable" so it does not execute the
  // user-supplied code even after the associated node is connected to the
  // graph.
  void MarkNonRunnable() { is_runnable_ = false; }
  bool IsRunnable() { return is_runnable_; }

  void Trace(blink::Visitor*);
  void TraceWrappers(const ScriptWrappableVisitor*) const;

 private:
  AudioWorkletProcessor(AudioWorkletGlobalScope*, const String& name);

  Member<AudioWorkletGlobalScope> global_scope_;
  TraceWrapperV8Reference<v8::Object> instance_;
  const String name_;

  // Becomes |false| when Process() method throws an exception from the the
  // user-supplied code. It is an irreversible transition.
  bool is_runnable_ = true;
};

}  // namespace blink

#endif  // AudioWorkletProcessor_h
