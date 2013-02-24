// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/external_mount_points.h"

#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/remote_file_system_proxy.h"

namespace {

// Normalizes file path so it has normalized separators and ends with exactly
// one separator. Paths have to be normalized this way for use in
// GetVirtualPath method. Separators cannot be completely stripped, or
// GetVirtualPath could not working in some edge cases.
// For example, /a/b/c(1)/d would be erroneously resolved as c/d if the
// following mount points were registered: "/a/b/c", "/a/b/c(1)". (Note:
// "/a/b/c" < "/a/b/c(1)" < "/a/b/c/").
base::FilePath NormalizeFilePath(const base::FilePath& path) {
  if (path.empty())
    return path;

  base::FilePath::StringType path_str = path.StripTrailingSeparators().value();
  if (!base::FilePath::IsSeparator(path_str[path_str.length() - 1]))
    path_str.append(FILE_PATH_LITERAL("/"));

  return base::FilePath(path_str).NormalizePathSeparators();
}

// Wrapper around ref-counted ExternalMountPoints that will be used to lazily
// create and initialize LazyInstance system ExternalMountPoints.
class SystemMountPointsLazyWrapper {
 public:
  SystemMountPointsLazyWrapper()
      : system_mount_points_(fileapi::ExternalMountPoints::CreateRefCounted()) {
    RegisterDefaultMountPoints();
  }

  ~SystemMountPointsLazyWrapper() {}

  fileapi::ExternalMountPoints* get() {
    return system_mount_points_.get();
  }

 private:
  void RegisterDefaultMountPoints() {
#if defined(OS_CHROMEOS)
    // Add default system mount points.
    system_mount_points_->RegisterFileSystem(
        "archive",
        fileapi::kFileSystemTypeNativeLocal,
        base::FilePath(FILE_PATH_LITERAL("/media/archive")));
    system_mount_points_->RegisterFileSystem(
        "removable",
        fileapi::kFileSystemTypeNativeLocal,
        base::FilePath(FILE_PATH_LITERAL("/media/removable")));
    system_mount_points_->RegisterFileSystem(
        "oem",
        fileapi::kFileSystemTypeRestrictedNativeLocal,
        base::FilePath(FILE_PATH_LITERAL("/usr/share/oem")));
#endif  // defined(OS_CHROMEOS)
  }

  scoped_refptr<fileapi::ExternalMountPoints> system_mount_points_;
};

base::LazyInstance<SystemMountPointsLazyWrapper>::Leaky
    g_external_mount_points = LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace fileapi {

class ExternalMountPoints::Instance {
 public:
  Instance(FileSystemType type,
           const base::FilePath& path,
           RemoteFileSystemProxyInterface* remote_proxy);

  ~Instance();

  FileSystemType type() const { return type_; }
  const base::FilePath& path() const { return path_; }
  RemoteFileSystemProxyInterface* remote_proxy() const {
    return remote_proxy_.get();
  }

 private:
  const FileSystemType type_;
  const base::FilePath path_;

  // For file systems that have a remote file system proxy.
  scoped_refptr<RemoteFileSystemProxyInterface> remote_proxy_;

  DISALLOW_COPY_AND_ASSIGN(Instance);
};

ExternalMountPoints::Instance::Instance(FileSystemType type,
                                        const base::FilePath& path,
                                        RemoteFileSystemProxyInterface* proxy)
    : type_(type),
      path_(path.StripTrailingSeparators()),
      remote_proxy_(proxy) {
  DCHECK(!proxy || (kFileSystemTypeDrive == type_));
}

ExternalMountPoints::Instance::~Instance() {}

//--------------------------------------------------------------------------

// static
ExternalMountPoints* ExternalMountPoints::GetSystemInstance() {
  return g_external_mount_points.Pointer()->get();
}

// static
scoped_refptr<ExternalMountPoints> ExternalMountPoints::CreateRefCounted() {
  return new ExternalMountPoints();
}

bool ExternalMountPoints::RegisterFileSystem(
    const std::string& mount_name,
    FileSystemType type,
    const base::FilePath& path) {
  return RegisterRemoteFileSystem(mount_name, type, NULL, path);
}

bool ExternalMountPoints::RegisterRemoteFileSystem(
    const std::string& mount_name,
    FileSystemType type,
    RemoteFileSystemProxyInterface* remote_proxy,
    const base::FilePath& path_in) {
  base::AutoLock locker(lock_);

  base::FilePath path = NormalizeFilePath(path_in);
  if (!ValidateNewMountPoint(mount_name, path))
    return false;

  instance_map_[mount_name] = new Instance(type, path, remote_proxy);
  if (!path.empty())
    path_to_name_map_.insert(std::make_pair(path, mount_name));
  return true;
}

bool ExternalMountPoints::HandlesFileSystemMountType(
    FileSystemType type) const {
  return type == kFileSystemTypeExternal;
}

bool ExternalMountPoints::RevokeFileSystem(const std::string& mount_name) {
  base::AutoLock locker(lock_);
  NameToInstance::iterator found = instance_map_.find(mount_name);
  if (found == instance_map_.end())
    return false;
  Instance* instance = found->second;
  path_to_name_map_.erase(NormalizeFilePath(instance->path()));
  delete found->second;
  instance_map_.erase(found);
  return true;
}

bool ExternalMountPoints::GetRegisteredPath(
    const std::string& filesystem_id, base::FilePath* path) const {
  DCHECK(path);
  base::AutoLock locker(lock_);
  NameToInstance::const_iterator found = instance_map_.find(filesystem_id);
  if (found == instance_map_.end())
    return false;
  *path = found->second->path();
  return true;
}

bool ExternalMountPoints::CrackVirtualPath(const base::FilePath& virtual_path,
                                           std::string* mount_name,
                                           FileSystemType* type,
                                           base::FilePath* path) const {
  DCHECK(mount_name);
  DCHECK(path);

  // The path should not contain any '..' references.
  if (virtual_path.ReferencesParent())
    return false;

  // The virtual_path should comprise of <mount_name> and <relative_path> parts.
  std::vector<base::FilePath::StringType> components;
  virtual_path.GetComponents(&components);
  if (components.size() < 1)
    return false;

  std::vector<base::FilePath::StringType>::iterator component_iter =
      components.begin();
  std::string maybe_mount_name = base::FilePath(*component_iter++).MaybeAsASCII();
  if (maybe_mount_name.empty())
    return false;

  base::FilePath cracked_path;
  {
    base::AutoLock locker(lock_);
    NameToInstance::const_iterator found_instance =
        instance_map_.find(maybe_mount_name);
    if (found_instance == instance_map_.end())
      return false;

    *mount_name = maybe_mount_name;
    const Instance* instance = found_instance->second;
    if (type)
      *type = instance->type();
    cracked_path = instance->path();
  }

  for (; component_iter != components.end(); ++component_iter)
    cracked_path = cracked_path.Append(*component_iter);
  *path = cracked_path;
  return true;
}

FileSystemURL ExternalMountPoints::CrackURL(const GURL& url) const {
  FileSystemURL filesystem_url = FileSystemURL(url);
  if (!filesystem_url.is_valid())
    return FileSystemURL();
  return CreateCrackedFileSystemURL(filesystem_url.origin(),
                                    filesystem_url.mount_type(),
                                    filesystem_url.path());
}

FileSystemURL ExternalMountPoints::CreateCrackedFileSystemURL(
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

RemoteFileSystemProxyInterface* ExternalMountPoints::GetRemoteFileSystemProxy(
    const std::string& mount_name) const {
  base::AutoLock locker(lock_);
  NameToInstance::const_iterator found = instance_map_.find(mount_name);
  if (found == instance_map_.end())
    return NULL;
  return found->second->remote_proxy();
}

void ExternalMountPoints::AddMountPointInfosTo(
    std::vector<MountPointInfo>* mount_points) const {
  base::AutoLock locker(lock_);
  DCHECK(mount_points);
  for (NameToInstance::const_iterator iter = instance_map_.begin();
       iter != instance_map_.end(); ++iter) {
    mount_points->push_back(MountPointInfo(iter->first, iter->second->path()));
  }
}

bool ExternalMountPoints::GetVirtualPath(const base::FilePath& path_in,
                                         base::FilePath* virtual_path) {
  DCHECK(virtual_path);

  base::AutoLock locker(lock_);

  base::FilePath path = NormalizeFilePath(path_in);
  std::map<base::FilePath, std::string>::reverse_iterator iter(
      path_to_name_map_.upper_bound(path));
  if (iter == path_to_name_map_.rend())
    return false;

  *virtual_path = CreateVirtualRootPath(iter->second);
  if (iter->first == path)
    return true;
  return iter->first.AppendRelativePath(path, virtual_path);
}

base::FilePath ExternalMountPoints::CreateVirtualRootPath(
    const std::string& mount_name) const {
  return base::FilePath().AppendASCII(mount_name);
}

ExternalMountPoints::ExternalMountPoints() {}

ExternalMountPoints::~ExternalMountPoints() {
  STLDeleteContainerPairSecondPointers(instance_map_.begin(),
                                       instance_map_.end());
}

bool ExternalMountPoints::ValidateNewMountPoint(const std::string& mount_name,
                                                const base::FilePath& path) {
  lock_.AssertAcquired();

  // Mount name must not be empty.
  if (mount_name.empty())
    return false;

  // Verify there is no registered mount point with the same name.
  NameToInstance::iterator found = instance_map_.find(mount_name);
  if (found != instance_map_.end())
    return false;

  // Allow empty paths.
  if (path.empty())
    return true;

  // Verify path is legal.
  if (path.ReferencesParent() || !path.IsAbsolute())
    return false;

  // Check there the new path does not overlap with one of the existing ones.
  std::map<base::FilePath, std::string>::reverse_iterator potential_parent(
      path_to_name_map_.upper_bound(path));
  if (potential_parent != path_to_name_map_.rend()) {
    if (potential_parent->first == path ||
        potential_parent->first.IsParent(path)) {
      return false;
    }
  }

  std::map<base::FilePath, std::string>::iterator potential_child =
      path_to_name_map_.upper_bound(path);
  if (potential_child == path_to_name_map_.end())
    return true;
  return !(potential_child->first == path) &&
         !path.IsParent(potential_child->first);
}

ScopedExternalFileSystem::ScopedExternalFileSystem(
    const std::string& mount_name,
    FileSystemType type,
    const base::FilePath& path)
    : mount_name_(mount_name) {
  ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
      mount_name, type, path);
}

base::FilePath ScopedExternalFileSystem::GetVirtualRootPath() const {
  return ExternalMountPoints::GetSystemInstance()->
      CreateVirtualRootPath(mount_name_);
}

ScopedExternalFileSystem::~ScopedExternalFileSystem() {
  ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(mount_name_);
}

}  // namespace fileapi

