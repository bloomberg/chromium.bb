/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Contains windows-specific code for setting up the Client object
// used in the unit tests.  Defines WinMain and a WindowProc for running
// the GUnit tests

#include <windows.h>
#include <Shellapi.h>

#include "tests/common/win/testing_common.h"
#include "core/cross/install_check.h"
#include "core/cross/service_locator.h"
#include "core/cross/evaluation_counter.h"
#include "core/cross/client_info.h"
#include "core/cross/class_manager.h"
#include "core/cross/features.h"
#include "core/cross/object_manager.h"
#include "core/cross/profiler.h"
#include "core/cross/renderer.h"
#include "core/cross/renderer_platform.h"
#include "core/cross/types.h"
#include "core/win/display_window_win.h"

#if defined(RENDERER_CB)
#include "core/cross/command_buffer/renderer_cb.h"
#include "core/cross/command_buffer/display_window_cb.h"
#include "gpu/gpu_plugin/command_buffer.h"
#include "gpu/np_utils/np_browser_stub.h"
#endif

using o3d::DisplayWindowWindows;

#if defined(RENDERER_CB)
using o3d::DisplayWindowCB;
using gpu_plugin::CommandBuffer;
using gpu_plugin::NPObjectPointer;
using gpu_plugin::StubNPBrowser;
using o3d::RendererCBLocal;
#endif

const int kWindowWidth = 16;
const int kWindowHeight = 16;

o3d::ServiceLocator* g_service_locator = NULL;
o3d::DisplayWindow* g_display_window = NULL;

HWND g_window_handle = NULL;

o3d::String *g_program_path = NULL;
o3d::String *g_program_name = NULL;
o3d::Renderer* g_renderer = NULL;

static wchar_t kOffScreenRenderer[] = L"O3D_D3D9_OFF_SCREEN";

extern int test_main(int argc, wchar_t **argv);

LRESULT CALLBACK WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  return true;
}

// Handles some errors that would typically cause an OS dialog box to appear.
LONG WINAPI LocalUnhandledExceptionFilter(EXCEPTION_POINTERS* pep) {
  fprintf(stdout, "ERROR: Unhandled Exception\n");
  exit(EXIT_FAILURE);
}

// Main entry point for the app.  Creates a new window, and calls main()
int WINAPI WinMain(HINSTANCE instance,
                   HINSTANCE prev_instance,
                   LPSTR cmd_line,
                   int n_cmd_show) {
  // Turn off some of the OS error dialogs.
  ::SetUnhandledExceptionFilter(LocalUnhandledExceptionFilter);

  std::string error;
  if (!o3d::RendererInstallCheck(&error)) {
    return false;
  }
  WNDCLASSEX wc = {0};

  wc.lpszClassName = L"MY_WINDOWS_CLASS";
  wc.cbSize = sizeof(WNDCLASSEX);
  wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
  wc.lpfnWndProc = ::WindowProc;
  wc.cbClsExtra = 0;
  wc.cbWndExtra = 0;
  wc.hInstance = instance;
  wc.hIcon = ::LoadIcon(instance, IDI_APPLICATION);
  wc.hIconSm = NULL;
  wc.hCursor = ::LoadCursor(instance, IDC_ARROW);
  wc.hbrBackground = static_cast<HBRUSH>(::GetStockObject(BLACK_BRUSH));
  wc.lpszMenuName = NULL;

  if (!::RegisterClassEx(&wc))
    return false;

  // Leaving this window onscreen leads to a redraw error which makes it
  // a hassle to debug tests in an IDE, so we place the window somewhere that
  // won't happen.
  g_window_handle = ::CreateWindowExW(NULL,
                                      wc.lpszClassName,
                                      L"",
                                      WS_OVERLAPPEDWINDOW,
                                      10,
                                      10,
                                      kWindowWidth,
                                      kWindowHeight,
                                      0,
                                      0,
                                      instance,
                                      0);

  if (g_window_handle == NULL)
    return false;

  static wchar_t program_filename[512];
  ::GetModuleFileNameW(NULL, program_filename, sizeof(program_filename));
  program_filename[511] = 0;

  std::wstring program_path(program_filename);
  std::wstring program_name;

  // Remove all characters starting with last '\'.
  size_t backslash_pos = program_path.rfind('\\');
  if (backslash_pos != o3d::String::npos) {
    program_name = &program_filename[backslash_pos + 1];
    program_path.erase(backslash_pos);
  } else {
    program_name = program_path;
  }

  o3d::String program_path_utf8 = WideToUTF8(program_path);
  o3d::String program_name_utf8 = WideToUTF8(program_name);
  g_program_path = &program_path_utf8;
  g_program_name = &program_name_utf8;

  o3d::ServiceLocator service_locator;
  g_service_locator = &service_locator;

  o3d::EvaluationCounter evaluation_counter(g_service_locator);
  o3d::ClassManager class_manager(g_service_locator);
  o3d::ClientInfoManager client_info_manager(g_service_locator);
  o3d::ObjectManager object_manager(g_service_locator);
  o3d::Profiler profiler(g_service_locator);
  o3d::Features features(g_service_locator);

  // TODO(apatrick): We can have an NPBrowser in the other configurations when
  //    we move over to gyp. This is just to avoid having to write scons files
  //    for np_utils.
#if defined(RENDERER_CB)
  StubNPBrowser browser;
#endif

  // create a renderer device based on the current platform
  g_renderer = o3d::Renderer::CreateDefaultRenderer(g_service_locator);

  // Initialize the renderer for off-screen rendering if kOffScreenRenderer
  // is in the environment.
  bool success;

#if defined(RENDERER_CB)
  const unsigned int kDefaultCommandBufferSize = 256 << 10;

  DisplayWindowCB* display_window = new o3d::DisplayWindowCB;
  display_window->set_npp(NULL);
  display_window->set_command_buffer(RendererCBLocal::CreateCommandBuffer(
      NULL,
      g_window_handle,
      kDefaultCommandBufferSize));
  display_window->set_width(kWindowWidth);
  display_window->set_height(kWindowHeight);
#else
  DisplayWindowWindows* display_window = new o3d::DisplayWindowWindows;
  display_window->set_hwnd(g_window_handle);
#endif

  g_display_window = display_window;
  bool offscreen = (::GetEnvironmentVariableW(kOffScreenRenderer,
                                              NULL, 0) != 0);
  if (offscreen) {
    success =
        g_renderer->Init(*g_display_window, true) == o3d::Renderer::SUCCESS;
  } else {
    success =
        g_renderer->Init(*g_display_window, false) == o3d::Renderer::SUCCESS;
    if (success) {
      ::SetWindowPos(g_window_handle, HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE);
      ::ShowWindow(g_window_handle, SW_SHOWNORMAL);
    }
  }

  int ret = EXIT_FAILURE;
  if (!success) {
    if (offscreen) {
      ::fprintf(stdout, "Failed to initialize OFFSCREEN renderer\n");
    } else {
      ::fprintf(stdout, "Failed to initialize on screen renderer\n");
    }
  } else {
    // Invoke the main entry point with the command-line arguments
    int arg_count;
    LPWSTR *arg_values = ::CommandLineToArgvW(::GetCommandLine(), &arg_count);
    ret = ::test_main(arg_count, arg_values);
    ::LocalFree(arg_values);
    g_renderer->Destroy();
  }

  delete g_renderer;
  g_renderer = NULL;

  delete display_window;
  g_display_window = NULL;
  g_program_path = NULL;
  g_program_name = NULL;

  return ret;
}
