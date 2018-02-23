// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ComputedAccessibleNode_h
#define ComputedAccessibleNode_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/AXObjectCache.h"
#include "core/dom/events/EventTarget.h"
#include "platform/bindings/ScriptWrappable.h"
#include "third_party/WebKit/Source/core/dom/Element.h"
#include "third_party/WebKit/Source/core/frame/LocalFrame.h"
#include "third_party/WebKit/Source/platform/wtf/text/WTFString.h"
#include "third_party/WebKit/public/platform/WebComputedAXTree.h"

namespace blink {

class ScriptState;

class ComputedAccessibleNodePromiseResolver final
    : public GarbageCollectedFinalized<ComputedAccessibleNodePromiseResolver> {
 public:
  static ComputedAccessibleNodePromiseResolver* Create(ScriptState*, Element&);
  ~ComputedAccessibleNodePromiseResolver() {}

  ScriptPromise Promise();
  void ComputeAccessibleNode();
  void EnsureUpToDate();
  void Trace(blink::Visitor*);

 private:
  ComputedAccessibleNodePromiseResolver(ScriptState*, Element&);
  void UpdateTreeAndResolve();
  class RequestAnimationFrameCallback;

  int continue_callback_request_id_ = 0;
  Member<Element> element_;
  Member<ScriptPromiseResolver> resolver_;
  bool resolve_with_node_;
};

class ComputedAccessibleNode : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static ComputedAccessibleNode* Create(AXID, WebComputedAXTree*, LocalFrame*);
  virtual ~ComputedAccessibleNode();

  void Trace(Visitor*);

  // TODO(meredithl): add accessors for state properties.
  bool atomic(bool& is_null) const;
  bool busy(bool& is_null) const;
  bool disabled(bool& is_null) const;
  bool readOnly(bool& is_null) const;
  bool expanded(bool& is_null) const;
  bool modal(bool& is_null) const;
  bool multiline(bool& is_null) const;
  bool multiselectable(bool& is_null) const;
  bool required(bool& is_null) const;
  bool selected(bool& is_null) const;

  int32_t colCount(bool& is_null) const;
  int32_t colIndex(bool& is_null) const;
  int32_t colSpan(bool& is_null) const;
  int32_t level(bool& is_null) const;
  int32_t posInSet(bool& is_null) const;
  int32_t rowCount(bool& is_null) const;
  int32_t rowIndex(bool& is_null) const;
  int32_t rowSpan(bool& is_null) const;
  int32_t setSize(bool& is_null) const;

  float valueMax(bool& is_null) const;
  float valueMin(bool& is_null) const;
  float valueNow(bool& is_null) const;

  const String autocomplete() const;
  const String checked() const;
  const String keyShortcuts() const;
  const String name() const;
  const String placeholder() const;
  const String role() const;
  const String roleDescription() const;
  const String valueText() const;

  ComputedAccessibleNode* parent() const;
  ComputedAccessibleNode* firstChild() const;
  ComputedAccessibleNode* lastChild() const;
  ComputedAccessibleNode* previousSibling() const;
  ComputedAccessibleNode* nextSibling() const;

  ScriptPromise ensureUpToDate(ScriptState*);

 private:
  ComputedAccessibleNode(AXID, WebComputedAXTree*, LocalFrame*);
  bool GetBoolAttribute(WebAOMBoolAttribute, bool& is_null) const;
  int32_t GetIntAttribute(WebAOMIntAttribute, bool& is_null) const;
  float GetFloatAttribute(WebAOMFloatAttribute, bool& is_null) const;
  const String GetStringAttribute(WebAOMStringAttribute) const;

  AXID ax_id_;

  // This tree is owned by the RenderFrame.
  blink::WebComputedAXTree* tree_;
  Member<LocalFrame> frame_;
};

}  // namespace blink

#endif  // ComputedAccessibleNode_h
