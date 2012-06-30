// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_ISOLATED_CONTEXT_H_
#define WEBKIT_FILEAPI_ISOLATED_CONTEXT_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/memory/singleton.h"
#include "base/synchronization/lock.h"
#include "base/lazy_instance.h"
#include "webkit/fileapi/fileapi_export.h"

namespace fileapi {

// Manages isolated filename namespaces.  A namespace is simply defined as a
// set of file paths and corresponding filesystem ID.  This context class is
// a singleton and access to the context is thread-safe (protected with a
// lock).
// Some methods of this class are virtual just for mocking.
class FILEAPI_EXPORT IsolatedContext {
 public:
  // The instance is lazily created per browser process.
  static IsolatedContext* GetInstance();

  // Registers a new file isolated filesystem with the given set of files
  // and returns the new filesystem_id.  The files are registered with their
  // basenames as their keys so that later we can resolve the full paths
  // for the given file name in the isolated filesystem.  We only expose the
  // key and the ID for the newly created filesystem to the renderer for
  // the sake of security.
  //
  // The renderer will be sending filesystem requests with a virtual path like
  // '/<filesystem_id>/<relative_path_from_the_basename_of_dropped_path>'
  // for which we could crack in the browser by calling CrackIsolatedPath to
  // get the full path.
  //
  // For example: if a dropped file has a path like '/a/b/foo' we register
  // the path with the key 'foo' in the newly created filesystem.
  // Later if the context is asked to crack a virtual path like '/<fsid>/foo'
  // it can properly return the original path '/a/b/foo' by looking up the
  // internal mapping.  Similarly if a dropped entry is a directory and its
  // path is like '/a/b/dir' a virtual path like '/<fsid>/dir/foo' can be
  // cracked into '/a/b/dir/foo'.
  //
  // Note that the path in |fileset| that contains '..' or is not an
  // absolute path is skipped and is not registerred.
  std::string RegisterIsolatedFileSystem(const std::set<FilePath>& fileset);

  // Revokes filesystem specified by the given filesystem_id.
  // Note that this revokes the filesystem no matter how many references it has.
  // It is ok to call this on the filesystem that has been already deleted
  // (if its reference count had reached 0).
  void RevokeIsolatedFileSystem(const std::string& filesystem_id);

  // Adds a reference to a filesystem specified by the given filesystem_id.
  void AddReference(const std::string& filesystem_id);

  // Removes a reference to a filesystem specified by the given filesystem_id.
  // If the reference count reaches 0 the isolated context gets destroyed.
  // It is ok to call this on the filesystem that has been already deleted
  // (e.g. by RevokeIsolatedFileSystem).
  void RemoveReference(const std::string& filesystem_id);

  // Cracks the given |virtual_path| (which should look like
  // "/<filesystem_id>/<relative_path>") and populates the |filesystem_id|
  // and |platform_path| if the embedded <filesystem_id> is registerred
  // to this context.  |root_path| is also populated to have the platform
  // root (toplevel) path for the |virtual_path|
  // (i.e. |platform_path| = |root_path| + <relative_path>).
  //
  // Returns false if the given virtual_path or the cracked filesystem_id
  // is not valid.
  //
  // Note that |root_path| and |platform_path| are set to empty paths if
  // |virtual_path| has no <relative_path> part (i.e. pointing to
  // the virtual root).
  bool CrackIsolatedPath(const FilePath& virtual_path,
                         std::string* filesystem_id,
                         FilePath* root_path,
                         FilePath* platform_path) const;

  // Returns a vector of the full paths of the top-level entry paths
  // registered for the |filesystem_id|.  Returns false if the
  // |filesystem_is| is not valid.
  bool GetTopLevelPaths(const std::string& filesystem_id,
                        std::vector<FilePath>* paths) const;

  // Returns the virtual path that looks like /<filesystem_id>/<relative_path>.
  FilePath CreateVirtualPath(const std::string& filesystem_id,
                             const FilePath& relative_path) const;

 private:
  friend struct base::DefaultLazyInstanceTraits<IsolatedContext>;

  // Maps from filesystem id to a path conversion map for top-level entries.
  typedef std::map<FilePath, FilePath> PathMap;
  typedef std::map<std::string, PathMap> IDToPathMap;

  // Obtain an instance of this class via GetInstance().
  IsolatedContext();
  ~IsolatedContext();

  // Removes the given filesystem without locking.
  // (The caller must hold a lock)
  void RevokeWithoutLocking(const std::string& filesystem_id);

  // Returns a new filesystem_id.  Called with lock.
  std::string GetNewFileSystemId() const;

  // This lock needs to be obtained when accessing the toplevel_map_.
  mutable base::Lock lock_;

  // Maps the toplevel entries to the filesystem id.
  IDToPathMap toplevel_map_;

  // Reference counts. Note that an isolated filesystem is created with ref==0.
  // and will get deleted when the ref count reaches <=0.
  std::map<std::string, int> ref_counts_;

  DISALLOW_COPY_AND_ASSIGN(IsolatedContext);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_ISOLATED_CONTEXT_H_
