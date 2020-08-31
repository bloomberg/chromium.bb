// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/session/arc_vm_client_adapter.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>

#include <set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/optional.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/platform_thread.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/concierge_client.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/dbus/upstart/upstart_client.h"
#include "chromeos/system/statistics_provider.h"
#include "components/arc/arc_features.h"
#include "components/arc/arc_util.h"
#include "components/arc/session/arc_session.h"
#include "components/arc/session/file_system_status.h"
#include "components/version_info/version_info.h"

namespace arc {
namespace {

// The "_2d" in job names below corresponds to "-". Upstart escapes characters
// that aren't valid in D-Bus object paths with underscore followed by its
// ascii code in hex. So "arc_2dcreate_2ddata" becomes "arc-create-data".
constexpr const char kArcCreateDataJobName[] = "arc_2dcreate_2ddata";
constexpr const char kArcKeymasterJobName[] = "arc_2dkeymasterd";
constexpr const char kArcVmServerProxyJobName[] = "arcvm_2dserver_2dproxy";
constexpr const char kArcVmPerBoardFeaturesJobName[] =
    "arcvm_2dper_2dboard_2dfeatures";
constexpr const char kArcVmBootNotificationServerJobName[] =
    "arcvm_2dboot_2dnotification_2dserver";

constexpr const char kCrosSystemPath[] = "/usr/bin/crossystem";
constexpr const char kArcVmBootNotificationServerSocketPath[] =
    "/run/arcvm_boot_notification_server/host.socket";

constexpr int64_t kInvalidCid = -1;

constexpr base::TimeDelta kConnectTimeoutLimit =
    base::TimeDelta::FromSeconds(20);
constexpr base::TimeDelta kConnectSleepDurationInitial =
    base::TimeDelta::FromMilliseconds(100);

base::Optional<base::TimeDelta> g_connect_timeout_limit_for_testing;
base::Optional<base::TimeDelta> g_connect_sleep_duration_initial_for_testing;

chromeos::ConciergeClient* GetConciergeClient() {
  return chromeos::DBusThreadManager::Get()->GetConciergeClient();
}

chromeos::DebugDaemonClient* GetDebugDaemonClient() {
  return chromeos::DBusThreadManager::Get()->GetDebugDaemonClient();
}

std::string GetChromeOsChannelFromLsbRelease() {
  constexpr const char kChromeOsReleaseTrack[] = "CHROMEOS_RELEASE_TRACK";
  constexpr const char kUnknown[] = "unknown";
  const std::string kChannelSuffix = "-channel";

  std::string value;
  base::SysInfo::GetLsbReleaseValue(kChromeOsReleaseTrack, &value);

  if (!base::EndsWith(value, kChannelSuffix, base::CompareCase::SENSITIVE)) {
    LOG(ERROR) << "Unknown ChromeOS channel: \"" << value << "\"";
    return kUnknown;
  }
  return value.erase(value.find(kChannelSuffix), kChannelSuffix.size());
}

std::string MonotonicTimestamp() {
  struct timespec ts;
  const int ret = clock_gettime(CLOCK_BOOTTIME, &ts);
  DPCHECK(ret == 0);
  const int64_t time =
      ts.tv_sec * base::Time::kNanosecondsPerSecond + ts.tv_nsec;
  return base::NumberToString(time);
}

ArcBinaryTranslationType IdentifyBinaryTranslationType(
    const StartParams& start_params) {
  const auto* command_line = base::CommandLine::ForCurrentProcess();
  bool is_houdini_available =
      command_line->HasSwitch(chromeos::switches::kEnableHoudini) ||
      command_line->HasSwitch(chromeos::switches::kEnableHoudini64);
  bool is_ndk_translation_available =
      command_line->HasSwitch(chromeos::switches::kEnableNdkTranslation);

  if (!is_houdini_available && !is_ndk_translation_available)
    return ArcBinaryTranslationType::NONE;

  const bool prefer_ndk_translation =
      !is_houdini_available || start_params.native_bridge_experiment;

  if (is_ndk_translation_available && prefer_ndk_translation)
    return ArcBinaryTranslationType::NDK_TRANSLATION;

  return ArcBinaryTranslationType::HOUDINI;
}

std::vector<std::string> GenerateKernelCmdline(
    const StartParams& start_params,
    const UpgradeParams& upgrade_params,
    const FileSystemStatus& file_system_status,
    bool is_dev_mode,
    bool is_host_on_vm,
    const std::string& channel,
    const std::string& serial_number) {
  DCHECK(!serial_number.empty());

  std::string native_bridge;
  switch (IdentifyBinaryTranslationType(start_params)) {
    case ArcBinaryTranslationType::NONE:
      native_bridge = "0";
      break;
    case ArcBinaryTranslationType::HOUDINI:
      native_bridge = "libhoudini.so";
      break;
    case ArcBinaryTranslationType::NDK_TRANSLATION:
      native_bridge = "libndk_translation.so";
      break;
  }

  std::vector<std::string> result = {
      "androidboot.hardware=bertha",
      "androidboot.container=1",
      base::StringPrintf("androidboot.native_bridge=%s", native_bridge.c_str()),
      base::StringPrintf("androidboot.dev_mode=%d", is_dev_mode),
      base::StringPrintf("androidboot.disable_runas=%d", !is_dev_mode),
      base::StringPrintf("androidboot.host_is_in_vm=%d", is_host_on_vm),
      base::StringPrintf("androidboot.debuggable=%d",
                         file_system_status.is_android_debuggable()),
      base::StringPrintf("androidboot.lcd_density=%d",
                         start_params.lcd_density),
      base::StringPrintf("androidboot.arc_file_picker=%d",
                         start_params.arc_file_picker_experiment),
      base::StringPrintf("androidboot.arc_custom_tabs=%d",
                         start_params.arc_custom_tabs_experiment),
      base::StringPrintf("androidboot.arc_print_spooler=%d",
                         start_params.arc_print_spooler_experiment),
      base::StringPrintf("androidboot.disable_system_default_app=%d",
                         start_params.arc_disable_system_default_app),
      "androidboot.chromeos_channel=" + channel,
      "androidboot.boottime_offset=" + MonotonicTimestamp(),
  };
  // Since we don't do mini VM yet, set not only |start_params| but also
  // |upgrade_params| here for now.
  const std::vector<std::string> upgrade_props =
      GenerateUpgradeProps(upgrade_params, serial_number, "androidboot");
  result.insert(result.end(), upgrade_props.begin(), upgrade_props.end());

  // TODO(niwa): Check if we need to set ro.boot.enable_adb_sideloading for
  // ARCVM.

  // Conditionally sets some properties based on |start_params|.
  switch (start_params.play_store_auto_update) {
    case StartParams::PlayStoreAutoUpdate::AUTO_UPDATE_DEFAULT:
      break;
    case StartParams::PlayStoreAutoUpdate::AUTO_UPDATE_ON:
      result.push_back("androidboot.play_store_auto_update=1");
      break;
    case StartParams::PlayStoreAutoUpdate::AUTO_UPDATE_OFF:
      result.push_back("androidboot.play_store_auto_update=0");
      break;
  }

  return result;
}

vm_tools::concierge::StartArcVmRequest CreateStartArcVmRequest(
    const std::string& user_id_hash,
    uint32_t cpus,
    const base::FilePath& demo_session_apps_path,
    const FileSystemStatus& file_system_status,
    std::vector<std::string> kernel_cmdline) {
  vm_tools::concierge::StartArcVmRequest request;

  request.set_name(kArcVmName);
  request.set_owner_id(user_id_hash);

  request.add_params("root=/dev/vda");
  if (file_system_status.is_host_rootfs_writable() &&
      file_system_status.is_system_image_ext_format()) {
    request.add_params("rw");
  }
  request.add_params("init=/init");

  // TIP: When you want to see all dmesg logs from the Android system processes
  // such as init, uncomment the following line. By default, the guest kernel
  // rate-limits the logging and you might not be able to see all LOGs from
  // them. The logs could be silently dropped. This is useful when modifying
  // init.bertha.rc, for example.
  //
  // request.add_params("printk.devkmsg=on");

  for (auto& entry : kernel_cmdline)
    request.add_params(std::move(entry));

  vm_tools::concierge::VirtualMachineSpec* vm = request.mutable_vm();

  vm->set_kernel(file_system_status.guest_kernel_path().value());

  // Add / as /dev/vda.
  vm->set_rootfs(file_system_status.system_image_path().value());
  request.set_rootfs_writable(file_system_status.is_host_rootfs_writable() &&
                              file_system_status.is_system_image_ext_format());

  // Add /vendor as /dev/vdb.
  vm_tools::concierge::DiskImage* disk_image = request.add_disks();
  disk_image->set_path(file_system_status.vendor_image_path().value());
  disk_image->set_image_type(vm_tools::concierge::DISK_IMAGE_AUTO);
  disk_image->set_writable(false);
  disk_image->set_do_mount(true);

  // Add /vendor as /dev/vdc.
  // TODO(yusukes): Remove /dev/vdc once Android side stops using it.
  disk_image = request.add_disks();
  disk_image->set_path(file_system_status.vendor_image_path().value());
  disk_image->set_image_type(vm_tools::concierge::DISK_IMAGE_AUTO);
  disk_image->set_writable(false);
  disk_image->set_do_mount(true);

  // Add /run/imageloader/.../android_demo_apps.squash as /dev/vdd if needed.
  // TODO(b/144542975): Do this on upgrade instead.
  if (!demo_session_apps_path.empty()) {
    disk_image = request.add_disks();
    disk_image->set_path(demo_session_apps_path.value());
    disk_image->set_image_type(vm_tools::concierge::DISK_IMAGE_AUTO);
    disk_image->set_writable(false);
    disk_image->set_do_mount(true);
  }

  // Add Android fstab.
  request.set_fstab(file_system_status.fstab_path().value());

  // Add cpus.
  request.set_cpus(cpus);

  return request;
}

// Gets a system property managed by crossystem. This function can be called
// only with base::MayBlock().
int GetSystemPropertyInt(const std::string& property) {
  std::string output;
  if (!base::GetAppOutput({kCrosSystemPath, property}, &output))
    return -1;
  int output_int;
  return base::StringToInt(output, &output_int) ? output_int : -1;
}

const sockaddr_un* GetArcVmBootNotificationServerAddress() {
  static struct sockaddr_un address {
    .sun_family = AF_UNIX,
    .sun_path = "/run/arcvm_boot_notification_server/host.socket"
  };
  return &address;
}

// Connects to UDS socket at |kArcVmBootNotificationServerSocketPath|.
// Returns the connected socket fd if successful, or else an invalid fd. This
// function can only be called with base::MayBlock().
base::ScopedFD ConnectToArcVmBootNotificationServer() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  base::ScopedFD fd(socket(AF_UNIX, SOCK_STREAM, 0));
  DCHECK(fd.is_valid());

  if (HANDLE_EINTR(connect(fd.get(),
                           reinterpret_cast<const sockaddr*>(
                               GetArcVmBootNotificationServerAddress()),
                           sizeof(sockaddr_un)))) {
    PLOG(ERROR) << "Unable to connect to "
                << kArcVmBootNotificationServerSocketPath;
    return {};
  }

  return fd;
}

// Connects to arcvm-boot-notification-server to verify that it is listening.
// When this function is called, the server has just started and may not be
// listening on the socket yet, so this function will retry connecting for up
// to 20s, with exponential backoff. This function can only be called with
// base::MayBlock().
bool IsArcVmBootNotificationServerListening() {
  const base::ElapsedTimer timer;
  const base::TimeDelta limit = g_connect_timeout_limit_for_testing
                                    ? *g_connect_timeout_limit_for_testing
                                    : kConnectTimeoutLimit;
  base::TimeDelta sleep_duration =
      g_connect_sleep_duration_initial_for_testing
          ? *g_connect_sleep_duration_initial_for_testing
          : kConnectSleepDurationInitial;

  do {
    if (ConnectToArcVmBootNotificationServer().is_valid())
      return true;

    LOG(ERROR) << "Retrying to connect to boot notification server in "
               << sleep_duration;
    base::PlatformThread::Sleep(sleep_duration);
    sleep_duration *= 2;
  } while (timer.Elapsed() < limit);
  return false;
}

// Sends upgrade props to arcvm-boot-notification-server over socket at
// |kArcVmBootNotificationServerSocketPath|. This function can only be called
// with base::MayBlock().
bool SendUpgradePropsToArcVmBootNotificationServer(
    const UpgradeParams& params,
    const std::string& serial_number) {
  std::string props = base::JoinString(
      GenerateUpgradeProps(params, serial_number, "ro.boot"), "\n");

  base::ScopedFD fd = ConnectToArcVmBootNotificationServer();
  if (!fd.is_valid())
    return false;

  if (!base::WriteFileDescriptor(fd.get(), props.c_str(), props.size())) {
    // TODO(wvk): Add a unittest to cover this failure once the UpgradeArc flow
    // requires this function to run successfully.
    PLOG(ERROR) << "Unable to write props to "
                << kArcVmBootNotificationServerSocketPath;
    return false;
  }
  return true;
}

}  // namespace

class ArcVmClientAdapter : public ArcClientAdapter,
                           public chromeos::ConciergeClient::VmObserver,
                           public chromeos::ConciergeClient::Observer {
 public:
  // Initializing |is_host_on_vm_| and |is_dev_mode_| is not always very fast.
  // Try to initialize them in the constructor and in StartMiniArc respectively.
  // They usually run when the system is not busy.
  ArcVmClientAdapter() : ArcVmClientAdapter(FileSystemStatusRewriter{}) {}

  // For testing purposes and the internal use (by the other ctor) only.
  explicit ArcVmClientAdapter(const FileSystemStatusRewriter& rewriter)
      : is_host_on_vm_(chromeos::system::StatisticsProvider::GetInstance()
                           ->IsRunningOnVm()),
        file_system_status_rewriter_for_testing_(rewriter) {
    auto* client = GetConciergeClient();
    client->AddVmObserver(this);
    client->AddObserver(this);
  }

  ~ArcVmClientAdapter() override {
    auto* client = GetConciergeClient();
    client->RemoveObserver(this);
    client->RemoveVmObserver(this);
  }

  // chromeos::ConciergeClient::VmObserver overrides:
  void OnVmStarted(
      const vm_tools::concierge::VmStartedSignal& signal) override {
    if (signal.name() == kArcVmName)
      VLOG(1) << "OnVmStarted: ARCVM cid=" << signal.vm_info().cid();
  }

  void OnVmStopped(
      const vm_tools::concierge::VmStoppedSignal& signal) override {
    if (signal.name() != kArcVmName)
      return;
    const int64_t cid = signal.cid();
    if (cid != current_cid_) {
      VLOG(1) << "Ignoring VmStopped signal: current CID=" << current_cid_
              << ", stopped CID=" << cid;
      return;
    }
    VLOG(1) << "OnVmStopped: ARCVM cid=" << cid;
    current_cid_ = kInvalidCid;
    OnArcInstanceStopped();
  }

  // ArcClientAdapter overrides:
  void StartMiniArc(StartParams params,
                    chromeos::VoidDBusMethodCallback callback) override {
    // TODO(wvk): Support mini ARC.
    VLOG(2) << "Mini ARCVM instance is not supported.";

    // Save the parameters for the later call to UpgradeArc.
    start_params_ = std::move(params);

    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
        base::BindOnce(
            []() { return GetSystemPropertyInt("cros_debug") == 1; }),
        base::BindOnce(&ArcVmClientAdapter::OnIsDevMode,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  void UpgradeArc(UpgradeParams params,
                  chromeos::VoidDBusMethodCallback callback) override {
    VLOG(1) << "Starting Concierge service";
    GetDebugDaemonClient()->StartConcierge(base::BindOnce(
        &ArcVmClientAdapter::OnConciergeStarted, weak_factory_.GetWeakPtr(),
        std::move(params), std::move(callback)));
  }

  void StopArcInstance(bool on_shutdown, bool should_backup_log) override {
    if (on_shutdown) {
      // Do nothing when |on_shutdown| is true because either vm_concierge.conf
      // job (in case of user session termination) or session_manager (in case
      // of browser-initiated exit on e.g. chrome://flags or UI language change)
      // will stop all VMs including ARCVM right after the browser exits.
      VLOG(1)
          << "StopArcInstance is called during browser shutdown. Do nothing.";
      return;
    }

    if (should_backup_log) {
      GetDebugDaemonClient()->BackupArcBugReport(
          user_id_hash_,
          base::BindOnce(&ArcVmClientAdapter::OnArcBugReportBackedUp,
                         weak_factory_.GetWeakPtr()));
    } else {
      StopArcInstanceInternal();
    }
  }

  void SetUserInfo(const cryptohome::Identification& cryptohome_id,
                   const std::string& hash,
                   const std::string& serial_number) override {
    DCHECK(cryptohome_id_.id().empty());
    DCHECK(user_id_hash_.empty());
    DCHECK(serial_number_.empty());
    if (cryptohome_id.id().empty())
      LOG(WARNING) << "cryptohome_id is empty";
    if (hash.empty())
      LOG(WARNING) << "hash is empty";
    if (serial_number.empty())
      LOG(WARNING) << "serial_number is empty";
    cryptohome_id_ = cryptohome_id;
    user_id_hash_ = hash;
    serial_number_ = serial_number;
  }

  // chromeos::ConciergeClient::Observer overrides:
  void ConciergeServiceStopped() override {
    VLOG(1) << "vm_concierge stopped";
    // At this point, all crosvm processes are gone. Notify the observer of the
    // event.
    OnArcInstanceStopped();
  }

  void ConciergeServiceRestarted() override {}

 private:
  void OnArcBugReportBackedUp(bool result) {
    VLOG(1) << "OnArcBugReportBackedUp: back up "
            << (result ? "done" : "failed");

    StopArcInstanceInternal();
  }

  void StopArcInstanceInternal() {
    VLOG(1) << "Stopping arcvm";
    vm_tools::concierge::StopVmRequest request;
    request.set_name(kArcVmName);
    request.set_owner_id(user_id_hash_);
    GetConciergeClient()->StopVm(
        request, base::BindOnce(&ArcVmClientAdapter::OnStopVmReply,
                                weak_factory_.GetWeakPtr()));
  }

  void OnIsDevMode(chromeos::VoidDBusMethodCallback callback,
                   bool is_dev_mode) {
    VLOG(1) << "Starting arcvm-per-board-features";
    // Note: the Upstart job is a task, and the callback for the start request
    // won't be called until the task finishes. When the callback is called with
    // true, it is ensured that the per-board features files exist.
    chromeos::UpstartClient::Get()->StartJob(
        kArcVmPerBoardFeaturesJobName, /*environment=*/{},
        base::BindOnce(&ArcVmClientAdapter::OnArcVmPerBoardFeaturesStarted,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
    is_dev_mode_ = is_dev_mode;
  }

  void OnArcVmPerBoardFeaturesStarted(chromeos::VoidDBusMethodCallback callback,
                                      bool result) {
    if (!result) {
      LOG(ERROR) << "Failed to start arcvm-per-board-features";
      // TODO(yusukes): Record UMA for this case.
      std::move(callback).Run(result);
      return;
    }
    // Make sure to kill a stale arcvm-server-proxy job (if any).
    chromeos::UpstartClient::Get()->StopJob(
        kArcVmServerProxyJobName, /*environment=*/{},
        base::BindOnce(&ArcVmClientAdapter::OnArcVmServerProxyJobStopped,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  void OnArcVmServerProxyJobStopped(chromeos::VoidDBusMethodCallback callback,
                                    bool result) {
    // Ignore |result| since it can be false when the proxy job has already been
    // stopped for other reasons, but it's not considered as an error.
    VLOG(1) << "OnArcVmServerProxyJobStopped: job "
            << (result ? "stopped" : "not running?");

    should_notify_observers_ = true;

    // Make sure to stop arc-keymasterd if it's already started. Always move
    // |callback| as is and ignore |result|.
    chromeos::UpstartClient::Get()->StopJob(
        kArcKeymasterJobName, /*environment=*/{},
        base::BindOnce(&ArcVmClientAdapter::OnArcKeymasterJobStopped,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  void OnArcKeymasterJobStopped(chromeos::VoidDBusMethodCallback callback,
                                bool result) {
    VLOG(1) << "OnArcKeymasterJobStopped: arc-keymasterd job "
            << (result ? "stopped" : "not running?");

    // Start arc-keymasterd. Always move |callback| as is and ignore |result|.
    VLOG(1) << "Starting arc-keymasterd";
    chromeos::UpstartClient::Get()->StartJob(
        kArcKeymasterJobName, /*environment=*/{},
        base::BindOnce(&ArcVmClientAdapter::OnArcKeymasterJobStarted,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  void OnArcKeymasterJobStarted(chromeos::VoidDBusMethodCallback callback,
                                bool result) {
    if (!result) {
      LOG(ERROR) << "Failed to start arc-keymasterd job";
      std::move(callback).Run(false);
      return;
    }

    // Kill a stale arcvm-boot-notification-server job
    chromeos::UpstartClient::Get()->StopJob(
        kArcVmBootNotificationServerJobName, /*environment=*/{},
        base::BindOnce(
            &ArcVmClientAdapter::OnArcVmBootNotificationServerStopped,
            weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  void OnArcVmBootNotificationServerStopped(
      chromeos::VoidDBusMethodCallback callback,
      bool result) {
    VLOG(1) << "OnArcVmBootNotificationServerStopped: job "
            << (result ? "stopped" : "not running?");

    VLOG(1) << "Starting arcvm-boot-notification-server";
    chromeos::UpstartClient::Get()->StartJob(
        kArcVmBootNotificationServerJobName, /*environment=*/{},
        base::BindOnce(
            &ArcVmClientAdapter::OnArcVmBootNotificationServerStarted,
            weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  void OnArcVmBootNotificationServerStarted(
      chromeos::VoidDBusMethodCallback callback,
      bool result) {
    if (!result) {
      LOG(ERROR) << "Failed to start arcvm-boot-notification-server job";
      std::move(callback).Run(false);
      return;
    }

    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
        base::BindOnce(&IsArcVmBootNotificationServerListening),
        base::BindOnce(
            &ArcVmClientAdapter::OnArcVmBootNotificationServerIsListening,
            weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  void OnArcVmBootNotificationServerIsListening(
      chromeos::VoidDBusMethodCallback callback,
      bool result) {
    if (!result) {
      LOG(ERROR) << "Failed to connect to arcvm-boot-notification-server";
      std::move(callback).Run(false);
      return;
    }
    std::move(callback).Run(true);
  }

  void OnConciergeStarted(UpgradeParams params,
                          chromeos::VoidDBusMethodCallback callback,
                          bool success) {
    if (!success) {
      LOG(ERROR) << "Failed to start Concierge service for arcvm";
      std::move(callback).Run(false);
      return;
    }
    VLOG(1) << "Starting arcvm-server-proxy";
    chromeos::UpstartClient::Get()->StartJob(
        kArcVmServerProxyJobName, /*environment=*/{},
        base::BindOnce(&ArcVmClientAdapter::OnArcVmServerProxyJobStarted,
                       weak_factory_.GetWeakPtr(), std::move(params),
                       std::move(callback)));
  }

  void OnArcVmServerProxyJobStarted(UpgradeParams params,
                                    chromeos::VoidDBusMethodCallback callback,
                                    bool result) {
    if (!result) {
      LOG(ERROR) << "Failed to start arcvm-server-proxy job";
      std::move(callback).Run(false);
      return;
    }

    VLOG(1) << "Starting arc-create-data";
    const std::string account_id =
        cryptohome::CreateAccountIdentifierFromIdentification(cryptohome_id_)
            .account_id();
    chromeos::UpstartClient::Get()->StartJob(
        kArcCreateDataJobName, {"CHROMEOS_USER=" + account_id},
        base::BindOnce(&ArcVmClientAdapter::OnArcCreateDataJobStarted,
                       weak_factory_.GetWeakPtr(), std::move(params),
                       std::move(callback)));
  }

  void OnArcCreateDataJobStarted(UpgradeParams params,
                                 chromeos::VoidDBusMethodCallback callback,
                                 bool result) {
    if (!result) {
      LOG(ERROR) << "Failed to start arc-create-data job";
      std::move(callback).Run(false);
      return;
    }

    VLOG(2) << "Checking file system status";
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
        base::BindOnce(&FileSystemStatus::GetFileSystemStatusBlocking),
        base::BindOnce(&ArcVmClientAdapter::OnFileSystemStatus,
                       weak_factory_.GetWeakPtr(), std::move(params),
                       std::move(callback)));
  }

  void OnFileSystemStatus(UpgradeParams params,
                          chromeos::VoidDBusMethodCallback callback,
                          FileSystemStatus file_system_status) {
    VLOG(2) << "Got file system status";
    if (file_system_status_rewriter_for_testing_)
      file_system_status_rewriter_for_testing_.Run(&file_system_status);

    if (user_id_hash_.empty()) {
      LOG(ERROR) << "User ID hash is not set";
      std::move(callback).Run(false);
      return;
    }
    if (serial_number_.empty()) {
      LOG(ERROR) << "Serial number is not set";
      std::move(callback).Run(false);
      return;
    }

    const int32_t cpus =
        base::SysInfo::NumberOfProcessors() - start_params_.num_cores_disabled;
    DCHECK_LT(0, cpus);

    DCHECK(is_dev_mode_);
    std::vector<std::string> kernel_cmdline = GenerateKernelCmdline(
        start_params_, params, file_system_status, *is_dev_mode_,
        is_host_on_vm_, GetChromeOsChannelFromLsbRelease(), serial_number_);
    auto start_request = CreateStartArcVmRequest(
        user_id_hash_, cpus, params.demo_session_apps_path, file_system_status,
        std::move(kernel_cmdline));

    GetConciergeClient()->StartArcVm(
        start_request,
        base::BindOnce(&ArcVmClientAdapter::OnStartArcVmReply,
                       weak_factory_.GetWeakPtr(), std::move(callback)));

    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
        base::BindOnce(&SendUpgradePropsToArcVmBootNotificationServer, params,
                       serial_number_),
        base::BindOnce([](bool result) {
          VLOG(1)
              << "Sending upgrade props to arcvm-boot-notification-server was "
              << (result ? "successful" : "unsuccessful");
        }));
  }

  void OnStartArcVmReply(
      chromeos::VoidDBusMethodCallback callback,
      base::Optional<vm_tools::concierge::StartVmResponse> reply) {
    if (!reply.has_value()) {
      LOG(ERROR) << "Failed to start arcvm. Empty response.";
      std::move(callback).Run(false);
      return;
    }

    const vm_tools::concierge::StartVmResponse& response = reply.value();
    if (response.status() != vm_tools::concierge::VM_STATUS_RUNNING) {
      LOG(ERROR) << "Failed to start arcvm: status=" << response.status()
                 << ", reason=" << response.failure_reason();
      std::move(callback).Run(false);
      return;
    }
    current_cid_ = response.vm_info().cid();

    VLOG(1) << "ARCVM started cid=" << current_cid_;
    std::move(callback).Run(true);
  }

  void OnArcInstanceStopped() {
    VLOG(1) << "ARCVM stopped. Stopping arcvm-server-proxy";

    // TODO(yusukes): Consider removing this stop call once b/142140355 is
    // implemented.
    chromeos::UpstartClient::Get()->StopJob(
        kArcVmServerProxyJobName, /*environment=*/{}, base::DoNothing());

    // If this method is called before even mini VM is started (e.g. very early
    // vm_concierge crash), or this method is called twice (e.g. crosvm crash
    // followed by vm_concierge crash), do nothing.
    if (!should_notify_observers_)
      return;
    should_notify_observers_ = false;

    for (auto& observer : observer_list_)
      observer.ArcInstanceStopped();
  }

  void OnStopVmReply(
      base::Optional<vm_tools::concierge::StopVmResponse> reply) {
    // If the reply indicates the D-Bus call is successfully done, do nothing.
    // Concierge will call OnVmStopped() eventually.
    if (reply.has_value() && reply.value().success())
      return;

    // We likely tried to stop mini VM which doesn't exist today. Notify
    // observers.
    // TODO(wvk): Remove the fallback once we implement mini VM.
    OnArcInstanceStopped();
  }

  base::Optional<bool> is_dev_mode_;
  // True when the *host* is running on a VM.
  const bool is_host_on_vm_;

  // A cryptohome ID of the primary profile.
  cryptohome::Identification cryptohome_id_;
  // A hash of the primary profile user ID.
  std::string user_id_hash_;
  // A serial number for the current profile.
  std::string serial_number_;

  StartParams start_params_;
  bool should_notify_observers_ = false;
  int64_t current_cid_ = kInvalidCid;

  FileSystemStatusRewriter file_system_status_rewriter_for_testing_;

  // For callbacks.
  base::WeakPtrFactory<ArcVmClientAdapter> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ArcVmClientAdapter);
};

std::unique_ptr<ArcClientAdapter> CreateArcVmClientAdapter() {
  return std::make_unique<ArcVmClientAdapter>();
}

std::unique_ptr<ArcClientAdapter> CreateArcVmClientAdapterForTesting(
    const FileSystemStatusRewriter& rewriter) {
  return std::make_unique<ArcVmClientAdapter>(rewriter);
}

void SetArcVmBootNotificationServerAddressForTesting(
    const std::string& new_address,
    base::TimeDelta connect_timeout_limit,
    base::TimeDelta connect_sleep_duration_initial) {
  sockaddr_un* address =
      const_cast<sockaddr_un*>(GetArcVmBootNotificationServerAddress());
  DCHECK_GE(sizeof(address->sun_path), new_address.size());
  DCHECK_GT(connect_timeout_limit, connect_sleep_duration_initial);

  memset(address->sun_path, 0, sizeof(address->sun_path));
  // |new_address| may contain '\0' if it is an abstract socket address, so use
  // memcpy instead of strcpy.
  memcpy(address->sun_path, new_address.data(), new_address.size());

  g_connect_timeout_limit_for_testing = connect_timeout_limit;
  g_connect_sleep_duration_initial_for_testing = connect_sleep_duration_initial;
}

std::vector<std::string> GenerateUpgradeProps(
    const UpgradeParams& upgrade_params,
    const std::string& serial_number,
    const std::string& prefix) {
  std::vector<std::string> result = {
      base::StringPrintf("%s.disable_boot_completed=%d", prefix.c_str(),
                         upgrade_params.skip_boot_completed_broadcast),
      base::StringPrintf("%s.copy_packages_cache=%d", prefix.c_str(),
                         static_cast<int>(upgrade_params.packages_cache_mode)),
      base::StringPrintf("%s.skip_gms_core_cache=%d", prefix.c_str(),
                         upgrade_params.skip_gms_core_cache),
      base::StringPrintf("%s.arc_demo_mode=%d", prefix.c_str(),
                         upgrade_params.is_demo_session),
      base::StringPrintf(
          "%s.supervision.transition=%d", prefix.c_str(),
          static_cast<int>(upgrade_params.supervision_transition)),
      base::StringPrintf("%s.serialno=%s", prefix.c_str(),
                         serial_number.c_str()),
  };
  // Conditionally sets more properties based on |upgrade_params|.
  if (!upgrade_params.locale.empty()) {
    result.push_back(base::StringPrintf("%s.locale=%s", prefix.c_str(),
                                        upgrade_params.locale.c_str()));
    if (!upgrade_params.preferred_languages.empty()) {
      result.push_back(base::StringPrintf(
          "%s.preferred_languages=%s", prefix.c_str(),
          base::JoinString(upgrade_params.preferred_languages, ",").c_str()));
    }
  }

  // TODO(niwa): Handle |is_managed_account| in |upgrade_params| when we
  // implement apk sideloading for ARCVM.
  return result;
}

}  // namespace arc
