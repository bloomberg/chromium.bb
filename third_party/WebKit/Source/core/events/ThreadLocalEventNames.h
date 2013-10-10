/*
 * Copyright (C) 2005, 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Jon Shier (jshier@iastate.edu)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef ThreadLocalEventNames_h
#define ThreadLocalEventNames_h

#include "EventInterfaces.h"
#include "EventTargetInterfaces.h"
#include "EventTargetNames.h"
#include "EventTypeNames.h"
#include "core/platform/ThreadGlobalData.h"
#include "wtf/text/AtomicString.h"

namespace WebCore {

    class ThreadLocalEventNames {
        WTF_MAKE_NONCOPYABLE(ThreadLocalEventNames); WTF_MAKE_FAST_ALLOCATED;
        int dummy; // Needed to make initialization macro work.
        // Private to prevent accidental call to ThreadLocalEventNames() instead of eventNames()
        ThreadLocalEventNames();
        friend class ThreadGlobalData;

    public:
        #define EVENT_INTERFACE_DECLARE(name) AtomicString interfaceFor##name;
        EVENT_INTERFACES_FOR_EACH(EVENT_INTERFACE_DECLARE)
        EVENT_TARGET_INTERFACES_FOR_EACH(EVENT_INTERFACE_DECLARE)
        #undef EVENT_INTERFACE_DECLARE
    };

    inline ThreadLocalEventNames& eventNames()
    {
        return threadGlobalData().eventNames();
    }

    inline bool isTouchEventType(const AtomicString& eventType)
    {
        return eventType == EventTypeNames::touchstart
            || eventType == EventTypeNames::touchmove
            || eventType == EventTypeNames::touchend
            || eventType == EventTypeNames::touchcancel;
    }

}

#endif
