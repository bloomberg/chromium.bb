/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef RTCLegacyStatsReport_h
#define RTCLegacyStatsReport_h

#include "platform/bindings/ScriptWrappable.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/StringHash.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class RTCLegacyStatsReport final
    : public GarbageCollectedFinalized<RTCLegacyStatsReport>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static RTCLegacyStatsReport* Create(const String& id,
                                      const String& type,
                                      double timestamp);

  double timestamp() const { return timestamp_; }
  String id() { return id_; }
  String type() { return type_; }
  String stat(const String& name) { return stats_.at(name); }
  Vector<String> names() const;

  void AddStatistic(const String& name, const String& value);

  void Trace(blink::Visitor* visitor) {}

 private:
  RTCLegacyStatsReport(const String& id, const String& type, double timestamp);

  String id_;
  String type_;
  double timestamp_;
  HashMap<String, String> stats_;
};

}  // namespace blink

#endif  // RTCLegacyStatsReport_h
