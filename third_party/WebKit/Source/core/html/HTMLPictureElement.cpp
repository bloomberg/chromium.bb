// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/html/HTMLPictureElement.h"

#include "core/HTMLNames.h"
#include "core/dom/ElementTraversal.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/HTMLSourceElement.h"
#include "core/loader/ImageLoader.h"

namespace blink {

using namespace HTMLNames;

inline HTMLPictureElement::HTMLPictureElement(Document& document)
    : HTMLElement(pictureTag, document)
{
}

DEFINE_NODE_FACTORY(HTMLPictureElement)

void HTMLPictureElement::sourceOrMediaChanged(HTMLElement* sourceElement, Node* next)
{
    bool seenSource = false;
    Node* node;
    NodeVector potentialSourceNodes;
    getChildNodes(*this, potentialSourceNodes);

    for (unsigned i = 0; i < potentialSourceNodes.size(); ++i) {
        node = potentialSourceNodes[i].get();
        if (sourceElement == node || (next && node == next))
            seenSource = true;
        if (isHTMLImageElement(node) && seenSource)
            toHTMLImageElement(node)->selectSourceURL(ImageLoader::UpdateNormal);
    }
}

Node::InsertionNotificationRequest HTMLPictureElement::insertedInto(ContainerNode* insertionPoint)
{
    UseCounter::count(document(), UseCounter::Picture);
    return HTMLElement::insertedInto(insertionPoint);
}

} // namespace
