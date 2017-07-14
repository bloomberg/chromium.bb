/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/webmidi/MIDIOutput.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/UseCounter.h"
#include "core/timing/DOMWindowPerformance.h"
#include "core/timing/Performance.h"
#include "media/midi/midi_service.mojom-blink.h"
#include "modules/webmidi/MIDIAccess.h"

using midi::mojom::PortState;

namespace blink {

namespace {

double Now(ExecutionContext* context) {
  LocalDOMWindow* window = context ? context->ExecutingWindow() : nullptr;
  Performance* performance =
      window ? DOMWindowPerformance::performance(*window) : nullptr;
  return performance ? performance->now() : 0.0;
}

class MessageValidator {
 public:
  static bool Validate(DOMUint8Array* array,
                       ExceptionState& exception_state,
                       bool sysex_enabled) {
    MessageValidator validator(array);
    return validator.Process(exception_state, sysex_enabled);
  }

 private:
  MessageValidator(DOMUint8Array* array)
      : data_(array->Data()), length_(array->length()), offset_(0) {}

  bool Process(ExceptionState& exception_state, bool sysex_enabled) {
    while (!IsEndOfData() && AcceptRealTimeMessages()) {
      if (!IsStatusByte()) {
        exception_state.ThrowTypeError("Running status is not allowed " +
                                       GetPositionString());
        return false;
      }
      if (IsEndOfSysex()) {
        exception_state.ThrowTypeError(
            "Unexpected end of system exclusive message " +
            GetPositionString());
        return false;
      }
      if (IsReservedStatusByte()) {
        exception_state.ThrowTypeError("Reserved status is not allowed " +
                                       GetPositionString());
        return false;
      }
      if (IsSysex()) {
        if (!sysex_enabled) {
          exception_state.ThrowDOMException(
              kInvalidAccessError,
              "System exclusive message is not allowed " + GetPositionString());
          return false;
        }
        if (!AcceptCurrentSysex()) {
          if (IsEndOfData())
            exception_state.ThrowTypeError(
                "System exclusive message is not ended by end of system "
                "exclusive message.");
          else
            exception_state.ThrowTypeError(
                "System exclusive message contains a status byte " +
                GetPositionString());
          return false;
        }
      } else {
        if (!AcceptCurrentMessage()) {
          if (IsEndOfData())
            exception_state.ThrowTypeError("Message is incomplete.");
          else
            exception_state.ThrowTypeError("Unexpected status byte " +
                                           GetPositionString());
          return false;
        }
      }
    }
    return true;
  }

 private:
  bool IsEndOfData() { return offset_ >= length_; }
  bool IsSysex() { return data_[offset_] == 0xf0; }
  bool IsSystemMessage() { return data_[offset_] >= 0xf0; }
  bool IsEndOfSysex() { return data_[offset_] == 0xf7; }
  bool IsRealTimeMessage() { return data_[offset_] >= 0xf8; }
  bool IsStatusByte() { return data_[offset_] & 0x80; }
  bool IsReservedStatusByte() {
    return data_[offset_] == 0xf4 || data_[offset_] == 0xf5 ||
           data_[offset_] == 0xf9 || data_[offset_] == 0xfd;
  }

  bool AcceptRealTimeMessages() {
    for (; !IsEndOfData(); offset_++) {
      if (IsRealTimeMessage() && !IsReservedStatusByte())
        continue;
      return true;
    }
    return false;
  }

  bool AcceptCurrentSysex() {
    DCHECK(IsSysex());
    for (offset_++; !IsEndOfData(); offset_++) {
      if (IsReservedStatusByte())
        return false;
      if (IsRealTimeMessage())
        continue;
      if (IsEndOfSysex()) {
        offset_++;
        return true;
      }
      if (IsStatusByte())
        return false;
    }
    return false;
  }

  bool AcceptCurrentMessage() {
    DCHECK(IsStatusByte());
    DCHECK(!IsSysex());
    DCHECK(!IsReservedStatusByte());
    DCHECK(!IsRealTimeMessage());
    DCHECK(!IsEndOfSysex());
    static const int kChannelMessageLength[7] = {
        3, 3, 3, 3, 2, 2, 3};  // for 0x8*, 0x9*, ..., 0xe*
    static const int kSystemMessageLength[7] = {
        2, 3, 2, 0, 0, 1, 0};  // for 0xf1, 0xf2, ..., 0xf7
    size_t length = IsSystemMessage()
                        ? kSystemMessageLength[data_[offset_] - 0xf1]
                        : kChannelMessageLength[(data_[offset_] >> 4) - 8];
    offset_++;
    DCHECK_GT(length, 0UL);
    if (length == 1)
      return true;
    for (size_t count = 1; !IsEndOfData(); offset_++) {
      if (IsReservedStatusByte())
        return false;
      if (IsRealTimeMessage())
        continue;
      if (IsStatusByte())
        return false;
      if (++count == length) {
        offset_++;
        return true;
      }
    }
    return false;
  }

  String GetPositionString() {
    return "at index " + String::Number(offset_) + " (" +
           String::Number(data_[offset_]) + ").";
  }

  const unsigned char* data_;
  const size_t length_;
  size_t offset_;
};

}  // namespace

MIDIOutput* MIDIOutput::Create(MIDIAccess* access,
                               unsigned port_index,
                               const String& id,
                               const String& manufacturer,
                               const String& name,
                               const String& version,
                               PortState state) {
  DCHECK(access);
  return new MIDIOutput(access, port_index, id, manufacturer, name, version,
                        state);
}

MIDIOutput::MIDIOutput(MIDIAccess* access,
                       unsigned port_index,
                       const String& id,
                       const String& manufacturer,
                       const String& name,
                       const String& version,
                       PortState state)
    : MIDIPort(access, id, manufacturer, name, kTypeOutput, version, state),
      port_index_(port_index) {}

MIDIOutput::~MIDIOutput() {}

void MIDIOutput::send(NotShared<DOMUint8Array> array,
                      double timestamp,
                      ExceptionState& exception_state) {
  DCHECK(array);

  if (timestamp == 0.0)
    timestamp = Now(GetExecutionContext());

  UseCounter::Count(*ToDocument(GetExecutionContext()),
                    WebFeature::kMIDIOutputSend);

  // Implicit open. It does nothing if the port is already opened.
  // This should be performed even if |array| is invalid.
  open();

  if (MessageValidator::Validate(array.View(), exception_state,
                                 midiAccess()->sysexEnabled())) {
    if (IsOpening()) {
      pending_data_.push_back(std::make_pair(Vector<uint8_t>(), timestamp));
      pending_data_.back().first.Append(array.View()->Data(),
                                        array.View()->length());
    } else {
      midiAccess()->SendMIDIData(port_index_, array.View()->Data(),
                                 array.View()->length(), timestamp);
    }
  }
}

void MIDIOutput::send(Vector<unsigned> unsigned_data,
                      double timestamp,
                      ExceptionState& exception_state) {
  if (timestamp == 0.0)
    timestamp = Now(GetExecutionContext());

  DOMUint8Array* array = DOMUint8Array::Create(unsigned_data.size());
  DOMUint8Array::ValueType* const array_data = array->Data();
  const uint32_t array_length = array->length();

  for (size_t i = 0; i < unsigned_data.size(); ++i) {
    if (unsigned_data[i] > 0xff) {
      exception_state.ThrowTypeError("The value at index " + String::Number(i) +
                                     " (" + String::Number(unsigned_data[i]) +
                                     ") is greater than 0xFF.");
      return;
    }
    if (i < array_length)
      array_data[i] = unsigned_data[i] & 0xff;
  }

  send(NotShared<DOMUint8Array>(array), timestamp, exception_state);
}

void MIDIOutput::send(NotShared<DOMUint8Array> data,
                      ExceptionState& exception_state) {
  DCHECK(data);
  send(data, 0.0, exception_state);
}

void MIDIOutput::send(Vector<unsigned> unsigned_data,
                      ExceptionState& exception_state) {
  send(unsigned_data, 0.0, exception_state);
}

void MIDIOutput::DidOpen(bool opened) {
  while (!pending_data_.empty()) {
    if (opened) {
      auto& front = pending_data_.front();
      midiAccess()->SendMIDIData(port_index_, front.first.data(),
                                 front.first.size(), front.second);
    }
    pending_data_.TakeFirst();
  }
}

DEFINE_TRACE(MIDIOutput) {
  MIDIPort::Trace(visitor);
}

}  // namespace blink
