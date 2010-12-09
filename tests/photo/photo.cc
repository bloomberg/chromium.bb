/*
 * Copyright (c) 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that be
 * found in the LICENSE file.
 */

// Native Client Photo Darkroom demo
//   Uses SRPC (Simple Remote Procedure Call)
//   Uses low level NaCl multimedia system.
//   Runs in the browser.
//   Can not be run standalone.
//   Expects to live in native_client/tests/photo directory

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <nacl/nacl_av.h>
#include <nacl/nacl_srpc.h>
#include <pmmintrin.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/nacl_syscalls.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <xmmintrin.h>

extern "C" {
#include <jpeglib.h>
#include <jerror.h>
}

#include <cstdlib>

#include "fastmath.h"
#include "surface.h"

// global properties used to setup darkroom
static int g_window_width = 620;
static int g_window_height = 420;
static int g_working_surf_width = 0;
static int g_working_surf_height = 0;
static int g_border_size = 10;
static uint32_t g_default_border_color = MakeRGBA(96, 96, 96, 255);


// Darkroom class

class Darkroom {
 public:
  // The functions in this block should only be called
  // from the main application thread.
  void ReadJPEG(FILE *f);
  void DoZoom(bool enable, int mx, int my);
  void DoDrag(int xrel, int yrel);
  bool CheckForOpen();
  bool CheckForUpdate();
  void ApplyWorkingToProcessed();
  void TransformPhotoToGeo(float angle);
  void FitPhotoToWorking();
  void ZoomPhotoToWorking();
  void RenderWorkingToBrowser();
  void Draw();
  bool PollEvents();
  void SetBrowserSurface(Surface *surf) { browserSurf_ = surf; }
  void SetWorkingSurface(Surface *surf) { workingSurf_ = surf; }
  void SetProcessedSurface(Surface *surf) { processedSurf_ = surf; }
  void SetGeoSurface(Surface *surf) { geoSurf_ = surf; }
  void SetPhotoSurface(Surface *surf) { photoSurf_ = surf; }
  void SetTempSurface(Surface *surf) { tempSurf_ = surf; }
  explicit Darkroom();
  ~Darkroom();

  // The following Set* functions can be called from either the
  // SRPC service thread (when the GUI sliders are moved by the user)
  // or from the main application thread.
  void SetSaturation(float v) { SetSetting(v, &saturation_); }
  void SetContrast(float v) { SetSetting(v, &contrast_); }
  void SetBrightness(float v) { SetSetting(v, &brightness_); }
  void SetFill(float v) { SetSetting(v, &fill_); }
  void SetBlackPoint(float v) { SetSetting(v, &blackPoint_); }
  void SetTemperature(float v) { SetSetting(v, &temperature_); }
  void SetShadowsHue(float v) { SetSetting(v, &shadowsHue_); }
  void SetShadowsSaturation(float v) { SetSetting(v, &shadowsSaturation_); }
  void SetHighlightsHue(float v) { SetSetting(v, &highlightsHue_); }
  void SetHighlightsSaturation(float v)
      { SetSetting(v, &highlightsSaturation_); }
  void SetSplitPoint(float v) { SetSetting(v, &splitPoint_); }
  void SetAngle(float v) { SetSetting(v, &angle_, &angleChanged_, true); }
  void SetFineAngle(float v)
      { SetSetting(v, &fineAngle_, &angleChanged_, true); }
  void SetLoadFilename(char *filename);
  void TriggerUpdate() {
    pthread_mutex_lock(&mutex_);
    update_ = true;
    pthread_mutex_unlock(&mutex_);
  }

 private:
  // collection of surfaces representing various stages of the
  // photo processing pipeline.
  Surface *browserSurf_;
  Surface *tempSurf_;
  Surface *workingSurf_;
  Surface *processedSurf_;
  Surface *geoSurf_;
  Surface *photoSurf_;

  // thread safe Get/Set functions
  void SetSetting(float v, float *setting,
                  bool *opt = NULL, bool opt_val = false) {
    pthread_mutex_lock(&mutex_);
    *setting = v;
    update_ = true;
    if (NULL != opt) {
      *opt = opt_val;
    }
    pthread_mutex_unlock(&mutex_);
  }
  float GetSetting(float *setting) {
    float r;
    pthread_mutex_lock(&mutex_);
    r = *setting;
    pthread_mutex_unlock(&mutex_);
    return r;
  }

  // general mutex used to protect settings
  pthread_mutex_t mutex_;

  // internal state of darkroom
  //    setting                    range
  float saturation_;            // 0 (b&w) .. 1 (identity) .. 2 (over)
  float contrast_;              // 0 (crush) .. 1 (identity) .. 2
  float brightness_;            // 0 (black) .. 1 (identity) .. 2
  float blackPoint_;            // 0 (identity) .. 1 (1 -> black)
  float fill_;                  // 0 (identity) .. 1 (1 -> white)
  float temperature_;           // -2000 .. 2000
  float shadowsHue_;            // 0 .. 1 (normalized hue spectrum)
  float shadowsSaturation_;     // 0 (none) .. 1 (full)
  float highlightsHue_;         // 0 .. 1 (normalized hue spectrum)
  float highlightsSaturation_;  // 0 (none) .. 1 (full)
  float splitPoint_;            // -1 (shadows) .. 0 (mid) .. 1 (highlights)
  float angle_;                 // -45 .. 45
  float fineAngle_;             // -2 .. 2
  bool update_;
  char filename_[FILENAME_MAX];
  bool loadfile_;
  bool zoom_;
  bool dragging_;
  bool angleChanged_;
  int zoomx_;
  int zoomy_;
};


// application has a single public instance
static Darkroom g_darkroom;


// Set a few vars which alert the main thread to load an image.
// Called from the SRPC service thread.
void Darkroom::SetLoadFilename(char *filename) {
  pthread_mutex_lock(&mutex_);
  strncpy(filename_, filename, FILENAME_MAX);
  loadfile_ = true;
  pthread_mutex_unlock(&mutex_);
}


// Called to switch between zoomed and not zoomed view.
// Called from the main application thread.
void Darkroom::DoZoom(bool enable, int mx, int my) {
  if ((photoSurf_->width_ < g_working_surf_width) &&
      (photoSurf_->height_ < g_working_surf_height))
    enable = false;
  if (enable) {
    // Note: would be nice to zoom in on area clicked instead of center
    zoomx_ = (g_working_surf_width - photoSurf_->width_) / 2;
    zoomy_ = (g_working_surf_height - photoSurf_->height_) / 2;
    ZoomPhotoToWorking();
  } else {
    FitPhotoToWorking();
  }
  zoom_ = enable;
  RenderWorkingToBrowser();
}


// Called when the user drags the zoomed view
// Called from the main application thread.
void Darkroom::DoDrag(int xrel, int yrel) {
  if (zoom_) {
    zoomx_ += xrel;
    zoomy_ += yrel;
    // do blit...
    ZoomPhotoToWorking();
    RenderWorkingToBrowser();
  }
}


// Apply the slider settings to the image.
// Called from the main application thread.
void Darkroom::ApplyWorkingToProcessed() {
  float saturation;
  float contrast;
  float brightness;
  float brightness_a;
  float brightness_b;
  float blackPoint;
  float fill;
  float temperature;
  float shadowsHue;
  float shadowsSaturation;
  float highlightsHue;
  float highlightsSaturation;
  float splitPoint;
  float oo255 = 1.0f / 255.0f;

  // grab a snapshot into local vars of current settings
  pthread_mutex_lock(&mutex_);
  saturation = saturation_;
  contrast = contrast_;
  brightness = brightness_;
  blackPoint = blackPoint_;
  fill = fill_;
  temperature = temperature_;
  shadowsHue = shadowsHue_;
  shadowsSaturation = shadowsSaturation_;
  highlightsHue = highlightsHue_;
  highlightsSaturation = highlightsSaturation_;
  splitPoint = splitPoint_;
  pthread_mutex_unlock(&mutex_);

  // do some adjustments
  fill *= 0.2f;
  brightness = (brightness - 1.0f) * 0.75f + 1.0f;
  if (brightness < 1.0f) {
    brightness_a = brightness;
    brightness_b = 0.0f;
  } else {
    brightness_b = brightness - 1.0f;
    brightness_a = 1.0f - brightness_b;
  }
  contrast = contrast * 0.5f;
  contrast = (contrast - 0.5f) * 0.75f + 0.5f;
  temperature = (temperature / 2000.0f) * 0.1f;
  if (temperature > 0.0f) temperature *= 2.0f;
  splitPoint = ((splitPoint + 1.0f) * 0.5f);

  // apply to pixels
  processedSurf_->Realloc(workingSurf_->width_, workingSurf_->height_, true);
  int sz = processedSurf_->width_ * processedSurf_->height_;
  for (int j = 0; j < sz; j++) {
    uint32_t color = workingSurf_->pixels_[j];
    float r = ExtractR(color) * oo255;
    float g = ExtractG(color) * oo255;
    float b = ExtractB(color) * oo255;
    // convert RGB to YIQ
    // this is a less than ideal colorspace;
    // HSL would probably be better, but more expensive
    float y = 0.299f * r + 0.587f * g + 0.114f * b;
    float i = 0.596f * r - 0.275f * g - 0.321f * b;
    float q = 0.212f * r - 0.523f * g + 0.311f * b;
    i = i + temperature;
    q = q - temperature;
    i = i * saturation;
    q = q * saturation;
    y = (1.0f + blackPoint) * y - blackPoint;
    y = y + fill;
    y = y * brightness_a + brightness_b;
    y = FastGain(contrast, Clamp(y));
    if (y < splitPoint) {
      q = q + (shadowsHue * shadowsSaturation) * (splitPoint - y);
    } else {
      i = i + (highlightsHue * highlightsSaturation) * (y - splitPoint);
    }
    // convert back to RGB for display
    r = y + 0.956f * i + 0.621f * q;
    g = y - 0.272f * i - 0.647f * q;
    b = y - 1.105f * i + 1.702f * q;
    r = Clamp(r);
    g = Clamp(g);
    b = Clamp(b);
    int ir = static_cast<int>(r * 255.0f);
    int ig = static_cast<int>(g * 255.0f);
    int ib = static_cast<int>(b * 255.0f);
    uint32_t dst_color;
    dst_color = MakeRGBA(ir, ig, ib, 255);
    processedSurf_->pixels_[j] = dst_color;
  }
}


// Apply rotation value
// Called from the main application thread.
void Darkroom::TransformPhotoToGeo(float angle) {
  geoSurf_->Realloc(photoSurf_->width_, photoSurf_->height_, true);
  geoSurf_->Clear(g_default_border_color);
  tempSurf_->SetBorderColor(g_default_border_color);
  geoSurf_->Rotate(angle, photoSurf_, tempSurf_);
}


// Fit the photo to the working surface
// Called from the main application thread.
void Darkroom::FitPhotoToWorking() {
  TransformPhotoToGeo(angle_ + fineAngle_);
  // convert photo -> working surface
  if ((geoSurf_->width_ <= g_working_surf_width) &&
      (geoSurf_->height_ <= g_working_surf_height)) {
    // photo is smaller than working surface, so blit 1:1
    workingSurf_->Realloc(geoSurf_->width_, geoSurf_->height_, true);
    workingSurf_->Blit(0, 0, geoSurf_);
  } else {
    // photo is larger than working surface, so needs to be scaled
    float scale;
    if (geoSurf_->width_ > geoSurf_->height_) {
      scale = static_cast<float>(g_working_surf_width) /
              static_cast<float>(geoSurf_->width_);
    } else {
      scale = static_cast<float>(g_working_surf_height) /
              static_cast<float>(geoSurf_->height_);
    }
    if (geoSurf_->width_ * scale > g_working_surf_width) {
      scale *= g_working_surf_width / (geoSurf_->width_ * scale);
    }
    if (geoSurf_->height_ * scale > g_working_surf_height) {
      scale *= g_working_surf_height / (geoSurf_->height_ * scale);
    }
    workingSurf_->RescaleFrom(geoSurf_, scale, scale, tempSurf_);
  }
}


// Zoom (and clip) to the photo surface
// Called from the main application thread.
// Note: at some point, need to implement support for rotation while zoomed
void Darkroom::ZoomPhotoToWorking() {
  workingSurf_->Realloc(g_working_surf_width, g_working_surf_height, true);
  workingSurf_->Clear(g_default_border_color);
  workingSurf_->Blit(zoomx_, zoomy_, photoSurf_);
}


// Called from the main application thread.
void Darkroom::RenderWorkingToBrowser() {
  // blit the working surface to the browser surface, centered
  int offsetx = (browserSurf_->width_ - workingSurf_->width_) / 2;
  int offsety = (browserSurf_->height_ - workingSurf_->height_) / 2;
  browserSurf_->Clear(g_default_border_color);
  ApplyWorkingToProcessed();
  browserSurf_->Blit(offsetx, offsety, processedSurf_);
  pthread_mutex_lock(&mutex_);
  update_ = true;
  pthread_mutex_unlock(&mutex_);
}


// Reads a jpeg via libjpeg
// Triggers an update to show new image.
// Called from the main application thread.
void Darkroom::ReadJPEG(FILE *f) {
  struct jpeg_decompress_struct cinfo;
  struct jpeg_error_mgr jerr;
  uint8_t *scanline, *scantmp;
  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_decompress(&cinfo);
  jpeg_stdio_src(&cinfo, f);
  jpeg_read_header(&cinfo, TRUE);
  jpeg_start_decompress(&cinfo);

  photoSurf_->Realloc(cinfo.output_width, cinfo.output_height, false);
  scanline = reinterpret_cast<uint8_t *>(alloca(cinfo.output_width * 3));

  for (unsigned int i = 0; i < cinfo.output_height; i++) {
    scantmp = scanline;
    jpeg_read_scanlines(&cinfo, &scantmp, 1);
    // copy scanline into surface...
    for (unsigned int j = 0, p = 0; j < cinfo.output_width; j++) {
      photoSurf_->PutPixelNoClip(j, i,
          MakeRGBA(scanline[p], scanline[p+1], scanline[p+2], 0xFF));
      p += 3;
    }
  }
  DoZoom(false, 0, 0);
  FitPhotoToWorking();
  RenderWorkingToBrowser();
}


// CheckForOpen() checks to see if the user pressed the open button (the open
// button invokes the SRPC function on a different thread)
// Called from the main application thread.
bool Darkroom::CheckForOpen() {
  char filename[FILENAME_MAX];
  bool load;
  // the srpc listing thread will set loadfile_ & filename_ via
  // Darkroom::SetLoadFilename()
  pthread_mutex_lock(&mutex_);
  load = loadfile_;
  loadfile_ = false;
  strncpy(filename, filename_, FILENAME_MAX);
  pthread_mutex_unlock(&mutex_);
  if (load) {
    browserSurf_->Clear(g_default_border_color);
    // load the file here...
    printf("Loading file....\n");
    FILE *f;
    f = fopen(filename, "r");
    if (f) {
      ReadJPEG(f);
      fclose(f);
    }
    TriggerUpdate();
  }
  return load;
}


// CheckForUpdate checks to see if the photo output pane needs updating
// Called from the main application thread.
bool Darkroom::CheckForUpdate() {
  // update the browser image...
  bool update;
  bool angleChanged;
  float angle;

  pthread_mutex_lock(&mutex_);
  update = update_;
  angleChanged = angleChanged_;
  angle = angle_;
  pthread_mutex_unlock(&mutex_);

  if (update) {
    if (angleChanged) {
      DoZoom(false, 0, 0);
    } else {
      RenderWorkingToBrowser();
    }
  }

  pthread_mutex_lock(&mutex_);
  update_ = false;
  angleChanged_ = false;
  pthread_mutex_unlock(&mutex_);

  return update;
}


// Copies sw rendered darkroom image to screen
// Called from the main application thread.
void Darkroom::Draw() {
  int r;
  r = nacl_video_update(browserSurf_->pixels_);
  if (-1 == r) {
    printf("nacl_video_update() returned %d\n", errno);
  }
}


// Polls events and services them.  These are Native Client events generated
// by the user within the photo region.  Called by the main application thread.
bool Darkroom::PollEvents() {
  NaClMultimediaEvent event;
  bool drag = false;
  int drag_x = 0;
  int drag_y = 0;
  // while events are available...
  while (0 == nacl_video_poll_event(&event)) {
    switch (event.type) {
      case NACL_EVENT_MOUSE_BUTTON_DOWN:
        if (event.button.button == 3) {
          DoZoom(zoom_ ^ true, event.button.x, event.button.y);
        }
        if (event.button.button == 1) {
          dragging_ = true;
        }
        break;
      case NACL_EVENT_MOUSE_BUTTON_UP:
        if (event.button.button == 1) {
          dragging_ = false;
        }
        break;
      case NACL_EVENT_MOUSE_MOTION:
        if (dragging_) {
          drag = true;
          drag_x += event.motion.xrel;
          drag_y += event.motion.yrel;
        }
        break;
      case NACL_EVENT_QUIT:
        return false;
    }
  }

  // service all pending events above, then perform dragging only once
  if (drag) {
    DoDrag(drag_x, drag_y);
  }

  return true;
}


// Sets up and initializes Darkroom data structures.  Seeds with random values.
// Called from the main application thread.
Darkroom::Darkroom()
    : saturation_(1.0f),
      contrast_(1.0f),
      brightness_(1.0f),
      blackPoint_(0.0f),
      fill_(0.0f),
      temperature_(0.0f),
      shadowsHue_(0.0f),
      shadowsSaturation_(0.0f),
      highlightsHue_(0.0f),
      highlightsSaturation_(0.0f),
      splitPoint_(0.0f),
      angle_(0.0f),
      update_(true),
      loadfile_(false),
      zoom_(false),
      dragging_(false),
      angleChanged_(false),
      zoomx_(0),
      zoomy_(0) {
  pthread_mutex_init(&mutex_, NULL);
  strncpy(filename_, "", FILENAME_MAX);
  browserSurf_ = NULL;
  workingSurf_ = NULL;
  photoSurf_ = NULL;
}


// Frees up resources.
// Called from the main application thread.
Darkroom::~Darkroom() {
  pthread_mutex_destroy(&mutex_);
}


// SRPC function OpenPhoto
// invoked via Javascript, called from SRPC service thread.
void OpenPhoto(NaClSrpcRpc* rpc,
               NaClSrpcArg** in_args,
               NaClSrpcArg** out_args,
               NaClSrpcClosure* done) {
  char* filename = in_args[0]->u.sval;
  g_darkroom.SetLoadFilename(filename);
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}


// SRPC function UpdateSaturation
// invoked via Javascript, called from SRPC service thread.
void UpdateSaturation(NaClSrpcRpc* rpc,
                      NaClSrpcArg** in_args,
                      NaClSrpcArg** out_args,
                      NaClSrpcClosure* done) {
  int slider = in_args[0]->u.ival;
  float saturation = slider / 100.0f;
  g_darkroom.SetSaturation(saturation);
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}


// SRPC function UpdateContrast
// invoked via Javascript, called from SRPC service thread.
void UpdateContrast(NaClSrpcRpc* rpc,
                    NaClSrpcArg** in_args,
                    NaClSrpcArg** out_args,
                    NaClSrpcClosure* done) {
  int slider = in_args[0]->u.ival;
  float contrast = slider / 100.0f;
  g_darkroom.SetContrast(contrast);
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}


// SRPC function UpdateBrightness
// invoked via Javascript, called from SRPC service thread.
void UpdateBrightness(NaClSrpcRpc* rpc,
                      NaClSrpcArg** in_args,
                      NaClSrpcArg** out_args,
                      NaClSrpcClosure* done) {
  int slider = in_args[0]->u.ival;
  float brightness = slider / 100.0f;
  g_darkroom.SetBrightness(brightness);
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}


// SRPC function UpdateTemperature
// invoked via Javascript, called from SRPC service thread.
void UpdateTemperature(NaClSrpcRpc* rpc,
                       NaClSrpcArg** in_args,
                       NaClSrpcArg** out_args,
                       NaClSrpcClosure* done) {
  int slider = in_args[0]->u.ival;
  float temperature = static_cast<float>(slider);
  g_darkroom.SetTemperature(temperature);
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}


// SRPC function UpdateBlackPoint
// invoked via Javascript, called from SRPC service thread.
void UpdateBlackPoint(NaClSrpcRpc* rpc,
                    NaClSrpcArg** in_args,
                    NaClSrpcArg** out_args,
                    NaClSrpcClosure* done) {
  int slider = in_args[0]->u.ival;
  float blackPoint = slider / 100.0f;
  g_darkroom.SetBlackPoint(blackPoint);
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}


// SRPC function UpdateFill
// invoked via Javascript, called from SRPC service thread.
void UpdateFill(NaClSrpcRpc* rpc,
                NaClSrpcArg** in_args,
                NaClSrpcArg** out_args,
                NaClSrpcClosure* done) {
  int slider = in_args[0]->u.ival;
  float fill = slider / 100.0f;
  g_darkroom.SetFill(fill);
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}


// SRPC function UpdateShadowsHue
// invoked via Javascript, called from SRPC service thread.
void UpdateShadowsHue(NaClSrpcRpc* rpc,
                      NaClSrpcArg** in_args,
                      NaClSrpcArg** out_args,
                      NaClSrpcClosure* done) {
  int slider = in_args[0]->u.ival;
  float shadowsHue = slider / 1000.0f;
  g_darkroom.SetShadowsHue(shadowsHue);
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}


// SRPC function UpdateShdowsSaturation
// invoked via Javascript, called from SRPC service thread.
void UpdateShadowsSaturation(NaClSrpcRpc* rpc,
                             NaClSrpcArg** in_args,
                             NaClSrpcArg** out_args,
                             NaClSrpcClosure* done) {
  int slider = in_args[0]->u.ival;
  float shadowsSaturation = slider / 100.0f;
  g_darkroom.SetShadowsSaturation(shadowsSaturation);
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}


// SRPC function UpdateHighlightsHue
// invoked via Javascript, called from SRPC service thread.
void UpdateHighlightsHue(NaClSrpcRpc* rpc,
                         NaClSrpcArg** in_args,
                         NaClSrpcArg** out_args,
                         NaClSrpcClosure* done) {
  int slider = in_args[0]->u.ival;
  float highlightsHue = slider / 1000.0f;
  g_darkroom.SetHighlightsHue(highlightsHue);
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}


// SRPC function UpdateHighlightsSaturation
// invoked via Javascript, called from SRPC service thread.
void UpdateHighlightsSaturation(NaClSrpcRpc* rpc,
                                NaClSrpcArg** in_args,
                                NaClSrpcArg** out_args,
                                NaClSrpcClosure* done) {
  int slider = in_args[0]->u.ival;
  float highlightsSaturation = slider / 100.0f;
  g_darkroom.SetHighlightsSaturation(highlightsSaturation);
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}


// SRPC UpdateSplitPoint
// invoked via Javascript, called from SRPC service thread.
void UpdateSplitPoint(NaClSrpcRpc* rpc,
                      NaClSrpcArg** in_args,
                      NaClSrpcArg** out_args,
                      NaClSrpcClosure* done) {
  int slider = in_args[0]->u.ival;
  float splitPoint = slider / 100.0f;
  g_darkroom.SetSplitPoint(splitPoint);
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}


// SRPC UpdateAngle
// invoked via Javascript, called from SRPC service thread.
void UpdateAngle(NaClSrpcRpc* rpc,
                 NaClSrpcArg** in_args,
                 NaClSrpcArg** out_args,
                 NaClSrpcClosure* done) {
  int slider = in_args[0]->u.ival;
  float angle = static_cast<float>(slider);
  g_darkroom.SetAngle(angle);
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}


// SRPC UpdateFineAngle
// invoked via Javascript, called from SRPC service thread.
void UpdateFineAngle(NaClSrpcRpc* rpc,
                     NaClSrpcArg** in_args,
                     NaClSrpcArg** out_args,
                     NaClSrpcClosure* done) {
  int slider = in_args[0]->u.ival;
  float fine = static_cast<float>(slider) / 100.0f;
  g_darkroom.SetFineAngle(fine);
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}


// register SRPC methods
// These functions are called by the SRPC service thread via Javascript.
NACL_SRPC_METHOD("OpenPhoto:s:", OpenPhoto);
NACL_SRPC_METHOD("UpdateSaturation:i:", UpdateSaturation);
NACL_SRPC_METHOD("UpdateContrast:i:", UpdateContrast);
NACL_SRPC_METHOD("UpdateBrightness:i:", UpdateBrightness);
NACL_SRPC_METHOD("UpdateTemperature:i:", UpdateTemperature);
NACL_SRPC_METHOD("UpdateBlackPoint:i:", UpdateBlackPoint);
NACL_SRPC_METHOD("UpdateFill:i:", UpdateFill);
NACL_SRPC_METHOD("UpdateShadowsHue:i:", UpdateShadowsHue);
NACL_SRPC_METHOD("UpdateShadowsSaturation:i:", UpdateShadowsSaturation);
NACL_SRPC_METHOD("UpdateHighlightsHue:i:", UpdateHighlightsHue);
NACL_SRPC_METHOD("UpdateHighlightsSaturation:i:", UpdateHighlightsSaturation);
NACL_SRPC_METHOD("UpdateSplitPoint:i:", UpdateSplitPoint);
NACL_SRPC_METHOD("UpdateAngle:i:", UpdateAngle);
NACL_SRPC_METHOD("UpdateFineAngle:i:", UpdateFineAngle);



// Runs the darmroom demo.
// Called from the main application thread.
void RunDemo() {
  // these surfaces will live for the duration of this function
  Surface browserSurface(g_window_width, g_window_height,
      g_default_border_color);
  Surface workingSurface(g_working_surf_width, g_working_surf_height,
      g_default_border_color);
  Surface processedSurface(g_working_surf_width, g_working_surf_height,
      g_default_border_color);
  Surface geoSurface(g_working_surf_width, g_working_surf_height,
      g_default_border_color);
  Surface photoSurface(g_working_surf_width, g_working_surf_height,
      g_default_border_color);
  Surface tempSurface(browserSurface.width_, browserSurface.height_,
      g_default_border_color);

  // set darkroom surfaces
  g_darkroom.SetBrowserSurface(&browserSurface);
  g_darkroom.SetWorkingSurface(&workingSurface);
  g_darkroom.SetProcessedSurface(&processedSurface);
  g_darkroom.SetGeoSurface(&geoSurface);
  g_darkroom.SetPhotoSurface(&photoSurface);
  g_darkroom.SetTempSurface(&tempSurface);

  // trigger initial update
  g_darkroom.TriggerUpdate();

  // main demo loop
  while (true) {
    // did a file open request occur on SRPC?
    g_darkroom.CheckForOpen();
    // poll events from nacl region
    if (!g_darkroom.PollEvents())
      break;
    // did something change that requires an update?
    bool update = g_darkroom.CheckForUpdate();
    if (update) {
      g_darkroom.Draw();
    } else {
      // attempt to play nice and conserve resources
      struct timespec q;
      q.tv_sec = 0;
      q.tv_nsec = 16666666;  // 60fps
      nanosleep(&q, &q);
    }
  }

  // reset surfaces to NULL
  g_darkroom.SetBrowserSurface(NULL);
  g_darkroom.SetWorkingSurface(NULL);
  g_darkroom.SetProcessedSurface(NULL);
  g_darkroom.SetGeoSurface(NULL);
  g_darkroom.SetPhotoSurface(NULL);
  g_darkroom.SetTempSurface(NULL);
}


// Initializes a window buffer.
// Called from the main application thread.
void Initialize() {
  int r;
  int width;
  int height;
  r = nacl_multimedia_init(NACL_SUBSYSTEM_VIDEO | NACL_SUBSYSTEM_EMBED);
  if (-1 == r) {
    printf("Multimedia system failed to initialize!  errno: %d\n", errno);
    exit(-1);
  }
  // if this call succeeds, use width & height from embedded html
  r = nacl_multimedia_get_embed_size(&width, &height);
  if (0 == r) {
    g_window_width = width;
    g_window_height = height;
  } else {
    printf("Sorry, this Native Client demo can only run in the browser!\n");
    nacl_multimedia_shutdown();
    exit(-1);
  }
  r = nacl_video_init(g_window_width, g_window_height);
  if (-1 == r) {
    printf("Video subsystem failed to initialize!  errno; %d\n", errno);
    exit(-1);
  }
  g_working_surf_width = g_window_width - g_border_size * 2;
  g_working_surf_height = g_window_height - g_border_size * 2;
}


// Frees window buffer.
// Called from the main application thread.
void Shutdown() {
  nacl_video_shutdown();
  nacl_multimedia_shutdown();
}


// Initializes SRPC service, acquires video output, runs demo & shuts down.
// Main application thread entry point.
int main(int argc, char **argv) {
  if (!NaClSrpcModuleInit()) {
    return 1;
  }
  if (!NaClSrpcAcceptClientOnThread(__kNaClSrpcHandlers)) {
    return 1;
  }
  // initialize nacl video
  Initialize();
  // run the photo demo loop
  RunDemo();
  // shutdown nacl video
  Shutdown();
  NaClSrpcModuleFini();
  return 0;
}
