// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/nfc/nfc_constants.h"

namespace blink {

const char kNfcJsonMimePostfix[] = "+json";
const char kNfcJsonMimePrefix[] = "application/";
const char kNfcJsonMimeType[] = "application/json";
const char kNfcJsonTextMimeType[] = "text/json";
const char kNfcOpaqueMimeType[] = "application/octet-stream";
const char kNfcPlainTextMimeType[] = "text/plain";
const char kNfcPlainTextMimePrefix[] = "text/";
const char kNfcProtocolHttps[] = "https";
const char kNfcCharSetUTF8[] = ";charset=UTF-8";

// Error messages.
const char kNfcNotAllowed[] = "NFC operation not allowed.";
const char kNfcNotSupported[] =
    "No NFC adapter or cannot establish connection.";
const char kNfcNotReadable[] = "NFC is not enabled.";
const char kNfcTextRecordTypeError[] =
    "The data for 'text' NDEFRecords must be of String.";
const char kNfcSetIdError[] = "Cannot set WebNFC Id.";
const char kNfcTextRecordMediaTypeError[] =
    "Invalid media type for 'text' record.";
const char kNfcUrlRecordTypeError[] =
    "The data for 'url' NDEFRecord must be of String type.";
const char kNfcUrlRecordParseError[] = "Cannot parse data for 'url' record.";
const char kNfcJsonRecordNoDataError[] =
    "The data for 'json' NDEFRecord is missing.";
const char kNfcJsonRecordStringifyError[] =
    "Cannot stringify data for 'json' record.";
const char kNfcJsonRecordMediaTypeError[] =
    "Invalid media type for 'json' record.";
const char kNfcOpaqueRecordTypeError[] =
    "The data for 'opaque' NDEFRecord must be of ArrayBuffer type.";
const char kNfcRecordTypeError[] = "Unknown NDEFRecord type.";
const char kNfcRecordDataError[] = "Nonempty NDEFRecord must have data.";
const char kNfcRecordError[] = "Invalid NDEFRecordType was provided.";
const char kNfcMsgTypeError[] = "Invalid NDEFMessageSource type was provided.";
const char kNfcEmptyMsg[] = "Empty NDEFMessage was provided.";
const char kNfcInvalidMsg[] = "Invalid NFC message was provided.";
const char kNfcMsgConvertError[] = "Cannot convert NDEFMessage.";
const char kNfcMsgMaxSizeError[] =
    "NDEFMessage exceeds maximum supported size.";
const char kNfcUrlPatternError[] = "Invalid URL pattern was provided.";
const char kNfcInvalidPushTimeout[] =
    "Invalid NFCPushOptions.timeout value was provided.";
const char kNfcWatchIdNotFound[] = "Provided watch id cannot be found.";
const char kNfcAccessInNonTopFrame[] =
    "NFC interfaces are only avaliable in a top-level browsing context";
const char kNfcCancelled[] = "The NFC operation was cancelled.";
const char kNfcTimeout[] = "NFC operation has timed out.";
const char kNfcUnknownError[] = "An unknown NFC error has occurred.";
const char kNfcDataTransferError[] = "NFC data transfer error has occurred.";
const char kNfcNoModificationAllowed[] = "NFC operation cannot be cancelled.";

}  // namespace blink
