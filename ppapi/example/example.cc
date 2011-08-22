// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>
#include <stdio.h>  // FIXME(brettw) erase me.
#ifndef _WIN32
#include <sys/time.h>
#else
#include <windows.h>
#endif
#include <time.h>

#include <algorithm>

#include "ppapi/c/dev/ppb_console_dev.h"
#include "ppapi/c/dev/ppb_cursor_control_dev.h"
#include "ppapi/c/dev/ppp_printing_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/dev/memory_dev.h"
#include "ppapi/cpp/dev/scriptable_object_deprecated.h"
#include "ppapi/cpp/graphics_2d.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/input_event.h"
#include "ppapi/cpp/private/instance_private.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/private/var_private.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/url_loader.h"
#include "ppapi/cpp/url_request_info.h"
#include "ppapi/cpp/var.h"

static const int kStepsPerCircle = 800;

void FlushCallback(void* data, int32_t result);

void FillRect(pp::ImageData* image, int left, int top, int width, int height,
              uint32_t color) {
  for (int y = std::max(0, top);
       y < std::min(image->size().height() - 1, top + height);
       y++) {
    for (int x = std::max(0, left);
         x < std::min(image->size().width() - 1, left + width);
         x++)
      *image->GetAddr32(pp::Point(x, y)) = color;
  }
}

class MyScriptableObject : public pp::deprecated::ScriptableObject {
 public:
  explicit MyScriptableObject(pp::InstancePrivate* instance)
      : instance_(instance) {}

  virtual bool HasMethod(const pp::Var& method, pp::Var* exception) {
    return method.AsString() == "toString";
  }

  virtual bool HasProperty(const pp::Var& name, pp::Var* exception) {
    if (name.is_string() && name.AsString() == "blah")
      return true;
    return false;
  }

  virtual pp::Var GetProperty(const pp::Var& name, pp::Var* exception) {
    if (name.is_string() && name.AsString() == "blah")
      return pp::VarPrivate(instance_, new MyScriptableObject(instance_));
    return pp::Var();
  }

  virtual void GetAllPropertyNames(std::vector<pp::Var>* names,
                                   pp::Var* exception) {
    names->push_back("blah");
  }

  virtual pp::Var Call(const pp::Var& method,
                       const std::vector<pp::Var>& args,
                       pp::Var* exception) {
    if (method.AsString() == "toString")
      return pp::Var("hello world");
    return pp::Var();
  }

 private:
  pp::InstancePrivate* instance_;
};

class MyFetcherClient {
 public:
  virtual void DidFetch(bool success, const std::string& data) = 0;
};

class MyFetcher {
 public:
  MyFetcher() : client_(NULL) {
    callback_factory_.Initialize(this);
  }

  void Start(pp::InstancePrivate& instance,
             const pp::Var& url,
             MyFetcherClient* client) {
    pp::URLRequestInfo request(&instance);
    request.SetURL(url);
    request.SetMethod("GET");

    loader_ = pp::URLLoader(instance);
    client_ = client;

    pp::CompletionCallback callback =
        callback_factory_.NewOptionalCallback(&MyFetcher::DidOpen);
    int rv = loader_.Open(request, callback);
    if (rv != PP_OK_COMPLETIONPENDING)
      callback.Run(rv);
  }

  void StartWithOpenedLoader(const pp::URLLoader& loader,
                             MyFetcherClient* client) {
    loader_ = loader;
    client_ = client;

    ReadMore();
  }

 private:
  void ReadMore() {
    pp::CompletionCallback callback =
        callback_factory_.NewOptionalCallback(&MyFetcher::DidRead);
    int rv = loader_.ReadResponseBody(buf_, sizeof(buf_), callback);
    if (rv != PP_OK_COMPLETIONPENDING)
      callback.Run(rv);
  }

  void DidOpen(int32_t result) {
    if (result == PP_OK) {
      ReadMore();
    } else {
      DidFinish(result);
    }
  }

  void DidRead(int32_t result) {
    if (result > 0) {
      data_.append(buf_, result);
      ReadMore();
    } else {
      DidFinish(result);
    }
  }

  void DidFinish(int32_t result) {
    if (client_)
      client_->DidFetch(result == PP_OK, data_);
  }

  pp::CompletionCallbackFactory<MyFetcher> callback_factory_;
  pp::URLLoader loader_;
  MyFetcherClient* client_;
  char buf_[4096];
  std::string data_;
};

class MyInstance : public pp::InstancePrivate, public MyFetcherClient {
 public:
  MyInstance(PP_Instance instance)
      : pp::InstancePrivate(instance),
        time_at_last_check_(0.0),
        fetcher_(NULL),
        width_(0),
        height_(0),
        animation_counter_(0),
        print_settings_valid_(false),
        showing_custom_cursor_(false) {
    RequestInputEvents(PP_INPUTEVENT_CLASS_MOUSE);
  }

  virtual ~MyInstance() {
    if (fetcher_) {
      delete fetcher_;
      fetcher_ = NULL;
    }
  }

  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]) {
    return true;
  }

  void Log(PP_LogLevel_Dev level, const pp::Var& value) {
    const PPB_Console_Dev* console = reinterpret_cast<const PPB_Console_Dev*>(
        pp::Module::Get()->GetBrowserInterface(PPB_CONSOLE_DEV_INTERFACE));
    if (!console)
      return;
    console->Log(pp_instance(), level, value.pp_var());
  }

  virtual bool HandleDocumentLoad(const pp::URLLoader& loader) {
    fetcher_ = new MyFetcher();
    fetcher_->StartWithOpenedLoader(loader, this);
    return true;
  }

  virtual bool HandleInputEvent(const pp::InputEvent& event) {
    switch (event.GetType()) {
      case PP_INPUTEVENT_TYPE_MOUSEDOWN:
        SayHello();
        ToggleCursor();
        return true;
      case PP_INPUTEVENT_TYPE_MOUSEMOVE:
        return true;
      case PP_INPUTEVENT_TYPE_KEYDOWN:
        return true;
      default:
        return false;
    }
  }

  virtual pp::Var GetInstanceObject() {
    return pp::VarPrivate(this, new MyScriptableObject(this));
  }

  pp::ImageData PaintImage(int width, int height) {
    pp::ImageData image(this, PP_IMAGEDATAFORMAT_BGRA_PREMUL,
                        pp::Size(width, height), false);
    if (image.is_null()) {
      printf("Couldn't allocate the image data: %d, %d\n", width, height);
      return image;
    }

    // Fill with semitransparent gradient.
    for (int y = 0; y < image.size().height(); y++) {
      char* row = &static_cast<char*>(image.data())[y * image.stride()];
      for (int x = 0; x < image.size().width(); x++) {
        row[x * 4 + 0] = y;
        row[x * 4 + 1] = y;
        row[x * 4 + 2] = 0;
        row[x * 4 + 3] = y;
      }
    }

    // Draw the orbiting box.
    float radians = static_cast<float>(animation_counter_) / kStepsPerCircle *
        2 * 3.14159265358979F;

    float radius = static_cast<float>(std::min(width, height)) / 2.0f - 3.0f;
    int x = static_cast<int>(cos(radians) * radius + radius + 2);
    int y = static_cast<int>(sin(radians) * radius + radius + 2);

    const uint32_t box_bgra = 0x80000000;  // Alpha 50%.
    FillRect(&image, x - 3, y - 3, 7, 7, box_bgra);
    return image;
  }

  void Paint() {
    pp::ImageData image = PaintImage(width_, height_);
    if (!image.is_null()) {
      device_context_.ReplaceContents(&image);
      device_context_.Flush(pp::CompletionCallback(&FlushCallback, this));
    } else {
      printf("NullImage: %d, %d\n", width_, height_);
    }
  }

  virtual void DidChangeView(const pp::Rect& position, const pp::Rect& clip) {
    Log(PP_LOGLEVEL_LOG, "DidChangeView");
    if (position.size().width() == width_ &&
        position.size().height() == height_)
      return;  // We don't care about the position, only the size.

    width_ = position.size().width();
    height_ = position.size().height();
    printf("DidChangeView relevant change: width=%d height:%d\n",
           width_, height_);

    device_context_ = pp::Graphics2D(this, pp::Size(width_, height_), false);
    if (!BindGraphics(device_context_)) {
      printf("Couldn't bind the device context\n");
      return;
    }

    Paint();
  }

#if defined(_WIN32)
struct timeval {
  long tv_sec;
  long tv_usec;
};

struct timezone {
  long x;
  long y;
};

#define EPOCHFILETIME (116444736000000000i64)

int gettimeofday(struct timeval *tv, struct timezone*) {
  FILETIME ft;
  ::GetSystemTimeAsFileTime(&ft);
  LARGE_INTEGER li = {ft.dwLowDateTime, ft.dwHighDateTime};
  __int64 t = li.QuadPart - EPOCHFILETIME;
  tv->tv_sec  = static_cast<long>(t / 10000000);
  tv->tv_usec = static_cast<long>(t % 10000000);
  return 0;
}
#endif  // if defined(_WIN32)

  void UpdateFps() {
    pp::VarPrivate window = GetWindowObject();
    if (window.is_undefined())
      return;
    pp::VarPrivate doc = window.GetProperty("document");
    pp::VarPrivate fps = doc.Call("getElementById", "fps");

    struct timeval tv;
    struct timezone tz = {0, 0};
    gettimeofday(&tv, &tz);

    double time_now = tv.tv_sec + tv.tv_usec / 1000000.0;

    if (animation_counter_ > 0) {
      char fps_text[64];
      sprintf(fps_text, "%g fps",
              kStepsPerCircle / (time_now - time_at_last_check_));
      fps.SetProperty("innerHTML", fps_text);
    }

    time_at_last_check_ = time_now;
  }

  // Print interfaces.
  // TODO(mball,dmichael) Replace this with the PPP_PRINTING_DEV_USE_0_4 version
  virtual PP_PrintOutputFormat_Dev* QuerySupportedPrintOutputFormats(
      uint32_t* format_count) {
    pp::Memory_Dev memory;
    PP_PrintOutputFormat_Dev* format =
        static_cast<PP_PrintOutputFormat_Dev*>(
            memory.MemAlloc(sizeof(PP_PrintOutputFormat_Dev)));
    *format = PP_PRINTOUTPUTFORMAT_RASTER;
    *format_count = 1;
    return format;
  }

  virtual int32_t PrintBegin(const PP_PrintSettings_Dev& print_settings) {
    if (print_settings_.format != PP_PRINTOUTPUTFORMAT_RASTER)
      return 0;

    print_settings_ = print_settings;
    print_settings_valid_ = true;
    return 1;
  }

  virtual pp::Resource PrintPages(
      const PP_PrintPageNumberRange_Dev* page_ranges,
      uint32_t page_range_count) {
    if (!print_settings_valid_)
      return pp::Resource();

    if (page_range_count != 1)
      return pp::Resource();

    // Check if the page numbers are valid. We returned 1 in PrintBegin so we
    // only have 1 page to print.
    if (page_ranges[0].first_page_number || page_ranges[0].last_page_number) {
      return pp::Resource();
    }

    int width = static_cast<int>(
        (print_settings_.printable_area.size.width / 72.0) *
         print_settings_.dpi);
    int height = static_cast<int>(
        (print_settings_.printable_area.size.height / 72.0) *
         print_settings_.dpi);

    return PaintImage(width, height);
  }

  virtual void PrintEnd() {
    print_settings_valid_ = false;
  }

  void OnFlush() {
    if (animation_counter_ % kStepsPerCircle == 0)
      UpdateFps();
    animation_counter_++;
    Paint();
  }

 private:
  void SayHello() {
    pp::VarPrivate window = GetWindowObject();
    pp::VarPrivate doc = window.GetProperty("document");
    pp::VarPrivate body = doc.GetProperty("body");

    pp::VarPrivate obj(this, new MyScriptableObject(this));

    // Our object should have its toString method called.
    Log(PP_LOGLEVEL_LOG, "Testing MyScriptableObject::toString():");
    Log(PP_LOGLEVEL_LOG, obj);

    // body.appendChild(body) should throw an exception
    Log(PP_LOGLEVEL_LOG, "Calling body.appendChild(body):");
    pp::Var exception;
    body.Call("appendChild", body, &exception);
    Log(PP_LOGLEVEL_LOG, exception);

    Log(PP_LOGLEVEL_LOG, "Enumeration of window properties:");
    std::vector<pp::Var> props;
    window.GetAllPropertyNames(&props);
    for (size_t i = 0; i < props.size(); ++i)
      Log(PP_LOGLEVEL_LOG, props[i]);

    pp::VarPrivate location = window.GetProperty("location");
    pp::VarPrivate href = location.GetProperty("href");

    if (!fetcher_) {
      fetcher_ = new MyFetcher();
      fetcher_->Start(*this, href, this);
    }
  }

  void DidFetch(bool success, const std::string& data) {
    Log(PP_LOGLEVEL_LOG, "Downloaded location.href:");
    if (success) {
      Log(PP_LOGLEVEL_LOG, data);
    } else {
      Log(PP_LOGLEVEL_ERROR, "Failed to download.");
    }
    delete fetcher_;
    fetcher_ = NULL;
  }

  void ToggleCursor() {
    const PPB_CursorControl_Dev* cursor_control =
        reinterpret_cast<const PPB_CursorControl_Dev*>(
            pp::Module::Get()->GetBrowserInterface(
                PPB_CURSOR_CONTROL_DEV_INTERFACE));
    if (!cursor_control)
      return;

    if (showing_custom_cursor_) {
      cursor_control->SetCursor(pp_instance(), PP_CURSORTYPE_POINTER, 0, NULL);
    } else {
      pp::ImageData image_data(this, pp::ImageData::GetNativeImageDataFormat(),
                               pp::Size(50, 50), false);
      FillRect(&image_data, 0, 0, 50, 50,
               image_data.format() == PP_IMAGEDATAFORMAT_BGRA_PREMUL ?
                   0x80800000 : 0x80000080);
      pp::Point hot_spot(0, 0);
      cursor_control->SetCursor(pp_instance(), PP_CURSORTYPE_CUSTOM,
                                image_data.pp_resource(), &hot_spot.pp_point());
    }

    showing_custom_cursor_ = !showing_custom_cursor_;
  }

  pp::Var console_;
  pp::Graphics2D device_context_;

  double time_at_last_check_;

  MyFetcher* fetcher_;

  int width_;
  int height_;

  // Incremented for each flush we get.
  int animation_counter_;
  bool print_settings_valid_;
  PP_PrintSettings_Dev print_settings_;

  bool showing_custom_cursor_;
};

void FlushCallback(void* data, int32_t result) {
  static_cast<MyInstance*>(data)->OnFlush();
}

class MyModule : public pp::Module {
 public:
  MyModule() : pp::Module() {}
  virtual ~MyModule() {}

  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new MyInstance(instance);
  }
};

namespace pp {

// Factory function for your specialization of the Module object.
Module* CreateModule() {
  return new MyModule();
}

}  // namespace pp
