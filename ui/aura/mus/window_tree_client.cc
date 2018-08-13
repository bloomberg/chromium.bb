// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/window_tree_client.h"

#include <stddef.h>

#include <string>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/threading/thread.h"
#include "cc/base/switches.h"
#include "components/discardable_memory/client/client_discardable_shared_memory_manager.h"
#include "mojo/public/cpp/bindings/map.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/ui/common/util.h"
#include "services/ui/public/cpp/gpu/gpu.h"
#include "services/ui/public/cpp/property_type_converters.h"
#include "services/ui/public/interfaces/constants.mojom.h"
#include "services/ui/public/interfaces/window_tree_host_factory.mojom.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/client/transient_window_client.h"
#include "ui/aura/env.h"
#include "ui/aura/env_input_state_controller.h"
#include "ui/aura/mus/capture_synchronizer.h"
#include "ui/aura/mus/drag_drop_controller_mus.h"
#include "ui/aura/mus/embed_root.h"
#include "ui/aura/mus/embed_root_delegate.h"
#include "ui/aura/mus/focus_synchronizer.h"
#include "ui/aura/mus/in_flight_change.h"
#include "ui/aura/mus/input_method_mus.h"
#include "ui/aura/mus/mus_context_factory.h"
#include "ui/aura/mus/property_converter.h"
#include "ui/aura/mus/property_utils.h"
#include "ui/aura/mus/topmost_window_tracker.h"
#include "ui/aura/mus/window_mus.h"
#include "ui/aura/mus/window_port_mus.h"
#include "ui/aura/mus/window_tree_client_delegate.h"
#include "ui/aura/mus/window_tree_client_observer.h"
#include "ui/aura/mus/window_tree_client_test_observer.h"
#include "ui/aura/mus/window_tree_host_mus.h"
#include "ui/aura/mus/window_tree_host_mus_init_params.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_port_for_shutdown.h"
#include "ui/aura/window_tracker.h"
#include "ui/base/layout.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches_util.h"
#include "ui/base/ui_base_types.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"

namespace aura {
namespace {

struct WindowPortPropertyDataMus : public ui::PropertyData {
  std::string transport_name;
  std::unique_ptr<std::vector<uint8_t>> transport_value;
};

// Handles acknowledgment of an input event, either immediately when a nested
// message loop starts, or upon destruction.
class EventAckHandler : public base::RunLoop::NestingObserver {
 public:
  explicit EventAckHandler(EventResultCallback ack_callback)
      : ack_callback_(std::move(ack_callback)) {
    DCHECK(ack_callback_);
    base::RunLoop::AddNestingObserverOnCurrentThread(this);
  }

  ~EventAckHandler() override {
    base::RunLoop::RemoveNestingObserverOnCurrentThread(this);
    if (ack_callback_) {
      std::move(ack_callback_)
          .Run(handled_ ? ui::mojom::EventResult::HANDLED
                        : ui::mojom::EventResult::UNHANDLED);
    }
  }

  void set_handled(bool handled) { handled_ = handled; }

  // base::RunLoop::NestingObserver:
  void OnBeginNestedRunLoop() override {
    // Acknowledge the event immediately if a nested run loop starts.
    // Otherwise we appear unresponsive for the life of the nested run loop.
    if (ack_callback_)
      std::move(ack_callback_).Run(ui::mojom::EventResult::HANDLED);
  }

 private:
  EventResultCallback ack_callback_;
  bool handled_ = false;

  DISALLOW_COPY_AND_ASSIGN(EventAckHandler);
};

WindowTreeHostMus* GetWindowTreeHostMus(Window* window) {
  return WindowTreeHostMus::ForWindow(window);
}

WindowTreeHostMus* GetWindowTreeHostMus(WindowMus* window) {
  return GetWindowTreeHostMus(window->GetWindow());
}

bool IsInternalProperty(const void* key) {
  return key == client::kModalKey || key == client::kChildModalParentKey;
}

// Create and return a MouseEvent or TouchEvent from |event| if |event| is a
// PointerEvent, otherwise return the copy of |event|.
std::unique_ptr<ui::Event> MapEvent(const ui::Event& event) {
  if (event.IsPointerEvent()) {
    const ui::PointerEvent& pointer_event = *event.AsPointerEvent();
    // Use a switch statement in case more pointer types are added.
    switch (pointer_event.pointer_details().pointer_type) {
      case ui::EventPointerType::POINTER_TYPE_MOUSE:
        if (event.type() == ui::ET_POINTER_WHEEL_CHANGED)
          return std::make_unique<ui::MouseWheelEvent>(pointer_event);
        return std::make_unique<ui::MouseEvent>(pointer_event);
      case ui::EventPointerType::POINTER_TYPE_TOUCH:
      case ui::EventPointerType::POINTER_TYPE_PEN:
        return std::make_unique<ui::TouchEvent>(pointer_event);
      case ui::EventPointerType::POINTER_TYPE_ERASER:
        NOTIMPLEMENTED();
        break;
      case ui::EventPointerType::POINTER_TYPE_UNKNOWN:
        NOTREACHED();
        break;
    }
  }
  return ui::Event::Clone(event);
}

}  // namespace

// static
std::unique_ptr<WindowTreeClient> WindowTreeClient::CreateForEmbedding(
    service_manager::Connector* connector,
    WindowTreeClientDelegate* delegate,
    ui::mojom::WindowTreeClientRequest request,
    bool create_discardable_memory) {
  std::unique_ptr<WindowTreeClient> wtc(
      new WindowTreeClient(connector, delegate, std::move(request), nullptr,
                           create_discardable_memory));
  return wtc;
}

// static
std::unique_ptr<WindowTreeClient> WindowTreeClient::CreateForWindowTreeFactory(
    service_manager::Connector* connector,
    WindowTreeClientDelegate* delegate,
    bool create_discardable_memory,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner) {
  std::unique_ptr<WindowTreeClient> wtc(new WindowTreeClient(
      connector, delegate, nullptr, nullptr, create_discardable_memory));
  ui::mojom::WindowTreeFactoryPtr factory;
  connector->BindInterface(ui::mojom::kServiceName, &factory);
  ui::mojom::WindowTreePtr window_tree;
  ui::mojom::WindowTreeClientPtr client;
  wtc->binding_.Bind(MakeRequest(&client));
  factory->CreateWindowTree(MakeRequest(&window_tree), std::move(client));
  wtc->SetWindowTree(std::move(window_tree));
  return wtc;
}

// static
std::unique_ptr<WindowTreeClient>
WindowTreeClient::CreateForWindowTreeHostFactory(
    service_manager::Connector* connector,
    WindowTreeClientDelegate* delegate,
    bool create_discardable_memory) {
  std::unique_ptr<WindowTreeClient> wtc(new WindowTreeClient(
      connector, delegate, nullptr, nullptr, create_discardable_memory));
  ui::mojom::WindowTreeHostFactoryPtr factory;
  connector->BindInterface(ui::mojom::kServiceName, &factory);

  ui::mojom::WindowTreeHostPtr window_tree_host;
  ui::mojom::WindowTreeClientPtr client;
  wtc->binding_.Bind(MakeRequest(&client));
  factory->CreateWindowTreeHost(MakeRequest(&window_tree_host),
                                std::move(client));
  return wtc;
}

WindowTreeClient::~WindowTreeClient() {
  in_destructor_ = true;

  if (discardable_shared_memory_manager_)
    base::DiscardableMemoryAllocator::SetInstance(nullptr);

  for (WindowTreeClientObserver& observer : observers_)
    observer.OnWillDestroyClient(this);

  capture_synchronizer_.reset();

  // Some tests may not create a TransientWindowClient.
  if (client::GetTransientWindowClient())
    client::GetTransientWindowClient()->RemoveObserver(this);

  Env* env = Env::GetInstance();
  if (compositor_context_factory_ &&
      env->context_factory() == compositor_context_factory_.get()) {
    env->set_context_factory(initial_context_factory_);
  }

  // Allow for windows to exist (and be created) after we are destroyed. This
  // is necessary because of shutdown ordering (WindowTreeClient is destroyed
  // before windows).
  in_shutdown_ = true;

  // Windows of type WindowMusType::OTHER were implicitly created from the
  // server and may not have been destroyed. Delete them to ensure we don't
  // leak.
  WindowTracker windows_to_destroy;
  for (auto& pair : windows_) {
    if (pair.second->window_mus_type() == WindowMusType::OTHER)
      windows_to_destroy.Add(pair.second->GetWindow());
  }
  while (!windows_to_destroy.windows().empty())
    delete windows_to_destroy.Pop();

  IdToWindowMap windows;
  std::swap(windows, windows_);
  for (auto& pair : windows)
    WindowPortForShutdown::Install(pair.second->GetWindow());

  // EmbedRoots keep a reference to this; so they must all be destroyed before
  // the destructor completes.
  DCHECK(embed_roots_.empty());

  env->WindowTreeClientDestroyed(this);
  CHECK(windows_.empty());
}

bool WindowTreeClient::WaitForDisplays() {
  if (got_initial_displays_)
    return true;

  bool valid_wait = true;
  // TODO(sky): having to block here is not ideal. http://crbug.com/594852.
  while (!got_initial_displays_ && valid_wait)
    valid_wait = binding_.WaitForIncomingMethodCall();
  return valid_wait;
}

void WindowTreeClient::SetCanFocus(Window* window, bool can_focus) {
  DCHECK(tree_);
  DCHECK(window);
  tree_->SetCanFocus(WindowMus::Get(window)->server_id(), can_focus);
}

void WindowTreeClient::SetCursor(WindowMus* window,
                                 const ui::CursorData& old_cursor,
                                 const ui::CursorData& new_cursor) {
  DCHECK(tree_);

  const uint32_t change_id = ScheduleInFlightChange(
      std::make_unique<InFlightCursorChange>(window, old_cursor));
  tree_->SetCursor(change_id, window->server_id(), new_cursor);
}

void WindowTreeClient::SetWindowTextInputState(
    WindowMus* window,
    ui::mojom::TextInputStatePtr state) {
  DCHECK(tree_);
  tree_->SetWindowTextInputState(window->server_id(), std::move(state));
}

void WindowTreeClient::SetImeVisibility(WindowMus* window,
                                        bool visible,
                                        ui::mojom::TextInputStatePtr state) {
  DCHECK(tree_);
  tree_->SetImeVisibility(window->server_id(), visible, std::move(state));
}

void WindowTreeClient::SetHitTestMask(
    WindowMus* window,
    const base::Optional<gfx::Rect>& mask_rect) {
  base::Optional<gfx::Rect> out_rect = base::nullopt;
  if (mask_rect)
    out_rect = mask_rect.value();

  tree_->SetHitTestMask(window->server_id(), out_rect);
}

void WindowTreeClient::Embed(Window* window,
                             ui::mojom::WindowTreeClientPtr client,
                             uint32_t flags,
                             ui::mojom::WindowTree::EmbedCallback callback) {
  DCHECK(tree_);
  // Window::Init() must be called before Embed() (otherwise the server hasn't
  // been told about the window).
  DCHECK(window->layer());
  if (!window->children().empty()) {
    // The window server removes all children before embedding. In other words,
    // it's generally an error to Embed() with existing children. So, fail
    // early.
    std::move(callback).Run(false);
    return;
  }

  tree_->Embed(WindowMus::Get(window)->server_id(), std::move(client), flags,
               std::move(callback));
}

void WindowTreeClient::ScheduleEmbed(
    ui::mojom::WindowTreeClientPtr client,
    base::OnceCallback<void(const base::UnguessableToken&)> callback) {
  tree_->ScheduleEmbed(std::move(client), std::move(callback));
}

void WindowTreeClient::EmbedUsingToken(
    Window* window,
    const base::UnguessableToken& token,
    uint32_t flags,
    ui::mojom::WindowTree::EmbedCallback callback) {
  DCHECK(tree_);
  // Window::Init() must be called before Embed() (otherwise the server hasn't
  // been told about the window).
  DCHECK(window->layer());
  if (!window->children().empty()) {
    // The window server removes all children before embedding. In other words,
    // it's generally an error to Embed() with existing children. So, fail
    // early.
    std::move(callback).Run(false);
    return;
  }

  tree_->EmbedUsingToken(WindowMus::Get(window)->server_id(), token, flags,
                         std::move(callback));
}

void WindowTreeClient::AttachCompositorFrameSink(
    ui::Id window_id,
    viz::mojom::CompositorFrameSinkRequest compositor_frame_sink,
    viz::mojom::CompositorFrameSinkClientPtr client) {
  DCHECK(tree_);
  tree_->AttachCompositorFrameSink(window_id, std::move(compositor_frame_sink),
                                   std::move(client));
}

std::unique_ptr<EmbedRoot> WindowTreeClient::CreateEmbedRoot(
    EmbedRootDelegate* delegate) {
  std::unique_ptr<EmbedRoot> embed_root =
      base::WrapUnique(new EmbedRoot(this, delegate, next_window_id_++));
  embed_roots_.insert(embed_root.get());
  return embed_root;
}

WindowTreeClient::WindowTreeClient(
    service_manager::Connector* connector,
    WindowTreeClientDelegate* delegate,
    mojo::InterfaceRequest<ui::mojom::WindowTreeClient> request,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    bool create_discardable_memory)
    : connector_(connector),
      next_window_id_(1),
      next_change_id_(1),
      delegate_(delegate),
      binding_(this),
      tree_(nullptr),
      in_destructor_(false),
      weak_factory_(this) {
  DCHECK(delegate_);
  // Allow for a null request in tests.
  if (request.is_pending())
    binding_.Bind(std::move(request));
  // Some tests may not create a TransientWindowClient.
  if (client::GetTransientWindowClient())
    client::GetTransientWindowClient()->AddObserver(this);
  if (connector) {  // |connector| can be null in tests.
    if (!io_task_runner) {
      // |io_task_runner| is typically null. When used in the browser process,
      // |io_task_runner| is the browser's IO thread.
      io_thread_ = std::make_unique<base::Thread>("IOThread");
      base::Thread::Options thread_options(base::MessageLoop::TYPE_IO, 0);
      thread_options.priority = base::ThreadPriority::NORMAL;
      CHECK(io_thread_->StartWithOptions(thread_options));
      io_task_runner = io_thread_->task_runner();
    }

    gpu_ = ui::Gpu::Create(connector, ui::mojom::kServiceName, io_task_runner);
    compositor_context_factory_ =
        std::make_unique<MusContextFactory>(gpu_.get());
    initial_context_factory_ = Env::GetInstance()->context_factory();
    Env::GetInstance()->set_context_factory(compositor_context_factory_.get());

    // WindowServerTest will create more than one WindowTreeClient. We will not
    // create the discardable memory manager for those tests.
    if (create_discardable_memory) {
      discardable_memory::mojom::DiscardableSharedMemoryManagerPtr manager_ptr;
      connector->BindInterface(ui::mojom::kServiceName, &manager_ptr);
      discardable_shared_memory_manager_ = std::make_unique<
          discardable_memory::ClientDiscardableSharedMemoryManager>(
          std::move(manager_ptr), std::move(io_task_runner));
      base::DiscardableMemoryAllocator::SetInstance(
          discardable_shared_memory_manager_.get());
    }
  }
}

void WindowTreeClient::RegisterWindowMus(WindowMus* window) {
  DCHECK(windows_.find(window->server_id()) == windows_.end());
  windows_[window->server_id()] = window;
  if (window->GetWindow()) {
    auto* port = WindowPortMus::Get(window->GetWindow());
    window->GetWindow()->set_frame_sink_id(
        port->GenerateFrameSinkIdFromServerId());
  }
}

WindowMus* WindowTreeClient::GetWindowByServerId(ui::Id id) {
  IdToWindowMap::const_iterator it = windows_.find(id);
  return it != windows_.end() ? it->second : nullptr;
}

bool WindowTreeClient::IsWindowKnown(aura::Window* window) {
  WindowMus* window_mus = WindowMus::Get(window);
  return windows_.count(window_mus->server_id()) > 0;
}

InFlightChange* WindowTreeClient::GetOldestInFlightChangeMatching(
    const InFlightChange& change) {
  for (const auto& pair : in_flight_map_) {
    if (pair.second->window() == change.window() &&
        pair.second->change_type() == change.change_type() &&
        pair.second->Matches(change)) {
      return pair.second.get();
    }
  }
  return nullptr;
}

uint32_t WindowTreeClient::ScheduleInFlightChange(
    std::unique_ptr<InFlightChange> change) {
  DCHECK(!change->window() ||
         windows_.count(change->window()->server_id()) > 0);
  ChangeType t = change->change_type();
  const uint32_t change_id = next_change_id_++;
  in_flight_map_[change_id] = std::move(change);
  for (auto& observer : test_observers_)
    observer.OnChangeStarted(change_id, t);
  return change_id;
}

bool WindowTreeClient::ApplyServerChangeToExistingInFlightChange(
    const InFlightChange& change) {
  InFlightChange* existing_change = GetOldestInFlightChangeMatching(change);
  if (!existing_change)
    return false;

  existing_change->SetRevertValueFrom(change);
  return true;
}

void WindowTreeClient::BuildWindowTree(
    const std::vector<ui::mojom::WindowDataPtr>& windows) {
  for (const auto& window_data : windows)
    CreateOrUpdateWindowFromWindowData(*window_data);
}

void WindowTreeClient::CreateOrUpdateWindowFromWindowData(
    const ui::mojom::WindowData& window_data) {
  WindowMus* parent = window_data.parent_id == kInvalidServerId
                          ? nullptr
                          : GetWindowByServerId(window_data.parent_id);
  WindowMus* window = GetWindowByServerId(window_data.window_id);
  if (!window)
    window = NewWindowFromWindowData(parent, window_data);
  else if (parent)
    parent->AddChildFromServer(window);

  if (window_data.transient_parent_id == kInvalidServerId)
    return;

  // Some tests may not create a TransientWindowClient.
  client::TransientWindowClient* transient_window_client =
      client::GetTransientWindowClient();
  if (!transient_window_client)
    return;

  // Adjust the transient parent if necessary.
  Window* existing_transient_parent =
      transient_window_client->GetTransientParent(window->GetWindow());
  WindowMus* new_transient_parent =
      GetWindowByServerId(window_data.transient_parent_id);
  if (!new_transient_parent && existing_transient_parent) {
    WindowMus::Get(existing_transient_parent)
        ->RemoveTransientChildFromServer(window);
  } else if (new_transient_parent &&
             new_transient_parent->GetWindow() != existing_transient_parent) {
    if (existing_transient_parent) {
      WindowMus::Get(existing_transient_parent)
          ->RemoveTransientChildFromServer(window);
    }
    new_transient_parent->AddTransientChildFromServer(window);
  }
}

std::unique_ptr<WindowPortMus> WindowTreeClient::CreateWindowPortMus(
    const ui::mojom::WindowData& window_data,
    WindowMusType window_mus_type) {
  std::unique_ptr<WindowPortMus> window_port_mus(
      std::make_unique<WindowPortMus>(this, window_mus_type));
  window_port_mus->set_server_id(window_data.window_id);
  RegisterWindowMus(window_port_mus.get());
  return window_port_mus;
}

void WindowTreeClient::SetLocalPropertiesFromServerProperties(
    WindowMus* window,
    const ui::mojom::WindowData& window_data) {
  for (auto& pair : window_data.properties)
    window->SetPropertyFromServer(pair.first, &pair.second);
}

std::unique_ptr<WindowTreeHostMus> WindowTreeClient::CreateWindowTreeHost(
    WindowMusType window_mus_type,
    const ui::mojom::WindowData& window_data,
    int64_t display_id,
    const base::Optional<viz::LocalSurfaceId>& local_surface_id) {
  std::unique_ptr<WindowPortMus> window_port =
      CreateWindowPortMus(window_data, window_mus_type);
  roots_.insert(window_port.get());
  WindowTreeHostMusInitParams init_params;
  init_params.window_port = std::move(window_port);
  init_params.window_tree_client = this;
  init_params.display_id = display_id;
  std::unique_ptr<WindowTreeHostMus> window_tree_host =
      std::make_unique<WindowTreeHostMus>(std::move(init_params));
  window_tree_host->InitHost();
  SetLocalPropertiesFromServerProperties(
      WindowMus::Get(window_tree_host->window()), window_data);
  if (window_data.visible) {
    SetWindowVisibleFromServer(WindowMus::Get(window_tree_host->window()),
                               true);
  }
  WindowMus* window = WindowMus::Get(window_tree_host->window());

  SetWindowBoundsFromServer(window, window_data.bounds, local_surface_id);
  return window_tree_host;
}

WindowMus* WindowTreeClient::NewWindowFromWindowData(
    WindowMus* parent,
    const ui::mojom::WindowData& window_data) {
  // This function is only called for windows coming from other clients.
  std::unique_ptr<WindowPortMus> window_port_mus(
      CreateWindowPortMus(window_data, WindowMusType::OTHER));
  WindowPortMus* window_port_mus_ptr = window_port_mus.get();
  // Children of windows created from another client need to be restacked by
  // the client that created them. To do otherwise means two clients will
  // attempt to restack the same windows, leading to raciness (and most likely
  // be rejected by the server anyway).
  window_port_mus_ptr->should_restack_transient_children_ = false;
  Window* window = new Window(nullptr, std::move(window_port_mus));
  WindowMus* window_mus = window_port_mus_ptr;
  std::map<std::string, std::vector<uint8_t>> properties =
      mojo::FlatMapToMap(window_data.properties);
  SetWindowType(window, GetWindowTypeFromProperties(properties));
  window->Init(ui::LAYER_NOT_DRAWN);
  SetLocalPropertiesFromServerProperties(window_mus, window_data);
  window_mus->SetBoundsFromServer(window_data.bounds, base::nullopt);
  if (parent)
    parent->AddChildFromServer(window_port_mus_ptr);
  if (window_data.visible)
    window_mus->SetVisibleFromServer(true);
  return window_port_mus_ptr;
}

void WindowTreeClient::SetWindowTree(ui::mojom::WindowTreePtr window_tree_ptr) {
  tree_ptr_ = std::move(window_tree_ptr);

  WindowTreeConnectionEstablished(tree_ptr_.get());
  tree_ptr_->GetCursorLocationMemory(
      base::BindOnce(&WindowTreeClient::OnReceivedCursorLocationMemory,
                     weak_factory_.GetWeakPtr()));

  tree_ptr_.set_connection_error_handler(base::BindOnce(
      &WindowTreeClient::OnConnectionLost, weak_factory_.GetWeakPtr()));
}

void WindowTreeClient::WindowTreeConnectionEstablished(
    ui::mojom::WindowTree* window_tree) {
  tree_ = window_tree;

  drag_drop_controller_ = std::make_unique<DragDropControllerMus>(this, tree_);
  capture_synchronizer_ = std::make_unique<CaptureSynchronizer>(this, tree_);
  focus_synchronizer_ = std::make_unique<FocusSynchronizer>(this, tree_);
}

void WindowTreeClient::OnConnectionLost() {
  delegate_->OnLostConnection(this);
}

bool WindowTreeClient::HandleInternalPropertyChanged(WindowMus* window,
                                                     const void* key,
                                                     int64_t old_value) {
  if (key == client::kModalKey) {
    const uint32_t change_id =
        ScheduleInFlightChange(std::make_unique<InFlightSetModalTypeChange>(
            window, static_cast<ui::ModalType>(old_value)));
    tree_->SetModalType(change_id, window->server_id(),
                        window->GetWindow()->GetProperty(client::kModalKey));
    return true;
  }
  if (key == client::kChildModalParentKey) {
    const uint32_t change_id =
        ScheduleInFlightChange(std::make_unique<CrashInFlightChange>(
            window, ChangeType::CHILD_MODAL_PARENT));
    Window* child_modal_parent =
        window->GetWindow()->GetProperty(client::kChildModalParentKey);
    tree_->SetChildModalParent(
        change_id, window->server_id(),
        child_modal_parent ? WindowMus::Get(child_modal_parent)->server_id()
                           : kInvalidServerId);
    return true;
  }
  return false;
}

void WindowTreeClient::OnEmbedImpl(
    ui::mojom::WindowTree* window_tree,
    ui::mojom::WindowDataPtr root_data,
    int64_t display_id,
    ui::Id focused_window_id,
    bool drawn,
    const base::Optional<viz::LocalSurfaceId>& local_surface_id) {
  WindowTreeConnectionEstablished(window_tree);

  DCHECK(roots_.empty());
  std::unique_ptr<WindowTreeHostMus> window_tree_host = CreateWindowTreeHost(
      WindowMusType::EMBED, *root_data, display_id, local_surface_id);

  focus_synchronizer_->SetFocusFromServer(
      GetWindowByServerId(focused_window_id));

  delegate_->OnEmbed(std::move(window_tree_host));
}

EmbedRoot* WindowTreeClient::GetEmbedRootWithRootWindow(aura::Window* window) {
  for (EmbedRoot* embed_root : embed_roots_) {
    if (embed_root->window() == window)
      return embed_root;
  }
  return nullptr;
}

void WindowTreeClient::OnEmbedRootDestroyed(EmbedRoot* embed_root) {
  embed_roots_.erase(embed_root);
}

EventResultCallback WindowTreeClient::CreateEventResultCallback(
    int32_t event_id) {
  return base::BindOnce(&ui::mojom::WindowTree::OnWindowInputEventAck,
                        base::Unretained(tree_), event_id);
}

void WindowTreeClient::OnReceivedCursorLocationMemory(
    mojo::ScopedSharedBufferHandle handle) {
  cursor_location_mapping_ = handle->Map(sizeof(base::subtle::Atomic32));
  DCHECK(cursor_location_mapping_);
}

void WindowTreeClient::SetWindowBoundsFromServer(
    WindowMus* window,
    const gfx::Rect& revert_bounds,
    const base::Optional<viz::LocalSurfaceId>& local_surface_id) {
  if (IsRoot(window)) {
    // This uses GetScaleFactorForNativeView() as it's called at a time when the
    // scale factor may not have been applied to the Compositor yet. In
    // particular, when the scale-factor changes this is called in terms of the
    // scale factor set on the display. It's the call to
    // SetBoundsFromServerInPixels() that is responsible for updating the scale
    // factor in the Compositor.
    const float dsf = ui::GetScaleFactorForNativeView(window->GetWindow());
    GetWindowTreeHostMus(window)->SetBoundsFromServerInPixels(
        gfx::ConvertRectToPixel(dsf, revert_bounds),
        local_surface_id ? *local_surface_id : viz::LocalSurfaceId());
    return;
  }

  window->SetBoundsFromServer(revert_bounds, local_surface_id);
}

void WindowTreeClient::SetWindowTransformFromServer(
    WindowMus* window,
    const gfx::Transform& transform) {
  window->SetTransformFromServer(transform);
}

void WindowTreeClient::SetWindowVisibleFromServer(WindowMus* window,
                                                  bool visible) {
  if (!IsRoot(window)) {
    window->SetVisibleFromServer(visible);
    return;
  }

  std::unique_ptr<WindowMusChangeData> data =
      window->PrepareForServerVisibilityChange(visible);
  WindowTreeHostMus* window_tree_host = GetWindowTreeHostMus(window);
  if (visible)
    window_tree_host->Show();
  else
    window_tree_host->Hide();
}

void WindowTreeClient::ScheduleInFlightBoundsChange(
    WindowMus* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds) {
  const uint32_t change_id =
      ScheduleInFlightChange(std::make_unique<InFlightBoundsChange>(
          this, window, old_bounds, window->GetLocalSurfaceId()));
  base::Optional<viz::LocalSurfaceId> local_surface_id;
  if (window->window_mus_type() == WindowMusType::EMBED_IN_OWNER ||
      window->HasLocalLayerTreeFrameSink()) {
    local_surface_id = window->GetOrAllocateLocalSurfaceId(
        gfx::ConvertRectToPixel(window->GetDeviceScaleFactor(), new_bounds)
            .size());
    // |window_tree_host| may be null if this is called during creation of
    // the window associated with the WindowTreeHostMus.
    WindowTreeHost* window_tree_host = window->GetWindow()->GetHost();
    if (window_tree_host)
      window_tree_host->compositor()->OnChildResizing();
  }
  tree_->SetWindowBounds(change_id, window->server_id(), new_bounds,
                         local_surface_id);
}

void WindowTreeClient::OnWindowMusCreated(WindowMus* window) {
  if (window->server_id() != kInvalidServerId)
    return;

  window->set_server_id(next_window_id_++);
  RegisterWindowMus(window);

  DCHECK(!IsRoot(window));

  PropertyConverter* property_converter = delegate_->GetPropertyConverter();
  base::flat_map<std::string, std::vector<uint8_t>> transport_properties =
      property_converter->GetTransportProperties(window->GetWindow());

  const uint32_t change_id = ScheduleInFlightChange(
      std::make_unique<CrashInFlightChange>(window, ChangeType::NEW_WINDOW));
  tree_->NewWindow(change_id, window->server_id(),
                   std::move(transport_properties));
  if (window->GetWindow()->event_targeting_policy() !=
      ui::mojom::EventTargetingPolicy::TARGET_AND_DESCENDANTS) {
    SetEventTargetingPolicy(window,
                            window->GetWindow()->event_targeting_policy());
  }
}

void WindowTreeClient::OnWindowMusDestroyed(WindowMus* window, Origin origin) {
  if (focus_synchronizer_->focused_window() == window)
    focus_synchronizer_->OnFocusedWindowDestroyed();

  // If we're |in_shutdown_| there is no point in telling the server about the
  // deletion. The connection to the server is about to be dropped and the
  // server will take appropriate action.
  // TODO: decide how to deal with windows not owned by this client.
  if (!in_shutdown_ && origin == Origin::CLIENT &&
      (WasCreatedByThisClient(window) || IsRoot(window))) {
    const uint32_t change_id =
        ScheduleInFlightChange(std::make_unique<CrashInFlightChange>(
            window, ChangeType::DELETE_WINDOW));
    tree_->DeleteWindow(change_id, window->server_id());
  }

  windows_.erase(window->server_id());

  // Remove any InFlightChanges associated with the window.
  std::set<uint32_t> in_flight_change_ids_to_remove;
  for (const auto& pair : in_flight_map_) {
    if (pair.second->window() == window)
      in_flight_change_ids_to_remove.insert(pair.first);
  }
  for (auto change_id : in_flight_change_ids_to_remove)
    in_flight_map_.erase(change_id);

  roots_.erase(window);
}

void WindowTreeClient::OnWindowMusBoundsChanged(WindowMus* window,
                                                const gfx::Rect& old_bounds,
                                                const gfx::Rect& new_bounds) {
  // Changes to bounds of root windows are routed through
  // OnWindowTreeHostBoundsWillChange(). Any bounds that happen here are a side
  // effect of those and can be ignored.
  if (IsRoot(window)) {
    // Do not set the LocalSurfaceId on the compositor here, because it has
    // already been set.
    return;
  }
  ScheduleInFlightBoundsChange(window, old_bounds, new_bounds);
}

void WindowTreeClient::OnWindowMusTransformChanged(
    WindowMus* window,
    const gfx::Transform& old_transform,
    const gfx::Transform& new_transform) {
  const uint32_t change_id = ScheduleInFlightChange(
      std::make_unique<InFlightTransformChange>(this, window, old_transform));
  tree_->SetWindowTransform(change_id, window->server_id(), new_transform);
}

void WindowTreeClient::OnWindowMusAddChild(WindowMus* parent,
                                           WindowMus* child) {
  // TODO: add checks to ensure this can work.
  const uint32_t change_id = ScheduleInFlightChange(
      std::make_unique<CrashInFlightChange>(parent, ChangeType::ADD_CHILD));
  tree_->AddWindow(change_id, parent->server_id(), child->server_id());
}

void WindowTreeClient::OnWindowMusRemoveChild(WindowMus* parent,
                                              WindowMus* child) {
  // TODO: add checks to ensure this can work.
  const uint32_t change_id = ScheduleInFlightChange(
      std::make_unique<CrashInFlightChange>(parent, ChangeType::REMOVE_CHILD));
  tree_->RemoveWindowFromParent(change_id, child->server_id());
}

void WindowTreeClient::OnWindowMusMoveChild(WindowMus* parent,
                                            size_t current_index,
                                            size_t dest_index) {
  DCHECK_NE(current_index, dest_index);
  // TODO: add checks to ensure this can work, e.g. we own the parent.
  const uint32_t change_id = ScheduleInFlightChange(
      std::make_unique<CrashInFlightChange>(parent, ChangeType::REORDER));
  WindowMus* window =
      WindowMus::Get(parent->GetWindow()->children()[current_index]);
  WindowMus* relative_window = nullptr;
  ui::mojom::OrderDirection direction;
  if (dest_index < current_index) {
    relative_window =
        WindowMus::Get(parent->GetWindow()->children()[dest_index]);
    direction = ui::mojom::OrderDirection::BELOW;
  } else {
    relative_window =
        WindowMus::Get(parent->GetWindow()->children()[dest_index]);
    direction = ui::mojom::OrderDirection::ABOVE;
  }
  tree_->ReorderWindow(change_id, window->server_id(),
                       relative_window->server_id(), direction);
}

void WindowTreeClient::OnWindowMusSetVisible(WindowMus* window, bool visible) {
  // TODO: add checks to ensure this can work.
  DCHECK(tree_);
  const uint32_t change_id = ScheduleInFlightChange(
      std::make_unique<InFlightVisibleChange>(this, window, !visible));
  tree_->SetWindowVisibility(change_id, window->server_id(), visible);
}

std::unique_ptr<ui::PropertyData>
WindowTreeClient::OnWindowMusWillChangeProperty(WindowMus* window,
                                                const void* key) {
  if (IsInternalProperty(key))
    return nullptr;

  std::unique_ptr<WindowPortPropertyDataMus> data(
      std::make_unique<WindowPortPropertyDataMus>());
  if (!delegate_->GetPropertyConverter()->ConvertPropertyForTransport(
          window->GetWindow(), key, &data->transport_name,
          &data->transport_value)) {
    return nullptr;
  }
  return std::move(data);
}

void WindowTreeClient::OnWindowMusPropertyChanged(
    WindowMus* window,
    const void* key,
    int64_t old_value,
    std::unique_ptr<ui::PropertyData> data) {
  if (HandleInternalPropertyChanged(window, key, old_value) || !data)
    return;

  WindowPortPropertyDataMus* data_mus =
      static_cast<WindowPortPropertyDataMus*>(data.get());

  std::string transport_name;
  std::unique_ptr<std::vector<uint8_t>> transport_value;
  if (!delegate_->GetPropertyConverter()->ConvertPropertyForTransport(
          window->GetWindow(), key, &transport_name, &transport_value)) {
    return;
  }
  DCHECK_EQ(transport_name, data_mus->transport_name);

  base::Optional<std::vector<uint8_t>> transport_value_mojo;
  if (transport_value)
    transport_value_mojo.emplace(std::move(*transport_value));

  const uint32_t change_id =
      ScheduleInFlightChange(std::make_unique<InFlightPropertyChange>(
          window, transport_name, std::move(data_mus->transport_value)));
  tree_->SetWindowProperty(change_id, window->server_id(), transport_name,
                           transport_value_mojo);
}

std::set<Window*> WindowTreeClient::GetRoots() {
  std::set<Window*> roots;
  for (WindowMus* window : roots_)
    roots.insert(window->GetWindow());
  return roots;
}

bool WindowTreeClient::WasCreatedByThisClient(const WindowMus* window) const {
  // Windows created via CreateTopLevelWindow() are not owned by us, but don't
  // have high-word set. const_cast is required by set.
  return !ui::ClientIdFromTransportId(window->server_id()) &&
         roots_.count(const_cast<WindowMus*>(window)) == 0;
}

gfx::Point WindowTreeClient::GetCursorScreenPoint() {
  if (!cursor_location_memory())
    return gfx::Point();  // We raced initialization. Return (0, 0).

  base::subtle::Atomic32 location =
      base::subtle::NoBarrier_Load(cursor_location_memory());
  return gfx::Point(static_cast<int16_t>(location >> 16),
                    static_cast<int16_t>(location & 0xFFFF));
}

void WindowTreeClient::StartPointerWatcher(bool want_moves) {
  if (has_pointer_watcher_)
    StopPointerWatcher();
  has_pointer_watcher_ = true;
  tree_->StartPointerWatcher(want_moves);
}

void WindowTreeClient::StopPointerWatcher() {
  DCHECK(has_pointer_watcher_);
  tree_->StopPointerWatcher();
  has_pointer_watcher_ = false;
}

void WindowTreeClient::AddObserver(WindowTreeClientObserver* observer) {
  observers_.AddObserver(observer);
}

void WindowTreeClient::RemoveObserver(WindowTreeClientObserver* observer) {
  observers_.RemoveObserver(observer);
}

void WindowTreeClient::AddTestObserver(WindowTreeClientTestObserver* observer) {
  test_observers_.AddObserver(observer);
}

void WindowTreeClient::RemoveTestObserver(
    WindowTreeClientTestObserver* observer) {
  test_observers_.RemoveObserver(observer);
}

void WindowTreeClient::SetCanAcceptDrops(WindowMus* window,
                                         bool can_accept_drops) {
  DCHECK(tree_);
  tree_->SetCanAcceptDrops(window->server_id(), can_accept_drops);
}

void WindowTreeClient::SetEventTargetingPolicy(
    WindowMus* window,
    ui::mojom::EventTargetingPolicy policy) {
  DCHECK(tree_);
  tree_->SetEventTargetingPolicy(window->server_id(), policy);
}

void WindowTreeClient::OnEmbed(
    ui::mojom::WindowDataPtr root_data,
    ui::mojom::WindowTreePtr tree,
    int64_t display_id,
    ui::Id focused_window_id,
    bool drawn,
    const base::Optional<viz::LocalSurfaceId>& local_surface_id) {
  DCHECK(!tree_ptr_);
  tree_ptr_ = std::move(tree);

  is_from_embed_ = true;
  got_initial_displays_ = true;

  OnEmbedImpl(tree_ptr_.get(), std::move(root_data), display_id,
              focused_window_id, drawn, local_surface_id);
}

void WindowTreeClient::OnEmbedFromToken(
    const base::UnguessableToken& token,
    ui::mojom::WindowDataPtr root,
    int64_t display_id,
    const base::Optional<viz::LocalSurfaceId>& local_surface_id) {
  for (EmbedRoot* embed_root : embed_roots_) {
    if (embed_root->token() == token) {
      embed_root->OnEmbed(CreateWindowTreeHost(WindowMusType::EMBED, *root,
                                               display_id, local_surface_id));
      break;
    }
  }
}

void WindowTreeClient::OnEmbeddedAppDisconnected(ui::Id window_id) {
  WindowMus* window = GetWindowByServerId(window_id);
  if (window)
    window->NotifyEmbeddedAppDisconnected();
}

void WindowTreeClient::OnUnembed(ui::Id window_id) {
  WindowMus* window = GetWindowByServerId(window_id);
  if (!window)
    return;

  EmbedRoot* embed_root = GetEmbedRootWithRootWindow(window->GetWindow());
  if (embed_root) {
    embed_root->OnUnembed();
    if (!GetWindowByServerId(window_id))
      return;  // EmbedRoot was deleted, resulting in deleting window.
  }

  delegate_->OnUnembed(window->GetWindow());
  delete window;
}

void WindowTreeClient::OnCaptureChanged(ui::Id new_capture_window_id,
                                        ui::Id old_capture_window_id) {
  WindowMus* new_capture_window = GetWindowByServerId(new_capture_window_id);
  WindowMus* lost_capture_window = GetWindowByServerId(old_capture_window_id);
  if (!new_capture_window && !lost_capture_window)
    return;

  InFlightCaptureChange change(this, capture_synchronizer_.get(),
                               new_capture_window);
  if (ApplyServerChangeToExistingInFlightChange(change))
    return;

  capture_synchronizer_->SetCaptureFromServer(new_capture_window);
}

void WindowTreeClient::OnFrameSinkIdAllocated(
    ui::Id window_id,
    const viz::FrameSinkId& frame_sink_id) {
  WindowMus* window = GetWindowByServerId(window_id);
  if (!window)
    return;

  window->SetFrameSinkIdFromServer(frame_sink_id);
}

void WindowTreeClient::OnTopLevelCreated(
    uint32_t change_id,
    ui::mojom::WindowDataPtr data,
    int64_t display_id,
    bool drawn,
    const base::Optional<viz::LocalSurfaceId>& local_surface_id) {
  // The server ack'd the top level window we created and supplied the state
  // of the window at the time the server created it. For properties we do not
  // have changes in flight for we can update them immediately. For properties
  // with changes in flight we set the revert value from the server.

  if (!in_flight_map_.count(change_id)) {
    // The window may have been destroyed locally before the server could finish
    // creating the window, and before the server received the notification that
    // the window has been destroyed.
    return;
  }
  std::unique_ptr<InFlightChange> change(std::move(in_flight_map_[change_id]));
  in_flight_map_.erase(change_id);

  WindowMus* window = change->window();
  WindowTreeHostMus* window_tree_host = GetWindowTreeHostMus(window);

  // Drawn state and display-id always come from the server (they can't be
  // modified locally).
  window_tree_host->set_display_id(display_id);

  // The default visibilty is false, we only need update visibility if it
  // differs from that.
  if (data->visible) {
    InFlightVisibleChange visible_change(this, window, data->visible);
    InFlightChange* current_change =
        GetOldestInFlightChangeMatching(visible_change);
    if (current_change)
      current_change->SetRevertValueFrom(visible_change);
    else
      SetWindowVisibleFromServer(window, true);
  }

  const gfx::Rect bounds(data->bounds);
  {
    InFlightBoundsChange bounds_change(this, window, bounds, local_surface_id);
    InFlightChange* current_change =
        GetOldestInFlightChangeMatching(bounds_change);
    if (current_change)
      current_change->SetRevertValueFrom(bounds_change);
    else if (window->GetWindow()->GetBoundsInScreen() != bounds)
      SetWindowBoundsFromServer(window, bounds, local_surface_id);
  }

  // There is currently no API to bulk set properties, so we iterate over each
  // property individually.
  for (const auto& pair : data->properties) {
    std::unique_ptr<std::vector<uint8_t>> revert_value(
        std::make_unique<std::vector<uint8_t>>(pair.second));
    InFlightPropertyChange property_change(window, pair.first,
                                           std::move(revert_value));
    InFlightChange* current_change =
        GetOldestInFlightChangeMatching(property_change);
    if (current_change) {
      current_change->SetRevertValueFrom(property_change);
    } else {
      window->SetPropertyFromServer(pair.first, &pair.second);
    }
  }

  // Top level windows should not have a parent.
  DCHECK_EQ(0u, data->parent_id);
}

void WindowTreeClient::OnWindowBoundsChanged(
    ui::Id window_id,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    const base::Optional<viz::LocalSurfaceId>& local_surface_id) {
  WindowMus* window = GetWindowByServerId(window_id);
  if (!window)
    return;

  InFlightBoundsChange new_change(this, window, new_bounds, local_surface_id);
  if (ApplyServerChangeToExistingInFlightChange(new_change))
    return;

  SetWindowBoundsFromServer(window, new_bounds, local_surface_id);
}

void WindowTreeClient::OnWindowTransformChanged(
    ui::Id window_id,
    const gfx::Transform& old_transform,
    const gfx::Transform& new_transform) {
  WindowMus* window = GetWindowByServerId(window_id);
  if (!window)
    return;

  InFlightTransformChange new_change(this, window, new_transform);
  if (ApplyServerChangeToExistingInFlightChange(new_change))
    return;

  SetWindowTransformFromServer(window, new_transform);
}

void WindowTreeClient::OnTransientWindowAdded(ui::Id window_id,
                                              ui::Id transient_window_id) {
  WindowMus* window = GetWindowByServerId(window_id);
  WindowMus* transient_window = GetWindowByServerId(transient_window_id);
  // window or transient_window or both may be null if a local delete occurs
  // with an in flight add from the server.
  if (window && transient_window)
    window->AddTransientChildFromServer(transient_window);
}

void WindowTreeClient::OnTransientWindowRemoved(ui::Id window_id,
                                                ui::Id transient_window_id) {
  WindowMus* window = GetWindowByServerId(window_id);
  WindowMus* transient_window = GetWindowByServerId(transient_window_id);
  // window or transient_window or both may be null if a local delete occurs
  // with an in flight delete from the server.
  if (window && transient_window)
    window->RemoveTransientChildFromServer(transient_window);
}

void WindowTreeClient::OnWindowHierarchyChanged(
    ui::Id window_id,
    ui::Id old_parent_id,
    ui::Id new_parent_id,
    std::vector<ui::mojom::WindowDataPtr> windows) {
  const bool was_window_known = GetWindowByServerId(window_id) != nullptr;

  BuildWindowTree(windows);

  // If the window was not known, then BuildWindowTree() will have created it
  // and parented the window.
  if (!was_window_known)
    return;

  WindowMus* new_parent = GetWindowByServerId(new_parent_id);
  WindowMus* old_parent = GetWindowByServerId(old_parent_id);
  WindowMus* window = GetWindowByServerId(window_id);
  if (new_parent)
    new_parent->AddChildFromServer(window);
  else
    old_parent->RemoveChildFromServer(window);
}

void WindowTreeClient::OnWindowReordered(ui::Id window_id,
                                         ui::Id relative_window_id,
                                         ui::mojom::OrderDirection direction) {
  WindowMus* window = GetWindowByServerId(window_id);
  WindowMus* relative_window = GetWindowByServerId(relative_window_id);
  WindowMus* parent = WindowMus::Get(window->GetWindow()->parent());
  if (window && relative_window && parent &&
      parent == WindowMus::Get(relative_window->GetWindow()->parent())) {
    parent->ReorderFromServer(window, relative_window, direction);
  }
}

void WindowTreeClient::OnWindowDeleted(ui::Id window_id) {
  WindowMus* window = GetWindowByServerId(window_id);
  if (!window)
    return;

  if (roots_.count(window)) {
    // Roots are associated with WindowTreeHosts. The WindowTreeHost owns the
    // root, so we have to delete the WindowTreeHost to indirectly delete the
    // Window. Clients may want to do extra processing before the delete,
    // notify the appropriate delegate to handle the deletion. Let the window
    // know it is going to be deleted so we don't callback to the server.
    window->PrepareForDestroy();
    EmbedRoot* embed_root = GetEmbedRootWithRootWindow(window->GetWindow());
    if (embed_root)
      embed_root->OnUnembed();
    else
      delegate_->OnEmbedRootDestroyed(GetWindowTreeHostMus(window));
  } else {
    window->DestroyFromServer();
  }
}

void WindowTreeClient::OnWindowVisibilityChanged(ui::Id window_id,
                                                 bool visible) {
  WindowMus* window = GetWindowByServerId(window_id);
  if (!window)
    return;

  InFlightVisibleChange new_change(this, window, visible);
  if (ApplyServerChangeToExistingInFlightChange(new_change))
    return;

  SetWindowVisibleFromServer(window, visible);
}

void WindowTreeClient::OnWindowOpacityChanged(ui::Id window_id,
                                              float old_opacity,
                                              float new_opacity) {
  WindowMus* window = GetWindowByServerId(window_id);
  if (!window)
    return;

  InFlightOpacityChange new_change(window, new_opacity);
  if (ApplyServerChangeToExistingInFlightChange(new_change))
    return;

  window->SetOpacityFromServer(new_opacity);
}

void WindowTreeClient::OnWindowParentDrawnStateChanged(ui::Id window_id,
                                                       bool drawn) {
  // TODO: route to WindowTreeHost.
  /*
  Window* window = GetWindowByServerId(window_id);
  if (window)
    WindowPrivate(window).LocalSetParentDrawn(drawn);
  */
}

void WindowTreeClient::OnWindowSharedPropertyChanged(
    ui::Id window_id,
    const std::string& name,
    const base::Optional<std::vector<uint8_t>>& transport_data) {
  WindowMus* window = GetWindowByServerId(window_id);
  if (!window)
    return;

  std::unique_ptr<std::vector<uint8_t>> data;
  if (transport_data.has_value())
    data = std::make_unique<std::vector<uint8_t>>(transport_data.value());

  InFlightPropertyChange new_change(window, name, std::move(data));
  if (ApplyServerChangeToExistingInFlightChange(new_change))
    return;

  window->SetPropertyFromServer(
      name, transport_data.has_value() ? &transport_data.value() : nullptr);
}

void WindowTreeClient::OnWindowInputEvent(
    uint32_t event_id,
    ui::Id window_id,
    int64_t display_id,
    std::unique_ptr<ui::Event> event,
    bool matches_pointer_watcher) {
  DCHECK(event);
  WindowMus* window = GetWindowByServerId(window_id);  // May be null.

  if (matches_pointer_watcher && has_pointer_watcher_) {
    DCHECK(event->IsPointerEvent());
    std::unique_ptr<ui::Event> event_in_dip(ui::Event::Clone(*event));
    if (!window) {
      // When there is no window force the root and location to be the same.
      // They may differ if |window| was valid at the time of the event, but
      // was since deleted.
      event_in_dip->AsLocatedEvent()->set_location_f(
          event_in_dip->AsLocatedEvent()->root_location_f());
    }
    delegate_->OnPointerEventObserved(*event_in_dip->AsPointerEvent(),
                                      display_id,
                                      window ? window->GetWindow() : nullptr);
  }

  // If the window has already been deleted, use |event| to update event states
  // kept in aura::Env.
  if (!window || !window->GetWindow()->GetHost()) {
    EnvInputStateController* env_controller =
        Env::GetInstance()->env_controller();
    std::unique_ptr<ui::Event> mapped_event = MapEvent(*event.get());
    if (mapped_event->IsMouseEvent()) {
      env_controller->UpdateStateForMouseEvent(nullptr,
                                               *mapped_event->AsMouseEvent());
    } else if (mapped_event->IsTouchEvent()) {
      env_controller->UpdateStateForTouchEvent(*mapped_event->AsTouchEvent());
    }
    tree_->OnWindowInputEventAck(event_id, ui::mojom::EventResult::UNHANDLED);
    return;
  }

  if (event->IsKeyEvent()) {
    InputMethodMus* input_method = GetWindowTreeHostMus(window)->input_method();
    if (input_method) {
      ignore_result(input_method->DispatchKeyEvent(
          event->AsKeyEvent(), CreateEventResultCallback(event_id)));
      return;
    }
  }

  // TODO(moshayedi): crbug.com/617222. No need to convert to ui::MouseEvent or
  // ui::TouchEvent once we have proper support for pointer events.
  std::unique_ptr<ui::Event> mapped_event = MapEvent(*event.get());
  ui::Event* event_to_dispatch = mapped_event.get();
  // |ack_handler| may use |event_to_dispatch| from its destructor, so it needs
  // to be destroyed after |event_to_dispatch| is destroyed.
  EventAckHandler ack_handler(CreateEventResultCallback(event_id));

  if (!event->IsKeyEvent()) {
    // Set |window| as the target, except for key events. Key events go to the
    // focused window, which may have changed by the time we process the event.
    ui::Event::DispatcherApi(event_to_dispatch).set_target(window->GetWindow());
  }

  GetWindowTreeHostMus(window)->SendEventToSink(event_to_dispatch);

  ack_handler.set_handled(event_to_dispatch->handled());
}

void WindowTreeClient::OnPointerEventObserved(std::unique_ptr<ui::Event> event,
                                              ui::Id window_id,
                                              int64_t display_id) {
  DCHECK(event);
  DCHECK(event->IsPointerEvent());
  if (!has_pointer_watcher_)
    return;

  WindowMus* target_window = GetWindowByServerId(window_id);
  if (!target_window) {
    // When there is no window force the root and location to be the same.
    // They may differ if |window| was valid at the time of the event, but
    // was since deleted.
    event->AsLocatedEvent()->set_location_f(
        event->AsLocatedEvent()->root_location_f());
  }
  delegate_->OnPointerEventObserved(
      *event->AsPointerEvent(), display_id,
      target_window ? target_window->GetWindow() : nullptr);
}

void WindowTreeClient::OnWindowFocused(ui::Id focused_window_id) {
  WindowMus* focused_window = GetWindowByServerId(focused_window_id);
  InFlightFocusChange new_change(this, focus_synchronizer_.get(),
                                 focused_window);
  if (ApplyServerChangeToExistingInFlightChange(new_change))
    return;

  focus_synchronizer_->SetFocusFromServer(focused_window);
}

void WindowTreeClient::OnWindowCursorChanged(ui::Id window_id,
                                             ui::CursorData cursor) {
  WindowMus* window = GetWindowByServerId(window_id);
  if (!window)
    return;

  InFlightCursorChange new_change(window, cursor);
  if (ApplyServerChangeToExistingInFlightChange(new_change))
    return;

  window->SetCursorFromServer(cursor);
}

void WindowTreeClient::OnWindowSurfaceChanged(
    ui::Id window_id,
    const viz::SurfaceInfo& surface_info) {
  WindowMus* window = GetWindowByServerId(window_id);
  if (!window)
    return;

  // If the parent is informed of a child's surface then that surface ID is
  // guaranteed to be available in the display compositor so we set it as the
  // fallback. If surface synchronization is enabled, the primary SurfaceInfo
  // is created by the embedder, and the LocalSurfaceId is allocated by the
  // embedder.
  window->SetFallbackSurfaceInfo(surface_info);
}

void WindowTreeClient::OnDragDropStart(
    const base::flat_map<std::string, std::vector<uint8_t>>& mime_data) {
  drag_drop_controller_->OnDragDropStart(mojo::FlatMapToMap(mime_data));
}

void WindowTreeClient::OnDragEnter(ui::Id window_id,
                                   uint32_t key_state,
                                   const gfx::Point& position,
                                   uint32_t effect_bitmask,
                                   OnDragEnterCallback callback) {
  std::move(callback).Run(drag_drop_controller_->OnDragEnter(
      GetWindowByServerId(window_id), key_state, position, effect_bitmask));
}

void WindowTreeClient::OnDragOver(ui::Id window_id,
                                  uint32_t key_state,
                                  const gfx::Point& position,
                                  uint32_t effect_bitmask,
                                  OnDragOverCallback callback) {
  std::move(callback).Run(drag_drop_controller_->OnDragOver(
      GetWindowByServerId(window_id), key_state, position, effect_bitmask));
}

void WindowTreeClient::OnDragLeave(ui::Id window_id) {
  drag_drop_controller_->OnDragLeave(GetWindowByServerId(window_id));
}

void WindowTreeClient::OnDragDropDone() {
  drag_drop_controller_->OnDragDropDone();
}

void WindowTreeClient::OnCompleteDrop(ui::Id window_id,
                                      uint32_t key_state,
                                      const gfx::Point& position,
                                      uint32_t effect_bitmask,
                                      OnCompleteDropCallback callback) {
  std::move(callback).Run(drag_drop_controller_->OnCompleteDrop(
      GetWindowByServerId(window_id), key_state, position, effect_bitmask));
}

void WindowTreeClient::OnPerformDragDropCompleted(uint32_t change_id,
                                                  bool success,
                                                  uint32_t action_taken) {
  OnChangeCompleted(change_id, success);
  if (drag_drop_controller_->DoesChangeIdMatchDragChangeId(change_id))
    drag_drop_controller_->OnPerformDragDropCompleted(action_taken);
}

std::unique_ptr<TopmostWindowTracker>
WindowTreeClient::StartObservingTopmostWindow(ui::mojom::MoveLoopSource source,
                                              aura::Window* initial_target) {
  DCHECK(!topmost_window_tracker_);
  WindowMus* window = WindowMus::Get(initial_target->GetRootWindow());
  DCHECK(window);

  tree_->ObserveTopmostWindow(source, window->server_id());
  auto topmost_window_tracker = std::make_unique<TopmostWindowTracker>(this);
  topmost_window_tracker_ = topmost_window_tracker.get();
  return topmost_window_tracker;
}

void WindowTreeClient::StopObservingTopmostWindow() {
  DCHECK(topmost_window_tracker_);
  tree_->StopObservingTopmostWindow();
  topmost_window_tracker_ = nullptr;
}

void WindowTreeClient::OnTopmostWindowChanged(
    const std::vector<ui::Id>& topmost_ids) {
  DCHECK_LE(topmost_ids.size(), 2u);
  // There's a slight chance of |topmost_window_tracker_| to be nullptr at the
  // end of a session of observing topmost window.
  if (!topmost_window_tracker_)
    return;
  std::vector<WindowMus*> windows;
  for (auto& id : topmost_ids)
    windows.push_back(GetWindowByServerId(id));
  topmost_window_tracker_->OnTopmostWindowChanged(windows);
}

void WindowTreeClient::OnChangeCompleted(uint32_t change_id, bool success) {
  std::unique_ptr<InFlightChange> change(std::move(in_flight_map_[change_id]));
  in_flight_map_.erase(change_id);
  if (!change)
    return;

  for (auto& observer : test_observers_)
    observer.OnChangeCompleted(change_id, change->change_type(), success);

  if (!success) {
    DVLOG(1) << "Change failed id=" << change_id
             << " type=" << ChangeTypeToString(change->change_type());
    change->ChangeFailed();
  }

  InFlightChange* next_change = GetOldestInFlightChangeMatching(*change);
  if (next_change) {
    if (!success)
      next_change->SetRevertValueFrom(*change);
  } else if (!success) {
    change->Revert();
  }

  if (change_id == current_move_loop_change_) {
    current_move_loop_change_ = 0;
    on_current_move_finished_.Run(success);
    on_current_move_finished_.Reset();
  }
}

void WindowTreeClient::GetScreenProviderObserver(
    ui::mojom::ScreenProviderObserverAssociatedRequest observer) {
  screen_provider_observer_binding_.Bind(std::move(observer));
}

void WindowTreeClient::OnDisplaysChanged(
    std::vector<ui::mojom::WsDisplayPtr> ws_displays,
    int64_t primary_display_id,
    int64_t internal_display_id,
    int64_t display_id_for_new_windows) {
  got_initial_displays_ = true;
  delegate_->OnDisplaysChanged(std::move(ws_displays), primary_display_id,
                               internal_display_id, display_id_for_new_windows);
}

void WindowTreeClient::RequestClose(ui::Id window_id) {
  WindowMus* window = GetWindowByServerId(window_id);
  if (!window || !IsRoot(window))
    return;

  // Since the window is the root window, we send close request to the entire
  // WindowTreeHost.
  GetWindowTreeHostMus(window->GetWindow())->OnCloseRequest();
}

void WindowTreeClient::OnWindowTreeHostBoundsWillChange(
    WindowTreeHostMus* window_tree_host,
    const gfx::Rect& bounds_in_pixels) {
  gfx::Rect old_bounds = window_tree_host->GetBoundsInPixels();
  gfx::Rect new_bounds = bounds_in_pixels;
  const float device_scale_factor = window_tree_host->device_scale_factor();
  old_bounds = gfx::ConvertRectToDIP(device_scale_factor, old_bounds);
  new_bounds = gfx::ConvertRectToDIP(device_scale_factor, new_bounds);
  ScheduleInFlightBoundsChange(WindowMus::Get(window_tree_host->window()),
                               old_bounds, new_bounds);
}

void WindowTreeClient::OnWindowTreeHostClientAreaWillChange(
    WindowTreeHostMus* window_tree_host,
    const gfx::Insets& client_area,
    const std::vector<gfx::Rect>& additional_client_areas) {
  DCHECK(tree_);
  WindowMus* window = WindowMus::Get(window_tree_host->window());
  tree_->SetClientArea(window->server_id(), client_area,
                       additional_client_areas);
}

void WindowTreeClient::OnWindowTreeHostSetOpacity(
    WindowTreeHostMus* window_tree_host,
    float opacity) {
  WindowMus* window = WindowMus::Get(window_tree_host->window());
  const uint32_t change_id = ScheduleInFlightChange(
      std::make_unique<CrashInFlightChange>(window, ChangeType::OPACITY));
  tree_->SetWindowOpacity(change_id, window->server_id(), opacity);
}

void WindowTreeClient::OnWindowTreeHostDeactivateWindow(
    WindowTreeHostMus* window_tree_host) {
  tree_->DeactivateWindow(
      WindowMus::Get(window_tree_host->window())->server_id());
}

void WindowTreeClient::OnWindowTreeHostStackAbove(
    WindowTreeHostMus* window_tree_host,
    Window* window) {
  WindowMus* above = WindowMus::Get(window_tree_host->window());
  WindowMus* below = WindowMus::Get(window);
  const uint32_t change_id = ScheduleInFlightChange(
      std::make_unique<CrashInFlightChange>(above, ChangeType::REORDER));
  tree_->StackAbove(change_id, above->server_id(), below->server_id());
}

void WindowTreeClient::OnWindowTreeHostStackAtTop(
    WindowTreeHostMus* window_tree_host) {
  WindowMus* window = WindowMus::Get(window_tree_host->window());
  const uint32_t change_id = ScheduleInFlightChange(
      std::make_unique<CrashInFlightChange>(window, ChangeType::REORDER));
  tree_->StackAtTop(change_id, window->server_id());
}

void WindowTreeClient::OnWindowTreeHostPerformWmAction(
    WindowTreeHostMus* window_tree_host,
    const std::string& action) {
  WindowMus* window = WindowMus::Get(window_tree_host->window());
  tree_->PerformWmAction(window->server_id(), action);
}

void WindowTreeClient::OnWindowTreeHostPerformWindowMove(
    WindowTreeHostMus* window_tree_host,
    ui::mojom::MoveLoopSource source,
    const gfx::Point& cursor_location,
    const base::Callback<void(bool)>& callback) {
  DCHECK(on_current_move_finished_.is_null());
  on_current_move_finished_ = callback;

  WindowMus* window_mus = WindowMus::Get(window_tree_host->window());
  current_move_loop_change_ = ScheduleInFlightChange(
      std::make_unique<InFlightDragChange>(window_mus, ChangeType::MOVE_LOOP));
  // Tell the window manager to take over moving us.
  tree_->PerformWindowMove(current_move_loop_change_, window_mus->server_id(),
                           source, cursor_location);
}

void WindowTreeClient::OnWindowTreeHostCancelWindowMove(
    WindowTreeHostMus* window_tree_host) {
  tree_->CancelWindowMove(
      WindowMus::Get(window_tree_host->window())->server_id());
}

std::unique_ptr<WindowPortMus> WindowTreeClient::CreateWindowPortForTopLevel(
    const std::map<std::string, std::vector<uint8_t>>* properties) {
  std::unique_ptr<WindowPortMus> window_port =
      std::make_unique<WindowPortMus>(this, WindowMusType::TOP_LEVEL);
  roots_.insert(window_port.get());

  window_port->set_server_id(next_window_id_++);
  RegisterWindowMus(window_port.get());

  base::flat_map<std::string, std::vector<uint8_t>> transport_properties;
  if (properties) {
    for (const auto& property_pair : *properties)
      transport_properties[property_pair.first] = property_pair.second;
  }

  const uint32_t change_id =
      ScheduleInFlightChange(std::make_unique<CrashInFlightChange>(
          window_port.get(), ChangeType::NEW_TOP_LEVEL_WINDOW));
  tree_->NewTopLevelWindow(change_id, window_port->server_id(),
                           transport_properties);
  return window_port;
}

void WindowTreeClient::OnWindowTreeHostCreated(
    WindowTreeHostMus* window_tree_host) {
  // All WindowTreeHosts are destroyed before this, so we don't need to unset
  // the DragDropClient.
  client::SetDragDropClient(window_tree_host->window(),
                            drag_drop_controller_.get());
}

void WindowTreeClient::OnTransientChildWindowAdded(Window* parent,
                                                   Window* transient_child) {
  // TransientWindowClient is a singleton and we allow multiple
  // WindowTreeClients. Ignore changes to windows we don't know about (assume
  // they came from another connection).
  if (!IsWindowKnown(parent) || !IsWindowKnown(transient_child))
    return;

  if (WindowMus::Get(parent)->OnTransientChildAdded(
          WindowMus::Get(transient_child)) == WindowMus::ChangeSource::SERVER) {
    return;
  }

  // The change originated from client code and needs to be sent to the server.
  DCHECK(tree_);
  WindowMus* parent_mus = WindowMus::Get(parent);
  const uint32_t change_id =
      ScheduleInFlightChange(std::make_unique<CrashInFlightChange>(
          parent_mus, ChangeType::ADD_TRANSIENT_WINDOW));
  tree_->AddTransientWindow(change_id, parent_mus->server_id(),
                            WindowMus::Get(transient_child)->server_id());
}

void WindowTreeClient::OnTransientChildWindowRemoved(Window* parent,
                                                     Window* transient_child) {
  // See comments in OnTransientChildWindowAdded() for details on early return.
  if (!IsWindowKnown(parent) || !IsWindowKnown(transient_child))
    return;

  if (WindowMus::Get(parent)->OnTransientChildRemoved(
          WindowMus::Get(transient_child)) == WindowMus::ChangeSource::SERVER) {
    return;
  }
  // The change originated from client code and needs to be sent to the server.
  DCHECK(tree_);
  WindowMus* child_mus = WindowMus::Get(transient_child);
  const uint32_t change_id =
      ScheduleInFlightChange(std::make_unique<CrashInFlightChange>(
          child_mus, ChangeType::REMOVE_TRANSIENT_WINDOW_FROM_PARENT));
  tree_->RemoveTransientWindowFromParent(change_id, child_mus->server_id());
}

uint32_t WindowTreeClient::CreateChangeIdForDrag(WindowMus* window) {
  return ScheduleInFlightChange(
      std::make_unique<InFlightDragChange>(window, ChangeType::DRAG_LOOP));
}

uint32_t WindowTreeClient::CreateChangeIdForCapture(WindowMus* window) {
  return ScheduleInFlightChange(std::make_unique<InFlightCaptureChange>(
      this, capture_synchronizer_.get(), window));
}

uint32_t WindowTreeClient::CreateChangeIdForFocus(WindowMus* window) {
  return ScheduleInFlightChange(std::make_unique<InFlightFocusChange>(
      this, focus_synchronizer_.get(), window));
}

}  // namespace aura
