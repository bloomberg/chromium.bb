/*
 * Copyright (C) 2008, 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef Location_h
#define Location_h

#include "bindings/v8/ScriptWrappable.h"
#include "core/dom/DOMStringList.h"
#include "core/frame/DOMWindowProperty.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class DOMWindow;
class ExceptionState;
class LocalFrame;
class KURL;

class Location FINAL : public ScriptWrappable, public RefCounted<Location>, public DOMWindowProperty {
public:
    static PassRefPtr<Location> create(LocalFrame* frame) { return adoptRef(new Location(frame)); }

    void setHref(DOMWindow* callingWindow, DOMWindow* enteredWindow, const String&);
    String href() const;

    void assign(DOMWindow* callingWindow, DOMWindow* enteredWindow, const String&);
    void replace(DOMWindow* callingWindow, DOMWindow* enteredWindow, const String&);
    void reload(DOMWindow* callingWindow);

    void setProtocol(DOMWindow* callingWindow, DOMWindow* enteredWindow, const String&, ExceptionState&);
    String protocol() const;
    void setHost(DOMWindow* callingWindow, DOMWindow* enteredWindow, const String&);
    String host() const;
    void setHostname(DOMWindow* callingWindow, DOMWindow* enteredWindow, const String&);
    String hostname() const;
    void setPort(DOMWindow* callingWindow, DOMWindow* enteredWindow, const String&);
    String port() const;
    void setPathname(DOMWindow* callingWindow, DOMWindow* enteredWindow, const String&);
    String pathname() const;
    void setSearch(DOMWindow* callingWindow, DOMWindow* enteredWindow, const String&);
    String search() const;
    void setHash(DOMWindow* callingWindow, DOMWindow* enteredWindow, const String&);
    String hash() const;
    String origin() const;

    PassRefPtr<DOMStringList> ancestorOrigins() const;

private:
    explicit Location(LocalFrame*);

    void setLocation(const String&, DOMWindow* callingWindow, DOMWindow* enteredWindow);

    const KURL& url() const;
};

} // namespace WebCore

#endif // Location_h
