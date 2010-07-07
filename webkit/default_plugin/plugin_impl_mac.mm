// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/default_plugin/plugin_impl_mac.h"

#import <Cocoa/Cocoa.h>

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "googleurl/src/gurl.h"
#include "grit/webkit_strings.h"
#include "unicode/locid.h"
#include "webkit/default_plugin/default_plugin_shared.h"
#include "webkit/default_plugin/plugin_main.h"
#include "webkit/glue/webkit_glue.h"

// TODO(thakis): Most methods in this class are stubbed out and need to be
// implemented.

PluginInstallerImpl::PluginInstallerImpl(int16 mode) {
}

PluginInstallerImpl::~PluginInstallerImpl() {
}

bool PluginInstallerImpl::Initialize(void* module_handle, NPP instance,
                                     NPMIMEType mime_type, int16 argc,
                                     char* argn[], char* argv[]) {
  DLOG(INFO) << __FUNCTION__ << " MIME Type : " << mime_type;
  DCHECK(instance != NULL);

  if (mime_type == NULL || strlen(mime_type) == 0) {
    DLOG(WARNING) << __FUNCTION__ << " Invalid parameters passed in";
    NOTREACHED();
    return false;
  }

  instance_ = instance;
  mime_type_ = mime_type;

  return true;
}

bool PluginInstallerImpl::NPP_SetWindow(NPWindow* window_info) {
  width_ = window_info->width;
  height_ = window_info->height;
  return true;
}

void PluginInstallerImpl::Shutdown() {
}

void PluginInstallerImpl::NewStream(NPStream* stream) {
  plugin_install_stream_ = stream;
}

void PluginInstallerImpl::DestroyStream(NPStream* stream, NPError reason) {
  if (stream == plugin_install_stream_)
    plugin_install_stream_ = NULL;
}

bool PluginInstallerImpl::WriteReady(NPStream* stream) {
  bool ready_to_accept_data = false;
  return ready_to_accept_data;
}

int32 PluginInstallerImpl::Write(NPStream* stream, int32 offset,
                                 int32 buffer_length, void* buffer) {
  return true;
}

void PluginInstallerImpl::ClearDisplay() {
}

void PluginInstallerImpl::RefreshDisplay() {
}

bool PluginInstallerImpl::CreateToolTip() {
  return true;
}

void PluginInstallerImpl::UpdateToolTip() {
}

void PluginInstallerImpl::DisplayAvailablePluginStatus() {
}

void PluginInstallerImpl::DisplayStatus(int message_resource_id) {
}

void PluginInstallerImpl::DisplayPluginDownloadFailedStatus() {
}

void PluginInstallerImpl::URLNotify(const char* url, NPReason reason) {
}

int16 PluginInstallerImpl::NPP_HandleEvent(void* event) {
  NPCocoaEvent* npp_event = static_cast<NPCocoaEvent*>(event);

  if (npp_event->type == NPCocoaEventDrawRect) {
    CGContextRef context = npp_event->data.draw.context;
    CGRect rect = CGRectMake(npp_event->data.draw.x,
                             npp_event->data.draw.y,
                             npp_event->data.draw.width,
                             npp_event->data.draw.height);
    return OnDrawRect(context, rect);
  }
  return 0;
}

std::wstring PluginInstallerImpl::ReplaceStringForPossibleEmptyReplacement(
    int message_id_with_placeholders,
    int messsage_id_without_placeholders,
    const std::wstring& replacement_string) {
  return L"";
}

void PluginInstallerImpl::DownloadPlugin() {
}

void PluginInstallerImpl::DownloadCancelled() {
  DisplayAvailablePluginStatus();
}

int16 PluginInstallerImpl::OnDrawRect(CGContextRef context, CGRect dirty_rect) {
  const NSString* text = @"Missing Plug-in";
  const float kTextMarginX = 6;
  const float kTextMarginY = 1;
  NSSize bounds = NSMakeSize(width_, height_);

  [NSGraphicsContext saveGraphicsState];
  NSGraphicsContext* ns_context = [NSGraphicsContext
      graphicsContextWithGraphicsPort:context flipped:YES];
  [NSGraphicsContext setCurrentContext:ns_context];

  NSShadow* shadow = [[[NSShadow alloc] init] autorelease];
  [shadow setShadowColor:[NSColor colorWithDeviceWhite:0.0 alpha:0.5]];
  [shadow setShadowOffset:NSMakeSize(0, -1)];
  NSFont* font = [NSFont boldSystemFontOfSize:[NSFont smallSystemFontSize]];
  NSDictionary* attributes = [NSDictionary dictionaryWithObjectsAndKeys:
      font, NSFontAttributeName,
      [NSColor whiteColor], NSForegroundColorAttributeName,
      shadow, NSShadowAttributeName,
      nil];

  NSSize text_size = [text sizeWithAttributes:attributes];
  NSRect text_rect;
  text_rect.size.width = text_size.width + 2*kTextMarginX;
  text_rect.size.height = text_size.height + 2*kTextMarginY;
  text_rect.origin.x = (bounds.width - NSWidth(text_rect))/2;
  text_rect.origin.y = (bounds.height - NSHeight(text_rect))/2;

  [[NSColor colorWithCalibratedWhite:0.52 alpha:1.0] setFill];
  [[NSBezierPath bezierPathWithRoundedRect:text_rect
                                   xRadius:NSHeight(text_rect)/2
                                   yRadius:NSHeight(text_rect)/2] fill];

  NSPoint label_point = NSMakePoint(
      roundf(text_rect.origin.x + (text_rect.size.width - text_size.width)/2),
      roundf(text_rect.origin.y + (text_rect.size.height - text_size.height)/2));
  [text drawAtPoint:label_point withAttributes:attributes];
  
  [NSGraphicsContext restoreGraphicsState];
  return 1;
}


void PluginInstallerImpl::ShowInstallDialog() {
}

void PluginInstallerImpl::NotifyPluginStatus(int status) {
  default_plugin::g_browser->getvalue(
      instance_,
      static_cast<NPNVariable>(
          default_plugin::kMissingPluginStatusStart + status),
      NULL);
}
