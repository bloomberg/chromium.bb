// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InspectedFrames_h
#define InspectedFrames_h

#include "base/macros.h"
#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class LocalFrame;

class CORE_EXPORT InspectedFrames final
    : public GarbageCollectedFinalized<InspectedFrames> {
 public:
  class CORE_EXPORT Iterator {
    STACK_ALLOCATED();

   public:
    Iterator operator++(int);
    Iterator& operator++();
    bool operator==(const Iterator& other);
    bool operator!=(const Iterator& other);
    LocalFrame* operator*() { return current_; }
    LocalFrame* operator->() { return current_; }

   private:
    friend class InspectedFrames;
    Iterator(LocalFrame* root, LocalFrame* current);
    Member<LocalFrame> root_;
    Member<LocalFrame> current_;
  };

  InspectedFrames(LocalFrame*, const String& devtools_frame);

  LocalFrame* Root() { return root_; }
  const String& GetDevToolsFrameToken() const { return devtools_frame_token_; }
  bool Contains(LocalFrame*) const;
  LocalFrame* FrameWithSecurityOrigin(const String& origin_raw_string);
  Iterator begin();
  Iterator end();

  virtual void Trace(blink::Visitor*);

 private:
  Member<LocalFrame> root_;
  String devtools_frame_token_;
  DISALLOW_COPY_AND_ASSIGN(InspectedFrames);
};

}  // namespace blink

#endif  // InspectedFrames_h
