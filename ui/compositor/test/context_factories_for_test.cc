// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/test/context_factories_for_test.h"

#include "base/command_line.h"
#include "base/sys_info.h"
#include "cc/surfaces/surface_manager.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/compositor/test/in_process_context_factory.h"
#include "ui/gl/gl_implementation.h"

namespace {

static cc::SurfaceManager* g_surface_manager = nullptr;
static ui::InProcessContextFactory* g_implicit_factory = NULL;
static gl::DisableNullDrawGLBindings* g_disable_null_draw = NULL;

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
  bool context_factory_for_test = true;
  g_surface_manager = new cc::SurfaceManager;
  g_implicit_factory =
      new InProcessContextFactory(context_factory_for_test, g_surface_manager);
  *context_factory = g_implicit_factory;
  *context_factory_private = g_implicit_factory;
}

void TerminateContextFactoryForTests() {
  if (g_implicit_factory) {
    g_implicit_factory->SendOnLostResources();
    delete g_implicit_factory;
    g_implicit_factory = NULL;
  }
  delete g_surface_manager;
  g_surface_manager = nullptr;
  delete g_disable_null_draw;
  g_disable_null_draw = NULL;
}

}  // namespace ui
