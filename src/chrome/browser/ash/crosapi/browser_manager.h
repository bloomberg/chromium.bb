// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_BROWSER_MANAGER_H_
#define CHROME_BROWSER_ASH_CROSAPI_BROWSER_MANAGER_H_

#include <memory>
#include <set>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/process/process.h"
#include "base/time/time.h"
#include "chrome/browser/ash/crosapi/browser_manager_observer.h"
#include "chrome/browser/ash/crosapi/browser_service_host_observer.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crosapi/crosapi_id.h"
#include "chrome/browser/ash/crosapi/environment_provider.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace component_updater {
class CrOSComponentManager;
}  // namespace component_updater

namespace apps {
class StandaloneBrowserExtensionApps;
}  // namespace apps

namespace crosapi {
namespace mojom {
class Crosapi;
}  // namespace mojom

class BrowserLoader;
class TestMojoConnectionManager;

using browser_util::LacrosSelection;

// Manages the lifetime of lacros-chrome, and its loading status. This class is
// a part of ash-chrome.
class BrowserManager : public session_manager::SessionManagerObserver,
                       public BrowserServiceHostObserver,
                       public policy::CloudPolicyStore::Observer {
 public:
  // Static getter of BrowserManager instance. In real use cases,
  // BrowserManager instance should be unique in the process.
  static BrowserManager* Get();

  explicit BrowserManager(
      scoped_refptr<component_updater::CrOSComponentManager> manager);

  BrowserManager(const BrowserManager&) = delete;
  BrowserManager& operator=(const BrowserManager&) = delete;

  ~BrowserManager() override;

  // Returns true if the binary is ready to launch or already launched.
  // Typical usage is to check IsReady(), then if it returns false,
  // call SetLoadCompleteCallback() to be notified when the download completes.
  bool IsReady() const;

  // Returns true if Lacros is in running state.
  // Virtual for testing.
  virtual bool IsRunning() const;

  // Return true if Lacros is running, launching or terminating.
  // We do not want the multi-signin to be available when Lacros is
  // running; therefore, we also have to exclude other states (e.g. if
  // Lacros is launched and multi-signin is enabled, we would have
  // Lacros running and multiple users signed in simultaneously).
  // Virtual for testing.
  virtual bool IsRunningOrWillRun() const;

  // Sets a callback to be called when the binary download completes. The
  // download may not be successful.
  using LoadCompleteCallback = base::OnceCallback<void(bool success)>;
  void SetLoadCompleteCallback(LoadCompleteCallback callback);

  // Opens the browser window in lacros-chrome.
  // If lacros-chrome is not yet launched, it triggers to launch. If this is
  // called again during the setup phase of the launch process, it will be
  // ignored. This needs to be called after loading. The condition can be
  // checked IsReady(), and if not yet, SetLoadCompletionCallback can be used to
  // wait for the loading.
  // TODO(crbug.com/1101676): Notify callers the result of opening window
  // request. Because of asynchronous operations crossing processes,
  // there's no guarantee that the opening window request succeeds.
  // Currently, its condition and result are completely hidden behind this
  // class, so there's no way for callers to handle such error cases properly.
  // This design often leads the flakiness behavior of the product and testing,
  // so should be avoided.
  void NewWindow(bool incognito);

  // Returns true if crosapi interface supports NewWindowForDetachingTab API.
  bool NewWindowForDetachingTabSupported() const;

  using NewWindowForDetachingTabCallback =
      base::OnceCallback<void(crosapi::mojom::CreationResult,
                              const std::string&)>;
  // Opens a new window in the browser and transfers the given tab (or group)
  // to it.
  // NOTE: This method is used by Chrome OS WebUI in tablet mode as a response
  // to a drag'n drop operation from the user.
  void NewWindowForDetachingTab(const std::u16string& tab_id,
                                const std::u16string& group_id,
                                NewWindowForDetachingTabCallback closure);

  // Returns true if crosapi interface supports NewFullscreenWindow API.
  bool NewFullscreenWindowSupported() const;

  using NewFullscreenWindowCallback =
      base::OnceCallback<void(crosapi::mojom::CreationResult)>;
  // Open a fullscreen browser window in lacros-chrome. The only tab will be
  // navigated to the given `url` once the window is launched.
  // NOTE: This method is used by Chrome OS web Kiosk session only. The behavior
  // may change and it shouldn't be used by anybody else.
  // Virtual for testing.
  virtual void NewFullscreenWindow(const GURL& url,
                                   NewFullscreenWindowCallback callback);

  // Similar to NewWindow(), but opens a tab, instead.
  // See crosapi::mojom::BrowserService::NewTab for more details
  void NewTab();

  // Opens the specified URL in lacros-chrome. If it is not running,
  // it launches lacros-chrome with the given URL.
  // See crosapi::mojom::BrowserService::OpenUrl for more details.
  void OpenUrl(const GURL& url);

  // Similar to NewWindow(), but restores a tab recently closed.
  // See crosapi::mojom::BrowserService::RestoreTab for more details
  void RestoreTab();

  // Initialize resources and start Lacros. This class provides two approaches
  // to fulfill different requirements.
  // - For most sessions, Lacros will be started automatically once
  // `SessionState` is changed to active.
  // - For Kiosk sessions, Lacros needs to be started earlier because all
  // extensions and browser window should be well prepared before the user
  // enters the session. This method should be called at the appropriate time.
  void InitializeAndStart();

  // Returns true if crosapi interface supports GetFeedbackData API.
  bool GetFeedbackDataSupported() const;

  using GetFeedbackDataCallback = base::OnceCallback<void(base::Value)>;
  // Gathers Lacros feedback data.
  // Virtual for testing.
  virtual void GetFeedbackData(GetFeedbackDataCallback callback);

  // Returns true if crosapi interface supports GetHistograms API.
  bool GetHistogramsSupported() const;

  using GetHistogramsCallback = base::OnceCallback<void(const std::string&)>;
  // Gets Lacros histograms.
  void GetHistograms(GetHistogramsCallback callback);

  // Returns true if crosapi interface supports GetActiveTabUrl API.
  bool GetActiveTabUrlSupported() const;

  using GetActiveTabUrlCallback =
      base::OnceCallback<void(const absl::optional<GURL>&)>;
  // Gets Url of the active tab from lacros if there is any.
  void GetActiveTabUrl(GetActiveTabUrlCallback callback);

  void AddObserver(BrowserManagerObserver* observer);
  void RemoveObserver(BrowserManagerObserver* observer);

  const std::string& browser_version() const { return browser_version_; }
  void set_browser_version(const std::string& version) {
    browser_version_ = version;
  }

  // Set the data of device account policy. It is the serialized blob of
  // PolicyFetchResponse received from the server, or parsed from the file after
  // is was validated by Ash.
  void SetDeviceAccountPolicy(const std::string& policy_blob);

  // Notifies the BrowserManager that it should prepare for shutdown. This is
  // called in the early stages of ash shutdown to give Lacros sufficient time
  // for a graceful exit.
  void Shutdown();

  // Parameters used to launch Lacros that are calculated on a background
  // sequence. Public so that it can be used from private static functions.
  struct LaunchParamsFromBackground {
   public:
    LaunchParamsFromBackground();
    LaunchParamsFromBackground(LaunchParamsFromBackground&&);
    LaunchParamsFromBackground(const LaunchParamsFromBackground&) = delete;
    LaunchParamsFromBackground& operator=(const LaunchParamsFromBackground&) =
        delete;
    ~LaunchParamsFromBackground();

    // An fd for a log file.
    base::ScopedFD logfd;

    // Whether this version of Lacros supports the new account manager.
    bool use_new_account_manager = false;
  };

 protected:
  enum class State {
    // Lacros is not initialized yet.
    // Lacros-chrome loading depends on user type, so it needs to wait
    // for user session.
    NOT_INITIALIZED,

    // User session started, and now it's mounting lacros-chrome.
    MOUNTING,

    // Lacros-chrome is unavailable. I.e., failed to load for some reason
    // or disabled.
    UNAVAILABLE,

    // Lacros-chrome is loaded and ready for launching.
    STOPPED,

    // Lacros-chrome is creating a new log file to log to.
    CREATING_LOG_FILE,

    // Lacros-chrome is launching.
    STARTING,

    // Mojo connection to lacros-chrome is established so, it's in
    // the running state.
    RUNNING,

    // Lacros-chrome is being terminated soon.
    TERMINATING,
  };
  // Changes |state| value and potentitally notify observers of the change.
  void SetState(State state);

  // Posts CreateLogFile() and StartWithLogFile() to the thread pool.
  // Virtual for tests.
  virtual void Start(browser_util::InitialBrowserAction initial_browser_action);

 private:
  FRIEND_TEST_ALL_PREFIXES(BrowserManagerTest, LacrosKeepAlive);
  friend class apps::StandaloneBrowserExtensionApps;

  // Remember the launch mode of Lacros.
  void RecordLacrosLaunchMode();

  // These ash features are allowed to request that Lacros stay running in the
  // background.
  enum class Feature {
    kTestOnly,
    kAppService,
    kChromeApps,
  };

  // Any instance of this class will ensure that the Lacros browser will stay
  // running in the background even when no windows are showing.
  class ScopedKeepAlive {
   public:
    ~ScopedKeepAlive();

   private:
    friend class BrowserManager;

    // BrowserManager must outlive this instance.
    ScopedKeepAlive(BrowserManager* manager, Feature feature);

    BrowserManager* manager_;
    Feature feature_;
  };

  // Ash features that want Lacros to stay running in the background must be
  // marked as friends of this class so that lacros owners can audit usage.
  std::unique_ptr<ScopedKeepAlive> KeepAlive(Feature feature);

  struct BrowserServiceInfo {
    BrowserServiceInfo(mojo::RemoteSetElementId mojo_id,
                       mojom::BrowserService* service,
                       uint32_t interface_version);
    BrowserServiceInfo(const BrowserServiceInfo&);
    BrowserServiceInfo& operator=(const BrowserServiceInfo&);
    ~BrowserServiceInfo();

    // ID managed in BrowserServiceHostAsh, which is tied to the |service|.
    mojo::RemoteSetElementId mojo_id;
    // BrowserService proxy connected to lacros-chrome.
    mojom::BrowserService* service;
    // Supported interface version of the BrowserService in Lacros-chrome.
    uint32_t interface_version;
  };

  enum class MaybeStartResult {
    kNotStarted,
    kStarting,
    kRunning,
  };
  // Checks the precondition to start Lacros, and actually trigger to start
  // if necessary.
  // If the condition to start lacros is not met, kNotStarted is returned.
  // If the condition to start lacros is met, and it is not yet started,
  // or it is under starting, kStarting is returned.
  // Otherwise, i.e., lacros is already running, kRunning is returned.
  // |extra_args| will be passed to the argument to launch lacros.
  MaybeStartResult MaybeStart(
      browser_util::InitialBrowserAction initial_browser_action);

  // Starts the lacros-chrome process and redirects stdout/err to file pointed
  // by logfd.
  void StartWithLogFile(
      browser_util::InitialBrowserAction initial_browser_action,
      LaunchParamsFromBackground params);

  // BrowserServiceHostObserver:
  void OnBrowserServiceConnected(CrosapiId id,
                                 mojo::RemoteSetElementId mojo_id,
                                 mojom::BrowserService* browser_service,
                                 uint32_t browser_service_version) override;
  void OnBrowserServiceDisconnected(CrosapiId id,
                                    mojo::RemoteSetElementId mojo_id) override;
  void OnBrowserRelaunchRequested(CrosapiId id) override;

  // Called when the Mojo connection to lacros-chrome is disconnected. It may be
  // "just a Mojo error" or "lacros-chrome crash". This method posts a
  // shutdown-blocking async task that waits lacros-chrome to exit, giving it a
  // chance to gracefully exit. The task will send a terminate signal to
  // lacros-chrome if the process has not terminated within the graceful
  // shutdown window.
  void OnMojoDisconnected();

  // Called when lacros-chrome is terminated and successfully wait(2)ed.
  void OnLacrosChromeTerminated();

  // session_manager::SessionManagerObserver:
  void OnSessionStateChanged() override;

  // Sets user policy to be propagated to Lacros and subsribes to the user
  // policy updates in Ash.
  void PrepareLacrosPolicies();
  policy::CloudPolicyStore* GetDeviceAccountPolicyStore();

  // policy::CloudPolicyStore::Observer:
  void OnStoreLoaded(policy::CloudPolicyStore* store) override;
  void OnStoreError(policy::CloudPolicyStore* store) override;
  void OnStoreDestruction(policy::CloudPolicyStore* store) override;

  // Called on load completion.
  void OnLoadComplete(const base::FilePath& path, LacrosSelection selection);

  // Methods for features to register and de-register for needing to keep Lacros
  // alive.
  void StartKeepAlive(Feature feature);
  void StopKeepAlive(Feature feature);

  // The implementation of keep-alive is simple: every time state_ becomes
  // STOPPED, launch Lacros.
  void LaunchForKeepAliveIfNecessary();

  // Notifies browser to update its keep-alive status.
  // Disabling keep-alive here may shut down the browser in background.
  // (i.e., if there's no browser window opened, it may be shut down).
  void UpdateKeepAliveInBrowserIfNecessary(bool enabled);

  State state_ = State::NOT_INITIALIZED;

  // May be null in tests.
  scoped_refptr<component_updater::CrOSComponentManager> component_manager_;

  std::unique_ptr<crosapi::BrowserLoader> browser_loader_;

  // Path to the lacros-chrome disk image directory.
  base::FilePath lacros_path_;

  // Whether we are starting "rootfs" or "stateful" lacros.
  absl::optional<LacrosSelection> lacros_selection_ = absl::nullopt;

  // Version of the browser (e.g. lacros-chrome) displayed to user in feedback
  // report, etc. It includes both browser version and channel in the format of:
  // {browser version} {channel}
  // For example, "87.0.0.1 dev", "86.0.4240.38 beta".
  std::string browser_version_;

  // Called when the binary download completes.
  LoadCompleteCallback load_complete_callback_;

  // Time when the lacros process was launched.
  base::TimeTicks lacros_launch_time_;

  // Process handle for the lacros-chrome process.
  base::Process lacros_process_;

  // ID for the current Crosapi connection.
  // Available only when lacros-chrome is running.
  absl::optional<CrosapiId> crosapi_id_;
  absl::optional<CrosapiId> legacy_crosapi_id_;

  // Proxy to BrowserService mojo service in lacros-chrome.
  // Available during lacros-chrome is running.
  absl::optional<BrowserServiceInfo> browser_service_;

  // Remembers the request from Lacros-chrome whether it needs to be
  // relaunched. Reset on new process start in any cases.
  bool relaunch_requested_ = false;

  // Tracks whether Shutdown() has been signalled by ash. This flag ensures any
  // new or existing lacros startup tasks are not executed during shutdown.
  bool shutdown_requested_ = false;

  // Helps set up and manage the mojo connections between lacros-chrome and
  // ash-chrome in testing environment. Only applicable when
  // '--lacros-mojo-socket-for-testing' is present in the command line.
  std::unique_ptr<TestMojoConnectionManager> test_mojo_connection_manager_;

  // Used to pass ash-chrome specific flags/configurations to lacros-chrome.
  std::unique_ptr<EnvironmentProvider> environment_provider_;

  // The features that are currently registered to keep Lacros alive.
  std::set<Feature> keep_alive_features_;

  base::ObserverList<BrowserManagerObserver> observers_;

  base::WeakPtrFactory<BrowserManager> weak_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_BROWSER_MANAGER_H_
