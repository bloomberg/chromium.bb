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

#ifndef DataObjectItem_h
#define DataObjectItem_h

#include "core/CoreExport.h"
#include "core/fileapi/File.h"
#include "platform/SharedBuffer.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/Forward.h"

namespace blink {

class CORE_EXPORT DataObjectItem
    : public GarbageCollectedFinalized<DataObjectItem> {
 public:
  enum ItemKind { kStringKind, kFileKind };

  static DataObjectItem* CreateFromString(const String& type,
                                          const String& data);
  static DataObjectItem* CreateFromFile(File*);
  // File with non-empty filesystem ID can be converted into FileEntry by using
  // webkitGetAsEntry.
  static DataObjectItem* CreateFromFileWithFileSystemId(
      File*,
      const String& file_system_id);
  static DataObjectItem* CreateFromURL(const String& url, const String& title);
  static DataObjectItem* CreateFromHTML(const String& html,
                                        const KURL& base_url);
  static DataObjectItem* CreateFromSharedBuffer(
      scoped_refptr<SharedBuffer>,
      const KURL&,
      const String& file_extension,
      const AtomicString& content_disposition);
  static DataObjectItem* CreateFromPasteboard(const String& type,
                                              uint64_t sequence_number);

  ItemKind Kind() const { return kind_; }
  String GetType() const { return type_; }
  String GetAsString() const;
  File* GetAsFile() const;

  // Used to support legacy DataTransfer APIs and renderer->browser
  // serialization.
  scoped_refptr<SharedBuffer> GetSharedBuffer() const { return shared_buffer_; }
  String FilenameExtension() const { return filename_extension_; }
  String Title() const { return title_; }
  KURL BaseURL() const { return base_url_; }
  bool IsFilename() const;

  bool HasFileSystemId() const;
  String FileSystemId() const;

  void Trace(blink::Visitor*);

 private:
  enum DataSource {
    kPasteboardSource,
    kInternalSource,
  };

  DataObjectItem(ItemKind, const String& type);
  DataObjectItem(ItemKind, const String& type, uint64_t sequence_number);

  DataSource source_;
  ItemKind kind_;
  String type_;

  String data_;
  Member<File> file_;
  scoped_refptr<SharedBuffer> shared_buffer_;
  // Optional metadata. Currently used for URL, HTML, and dragging files in.
  String filename_extension_;
  String title_;
  KURL base_url_;

  uint64_t sequence_number_;  // Only valid when m_source == PasteboardSource
  String file_system_id_;     // Only valid when m_file is backed by FileEntry.
};

}  // namespace blink

#endif  // DataObjectItem_h
