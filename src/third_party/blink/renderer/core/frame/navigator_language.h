// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_NAVIGATOR_LANGUAGE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_NAVIGATOR_LANGUAGE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class CORE_EXPORT NavigatorLanguage : public GarbageCollectedMixin {
 public:
  explicit NavigatorLanguage(ExecutionContext*);

  AtomicString language();
  const Vector<String>& languages();
  bool IsLanguagesDirty() const;
  void SetLanguagesDirty();
  AtomicString SerializeLanguagesForClientHintHeader();

  // Accepts a comma-separated list of languages.
  void SetLanguagesForTesting(const String& languages);

  void Trace(blink::Visitor*) override;

 protected:
  bool languages_dirty_ = true;
  WeakMember<ExecutionContext> context_;
  virtual String GetAcceptLanguages() = 0;

 private:
  void EnsureUpdatedLanguage();

  Vector<String> languages_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_NAVIGATOR_LANGUAGE_H_
