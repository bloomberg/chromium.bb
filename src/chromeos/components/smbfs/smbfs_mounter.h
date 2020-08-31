// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_SMBFS_SMBFS_MOUNTER_H_
#define CHROMEOS_COMPONENTS_SMBFS_SMBFS_MOUNTER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/timer/timer.h"
#include "base/unguessable_token.h"
#include "chromeos/components/smbfs/mojom/smbfs.mojom.h"
#include "chromeos/components/smbfs/smbfs_host.h"
#include "chromeos/disks/disk_mount_manager.h"
#include "chromeos/disks/mount_point.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/invitation.h"
#include "net/base/ip_address.h"

namespace smbfs {

// SmbFsMounter is a helper class that is used to mount an instance of smbfs. It
// performs all the actions necessary to start smbfs and initiate a connection
// to the SMB server.
class COMPONENT_EXPORT(SMBFS) SmbFsMounter {
 public:
  using DoneCallback =
      base::OnceCallback<void(mojom::MountError, std::unique_ptr<SmbFsHost>)>;

  struct KerberosOptions {
    using Source = mojom::KerberosConfig::Source;
    KerberosOptions(Source source, const std::string& identity);
    ~KerberosOptions();

    // Don't allow an invalid options struct to be created.
    KerberosOptions() = delete;

    Source source;
    std::string identity;
  };

  struct MountOptions {
    MountOptions();
    MountOptions(const MountOptions&);
    ~MountOptions();

    // Resolved IP address for share's hostname.
    net::IPAddress resolved_host;

    // Authentication options.
    std::string username;
    std::string workgroup;
    std::string password;
    base::Optional<KerberosOptions> kerberos_options;

    // Allow NTLM authentication to be used.
    bool allow_ntlm = false;

    // Skip attempting to connect to the share.
    bool skip_connect = false;

    // Have smbfs save/restore the share's password.
    bool save_restore_password = false;
    std::string account_hash;
    std::vector<uint8_t> password_salt;
  };

  SmbFsMounter(const std::string& share_path,
               const std::string& mount_dir_name,
               const MountOptions& options,
               SmbFsHost::Delegate* delegate,
               chromeos::disks::DiskMountManager* disk_mount_manager);
  virtual ~SmbFsMounter();

  // Initiate the filesystem mount request, and run |callback| when completed.
  // |callback| is guaranteed not to run after |this| is destroyed.
  // Must only be called once. Virtual for testing.
  virtual void Mount(DoneCallback callback);

 protected:
  // Additional constructors for tests.
  SmbFsMounter();
  SmbFsMounter(const std::string& share_path,
               const std::string& mount_dir_name,
               const MountOptions& options,
               SmbFsHost::Delegate* delegate,
               chromeos::disks::DiskMountManager* disk_mount_manager,
               mojo::Remote<mojom::SmbFsBootstrap> bootstrap);

 private:
  // Callback for MountPoint::Mount().
  void OnMountDone(chromeos::MountError error_code,
                   std::unique_ptr<chromeos::disks::MountPoint> mount_point);

  // Callback for receiving a Mojo bootstrap channel.
  void OnIpcChannel(base::ScopedFD mojo_fd);

  // Callback for bootstrap Mojo MountShare() method.
  void OnMountShare(
      mojo::PendingReceiver<mojom::SmbFsDelegate> delegate_receiver,
      mojom::MountError mount_error,
      mojo::PendingRemote<mojom::SmbFs> smbfs);

  // Mojo disconnection handler.
  void OnMojoDisconnect();

  // Mount timeout handler.
  void OnMountTimeout();

  // Perform cleanup and run |callback_| with |mount_error|.
  void ProcessMountError(mojom::MountError mount_error);

  const std::string share_path_;
  const std::string mount_dir_name_;
  const MountOptions options_;
  SmbFsHost::Delegate* const delegate_;
  chromeos::disks::DiskMountManager* const disk_mount_manager_;
  const base::UnguessableToken token_;
  const std::string mount_url_;
  bool mojo_fd_pending_ = false;

  base::OneShotTimer mount_timer_;
  DoneCallback callback_;

  std::unique_ptr<chromeos::disks::MountPoint> mount_point_;
  mojo::OutgoingInvitation bootstrap_invitation_;
  mojo::Remote<mojom::SmbFsBootstrap> bootstrap_;

  base::WeakPtrFactory<SmbFsMounter> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SmbFsMounter);
};

}  // namespace smbfs

#endif  // CHROMEOS_COMPONENTS_SMBFS_SMBFS_MOUNTER_H_
