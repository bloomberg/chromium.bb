// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HTMLMediaElementEncryptedMedia_h
#define HTMLMediaElementEncryptedMedia_h

#include "core/EventTypeNames.h"
#include "core/dom/DOMTypedArray.h"
#include "core/events/EventTarget.h"
#include "modules/ModulesExport.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebEncryptedMediaTypes.h"
#include "public/platform/WebMediaPlayerEncryptedMediaClient.h"

namespace blink {

class ExceptionState;
class HTMLMediaElement;
class MediaKeys;
class ScriptPromise;
class ScriptState;
class WebContentDecryptionModule;
class WebMediaPlayer;

class MODULES_EXPORT HTMLMediaElementEncryptedMedia final : public NoBaseWillBeGarbageCollectedFinalized<HTMLMediaElementEncryptedMedia>, public WillBeHeapSupplement<HTMLMediaElement>, public WebMediaPlayerEncryptedMediaClient {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(HTMLMediaElementEncryptedMedia);
    USING_FAST_MALLOC_WILL_BE_REMOVED(HTMLMediaElementEncryptedMedia);
public:
    // encrypted media extensions (v0.1b)
    static void webkitGenerateKeyRequest(HTMLMediaElement&, const String& keySystem, PassRefPtr<DOMUint8Array> initData, ExceptionState&);
    static void webkitGenerateKeyRequest(HTMLMediaElement&, const String& keySystem, ExceptionState&);
    static void webkitAddKey(HTMLMediaElement&, const String& keySystem, PassRefPtr<DOMUint8Array> key, PassRefPtr<DOMUint8Array> initData, const String& sessionId, ExceptionState&);
    static void webkitAddKey(HTMLMediaElement&, const String& keySystem, PassRefPtr<DOMUint8Array> key, ExceptionState&);
    static void webkitCancelKeyRequest(HTMLMediaElement&, const String& keySystem, const String& sessionId, ExceptionState&);

    DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(webkitkeyadded);
    DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(webkitkeyerror);
    DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(webkitkeymessage);
    DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(webkitneedkey);

    // encrypted media extensions (WD)
    static MediaKeys* mediaKeys(HTMLMediaElement&);
    static ScriptPromise setMediaKeys(ScriptState*, HTMLMediaElement&, MediaKeys*);
    DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(encrypted);

    // WebMediaPlayerEncryptedMediaClient methods
    void keyAdded(const WebString& keySystem, const WebString& sessionId) final;
    void keyError(const WebString& keySystem, const WebString& sessionId, WebMediaPlayerEncryptedMediaClient::MediaKeyErrorCode, unsigned short systemCode) final;
    void keyMessage(const WebString& keySystem, const WebString& sessionId, const unsigned char* message, unsigned messageLength, const WebURL& defaultURL) final;
    void encrypted(WebEncryptedMediaInitDataType, const unsigned char* initData, unsigned initDataLength) final;
    void didBlockPlaybackWaitingForKey() final;
    void didResumePlaybackBlockedForKey() final;
    WebContentDecryptionModule* contentDecryptionModule();

    static HTMLMediaElementEncryptedMedia& from(HTMLMediaElement&);
    static const char* supplementName();

    ~HTMLMediaElementEncryptedMedia();

    DECLARE_VIRTUAL_TRACE();

private:
    friend class SetMediaKeysHandler;

    HTMLMediaElementEncryptedMedia(HTMLMediaElement&);
    void generateKeyRequest(WebMediaPlayer*, const String& keySystem, PassRefPtr<DOMUint8Array> initData, ExceptionState&);
    void addKey(WebMediaPlayer*, const String& keySystem, PassRefPtr<DOMUint8Array> key, PassRefPtr<DOMUint8Array> initData, const String& sessionId, ExceptionState&);
    void cancelKeyRequest(WebMediaPlayer*, const String& keySystem, const String& sessionId, ExceptionState&);

    // EventTarget
    bool setAttributeEventListener(const AtomicString& eventType, PassRefPtrWillBeRawPtr<EventListener>);
    EventListener* getAttributeEventListener(const AtomicString& eventType);

    // Currently we have both EME v0.1b and EME WD implemented in media element.
    // But we do not want to support both at the same time. The one used first
    // will be supported. Use |m_emeMode| to track this selection.
    // FIXME: Remove EmeMode once EME v0.1b support is removed. See crbug.com/249976.
    enum EmeMode { EmeModeNotSelected, EmeModePrefixed, EmeModeUnprefixed };

    // check (and set if necessary) the encrypted media extensions (EME) mode
    // (v0.1b or WD). Returns whether the mode is allowed and successfully set.
    bool setEmeMode(EmeMode);

    RawPtrWillBeMember<HTMLMediaElement> m_mediaElement;
    EmeMode m_emeMode;

    bool m_isWaitingForKey;

    PersistentWillBeMember<MediaKeys> m_mediaKeys;
};

} // namespace blink

#endif
