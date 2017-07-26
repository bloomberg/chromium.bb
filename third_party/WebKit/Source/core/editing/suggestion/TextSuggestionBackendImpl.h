// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TextSuggestionBackendImpl_h
#define TextSuggestionBackendImpl_h

#include "core/CoreExport.h"
#include "platform/heap/Persistent.h"
#include "public/platform/input_messages.mojom-blink.h"

namespace blink {

class LocalFrame;

// Implementation of mojom::blink::TextSuggestionBackend
class CORE_EXPORT TextSuggestionBackendImpl final
    : public mojom::blink::TextSuggestionBackend {
 public:
  static void Create(LocalFrame*, mojom::blink::TextSuggestionBackendRequest);

  void ApplySpellCheckSuggestion(const String& suggestion) final;
  void DeleteActiveSuggestionRange() final;
  void NewWordAddedToDictionary(const String& word) final;
  void SpellCheckMenuTimeoutCallback() final;
  void SuggestionMenuClosed() final;

 private:
  explicit TextSuggestionBackendImpl(LocalFrame&);

  WeakPersistent<LocalFrame> frame_;

  DISALLOW_COPY_AND_ASSIGN(TextSuggestionBackendImpl);
};

}  // namespace blink

#endif  // TextSuggestionBackendImpl_h
