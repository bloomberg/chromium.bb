/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#ifndef MediaQueryListListener_h
#define MediaQueryListListener_h

#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ScriptValue.h"
#include "core/css/MediaQueryList.h"
#include "platform/heap/Handle.h"
#include "wtf/RefCounted.h"

namespace blink {

class MediaQueryList;

// See http://dev.w3.org/csswg/cssom-view/#the-mediaquerylist-interface
// FIXME: For JS use this should become a DOM Event.
// C++ listeners can subclass this class and override call(). The no-argument constructor
// is provided for this purpose.
class MediaQueryListListener : public RefCountedWillBeGarbageCollectedFinalized<MediaQueryListListener> {
public:
    static PassRefPtrWillBeRawPtr<MediaQueryListListener> create(ScriptState* scriptState, const ScriptValue& value)
    {
        if (!value.isFunction())
            return nullptr;
        return adoptRefWillBeNoop(new MediaQueryListListener(scriptState, value));
    }
    virtual ~MediaQueryListListener();

    virtual void call();

    // Used to keep the MediaQueryList alive and registered with the MediaQueryMatcher
    // as long as the listener exists.
    void setMediaQueryList(MediaQueryList* query) { m_query = query; }
    void clearMediaQueryList() { m_query = nullptr; }

    bool operator==(const MediaQueryListListener& other) const { return m_function.isNull() ? this == &other : m_function == other.m_function; }

    virtual void trace(Visitor* visitor) { visitor->trace(m_query); }

protected:
    MediaQueryListListener();

    RefPtrWillBeMember<MediaQueryList> m_query;

private:
    MediaQueryListListener(ScriptState*, const ScriptValue&);

    RefPtr<ScriptState> m_scriptState;
    ScriptValue m_function;
};

}

#endif // MediaQueryListListener_h
