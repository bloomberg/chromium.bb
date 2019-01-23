// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_FRAGMENT_ANCHOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_FRAGMENT_ANCHOR_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/scroll/scroll_types.h"

namespace blink {

class LocalFrame;
class KURL;

// This class encapsulates the behavior of a "fragment anchor". A fragment
// anchor allows a page to link to a specific part of a page by specifying an
// element id in the URL fragment. The fragment is the part after the '#'
// character. E.g. navigating to www.example.com/index.html#section3 will find
// the element with id "section3" and focus and scroll it into view.
//
// While the page is loading, the fragment anchor tries to repeatedly scroll
// the element into view since it's position may change as a result of layouts.
// TODO(bokan): Maybe we no longer need the repeated scrolling since that
// should be handled by scroll-anchoring?
class CORE_EXPORT FragmentAnchor : public GarbageCollected<FragmentAnchor> {
 public:
  // Parses the fragment string and tries to create a FragmentAnchor object. If
  // an appropriate target was found based on the given fragment, this method
  // will create and return a FragmentAnchor object that can be used to keep it
  // in view.  Otherwise, this will return nullptr. In either case,
  // side-effects on the document will be performed, for example,
  // setting/clearing :target and svgView(). If needs_invoke is false, only the
  // side effects will be performed, the element won't be scrolled/focused and
  // this method returns nullptr (e.g. used during history navigation where we
  // don't want to clobber the history scroll restoration).
  static FragmentAnchor* TryCreate(const KURL& url,
                                   bool needs_invoke,
                                   LocalFrame& frame);

  FragmentAnchor(Node& anchor_node, LocalFrame& frame);
  virtual ~FragmentAnchor() = default;

  // Invoking the fragment anchor scrolls it into view and performs any other
  // desired actions. This is called repeatedly during loading as the lifecycle
  // is updated to keep the element in view. If true, the anchor should be kept
  // alive and invoked again. Otherwise it may be disposed.
  bool Invoke();

  // Used to let the fragment know the frame's been scrolled and so we should
  // abort keeping the fragment target in view to avoid fighting with user
  // scrolls.
  void DidScroll(ScrollType type);

  void PerformPreRafActions();

  void DidCompleteLoad();

  void Trace(blink::Visitor*);

 private:
  void ApplyFocusIfNeeded();

  WeakMember<Node> anchor_node_;
  Member<LocalFrame> frame_;
  bool needs_focus_;

  // While this is true, the fragment is still "active" in the sense that we
  // want the owner to continue calling Invoke(). Once this is false, calling
  // Invoke has no effect and the fragment can be disposed.
  bool needs_invoke_ = true;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_FRAGMENT_ANCHOR_H_
