// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_PROVIDER_INTERFACE_H_
#define CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_PROVIDER_INTERFACE_H_

#include <memory>
#include <string>

#include "chrome/browser/chromeos/file_system_provider/icon_set.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/file_system_provider_capabilities/file_system_provider_capabilities_handler.h"

class Profile;

namespace chromeos {
namespace file_system_provider {

class ProvidedFileSystemInterface;
class ProviderId;

struct Capabilities {
  Capabilities(bool configurable,
               bool watchable,
               bool multiple_mounts,
               extensions::FileSystemProviderSource source)
      : configurable(configurable),
        watchable(watchable),
        multiple_mounts(multiple_mounts),
        source(source) {}
  Capabilities()
      : configurable(false),
        watchable(false),
        multiple_mounts(false),
        source(extensions::SOURCE_NETWORK) {}

  bool configurable;
  bool watchable;
  bool multiple_mounts;
  extensions::FileSystemProviderSource source;
};

class ProviderInterface {
 public:
  enum class IconSize {
    SIZE_16x16,
    SIZE_32_32,
  };

  virtual ~ProviderInterface() {}

  // Returns a pointer to a created file system.
  virtual std::unique_ptr<ProvidedFileSystemInterface> CreateProvidedFileSystem(
      Profile* profile,
      const ProvidedFileSystemInfo& file_system_info) = 0;

  // Returns the capabilites of the provider.
  virtual const Capabilities& GetCapabilities() const = 0;

  // Returns id of this provider.
  virtual const ProviderId& GetId() const = 0;

  // Returns a user friendly name of this provider.
  virtual const std::string& GetName() const = 0;

  // Returns an icon URL set for the provider.
  virtual const IconSet& GetIconSet() const = 0;

  // Requests mounting a new file system. Returns false if the request could not
  // be created, true otherwise.
  virtual bool RequestMount(Profile* profile) = 0;
};

}  // namespace file_system_provider
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_PROVIDER_INTERFACE_H_
