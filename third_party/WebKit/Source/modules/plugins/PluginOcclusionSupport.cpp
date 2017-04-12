/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/plugins/PluginOcclusionSupport.h"

#include "core/HTMLNames.h"
#include "core/dom/Element.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLElement.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/LayoutObject.h"
#include "platform/FrameViewBase.h"
#include "platform/wtf/HashSet.h"

// This file provides a utility function to support rendering certain elements
// above plugins.

namespace blink {

static void GetObjectStack(const LayoutObject* ro,
                           Vector<const LayoutObject*>* ro_stack) {
  ro_stack->Clear();
  while (ro) {
    ro_stack->push_back(ro);
    ro = ro->Parent();
  }
}

// Returns true if stack1 is at or above stack2
static bool IframeIsAbovePlugin(
    const Vector<const LayoutObject*>& iframe_zstack,
    const Vector<const LayoutObject*>& plugin_zstack) {
  for (size_t i = 0; i < iframe_zstack.size() && i < plugin_zstack.size();
       i++) {
    // The root is at the end of these stacks. We want to iterate
    // root-downwards so we index backwards from the end.
    const LayoutObject* ro1 = iframe_zstack[iframe_zstack.size() - 1 - i];
    const LayoutObject* ro2 = plugin_zstack[plugin_zstack.size() - 1 - i];

    if (ro1 != ro2) {
      // When we find nodes in the stack that are not the same, then
      // we've found the nodes just below the lowest comment ancestor.
      // Determine which should be on top.

      // See if z-index determines an order.
      if (ro1->Style() && ro2->Style()) {
        int z1 = ro1->Style()->ZIndex();
        int z2 = ro2->Style()->ZIndex();
        if (z1 > z2)
          return true;
        if (z1 < z2)
          return false;
      }

      // If the plugin does not have an explicit z-index it stacks behind the
      // iframe.  This is for maintaining compatibility with IE.
      if (!ro2->IsPositioned()) {
        // The 0'th elements of these LayoutObject arrays represent the plugin
        // node and the iframe.
        const LayoutObject* plugin_layout_object = plugin_zstack[0];
        const LayoutObject* iframe_layout_object = iframe_zstack[0];

        if (plugin_layout_object->Style() && iframe_layout_object->Style()) {
          if (plugin_layout_object->Style()->ZIndex() >
              iframe_layout_object->Style()->ZIndex())
            return false;
        }
        return true;
      }

      // Inspect the document order. Later order means higher stacking.
      const LayoutObject* parent = ro1->Parent();
      if (!parent)
        return false;
      ASSERT(parent == ro2->Parent());

      for (const LayoutObject* ro = parent->SlowFirstChild(); ro;
           ro = ro->NextSibling()) {
        if (ro == ro1)
          return false;
        if (ro == ro2)
          return true;
      }
      ASSERT(false);  // We should have seen ro1 and ro2 by now.
      return false;
    }
  }
  return true;
}

static bool IntersectsRect(const LayoutObject* renderer, const IntRect& rect) {
  return renderer->AbsoluteBoundingBoxRectIgnoringTransforms().Intersects(
             rect) &&
         (!renderer->Style() ||
          renderer->Style()->Visibility() == EVisibility::kVisible);
}

static void AddToOcclusions(const LayoutBox* renderer,
                            Vector<IntRect>& occlusions) {
  occlusions.push_back(IntRect(RoundedIntPoint(renderer->LocalToAbsolute()),
                               FlooredIntSize(renderer->Size())));
}

static void AddTreeToOcclusions(const LayoutObject* renderer,
                                const IntRect& frame_rect,
                                Vector<IntRect>& occlusions) {
  if (!renderer)
    return;
  if (renderer->IsBox() && IntersectsRect(renderer, frame_rect))
    AddToOcclusions(ToLayoutBox(renderer), occlusions);
  for (LayoutObject* child = renderer->SlowFirstChild(); child;
       child = child->NextSibling())
    AddTreeToOcclusions(child, frame_rect, occlusions);
}

static const Element* TopLayerAncestor(const Element* element) {
  while (element && !element->IsInTopLayer())
    element = element->ParentOrShadowHostElement();
  return element;
}

// Return a set of rectangles that should not be overdrawn by the
// plugin ("cutouts"). This helps implement the "iframe shim"
// technique of overlaying a windowed plugin with content from the
// page. In a nutshell, iframe elements should occlude plugins when
// they occur higher in the stacking order.
void GetPluginOcclusions(Element* element,
                         FrameViewBase* parent,
                         const IntRect& frame_rect,
                         Vector<IntRect>& occlusions) {
  LayoutObject* plugin_node = element->GetLayoutObject();
  ASSERT(plugin_node);
  if (!plugin_node->Style())
    return;
  Vector<const LayoutObject*> plugin_zstack;
  Vector<const LayoutObject*> iframe_zstack;
  GetObjectStack(plugin_node, &plugin_zstack);

  if (!parent->IsFrameView())
    return;

  FrameView* parent_frame_view = ToFrameView(parent);

  // Occlusions by iframes.
  const FrameView::ChildrenSet* children = parent_frame_view->Children();
  for (FrameView::ChildrenSet::const_iterator it = children->begin();
       it != children->end(); ++it) {
    // We only care about FrameView's because iframes show up as FrameViews.
    if (!(*it)->IsFrameView())
      continue;

    const FrameView* frame_view = ToFrameView(it->Get());
    // Check to make sure we can get both the element and the LayoutObject
    // for this FrameView, if we can't just move on to the next object.
    // FIXME: Plugin occlusion by remote frames is probably broken.
    HTMLElement* element = frame_view->GetFrame().DeprecatedLocalOwner();
    if (!element || !element->GetLayoutObject())
      continue;

    LayoutObject* iframe_renderer = element->GetLayoutObject();

    if (isHTMLIFrameElement(*element) &&
        IntersectsRect(iframe_renderer, frame_rect)) {
      GetObjectStack(iframe_renderer, &iframe_zstack);
      if (IframeIsAbovePlugin(iframe_zstack, plugin_zstack))
        AddToOcclusions(ToLayoutBox(iframe_renderer), occlusions);
    }
  }

  // Occlusions by top layer elements.
  // FIXME: There's no handling yet for the interaction between top layer and
  // iframes. For example, a plugin in the top layer will be occluded by an
  // iframe. And a plugin inside an iframe in the top layer won't be respected
  // as being in the top layer.
  const Element* ancestor = TopLayerAncestor(element);
  Document* document = parent_frame_view->GetFrame().GetDocument();
  const HeapVector<Member<Element>>& elements = document->TopLayerElements();
  size_t start = ancestor ? elements.Find(ancestor) + 1 : 0;
  for (size_t i = start; i < elements.size(); ++i)
    AddTreeToOcclusions(elements[i]->GetLayoutObject(), frame_rect, occlusions);
}

}  // namespace blink
