// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wtf/text/TextCodec.h"

#include "platform/testing/BlinkFuzzerTestSupport.h"
#include "platform/testing/FuzzedDataProvider.h"
#include "wtf/text/CString.h"
#include "wtf/text/TextEncoding.h"
#include "wtf/text/TextEncodingRegistry.h"

using namespace blink;

// TODO(jsbell): This fuzzes code in wtf/ but has dependencies on platform/,
// so it must live in the latter directory. Once wtf/ moves into platform/wtf
// this should move there as well.

WTF::FlushBehavior kFlushBehavior[] = {WTF::DoNotFlush, WTF::FetchEOF,
                                       WTF::DataEOF};

WTF::UnencodableHandling kUnencodableHandlingOptions[] = {
    WTF::QuestionMarksForUnencodables, WTF::EntitiesForUnencodables,
    WTF::URLEncodedEntitiesForUnencodables,
    WTF::CSSEncodedEntitiesForUnencodables};

class TextCodecFuzzHarness {};
extern "C" int LLVMFuzzerInitialize(int* argc, char*** argv) {
  InitializeBlinkFuzzTest(argc, argv);
  return 0;
}

// Fuzzer for WTF::TextCodec.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // The fuzzer picks 3 bytes off the end of the data to initialize metadata, so
  // abort if the input is smaller than that.
  if (size < 3)
    return 0;

  // Initializes the codec map.
  static const WTF::TextEncoding encoding = WTF::TextEncoding(
#if defined(BIG5)
      "Big5"
#elif defined(EUC_JP)
      "EUC-JP"
#elif defined(EUC_KR)
      "EUC-KR"
#elif defined(GBK)
      "GBK"
#elif defined(IBM866)
      "IBM866"
#elif defined(ISO_2022_JP)
      "ISO-2022-JP"
#elif defined(ISO_8859_10)
      "ISO-8859-10"
#elif defined(ISO_8859_13)
      "ISO-8859-13"
#elif defined(ISO_8859_14)
      "ISO-8859-14"
#elif defined(ISO_8859_15)
      "ISO-8859-15"
#elif defined(ISO_8859_16)
      "ISO-8859-16"
#elif defined(ISO_8859_2)
      "ISO-8859-2"
#elif defined(ISO_8859_3)
      "ISO-8859-3"
#elif defined(ISO_8859_4)
      "ISO-8859-4"
#elif defined(ISO_8859_5)
      "ISO-8859-5"
#elif defined(ISO_8859_6)
      "ISO-8859-6"
#elif defined(ISO_8859_7)
      "ISO-8859-7"
#elif defined(ISO_8859_8)
      "ISO-8859-8"
#elif defined(ISO_8859_8_I)
      "ISO-8859-8-I"
#elif defined(KOI8_R)
      "KOI8-R"
#elif defined(KOI8_U)
      "KOI8-U"
#elif defined(SHIFT_JIS)
      "Shift_JIS"
#elif defined(UTF_16BE)
      "UTF-16BE"
#elif defined(UTF_16LE)
      "UTF-16LE"
#elif defined(UTF_32)
      "UTF-32"
#elif defined(UTF_32BE)
      "UTF-32BE"
#elif defined(UTF_32LE)
      "UTF-32LE"
#elif defined(UTF_8)
      "UTF-8"
#elif defined(GB18030)
      "gb18030"
#elif defined(MACINTOSH)
      "macintosh"
#elif defined(WINDOWS_1250)
      "windows-1250"
#elif defined(WINDOWS_1251)
      "windows-1251"
#elif defined(WINDOWS_1252)
      "windows-1252"
#elif defined(WINDOWS_1253)
      "windows-1253"
#elif defined(WINDOWS_1254)
      "windows-1254"
#elif defined(WINDOWS_1255)
      "windows-1255"
#elif defined(WINDOWS_1256)
      "windows-1256"
#elif defined(WINDOWS_1257)
      "windows-1257"
#elif defined(WINDOWS_1258)
      "windows-1258"
#elif defined(WINDOWS_874)
      "windows-874"
#elif defined(X_MAC_CYRILLIC)
      "x-mac-cyrillic"
#elif defined(X_USER_DEFINED)
      "x-user-defined"
#endif
      "");

  FuzzedDataProvider fuzzedData(data, size);

  // Initialize metadata using the fuzzed data.
  bool stopOnError = fuzzedData.ConsumeBool();
  WTF::UnencodableHandling unencodableHandling =
      fuzzedData.PickValueInArray(kUnencodableHandlingOptions);
  WTF::FlushBehavior flushBehavior =
      fuzzedData.PickValueInArray(kFlushBehavior);

  // Now, use the rest of the fuzzy data to stress test decoding and encoding.
  const CString byteString = fuzzedData.ConsumeRemainingBytes();
  std::unique_ptr<TextCodec> codec = newTextCodec(encoding);

  // Treat as bytes-off-the-wire.
  bool sawError;
  const String decoded = codec->decode(byteString.data(), byteString.length(),
                                       flushBehavior, stopOnError, sawError);

  // Treat as blink 8-bit string (latin1).
  if (size % sizeof(LChar) == 0) {
    std::unique_ptr<TextCodec> codec = newTextCodec(encoding);
    codec->encode(reinterpret_cast<const LChar*>(byteString.data()),
                  byteString.length() / sizeof(LChar), unencodableHandling);
  }

  // Treat as blink 16-bit string (utf-16) if there are an even number of bytes.
  if (size % sizeof(UChar) == 0) {
    std::unique_ptr<TextCodec> codec = newTextCodec(encoding);
    codec->encode(reinterpret_cast<const UChar*>(byteString.data()),
                  byteString.length() / sizeof(UChar), unencodableHandling);
  }

  if (decoded.isNull())
    return 0;

  // Round trip the bytes (aka encode the decoded bytes).
  if (decoded.is8Bit()) {
    codec->encode(decoded.characters8(), decoded.length(), unencodableHandling);
  } else {
    codec->encode(decoded.characters16(), decoded.length(),
                  unencodableHandling);
  }
  return 0;
}
