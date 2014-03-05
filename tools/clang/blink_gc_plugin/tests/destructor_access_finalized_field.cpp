// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "destructor_access_finalized_field.h"

namespace WebCore {

HeapObject::~HeapObject()
{
    if (m_ref && m_obj)
        (void)m_objs;
}

void HeapObject::trace(Visitor* visitor)
{
    visitor->trace(m_obj);
    visitor->trace(m_objs);
}

}
