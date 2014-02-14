// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/geolocation/Geoposition.h"

namespace WebCore {

DEFINE_GC_INFO(Geoposition);

void Geoposition::trace(Visitor* visitor)
{
    visitor->trace(m_coordinates);
}

}
