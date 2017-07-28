/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef History_h
#define History_h

#include "base/gtest_prod_util.h"
#include "bindings/core/v8/serialization/SerializedScriptValue.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/loader/FrameLoaderTypes.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/Time.h"

namespace blink {

class LocalFrame;
class KURL;
class ExceptionState;
class SecurityOrigin;
class ScriptState;

// This class corresponds to the History interface.
class CORE_EXPORT History final : public GarbageCollectedFinalized<History>,
                                  public ScriptWrappable,
                                  public DOMWindowClient {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(History);

 public:
  static History* Create(LocalFrame* frame) { return new History(frame); }

  unsigned length() const;
  SerializedScriptValue* state();

  void back(ScriptState*);
  void forward(ScriptState*);
  void go(ScriptState*, int delta);

  void pushState(PassRefPtr<SerializedScriptValue>,
                 const String& title,
                 const String& url,
                 ExceptionState&);

  void replaceState(PassRefPtr<SerializedScriptValue> data,
                    const String& title,
                    const String& url,
                    ExceptionState& exception_state) {
    StateObjectAdded(std::move(data), title, url, ScrollRestorationInternal(),
                     kFrameLoadTypeReplaceCurrentItem, exception_state);
  }

  void setScrollRestoration(const String& value);
  String scrollRestoration();

  bool stateChanged() const;
  bool IsSameAsCurrentState(SerializedScriptValue*) const;

  DECLARE_VIRTUAL_TRACE();

 private:
  FRIEND_TEST_ALL_PREFIXES(HistoryTest, CanChangeToURL);
  FRIEND_TEST_ALL_PREFIXES(HistoryTest, CanChangeToURLInFileOrigin);
  FRIEND_TEST_ALL_PREFIXES(HistoryTest, CanChangeToURLInUniqueOrigin);

  explicit History(LocalFrame*);

  static bool CanChangeToUrl(const KURL&,
                             SecurityOrigin*,
                             const KURL& document_url);

  KURL UrlForState(const String& url);

  void StateObjectAdded(PassRefPtr<SerializedScriptValue>,
                        const String& title,
                        const String& url,
                        HistoryScrollRestorationType,
                        FrameLoadType,
                        ExceptionState&);
  SerializedScriptValue* StateInternal() const;
  HistoryScrollRestorationType ScrollRestorationInternal() const;

  bool ShouldThrottleStateObjectChanges();

  RefPtr<SerializedScriptValue> last_state_object_requested_;
  struct {
    int count;
    TimeTicks last_updated;
  } state_flood_guard;
};

}  // namespace blink

#endif  // History_h
