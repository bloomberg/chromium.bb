// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/eventsource/EventSourceParser.h"

#include "core/event_type_names.h"
#include "modules/eventsource/EventSource.h"
#include "platform/wtf/ASCIICType.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/NotFound.h"
#include "platform/wtf/StdLibExtras.h"
#include "platform/wtf/text/TextEncoding.h"
#include "platform/wtf/text/TextEncodingRegistry.h"

namespace blink {

EventSourceParser::EventSourceParser(const AtomicString& last_event_id,
                                     Client* client)
    : id_(last_event_id),
      last_event_id_(last_event_id),
      client_(client),
      codec_(NewTextCodec(UTF8Encoding())) {}

void EventSourceParser::AddBytes(const char* bytes, size_t size) {
  // A line consists of |m_line| followed by
  // |bytes[start..(next line break)]|.
  size_t start = 0;
  const unsigned char kBOM[] = {0xef, 0xbb, 0xbf};
  for (size_t i = 0; i < size && !is_stopped_; ++i) {
    // As kBOM contains neither CR nor LF, we can think BOM and the line
    // break separately.
    if (is_recognizing_bom_ &&
        line_.size() + (i - start) == WTF_ARRAY_LENGTH(kBOM)) {
      Vector<char> line = line_;
      line.Append(&bytes[start], i - start);
      DCHECK_EQ(line.size(), WTF_ARRAY_LENGTH(kBOM));
      is_recognizing_bom_ = false;
      if (memcmp(line.data(), kBOM, sizeof(kBOM)) == 0) {
        start = i;
        line_.clear();
        continue;
      }
    }
    if (is_recognizing_crlf_ && bytes[i] == '\n') {
      // This is the latter part of "\r\n".
      is_recognizing_crlf_ = false;
      ++start;
      continue;
    }
    is_recognizing_crlf_ = false;
    if (bytes[i] == '\r' || bytes[i] == '\n') {
      line_.Append(&bytes[start], i - start);
      ParseLine();
      line_.clear();
      start = i + 1;
      is_recognizing_crlf_ = bytes[i] == '\r';
      is_recognizing_bom_ = false;
    }
  }
  if (is_stopped_)
    return;
  line_.Append(&bytes[start], size - start);
}

void EventSourceParser::ParseLine() {
  if (line_.size() == 0) {
    last_event_id_ = id_;
    // We dispatch an event when seeing an empty line.
    if (!data_.IsEmpty()) {
      DCHECK_EQ(data_[data_.size() - 1], '\n');
      String data = FromUTF8(data_.data(), data_.size() - 1);
      client_->OnMessageEvent(
          event_type_.IsEmpty() ? EventTypeNames::message : event_type_, data,
          last_event_id_);
      data_.clear();
    }
    event_type_ = g_null_atom;
    return;
  }
  size_t field_name_end = line_.Find(':');
  size_t field_value_start;
  if (field_name_end == WTF::kNotFound) {
    field_name_end = line_.size();
    field_value_start = field_name_end;
  } else {
    field_value_start = field_name_end + 1;
    if (field_value_start < line_.size() && line_[field_value_start] == ' ') {
      ++field_value_start;
    }
  }
  size_t field_value_size = line_.size() - field_value_start;
  String field_name = FromUTF8(line_.data(), field_name_end);
  if (field_name == "event") {
    event_type_ = AtomicString(
        FromUTF8(line_.data() + field_value_start, field_value_size));
    return;
  }
  if (field_name == "data") {
    data_.Append(line_.data() + field_value_start, field_value_size);
    data_.push_back('\n');
    return;
  }
  if (field_name == "id") {
    if (!memchr(line_.data() + field_value_start, '\0', field_value_size)) {
      id_ = AtomicString(
          FromUTF8(line_.data() + field_value_start, field_value_size));
    }
    return;
  }
  if (field_name == "retry") {
    bool has_only_digits = true;
    for (size_t i = field_value_start; i < line_.size() && has_only_digits; ++i)
      has_only_digits = IsASCIIDigit(line_[i]);
    if (field_value_start == line_.size()) {
      client_->OnReconnectionTimeSet(EventSource::kDefaultReconnectDelay);
    } else if (has_only_digits) {
      bool ok;
      auto reconnection_time =
          FromUTF8(line_.data() + field_value_start, field_value_size)
              .ToUInt64Strict(&ok);
      if (ok)
        client_->OnReconnectionTimeSet(reconnection_time);
    }
    return;
  }
  // Unrecognized field name. Ignore!
}

String EventSourceParser::FromUTF8(const char* bytes, size_t size) {
  return codec_->Decode(bytes, size, WTF::kDataEOF);
}

DEFINE_TRACE(EventSourceParser) {
  visitor->Trace(client_);
}

}  // namespace blink
