// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FENCED_FRAME_HTML_FENCED_FRAME_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FENCED_FRAME_HTML_FENCED_FRAME_ELEMENT_H_

#include "base/notreached.h"
#include "third_party/blink/public/mojom/fenced_frame/fenced_frame.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class KURL;

// HTMLFencedFrameElement implements the <fencedframe> element, which hosts the
// main frame of a top-level browsing context in an isolated frame. This element
// is non-standard and is currently being developed in
// https://github.com/shivanigithub/fenced-frame. As a result, this element is
// not exposed by default, but can be enabled by one of the following:
// - Enabling the Fenced Frames about:flags entry
// - Passing --enable-features=FencedFrames
class CORE_EXPORT HTMLFencedFrameElement : public HTMLFrameOwnerElement {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // For a while there will be two underlying implementations of Fenced Frames:
  //   1.) The early Origin Trial implementation based on the ShadowDOM
  //       encapsulating a neutered <iframe> element
  //   2.) The MPArch implementation, which hosts a truly top-level FrameTree in
  //       the browser process, and relies on the MPArch long-tail feature work
  // For as long as both of these implementations need to exist, we abstract a
  // common API from them which is neatly captured by `FencedFrameDelegate`. The
  // actual implementation of this interface will be one of the options listed
  // above. See documentation above `FencedFrameMPArchDelegate` and
  // `FencedFrameShadowDOMDelegate`.
  class CORE_EXPORT FencedFrameDelegate
      : public GarbageCollected<FencedFrameDelegate> {
   public:
    static FencedFrameDelegate* Create(HTMLFencedFrameElement*);
    explicit FencedFrameDelegate(HTMLFencedFrameElement* outer_element)
        : outer_element_(outer_element) {}
    virtual ~FencedFrameDelegate();
    void Trace(Visitor* visitor) const;

    virtual void Navigate(const KURL&) = 0;
    // This method is used to clean up all state in preparation for destruction,
    // even though the destruction may happen arbitrarily later during garbage
    // collection.
    virtual void Dispose() {}

   protected:
    HTMLFencedFrameElement& GetElement() const { return *outer_element_; }

   private:
    Member<HTMLFencedFrameElement> outer_element_;
  };

  explicit HTMLFencedFrameElement(Document& document);
  ~HTMLFencedFrameElement() override;
  void Trace(Visitor* visitor) const override;

  // HTMLFrameOwnerElement overrides.
  void DisconnectContentFrame() override;
  FrameOwnerElementType OwnerType() const override {
    return FrameOwnerElementType::kFencedframe;
  }
  ParsedPermissionsPolicy ConstructContainerPolicy() const override {
    NOTREACHED();
    return ParsedPermissionsPolicy();
  }
  void SetCollapsed(bool) override;

  // HTMLElement overrides.
  bool IsHTMLFencedFrameElement() const final { return true; }

  // See the documentation above `mode_`.
  mojom::blink::FencedFrameMode GetMode() const { return mode_; }

  // The frame size is "frozen" when the `src` attribute is set.
  // The frozen state is kept in this element so that it can survive across
  // reattaches.
  // The size is in layout size (i.e., DSF multiplied.)
  const absl::optional<PhysicalSize> FrozenFrameSize() const;
  // True if the frame size should be frozen when the next resize completed.
  // When `src` is set but layout is not completed yet, the frame size is frozen
  // after the first layout.
  bool ShouldFreezeFrameSizeOnNextLayoutForTesting() const {
    return should_freeze_frame_size_on_next_layout_;
  }

  // Returns the inner `IFRAME` element. This element creates two boxes, the
  // outer container and the inner frame, so that the outer container can
  // respond to the size change requests from the containing layout algorithm,
  // while keeping the inner frame size unchanged.
  HTMLIFrameElement* InnerIFrameElement() const;

 private:
  // This method will only navigate the underlying frame if the element
  // `isConnected()`. It will be deferred if the page is currently prerendering.
  void Navigate();

  // Delegate creation will be deferred if the page is currently prerendering.
  void CreateDelegateAndNavigate();

  // Node overrides.
  Node::InsertionNotificationRequest InsertedInto(ContainerNode&) override;
  void DidNotifySubtreeInsertionsToDocument() override;
  void RemovedFrom(ContainerNode& node) override;

  // Element overrides.
  void ParseAttribute(const AttributeModificationParams&) override;
  bool IsURLAttribute(const Attribute&) const override;
  bool IsPresentationAttribute(const QualifiedName&) const override;
  void CollectStyleForPresentationAttribute(
      const QualifiedName&,
      const AtomicString&,
      MutableCSSPropertyValueSet*) override;
  bool LayoutObjectIsNeeded(const ComputedStyle&) const override;
  LayoutObject* CreateLayoutObject(const ComputedStyle&, LegacyLayout) override;
  void AttachLayoutTree(AttachContext& context) override;
  bool SupportsFocus() const override;

  void FreezeFrameSize();
  void FreezeFrameSize(const PhysicalSize&);

  // Given a size `requested_size`, return the nearest allowed fenced frame
  // size. Note that size restrictions only apply to top-level opaque-ads
  // fenced frames.
  // NB: `requested_size` should be in logical/CSS units, NOT physical units.
  // The returned size is also in logical/CSS units.
  // TODO(crbug.com/1123606): remove this once we bind size to opaque URLs.
  PhysicalSize CoerceFrameSize(const PhysicalSize& requested_size);

  void StartResizeObserver();
  void StopResizeObserver();
  void OnResize(const PhysicalRect& content_box);
  void UpdateInnerStyleOnFrozenInternalFrame();

  class ResizeObserverDelegate final : public ResizeObserver::Delegate {
   public:
    void OnResize(const HeapVector<Member<ResizeObserverEntry>>& entries) final;
  };

  // The underlying <fencedframe> implementation that we delegate all of the
  // important bits to. See the comment above this class declaration.
  // Note: This is null when the document is sandboxed without
  // `kFencedFrameMandatoryUnsandboxedFlags`.
  Member<FencedFrameDelegate> frame_delegate_;
  Member<ResizeObserver> resize_observer_;
  // See |FrozenFrameSize| above. Stored in CSS pixel (without DSF multiplied.)
  absl::optional<PhysicalSize> frozen_frame_size_;
  absl::optional<PhysicalRect> content_rect_;
  bool should_freeze_frame_size_on_next_layout_ = false;
  bool collapsed_by_client_ = false;
  // This represents the element's `mode` attribute. We store it here instead of
  // always reading it off of the element, because after the first navigation it
  // is effectively frozen. Like the frozen size of the frame, it survives
  // element reattachments too. We maintain the `freeze_mode_attribute_`
  // variable below so we can know when to reject updates to `mode_`.
  mojom::blink::FencedFrameMode mode_ = mojom::blink::FencedFrameMode::kDefault;
  bool freeze_mode_attribute_ = false;

  friend class ResizeObserverDelegate;
  FRIEND_TEST_ALL_PREFIXES(HTMLFencedFrameElementTest,
                           FreezeSizePageZoomFactor);
  FRIEND_TEST_ALL_PREFIXES(HTMLFencedFrameElementTest, CoerceFrameSizeTest);
};

// Type casting. Custom since adoption could lead to an HTMLFencedFrameElement
// ending up in a document that doesn't have the Fenced Frame origin trial
// enabled, which would result in creation of an HTMLUnknownElement with the
// "fencedframe" tag name. We can't support casting those elements to
// HTMLFencedFrameElements because they are not fenced frame elements.
// TODO(crbug.com/1123606): Remove these custom helpers when the origin trial is
// over.
template <>
struct DowncastTraits<HTMLFencedFrameElement> {
  static bool AllowFrom(const HTMLElement& element) {
    return element.IsHTMLFencedFrameElement();
  }
  static bool AllowFrom(const Node& node) {
    if (const HTMLElement* html_element = DynamicTo<HTMLElement>(node))
      return html_element->IsHTMLFencedFrameElement();
    return false;
  }
  static bool AllowFrom(const Element& element) {
    if (const HTMLElement* html_element = DynamicTo<HTMLElement>(element))
      return html_element->IsHTMLFencedFrameElement();
    return false;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FENCED_FRAME_HTML_FENCED_FRAME_ELEMENT_H_
