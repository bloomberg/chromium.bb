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

#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "core/CoreExport.h"
#include "core/dom/DOMStringList.h"
#include "core/frame/DOMWindow.h"
#include "wtf/text/WTFString.h"

namespace blink {

class Document;
class LocalDOMWindow;
class ExceptionState;
class KURL;

// This class corresponds to the Location interface. Location is the only
// interface besides Window that is accessible cross-origin and must handle
// remote frames.
//
// HTML standard: https://whatwg.org/C/browsers.html#the-location-interface
class CORE_EXPORT Location final : public GarbageCollected<Location>,
                                   public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static Location* create(DOMWindow* domWindow) {
    return new Location(domWindow);
  }

  DOMWindow* domWindow() const { return m_domWindow.get(); }
  // TODO(dcheng): Deprecated and will be removed. Do not use in new code!
  Frame* frame() const { return m_domWindow->frame(); }

  void setHref(LocalDOMWindow* currentWindow,
               LocalDOMWindow* enteredWindow,
               const String&,
               ExceptionState&);
  String href() const;

  void assign(LocalDOMWindow* currentWindow,
              LocalDOMWindow* enteredWindow,
              const String&,
              ExceptionState&);
  void replace(LocalDOMWindow* currentWindow,
               LocalDOMWindow* enteredWindow,
               const String&,
               ExceptionState&);
  void reload(LocalDOMWindow* currentWindow);

  void setProtocol(LocalDOMWindow* currentWindow,
                   LocalDOMWindow* enteredWindow,
                   const String&,
                   ExceptionState&);
  String protocol() const;
  void setHost(LocalDOMWindow* currentWindow,
               LocalDOMWindow* enteredWindow,
               const String&,
               ExceptionState&);
  String host() const;
  void setHostname(LocalDOMWindow* currentWindow,
                   LocalDOMWindow* enteredWindow,
                   const String&,
                   ExceptionState&);
  String hostname() const;
  void setPort(LocalDOMWindow* currentWindow,
               LocalDOMWindow* enteredWindow,
               const String&,
               ExceptionState&);
  String port() const;
  void setPathname(LocalDOMWindow* currentWindow,
                   LocalDOMWindow* enteredWindow,
                   const String&,
                   ExceptionState&);
  String pathname() const;
  void setSearch(LocalDOMWindow* currentWindow,
                 LocalDOMWindow* enteredWindow,
                 const String&,
                 ExceptionState&);
  String search() const;
  void setHash(LocalDOMWindow* currentWindow,
               LocalDOMWindow* enteredWindow,
               const String&,
               ExceptionState&);
  String hash() const;
  String origin() const;

  DOMStringList* ancestorOrigins() const;

  // Just return the |this| object the way the normal valueOf function on the
  // Object prototype would.  The valueOf function is only added to make sure
  // that it cannot be overwritten on location objects, since that would provide
  // a hook to change the string conversion behavior of location objects.
  ScriptValue valueOf(const ScriptValue& thisObject) { return thisObject; }

  DECLARE_VIRTUAL_TRACE();

 private:
  explicit Location(DOMWindow*);

  // Note: it is only valid to call this if this is a Location object for a
  // LocalDOMWindow.
  Document* document() const;

  // Returns true if the associated Window is the active Window in the frame.
  bool isAttached() const;

  enum class SetLocationPolicy { Normal, ReplaceThisFrame };
  void setLocation(const String&,
                   LocalDOMWindow* currentWindow,
                   LocalDOMWindow* enteredWindow,
                   ExceptionState* = nullptr,
                   SetLocationPolicy = SetLocationPolicy::Normal);

  const KURL& url() const;

  const Member<DOMWindow> m_domWindow;
};

}  // namespace blink

#endif  // Location_h
