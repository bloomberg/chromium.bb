/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "core/clipboard/DataObjectItem.h"

#include "core/clipboard/Pasteboard.h"
#include "core/fileapi/Blob.h"
#include "platform/clipboard/ClipboardMimeTypes.h"
#include "platform/network/mime/MIMETypeRegistry.h"
#include "public/platform/Platform.h"
#include "public/platform/WebClipboard.h"

namespace blink {

DataObjectItem* DataObjectItem::CreateFromString(const String& type,
                                                 const String& data) {
  DataObjectItem* item = new DataObjectItem(kStringKind, type);
  item->data_ = data;
  return item;
}

DataObjectItem* DataObjectItem::CreateFromFile(File* file) {
  DataObjectItem* item = new DataObjectItem(kFileKind, file->type());
  item->file_ = file;
  return item;
}

DataObjectItem* DataObjectItem::CreateFromFileWithFileSystemId(
    File* file,
    const String& file_system_id) {
  DataObjectItem* item = new DataObjectItem(kFileKind, file->type());
  item->file_ = file;
  item->file_system_id_ = file_system_id;
  return item;
}

DataObjectItem* DataObjectItem::CreateFromURL(const String& url,
                                              const String& title) {
  DataObjectItem* item = new DataObjectItem(kStringKind, kMimeTypeTextURIList);
  item->data_ = url;
  item->title_ = title;
  return item;
}

DataObjectItem* DataObjectItem::CreateFromHTML(const String& html,
                                               const KURL& base_url) {
  DataObjectItem* item = new DataObjectItem(kStringKind, kMimeTypeTextHTML);
  item->data_ = html;
  item->base_url_ = base_url;
  return item;
}

DataObjectItem* DataObjectItem::CreateFromSharedBuffer(
    scoped_refptr<SharedBuffer> buffer,
    const KURL& source_url,
    const String& filename_extension,
    const AtomicString& content_disposition) {
  DataObjectItem* item = new DataObjectItem(
      kFileKind,
      MIMETypeRegistry::GetWellKnownMIMETypeForExtension(filename_extension));
  item->shared_buffer_ = std::move(buffer);
  item->filename_extension_ = filename_extension;
  // TODO(dcheng): Rename these fields to be more generically named.
  item->title_ = content_disposition;
  item->base_url_ = source_url;
  return item;
}

DataObjectItem* DataObjectItem::CreateFromPasteboard(const String& type,
                                                     uint64_t sequence_number) {
  if (type == kMimeTypeImagePng)
    return new DataObjectItem(kFileKind, type, sequence_number);
  return new DataObjectItem(kStringKind, type, sequence_number);
}

DataObjectItem::DataObjectItem(ItemKind kind, const String& type)
    : source_(kInternalSource), kind_(kind), type_(type), sequence_number_(0) {}

DataObjectItem::DataObjectItem(ItemKind kind,
                               const String& type,
                               uint64_t sequence_number)
    : source_(kPasteboardSource),
      kind_(kind),
      type_(type),
      sequence_number_(sequence_number) {}

File* DataObjectItem::GetAsFile() const {
  if (Kind() != kFileKind)
    return nullptr;

  if (source_ == kInternalSource) {
    if (file_)
      return file_.Get();
    DCHECK(shared_buffer_);
    // FIXME: This code is currently impossible--we never populate
    // m_sharedBuffer when dragging in. At some point though, we may need to
    // support correctly converting a shared buffer into a file.
    return nullptr;
  }

  DCHECK_EQ(source_, kPasteboardSource);
  if (GetType() == kMimeTypeImagePng) {
    WebBlobInfo blob_info = Platform::Current()->Clipboard()->ReadImage(
        WebClipboard::kBufferStandard);
    if (blob_info.size() < 0)
      return nullptr;
    return File::Create(
        "image.png", CurrentTimeMS(),
        BlobDataHandle::Create(blob_info.Uuid(), blob_info.GetType(),
                               blob_info.size()));
  }

  return nullptr;
}

String DataObjectItem::GetAsString() const {
  DCHECK_EQ(kind_, kStringKind);

  if (source_ == kInternalSource)
    return data_;

  DCHECK_EQ(source_, kPasteboardSource);

  WebClipboard::Buffer buffer = Pasteboard::GeneralPasteboard()->GetBuffer();
  String data;
  // This is ugly but there's no real alternative.
  if (type_ == kMimeTypeTextPlain) {
    data = Platform::Current()->Clipboard()->ReadPlainText(buffer);
  } else if (type_ == kMimeTypeTextRTF) {
    data = Platform::Current()->Clipboard()->ReadRTF(buffer);
  } else if (type_ == kMimeTypeTextHTML) {
    WebURL ignored_source_url;
    unsigned ignored;
    data = Platform::Current()->Clipboard()->ReadHTML(
        buffer, &ignored_source_url, &ignored, &ignored);
  } else {
    data = Platform::Current()->Clipboard()->ReadCustomData(buffer, type_);
  }

  return Platform::Current()->Clipboard()->SequenceNumber(buffer) ==
                 sequence_number_
             ? data
             : String();
}

bool DataObjectItem::IsFilename() const {
  // FIXME: https://bugs.webkit.org/show_bug.cgi?id=81261: When we properly
  // support File dragout, we'll need to make sure this works as expected for
  // DragDataChromium.
  return kind_ == kFileKind && file_;
}

bool DataObjectItem::HasFileSystemId() const {
  return kind_ == kFileKind && !file_system_id_.IsEmpty();
}

String DataObjectItem::FileSystemId() const {
  return file_system_id_;
}

void DataObjectItem::Trace(blink::Visitor* visitor) {
  visitor->Trace(file_);
}

}  // namespace blink
