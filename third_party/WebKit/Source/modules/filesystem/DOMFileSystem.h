/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef DOMFileSystem_h
#define DOMFileSystem_h

#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/TaskRunnerHelper.h"
#include "modules/ModulesExport.h"
#include "modules/filesystem/DOMFileSystemBase.h"
#include "modules/filesystem/EntriesCallback.h"
#include "platform/bindings/ActiveScriptWrappable.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/WebTraceLocation.h"

namespace blink {

class DirectoryEntry;
class BlobCallback;
class FileEntry;
class FileWriterCallback;

class MODULES_EXPORT DOMFileSystem final
    : public DOMFileSystemBase,
      public ScriptWrappable,
      public ActiveScriptWrappable<DOMFileSystem>,
      public ContextClient {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(DOMFileSystem);

 public:
  static DOMFileSystem* Create(ExecutionContext*,
                               const String& name,
                               FileSystemType,
                               const KURL& root_url);

  // Creates a new isolated file system for the given filesystemId.
  static DOMFileSystem* CreateIsolatedFileSystem(ExecutionContext*,
                                                 const String& filesystem_id);

  DirectoryEntry* root() const;

  // DOMFileSystemBase overrides.
  void AddPendingCallbacks() override;
  void RemovePendingCallbacks() override;
  void ReportError(ErrorCallbackBase*, FileError::ErrorCode) override;

  static void ReportError(ExecutionContext*,
                          ErrorCallbackBase*,
                          FileError::ErrorCode);

  // ScriptWrappable overrides.
  bool HasPendingActivity() const final;

  void CreateWriter(const FileEntry*, FileWriterCallback*, ErrorCallbackBase*);
  void CreateFile(const FileEntry*, BlobCallback*, ErrorCallbackBase*);

  // Schedule a callback. This should not cross threads (should be called on the
  // same context thread).
  static void ScheduleCallback(ExecutionContext* execution_context,
                               WTF::Closure task);

  DECLARE_VIRTUAL_TRACE();

 private:
  DOMFileSystem(ExecutionContext*,
                const String& name,
                FileSystemType,
                const KURL& root_url);

  static String TaskNameForInstrumentation() { return "FileSystem"; }

  int number_of_pending_callbacks_;
  Member<DirectoryEntry> root_entry_;
};

}  // namespace blink

#endif  // DOMFileSystem_h
