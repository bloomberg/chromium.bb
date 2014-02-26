/*
    Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef Navigator_h
#define Navigator_h

#include "bindings/v8/ScriptWrappable.h"
#include "core/frame/DOMWindowProperty.h"
#include "core/frame/NavigatorBase.h"
#include "heap/Handle.h"
#include "platform/Supplementable.h"
#include "wtf/Forward.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"

namespace WebCore {

class DOMMimeTypeArray;
class DOMPluginArray;
class LocalFrame;
class PluginData;

typedef int ExceptionCode;

class Navigator FINAL : public NavigatorBase, public ScriptWrappable, public RefCounted<Navigator>, public DOMWindowProperty, public Supplementable<Navigator> {
public:
    static PassRefPtr<Navigator> create(LocalFrame* frame) { return adoptRef(new Navigator(frame)); }
    virtual ~Navigator();

    AtomicString language() const;
    DOMPluginArray* plugins() const;
    DOMMimeTypeArray* mimeTypes() const;
    bool cookieEnabled() const;
    bool javaEnabled() const;

    String productSub() const;
    String vendor() const;
    String vendorSub() const;

    virtual String userAgent() const OVERRIDE;

    // Relinquishes the storage lock, if one exists.
    void getStorageUpdates();

private:
    explicit Navigator(LocalFrame*);

    mutable RefPtrWillBePersistent<DOMPluginArray> m_plugins;
    mutable RefPtrWillBePersistent<DOMMimeTypeArray> m_mimeTypes;
};

}

#endif
