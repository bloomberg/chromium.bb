// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_MANAGER_IMPL_H_
#define CONTENT_BROWSER_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_MANAGER_IMPL_H_

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/native_file_system/file_system_chooser.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/cpp/bindings/strong_binding_set.h"
#include "storage/browser/fileapi/file_system_url.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_manager.mojom.h"

namespace storage {
class FileSystemContext;
class FileSystemOperationRunner;
}  // namespace storage

namespace content {
class NativeFileSystemFileHandleImpl;
class NativeFileSystemDirectoryHandleImpl;
class NativeFileSystemTransferTokenImpl;

// This is the browser side implementation of the
// NativeFileSystemManager mojom interface. This is the main entry point for
// the native file system API in the browser process.Instances of this class are
// owned by StoragePartitionImpl.
//
// This class owns all the NativeFileSystemFileHandleImpl,
// NativeFileSystemDirectoryHandleImpl and NativeFileSystemTransferTokenImpl
// instances for a specific storage partition.
//
// This class is not thread safe, it is constructed on the UI thread, and after
// that all methods should be called on the IO thread.
class CONTENT_EXPORT NativeFileSystemManagerImpl
    : public base::RefCountedThreadSafe<NativeFileSystemManagerImpl,
                                        BrowserThread::DeleteOnIOThread>,
      public blink::mojom::NativeFileSystemManager {
 public:
  struct BindingContext {
    BindingContext(const url::Origin& origin, int process_id, int frame_id)
        : origin(origin), process_id(process_id), frame_id(frame_id) {}
    url::Origin origin;
    int process_id;
    int frame_id;
  };

  NativeFileSystemManagerImpl(
      scoped_refptr<storage::FileSystemContext> context,
      scoped_refptr<ChromeBlobStorageContext> blob_context);

  void BindRequest(const BindingContext& binding_context,
                   blink::mojom::NativeFileSystemManagerRequest request);

  // blink::mojom::NativeFileSystemManager:
  void GetSandboxedFileSystem(GetSandboxedFileSystemCallback callback) override;
  void ChooseEntries(
      blink::mojom::ChooseFileSystemEntryType type,
      std::vector<blink::mojom::ChooseFileSystemEntryAcceptsOptionPtr> accepts,
      bool include_accepts_all,
      ChooseEntriesCallback callback) override;

  // Creates a new NativeFileSystemFileHandleImpl for a given url. Assumes the
  // passed in URL is valid and represents a file.
  blink::mojom::NativeFileSystemFileHandlePtr CreateFileHandle(
      const BindingContext& binding_context,
      const storage::FileSystemURL& url,
      storage::IsolatedContext::ScopedFSHandle file_system);

  // Creates a new NativeFileSystemDirectoryHandleImpl for a given url. Assumes
  // the passed in URL is valid and represents a directory.
  blink::mojom::NativeFileSystemDirectoryHandlePtr CreateDirectoryHandle(
      const BindingContext& context,
      const storage::FileSystemURL& url,
      storage::IsolatedContext::ScopedFSHandle file_system);

  // Creates a new NativeFileSystemEntryPtr from the path to a file. Assumes the
  // passed in path is valid and represents a file.
  blink::mojom::NativeFileSystemEntryPtr CreateFileEntryFromPath(
      const BindingContext& binding_context,
      const base::FilePath& file_path);

  // Creates a new NativeFileSystemEntryPtr from the path to a directory.
  // Assumes the passed in path is valid and represents a directory.
  blink::mojom::NativeFileSystemEntryPtr CreateDirectoryEntryFromPath(
      const BindingContext& binding_context,
      const base::FilePath& directory_path);

  // Create a transfer token for a specific file or directory.
  void CreateTransferToken(
      const NativeFileSystemFileHandleImpl& file,
      blink::mojom::NativeFileSystemTransferTokenRequest request);
  void CreateTransferToken(
      const NativeFileSystemDirectoryHandleImpl& directory,
      blink::mojom::NativeFileSystemTransferTokenRequest request);

  // Given a mojom transfer token, looks up the token in our internal list of
  // valid tokens. Calls the callback with the found token, or nullptr if no
  // valid token was found.
  using ResolvedTokenCallback =
      base::OnceCallback<void(NativeFileSystemTransferTokenImpl*)>;
  void ResolveTransferToken(
      blink::mojom::NativeFileSystemTransferTokenPtr token,
      ResolvedTokenCallback callback);

  storage::FileSystemContext* context() {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    return context_.get();
  }
  storage::BlobStorageContext* blob_context() {
    return blob_context_->context();
  }
  storage::FileSystemOperationRunner* operation_runner();

 private:
  friend struct BrowserThread::DeleteOnThread<BrowserThread::IO>;
  friend class base::DeleteHelper<NativeFileSystemManagerImpl>;
  ~NativeFileSystemManagerImpl() override;

  void DidOpenSandboxedFileSystem(const BindingContext& binding_context,
                                  GetSandboxedFileSystemCallback callback,
                                  const GURL& root,
                                  const std::string& filesystem_name,
                                  base::File::Error result);

  void DidChooseEntries(const BindingContext& binding_context,
                        blink::mojom::ChooseFileSystemEntryType type,
                        ChooseEntriesCallback callback,
                        blink::mojom::NativeFileSystemErrorPtr result,
                        std::vector<base::FilePath> entries);

  void CreateTransferTokenImpl(
      const storage::FileSystemURL& url,
      storage::IsolatedContext::ScopedFSHandle file_system,
      bool is_directory,
      blink::mojom::NativeFileSystemTransferTokenRequest request);
  void TransferTokenConnectionErrorHandler(const base::UnguessableToken& token);
  void DoResolveTransferToken(blink::mojom::NativeFileSystemTransferTokenPtr,
                              ResolvedTokenCallback callback,
                              const base::UnguessableToken& token);

  // Creates a FileSystemURL which corresponds to a FilePath and Origin.
  struct FileSystemURLAndFSHandle {
    storage::FileSystemURL url;
    std::string base_name;
    storage::IsolatedContext::ScopedFSHandle file_system;
  };
  FileSystemURLAndFSHandle CreateFileSystemURLFromPath(
      const url::Origin& origin,
      const base::FilePath& path);

  const scoped_refptr<storage::FileSystemContext> context_;
  const scoped_refptr<ChromeBlobStorageContext> blob_context_;
  std::unique_ptr<storage::FileSystemOperationRunner> operation_runner_;

  // All the mojo bindings for this NativeFileSystemManager itself. Keeps track
  // of associated origin and other state as well to not have to rely on the
  // renderer passing that in, and to be able to do security checks around
  // transferability etc.
  mojo::BindingSet<blink::mojom::NativeFileSystemManager, BindingContext>
      bindings_;

  // All the bindings for file and directory handles that have references to
  // them.
  mojo::StrongBindingSet<blink::mojom::NativeFileSystemFileHandle>
      file_bindings_;
  mojo::StrongBindingSet<blink::mojom::NativeFileSystemDirectoryHandle>
      directory_bindings_;

  // Transfer token bindings are stored in what is effectively a
  // StrongBindingMap. The Binding instances own the implementation, and tokens
  // are removed from this map when the mojo connection is closed.
  using TransferTokenBinding =
      mojo::Binding<blink::mojom::NativeFileSystemTransferToken,
                    mojo::UniquePtrImplRefTraits<
                        blink::mojom::NativeFileSystemTransferToken>>;
  std::map<base::UnguessableToken, TransferTokenBinding> transfer_tokens_;

  base::WeakPtrFactory<NativeFileSystemManagerImpl> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(NativeFileSystemManagerImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_MANAGER_IMPL_H_
