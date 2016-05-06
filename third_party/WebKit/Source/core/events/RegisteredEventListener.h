/*
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2008, 2009 Apple Inc. All rights reserved.
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

#ifndef RegisteredEventListener_h
#define RegisteredEventListener_h

#include "core/events/EventListener.h"
#include "wtf/RefPtr.h"

namespace blink {

class RegisteredEventListener {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
public:
    RegisteredEventListener()
        : m_useCapture(false)
        , m_passive(false)
    {
    }

    RegisteredEventListener(EventListener* listener, const EventListenerOptions& options)
        : m_listener(listener)
        , m_useCapture(options.capture())
        , m_passive(options.passive())
    {
    }

    DEFINE_INLINE_TRACE()
    {
        visitor->trace(m_listener);
    }

    EventListenerOptions options() const
    {
        EventListenerOptions result;
        result.setCapture(m_useCapture);
        result.setPassive(m_passive);
        return result;
    }

    const EventListener* listener() const
    {
        return m_listener;
    }

    EventListener* listener()
    {
        return m_listener;
    }

    bool passive() const
    {
        return m_passive;
    }

    bool capture() const
    {
        return m_useCapture;
    }

    bool matches(const EventListener* listener, const EventListenerOptions& options) const
    {
        return *m_listener == *listener && static_cast<bool>(m_useCapture) == options.capture() && static_cast<bool>(m_passive) == options.passive();
    }

    bool operator==(const RegisteredEventListener& other) const
    {
        ASSERT(m_listener);
        ASSERT(other.m_listener);
        return *m_listener == *other.m_listener && m_useCapture == other.m_useCapture && m_passive == other.m_passive;
    }

private:
    Member<EventListener> m_listener;
    unsigned m_useCapture : 1;
    unsigned m_passive : 1;
};

} // namespace blink

WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(blink::RegisteredEventListener);

#endif // RegisteredEventListener_h
