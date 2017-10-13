// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HTMLMediaElementEncryptedMedia_h
#define HTMLMediaElementEncryptedMedia_h

#include "core/dom/events/EventTarget.h"
#include "core/event_type_names.h"
#include "core/html/media/HTMLMediaElement.h"
#include "core/typed_arrays/DOMTypedArray.h"
#include "modules/ModulesExport.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebEncryptedMediaTypes.h"
#include "public/platform/WebMediaPlayerEncryptedMediaClient.h"

namespace blink {

class HTMLMediaElement;
class MediaKeys;
class ScriptPromise;
class ScriptState;
class WebContentDecryptionModule;

class MODULES_EXPORT HTMLMediaElementEncryptedMedia final
    : public GarbageCollectedFinalized<HTMLMediaElementEncryptedMedia>,
      public Supplement<HTMLMediaElement>,
      public WebMediaPlayerEncryptedMediaClient {
  USING_GARBAGE_COLLECTED_MIXIN(HTMLMediaElementEncryptedMedia);

 public:
  static MediaKeys* mediaKeys(HTMLMediaElement&);
  static ScriptPromise setMediaKeys(ScriptState*,
                                    HTMLMediaElement&,
                                    MediaKeys*);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(encrypted);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(waitingforkey);

  // WebMediaPlayerEncryptedMediaClient methods
  void Encrypted(WebEncryptedMediaInitDataType,
                 const unsigned char* init_data,
                 unsigned init_data_length) final;
  void DidBlockPlaybackWaitingForKey() final;
  void DidResumePlaybackBlockedForKey() final;
  WebContentDecryptionModule* ContentDecryptionModule();

  static HTMLMediaElementEncryptedMedia& From(HTMLMediaElement&);
  static const char* SupplementName();

  ~HTMLMediaElementEncryptedMedia();

  DECLARE_VIRTUAL_TRACE();

 private:
  friend class SetMediaKeysHandler;

  HTMLMediaElementEncryptedMedia(HTMLMediaElement&);

  // EventTarget
  bool SetAttributeEventListener(const AtomicString& event_type,
                                 EventListener*);
  EventListener* GetAttributeEventListener(const AtomicString& event_type);

  Member<HTMLMediaElement> media_element_;

  // Internal values specified by the EME spec:
  // http://w3c.github.io/encrypted-media/#idl-def-HTMLMediaElement
  // The following internal values are added to the HTMLMediaElement:
  // - waiting for key, which shall have a boolean value
  // - attaching media keys, which shall have a boolean value
  bool is_waiting_for_key_;
  bool is_attaching_media_keys_;

  Member<MediaKeys> media_keys_;
};

}  // namespace blink

#endif
