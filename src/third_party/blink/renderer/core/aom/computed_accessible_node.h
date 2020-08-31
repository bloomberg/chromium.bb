// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_AOM_COMPUTED_ACCESSIBLE_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_AOM_COMPUTED_ACCESSIBLE_NODE_H_

#include "third_party/blink/public/platform/web_computed_ax_tree.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/accessibility/ax_context.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class Document;
class ScriptState;

class ComputedAccessibleNodePromiseResolver final
    : public GarbageCollected<ComputedAccessibleNodePromiseResolver> {
 public:
  ComputedAccessibleNodePromiseResolver(ScriptState*, Element&);
  ~ComputedAccessibleNodePromiseResolver() {}

  ScriptPromise Promise();
  void ComputeAccessibleNode();
  void EnsureUpToDate();
  void Trace(Visitor*);

 private:
  void UpdateTreeAndResolve();
  class RequestAnimationFrameCallback;

  int continue_callback_request_id_ = 0;
  Member<Element> element_;
  Member<ScriptPromiseResolver> resolver_;
  bool resolve_with_node_;
  std::unique_ptr<AXContext> ax_context_;
};

class ComputedAccessibleNode : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ComputedAccessibleNode(AXID, WebComputedAXTree*, Document*);
  ~ComputedAccessibleNode() override = default;

  void Trace(Visitor*) override;

  // TODO(meredithl): add accessors for state properties.
  base::Optional<bool> atomic() const;
  base::Optional<bool> busy() const;
  base::Optional<bool> disabled() const;
  base::Optional<bool> readOnly() const;
  base::Optional<bool> expanded() const;
  base::Optional<bool> modal() const;
  base::Optional<bool> multiline() const;
  base::Optional<bool> multiselectable() const;
  base::Optional<bool> required() const;
  base::Optional<bool> selected() const;

  base::Optional<int32_t> colCount() const;
  base::Optional<int32_t> colIndex() const;
  base::Optional<int32_t> colSpan() const;
  base::Optional<int32_t> level() const;
  base::Optional<int32_t> posInSet() const;
  base::Optional<int32_t> rowCount() const;
  base::Optional<int32_t> rowIndex() const;
  base::Optional<int32_t> rowSpan() const;
  base::Optional<int32_t> setSize() const;

  base::Optional<float> valueMax() const;
  base::Optional<float> valueMin() const;
  base::Optional<float> valueNow() const;

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
  base::Optional<bool> GetBoolAttribute(WebAOMBoolAttribute) const;
  base::Optional<int32_t> GetIntAttribute(WebAOMIntAttribute) const;
  base::Optional<float> GetFloatAttribute(WebAOMFloatAttribute) const;
  const String GetStringAttribute(WebAOMStringAttribute) const;

  AXID ax_id_;

  // This tree is owned by the RenderFrame.
  blink::WebComputedAXTree* tree_;
  Member<Document> document_;
  std::unique_ptr<AXContext> ax_context_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_AOM_COMPUTED_ACCESSIBLE_NODE_H_
