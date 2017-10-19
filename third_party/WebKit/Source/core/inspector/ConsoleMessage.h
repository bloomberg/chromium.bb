// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ConsoleMessage_h
#define ConsoleMessage_h

#include "core/CoreExport.h"
#include "core/dom/DOMNodeIds.h"
#include "core/inspector/ConsoleTypes.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class LocalFrame;
class SourceLocation;

class CORE_EXPORT ConsoleMessage final
    : public GarbageCollectedFinalized<ConsoleMessage> {
 public:
  // Location must be non-null.
  static ConsoleMessage* Create(MessageSource,
                                MessageLevel,
                                const String& message,
                                std::unique_ptr<SourceLocation>);

  // Shortcut when location is unknown. Captures current location.
  static ConsoleMessage* Create(MessageSource,
                                MessageLevel,
                                const String& message);

  // This method captures current location if available.
  static ConsoleMessage* CreateForRequest(MessageSource,
                                          MessageLevel,
                                          const String& message,
                                          const String& url,
                                          unsigned long request_identifier);

  // This creates message from WorkerMessageSource.
  static ConsoleMessage* CreateFromWorker(MessageLevel,
                                          const String& message,
                                          std::unique_ptr<SourceLocation>,
                                          const String& worker_id);

  ~ConsoleMessage();

  SourceLocation* Location() const;
  unsigned long RequestIdentifier() const;
  double Timestamp() const;
  MessageSource Source() const;
  MessageLevel Level() const;
  const String& Message() const;
  const String& WorkerId() const;
  LocalFrame* Frame() const;
  Vector<DOMNodeId>& Nodes();
  void SetNodes(LocalFrame*, Vector<DOMNodeId> nodes);

  void Trace(blink::Visitor*);

 private:
  ConsoleMessage(MessageSource,
                 MessageLevel,
                 const String& message,
                 std::unique_ptr<SourceLocation>);

  MessageSource source_;
  MessageLevel level_;
  String message_;
  std::unique_ptr<SourceLocation> location_;
  unsigned long request_identifier_;
  double timestamp_;
  String worker_id_;
  WeakMember<LocalFrame> frame_;
  Vector<DOMNodeId> nodes_;
};

}  // namespace blink

#endif  // ConsoleMessage_h
