// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_SETUP_SETUP_UTIL_H_
#define CHROME_UPDATER_WIN_SETUP_SETUP_UTIL_H_

#include <guiddef.h>

#include <string>
#include <vector>

#include "base/win/windows_types.h"

class WorkItemList;

namespace base {
class CommandLine;
class FilePath;
}  // namespace base

namespace updater {

enum class UpdaterScope;

bool RegisterWakeTask(const base::CommandLine& run_command, UpdaterScope scope);
void UnregisterWakeTask(UpdaterScope scope);

std::wstring GetComServerClsidRegistryPath(REFCLSID clsid);
std::wstring GetComServerAppidRegistryPath(REFGUID appid);
std::wstring GetComIidRegistryPath(REFIID iid);
std::wstring GetComTypeLibRegistryPath(REFIID iid);

// Returns the resource index for the type library where the interface specified
// by the `iid` is defined. For encapsulation reasons, the updater interfaces
// are segregated in multiple IDL files, which get compiled to multiple type
// libraries. The type libraries are inserted in the compiled binary as
// resources with different resource indexes. The resource index becomes a
// suffix of the path to where the type library exists, such as
// `...\updater.exe\\1`. See the Windows SDK documentation for LoadTypeLib for
// details.
std::wstring GetComTypeLibResourceIndex(REFIID iid);

// Returns the interfaces ids of all interfaces declared in IDL of the updater
// that can be installed side-by-side with other instances of the updater.
std::vector<IID> GetSideBySideInterfaces();

// Returns the interfaces ids of all interfaces declared in IDL of the updater
// that can only be installed for the active instance of the updater.
std::vector<IID> GetActiveInterfaces();

// Returns the CLSIDs of servers that can be installed side-by-side with other
// instances of the updater.
std::vector<CLSID> GetSideBySideServers(UpdaterScope scope);

// Returns the CLSIDs of servers that can only be installed for the active
// instance of the updater.
std::vector<CLSID> GetActiveServers(UpdaterScope scope);

// Helper function that joins two vectors and returns the resultant vector.
template <typename T>
std::vector<T> JoinVectors(const std::vector<T>& vector1,
                           const std::vector<T>& vector2) {
  std::vector<T> joined_vector = vector1;
  joined_vector.insert(joined_vector.end(), vector2.begin(), vector2.end());
  return joined_vector;
}

// Adds work items to `list` to install the interface `iid`.
void AddInstallComInterfaceWorkItems(HKEY root,
                                     const base::FilePath& typelib_path,
                                     GUID iid,
                                     WorkItemList* list);

// Adds work items to `list` to install the server `iid`.
void AddInstallServerWorkItems(HKEY root,
                               CLSID iid,
                               const base::FilePath& executable_path,
                               bool internal_service,
                               WorkItemList* list);

// Adds work items to `list` to install the COM service.
void AddComServiceWorkItems(const base::FilePath& com_service_path,
                            bool internal_service,
                            WorkItemList* list);

// Parses the run time dependency file which contains all dependencies of
// the `updater` target. This file is a text file, where each line of
// text represents a single dependency. Some dependencies are not needed for
// updater to run, and are filtered out from the return value of this function.
std::vector<base::FilePath> ParseFilesFromDeps(const base::FilePath& deps);

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_SETUP_SETUP_UTIL_H_
