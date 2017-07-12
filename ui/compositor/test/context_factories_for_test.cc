// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/test/context_factories_for_test.h"

#include "base/command_line.h"
#include "base/sys_info.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "cc/surfaces/frame_sink_manager.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/compositor/test/in_process_context_factory.h"
#include "ui/gl/gl_implementation.h"

namespace {

static viz::HostFrameSinkManager* g_host_frame_sink_manager = nullptr;
static viz::FrameSinkManagerImpl* g_frame_sink_manager_impl = nullptr;
static ui::InProcessContextFactory* g_implicit_factory = nullptr;
static gl::DisableNullDrawGLBindings* g_disable_null_draw = nullptr;

// Connect HostFrameSinkManager to FrameSinkManagerImpl.
void ConnectFrameSinkManager() {
  // Interfaces and requests to bind to each of the interfaces.
  cc::mojom::FrameSinkManagerClientPtr host_mojo;
  cc::mojom::FrameSinkManagerPtr manager_mojo;
  cc::mojom::FrameSinkManagerClientRequest host_mojo_request =
      mojo::MakeRequest(&host_mojo);
  cc::mojom::FrameSinkManagerRequest manager_mojo_request =
      mojo::MakeRequest(&manager_mojo);

  // Make the Mojo connections on both ends.
  g_frame_sink_manager_impl->BindAndSetClient(
      std::move(manager_mojo_request), base::SequencedTaskRunnerHandle::Get(),
      std::move(host_mojo));
  g_host_frame_sink_manager->BindAndSetManager(
      std::move(host_mojo_request), base::SequencedTaskRunnerHandle::Get(),
      std::move(manager_mojo));
}

}  // namespace

namespace ui {

// static
void InitializeContextFactoryForTests(
    bool enable_pixel_output,
    ui::ContextFactory** context_factory,
    ui::ContextFactoryPrivate** context_factory_private) {
  DCHECK(!g_implicit_factory) <<
      "ContextFactory for tests already initialized.";
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEnablePixelOutputInTests))
    enable_pixel_output = true;
  if (enable_pixel_output)
    g_disable_null_draw = new gl::DisableNullDrawGLBindings;
  g_host_frame_sink_manager = new viz::HostFrameSinkManager;
  g_frame_sink_manager_impl = new viz::FrameSinkManagerImpl(false, nullptr);
  g_implicit_factory = new InProcessContextFactory(
      g_host_frame_sink_manager,
      g_frame_sink_manager_impl->frame_sink_manager());
  g_implicit_factory->SetUseFastRefreshRateForTests();
  *context_factory = g_implicit_factory;
  *context_factory_private = g_implicit_factory;

  ConnectFrameSinkManager();
}

void TerminateContextFactoryForTests() {
  if (g_implicit_factory) {
    g_implicit_factory->SendOnLostResources();
    delete g_implicit_factory;
    g_implicit_factory = nullptr;
  }
  delete g_host_frame_sink_manager;
  g_host_frame_sink_manager = nullptr;
  delete g_frame_sink_manager_impl;
  g_frame_sink_manager_impl = nullptr;
  delete g_disable_null_draw;
  g_disable_null_draw = nullptr;
}

}  // namespace ui
