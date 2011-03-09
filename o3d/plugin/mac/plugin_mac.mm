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



#include <GL/glew.h>
#include "plugin_mac.h"
#include "Breakpad.h"
#include <Cocoa/Cocoa.h>
#include <QuickTime/QuickTime.h>
#include "plugin/cross/o3d_glue.h"
#include "plugin/cross/main.h"
#include "core/mac/display_window_mac.h"
#include "core/cross/gl/renderer_gl.h"
#include "plugin/mac/graphics_utils_mac.h"
#import "plugin/mac/o3d_layer.h"


#if !defined(O3D_INTERNAL_PLUGIN)
BreakpadRef gBreakpadRef =  NULL;
#endif

using glue::_o3d::PluginObject;
using o3d::DisplayWindowMac;

@interface NSWindowController (plugin_hack)
- (id)selectedTab;
@end

namespace o3d {

// Returns the version number of the running Mac browser, as parsed from
// the short version string in the plist of the app's bundle.
bool GetBrowserVersionInfo(int *returned_major,
                           int *returned_minor,
                           int *returned_bugfix) {
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  int major = 0;
  int minor = 0;
  int bugfix = 0;
  NSBundle *app_bundle = [NSBundle mainBundle];
  NSString *versionString =
      [app_bundle objectForInfoDictionaryKey:@"CFBundleShortVersionString"];

  if ([versionString length]) {
    NSScanner *versionScanner = [NSScanner scannerWithString:versionString];
    NSCharacterSet *dotSet =
        [NSCharacterSet characterSetWithCharactersInString:@"."];
    NSCharacterSet *numSet =
        [NSCharacterSet characterSetWithCharactersInString:@"0123456789"];
    NSCharacterSet *notNumSet = [numSet invertedSet];

    // Skip anything at start that is not a number.
    [versionScanner scanCharactersFromSet:notNumSet intoString:NULL];

    // Keep reading values until we get all 3 numbers, or something isn't found.
    if ([versionScanner scanInt:&major])
      if ([versionScanner scanCharactersFromSet:dotSet intoString:NULL])
        if ([versionScanner scanInt:&minor])
          if ([versionScanner scanCharactersFromSet:dotSet intoString:NULL])
            [versionScanner scanInt:&bugfix];
  }

  *returned_major = major;
  *returned_minor = minor;
  *returned_bugfix = bugfix;

  [pool release];

  // If we read any version numbers, then we succeeded.
  return (major || minor || bugfix);
}


// Code for peeking at the selected tab object for a WindowRef in Safari.  This
// is purely so we can detect changes in active tab in Safari, since it does not
// notify us when our tab is hidden or revealed.
//
// If the browser is not Safari these functions should harmlessly be NULL since
// the walk from windowref to cocoa window (of type "BrowserWindow") to cocoa
// windowcontroller to selectedTab should harmlessly break down somewhere along
// the way.
//
// This is useful merely for peeking at Safari's internal state for a tab change
// event, as Safari does not notify us when this happens.  It coerces the value
// to a void*, as the plain .cc files don't like the type id, and we actually
// only care whether the value changes or not.


void ReleaseSafariBrowserWindow(void* browserWindow) {
  if (browserWindow) {
    NSWindow* cocoaWindow = (NSWindow*) browserWindow;
    // Retain the WindowRef so it doesn't go away when we release the
    // NSWindow copy we made.
    WindowRef theWindow = (WindowRef)[cocoaWindow windowRef];
    if (theWindow)
      CFRetain(theWindow);
    [cocoaWindow release];
  }
}

void* SafariBrowserWindowForWindowRef(WindowRef theWindow) {
  if (theWindow != NULL) {
    NSWindow* cocoaWindow = [[NSWindow alloc] initWithWindowRef:theWindow];
    if (cocoaWindow) {
      if (strcmp(object_getClassName(cocoaWindow), "BrowserWindow") == 0) {
        return cocoaWindow;
      } else {
        // Retain the WindowRef so it doesn't go away when we release the
        // NSWindow copy we made.
        CFRetain(theWindow);
        [cocoaWindow release];
      }
    }
  }
  return NULL;
}

void* SelectedTabForSafariBrowserWindow(void* browserWindow) {
  id selectedTab = nil;
  NSWindow* cocoaWindow = (NSWindow*) browserWindow;
  if (cocoaWindow) {
    @try {
      selectedTab = [[cocoaWindow windowController] selectedTab];
    } @catch(NSException* exc) {
    }
  }
  return (void*) selectedTab;
}

// Detects when Safari has hidden or revealed our tab.
// If a hide event is detected, it sets the mac_surface_hidden_ flag and hides
// the surface.
// Later, if the mac_surface_hidden_ flag is set but we are no longer hidden,
// ie DetectTabHiding is now returning false, it restores the surface to the
// previous state.
void ManageSafariTabSwitching(PluginObject* obj) {
  // This is only needed when we are using an AGL context and need to hide
  // and show it.
  if (!obj->mac_agl_context_)
    return;

  if (obj->DetectTabHiding()) {
    if (!obj->mac_surface_hidden_) {
      obj->mac_surface_hidden_ = true;
      GLint rect[4] = {0,0,0,0};
      aglSetInteger(obj->mac_agl_context_, AGL_BUFFER_RECT, rect);
      aglEnable(obj->mac_agl_context_, AGL_BUFFER_RECT);
    }
  } else if (obj->mac_surface_hidden_) {
    obj->mac_surface_hidden_ = false;
    aglSetInteger(obj->mac_agl_context_, AGL_BUFFER_RECT,
                  obj->last_buffer_rect_);
    aglEnable(obj->mac_agl_context_, AGL_BUFFER_RECT);
  }
}


#pragma mark ____RenderTimer

RenderTimer gRenderTimer;
std::vector<NPP> RenderTimer::instances_;

void RenderTimer::Start() {
  CFRunLoopTimerContext timerContext;
  memset(&timerContext, 0, sizeof(timerContext));
  timerContext.info = (void*)NULL;

  if (!timerRef_) {
    timerRef_ = CFRunLoopTimerCreate(NULL,
                                     1.0,
                                     1.0 / 60.0,
                                     0,
                                     0,
                                     TimerCallback,
                                     &timerContext);

    CFRunLoopAddTimer(CFRunLoopGetCurrent(), timerRef_, kCFRunLoopCommonModes);
  }
}

void RenderTimer::Stop() {
  if (timerRef_) {
    CFRunLoopTimerInvalidate(timerRef_);
    timerRef_ = NULL;
  }
  instances_.clear();   // this should already be empty, but make sure
}

void RenderTimer::AddInstance(NPP instance) {
  // avoid adding the same instance twice!
  InstanceIterator i = find(instances_.begin(), instances_.end(), instance);
  if (i == instances_.end()) {
    instances_.push_back(instance);
  }
}

void RenderTimer::RemoveInstance(NPP instance) {
  InstanceIterator i = find(instances_.begin(), instances_.end(), instance);
  if (i != instances_.end()) {
    instances_.erase(i);
  }
}

void RenderTimer::TimerCallback(CFRunLoopTimerRef timer, void* info) {
  HANDLE_CRASHES;
  int instance_count = instances_.size();
  for (int i = 0; i < instance_count; ++i) {
    NPP instance = instances_[i];
    PluginObject* obj = static_cast<PluginObject*>(instance->pdata);

    // RenderClient() and Tick() may cause events to be processed,
    // leading to reentrant calling of this code. Detect and avoid
    // this case.
    if (obj->client()->IsRendering() || obj->client()->IsTicking()) {
      continue;
    }

    bool in_fullscreen = obj->GetFullscreenMacWindow();

    if (obj->drawing_model_ == NPDrawingModelCoreAnimation &&
        !in_fullscreen) {
      O3DLayer* o3dLayer = static_cast<O3DLayer*>(obj->gl_layer_);
      if (o3dLayer) {
        obj->client()->Tick();

        if (obj->client()->NeedsRender()) {
          [o3dLayer setNeedsDisplay];
        }
      }
      continue;
    }

    ManageSafariTabSwitching(obj);
    obj->client()->Tick();

    // It's possible that event processing may have torn down the
    // full-screen window in the call above.
    in_fullscreen = obj->GetFullscreenMacWindow();

    if (in_fullscreen) {
      obj->GetFullscreenMacWindow()->IdleCallback();
    }

    // We're visible if (a) we are in fullscreen mode, (b) we are using
    // QuickDraw and our cliprect height and width are both a sensible size, ie
    // > 1 pixel, or (c) we are using Core Graphics.
    //
    // We don't check for 0 as we have to size to 1 x 1 on occasion rather than
    // 0 x 0 to avoid crashing the Apple software renderer, but do not want to
    // actually draw to a 1 x 1 pixel area.
    bool plugin_visible = in_fullscreen ||
        (obj->drawing_model_ == NPDrawingModelQuickDraw &&
             obj->last_buffer_rect_[2] > 1 && obj->last_buffer_rect_[3] > 1) ||
        obj->drawing_model_ == NPDrawingModelCoreGraphics;

    if (plugin_visible && obj->renderer()) {
      if (obj->client()->NeedsRender()) {
        // Force a sync to the VBL (once per timer callback)
        // to avoid tearing, if using GL.
        GLint sync = (i == 0);
        if (obj->mac_cgl_context_) {
          CGLSetParameter(obj->mac_cgl_context_, kCGLCPSwapInterval, &sync);
        } else if (obj->mac_agl_context_) {
          aglSetInteger(obj->mac_agl_context_, AGL_SWAP_INTERVAL, &sync);
        }

        if (obj->drawing_model_ == NPDrawingModelCoreGraphics) {
          NPRect rect = { 0 };
          rect.bottom = obj->height();
          rect.right = obj->width();
          NPN_InvalidateRect(instance, &rect);
        } else {
          if (in_fullscreen) {
            obj->GetFullscreenMacWindow()->PrepareToRender();
          }
          obj->client()->RenderClient(true);
          if (in_fullscreen) {
            obj->GetFullscreenMacWindow()->FinishRendering();
          }
        }
      }
    }
  }
}

#pragma mark ____BREAKPAD

bool ExceptionCallback(int exception_type,
                       int exception_code,
                       mach_port_t crashing_thread,
                       void* context) {
  return BreakpadEnabler::IsEnabled();
}

void InitializeBreakpad() {
  if (!gBreakpadRef) {
    NSBundle* bundle = [NSBundle bundleWithIdentifier:@"com.google.o3d"];
    NSDictionary* info = [bundle infoDictionary];

    gBreakpadRef = BreakpadCreate(info);
    BreakpadSetFilterCallback(gBreakpadRef, ExceptionCallback, NULL);
  }
}

void ShutdownBreakpad() {
  BreakpadRelease(gBreakpadRef);
  gBreakpadRef = NULL;
}

#pragma mark ____MISCELLANEOUS_HELPER

void CFReleaseIfNotNull(CFTypeRef cf) {
  if (cf != NULL)
    CFRelease(cf);
}



// Converts an old style Mac HFS path eg "HD:Users:xxx:file.zip" into
// a standard Posix path eg "/Users/xxx/file.zip"
// Assumes UTF8 in and out, returns a block of memory allocated with new,
// so you'll want to delete this at some point.
// Returns NULL in the event of an error.
char* CreatePosixFilePathFromHFSFilePath(const char* hfsPath) {
  CFStringRef cfHFSPath = NULL;
  CFStringRef cfPosixPath = NULL;
  CFURLRef cfHFSURL = NULL;
  char* posix_path = NULL;

  if (hfsPath && hfsPath[0]) {
    cfHFSPath = CFStringCreateWithCString(kCFAllocatorDefault,
                                          hfsPath,
                                          kCFStringEncodingUTF8);
    if (cfHFSPath) {
      cfHFSURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
                                               cfHFSPath,
                                               kCFURLHFSPathStyle,
                                               false);
      if (cfHFSURL) {
        cfPosixPath = CFURLCopyFileSystemPath(cfHFSURL, kCFURLPOSIXPathStyle);
        if (cfPosixPath) {
          // returned value includes space for 0 terminator byte
          int maxSize =
              CFStringGetMaximumSizeOfFileSystemRepresentation(cfPosixPath);
          posix_path = new char [maxSize];
          CFStringGetFileSystemRepresentation(cfPosixPath,
                                              posix_path,
                                              maxSize);
        }
      }
    }
  }
  CFReleaseIfNotNull(cfHFSPath);
  CFReleaseIfNotNull(cfPosixPath);
  CFReleaseIfNotNull(cfHFSURL);
  return posix_path;
}



#pragma mark ____SCREEN_RESOLUTION_MANAGEMENT


// Constant kO3D_MODE_OFFSET is added to the position in the array returned by
// CGDisplayAvailableModes to make it an ID. This makes IDs distinguishable from
// array positions when debugging, and also means that ID 0 can have a special
// meaning (current mode) rather than meaning the first resolution in the list.
const int kO3D_MODE_OFFSET = 100;

// Extracts data from the Core Graphics screen mode data passed in.
// Returns false if the mode is an undesirable one, ie if it is not safe for
// the current screen hardware, is stretched, or is interlaced.
// Returns various information about the mode in the var parameters passed.
static bool ExtractDisplayModeData(NSDictionary *mode_dict,
                                   int *width,
                                   int *height,
                                   int *refresh_rate,
                                   int *bits_per_pixel) {

  *width = [[mode_dict objectForKey:(id)kCGDisplayWidth] intValue];
  *height = [[mode_dict objectForKey:(id)kCGDisplayHeight] intValue];
  *refresh_rate = [[mode_dict objectForKey:(id)kCGDisplayRefreshRate] intValue];
  *bits_per_pixel =
      [[mode_dict objectForKey:(id)kCGDisplayBitsPerPixel] intValue];

  if (![mode_dict objectForKey:(id)kCGDisplayModeIsSafeForHardware])
    return false;

  if ([mode_dict objectForKey:(id)kCGDisplayModeIsStretched])
    return false;

  if ([mode_dict objectForKey:(id)kCGDisplayModeIsInterlaced])
    return false;

  return true;
}


// Returns information on a display mode, which is mode n - kO3D_MODE_OFFSET
// in the raw list returned by CGDisplayAvailableModes on the main screen,
// with kO3D_MODE_OFFSET + 0 being the first entry in the array.
static bool GetOtherDisplayMode(int id, o3d::DisplayMode *mode) {
  NSArray *mac_modes = (NSArray*) CGDisplayAvailableModes(CGMainDisplayID());
  int num_modes = [mac_modes count];
  int array_offset = id - kO3D_MODE_OFFSET;

  if (array_offset >= 0 && array_offset < num_modes) {
    int width = 0;
    int height = 0;
    int refresh_rate = 0;
    int bpp = 0;

    ExtractDisplayModeData([mac_modes objectAtIndex:array_offset],
                           &width, &height, &refresh_rate, &bpp);
    mode->Set(width, height, refresh_rate, id);
    return true;
  }

  return false;
}


static int GetCGDisplayModeID(NSDictionary* mode_dict) {
  return [[mode_dict valueForKey:@"Mode"] intValue];
}

// Returns DisplayMode data for the current state of the main display.
void GetCurrentDisplayMode(o3d::DisplayMode *mode) {
  int width = 0;
  int height = 0;
  int refresh_rate = 0;
  int bpp = 0;
  int mode_id = 0;

  NSDictionary* current_mode =
      (NSDictionary*)CGDisplayCurrentMode(CGMainDisplayID());

  // To get the O3D mode id of the current mode, we need to find it in the list
  // of all modes, since the id we use is it's index + kO3D_MODE_OFFSET.

  // Get the CG id of current mode so that we will recognize it.
  int current_cg_id = GetCGDisplayModeID(current_mode);

  // Get list of all modes.
  NSArray *modes = (NSArray*)CGDisplayAvailableModes(CGMainDisplayID());
  int num_modes = [modes count];

  // Find current mode in that list, and compute the O3D id for it.
  for (int x = 0 ; x < num_modes ; x++) {
    if (GetCGDisplayModeID([modes objectAtIndex:x]) == current_cg_id) {
      mode_id = x + kO3D_MODE_OFFSET;
      break;
    }
  }

  ExtractDisplayModeData(current_mode, &width, &height, &refresh_rate, &bpp);
  mode->Set(width, height, refresh_rate, mode_id);
}



}  // namespace o3d


bool PluginObject::GetDisplayMode(int id, o3d::DisplayMode *mode) {
  if (id == o3d::Renderer::DISPLAY_MODE_DEFAULT) {
    GetCurrentDisplayMode(mode);
    return true;
  } else {
    return GetOtherDisplayMode(id, mode);
  }
}


void PluginObject::GetDisplayModes(std::vector<o3d::DisplayMode> *modes) {
  NSArray* mac_modes = (NSArray*)CGDisplayAvailableModes(CGMainDisplayID());
  int num_modes = [mac_modes count];
  std::vector<o3d::DisplayMode> modes_found;

  for (int i = 0; i < num_modes; ++i) {
    int width = 0;
    int height = 0;
    int refresh_rate = 0;
    int bpp = 0;

    if (o3d::ExtractDisplayModeData([mac_modes objectAtIndex:i],
                                    &width,
                                    &height,
                                    &refresh_rate,
                                    &bpp) && bpp == 32)
      modes_found.push_back(o3d::DisplayMode(width, height, refresh_rate,
                                             i + o3d::kO3D_MODE_OFFSET));
  }

  modes->swap(modes_found);
}


#pragma mark ____FULLSCREEN_SWITCHING

namespace glue {
namespace _o3d {

bool PluginObject::RequestFullscreenDisplay() {
  // If already in fullscreen mode, do nothing.
  if (GetFullscreenMacWindow())
    return false;

  int target_width = 0;
  int target_height = 0;

  if (fullscreen_region_valid_ &&
      fullscreen_region_mode_id_ != Renderer::DISPLAY_MODE_DEFAULT) {
    o3d::DisplayMode the_mode;
    if (GetDisplayMode(fullscreen_region_mode_id_, &the_mode)) {
      target_width = the_mode.width();
      target_height = the_mode.height();
    }
  }

  was_offscreen_ = IsOffscreenRenderingEnabled();
  if (was_offscreen_) {
    DisableOffscreenRendering();
  }

  FullscreenWindowMac* fullscreen_window =
      FullscreenWindowMac::Create(this, target_width, target_height);
  if (!fullscreen_window) {
    if (was_offscreen_) {
      EnableOffscreenRendering();
    }
    return false;
  }
  SetFullscreenMacWindow(fullscreen_window);
  Rect bounds = o3d::CGRect2Rect(fullscreen_window->GetWindowBounds());

  renderer()->SetClientOriginOffset(0, 0);
  renderer_->Resize(bounds.right - bounds.left, bounds.bottom - bounds.top);

  fullscreen_ = true;
  client()->SendResizeEvent(renderer_->width(), renderer_->height(), true);

  return true;
}

void PluginObject::CancelFullscreenDisplay() {
  FullscreenWindowMac* window = GetFullscreenMacWindow();

  // if not in fullscreen mode, do nothing
  if (!window)
    return;

  // The focus change during closing of the fullscreen window may
  // cause the plugin to become reentrant. Store the full-screen
  // window on the stack to prevent attempting to shut down twice.
  SetFullscreenMacWindow(NULL);
  window->Shutdown(last_buffer_rect_);
  delete window;
  renderer_->Resize(prev_width_, prev_height_);
  fullscreen_ = false;
  client()->SendResizeEvent(prev_width_, prev_height_, false);

  if (was_offscreen_) {
    EnableOffscreenRendering();
  }

  // Somehow the browser window does not automatically activate again
  // when we close the fullscreen window, so explicitly reactivate it.
  if (mac_cocoa_window_) {
    NSWindow* browser_window = (NSWindow*) mac_cocoa_window_;
    [browser_window makeKeyAndOrderFront:browser_window];
  } else if (mac_window_) {
    SelectWindow(mac_window_);
  }
}

#define PFA(number) static_cast<NSOpenGLPixelFormatAttribute>(number)

#define O3D_NSO_COLOR_AND_DEPTH_SETTINGS NSOpenGLPFAClosestPolicy, \
                                     NSOpenGLPFAColorSize, PFA(24), \
                                     NSOpenGLPFAAlphaSize, PFA(8),  \
                                     NSOpenGLPFADepthSize, PFA(24), \
                                     NSOpenGLPFADoubleBuffer,
// The Core Animation code path on 10.6 core dumps if the
// NSOpenGLPFAPixelBuffer and NSOpenGLPFAWindow attributes are
// specified. It seems risky for the 10.5 code path if they aren't,
// since this actual CGLPixelFormatObj is used to create the pbuffer's
// context, but no ill effects have been seen there so leaving them
// out for now.
#define O3D_NSO_PBUFFER_SETTINGS
#define O3D_NSO_STENCIL_SETTINGS NSOpenGLPFAStencilSize, PFA(8),
#define O3D_NSO_HARDWARE_RENDERER \
            NSOpenGLPFAAccelerated, NSOpenGLPFANoRecovery,
#define O3D_NSO_MULTISAMPLE \
            NSOpenGLPFAMultisample, NSOpenGLPFASamples, PFA(4),
#define O3D_NSO_END PFA(0)

CGLContextObj PluginObject::GetFullscreenShareContext() {
  if (mac_fullscreen_nsopenglcontext_ == NULL) {
    static const NSOpenGLPixelFormatAttribute attributes[] = {
      O3D_NSO_COLOR_AND_DEPTH_SETTINGS
      O3D_NSO_PBUFFER_SETTINGS
      O3D_NSO_STENCIL_SETTINGS
      O3D_NSO_HARDWARE_RENDERER
      O3D_NSO_MULTISAMPLE
      O3D_NSO_END
    };
    NSOpenGLPixelFormat* pixel_format =
        [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
    if (!pixel_format) {
      // Try a less capable set.
      static const NSOpenGLPixelFormatAttribute low_end_attributes[] = {
        O3D_NSO_COLOR_AND_DEPTH_SETTINGS
        O3D_NSO_PBUFFER_SETTINGS
        O3D_NSO_STENCIL_SETTINGS
        O3D_NSO_HARDWARE_RENDERER
        O3D_NSO_END
      };
      pixel_format =
          [[NSOpenGLPixelFormat alloc] initWithAttributes:low_end_attributes];
    }
    if (pixel_format) {
      mac_fullscreen_nsopenglpixelformat_ = pixel_format;

      NSOpenGLContext* context =
          [[NSOpenGLContext alloc] initWithFormat:pixel_format
                                     shareContext:nil];
      mac_fullscreen_nsopenglcontext_ = context;
    } else {
      DLOG(ERROR) << "Error choosing NSOpenGLPixelFormat.";
    }
  }

  NSOpenGLContext* context = (NSOpenGLContext*) mac_fullscreen_nsopenglcontext_;
  return (CGLContextObj) [context CGLContextObj];
}

void* PluginObject::GetFullscreenNSOpenGLContext() {
  return mac_fullscreen_nsopenglcontext_;
}

CGLPixelFormatObj PluginObject::GetFullscreenCGLPixelFormatObj() {
  NSOpenGLPixelFormat* pixel_format =
      (NSOpenGLPixelFormat*) mac_fullscreen_nsopenglpixelformat_;
  if (pixel_format == nil) {
    return NULL;
  }
  return (CGLPixelFormatObj) [pixel_format CGLPixelFormatObj];
}

void PluginObject::CleanupFullscreenOpenGLContext() {
  NSOpenGLContext* context =
      (NSOpenGLContext*) mac_fullscreen_nsopenglcontext_;
  mac_fullscreen_nsopenglcontext_ = NULL;
  if (context) {
    [context release];
  }
  NSOpenGLPixelFormat* format =
      (NSOpenGLPixelFormat*) mac_fullscreen_nsopenglpixelformat_;
  mac_fullscreen_nsopenglpixelformat_ = NULL;
  if (format) {
    [format release];
  }
}

void PluginObject::SetMacCGLContext(CGLContextObj context) {
  mac_cgl_context_ = context;
  if (renderer_) {
#ifdef RENDERER_GLES2
    ((o3d::RendererGLES2*) renderer_)->set_mac_cgl_context(context);
#else
    ((o3d::RendererGL*) renderer_)->set_mac_cgl_context(context);
#endif
  }
}

}  // namespace glue
}  // namespace o3d
