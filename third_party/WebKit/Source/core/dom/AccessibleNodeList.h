// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AccessibleNodeList_h
#define AccessibleNodeList_h

#include "core/CoreExport.h"
#include "platform/bindings/ScriptWrappable.h"

namespace blink {

class AccessibleNode;
enum class AOMRelationListProperty;
class ExceptionState;

// Accessibility Object Model node list
// Explainer: https://github.com/WICG/aom/blob/master/explainer.md
// Spec: https://wicg.github.io/aom/spec/
class CORE_EXPORT AccessibleNodeList
    : public GarbageCollectedFinalized<AccessibleNodeList>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static AccessibleNodeList* Create(const HeapVector<Member<AccessibleNode>>&);

  AccessibleNodeList();
  virtual ~AccessibleNodeList();

  void AddOwner(AOMRelationListProperty, AccessibleNode*);
  void RemoveOwner(AOMRelationListProperty, AccessibleNode*);

  AccessibleNode* item(unsigned offset) const;
  void add(AccessibleNode*, AccessibleNode* = nullptr);
  void remove(int index);
  bool AnonymousIndexedSetter(unsigned, AccessibleNode*, ExceptionState&);
  unsigned length() const;
  void setLength(unsigned);

  virtual void Trace(blink::Visitor*);

 private:
  void NotifyChanged();

  HeapVector<std::pair<AOMRelationListProperty, Member<AccessibleNode>>>
      owners_;
  HeapVector<Member<AccessibleNode>> nodes_;
};

}  // namespace blink

#endif  // AccessibleNodeList_h
