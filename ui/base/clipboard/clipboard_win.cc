// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Many of these functions are based on those found in
// webkit/port/platform/PasteboardWin.cpp

#include "ui/base/clipboard/clipboard.h"

#include <shlobj.h>
#include <shellapi.h>

#include "base/file_path.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/shared_memory.h"
#include "base/string_util.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "ui/base/clipboard/clipboard_util_win.h"
#include "ui/gfx/size.h"

namespace ui {

namespace {

// A scoper to manage acquiring and automatically releasing the clipboard.
class ScopedClipboard {
 public:
  ScopedClipboard() : opened_(false) { }

  ~ScopedClipboard() {
    if (opened_)
      Release();
  }

  bool Acquire(HWND owner) {
    const int kMaxAttemptsToOpenClipboard = 5;

    if (opened_) {
      NOTREACHED();
      return false;
    }

    // Attempt to open the clipboard, which will acquire the Windows clipboard
    // lock.  This may fail if another process currently holds this lock.
    // We're willing to try a few times in the hopes of acquiring it.
    //
    // This turns out to be an issue when using remote desktop because the
    // rdpclip.exe process likes to read what we've written to the clipboard and
    // send it to the RDP client.  If we open and close the clipboard in quick
    // succession, we might be trying to open it while rdpclip.exe has it open,
    // See Bug 815425.
    //
    // In fact, we believe we'll only spin this loop over remote desktop.  In
    // normal situations, the user is initiating clipboard operations and there
    // shouldn't be contention.

    for (int attempts = 0; attempts < kMaxAttemptsToOpenClipboard; ++attempts) {
      // If we didn't manage to open the clipboard, sleep a bit and be hopeful.
      if (attempts != 0)
        ::Sleep(5);

      if (::OpenClipboard(owner)) {
        opened_ = true;
        return true;
      }
    }

    // We failed to acquire the clipboard.
    return false;
  }

  void Release() {
    if (opened_) {
      ::CloseClipboard();
      opened_ = false;
    } else {
      NOTREACHED();
    }
  }

 private:
  bool opened_;
};

LRESULT CALLBACK ClipboardOwnerWndProc(HWND hwnd,
                                       UINT message,
                                       WPARAM wparam,
                                       LPARAM lparam) {
  LRESULT lresult = 0;

  switch (message) {
  case WM_RENDERFORMAT:
    // This message comes when SetClipboardData was sent a null data handle
    // and now it's come time to put the data on the clipboard.
    // We always set data, so there isn't a need to actually do anything here.
    break;
  case WM_RENDERALLFORMATS:
    // This message comes when SetClipboardData was sent a null data handle
    // and now this application is about to quit, so it must put data on
    // the clipboard before it exits.
    // We always set data, so there isn't a need to actually do anything here.
    break;
  case WM_DRAWCLIPBOARD:
    break;
  case WM_DESTROY:
    break;
  case WM_CHANGECBCHAIN:
    break;
  default:
    lresult = DefWindowProc(hwnd, message, wparam, lparam);
    break;
  }
  return lresult;
}

template <typename charT>
HGLOBAL CreateGlobalData(const std::basic_string<charT>& str) {
  HGLOBAL data =
    ::GlobalAlloc(GMEM_MOVEABLE, ((str.size() + 1) * sizeof(charT)));
  if (data) {
    charT* raw_data = static_cast<charT*>(::GlobalLock(data));
    memcpy(raw_data, str.data(), str.size() * sizeof(charT));
    raw_data[str.size()] = '\0';
    ::GlobalUnlock(data);
  }
  return data;
};

}  // namespace

Clipboard::Clipboard() : create_window_(false) {
  if (MessageLoop::current()->type() == MessageLoop::TYPE_UI) {
    // Make a dummy HWND to be the clipboard's owner.
    WNDCLASSEX wcex = {0};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.lpfnWndProc = ClipboardOwnerWndProc;
    wcex.hInstance = GetModuleHandle(NULL);
    wcex.lpszClassName = L"ClipboardOwnerWindowClass";
    ::RegisterClassEx(&wcex);
    create_window_ = true;
  }

  clipboard_owner_ = NULL;
}

Clipboard::~Clipboard() {
  if (clipboard_owner_)
    ::DestroyWindow(clipboard_owner_);
  clipboard_owner_ = NULL;
}

void Clipboard::WriteObjects(const ObjectMap& objects) {
  WriteObjects(objects, NULL);
}

void Clipboard::WriteObjects(const ObjectMap& objects,
                             base::ProcessHandle process) {
  ScopedClipboard clipboard;
  if (!clipboard.Acquire(GetClipboardWindow()))
    return;

  ::EmptyClipboard();

  for (ObjectMap::const_iterator iter = objects.begin();
       iter != objects.end(); ++iter) {
    DispatchObject(static_cast<ObjectType>(iter->first), iter->second);
  }
}

void Clipboard::WriteText(const char* text_data, size_t text_len) {
  string16 text;
  UTF8ToUTF16(text_data, text_len, &text);
  HGLOBAL glob = CreateGlobalData(text);

  WriteToClipboard(CF_UNICODETEXT, glob);
}

void Clipboard::WriteHTML(const char* markup_data,
                          size_t markup_len,
                          const char* url_data,
                          size_t url_len) {
  std::string markup(markup_data, markup_len);
  std::string url;

  if (url_len > 0)
    url.assign(url_data, url_len);

  std::string html_fragment = ClipboardUtil::HtmlToCFHtml(markup, url);
  HGLOBAL glob = CreateGlobalData(html_fragment);

  WriteToClipboard(ClipboardUtil::GetHtmlFormat()->cfFormat, glob);
}

void Clipboard::WriteBookmark(const char* title_data,
                              size_t title_len,
                              const char* url_data,
                              size_t url_len) {
  std::string bookmark(title_data, title_len);
  bookmark.append(1, L'\n');
  bookmark.append(url_data, url_len);

  string16 wide_bookmark = UTF8ToWide(bookmark);
  HGLOBAL glob = CreateGlobalData(wide_bookmark);

  WriteToClipboard(ClipboardUtil::GetUrlWFormat()->cfFormat, glob);
}

void Clipboard::WriteWebSmartPaste() {
  DCHECK(clipboard_owner_);
  ::SetClipboardData(ClipboardUtil::GetWebKitSmartPasteFormat()->cfFormat,
                     NULL);
}

void Clipboard::WriteBitmap(const char* pixel_data, const char* size_data) {
  const gfx::Size* size = reinterpret_cast<const gfx::Size*>(size_data);
  HDC dc = ::GetDC(NULL);

  // This doesn't actually cost us a memcpy when the bitmap comes from the
  // renderer as we load it into the bitmap using setPixels which just sets a
  // pointer.  Someone has to memcpy it into GDI, it might as well be us here.

  // TODO(darin): share data in gfx/bitmap_header.cc somehow
  BITMAPINFO bm_info = {0};
  bm_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bm_info.bmiHeader.biWidth = size->width();
  bm_info.bmiHeader.biHeight = -size->height();  // sets vertical orientation
  bm_info.bmiHeader.biPlanes = 1;
  bm_info.bmiHeader.biBitCount = 32;
  bm_info.bmiHeader.biCompression = BI_RGB;

  // ::CreateDIBSection allocates memory for us to copy our bitmap into.
  // Unfortunately, we can't write the created bitmap to the clipboard,
  // (see http://msdn2.microsoft.com/en-us/library/ms532292.aspx)
  void *bits;
  HBITMAP source_hbitmap =
      ::CreateDIBSection(dc, &bm_info, DIB_RGB_COLORS, &bits, NULL, 0);

  if (bits && source_hbitmap) {
    // Copy the bitmap out of shared memory and into GDI
    memcpy(bits, pixel_data, 4 * size->width() * size->height());

    // Now we have an HBITMAP, we can write it to the clipboard
    WriteBitmapFromHandle(source_hbitmap, *size);
  }

  ::DeleteObject(source_hbitmap);
  ::ReleaseDC(NULL, dc);
}

void Clipboard::WriteBitmapFromHandle(HBITMAP source_hbitmap,
                                      const gfx::Size& size) {
  // We would like to just call ::SetClipboardData on the source_hbitmap,
  // but that bitmap might not be of a sort we can write to the clipboard.
  // For this reason, we create a new bitmap, copy the bits over, and then
  // write that to the clipboard.

  HDC dc = ::GetDC(NULL);
  HDC compatible_dc = ::CreateCompatibleDC(NULL);
  HDC source_dc = ::CreateCompatibleDC(NULL);

  // This is the HBITMAP we will eventually write to the clipboard
  HBITMAP hbitmap = ::CreateCompatibleBitmap(dc, size.width(), size.height());
  if (!hbitmap) {
    // Failed to create the bitmap
    ::DeleteDC(compatible_dc);
    ::DeleteDC(source_dc);
    ::ReleaseDC(NULL, dc);
    return;
  }

  HBITMAP old_hbitmap = (HBITMAP)SelectObject(compatible_dc, hbitmap);
  HBITMAP old_source = (HBITMAP)SelectObject(source_dc, source_hbitmap);

  // Now we need to blend it into an HBITMAP we can place on the clipboard
  BLENDFUNCTION bf = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
  ::GdiAlphaBlend(compatible_dc, 0, 0, size.width(), size.height(),
                  source_dc, 0, 0, size.width(), size.height(), bf);

  // Clean up all the handles we just opened
  ::SelectObject(compatible_dc, old_hbitmap);
  ::SelectObject(source_dc, old_source);
  ::DeleteObject(old_hbitmap);
  ::DeleteObject(old_source);
  ::DeleteDC(compatible_dc);
  ::DeleteDC(source_dc);
  ::ReleaseDC(NULL, dc);

  WriteToClipboard(CF_BITMAP, hbitmap);
}

void Clipboard::WriteData(const char* format_name, size_t format_len,
                          const char* data_data, size_t data_len) {
  std::string format(format_name, format_len);
  CLIPFORMAT clip_format =
      ::RegisterClipboardFormat(ASCIIToWide(format).c_str());

  HGLOBAL hdata = ::GlobalAlloc(GMEM_MOVEABLE, data_len);
  if (!hdata)
    return;

  char* data = static_cast<char*>(::GlobalLock(hdata));
  memcpy(data, data_data, data_len);
  ::GlobalUnlock(data);
  WriteToClipboard(clip_format, hdata);
}

void Clipboard::WriteToClipboard(unsigned int format, HANDLE handle) {
  DCHECK(clipboard_owner_);
  if (handle && !::SetClipboardData(format, handle)) {
    DCHECK(ERROR_CLIPBOARD_NOT_OPEN != GetLastError());
    FreeData(format, handle);
  }
}

bool Clipboard::IsFormatAvailable(const Clipboard::FormatType& format,
                                  Clipboard::Buffer buffer) const {
  DCHECK_EQ(buffer, BUFFER_STANDARD);
  int f;
  if (!base::StringToInt(format, &f))
    return false;
  return ::IsClipboardFormatAvailable(f) != FALSE;
}

bool Clipboard::IsFormatAvailableByString(
    const std::string& ascii_format, Clipboard::Buffer buffer) const {
  DCHECK_EQ(buffer, BUFFER_STANDARD);
  std::wstring wide_format = ASCIIToWide(ascii_format);
  CLIPFORMAT format = ::RegisterClipboardFormat(wide_format.c_str());
  return ::IsClipboardFormatAvailable(format) != FALSE;
}

void Clipboard::ReadText(Clipboard::Buffer buffer, string16* result) const {
  DCHECK_EQ(buffer, BUFFER_STANDARD);
  if (!result) {
    NOTREACHED();
    return;
  }

  result->clear();

  // Acquire the clipboard.
  ScopedClipboard clipboard;
  if (!clipboard.Acquire(GetClipboardWindow()))
    return;

  HANDLE data = ::GetClipboardData(CF_UNICODETEXT);
  if (!data)
    return;

  result->assign(static_cast<const char16*>(::GlobalLock(data)));
  ::GlobalUnlock(data);
}

void Clipboard::ReadAsciiText(Clipboard::Buffer buffer,
                              std::string* result) const {
  DCHECK_EQ(buffer, BUFFER_STANDARD);
  if (!result) {
    NOTREACHED();
    return;
  }

  result->clear();

  // Acquire the clipboard.
  ScopedClipboard clipboard;
  if (!clipboard.Acquire(GetClipboardWindow()))
    return;

  HANDLE data = ::GetClipboardData(CF_TEXT);
  if (!data)
    return;

  result->assign(static_cast<const char*>(::GlobalLock(data)));
  ::GlobalUnlock(data);
}

void Clipboard::ReadHTML(Clipboard::Buffer buffer, string16* markup,
                         std::string* src_url) const {
  DCHECK_EQ(buffer, BUFFER_STANDARD);
  if (markup)
    markup->clear();

  if (src_url)
    src_url->clear();

  // Acquire the clipboard.
  ScopedClipboard clipboard;
  if (!clipboard.Acquire(GetClipboardWindow()))
    return;

  HANDLE data = ::GetClipboardData(ClipboardUtil::GetHtmlFormat()->cfFormat);
  if (!data)
    return;

  std::string html_fragment(static_cast<const char*>(::GlobalLock(data)));
  ::GlobalUnlock(data);

  std::string markup_utf8;
  ClipboardUtil::CFHtmlToHtml(html_fragment, markup ? &markup_utf8 : NULL,
                              src_url);
  if (markup)
    markup->assign(UTF8ToWide(markup_utf8));
}

void Clipboard::ReadImage(Buffer buffer, std::string* data) const {
  DCHECK_EQ(buffer, BUFFER_STANDARD);
  if (!data) {
    NOTREACHED();
    return;
  }
}

void Clipboard::ReadBookmark(string16* title, std::string* url) const {
  if (title)
    title->clear();

  if (url)
    url->clear();

  // Acquire the clipboard.
  ScopedClipboard clipboard;
  if (!clipboard.Acquire(GetClipboardWindow()))
    return;

  HANDLE data = ::GetClipboardData(ClipboardUtil::GetUrlWFormat()->cfFormat);
  if (!data)
    return;

  string16 bookmark(static_cast<const char16*>(::GlobalLock(data)));
  ::GlobalUnlock(data);

  ParseBookmarkClipboardFormat(bookmark, title, url);
}

// Read a file in HDROP format from the clipboard.
void Clipboard::ReadFile(FilePath* file) const {
  if (!file) {
    NOTREACHED();
    return;
  }

  *file = FilePath();
  std::vector<FilePath> files;
  ReadFiles(&files);

  // Take the first file, if available.
  if (!files.empty())
    *file = files[0];
}

// Read a set of files in HDROP format from the clipboard.
void Clipboard::ReadFiles(std::vector<FilePath>* files) const {
  if (!files) {
    NOTREACHED();
    return;
  }

  files->clear();

  ScopedClipboard clipboard;
  if (!clipboard.Acquire(GetClipboardWindow()))
    return;

  HDROP drop = static_cast<HDROP>(::GetClipboardData(CF_HDROP));
  if (!drop)
    return;

  // Count of files in the HDROP.
  int count = ::DragQueryFile(drop, 0xffffffff, NULL, 0);

  if (count) {
    for (int i = 0; i < count; ++i) {
      int size = ::DragQueryFile(drop, i, NULL, 0) + 1;
      std::wstring file;
      ::DragQueryFile(drop, i, WriteInto(&file, size), size);
      files->push_back(FilePath(file));
    }
  }
}

void Clipboard::ReadData(const std::string& format, std::string* result) {
  if (!result) {
    NOTREACHED();
    return;
  }

  CLIPFORMAT clip_format =
      ::RegisterClipboardFormat(ASCIIToWide(format).c_str());

  ScopedClipboard clipboard;
  if (!clipboard.Acquire(GetClipboardWindow()))
    return;

  HANDLE data = ::GetClipboardData(clip_format);
  if (!data)
    return;

  result->assign(static_cast<const char*>(::GlobalLock(data)),
                 ::GlobalSize(data));
  ::GlobalUnlock(data);
}

// static
void Clipboard::ParseBookmarkClipboardFormat(const string16& bookmark,
                                             string16* title,
                                             std::string* url) {
  const string16 kDelim = ASCIIToUTF16("\r\n");

  const size_t title_end = bookmark.find_first_of(kDelim);
  if (title)
    title->assign(bookmark.substr(0, title_end));

  if (url) {
    const size_t url_start = bookmark.find_first_not_of(kDelim, title_end);
    if (url_start != string16::npos)
      *url = UTF16ToUTF8(bookmark.substr(url_start, string16::npos));
  }
}

// static
Clipboard::FormatType Clipboard::GetUrlFormatType() {
  return base::IntToString(ClipboardUtil::GetUrlFormat()->cfFormat);
}

// static
Clipboard::FormatType Clipboard::GetUrlWFormatType() {
  return base::IntToString(ClipboardUtil::GetUrlWFormat()->cfFormat);
}

// static
Clipboard::FormatType Clipboard::GetMozUrlFormatType() {
  return base::IntToString(ClipboardUtil::GetMozUrlFormat()->cfFormat);
}

// static
Clipboard::FormatType Clipboard::GetPlainTextFormatType() {
  return base::IntToString(ClipboardUtil::GetPlainTextFormat()->cfFormat);
}

// static
Clipboard::FormatType Clipboard::GetPlainTextWFormatType() {
  return base::IntToString(ClipboardUtil::GetPlainTextWFormat()->cfFormat);
}

// static
Clipboard::FormatType Clipboard::GetFilenameFormatType() {
  return base::IntToString(ClipboardUtil::GetFilenameFormat()->cfFormat);
}

// static
Clipboard::FormatType Clipboard::GetFilenameWFormatType() {
  return base::IntToString(ClipboardUtil::GetFilenameWFormat()->cfFormat);
}

// MS HTML Format
// static
Clipboard::FormatType Clipboard::GetHtmlFormatType() {
  return base::IntToString(ClipboardUtil::GetHtmlFormat()->cfFormat);
}

// static
Clipboard::FormatType Clipboard::GetBitmapFormatType() {
  return base::IntToString(CF_BITMAP);
}

// Firefox text/html
// static
Clipboard::FormatType Clipboard::GetTextHtmlFormatType() {
  return base::IntToString(ClipboardUtil::GetTextHtmlFormat()->cfFormat);
}

// static
Clipboard::FormatType Clipboard::GetCFHDropFormatType() {
  return base::IntToString(ClipboardUtil::GetCFHDropFormat()->cfFormat);
}

// static
Clipboard::FormatType Clipboard::GetFileDescriptorFormatType() {
  return base::IntToString(ClipboardUtil::GetFileDescriptorFormat()->cfFormat);
}

// static
Clipboard::FormatType Clipboard::GetFileContentFormatZeroType() {
  return base::IntToString(ClipboardUtil::GetFileContentFormatZero()->cfFormat);
}

// static
Clipboard::FormatType Clipboard::GetWebKitSmartPasteFormatType() {
  return base::IntToString(
      ClipboardUtil::GetWebKitSmartPasteFormat()->cfFormat);
}

// static
void Clipboard::FreeData(unsigned int format, HANDLE data) {
  if (format == CF_BITMAP)
    ::DeleteObject(static_cast<HBITMAP>(data));
  else
    ::GlobalFree(data);
}

HWND Clipboard::GetClipboardWindow() const {
  if (!clipboard_owner_ && create_window_) {
    clipboard_owner_ = ::CreateWindow(L"ClipboardOwnerWindowClass",
                                      L"ClipboardOwnerWindow",
                                      0, 0, 0, 0, 0,
                                      HWND_MESSAGE,
                                      0, 0, 0);
  }
  return clipboard_owner_;
}

}  // namespace ui
