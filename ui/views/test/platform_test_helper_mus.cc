// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/platform_test_helper_mus.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/threading/simple_thread.h"
#include "services/service_manager/background_service_manager.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/constants.h"
#include "services/service_manager/public/cpp/manifest_builder.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/viz/public/cpp/manifest.h"
#include "services/ws/ime/test_ime_driver/public/cpp/manifest.h"
#include "services/ws/public/mojom/constants.mojom.h"
#include "services/ws/test_ws/test_manifest.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/mus/window_tree_host_mus.h"
#include "ui/aura/test/env_test_helper.h"
#include "ui/aura/test/mus/input_method_mus_test_api.h"
#include "ui/aura/window.h"
#include "ui/base/ime/input_method_base.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/mus/desktop_window_tree_host_mus.h"
#include "ui/views/mus/mus_client.h"
#include "ui/views/test/views_test_helper_aura.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/native_widget_aura.h"

namespace views {

namespace {

class TestInputMethod : public ui::InputMethodBase {
 public:
  TestInputMethod() : ui::InputMethodBase(nullptr) {}
  ~TestInputMethod() override = default;

  // ui::InputMethod override:
  ui::EventDispatchDetails DispatchKeyEvent(ui::KeyEvent* event) override {
    return ui::EventDispatchDetails();
  }
  ui::AsyncKeyDispatcher* GetAsyncKeyDispatcher() override { return nullptr; }
  void OnCaretBoundsChanged(const ui::TextInputClient* client) override {}
  void CancelComposition(const ui::TextInputClient* client) override {}
  bool IsCandidatePopupOpen() const override { return false; }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestInputMethod);
};

NativeWidget* CreateNativeWidget(const Widget::InitParams& init_params,
                                 internal::NativeWidgetDelegate* delegate) {
  NativeWidget* native_widget =
      MusClient::Get()->CreateNativeWidget(init_params, delegate);
  if (!native_widget)
    return nullptr;

  // Disable sending KeyEvents to IME as tests aren't set up to wait for an
  // ack (and tests run concurrently).
  aura::WindowTreeHostMus* window_tree_host_mus =
      static_cast<aura::WindowTreeHostMus*>(
          static_cast<DesktopNativeWidgetAura*>(native_widget)->host());

  static TestInputMethod test_input_method;
  window_tree_host_mus->SetSharedInputMethod(&test_input_method);
  return native_widget;
}

}  // namespace

class PlatformTestHelperMus::ServiceManagerConnection {
 public:
  ServiceManagerConnection()
      : thread_("Persistent service_manager connections"),
        default_service_binding_(&default_service_) {
    base::WaitableEvent wait(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
    base::Thread::Options options;
    thread_.StartWithOptions(options);
    thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &ServiceManagerConnection::SetUpConnectionsOnBackgroundThread,
            base::Unretained(this), &wait));
    wait.Wait();
  }

  ~ServiceManagerConnection() {
    base::WaitableEvent wait(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
    thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &ServiceManagerConnection::TearDownConnectionsOnBackgroundThread,
            base::Unretained(this), &wait));
    wait.Wait();
  }

  std::unique_ptr<MusClient> CreateMusClient() {
    MusClient::InitParams params;
    params.connector = GetConnector();
    params.identity = service_manager_identity_;
    // The window tree client might have been created already by the test suite
    // (e.g. AuraTestSuiteSetup).
    params.window_tree_client =
        aura::test::EnvTestHelper().GetWindowTreeClient();
    params.running_in_ws_process = ::features::IsSingleProcessMash();
    return std::make_unique<MusClient>(params);
  }

 private:
  service_manager::Connector* GetConnector() {
    service_manager_connector_.reset();
    base::WaitableEvent wait(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
    thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&ServiceManagerConnection::CloneConnector,
                                  base::Unretained(this), &wait));
    wait.Wait();
    DCHECK(service_manager_connector_);
    return service_manager_connector_.get();
  }

  void CloneConnector(base::WaitableEvent* wait) {
    service_manager_connector_ =
        default_service_binding_.GetConnector()->Clone();
    wait->Signal();
  }

  void SetUpConnectionsOnBackgroundThread(base::WaitableEvent* wait) {
    static const char* kServiceName = "views_unittests";

    background_service_manager_ =
        std::make_unique<service_manager::BackgroundServiceManager>(
            std::vector<service_manager::Manifest>{
                test_ws::GetManifest(), test_ime_driver::GetManifest(),
                viz::GetManifest(),
                service_manager::ManifestBuilder()
                    .WithServiceName(kServiceName)
                    .RequireCapability("*", "app")
                    .RequireCapability("*", "test")
                    .Build()});

    service_manager::mojom::ServicePtrInfo service;
    default_service_binding_.Bind(mojo::MakeRequest(&service));
    // The service name matches the name field in unittests_manifest.json.
    background_service_manager_->RegisterService(
        service_manager::Identity(kServiceName,
                                  service_manager::kSystemInstanceGroup,
                                  base::Token{}, base::Token::CreateRandom()),
        std::move(service), mojo::NullReceiver());
    service_manager_connector_ =
        default_service_binding_.GetConnector()->Clone();
    service_manager_identity_ = default_service_binding_.identity();
    wait->Signal();
  }

  void TearDownConnectionsOnBackgroundThread(base::WaitableEvent* wait) {
    default_service_binding_.Close();
    background_service_manager_.reset();
    wait->Signal();
  }

  base::Thread thread_;
  std::unique_ptr<service_manager::BackgroundServiceManager>
      background_service_manager_;
  service_manager::Service default_service_;
  service_manager::ServiceBinding default_service_binding_;
  std::unique_ptr<service_manager::Connector> service_manager_connector_;
  service_manager::Identity service_manager_identity_;

  DISALLOW_COPY_AND_ASSIGN(ServiceManagerConnection);
};

PlatformTestHelperMus::PlatformTestHelperMus() {
  service_manager_connection_ = std::make_unique<ServiceManagerConnection>();
  mus_client_ = service_manager_connection_->CreateMusClient();
  ViewsDelegate::GetInstance()->set_native_widget_factory(
      base::BindRepeating(&CreateNativeWidget));
}

PlatformTestHelperMus::~PlatformTestHelperMus() = default;

void PlatformTestHelperMus::OnTestHelperCreated(ViewsTestHelper* helper) {
  static_cast<ViewsTestHelperAura*>(helper)->EnableMusWithWindowTreeClient(
      mus_client_->window_tree_client());
}

void PlatformTestHelperMus::SimulateNativeDestroy(Widget* widget) {
  aura::WindowTreeHostMus* window_tree_host =
      static_cast<aura::WindowTreeHostMus*>(widget->GetNativeView()->GetHost());
  static_cast<aura::WindowTreeClientDelegate*>(mus_client_.get())
      ->OnEmbedRootDestroyed(window_tree_host);
}

void PlatformTestHelperMus::InitializeContextFactory(
    ui::ContextFactory** context_factory,
    ui::ContextFactoryPrivate** context_factory_private) {
  *context_factory = &context_factory_;
  *context_factory_private = nullptr;
}

}  // namespace views
