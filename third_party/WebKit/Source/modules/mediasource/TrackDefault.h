// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TrackDefault_h
#define TrackDefault_h

#include "bindings/core/v8/ScriptValue.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class ExceptionState;
class ScriptState;

class TrackDefault final : public GarbageCollectedFinalized<TrackDefault>,
                           public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static const AtomicString& AudioKeyword();
  static const AtomicString& VideoKeyword();
  static const AtomicString& TextKeyword();

  static TrackDefault* Create(const AtomicString& type,
                              const String& language,
                              const String& label,
                              const Vector<String>& kinds,
                              const String& byte_stream_track_id,
                              ExceptionState&);

  virtual ~TrackDefault();

  // Implement the IDL
  AtomicString type() const { return type_; }
  String byteStreamTrackID() const { return byte_stream_track_id_; }
  String language() const { return language_; }
  String label() const { return label_; }
  ScriptValue kinds(ScriptState*) const;

  DEFINE_INLINE_TRACE() {}

 private:
  TrackDefault(const AtomicString& type,
               const String& language,
               const String& label,
               const Vector<String>& kinds,
               const String& byte_stream_track_id);

  const AtomicString type_;
  const String byte_stream_track_id_;
  const String language_;
  const String label_;
  const Vector<String> kinds_;
};

}  // namespace blink

#endif  // TrackDefault_h
