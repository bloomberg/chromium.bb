// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/isolated_context.h"

#include "base/file_path.h"
#include "base/basictypes.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/stringprintf.h"

namespace fileapi {

namespace {

FilePath::StringType GetRegisterNameForPath(const FilePath& path) {
  // If it's not a root path simply return a base name.
  if (path.DirName() != path)
    return path.BaseName().value();

#if defined(FILE_PATH_USES_DRIVE_LETTERS)
  FilePath::StringType name;
  for (size_t i = 0;
        i < path.value().size() && !FilePath::IsSeparator(path.value()[i]);
        ++i) {
    if (path.value()[i] == L':') {
      name.append(L"_drive");
      break;
    }
    name.append(1, path.value()[i]);
  }
  return name;
#else
  return FILE_PATH_LITERAL("<root>");
#endif
}

}

static base::LazyInstance<IsolatedContext>::Leaky g_isolated_context =
    LAZY_INSTANCE_INITIALIZER;

IsolatedContext::FileInfo::FileInfo() {}
IsolatedContext::FileInfo::FileInfo(
    const std::string& name, const FilePath& path)
    : name(name), path(path) {}

IsolatedContext::FileInfoSet::FileInfoSet() {}
IsolatedContext::FileInfoSet::~FileInfoSet() {}

std::string IsolatedContext::FileInfoSet::AddPath(
    const FilePath& path) {
  FilePath::StringType name = GetRegisterNameForPath(path);
  std::string utf8name = FilePath(name).AsUTF8Unsafe();
  bool inserted = fileset_.insert(FileInfo(utf8name, path)).second;
  if (!inserted) {
    int suffix = 1;
    std::string basepart = FilePath(name).RemoveExtension().AsUTF8Unsafe();
    std::string ext = FilePath(FilePath(name).Extension()).AsUTF8Unsafe();
    while (!inserted) {
      utf8name = base::StringPrintf("%s (%d)", basepart.c_str(), suffix++);
      if (!ext.empty())
        utf8name.append(ext);
      inserted = fileset_.insert(FileInfo(utf8name, path)).second;
    }
  }
  return utf8name;
}

bool IsolatedContext::FileInfoSet::AddPathWithName(
    const FilePath& path, const std::string& name) {
  return fileset_.insert(FileInfo(name, path)).second;
}

// static
IsolatedContext* IsolatedContext::GetInstance() {
  return g_isolated_context.Pointer();
}

std::string IsolatedContext::RegisterFileSystem(const FileInfoSet& files) {
  base::AutoLock locker(lock_);
  std::string filesystem_id = GetNewFileSystemId();
  // Stores name to fullpath map, as we store the name as a key in
  // the filesystem's toplevel entries.
  FileSet toplevels;
  for (std::set<FileInfo>::const_iterator iter = files.fileset().begin();
       iter != files.fileset().end();
       ++iter) {
    const FileInfo& info = *iter;
    // The given path should not contain any '..' and should be absolute.
    if (info.path.ReferencesParent() || !info.path.IsAbsolute())
      continue;

    // Register the basename -> fullpath map. (We only expose the basename
    // part to the user scripts)
    FilePath fullpath = info.path.NormalizePathSeparators();
    const bool inserted = toplevels.insert(
        FileInfo(info.name, fullpath)).second;
    DCHECK(inserted);
  }

  // TODO(kinuko): we may not want to register the file system if there're
  // no valid paths in the given file set.

  toplevel_map_[filesystem_id] = toplevels;

  // Each file system is created with refcount == 0.
  ref_counts_[filesystem_id] = 0;

  return filesystem_id;
}

std::string IsolatedContext::RegisterFileSystemForFile(
    const FilePath& path,
    std::string* register_name) {
  FileInfoSet files;
  if (register_name && !register_name->empty()) {
    const bool added = files.AddPathWithName(path, *register_name);
    DCHECK(added);
  } else {
    std::string name = files.AddPath(path);
    if (register_name)
      register_name->assign(name);
  }
  return RegisterFileSystem(files);
}

void IsolatedContext::RevokeFileSystem(const std::string& filesystem_id) {
  base::AutoLock locker(lock_);
  RevokeWithoutLocking(filesystem_id);
}

void IsolatedContext::AddReference(const std::string& filesystem_id) {
  base::AutoLock locker(lock_);
  DCHECK(ref_counts_.find(filesystem_id) != ref_counts_.end());
  ref_counts_[filesystem_id]++;
}

void IsolatedContext::RemoveReference(const std::string& filesystem_id) {
  base::AutoLock locker(lock_);
  // This could get called for non-existent filesystem if it has been
  // already deleted by RevokeFileSystem.
  if (ref_counts_.find(filesystem_id) == ref_counts_.end())
    return;
  DCHECK(ref_counts_[filesystem_id] > 0);
  if (--ref_counts_[filesystem_id] == 0)
    RevokeWithoutLocking(filesystem_id);
}

bool IsolatedContext::CrackIsolatedPath(const FilePath& virtual_path,
                                        std::string* filesystem_id,
                                        FileInfo* root_info,
                                        FilePath* platform_path) const {
  DCHECK(filesystem_id);
  DCHECK(platform_path);

  // This should not contain any '..' references.
  if (virtual_path.ReferencesParent())
    return false;

  // The virtual_path should comprise <filesystem_id> and <relative_path> parts.
  std::vector<FilePath::StringType> components;
  virtual_path.GetComponents(&components);
  if (components.size() < 1)
    return false;

  base::AutoLock locker(lock_);
  std::string fsid = FilePath(components[0]).MaybeAsASCII();
  if (fsid.empty())
    return false;
  IDToFileSet::const_iterator found_toplevels = toplevel_map_.find(fsid);
  if (found_toplevels == toplevel_map_.end())
    return false;
  *filesystem_id = fsid;
  if (components.size() == 1) {
    platform_path->clear();
    return true;
  }
  // components[1] should be a name of the dropped paths.
  FileSet::const_iterator found = found_toplevels->second.find(
      FileInfo(FilePath(components[1]).AsUTF8Unsafe(), FilePath()));
  if (found == found_toplevels->second.end())
    return false;
  if (root_info)
    *root_info = *found;
  FilePath path = found->path;
  for (size_t i = 2; i < components.size(); ++i)
    path = path.Append(components[i]);
  *platform_path = path;
  return true;
}

bool IsolatedContext::GetRegisteredFileInfo(
    const std::string& filesystem_id, std::vector<FileInfo>* files) const {
  DCHECK(files);
  base::AutoLock locker(lock_);
  IDToFileSet::const_iterator found = toplevel_map_.find(filesystem_id);
  if (found == toplevel_map_.end())
    return false;
  files->assign(found->second.begin(), found->second.end());
  return true;
}

FilePath IsolatedContext::CreateVirtualRootPath(
    const std::string& filesystem_id) const {
  return FilePath().AppendASCII(filesystem_id);
}

IsolatedContext::IsolatedContext() {
}

IsolatedContext::~IsolatedContext() {
}

void IsolatedContext::RevokeWithoutLocking(
    const std::string& filesystem_id) {
  toplevel_map_.erase(filesystem_id);
  ref_counts_.erase(filesystem_id);
}

std::string IsolatedContext::GetNewFileSystemId() const {
  // Returns an arbitrary random string which must be unique in the map.
  uint32 random_data[4];
  std::string id;
  do {
    base::RandBytes(random_data, sizeof(random_data));
    id = base::HexEncode(random_data, sizeof(random_data));
  } while (toplevel_map_.find(id) != toplevel_map_.end());
  return id;
}

}  // namespace fileapi
