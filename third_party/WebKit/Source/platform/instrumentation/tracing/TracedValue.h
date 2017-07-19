// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TracedValue_h
#define TracedValue_h

#include "base/trace_event/trace_event_argument.h"
#include "platform/PlatformExport.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

// Thin wrapper around base::trace_event::TracedValue.
class PLATFORM_EXPORT TracedValue final
    : public base::trace_event::ConvertableToTraceFormat {
  WTF_MAKE_NONCOPYABLE(TracedValue);

 public:
  ~TracedValue();

  static std::unique_ptr<TracedValue> Create();

  void EndDictionary();
  void EndArray();

  void SetInteger(const char* name, int value);
  void SetDouble(const char* name, double value);
  void SetBoolean(const char* name, bool value);
  void SetString(const char* name, const String& value);
  void BeginArray(const char* name);
  void BeginDictionary(const char* name);

  void SetIntegerWithCopiedName(const char* name, int value);
  void SetDoubleWithCopiedName(const char* name, double value);
  void SetBooleanWithCopiedName(const char* name, bool value);
  void SetStringWithCopiedName(const char* name, const String& value);
  void BeginArrayWithCopiedName(const char* name);
  void BeginDictionaryWithCopiedName(const char* name);

  void PushInteger(int);
  void PushDouble(double);
  void PushBoolean(bool);
  void PushString(const String&);
  void BeginArray();
  void BeginDictionary();

  String ToString() const;

 private:
  TracedValue();

  // ConvertableToTraceFormat

  void AppendAsTraceFormat(std::string*) const final;
  void EstimateTraceMemoryOverhead(
      base::trace_event::TraceEventMemoryOverhead*) final;

  base::trace_event::TracedValue traced_value_;
};

}  // namespace blink

#endif  // TracedValue_h
