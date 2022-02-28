// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_main_parts_fuchsia.h"

#include <fuchsia/ui/app/cpp/fidl.h>
#include <fuchsia/ui/composition/cpp/fidl.h>
#include <fuchsia/ui/scenic/cpp/fidl.h>
#include <fuchsia/ui/views/cpp/fidl.h>
#include <lib/sys/cpp/component_context.h>
#include <lib/ui/scenic/cpp/commands.h>
#include <lib/ui/scenic/cpp/resources.h>
#include <lib/ui/scenic/cpp/session.h>
#include <lib/ui/scenic/cpp/view_identity.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/check.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/fuchsia/process_lifecycle.h"
#include "base/fuchsia/scoped_service_binding.h"
#include "base/notreached.h"
#include "base/numerics/clamped_math.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "ui/gfx/geometry/size.h"
#include "ui/ozone/public/ozone_switches.h"
#include "ui/platform_window/fuchsia/initialize_presenter_api_view.h"

namespace {

// Checks the supported ozone platform with Scenic if no arg is specified.
// TODO(crbug.com/1230150): Delete this after Flatland migration is completed.
void HandleOzonePlatformArgs() {
  base::CommandLine* const launch_args = base::CommandLine::ForCurrentProcess();
  if (launch_args->HasSwitch(switches::kOzonePlatform))
    return;
  fuchsia::ui::scenic::ScenicSyncPtr scenic;
  zx_status_t status =
      base::ComponentContextForProcess()->svc()->Connect(scenic.NewRequest());
  if (status != ZX_OK) {
    ZX_LOG(ERROR, status) << "Couldn't connect to Scenic.";
    return;
  }
  bool scenic_uses_flatland = false;
  scenic->UsesFlatland(&scenic_uses_flatland);
  launch_args->AppendSwitchNative(switches::kOzonePlatform,
                                  scenic_uses_flatland ? "flatland" : "scenic");
}

fuchsia::ui::views::ViewRef CloneViewRef(
    const fuchsia::ui::views::ViewRef& view_ref) {
  fuchsia::ui::views::ViewRef dup;
  zx_status_t status =
      view_ref.reference.duplicate(ZX_RIGHT_SAME_RIGHTS, &dup.reference);
  ZX_CHECK(status == ZX_OK, status) << "zx_object_duplicate";
  return dup;
}

// ViewProviderScenic ----------------------------------------------------------

// ViewProvider implementation that provides a single view and exposes all
// requested views from OzonePlatformScenic inside it. This class owns the top
// level Scenic session.
// TODO(crbug.com/1230150): Delete ViewProviderScenic after Flatland migration
// is completed.
class ViewProviderScenic : public fuchsia::ui::app::ViewProvider {
 public:
  ViewProviderScenic()
      : scenic_(base::ComponentContextForProcess()
                    ->svc()
                    ->Connect<fuchsia::ui::scenic::Scenic>()),
        scenic_session_(scenic_.get(), focuser_.NewRequest()) {
    // This is safe since the callback is overwritten in dtor.
    ui::fuchsia::SetScenicViewPresenter(base::BindRepeating(
        &ViewProviderScenic::PresentView, base::Unretained(this)));

    scenic_.set_error_handler([](zx_status_t status) {
      ZX_LOG(ERROR, status) << " Scenic lost.";
      // Terminate here so that e.g. a Scenic crash results in the browser
      // immediately terminating, without generating a cascading crash report.
      base::Process::TerminateCurrentProcessImmediately(1);
    });
    scenic_session_.set_event_handler(
        fit::bind_member(this, &ViewProviderScenic::OnScenicEvents));
  }
  ViewProviderScenic(const ViewProviderScenic&) = delete;
  ViewProviderScenic& operator=(const ViewProviderScenic&) = delete;
  ~ViewProviderScenic() override {
    ui::fuchsia::SetScenicViewPresenter(
        ui::fuchsia::ScenicPresentViewCallback());
    scenic_.Unbind();
  }

  void PresentView(fuchsia::ui::views::ViewHolderToken view_holder_token,
                   fuchsia::ui::views::ViewRef view_ref) {
    ScenicSubViewData subview = {
        .view_holder = scenic::ViewHolder(&scenic_session_,
                                          std::move(view_holder_token).value,
                                          "subview-holder"),
        .view_ref = std::move(view_ref)};
    if (view_) {
      if (view_properties_) {
        subview.view_holder.SetViewProperties(*view_properties_);
      }
      node_->AddChild(subview.view_holder);
      Present();
    }
    subviews_.push_back(std::move(subview));
  }

  // fuchsia::ui::app::ViewProvider overrides.
  void CreateView(
      zx::eventpair token,
      fidl::InterfaceRequest<fuchsia::sys::ServiceProvider> incoming_services,
      fidl::InterfaceHandle<fuchsia::sys::ServiceProvider> outgoing_services)
      override {
    CreateViewWithViewRef(std::move(token), {}, {});
  }
  void CreateViewWithViewRef(zx::eventpair token,
                             fuchsia::ui::views::ViewRefControl control_ref,
                             fuchsia::ui::views::ViewRef view_ref) override {
    if (view_) {
      LOG(WARNING) << "Unexpected spurious call to CreateViewWithViewRef(). "
                      "Deleting previously created view.";
      subviews_.clear();
      is_node_attached_ = false;
      view_has_focus_ = false;
      node_.reset();
      view_.reset();
      view_properties_ = absl::nullopt;
    }

    view_ = std::make_unique<scenic::View>(
        &scenic_session_, fuchsia::ui::views::ViewToken({std::move(token)}),
        std::move(control_ref), std::move(view_ref), "root-view");
    node_ = std::make_unique<scenic::EntityNode>(&scenic_session_);
    for (auto& subview : subviews_) {
      node_->AddChild(subview.view_holder);
    }
    Present();
  }
  void CreateView2(fuchsia::ui::app::CreateView2Args view_args) override {
    // Unexpected call to CreateView2(). OzonePlatformScenic cannot handle
    // Flatland tokens. Make sure the correct Ozone platform is set.
    NOTREACHED();
  }

 private:
  struct ScenicSubViewData {
    scenic::ViewHolder view_holder;
    fuchsia::ui::views::ViewRef view_ref;
  };

  void OnScenicEvents(std::vector<fuchsia::ui::scenic::Event> events) {
    for (const auto& event : events) {
      if (event.is_gfx() && event.gfx().is_view_properties_changed()) {
        if (event.gfx().view_properties_changed().view_id != view_->id()) {
          LOG(WARNING) << "Received event for unknown view.";
          return;
        }
        UpdateViewProperties(event.gfx().view_properties_changed().properties);
      } else if (event.is_input() && event.input().is_focus()) {
        view_has_focus_ = event.input().focus().focused;
        FocusView();
      }
    }
  }

  void UpdateViewProperties(
      const fuchsia::ui::gfx::ViewProperties& view_properties) {
    const float width =
        view_properties.bounding_box.max.x - view_properties.bounding_box.min.x;
    const float height =
        view_properties.bounding_box.max.y - view_properties.bounding_box.min.y;
    if (width == 0 || height == 0) {
      if (is_node_attached_) {
        node_->Detach();
        is_node_attached_ = false;
      }
    } else {
      if (!is_node_attached_) {
        view_->AddChild(*node_);
        is_node_attached_ = true;
      }
    }

    view_properties_ = view_properties;
    for (auto& subview : subviews_) {
      subview.view_holder.SetViewProperties(*view_properties_);
    }
    Present();
  }

  void FocusView(int tries = 2) {
    if (tries == 0) {
      LOG(ERROR) << "Unable to pass focus to chrome window.";
      return;
    }
    if (!subviews_.empty() && view_has_focus_) {
      focuser_->RequestFocus({CloneViewRef(subviews_.front().view_ref)},
                             [this, tries](auto result) {
                               if (result.is_err()) {
                                 FocusView(tries - 1);
                               }
                             });
    }
  }

  void Present() {
    scenic_session_.Present(
        zx_clock_get_monotonic(),
        [this](fuchsia::images::PresentationInfo info) { FocusView(); });
  }

  fuchsia::ui::scenic::ScenicPtr scenic_;
  fuchsia::ui::views::FocuserPtr focuser_;
  scenic::Session scenic_session_;

  // The view created by this ViewProvider. The view is created lazily when a
  // request is received.
  std::unique_ptr<scenic::View> view_;

  // Entity node for the |view_|.
  std::unique_ptr<scenic::EntityNode> node_;

  // True if the root EntityNode has been added to the View.
  bool is_node_attached_ = false;

  // True is the root view has focus.
  bool view_has_focus_ = false;

  // The holders for all the views that are presented.
  std::vector<ScenicSubViewData> subviews_;

  // The properties of the top level view. They are forwarded to the embedded
  // views.
  absl::optional<fuchsia::ui::gfx::ViewProperties> view_properties_;
};

// ViewProviderFlatland --------------------------------------------------------

// ViewProvider implementation that provides a single view and exposes all
// requested views from OzonePlatformFlatland inside it. This class owns the top
// level Flatland session.
class ViewProviderFlatland : public fuchsia::ui::app::ViewProvider {
 public:
  ViewProviderFlatland() {
    // This is safe since the callback is overwritten in dtor.
    ui::fuchsia::SetFlatlandViewPresenter(base::BindRepeating(
        &ViewProviderFlatland::PresentView, base::Unretained(this)));

    flatland_.set_error_handler([](zx_status_t status) {
      ZX_LOG(ERROR, status) << " Flatland server disconnected.";
      // Terminate here so that e.g. a Scenic crash results in the browser
      // immediately terminating, without generating a cascading crash report.
      base::Process::TerminateCurrentProcessImmediately(1);
    });
    flatland_->SetDebugName("ChromeBrowserMainPartsFuchsia");
    flatland_.events().OnError =
        [](fuchsia::ui::composition::FlatlandError error) {
          LOG(ERROR) << "Flatland error: " << static_cast<int>(error);
        };
    flatland_.events().OnNextFrameBegin =
        fit::bind_member(this, &ViewProviderFlatland::OnNextFrameBegin);

    // Each Flatland session requires defining a root transform.
    flatland_->CreateTransform(kRootTransformId);
    flatland_->SetRootTransform(kRootTransformId);
  }
  ViewProviderFlatland(const ViewProviderFlatland&) = delete;
  ViewProviderFlatland& operator=(const ViewProviderFlatland&) = delete;
  ~ViewProviderFlatland() override {
    ui::fuchsia::SetFlatlandViewPresenter(
        ui::fuchsia::FlatlandPresentViewCallback());
  }

  // fuchsia::ui::app::ViewProvider overrides.
  void CreateView(
      zx::eventpair token,
      fidl::InterfaceRequest<fuchsia::sys::ServiceProvider> incoming_services,
      fidl::InterfaceHandle<fuchsia::sys::ServiceProvider> outgoing_services)
      override {
    // Unexpected call to CreateView(). OzonePlatformFlatland cannot handle Gfx
    // tokens. Make sure the correct Ozone platform is set.
    NOTREACHED();
  }
  void CreateViewWithViewRef(zx::eventpair token,
                             fuchsia::ui::views::ViewRefControl control_ref,
                             fuchsia::ui::views::ViewRef view_ref) override {
    // Unexpected call to CreateViewWithViewRef(). OzonePlatformFlatland cannot
    // handle Gfx tokens. Make sure the correct Ozone platform is set.
    NOTREACHED();
  }
  void CreateView2(fuchsia::ui::app::CreateView2Args view_args) override {
    if (parent_viewport_watcher_.is_bound()) {
      LOG(ERROR) << "Unexpected spurious call to CreateView2().";
      return;
    }

    auto view_identity = scenic::NewViewIdentityOnCreation();
    fuchsia::ui::composition::ViewBoundProtocols flatland_view_protocols;
    flatland_view_protocols.set_view_ref_focused(
        view_ref_focused_.NewRequest());
    flatland_view_protocols.set_view_focuser(focuser_.NewRequest());
    flatland_->CreateView2(std::move(*view_args.mutable_view_creation_token()),
                           std::move(view_identity),
                           std::move(flatland_view_protocols),
                           parent_viewport_watcher_.NewRequest());
    for (auto& subview : subviews_) {
      flatland_->AddChild(kRootTransformId, subview.transform_id);
    }
    // No need to call Present() because OnGetLayout() will trigger Present()
    // after receiving the proper size.
    parent_viewport_watcher_->GetLayout(
        fit::bind_member(this, &ViewProviderFlatland::OnGetLayout));
    view_ref_focused_->Watch(
        fit::bind_member(this, &ViewProviderFlatland::OnViewRefFocused));
  }

  void PresentView(
      fuchsia::ui::views::ViewportCreationToken viewport_creation_token) {
    // Flatland requires a size to be set in CreateViewport() calls, so wait for
    // receiving size before handling the presentation request. Flatland
    // guarantees that the size returned in OnGetLayout() hanging get is
    // non-zero. Size can be zero if we are running this code before
    // OnGetLayout().
    if (view_size_.IsZero()) {
      pending_present_views_.push_back(std::move(viewport_creation_token));
      return;
    }

    FlatlandSubViewData subview;
    subview.transform_id = {next_id_++};
    subview.content_id = {next_id_++};
    flatland_->CreateTransform(subview.transform_id);
    fuchsia::ui::composition::ViewportProperties properties;
    properties.set_logical_size({static_cast<uint32_t>(view_size_.width()),
                                 static_cast<uint32_t>(view_size_.height())});
    flatland_->CreateViewport(
        subview.content_id, std::move(viewport_creation_token),
        std::move(properties), subview.child_view_watcher.NewRequest());
    flatland_->SetContent(subview.transform_id, subview.content_id);
    flatland_->AddChild(kRootTransformId, subview.transform_id);
    subviews_.push_back(std::move(subview));

    MaybePresent();
    subviews_.back().child_view_watcher->GetViewRef(
        [this, index = subviews_.size() - 1](fuchsia::ui::views::ViewRef ref) {
          subviews_[index].child_view_ref = std::move(ref);
          RequestFocus();
        });
  }

 private:
  struct FlatlandSubViewData {
    fuchsia::ui::composition::TransformId transform_id;
    fuchsia::ui::composition::ContentId content_id;
    fuchsia::ui::composition::ChildViewWatcherPtr child_view_watcher;
    fuchsia::ui::views::ViewRef child_view_ref;
  };

  void OnGetLayout(fuchsia::ui::composition::LayoutInfo info) {
    const bool first_received_size = view_size_.IsZero();
    view_size_.SetSize(info.logical_size().width, info.logical_size().height);

    // If there were PresentView() calls before receiving OnGetLayout(), execute
    // them.
    if (first_received_size) {
      for (auto& present_view : std::exchange(pending_present_views_, {})) {
        PresentView(std::move(present_view));
      }
    } else {
      for (const auto& subview : subviews_) {
        fuchsia::ui::composition::ViewportProperties properties;
        properties.set_logical_size(info.logical_size());
        flatland_->SetViewportProperties(subview.content_id,
                                         std::move(properties));
      }
      MaybePresent();
    }

    // Queue another hanging get in case the size changes.
    parent_viewport_watcher_->GetLayout(
        fit::bind_member(this, &ViewProviderFlatland::OnGetLayout));
  }

  void OnViewRefFocused(fuchsia::ui::views::FocusState focus_state) {
    view_has_focus_ = focus_state.focused();
    RequestFocus();

    view_ref_focused_->Watch(
        fit::bind_member(this, &ViewProviderFlatland::OnViewRefFocused));
  }

  void RequestFocus() {
    if (!view_has_focus_ || subviews_.empty() ||
        !subviews_.front().child_view_ref.reference.is_valid())
      return;

    focuser_->RequestFocus(
        CloneViewRef(subviews_.front().child_view_ref),
        [](fuchsia::ui::views::Focuser_RequestFocus_Result result) {
          DCHECK(!result.is_err());
        });
  }

  void MaybePresent() {
    if (present_credits_ == 0) {
      present_after_receiving_credits_ = true;
      return;
    }

    present_after_receiving_credits_ = false;
    --present_credits_;
    Present();
  }

  void Present() {
    fuchsia::ui::composition::PresentArgs present_args;
    present_args.set_requested_presentation_time(0);
    present_args.set_acquire_fences({});
    present_args.set_release_fences({});
    present_args.set_unsquashable(false);
    flatland_->Present(std::move(present_args));
  }

  void OnNextFrameBegin(
      fuchsia::ui::composition::OnNextFrameBeginValues values) {
    present_credits_ =
        base::ClampAdd(present_credits_, values.additional_present_credits());
    if (present_after_receiving_credits_) {
      MaybePresent();
    }
  }

  fuchsia::ui::composition::FlatlandPtr flatland_ = {
      base::ComponentContextForProcess()
          ->svc()
          ->Connect<fuchsia::ui::composition::Flatland>()};

  // The counter used for limiting the number of Present calls to ensure that
  // |flatland_|will not be shut down because if presenting more times than
  // allowed.
  uint32_t present_credits_ = 1;

  // Root transform for |flatland_|. All |subviews_| are added as children.
  static const fuchsia::ui::composition::TransformId kRootTransformId;

  // Autoincrementing value of the next ID to use for |flatland_|.
  uint64_t next_id_ = 2;

  // True if we should queue Present() after receiving credits on
  // OnNextFrameBegin().
  bool present_after_receiving_credits_ = false;

  // True if the top level view has focus.
  bool view_has_focus_ = false;

  // Protocol for watching focus changes.
  fuchsia::ui::views::ViewRefFocusedPtr view_ref_focused_;

  // Protocol for setting focus changes.
  fuchsia::ui::views::FocuserPtr focuser_;

  // Protocol for watching size changes.
  fuchsia::ui::composition::ParentViewportWatcherPtr parent_viewport_watcher_;

  // The layout size of the View occupied by |flatland_| in logical pixels.
  gfx::Size view_size_;

  // Pending ViewportCreationTokens to be processed and added as |subviews_|.
  std::vector<fuchsia::ui::views::ViewportCreationToken> pending_present_views_;

  // The holders for all the views that are presented.
  std::vector<FlatlandSubViewData> subviews_;
};

// static
constexpr fuchsia::ui::composition::TransformId
    ViewProviderFlatland::kRootTransformId{1};

}  // namespace

// ViewProviderRouter ----------------------------------------------------------

// ViewProvider implementation that delegates calls to the correct Ozone
// platform's ViewProvider.
// TODO(crbug.com/1230150): Delete ViewProviderRouter after moving |binding_|
// to ViewProviderFlatland after migration is completed.
class ChromeBrowserMainPartsFuchsia::ViewProviderRouter
    : public fuchsia::ui::app::ViewProvider {
 public:
  ViewProviderRouter(std::unique_ptr<ViewProviderScenic> scenic,
                     std::unique_ptr<ViewProviderFlatland> flatland)
      : scenic_(std::move(scenic)), flatland_(std::move(flatland)) {
    DCHECK(scenic_);
    DCHECK(flatland_);
  }
  ViewProviderRouter(const ViewProviderRouter&) = delete;
  ViewProviderRouter& operator=(const ViewProviderRouter&) = delete;
  ~ViewProviderRouter() override = default;

  // fuchsia::ui::app::ViewProvider overrides.
  void CreateView(
      zx::eventpair token,
      fidl::InterfaceRequest<fuchsia::sys::ServiceProvider> incoming_services,
      fidl::InterfaceHandle<fuchsia::sys::ServiceProvider> outgoing_services)
      override {
    // Chrome is initialized with either Scenic or flatland Ozone platform.
    // Scenic and Flatland calls cannot be combined.
    DCHECK(scenic_);
    flatland_.reset();

    scenic_->CreateView(std::move(token), std::move(incoming_services),
                        std::move(outgoing_services));
  }
  void CreateViewWithViewRef(zx::eventpair token,
                             fuchsia::ui::views::ViewRefControl control_ref,
                             fuchsia::ui::views::ViewRef view_ref) override {
    // Chrome is initialized with either Scenic or flatland Ozone platform.
    // Scenic and Flatland calls cannot be combined.
    DCHECK(scenic_);
    flatland_.reset();

    scenic_->CreateViewWithViewRef(std::move(token), std::move(control_ref),
                                   std::move(view_ref));
  }
  void CreateView2(fuchsia::ui::app::CreateView2Args view_args) override {
    // Chrome is initialized with either Scenic or flatland Ozone platform.
    // Scenic and Flatland calls cannot be combined.
    DCHECK(flatland_);
    scenic_.reset();

    flatland_->CreateView2(std::move(view_args));
  }

 private:
  const base::ScopedServiceBinding<fuchsia::ui::app::ViewProvider> binding_ = {
      base::ComponentContextForProcess()->outgoing().get(), this};
  std::unique_ptr<ViewProviderScenic> scenic_;
  std::unique_ptr<ViewProviderFlatland> flatland_;
};

// ChromeBrowserMainPartsFuchsia -----------------------------------------------

ChromeBrowserMainPartsFuchsia::ChromeBrowserMainPartsFuchsia(
    content::MainFunctionParams parameters,
    StartupData* startup_data)
    : ChromeBrowserMainParts(std::move(parameters), startup_data) {}

ChromeBrowserMainPartsFuchsia::~ChromeBrowserMainPartsFuchsia() = default;

void ChromeBrowserMainPartsFuchsia::ShowMissingLocaleMessageBox() {
  // Locale data should be bundled for all possible platform locales,
  // so crash here to make missing-locale states more visible.
  CHECK(false);
}

int ChromeBrowserMainPartsFuchsia::PreEarlyInitialization() {
  HandleOzonePlatformArgs();
  return ChromeBrowserMainParts::PreEarlyInitialization();
}

int ChromeBrowserMainPartsFuchsia::PreMainMessageLoopRun() {
  // Register the ViewProvider API.
  view_provider_ = std::make_unique<ViewProviderRouter>(
      std::make_unique<ViewProviderScenic>(),
      std::make_unique<ViewProviderFlatland>());

  zx_status_t status =
      base::ComponentContextForProcess()->outgoing()->ServeFromStartupInfo();
  ZX_CHECK(status == ZX_OK, status);

  // Publish the fuchsia.process.lifecycle.Lifecycle service to allow graceful
  // teardown. If there is a |ui_task| then this is a browser-test and graceful
  // shutdown is not required.
  if (!parameters().ui_task) {
    lifecycle_ = std::make_unique<base::ProcessLifecycle>(
        base::BindOnce(&chrome::ExitIgnoreUnloadHandlers));
  }

  return ChromeBrowserMainParts::PreMainMessageLoopRun();
}

void ChromeBrowserMainPartsFuchsia::PostMainMessageLoopRun() {
  // |view_provider_| owns ViewProviderScenic and ViewProviderFlatland. They own
  // the Scenic channels so resetting here will unbind.
  view_provider_.reset();

  ChromeBrowserMainParts::PostMainMessageLoopRun();
}
