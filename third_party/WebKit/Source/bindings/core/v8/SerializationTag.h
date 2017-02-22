// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SerializationTag_h
#define SerializationTag_h

namespace blink {

// Serialization format is a sequence of tags followed by zero or more data
// arguments.  Tags always take exactly one byte. A serialized stream first
// begins with a complete VersionTag. If the stream does not begin with a
// VersionTag, we assume that the stream is in format 0.

// Tags which are not interpreted by Blink (but instead by V8) are omitted here.

// This format is private to the implementation of SerializedScriptValue. Do not
// rely on it externally. It is safe to persist a SerializedScriptValue as a
// binary blob, but this code should always be used to interpret it.

// WebCoreStrings are read as (length:uint32_t, string:UTF8[length]).
// RawStrings are read as (length:uint32_t, string:UTF8[length]).
// RawUCharStrings are read as
//     (length:uint32_t, string:UChar[length/sizeof(UChar)]).
// RawFiles are read as
//     (path:WebCoreString, url:WebCoreStrng, type:WebCoreString).
// There is a reference table that maps object references (uint32_t) to
// v8::Values.
// Tokens marked with (ref) are inserted into the reference table and given the
// next object reference ID after decoding.
// All tags except InvalidTag, PaddingTag, ReferenceCountTag, VersionTag,
// GenerateFreshObjectTag and GenerateFreshArrayTag push their results to the
// deserialization stack.
// There is also an 'open' stack that is used to resolve circular references.
// Objects or arrays may contain self-references. Before we begin to deserialize
// the contents of these values, they are first given object reference IDs (by
// GenerateFreshObjectTag/GenerateFreshArrayTag); these reference IDs are then
// used with ObjectReferenceTag to tie the recursive knot.
enum SerializationTag {
  MessagePortTag = 'M',  // index:int -> MessagePort. Fills the result with
                         // transferred MessagePort.
  BlobTag = 'b',  // uuid:WebCoreString, type:WebCoreString, size:uint64_t ->
                  // Blob (ref)
  BlobIndexTag = 'i',      // index:int32_t -> Blob (ref)
  FileTag = 'f',           // file:RawFile -> File (ref)
  FileIndexTag = 'e',      // index:int32_t -> File (ref)
  DOMFileSystemTag = 'd',  // type:int32_t, name:WebCoreString,
                           // uuid:WebCoreString -> FileSystem (ref)
  FileListTag =
      'l',  // length:uint32_t, files:RawFile[length] -> FileList (ref)
  FileListIndexTag =
      'L',  // length:uint32_t, files:int32_t[length] -> FileList (ref)
  ImageDataTag = '#',  // width:uint32_t, height:uint32_t,
                       // pixelDataLength:uint32_t, data:byte[pixelDataLength]
                       // -> ImageData (ref)
  ImageBitmapTag = 'g',  // size:uint32_t, data:byte[size] -> ImageBitmap (ref)
  ImageBitmapTransferTag =
      'G',  // index:uint32_t -> ImageBitmap. For ImageBitmap transfer
  OffscreenCanvasTransferTag = 'H',  // index, width, height, id:uint32_t ->
                                     // OffscreenCanvas. For OffscreenCanvas
                                     // transfer
  CryptoKeyTag = 'K',  // subtag:byte, props, usages:uint32_t,
                       // keyDataLength:uint32_t, keyData:byte[keyDataLength]
  //                 If subtag=AesKeyTag:
  //                     props = keyLengthBytes:uint32_t, algorithmId:uint32_t
  //                 If subtag=HmacKeyTag:
  //                     props = keyLengthBytes:uint32_t, hashId:uint32_t
  //                 If subtag=RsaHashedKeyTag:
  //                     props = algorithmId:uint32_t, type:uint32_t,
  //                     modulusLengthBits:uint32_t,
  //                     publicExponentLength:uint32_t,
  //                     publicExponent:byte[publicExponentLength],
  //                     hashId:uint32_t
  //                 If subtag=EcKeyTag:
  //                     props = algorithmId:uint32_t, type:uint32_t,
  //                     namedCurve:uint32_t
  RTCCertificateTag = 'k',   // length:uint32_t, pemPrivateKey:WebCoreString,
                             // pemCertificate:WebCoreString
  CompositorProxyTag =
      'C',  // elementId:uint64_t, bitfields:uint32_t -> CompositorProxy (ref)
  VersionTag = 0xFF  // version:uint32_t -> Uses this as the file version.
};

}  // namespace blink

#endif
