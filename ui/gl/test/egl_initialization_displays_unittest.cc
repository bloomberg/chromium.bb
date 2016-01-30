// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_surface_egl.h"

namespace {

TEST(EGLInitializationDisplaysTest, DisableD3D11) {
  scoped_ptr<base::CommandLine> command_line(
      new base::CommandLine(base::CommandLine::NO_PROGRAM));

  std::vector<gfx::DisplayType> displays;

  // using --disable-d3d11 with the default --use-angle should never return
  // D3D11.
  command_line->AppendSwitch(switches::kDisableD3D11);
  GetEGLInitDisplays(true, true, command_line.get(), &displays);
  EXPECT_EQ(std::find(displays.begin(), displays.end(), gfx::ANGLE_D3D11),
            displays.end());

  // Specifically requesting D3D11 should always return it if the extension is
  // available
  command_line->AppendSwitchASCII(switches::kUseANGLE,
                                  gfx::kANGLEImplementationD3D11Name);
  displays.clear();
  GetEGLInitDisplays(true, true, command_line.get(), &displays);
  EXPECT_NE(std::find(displays.begin(), displays.end(), gfx::ANGLE_D3D11),
            displays.end());
  EXPECT_EQ(displays.size(), 1u);

  // Specifically requesting D3D11 should not return D3D11 if the extension is
  // not available
  displays.clear();
  GetEGLInitDisplays(false, true, command_line.get(), &displays);
  EXPECT_EQ(std::find(displays.begin(), displays.end(), gfx::ANGLE_D3D11),
            displays.end());
}

TEST(EGLInitializationDisplaysTest, SwiftShader) {
  scoped_ptr<base::CommandLine> command_line(
      new base::CommandLine(base::CommandLine::NO_PROGRAM));

  std::vector<gfx::DisplayType> displays;

  // If swiftshader is requested, only SWIFT_SHADER should be returned
  command_line->AppendSwitchASCII(switches::kUseGL,
                                  gfx::kGLImplementationSwiftShaderName);
  displays.clear();
  GetEGLInitDisplays(true, true, command_line.get(), &displays);
  EXPECT_NE(std::find(displays.begin(), displays.end(), gfx::SWIFT_SHADER),
            displays.end());
  EXPECT_EQ(displays.size(), 1u);

  // Even if there are other flags, swiftshader should take prescedence
  command_line->AppendSwitchASCII(switches::kUseANGLE,
                                  gfx::kANGLEImplementationD3D11Name);
  displays.clear();
  GetEGLInitDisplays(true, true, command_line.get(), &displays);
  EXPECT_NE(std::find(displays.begin(), displays.end(), gfx::SWIFT_SHADER),
            displays.end());
  EXPECT_EQ(displays.size(), 1u);
}

TEST(EGLInitializationDisplaysTest, DefaultRenderers) {
  scoped_ptr<base::CommandLine> command_line(
      new base::CommandLine(base::CommandLine::NO_PROGRAM));

  // Default without --use-angle flag
  std::vector<gfx::DisplayType> default_no_flag_displays;
  GetEGLInitDisplays(true, true, command_line.get(), &default_no_flag_displays);
  EXPECT_FALSE(default_no_flag_displays.empty());

  // Default with --use-angle flag
  command_line->AppendSwitchASCII(switches::kUseANGLE,
                                  gfx::kANGLEImplementationDefaultName);
  std::vector<gfx::DisplayType> default_with_flag_displays;
  GetEGLInitDisplays(true, true, command_line.get(),
                     &default_with_flag_displays);
  EXPECT_FALSE(default_with_flag_displays.empty());

  // Make sure the same results are returned
  EXPECT_EQ(default_no_flag_displays, default_with_flag_displays);
}

TEST(EGLInitializationDisplaysTest, NonDefaultRenderers) {
  scoped_ptr<base::CommandLine> command_line(
      new base::CommandLine(base::CommandLine::NO_PROGRAM));

  std::vector<gfx::DisplayType> displays;

  // OpenGL
  command_line->AppendSwitchASCII(switches::kUseANGLE,
                                  gfx::kANGLEImplementationOpenGLName);
  displays.clear();
  GetEGLInitDisplays(true, true, command_line.get(), &displays);
  EXPECT_NE(std::find(displays.begin(), displays.end(), gfx::ANGLE_OPENGL),
            displays.end());
  EXPECT_EQ(displays.size(), 1u);

  // OpenGLES
  command_line->AppendSwitchASCII(switches::kUseANGLE,
                                  gfx::kANGLEImplementationOpenGLESName);
  displays.clear();
  GetEGLInitDisplays(true, true, command_line.get(), &displays);
  EXPECT_NE(std::find(displays.begin(), displays.end(), gfx::ANGLE_OPENGLES),
            displays.end());
  EXPECT_EQ(displays.size(), 1u);
}

TEST(EGLInitializationDisplaysTest, NoExtensions) {
  scoped_ptr<base::CommandLine> command_line(
      new base::CommandLine(base::CommandLine::NO_PROGRAM));

  // With no angle platform extensions, only DEFAULT should be available
  std::vector<gfx::DisplayType> displays;
  GetEGLInitDisplays(false, false, command_line.get(), &displays);
  EXPECT_NE(std::find(displays.begin(), displays.end(), gfx::DEFAULT),
            displays.end());
  EXPECT_EQ(displays.size(), 1u);
}

}  // namespace
