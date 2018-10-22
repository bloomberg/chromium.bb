// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_INSTALLATION_STATE_H_
#define CHROME_INSTALLER_UTIL_INSTALLATION_STATE_H_

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "chrome/installer/util/app_commands.h"
#include "chrome/installer/util/channel_info.h"

namespace base {
class Version;
}

namespace installer {

class InstallationState;

// A representation of Chrome's state on the machine based on the contents of
// the Windows registry.
// TODO(grt): Pull this out into its own file.
// TODO(grt): Evaluate whether this is still needed. If yes, rename to
// ChromeState or somesuch.
class ProductState {
 public:
  ProductState();
  ~ProductState();

  // Returns true if the product is installed (i.e., the product's Clients key
  // exists and has a "pv" value); false otherwise.
  bool Initialize(bool system_install);

  // Returns the product's channel info (i.e., the Google Update "ap" value).
  const ChannelInfo& channel() const { return channel_; }

  // Returns the path to the product's "setup.exe"; may be empty.
  base::FilePath GetSetupPath() const;

  // Returns the product's version.  This method may only be called on an
  // instance that has been initialized for an installed product.
  const base::Version& version() const;

  // Returns the current version of the product if a new version is awaiting
  // update; may be NULL.  Ownership of a returned value is not passed to the
  // caller.
  const base::Version* old_version() const { return old_version_.get(); }

  // Returns the brand code the product is currently installed with.
  const std::wstring& brand() const { return brand_; }

  // Returns the command to be used to update to the new version that is
  // awaiting update; may be empty.
  const std::wstring& rename_cmd() const { return rename_cmd_; }

  // Returns true and populates |eula_accepted| if the product has such a value;
  // otherwise, returns false and does not modify |eula_accepted|.  Expected
  // values are 0 (false) and 1 (true), although |eula_accepted| is given
  // whatever is found.
  bool GetEulaAccepted(DWORD* eula_accepted) const;

  // Returns true and populates |oem_install| if the product has such a value;
  // otherwise, returns false and does not modify |oem_install|.  Expected
  // value is "1", although |oem_install| is given whatever is found.
  bool GetOemInstall(std::wstring* oem_install) const;

  // Returns true and populates |usagestats| if the product has such a value;
  // otherwise, returns false and does not modify |usagestats|.  Expected values
  // are 0 (false) and 1 (true), although |usagestats| is given whatever is
  // found.
  bool GetUsageStats(DWORD* usagestats) const;

  // True if the "msi" value in the ClientState key is present and non-zero.
  bool is_msi() const { return msi_; }

  // The command to uninstall the product; may be empty.
  const base::CommandLine& uninstall_command() const {
    return uninstall_command_;
  }

  // True if |uninstall_command| contains --multi-install.
  bool is_multi_install() const { return multi_install_; }

  // Returns the set of Google Update commands.
  const AppCommands& commands() const { return commands_; }

  // Returns this object a la operator=().
  ProductState& CopyFrom(const ProductState& other);

  // Clears the state of this object.
  void Clear();

 protected:
  ChannelInfo channel_;
  std::unique_ptr<base::Version> version_;
  std::unique_ptr<base::Version> old_version_;
  std::wstring brand_;
  std::wstring rename_cmd_;
  std::wstring oem_install_;
  base::CommandLine uninstall_command_;
  AppCommands commands_;
  DWORD eula_accepted_;
  DWORD usagestats_;
  bool msi_ : 1;
  bool multi_install_ : 1;
  bool has_eula_accepted_ : 1;
  bool has_oem_install_ : 1;
  bool has_usagestats_ : 1;

 private:
  friend class InstallationState;

  DISALLOW_COPY_AND_ASSIGN(ProductState);
};  // class ProductState

// Encapsulates the state of all products on the system.
// TODO(grt): Rename this to MachineState and put it in its own file.
class InstallationState {
 public:
  InstallationState();

  // Initializes this object with the machine's current state.
  void Initialize();

  // Returns the state of a product or NULL if not installed.
  // Caller does NOT assume ownership of returned pointer.
  const ProductState* GetProductState(bool system_install) const;

  // Returns the state of a product, even one that has not yet been installed.
  // This is useful during first install, when some but not all ProductState
  // information has been written by Omaha. Notably absent from the
  // ProductState returned here are the version numbers. Do NOT try to access
  // the version numbers from a ProductState returned by this method.
  // Caller does NOT assume ownership of returned pointer. This method will
  // never return NULL.
  const ProductState* GetNonVersionedProductState(bool system_install) const;

 protected:
  ProductState user_chrome_;
  ProductState system_chrome_;

 private:
  DISALLOW_COPY_AND_ASSIGN(InstallationState);
};  // class InstallationState

}  // namespace installer

#endif  // CHROME_INSTALLER_UTIL_INSTALLATION_STATE_H_
