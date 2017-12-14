// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InspectorResourceContainer_h
#define InspectorResourceContainer_h

#include "base/macros.h"
#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/text/StringHash.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class InspectedFrames;
class LocalFrame;

class CORE_EXPORT InspectorResourceContainer
    : public GarbageCollectedFinalized<InspectorResourceContainer> {
 public:
  explicit InspectorResourceContainer(InspectedFrames*);
  ~InspectorResourceContainer();
  void Trace(blink::Visitor*);

  void DidCommitLoadForLocalFrame(LocalFrame*);

  void StoreStyleSheetContent(const String& url, const String& content);
  bool LoadStyleSheetContent(const String& url, String* content);

  void StoreStyleElementContent(int backend_node_id, const String& content);
  bool LoadStyleElementContent(int backend_node_id, String* content);
  void EraseStyleElementContent(int backend_node_id);

 private:
  Member<InspectedFrames> inspected_frames_;
  HashMap<String, String> style_sheet_contents_;
  HashMap<int, String> style_element_contents_;
  DISALLOW_COPY_AND_ASSIGN(InspectorResourceContainer);
};

}  // namespace blink

#endif  // !defined(InspectorResourceContainer_h)
