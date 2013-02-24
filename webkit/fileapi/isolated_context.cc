// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/isolated_context.h"

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "webkit/fileapi/file_system_url.h"

namespace fileapi {

namespace {

base::FilePath::StringType GetRegisterNameForPath(const base::FilePath& path) {
  // If it's not a root path simply return a base name.
  if (path.DirName() != path)
    return path.BaseName().value();

#if defined(FILE_PATH_USES_DRIVE_LETTERS)
  base::FilePath::StringType name;
  for (size_t i = 0;
        i < path.value().size() && !base::FilePath::IsSeparator(path.value()[i]);
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

bool IsSinglePathIsolatedFileSystem(FileSystemType type) {
  switch (type) {
    // As of writing dragged file system is the only filesystem
    // which could have multiple top-level paths.
    case kFileSystemTypeDragged:
      return false;

    case kFileSystemTypeUnknown:
      NOTREACHED();
      return true;

    default:
      return true;
  }
  NOTREACHED();
  return true;
}

static base::LazyInstance<IsolatedContext>::Leaky g_isolated_context =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

IsolatedContext::FileInfoSet::FileInfoSet() {}
IsolatedContext::FileInfoSet::~FileInfoSet() {}

bool IsolatedContext::FileInfoSet::AddPath(
    const base::FilePath& path, std::string* registered_name) {
  // The given path should not contain any '..' and should be absolute.
  if (path.ReferencesParent() || !path.IsAbsolute())
    return false;
  base::FilePath::StringType name = GetRegisterNameForPath(path);
  std::string utf8name = base::FilePath(name).AsUTF8Unsafe();
  base::FilePath normalized_path = path.NormalizePathSeparators();
  bool inserted =
      fileset_.insert(MountPointInfo(utf8name, normalized_path)).second;
  if (!inserted) {
    int suffix = 1;
    std::string basepart = base::FilePath(name).RemoveExtension().AsUTF8Unsafe();
    std::string ext = base::FilePath(base::FilePath(name).Extension()).AsUTF8Unsafe();
    while (!inserted) {
      utf8name = base::StringPrintf("%s (%d)", basepart.c_str(), suffix++);
      if (!ext.empty())
        utf8name.append(ext);
      inserted =
          fileset_.insert(MountPointInfo(utf8name, normalized_path)).second;
    }
  }
  if (registered_name)
    *registered_name = utf8name;
  return true;
}

bool IsolatedContext::FileInfoSet::AddPathWithName(
    const base::FilePath& path, const std::string& name) {
  // The given path should not contain any '..' and should be absolute.
  if (path.ReferencesParent() || !path.IsAbsolute())
    return false;
  return fileset_.insert(
      MountPointInfo(name, path.NormalizePathSeparators())).second;
}

//--------------------------------------------------------------------------

class IsolatedContext::Instance {
 public:
  // For a single-path isolated file system, which could be registered by
  // IsolatedContext::RegisterFileSystemForPath().
  // Most of isolated file system contexts should be of this type.
  Instance(FileSystemType type, const MountPointInfo& file_info);

  // For a multi-paths isolated file system.  As of writing only file system
  // type which could have multi-paths is Dragged file system, and
  // could be registered by IsolatedContext::RegisterDraggedFileSystem().
  Instance(FileSystemType type, const std::set<MountPointInfo>& files);

  ~Instance();

  FileSystemType type() const { return type_; }
  const MountPointInfo& file_info() const { return file_info_; }
  const std::set<MountPointInfo>& files() const { return files_; }
  int ref_counts() const { return ref_counts_; }

  void AddRef() { ++ref_counts_; }
  void RemoveRef() { --ref_counts_; }

  bool ResolvePathForName(const std::string& name, base::FilePath* path) const;

  // Returns true if the instance is a single-path instance.
  bool IsSinglePathInstance() const;

 private:
  const FileSystemType type_;

  // For single-path instance.
  const MountPointInfo file_info_;

  // For multiple-path instance (e.g. dragged file system).
  const std::set<MountPointInfo> files_;

  // Reference counts. Note that an isolated filesystem is created with ref==0
  // and will get deleted when the ref count reaches <=0.
  int ref_counts_;

  DISALLOW_COPY_AND_ASSIGN(Instance);
};

IsolatedContext::Instance::Instance(FileSystemType type,
                                    const MountPointInfo& file_info)
    : type_(type),
      file_info_(file_info),
      ref_counts_(0) {
  DCHECK(IsSinglePathIsolatedFileSystem(type_));
}

IsolatedContext::Instance::Instance(FileSystemType type,
                                    const std::set<MountPointInfo>& files)
    : type_(type),
      files_(files),
      ref_counts_(0) {
  DCHECK(!IsSinglePathIsolatedFileSystem(type_));
}

IsolatedContext::Instance::~Instance() {}

bool IsolatedContext::Instance::ResolvePathForName(const std::string& name,
                                                   base::FilePath* path) const {
  if (IsSinglePathIsolatedFileSystem(type_)) {
    *path = file_info_.path;
    return file_info_.name == name;
  }
  std::set<MountPointInfo>::const_iterator found = files_.find(
      MountPointInfo(name, base::FilePath()));
  if (found == files_.end())
    return false;
  *path = found->path;
  return true;
}

bool IsolatedContext::Instance::IsSinglePathInstance() const {
  return IsSinglePathIsolatedFileSystem(type_);
}

//--------------------------------------------------------------------------

// static
IsolatedContext* IsolatedContext::GetInstance() {
  return g_isolated_context.Pointer();
}

// static
bool IsolatedContext::IsIsolatedType(FileSystemType type) {
  return type == kFileSystemTypeIsolated || type == kFileSystemTypeExternal;
}

std::string IsolatedContext::RegisterDraggedFileSystem(
    const FileInfoSet& files) {
  base::AutoLock locker(lock_);
  std::string filesystem_id = GetNewFileSystemId();
  instance_map_[filesystem_id] = new Instance(
      kFileSystemTypeDragged, files.fileset());
  return filesystem_id;
}

std::string IsolatedContext::RegisterFileSystemForPath(
    FileSystemType type,
    const base::FilePath& path_in,
    std::string* register_name) {
  base::FilePath path(path_in.NormalizePathSeparators());
  DCHECK(!path.ReferencesParent() && path.IsAbsolute());
  std::string name;
  if (register_name && !register_name->empty()) {
    name = *register_name;
  } else {
    name = base::FilePath(GetRegisterNameForPath(path)).AsUTF8Unsafe();
    if (register_name)
      register_name->assign(name);
  }

  base::AutoLock locker(lock_);
  std::string filesystem_id = GetNewFileSystemId();
  instance_map_[filesystem_id] = new Instance(type, MountPointInfo(name, path));
  path_to_id_map_[path].insert(filesystem_id);
  return filesystem_id;
}

bool IsolatedContext::HandlesFileSystemMountType(FileSystemType type) const {
  return type == kFileSystemTypeIsolated;
}

bool IsolatedContext::RevokeFileSystem(const std::string& filesystem_id) {
  base::AutoLock locker(lock_);
  return UnregisterFileSystem(filesystem_id);
}

bool IsolatedContext::GetRegisteredPath(
    const std::string& filesystem_id, base::FilePath* path) const {
  DCHECK(path);
  base::AutoLock locker(lock_);
  IDToInstance::const_iterator found = instance_map_.find(filesystem_id);
  if (found == instance_map_.end() || !found->second->IsSinglePathInstance())
    return false;
  *path = found->second->file_info().path;
  return true;
}

bool IsolatedContext::CrackVirtualPath(const base::FilePath& virtual_path,
                                        std::string* id_or_name,
                                        FileSystemType* type,
                                        base::FilePath* path) const {
  DCHECK(id_or_name);
  DCHECK(path);

  // This should not contain any '..' references.
  if (virtual_path.ReferencesParent())
    return false;

  // The virtual_path should comprise <id_or_name> and <relative_path> parts.
  std::vector<base::FilePath::StringType> components;
  virtual_path.GetComponents(&components);
  if (components.size() < 1)
    return false;
  std::vector<base::FilePath::StringType>::iterator component_iter =
      components.begin();
  std::string fsid = base::FilePath(*component_iter++).MaybeAsASCII();
  if (fsid.empty())
    return false;

  base::FilePath cracked_path;
  {
    base::AutoLock locker(lock_);
    IDToInstance::const_iterator found_instance = instance_map_.find(fsid);
    if (found_instance == instance_map_.end())
      return false;
    *id_or_name = fsid;
    const Instance* instance = found_instance->second;
    if (type)
      *type = instance->type();

    if (component_iter == components.end()) {
      // The virtual root case.
      path->clear();
      return true;
    }

    // *component_iter should be a name of the registered path.
    std::string name = base::FilePath(*component_iter++).AsUTF8Unsafe();
    if (!instance->ResolvePathForName(name, &cracked_path))
      return false;
  }

  for (; component_iter != components.end(); ++component_iter)
    cracked_path = cracked_path.Append(*component_iter);
  *path = cracked_path;
  return true;
}

FileSystemURL IsolatedContext::CrackURL(const GURL& url) const {
  FileSystemURL filesystem_url = FileSystemURL(url);
  if (!filesystem_url.is_valid())
    return FileSystemURL();
  return CreateCrackedFileSystemURL(filesystem_url.origin(),
                                    filesystem_url.mount_type(),
                                    filesystem_url.path());
}

FileSystemURL IsolatedContext::CreateCrackedFileSystemURL(
    const GURL& origin,
    FileSystemType type,
    const base::FilePath& path) const {
  if (!HandlesFileSystemMountType(type))
    return FileSystemURL();

  std::string mount_name;
  FileSystemType cracked_type;
  base::FilePath cracked_path;
  if (!CrackVirtualPath(path, &mount_name, &cracked_type, &cracked_path))
    return FileSystemURL();

  return FileSystemURL(origin, type, path,
                       mount_name, cracked_type, cracked_path);
}

void IsolatedContext::RevokeFileSystemByPath(const base::FilePath& path_in) {
  base::AutoLock locker(lock_);
  base::FilePath path(path_in.NormalizePathSeparators());
  PathToID::iterator ids_iter = path_to_id_map_.find(path);
  if (ids_iter == path_to_id_map_.end())
    return;
  std::set<std::string>& ids = ids_iter->second;
  for (std::set<std::string>::iterator iter = ids.begin();
       iter != ids.end(); ++iter) {
    IDToInstance::iterator found = instance_map_.find(*iter);
    if (found != instance_map_.end()) {
      delete found->second;
      instance_map_.erase(found);
    }
  }
  path_to_id_map_.erase(ids_iter);
}

void IsolatedContext::AddReference(const std::string& filesystem_id) {
  base::AutoLock locker(lock_);
  DCHECK(instance_map_.find(filesystem_id) != instance_map_.end());
  instance_map_[filesystem_id]->AddRef();
}

void IsolatedContext::RemoveReference(const std::string& filesystem_id) {
  base::AutoLock locker(lock_);
  // This could get called for non-existent filesystem if it has been
  // already deleted by RevokeFileSystemByPath.
  IDToInstance::iterator found = instance_map_.find(filesystem_id);
  if (found == instance_map_.end())
    return;
  Instance* instance = found->second;
  DCHECK_GT(instance->ref_counts(), 0);
  instance->RemoveRef();
  if (instance->ref_counts() == 0) {
    bool deleted = UnregisterFileSystem(filesystem_id);
    DCHECK(deleted);
  }
}

bool IsolatedContext::GetDraggedFileInfo(
    const std::string& filesystem_id,
    std::vector<MountPointInfo>* files) const {
  DCHECK(files);
  base::AutoLock locker(lock_);
  IDToInstance::const_iterator found = instance_map_.find(filesystem_id);
  if (found == instance_map_.end() ||
      found->second->type() != kFileSystemTypeDragged)
    return false;
  files->assign(found->second->files().begin(),
                found->second->files().end());
  return true;
}

base::FilePath IsolatedContext::CreateVirtualRootPath(
    const std::string& filesystem_id) const {
  return base::FilePath().AppendASCII(filesystem_id);
}

IsolatedContext::IsolatedContext() {
}

IsolatedContext::~IsolatedContext() {
  STLDeleteContainerPairSecondPointers(instance_map_.begin(),
                                       instance_map_.end());
}

bool IsolatedContext::UnregisterFileSystem(const std::string& filesystem_id) {
  lock_.AssertAcquired();
  IDToInstance::iterator found = instance_map_.find(filesystem_id);
  if (found == instance_map_.end())
    return false;
  Instance* instance = found->second;
  if (instance->IsSinglePathInstance()) {
    PathToID::iterator ids_iter = path_to_id_map_.find(
        instance->file_info().path);
    DCHECK(ids_iter != path_to_id_map_.end());
    ids_iter->second.erase(filesystem_id);
    if (ids_iter->second.empty())
      path_to_id_map_.erase(ids_iter);
  }
  delete found->second;
  instance_map_.erase(found);
  return true;
}

std::string IsolatedContext::GetNewFileSystemId() const {
  // Returns an arbitrary random string which must be unique in the map.
  lock_.AssertAcquired();
  uint32 random_data[4];
  std::string id;
  do {
    base::RandBytes(random_data, sizeof(random_data));
    id = base::HexEncode(random_data, sizeof(random_data));
  } while (instance_map_.find(id) != instance_map_.end());
  return id;
}

}  // namespace fileapi
