// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ComputedAccessibleNode_h
#define ComputedAccessibleNode_h

#include "bindings/core/v8/ScriptPromise.h"
#include "core/dom/AXObjectCache.h"
#include "core/dom/events/EventTarget.h"
#include "platform/bindings/ScriptWrappable.h"
#include "third_party/WebKit/Source/core/dom/Element.h"
#include "third_party/WebKit/Source/platform/wtf/text/WTFString.h"
#include "third_party/WebKit/public/platform/WebComputedAXTree.h"

namespace blink {

class ScriptPromiseResolver;
class ScriptState;

class ComputedAccessibleNode : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static ComputedAccessibleNode* Create(Element*);
  virtual ~ComputedAccessibleNode();

  void Trace(Visitor*);

  // Starts computation of the accessibility properties of the stored element.
  ScriptPromise ComputeAccessibleProperties(ScriptState*);

  const String keyShortcuts() const;
  const String name() const;
  const String placeholder() const;
  const String role() const;
  const String roleDescription() const;
  const String valueText() const;

  int32_t colCount(bool& is_null) const;
  int32_t colIndex(bool& is_null) const;
  int32_t colSpan(bool& is_null) const;
  int32_t level(bool& is_null) const;
  int32_t posInSet(bool& is_null) const;
  int32_t rowCount(bool& is_null) const;
  int32_t rowIndex(bool& is_null) const;
  int32_t rowSpan(bool& is_null) const;
  int32_t setSize(bool& is_null) const;

  ComputedAccessibleNode* parent() const;
  ComputedAccessibleNode* firstChild() const;
  ComputedAccessibleNode* lastChild() const;
  ComputedAccessibleNode* previousSibling() const;
  ComputedAccessibleNode* nextSibling() const;

 private:
  explicit ComputedAccessibleNode(Element*);

  // content::ComputedAXTree callback.
  void OnSnapshotResponse(ScriptPromiseResolver*);

  int32_t GetIntAttribute(WebAOMIntAttribute, bool& is_null) const;
  const String GetStringAttribute(WebAOMStringAttribute) const;
  ComputedAccessibleNode* GetRelationFromCache(AXID) const;

  Member<Element> element_;
  Member<AXObjectCache> cache_;

  // This tree is owned by the RenderFrame.
  blink::WebComputedAXTree* tree_;
};

}  // namespace blink

#endif  // ComputedAccessibleNode_h
