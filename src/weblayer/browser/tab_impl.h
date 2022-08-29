// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_TAB_IMPL_H_
#define WEBLAYER_BROWSER_TAB_IMPL_H_

#include <memory>
#include <set>
#include <string>

#include "base/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "cc/input/browser_controls_state.h"
#include "components/find_in_page/find_result_observer.h"
#include "content/public/browser/color_chooser.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "weblayer/browser/i18n_util.h"
#include "weblayer/public/tab.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_java_ref.h"
#include "weblayer/browser/browser_controls_navigation_state_handler_delegate.h"
#endif

namespace js_injection {
class JsCommunicationHost;
}

namespace blink {
namespace web_pref {
struct WebPreferences;
}
}  // namespace blink

namespace content {
class RenderWidgetHostView;
class WebContents;
struct ContextMenuParams;
}  // namespace content

namespace gfx {
class Rect;
class Size;
}  // namespace gfx

namespace sessions {
class SessionTabHelperDelegate;
}

namespace weblayer {
class BrowserControlsNavigationStateHandler;
class BrowserImpl;
class FullscreenDelegate;
class NavigationControllerImpl;
class NewTabDelegate;
class ProfileImpl;

#if BUILDFLAG(IS_ANDROID)
class BrowserControlsContainerView;
enum class ControlsVisibilityReason;
#endif

class TabImpl : public Tab,
                public content::WebContentsDelegate,
                public content::WebContentsObserver,
#if BUILDFLAG(IS_ANDROID)
                public BrowserControlsNavigationStateHandlerDelegate,
#endif
                public find_in_page::FindResultObserver {
 public:
  enum class ScreenShotErrors {
    kNone = 0,
    kScaleOutOfRange,
    kTabNotActive,
    kWebContentsNotVisible,
    kNoSurface,
    kNoRenderWidgetHostView,
    kNoWindowAndroid,
    kEmptyViewport,
    kHiddenByControls,
    kScaledToEmpty,
    kCaptureFailed,
    kBitmapAllocationFailed,
  };

  class DataObserver {
   public:
    // Called when SetData() is called on |tab|.
    virtual void OnDataChanged(
        TabImpl* tab,
        const std::map<std::string, std::string>& data) = 0;
  };

  // TODO(sky): investigate a better way to not have so many ifdefs.
#if BUILDFLAG(IS_ANDROID)
  TabImpl(ProfileImpl* profile,
          const base::android::JavaParamRef<jobject>& java_impl,
          std::unique_ptr<content::WebContents> web_contents);
#endif
  explicit TabImpl(ProfileImpl* profile,
                   std::unique_ptr<content::WebContents> web_contents,
                   const std::string& guid = std::string());

  TabImpl(const TabImpl&) = delete;
  TabImpl& operator=(const TabImpl&) = delete;

  ~TabImpl() override;

  // Returns the TabImpl from the specified WebContents (which may be null), or
  // null if |web_contents| was not created by a TabImpl.
  static TabImpl* FromWebContents(content::WebContents* web_contents);

  static std::set<TabImpl*> GetAllTabImpl();

  ProfileImpl* profile() { return profile_; }

  void set_browser(BrowserImpl* browser) { browser_ = browser; }
  BrowserImpl* browser() { return browser_; }

  content::WebContents* web_contents() const { return web_contents_.get(); }

  bool has_new_tab_delegate() const { return new_tab_delegate_ != nullptr; }
  NewTabDelegate* new_tab_delegate() const { return new_tab_delegate_; }

  // Called from Browser when this Tab is gaining/losing active status.
  void OnGainedActive();
  void OnLosingActive();

  bool IsActive();

  void ShowContextMenu(const content::ContextMenuParams& params);

#if BUILDFLAG(IS_ANDROID)
  base::android::ScopedJavaGlobalRef<jobject> GetJavaTab() {
    return java_impl_;
  }

  bool desktop_user_agent_enabled() { return desktop_user_agent_enabled_; }

  // Call this method to disable integration with the system-level Autofill
  // infrastructure. Useful in conjunction with InitializeAutofillForTests().
  // Should be called early in the lifetime of WebLayer, and in
  // particular, must be called before the TabImpl is attached to the browser
  // on the Java side to have the desired effect.
  static void DisableAutofillSystemIntegrationForTesting();

  base::android::ScopedJavaLocalRef<jobject> GetWebContents(JNIEnv* env);
  void SetBrowserControlsContainerViews(
      JNIEnv* env,
      jlong native_top_browser_controls_container_view,
      jlong native_bottom_browser_controls_container_view);
  void ExecuteScript(JNIEnv* env,
                     const base::android::JavaParamRef<jstring>& script,
                     bool use_separate_isolate,
                     const base::android::JavaParamRef<jobject>& callback);
  void SetJavaImpl(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& impl);

  // Invoked every time that the Java-side AutofillProvider instance is created,
  // the native side autofill might have been initialized in the case that
  // Android context is switched.
  void InitializeAutofillIfNecessary(JNIEnv* env);
  void UpdateBrowserControlsConstraint(JNIEnv* env,
                                       jint constraint,
                                       jboolean animate);

  base::android::ScopedJavaLocalRef<jstring> GetGuid(JNIEnv* env);
  void CaptureScreenShot(
      JNIEnv* env,
      jfloat scale,
      const base::android::JavaParamRef<jobject>& value_callback);

  jboolean SetData(JNIEnv* env,
                   const base::android::JavaParamRef<jobjectArray>& data);
  base::android::ScopedJavaLocalRef<jobjectArray> GetData(JNIEnv* env);
  jboolean IsRendererControllingBrowserControlsOffsets(JNIEnv* env);
  base::android::ScopedJavaLocalRef<jstring> RegisterWebMessageCallback(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& js_object_name,
      const base::android::JavaParamRef<jobjectArray>& origins,
      const base::android::JavaParamRef<jobject>& client);
  void UnregisterWebMessageCallback(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& js_object_name);
  jboolean CanTranslate(JNIEnv* env);
  void ShowTranslateUi(JNIEnv* env);
  void RemoveTabFromBrowserBeforeDestroying(JNIEnv* env);
  void SetTranslateTargetLanguage(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& translate_target_lang);
  void SetDesktopUserAgentEnabled(JNIEnv* env, jboolean enable);
  jboolean IsDesktopUserAgentEnabled(JNIEnv* env);
  void Download(JNIEnv* env, jlong native_context_menu_params);
#endif

  ErrorPageDelegate* error_page_delegate() { return error_page_delegate_; }

  void AddDataObserver(DataObserver* observer);
  void RemoveDataObserver(DataObserver* observer);

  GoogleAccountsDelegate* google_accounts_delegate() {
    return google_accounts_delegate_;
  }

  // Tab:
  Browser* GetBrowser() override;
  void SetErrorPageDelegate(ErrorPageDelegate* delegate) override;
  void SetFullscreenDelegate(FullscreenDelegate* delegate) override;
  void SetNewTabDelegate(NewTabDelegate* delegate) override;
  void SetGoogleAccountsDelegate(GoogleAccountsDelegate* delegate) override;
  void AddObserver(TabObserver* observer) override;
  void RemoveObserver(TabObserver* observer) override;
  NavigationController* GetNavigationController() override;
  void ExecuteScript(const std::u16string& script,
                     bool use_separate_isolate,
                     JavaScriptResultCallback callback) override;
  const std::string& GetGuid() override;
  void SetData(const std::map<std::string, std::string>& data) override;
  const std::map<std::string, std::string>& GetData() override;
  std::u16string AddWebMessageHostFactory(
      std::unique_ptr<WebMessageHostFactory> factory,
      const std::u16string& js_object_name,
      const std::vector<std::string>& js_origins) override;
  void RemoveWebMessageHostFactory(
      const std::u16string& js_object_name) override;
  std::unique_ptr<FaviconFetcher> CreateFaviconFetcher(
      FaviconFetcherDelegate* delegate) override;
  void SetTranslateTargetLanguage(
      const std::string& translate_target_lang) override;
#if !BUILDFLAG(IS_ANDROID)
  void AttachToView(views::WebView* web_view) override;
#endif

  void WebPreferencesChanged();
  void SetWebPreferences(blink::web_pref::WebPreferences* prefs);

  // Executes |script| with a user gesture.
  void ExecuteScriptWithUserGestureForTests(const std::u16string& script);

#if BUILDFLAG(IS_ANDROID)
  // Initializes the autofill system for tests.
  void InitializeAutofillForTests();
#endif  // BUILDFLAG(IS_ANDROID)

 private:
  // content::WebContentsDelegate:
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) override;
  void ShowRepostFormWarningDialog(content::WebContents* source) override;
  void NavigationStateChanged(content::WebContents* source,
                              content::InvalidateTypes changed_flags) override;
  content::JavaScriptDialogManager* GetJavaScriptDialogManager(
      content::WebContents* web_contents) override;
#if BUILDFLAG(IS_ANDROID)
  std::unique_ptr<content::ColorChooser> OpenColorChooser(
      content::WebContents* web_contents,
      SkColor color,
      const std::vector<blink::mojom::ColorSuggestionPtr>& suggestions)
      override;
#endif
  void RunFileChooser(content::RenderFrameHost* render_frame_host,
                      scoped_refptr<content::FileSelectListener> listener,
                      const blink::mojom::FileChooserParams& params) override;
  void CreateSmsPrompt(content::RenderFrameHost*,
                       const std::vector<url::Origin>&,
                       const std::string& one_time_code,
                       base::OnceClosure on_confirm,
                       base::OnceClosure on_cancel) override;
  int GetTopControlsHeight() override;
  int GetTopControlsMinHeight() override;
  int GetBottomControlsHeight() override;
  bool DoBrowserControlsShrinkRendererSize(
      content::WebContents* web_contents) override;
  bool OnlyExpandTopControlsAtPageTop() override;
  bool ShouldAnimateBrowserControlsHeightChanges() override;
  bool IsBackForwardCacheSupported() override;
  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) override;
  bool CheckMediaAccessPermission(content::RenderFrameHost* render_frame_host,
                                  const GURL& security_origin,
                                  blink::mojom::MediaStreamType type) override;
  void EnterFullscreenModeForTab(
      content::RenderFrameHost* requesting_frame,
      const blink::mojom::FullscreenOptions& options) override;
  void ExitFullscreenModeForTab(content::WebContents* web_contents) override;
  bool IsFullscreenForTabOrPending(
      const content::WebContents* web_contents) override;
  blink::mojom::DisplayMode GetDisplayMode(
      const content::WebContents* web_contents) override;
  void AddNewContents(content::WebContents* source,
                      std::unique_ptr<content::WebContents> new_contents,
                      const GURL& target_url,
                      WindowOpenDisposition disposition,
                      const gfx::Rect& initial_rect,
                      bool user_gesture,
                      bool* was_blocked) override;
  void CloseContents(content::WebContents* source) override;
  void FindReply(content::WebContents* web_contents,
                 int request_id,
                 int number_of_matches,
                 const gfx::Rect& selection_rect,
                 int active_match_ordinal,
                 bool final_update) override;
#if BUILDFLAG(IS_ANDROID)
  void FindMatchRectsReply(content::WebContents* web_contents,
                           int version,
                           const std::vector<gfx::RectF>& rects,
                           const gfx::RectF& active_rect) override;

  // Pointer arguments are outputs. Check the preconditions for capturing a
  // screenshot and either set all outputs, or return an error code, in which
  // case the state of output arguments is undefined.
  ScreenShotErrors PrepareForCaptureScreenShot(
      float scale,
      content::RenderWidgetHostView** rwhv,
      gfx::Rect* src_rect,
      gfx::Size* output_size);

  void UpdateBrowserControlsState(cc::BrowserControlsState new_state,
                                  bool animate);
#endif

  // content::WebContentsObserver:
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override;
  void DidChangeVisibleSecurityState() override;

  // find_in_page::FindResultObserver:
  void OnFindResultAvailable(content::WebContents* web_contents) override;

#if BUILDFLAG(IS_ANDROID)
  // BrowserControlsNavigationStateHandlerDelegate:
  void OnBrowserControlsStateStateChanged(
      ControlsVisibilityReason reason,
      cc::BrowserControlsState state) override;
  void OnUpdateBrowserControlsStateBecauseOfProcessSwitch(
      bool did_commit) override;
#endif

  // Called from closure supplied to delegate to exit fullscreen.
  void OnExitFullscreen();

  void UpdateRendererPrefs(bool should_sync_prefs);

  // Returns the FindTabHelper for the page, or null if none exists.
  find_in_page::FindTabHelper* GetFindTabHelper();

  static sessions::SessionTabHelperDelegate* GetSessionServiceTabHelperDelegate(
      content::WebContents* web_contents);

#if BUILDFLAG(IS_ANDROID)
  void InitializeAutofillDriver();
  void SetBrowserControlsConstraint(ControlsVisibilityReason reason,
                                    cc::BrowserControlsState constraint);
#endif

  void UpdateBrowserVisibleSecurityStateIfNecessary();

  bool SetDataInternal(const std::map<std::string, std::string>& data);

  void EnterFullscreenImpl();

  raw_ptr<BrowserImpl> browser_ = nullptr;
  raw_ptr<ErrorPageDelegate> error_page_delegate_ = nullptr;
  raw_ptr<FullscreenDelegate> fullscreen_delegate_ = nullptr;
  raw_ptr<NewTabDelegate> new_tab_delegate_ = nullptr;
  raw_ptr<GoogleAccountsDelegate> google_accounts_delegate_ = nullptr;
  raw_ptr<ProfileImpl> profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<NavigationControllerImpl> navigation_controller_;
  base::ObserverList<TabObserver>::Unchecked observers_;
  base::CallbackListSubscription locale_change_subscription_;

#if BUILDFLAG(IS_ANDROID)
  raw_ptr<BrowserControlsContainerView> top_controls_container_view_ = nullptr;
  raw_ptr<BrowserControlsContainerView> bottom_controls_container_view_ =
      nullptr;
  base::android::ScopedJavaGlobalRef<jobject> java_impl_;
  std::unique_ptr<BrowserControlsNavigationStateHandler>
      browser_controls_navigation_state_handler_;

  // Last value supplied to UpdateBrowserControlsConstraint(). This *constraint*
  // can be SHOWN, if for example a modal dialog is forcing the controls to be
  // visible, HIDDEN, if for example fullscreen is forcing the controls to be
  // hidden, or BOTH, if either state is viable (e.g. during normal browsing).
  // When BOTH, the actual current state could be showing or hidden.
  cc::BrowserControlsState current_browser_controls_visibility_constraint_ =
      cc::BrowserControlsState::kShown;

  bool desktop_user_agent_enabled_ = false;
#endif

  bool is_fullscreen_ = false;
  // Set to true doing EnterFullscreenModeForTab().
  bool processing_enter_fullscreen_ = false;

  // If true, the fullscreen delegate is called when the tab gains active.
  bool enter_fullscreen_on_gained_active_ = false;

  const std::string guid_;

  std::map<std::string, std::string> data_;
  base::ObserverList<DataObserver>::Unchecked data_observers_;

  std::u16string title_;

  std::unique_ptr<js_injection::JsCommunicationHost> js_communication_host_;

  base::WeakPtrFactory<TabImpl> weak_ptr_factory_for_fullscreen_exit_{this};
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_TAB_IMPL_H_
