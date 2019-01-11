// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_DELETE_SOON_WITH_GRAPH_LOCK_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_DELETE_SOON_WITH_GRAPH_LOCK_H_

#include "base/callback_forward.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

class AudioHandler;
class AudioParamHandler;

// Performs deferred (non-nestable) deletion of the relevant object, acquiring
// the graph lock for the duration of the deletion.
//
// This is similar to base::RefCountedDeleteOnSequence, but assures non-nesting
// to avoid bad interactions with garbage collection. In particular, it assures
// that the destructor will not run within another task due to lazy sweeping.
MODULES_EXPORT void DeleteSoonWithGraphLock(const AudioHandler*);
MODULES_EXPORT void DeleteSoonWithGraphLock(const AudioParamHandler*);

// Waits for any audio handlers that are scheduled for deletion, and invoke the
// callback once no more remain. Will run immediately if there currently are
// none. This is intended for use by the leak detector and similar tests.
MODULES_EXPORT void WaitForAudioHandlerDeletion(base::OnceClosure);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_DELETE_SOON_WITH_GRAPH_LOCK_H_
