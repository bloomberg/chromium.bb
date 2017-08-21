/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef FileChooser_h
#define FileChooser_h

#include "platform/FileMetadata.h"
#include "platform/PlatformExport.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/RefCounted.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

// https://w3c.github.io/html-media-capture/#dom-capturefacingmode
enum CaptureFacingMode { CaptureFacingModeUser, CaptureFacingModeEnvironment };

class FileChooser;

struct FileChooserFileInfo {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
  FileChooserFileInfo(const String& path, const String& display_name = String())
      : path(path), display_name(display_name) {}

  FileChooserFileInfo(const KURL& file_system_url, const FileMetadata metadata)
      : file_system_url(file_system_url), metadata(metadata) {}

  // Members for native files.
  const String path;
  const String display_name;

  // Members for file system API files.
  const KURL file_system_url;
  const FileMetadata metadata;
};

struct FileChooserSettings {
  DISALLOW_NEW();
  bool allows_multiple_files;
  bool allows_directory_upload;
  Vector<String> accept_mime_types;
  Vector<String> accept_file_extensions;
  Vector<String> selected_files;
  bool use_media_capture;
  CaptureFacingMode capture;

  // Returns a combined vector of acceptMIMETypes and acceptFileExtensions.
  Vector<String> PLATFORM_EXPORT AcceptTypes() const;
};

class PLATFORM_EXPORT FileChooserClient : public GarbageCollectedMixin {
 public:
  virtual void FilesChosen(const Vector<FileChooserFileInfo>&) = 0;
  virtual ~FileChooserClient();

 protected:
  FileChooser* NewFileChooser(const FileChooserSettings&);

 private:
  RefPtr<FileChooser> chooser_;
};

class PLATFORM_EXPORT FileChooser : public RefCounted<FileChooser> {
 public:
  static PassRefPtr<FileChooser> Create(FileChooserClient*,
                                        const FileChooserSettings&);
  ~FileChooser();

  void DisconnectClient() { client_ = 0; }

  // FIXME: We should probably just pass file paths that could be virtual paths
  // with proper display names rather than passing structs.
  void ChooseFiles(const Vector<FileChooserFileInfo>& files);

  const FileChooserSettings& GetSettings() const { return settings_; }

 private:
  FileChooser(FileChooserClient*, const FileChooserSettings&);

  WeakPersistent<FileChooserClient> client_;
  FileChooserSettings settings_;
};

}  // namespace blink

#endif  // FileChooser_h
