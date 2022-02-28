// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_APP_HISTORY_APP_HISTORY_DESTINATION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_APP_HISTORY_APP_HISTORY_DESTINATION_H_

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

class CORE_EXPORT AppHistoryDestination final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  AppHistoryDestination(const KURL& url,
                        bool same_document,
                        SerializedScriptValue* state)
      : url_(url), same_document_(same_document), state_(state) {}
  ~AppHistoryDestination() final = default;

  void SetTraverseProperties(const String& key,
                             const String& id,
                             int64_t index) {
    key_ = key;
    id_ = id;
    index_ = index;
  }

  const String& key() const { return key_; }
  const String& id() const { return id_; }
  const KURL& url() const { return url_; }
  int64_t index() const { return index_; }
  bool sameDocument() const { return same_document_; }
  ScriptValue getState(ScriptState* script_state) {
    v8::Isolate* isolate = script_state->GetIsolate();
    return state_ ? ScriptValue(isolate, state_->Deserialize(isolate))
                  : ScriptValue();
  }

 private:
  String key_;
  String id_;
  KURL url_;
  int64_t index_ = -1;
  bool same_document_;
  scoped_refptr<SerializedScriptValue> state_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_APP_HISTORY_APP_HISTORY_DESTINATION_H_
