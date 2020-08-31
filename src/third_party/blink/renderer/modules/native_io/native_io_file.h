// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_NATIVE_IO_NATIVE_IO_FILE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_NATIVE_IO_NATIVE_IO_FILE_H_

#include <memory>

#include "base/files/file.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/mojom/native_io/native_io.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace blink {

class DOMSharedArrayBuffer;
class ExecutionContext;
class ScriptPromiseResolver;
class ScriptState;

class NativeIOFile final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  NativeIOFile(base::File backing_file,
               HeapMojoRemote<mojom::blink::NativeIOFileHost> backend_file,
               ExecutionContext*);

  NativeIOFile(const NativeIOFile&) = delete;
  NativeIOFile& operator=(const NativeIOFile&) = delete;

  // Needed because of the mojo::Remote<mojom::blink::NativeIOFile>.
  ~NativeIOFile() override;

  ScriptPromise close(ScriptState*);
  ScriptPromise read(ScriptState*,
                     MaybeShared<DOMArrayBufferView> buffer,
                     uint64_t file_offset,
                     ExceptionState&);
  ScriptPromise write(ScriptState*,
                      MaybeShared<DOMArrayBufferView> buffer,
                      uint64_t file_offset,
                      ExceptionState&);

  // GarbageCollected
  void Trace(Visitor* visitor) override;

 private:
  // Data accessed on the threads that do file I/O.
  //
  // Instances are allocated on the PartitionAlloc heap. Instances are initially
  // constructed on Blink's main thread, or on a worker thread. Afterwards,
  // instances are only accessed on dedicated threads that do blocking file I/O.
  struct FileState;

  // Called when the mojo backend disconnects.
  void OnBackendDisconnect();

  // Runs any queued close operation.
  void DispatchQueuedClose();

  // Performs the file I/O part of close().
  static void DoClose(
      CrossThreadPersistent<NativeIOFile> native_io_file,
      CrossThreadPersistent<ScriptPromiseResolver> resolver,
      NativeIOFile::FileState* file_state,
      scoped_refptr<base::SequencedTaskRunner> file_task_runner);
  // Performs the post file-I/O part of close(), on the main thread.
  void DidClose(CrossThreadPersistent<ScriptPromiseResolver> resolver);

  // Performs the file I/O part of read().
  static void DoRead(
      CrossThreadPersistent<NativeIOFile> native_io_file,
      CrossThreadPersistent<ScriptPromiseResolver> resolver,
      CrossThreadPersistent<DOMSharedArrayBuffer> read_buffer_keepalive,
      NativeIOFile::FileState* file_state,
      scoped_refptr<base::SequencedTaskRunner> file_task_runner,
      char* read_buffer,
      uint64_t file_offset,
      int read_size);
  // Performs the post file-I/O part of read(), on the main thread.
  void DidRead(CrossThreadPersistent<ScriptPromiseResolver> resolver,
               int read_bytes,
               base::File::Error read_error);

  // Performs the file I/O part of write().
  static void DoWrite(
      CrossThreadPersistent<NativeIOFile> native_io_file,
      CrossThreadPersistent<ScriptPromiseResolver> resolver,
      CrossThreadPersistent<DOMSharedArrayBuffer> write_data_keepalive,
      NativeIOFile::FileState* file_state,
      scoped_refptr<base::SequencedTaskRunner> file_task_runner,
      const char* write_data,
      uint64_t file_offset,
      int write_size);
  // Performs the post file-I/O part of write(), on the main thread.
  void DidWrite(CrossThreadPersistent<ScriptPromiseResolver> resolver,
                int written_bytes,
                base::File::Error write_error);

  // Kicks off closing the file from the main thread.
  void CloseBackingFile();

  // True when an I/O operation other than close is underway.
  //
  // Checked before kicking off any I/O operation except for close(). This
  // ensures that at most one I/O operation is underway at any given time.
  bool io_pending_ = false;

  // Non-null when a close() I/O is queued behind another I/O operation.
  //
  // Set when close() is called while another I/O operation is underway. Cleared
  // when the queued close() operation is queued.
  Member<ScriptPromiseResolver> queued_close_resolver_;

  // Set to true when close() is called, or when the backend goes away.
  bool closed_ = false;

  // See NativeIO::FileState, declared above.
  const std::unique_ptr<FileState> file_state_;

  // Schedules resolving Promises with file I/O results.
  const scoped_refptr<base::SequencedTaskRunner> resolver_task_runner_;

  // Mojo pipe that holds the renderer's lock on the file.
  HeapMojoRemote<mojom::blink::NativeIOFileHost> backend_file_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_NATIVE_IO_NATIVE_IO_FILE_H_
