// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAINT_PREVIEW_BROWSER_FILE_MANAGER_H_
#define COMPONENTS_PAINT_PREVIEW_BROWSER_FILE_MANAGER_H_

#include "base/containers/flat_set.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/paint_preview/browser/directory_key.h"
#include "components/paint_preview/common/proto/paint_preview.pb.h"
#include "url/gurl.h"

namespace paint_preview {

// FileManager manages paint preview files associated with a root directory.
// Typically the root directory is <profile_dir>/paint_previews/<feature>.
//
// This class is refcounted so scheduled tasks may continue during shutdown.
class FileManager : public base::RefCountedThreadSafe<FileManager> {
 public:
  // Create a file manager for |root_directory|. Top level items in
  // |root_directoy| should be exclusively managed by this class. Items within
  // the subdirectories it creates can be freely modified. All methods will be
  // should be posted to the |io_task_runner| thread.
  FileManager(const base::FilePath& root_directory,
              scoped_refptr<base::SequencedTaskRunner> io_task_runner);

  FileManager(const FileManager&) = delete;
  FileManager& operator=(const FileManager&) = delete;

  scoped_refptr<base::SequencedTaskRunner> GetTaskRunner() {
    return io_task_runner_;
  }

  // Creates a DirectoryKey from keying material.
  // TODO(crbug/1056226): implement collision resolution. At present collisions
  // result in overwriting data.
  DirectoryKey CreateKey(const GURL& url) const;
  DirectoryKey CreateKey(uint64_t tab_id) const;

  // Get statistics about the time of creation and size of artifacts.
  size_t GetSizeOfArtifacts(const DirectoryKey& key) const;
  base::Optional<base::File::Info> GetInfo(const DirectoryKey& key) const;

  // Returns true if the directory for |key| exists.
  bool DirectoryExists(const DirectoryKey& key) const;

  // Returns true if there is a capture for |key| i.e. there exists a proto or
  // zip file.
  bool CaptureExists(const DirectoryKey& key) const;

  // Creates or gets a subdirectory under |root_directory| for |key| and
  // assigns it to |directory|. The directory will be wiped if |clear| is true.
  // Returns a path on success or nullopt on failure. If the directory was
  // compressed then it will be uncompressed automatically.
  base::Optional<base::FilePath> CreateOrGetDirectory(const DirectoryKey& key,
                                                      bool clear) const;

  // Compresses the directory associated with |key|. Returns true on success or
  // if the directory was already compressed. NOTE: an empty directory or a
  // directory containing only empty files/directories will not be compressed.
  bool CompressDirectory(const DirectoryKey& key) const;

  // Deletes artifacts associated with |key| or |keys|.
  void DeleteArtifactSet(const DirectoryKey& key) const;
  void DeleteArtifactSets(const std::vector<DirectoryKey>& keys) const;

  // Deletes all stored paint previews stored in the |root_directory_|.
  void DeleteAll() const;

  // Serializes |proto| to the directory for |key| also compresses is |compress|
  // is true. Returns true on success.
  bool SerializePaintPreviewProto(const DirectoryKey& key,
                                  const PaintPreviewProto& proto,
                                  bool compress) const;

  // Deserializes PaintPreviewProto stored in |key|. Returns nullptr if not
  // found or the proto cannot be parsed.
  std::unique_ptr<PaintPreviewProto> DeserializePaintPreviewProto(
      const DirectoryKey& key) const;

  // Lists the current set of in-use DirectoryKeys.
  base::flat_set<DirectoryKey> ListUsedKeys() const;

 private:
  friend class base::RefCountedThreadSafe<FileManager>;
  ~FileManager();

  enum StorageType {
    kNone = 0,
    kDirectory = 1,
    kZip = 2,
  };

  StorageType GetPathForKey(const DirectoryKey& key,
                            base::FilePath* path) const;

  base::FilePath root_directory_;
  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;
};

}  // namespace paint_preview

#endif  // COMPONENTS_PAINT_PREVIEW_BROWSER_FILE_MANAGER_H_
