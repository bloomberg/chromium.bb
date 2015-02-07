// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

part of bindings;

class MessageHeader {
  static const int kSimpleMessageSize = 16;
  static const int kSimpleMessageNumFields = 2;
  static const int kMessageWithRequestIdSize = 24;
  static const int kMessageWithRequestIdNumFields = 3;
  static const int kMessageTypeOffset = DataHeader.kHeaderSize;
  static const int kMessageFlagsOffset = kMessageTypeOffset + 4;
  static const int kMessageRequestIdOffset = kMessageFlagsOffset + 4;
  static const int kMessageExpectsResponse = 1 << 0;
  static const int kMessageIsResponse = 1 << 1;

  DataHeader _header;
  int type;
  int flags;
  int requestId;

  static bool mustHaveRequestId(int flags) =>
      (flags & (kMessageExpectsResponse | kMessageIsResponse)) != 0;

  MessageHeader(this.type) :
    _header = new DataHeader(kSimpleMessageSize, kSimpleMessageNumFields),
    flags = 0,
    requestId = 0;

  MessageHeader.withRequestId(this.type, this.flags, this.requestId) :
    _header = new DataHeader(
        kMessageWithRequestIdSize, kMessageWithRequestIdNumFields);

  MessageHeader.fromMessage(Message message) {
    var decoder = new Decoder(message);
    _header = decoder.decodeDataHeader();
    if (_header.size < kSimpleMessageSize) {
      throw new MojoCodecError(
          'Incorrect message size. Got: ${_header.size} '
          'wanted $kSimpleMessageSize');
    }
    type = decoder.decodeUint32(kMessageTypeOffset);
    flags = decoder.decodeUint32(kMessageFlagsOffset);
    if (mustHaveRequestId(flags)) {
      if (_header.size < kMessageWithRequestIdSize) {
        throw new MojoCodecError(
            'Incorrect message size. Got: ${_header.size} '
            'wanted $kMessageWithRequestIdSize');
      }
      requestId = decoder.decodeUint64(kMessageRequestIdOffset);
    } else {
      requestId = 0;
    }
  }

  int get size => _header.size;
  bool get hasRequestId => mustHaveRequestId(flags);

  void encode(Encoder encoder) {
    encoder.encodeDataHeader(_header);
    encoder.encodeUint32(type, kMessageTypeOffset);
    encoder.encodeUint32(flags, kMessageFlagsOffset);
    if (hasRequestId) {
      encoder.encodeUint64(requestId, kMessageRequestIdOffset);
    }
  }

  ServiceMessage get serviceMessage => new ServiceMessage(this);

  String toString() => "MessageHeader($_header, $type, $flags, $requestId)";

  bool validateHeaderFlags(expectedFlags) =>
      (flags & (kMessageExpectsResponse | kMessageIsResponse)) == expectedFlags;

  bool validateHeader(int expectedType, int expectedFlags) =>
      (type == expectedType) && validateHeaderFlags(expectedFlags);

  static void _validateDataHeader(DataHeader dataHeader) {
    if (dataHeader.numFields < kSimpleMessageNumFields) {
      throw 'Incorrect number of fields, expecting at least '
            '$kSimpleMessageNumFields, but got: ${dataHeader.numFields}.';
    }
    if (dataHeader.size < kSimpleMessageSize) {
      throw 'Incorrect message size, expecting at least $kSimpleMessageSize, '
            'but got: ${dataHeader.size}';
    }
    if ((dataHeader.numFields == kSimpleMessageSize) &&
        (dataHeader.size != kSimpleMessageSize)) {
      throw 'Incorrect message size for a message with $kSimpleMessageNumFields'
            ' fields, expecting $kSimpleMessageSize, '
            'but got ${dataHeader.size}';
    }
    if ((dataHeader.numFields == kMessageWithRequestIdNumFields) &&
        (dataHeader.size != kMessageWithRequestIdSize)) {
      throw 'Incorrect message size for a message with '
            '$kMessageWithRequestIdNumFields fields, expecting '
            '$kMessageWithRequestIdSize, but got ${dataHeader.size}';
    }
  }
}


class Message {
  final ByteData buffer;
  final List<core.MojoHandle> handles;
  Message(this.buffer, this.handles);
  String toString() =>
      "Message(numBytes=${buffer.lengthInBytes}, numHandles=${handles.length})";
}


class ServiceMessage extends Message {
  final MessageHeader header;
  Message _payload;

  ServiceMessage(Message message, this.header)
      : super(message.buffer, message.handles);

  ServiceMessage.fromMessage(Message message)
      : this(message, new MessageHeader.fromMessage(message));

  Message get payload {
    if (_payload == null) {
      var truncatedBuffer = new ByteData.view(buffer.buffer, header.size);
      _payload = new Message(truncatedBuffer, handles);
    }
    return _payload;
  }

  String toString() => "ServiceMessage($header, $_payload)";
}
