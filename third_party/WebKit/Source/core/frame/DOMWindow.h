// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DOMWindow_h
#define DOMWindow_h

#include "bindings/core/v8/Transferables.h"
#include "core/CoreExport.h"
#include "core/events/EventTarget.h"
#include "core/frame/DOMWindowBase64.h"
#include "core/frame/Frame.h"
#include "platform/heap/Handle.h"
#include "wtf/Assertions.h"
#include "wtf/Forward.h"

namespace blink {

class Document;
class InputDeviceCapabilitiesConstants;
class Location;
class LocalDOMWindow;
class MessageEvent;
class SerializedScriptValue;

class CORE_EXPORT DOMWindow : public EventTargetWithInlineData,
                              public DOMWindowBase64 {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ~DOMWindow() override;

  Frame* frame() const {
    // A Frame is typically reused for navigations. If |m_frame| is not null,
    // two conditions must always be true:
    // - |m_frame->domWindow()| must point back to this DOMWindow. If it does
    //   not, it is easy to introduce a bug where script execution uses the
    //   wrong DOMWindow (which may be cross-origin).
    // - |m_frame| must be attached, i.e. |m_frame->host()| must not be null.
    //   If |m_frame->host()| is null, this indicates a bug where the frame was
    //   detached but |m_frame| was not set to null. This bug can lead to
    //   issues where executing script incorrectly schedules work on a detached
    //   frame.
    SECURITY_DCHECK(!m_frame ||
                    (m_frame->domWindow() == this && m_frame->host()));
    return m_frame;
  }

  // GarbageCollectedFinalized overrides:
  DECLARE_VIRTUAL_TRACE();

  virtual bool isLocalDOMWindow() const = 0;
  virtual bool isRemoteDOMWindow() const = 0;

  // ScriptWrappable overrides:
  v8::Local<v8::Object> wrap(v8::Isolate*,
                             v8::Local<v8::Object> creationContext) final;
  v8::Local<v8::Object> associateWithWrapper(
      v8::Isolate*,
      const WrapperTypeInfo*,
      v8::Local<v8::Object> wrapper) final;

  // EventTarget overrides:
  const AtomicString& interfaceName() const override;
  const DOMWindow* toDOMWindow() const override;

  // Cross-origin DOM Level 0
  Location* location() const;
  bool closed() const;
  unsigned length() const;

  // Self-referential attributes
  DOMWindow* self() const;
  DOMWindow* window() const { return self(); }
  DOMWindow* frames() const { return self(); }

  DOMWindow* opener() const;
  DOMWindow* parent() const;
  DOMWindow* top() const;

  void focus(ExecutionContext*);
  virtual void blur() = 0;
  void close(ExecutionContext*);

  // FIXME: This handles both window[index] and window.frames[index]. However,
  // the spec exposes window.frames[index] across origins but not
  // window[index]...
  DOMWindow* anonymousIndexedGetter(uint32_t) const;

  void postMessage(PassRefPtr<SerializedScriptValue> message,
                   const MessagePortArray&,
                   const String& targetOrigin,
                   LocalDOMWindow* source,
                   ExceptionState&);

  String sanitizedCrossDomainAccessErrorMessage(
      const LocalDOMWindow* callingWindow) const;
  String crossDomainAccessErrorMessage(
      const LocalDOMWindow* callingWindow) const;
  bool isInsecureScriptAccess(LocalDOMWindow& callingWindow, const KURL&);

  // FIXME: When this DOMWindow is no longer the active DOMWindow (i.e.,
  // when its document is no longer the document that is displayed in its
  // frame), we would like to zero out m_frame to avoid being confused
  // by the document that is currently active in m_frame.
  // See https://bugs.webkit.org/show_bug.cgi?id=62054
  bool isCurrentlyDisplayedInFrame() const;

  bool isSecureContext() const;

  InputDeviceCapabilitiesConstants* getInputDeviceCapabilities();

 protected:
  explicit DOMWindow(Frame&);

  virtual void schedulePostMessage(MessageEvent*,
                                   PassRefPtr<SecurityOrigin> target,
                                   Document* source) = 0;

  void disconnectFromFrame() { m_frame = nullptr; }

 private:
  Member<Frame> m_frame;
  Member<InputDeviceCapabilitiesConstants> m_inputCapabilities;
  mutable Member<Location> m_location;

  // Set to true when close() has been called. Needed for
  // |window.closed| determinism; having it return 'true'
  // only after the layout widget's deferred window close
  // operation has been performed, exposes (confusing)
  // implementation details to scripts.
  bool m_windowIsClosing;

};

}  // namespace blink

#endif  // DOMWindow_h
