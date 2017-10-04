// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/HTMLPictureElement.h"

#include "core/dom/ElementTraversal.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/HTMLSourceElement.h"
#include "core/html_names.h"
#include "core/loader/ImageLoader.h"

namespace blink {

using namespace HTMLNames;

inline HTMLPictureElement::HTMLPictureElement(Document& document)
    : HTMLElement(pictureTag, document) {}

DEFINE_NODE_FACTORY(HTMLPictureElement)

void HTMLPictureElement::SourceOrMediaChanged() {
  for (HTMLImageElement* image_element =
           Traversal<HTMLImageElement>::FirstChild(*this);
       image_element; image_element = Traversal<HTMLImageElement>::NextSibling(
                          *image_element)) {
    image_element->SelectSourceURL(ImageLoader::kUpdateNormal);
  }
}

void HTMLPictureElement::RemoveListenerFromSourceChildren() {
  for (HTMLSourceElement* source_element =
           Traversal<HTMLSourceElement>::FirstChild(*this);
       source_element;
       source_element =
           Traversal<HTMLSourceElement>::NextSibling(*source_element)) {
    source_element->RemoveMediaQueryListListener();
  }
}

void HTMLPictureElement::AddListenerToSourceChildren() {
  for (HTMLSourceElement* source_element =
           Traversal<HTMLSourceElement>::FirstChild(*this);
       source_element;
       source_element =
           Traversal<HTMLSourceElement>::NextSibling(*source_element)) {
    source_element->AddMediaQueryListListener();
  }
}

Node::InsertionNotificationRequest HTMLPictureElement::InsertedInto(
    ContainerNode* insertion_point) {
  UseCounter::Count(GetDocument(), WebFeature::kPicture);
  return HTMLElement::InsertedInto(insertion_point);
}

}  // namespace blink
