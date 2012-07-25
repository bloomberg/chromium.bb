// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/isolated_context.h"

#include "base/file_path.h"
#include "base/basictypes.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
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

bool IsolatedContext::FileInfoSet::AddPath(
    const FilePath& path, std::string* registered_name) {
  // The given path should not contain any '..' and should be absolute.
  if (path.ReferencesParent() || !path.IsAbsolute())
    return false;
  FilePath::StringType name = GetRegisterNameForPath(path);
  std::string utf8name = FilePath(name).AsUTF8Unsafe();
  FilePath normalized_path = path.NormalizePathSeparators();
  bool inserted = fileset_.insert(FileInfo(utf8name, normalized_path)).second;
  if (!inserted) {
    int suffix = 1;
    std::string basepart = FilePath(name).RemoveExtension().AsUTF8Unsafe();
    std::string ext = FilePath(FilePath(name).Extension()).AsUTF8Unsafe();
    while (!inserted) {
      utf8name = base::StringPrintf("%s (%d)", basepart.c_str(), suffix++);
      if (!ext.empty())
        utf8name.append(ext);
      inserted = fileset_.insert(FileInfo(utf8name, normalized_path)).second;
    }
  }
  if (registered_name)
    *registered_name = utf8name;
  return true;
}

bool IsolatedContext::FileInfoSet::AddPathWithName(
    const FilePath& path, const std::string& name) {
  // The given path should not contain any '..' and should be absolute.
  if (path.ReferencesParent() || !path.IsAbsolute())
    return false;
  return fileset_.insert(FileInfo(name, path.NormalizePathSeparators())).second;
}

//--------------------------------------------------------------------------

IsolatedContext::Instance::Instance(FileSystemType type,
                                    const FileInfo& file_info)
    : type_(type),
      file_info_(file_info),
      ref_counts_(0) {}

IsolatedContext::Instance::Instance(const std::set<FileInfo>& dragged_files)
    : type_(kFileSystemTypeDragged),
      dragged_files_(dragged_files),
      ref_counts_(0) {}

IsolatedContext::Instance::~Instance() {}

bool IsolatedContext::Instance::ResolvePathForName(const std::string& name,
                                                   FilePath* path) {
  if (type_ != kFileSystemTypeDragged) {
    *path = file_info_.path;
    return file_info_.name == name;
  }
  std::set<FileInfo>::const_iterator found = dragged_files_.find(
      FileInfo(name, FilePath()));
  if (found == dragged_files_.end())
    return false;
  *path = found->path;
  return true;
}

//--------------------------------------------------------------------------

// static
IsolatedContext* IsolatedContext::GetInstance() {
  return g_isolated_context.Pointer();
}

std::string IsolatedContext::RegisterDraggedFileSystem(
    const FileInfoSet& files) {
  base::AutoLock locker(lock_);
  std::string filesystem_id = GetNewFileSystemId();
  instance_map_[filesystem_id] = new Instance(files.fileset());
  return filesystem_id;
}

std::string IsolatedContext::RegisterFileSystemForPath(
    FileSystemType type,
    const FilePath& path,
    std::string* register_name) {
  DCHECK(!path.ReferencesParent() && path.IsAbsolute());
  std::string name;
  if (register_name && !register_name->empty()) {
    name = *register_name;
  } else {
    name = FilePath(GetRegisterNameForPath(path)).AsUTF8Unsafe();
    if (register_name)
      register_name->assign(name);
  }

  base::AutoLock locker(lock_);
  std::string filesystem_id = GetNewFileSystemId();
  instance_map_[filesystem_id] = new Instance(type, FileInfo(name, path));
  return filesystem_id;
}

void IsolatedContext::RevokeFileSystem(const std::string& filesystem_id) {
  base::AutoLock locker(lock_);
  IDToInstance::iterator found = instance_map_.find(filesystem_id);
  if (found == instance_map_.end())
    return;
  delete found->second;
  instance_map_.erase(found);
}

void IsolatedContext::AddReference(const std::string& filesystem_id) {
  base::AutoLock locker(lock_);
  DCHECK(instance_map_.find(filesystem_id) != instance_map_.end());
  instance_map_[filesystem_id]->AddRef();
}

void IsolatedContext::RemoveReference(const std::string& filesystem_id) {
  base::AutoLock locker(lock_);
  // This could get called for non-existent filesystem if it has been
  // already deleted by RevokeFileSystem.
  IDToInstance::iterator found = instance_map_.find(filesystem_id);
  if (found == instance_map_.end())
    return;
  DCHECK(found->second->ref_counts() > 0);
  found->second->RemoveRef();
  if (found->second->ref_counts() == 0) {
    delete found->second;
    instance_map_.erase(found);
  }
}

bool IsolatedContext::CrackIsolatedPath(const FilePath& virtual_path,
                                        std::string* filesystem_id,
                                        FileSystemType* type,
                                        FilePath* path) const {
  DCHECK(filesystem_id);
  DCHECK(path);

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
  IDToInstance::const_iterator found_instance = instance_map_.find(fsid);
  if (found_instance == instance_map_.end())
    return false;
  *filesystem_id = fsid;
  if (type)
    *type = found_instance->second->type();
  if (components.size() == 1) {
    path->clear();
    return true;
  }
  // components[1] should be a name of the registered paths.
  FilePath cracked_path;
  std::string name = FilePath(components[1]).AsUTF8Unsafe();
  if (!found_instance->second->ResolvePathForName(name, &cracked_path))
    return false;
  for (size_t i = 2; i < components.size(); ++i)
    cracked_path = cracked_path.Append(components[i]);
  *path = cracked_path;
  return true;
}

bool IsolatedContext::GetDraggedFileInfo(
    const std::string& filesystem_id, std::vector<FileInfo>* files) const {
  DCHECK(files);
  base::AutoLock locker(lock_);
  IDToInstance::const_iterator found = instance_map_.find(filesystem_id);
  if (found == instance_map_.end() ||
      found->second->type() != kFileSystemTypeDragged)
    return false;
  files->assign(found->second->dragged_files().begin(),
                found->second->dragged_files().end());
  return true;
}

bool IsolatedContext::GetRegisteredPath(
    const std::string& filesystem_id, FilePath* path) const {
  DCHECK(path);
  base::AutoLock locker(lock_);
  IDToInstance::const_iterator found = instance_map_.find(filesystem_id);
  if (found == instance_map_.end() ||
      found->second->type() == kFileSystemTypeDragged)
    return false;
  *path = found->second->file_info().path;
  return true;
}

FilePath IsolatedContext::CreateVirtualRootPath(
    const std::string& filesystem_id) const {
  return FilePath().AppendASCII(filesystem_id);
}

IsolatedContext::IsolatedContext() {
}

IsolatedContext::~IsolatedContext() {
  STLDeleteContainerPairSecondPointers(instance_map_.begin(),
                                       instance_map_.end());
}

std::string IsolatedContext::GetNewFileSystemId() const {
  // Returns an arbitrary random string which must be unique in the map.
  uint32 random_data[4];
  std::string id;
  do {
    base::RandBytes(random_data, sizeof(random_data));
    id = base::HexEncode(random_data, sizeof(random_data));
  } while (instance_map_.find(id) != instance_map_.end());
  return id;
}

}  // namespace fileapi
