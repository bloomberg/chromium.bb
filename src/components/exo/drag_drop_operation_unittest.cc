// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/drag_drop_operation.h"

#include <memory>

#include "ash/shell.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/exo/buffer.h"
#include "components/exo/data_exchange_delegate.h"
#include "components/exo/data_source.h"
#include "components/exo/data_source_delegate.h"
#include "components/exo/shell_surface.h"
#include "components/exo/surface.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/test/exo_test_data_exchange_delegate.h"
#include "components/exo/test/shell_surface_builder.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-shared.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/point_f.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/drag_drop/drag_drop_controller.h"
#endif

namespace exo {
namespace {

constexpr char kText[] = "test";
constexpr char kTextMimeType[] = "text/plain";

}  // namespace

class TestDataSourceDelegate : public DataSourceDelegate {
 public:
  // DataSourceDelegate:
  void OnDataSourceDestroying(DataSource* source) override {}

  void OnTarget(const absl::optional<std::string>& mime_type) override {}

  void OnSend(const std::string& mime_type, base::ScopedFD fd) override {
    base::WriteFileDescriptor(fd.get(), kText);
  }

  void OnCancelled() override {}

  void OnDndDropPerformed() override {}

  void OnDndFinished() override {}

  void OnAction(DndAction dnd_action) override {}

  bool CanAcceptDataEventsForSurface(Surface* surface) const override {
    return true;
  }
};

class DragDropOperationTest : public test::ExoTestBase,
                              public aura::client::DragDropClientObserver {
 public:
  DragDropOperationTest() = default;
  ~DragDropOperationTest() override = default;
  DragDropOperationTest(const DragDropOperationTest&) = delete;
  DragDropOperationTest& operator=(const DragDropOperationTest&) = delete;

  void SetUp() override {
    test::ExoTestBase::SetUp();
    aura::client::GetDragDropClient(ash::Shell::GetPrimaryRootWindow())
        ->AddObserver(this);
  }

  void TearDown() override {
    aura::client::GetDragDropClient(ash::Shell::GetPrimaryRootWindow())
        ->RemoveObserver(this);
    test::ExoTestBase::TearDown();
  }

  // aura::client::DragDropClientObserver:
  void OnDragStarted() override {
    drag_start_count_++;
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, std::move(drag_blocked_callback_));
  }

  void OnDragEnded() override { drag_end_count_++; }

 protected:
  void set_drag_blocked_callback(base::OnceClosure callback) {
    drag_blocked_callback_ = std::move(callback);
  }

  int GetDragStartCountAndReset() {
    int result = drag_start_count_;
    drag_start_count_ = 0;
    return result;
  }

  int GetDragEndCountAndReset() {
    int result = drag_end_count_;
    drag_end_count_ = 0;
    return result;
  }

 private:
  // Callback running inside the nested RunLoop in StartDragAndDrop().
  base::OnceClosure drag_blocked_callback_;

  int drag_start_count_ = 0;
  int drag_end_count_ = 0;
};

TEST_F(DragDropOperationTest, DeleteDuringDragging) {
  TestDataExchangeDelegate data_exchange_delegate;

  auto delegate = std::make_unique<TestDataSourceDelegate>();
  auto data_source = std::make_unique<DataSource>(delegate.get());
  data_source->Offer(kTextMimeType);

  auto origin_surface = std::make_unique<Surface>();
  ash::Shell::GetPrimaryRootWindow()->AddChild(origin_surface->window());

  gfx::Size buffer_size(100, 100);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  auto icon_surface = std::make_unique<Surface>();
  icon_surface->Attach(buffer.get());

  auto operation = DragDropOperation::Create(
      &data_exchange_delegate, data_source.get(), origin_surface.get(),
      icon_surface.get(), gfx::PointF(), ui::mojom::DragEventSource::kMouse);
  icon_surface->Commit();

  base::RunLoop run_loop;
  set_drag_blocked_callback(base::BindOnce(
      [](std::unique_ptr<DataSource> data_source,
         base::WeakPtr<DragDropOperation> operation,
         base::OnceClosure quit_closure) {
        // This function runs inside the nested RunLoop in
        // ash::DragDropController::StartDragAndDrop().
        EXPECT_TRUE(operation);
        // Deleting DataSource causes DragDropOperation to be deleted as well.
        data_source.reset();
        EXPECT_FALSE(operation);
        std::move(quit_closure).Run();
      },
      std::move(data_source), operation, run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_FALSE(operation);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(DragDropOperationTest, DragDropFromPopup) {
  static_cast<ash::DragDropController*>(
      aura::client::GetDragDropClient(ash::Shell::GetPrimaryRootWindow()))
      ->set_should_block_during_drag_drop(false);
  TestDataExchangeDelegate data_exchange_delegate;

  auto delegate = std::make_unique<TestDataSourceDelegate>();
  auto data_source = std::make_unique<DataSource>(delegate.get());
  data_source->Offer(kTextMimeType);

  auto origin_shell_surface = test::ShellSurfaceBuilder(gfx::Size(100, 100))
                                  .SetNoCommit()
                                  .BuildShellSurface();
  origin_shell_surface->SetPopup();
  origin_shell_surface->Grab();
  int closed_count = 0;
  origin_shell_surface->set_close_callback(
      base::BindLambdaForTesting([&]() { closed_count++; }));

  auto* origin_surface = origin_shell_surface->root_surface();
  origin_surface->Commit();

  gfx::Size buffer_size(32, 32);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  auto icon_surface = std::make_unique<Surface>();
  icon_surface->Attach(buffer.get());

  base::RunLoop run_loop;
  set_drag_blocked_callback(run_loop.QuitClosure());

  ui::test::EventGenerator generator(origin_surface->window()->GetRootWindow(),
                                     origin_surface->window());
  generator.PressLeftButton();
  gfx::Point location =
      generator.current_screen_location() -
      origin_surface->window()->GetBoundsInScreen().OffsetFromOrigin();
  auto operation = DragDropOperation::Create(
      &data_exchange_delegate, data_source.get(),
      origin_shell_surface->root_surface(), icon_surface.get(),
      gfx::PointF(location), ui::mojom::DragEventSource::kMouse);
  icon_surface->Commit();

  run_loop.Run();

  EXPECT_EQ(1, GetDragStartCountAndReset());
  EXPECT_EQ(0, GetDragEndCountAndReset());
  EXPECT_EQ(0, closed_count);

  generator.MoveMouseBy(150, 150);

  EXPECT_EQ(0, GetDragStartCountAndReset());
  EXPECT_EQ(0, GetDragEndCountAndReset());

  generator.ReleaseLeftButton();
  EXPECT_EQ(0, GetDragStartCountAndReset());
  EXPECT_EQ(1, GetDragEndCountAndReset());
}

TEST_F(DragDropOperationTest, DragDropFromNestedPopup) {
  static_cast<ash::DragDropController*>(
      aura::client::GetDragDropClient(ash::Shell::GetPrimaryRootWindow()))
      ->set_should_block_during_drag_drop(false);
  TestDataExchangeDelegate data_exchange_delegate;

  auto parent_shell_surface = test::ShellSurfaceBuilder(gfx::Size(100, 100))
                                  .SetNoCommit()
                                  .BuildShellSurface();
  parent_shell_surface->SetPopup();
  parent_shell_surface->Grab();
  parent_shell_surface->root_surface()->Commit();

  auto delegate = std::make_unique<TestDataSourceDelegate>();
  auto data_source = std::make_unique<DataSource>(delegate.get());
  data_source->Offer(kTextMimeType);

  auto origin_shell_surface = test::ShellSurfaceBuilder(gfx::Size(100, 100))
                                  .SetNoCommit()
                                  .SetParent(parent_shell_surface.get())
                                  .BuildShellSurface();
  origin_shell_surface->SetPopup();
  origin_shell_surface->Grab();

  auto* origin_surface = origin_shell_surface->root_surface();
  origin_surface->Commit();

  gfx::Size buffer_size(32, 32);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  auto icon_surface = std::make_unique<Surface>();
  icon_surface->Attach(buffer.get());

  base::RunLoop run_loop;
  set_drag_blocked_callback(run_loop.QuitClosure());

  ui::test::EventGenerator generator(origin_surface->window()->GetRootWindow(),
                                     origin_surface->window());
  generator.PressLeftButton();
  gfx::Point location =
      generator.current_screen_location() -
      origin_surface->window()->GetBoundsInScreen().OffsetFromOrigin();
  auto operation = DragDropOperation::Create(
      &data_exchange_delegate, data_source.get(),
      origin_shell_surface->root_surface(), icon_surface.get(),
      gfx::PointF(location), ui::mojom::DragEventSource::kMouse);
  icon_surface->Commit();

  run_loop.Run();

  EXPECT_EQ(1, GetDragStartCountAndReset());
  EXPECT_EQ(0, GetDragEndCountAndReset());

  generator.MoveMouseBy(150, 150);

  EXPECT_EQ(0, GetDragStartCountAndReset());
  EXPECT_EQ(0, GetDragEndCountAndReset());

  // Hide the origin_shell_surface -- that will update the capture but shouldn't
  // stop the drag&drop session. Note: do not remove |origin_shell_surface|
  // itself, as the testing shell surface will also remove the root surface
  // which will stop the drag&drop while root surface remains normally.
  origin_shell_surface->GetWidget()->Hide();
  EXPECT_EQ(0, GetDragStartCountAndReset());
  EXPECT_EQ(0, GetDragEndCountAndReset());

  generator.ReleaseLeftButton();
  EXPECT_EQ(0, GetDragStartCountAndReset());
  EXPECT_EQ(1, GetDragEndCountAndReset());
}
#endif

}  // namespace exo
