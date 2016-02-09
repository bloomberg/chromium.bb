// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/svg/SVGResourceClient.h"

#include "core/layout/svg/LayoutSVGResourceContainer.h"

namespace blink {

SVGResourceClient::~SVGResourceClient()
{
}

void SVGResourceClient::addFilterReference(SVGFilterElement* filter)
{
    if (filter->layoutObject())
        toLayoutSVGResourceContainer(filter->layoutObject())->addResourceClient(this);
    else
        filter->addClient(this);
    m_filterReferences.add(filter);
}

void SVGResourceClient::clearFilterReferences()
{
    for (SVGFilterElement* filter : m_filterReferences) {
        if (filter->layoutObject())
            toLayoutSVGResourceContainer(filter->layoutObject())->removeResourceClient(this);
        else
            filter->removeClient(this);
    }
    m_filterReferences.clear();
}

void SVGResourceClient::filterWillBeDestroyed(SVGFilterElement* filter)
{
    m_filterReferences.remove(filter);
    filterNeedsInvalidation();
}

} // namespace blink
