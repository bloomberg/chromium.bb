// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AudioWorkletObjectProxy_h
#define AudioWorkletObjectProxy_h

#include "core/workers/ThreadedWorkletObjectProxy.h"
#include "modules/ModulesExport.h"

namespace blink {

class AudioWorkletMessagingProxy;

class MODULES_EXPORT AudioWorkletObjectProxy final
    : public ThreadedWorkletObjectProxy {
 public:
  AudioWorkletObjectProxy(AudioWorkletMessagingProxy*,
                          ParentFrameTaskRunners*);

  void EvaluateScript(const String& source,
                      const KURL& script_url,
                      WorkerThread*) final;

 private:
  CrossThreadWeakPersistent<AudioWorkletMessagingProxy>
      GetAudioWorkletMessagingProxyWeakPtr();
};

}  // namespace blink

#endif  // AudioWorkletObjectProxy_h
