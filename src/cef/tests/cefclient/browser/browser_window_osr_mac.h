// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_BROWSER_BROWSER_WINDOW_OSR_MAC_H_
#define CEF_TESTS_CEFCLIENT_BROWSER_BROWSER_WINDOW_OSR_MAC_H_
#pragma once

#include "tests/cefclient/browser/browser_window.h"
#include "tests/cefclient/browser/client_handler_osr.h"
#include "tests/cefclient/browser/osr_renderer.h"
#include "tests/cefclient/browser/text_input_client_osr_mac.h"

namespace client {

class BrowserWindowOsrMacImpl;

// Represents a native child window hosting a single off-screen browser
// instance. The methods of this class must be called on the main thread unless
// otherwise indicated.
class BrowserWindowOsrMac : public BrowserWindow,
                            public ClientHandlerOsr::OsrDelegate {
 public:
  // Constructor may be called on any thread.
  // |delegate| must outlive this object.
  BrowserWindowOsrMac(BrowserWindow::Delegate* delegate,
                      const std::string& startup_url,
                      const OsrRendererSettings& settings);
  ~BrowserWindowOsrMac();

  // BrowserWindow methods.
  void CreateBrowser(ClientWindowHandle parent_handle,
                     const CefRect& rect,
                     const CefBrowserSettings& settings,
                     CefRefPtr<CefDictionaryValue> extra_info,
                     CefRefPtr<CefRequestContext> request_context) OVERRIDE;
  void GetPopupConfig(CefWindowHandle temp_handle,
                      CefWindowInfo& windowInfo,
                      CefRefPtr<CefClient>& client,
                      CefBrowserSettings& settings) OVERRIDE;
  void ShowPopup(ClientWindowHandle parent_handle,
                 int x,
                 int y,
                 size_t width,
                 size_t height) OVERRIDE;
  void Show() OVERRIDE;
  void Hide() OVERRIDE;
  void SetBounds(int x, int y, size_t width, size_t height) OVERRIDE;
  void SetFocus(bool focus) OVERRIDE;
  void SetDeviceScaleFactor(float device_scale_factor) OVERRIDE;
  float GetDeviceScaleFactor() const OVERRIDE;
  ClientWindowHandle GetWindowHandle() const OVERRIDE;

  // ClientHandlerOsr::OsrDelegate methods.
  void OnAfterCreated(CefRefPtr<CefBrowser> browser) OVERRIDE;
  void OnBeforeClose(CefRefPtr<CefBrowser> browser) OVERRIDE;
  bool GetRootScreenRect(CefRefPtr<CefBrowser> browser, CefRect& rect) OVERRIDE;
  void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) OVERRIDE;
  bool GetScreenPoint(CefRefPtr<CefBrowser> browser,
                      int viewX,
                      int viewY,
                      int& screenX,
                      int& screenY) OVERRIDE;
  bool GetScreenInfo(CefRefPtr<CefBrowser> browser,
                     CefScreenInfo& screen_info) OVERRIDE;
  void OnPopupShow(CefRefPtr<CefBrowser> browser, bool show) OVERRIDE;
  void OnPopupSize(CefRefPtr<CefBrowser> browser, const CefRect& rect) OVERRIDE;
  void OnPaint(CefRefPtr<CefBrowser> browser,
               CefRenderHandler::PaintElementType type,
               const CefRenderHandler::RectList& dirtyRects,
               const void* buffer,
               int width,
               int height) OVERRIDE;
  void OnCursorChange(CefRefPtr<CefBrowser> browser,
                      CefCursorHandle cursor,
                      cef_cursor_type_t type,
                      const CefCursorInfo& custom_cursor_info) OVERRIDE;
  bool StartDragging(CefRefPtr<CefBrowser> browser,
                     CefRefPtr<CefDragData> drag_data,
                     CefRenderHandler::DragOperationsMask allowed_ops,
                     int x,
                     int y) OVERRIDE;
  void UpdateDragCursor(CefRefPtr<CefBrowser> browser,
                        CefRenderHandler::DragOperation operation) OVERRIDE;
  void OnImeCompositionRangeChanged(
      CefRefPtr<CefBrowser> browser,
      const CefRange& selection_range,
      const CefRenderHandler::RectList& character_bounds) OVERRIDE;

  void UpdateAccessibilityTree(CefRefPtr<CefValue> value) OVERRIDE;
  void UpdateAccessibilityLocation(CefRefPtr<CefValue> value) OVERRIDE;

 private:
  scoped_ptr<BrowserWindowOsrMacImpl> impl_;

  DISALLOW_COPY_AND_ASSIGN(BrowserWindowOsrMac);

  friend class BrowserWindowOsrMacImpl;
};

}  // namespace client

#endif  // CEF_TESTS_CEFCLIENT_BROWSER_BROWSER_WINDOW_OSR_MAC_H_
