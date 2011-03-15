// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CLIPBOARD_CLIPBOARD_H_
#define UI_BASE_CLIPBOARD_CLIPBOARD_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/process.h"
#include "base/shared_memory.h"
#include "base/string16.h"

namespace gfx {
class Size;
}

class FilePath;

#if defined(TOOLKIT_USES_GTK)
typedef struct _GtkClipboard GtkClipboard;
#endif

namespace ui {

class Clipboard {
 public:
  typedef std::string FormatType;

  // ObjectType designates the type of data to be stored in the clipboard. This
  // designation is shared across all OSes. The system-specific designation
  // is defined by FormatType. A single ObjectType might be represented by
  // several system-specific FormatTypes. For example, on Linux the CBF_TEXT
  // ObjectType maps to "text/plain", "STRING", and several other formats. On
  // windows it maps to CF_UNICODETEXT.
  enum ObjectType {
    CBF_TEXT,
    CBF_HTML,
    CBF_BOOKMARK,
    CBF_FILES,
    CBF_WEBKIT,
    CBF_BITMAP,
    CBF_SMBITMAP,  // Bitmap from shared memory.
    CBF_DATA,  // Arbitrary block of bytes.
  };

  // ObjectMap is a map from ObjectType to associated data.
  // The data is organized differently for each ObjectType. The following
  // table summarizes what kind of data is stored for each key.
  // * indicates an optional argument.
  //
  // Key           Arguments    Type
  // -------------------------------------
  // CBF_TEXT      text         char array
  // CBF_HTML      html         char array
  //               url*         char array
  // CBF_BOOKMARK  html         char array
  //               url          char array
  // CBF_LINK      html         char array
  //               url          char array
  // CBF_FILES     files        char array representing multiple files.
  //                            Filenames are separated by null characters and
  //                            the final filename is double null terminated.
  //                            The filenames are encoded in platform-specific
  //                            encoding.
  // CBF_WEBKIT    none         empty vector
  // CBF_BITMAP    pixels       byte array
  //               size         gfx::Size struct
  // CBF_SMBITMAP  shared_mem   A pointer to an unmapped base::SharedMemory
  //                            object containing the bitmap data.
  //               size         gfx::Size struct
  // CBF_DATA      format       char array
  //               data         byte array
  typedef std::vector<char> ObjectMapParam;
  typedef std::vector<ObjectMapParam> ObjectMapParams;
  typedef std::map<int /* ObjectType */, ObjectMapParams> ObjectMap;

  // Buffer designates which clipboard the action should be applied to.
  // Only platforms that use the X Window System support the selection
  // buffer. Furthermore we currently only use a buffer other than the
  // standard buffer when reading from the clipboard so only those
  // functions accept a buffer parameter.
  enum Buffer {
    BUFFER_STANDARD,
    BUFFER_SELECTION,
    BUFFER_DRAG,
  };

  static bool IsValidBuffer(int32 buffer) {
    switch (buffer) {
      case BUFFER_STANDARD:
        return true;
#if defined(USE_X11)
      case BUFFER_SELECTION:
        return true;
#endif
      case BUFFER_DRAG:
        return true;
    }
    return false;
  }

  static Buffer FromInt(int32 buffer) {
    return static_cast<Buffer>(buffer);
  }

  Clipboard();
  ~Clipboard();

  // Write a bunch of objects to the system clipboard. Copies are made of the
  // contents of |objects|. On Windows they are copied to the system clipboard.
  // On linux they are copied into a structure owned by the Clipboard object and
  // kept until the system clipboard is set again.
  void WriteObjects(const ObjectMap& objects);

  // Behaves as above. If there is some shared memory handle passed as one of
  // the objects, it came from the process designated by |process|. This will
  // assist in turning it into a shared memory region that the current process
  // can use.
  void WriteObjects(const ObjectMap& objects, base::ProcessHandle process);

  // On Linux/BSD, we need to know when the clipboard is set to a URL.  Most
  // platforms don't care.
#if defined(OS_WIN) || defined(OS_MACOSX)
  void DidWriteURL(const std::string& utf8_text) {}
#else  // !defined(OS_WIN) && !defined(OS_MACOSX)
  void DidWriteURL(const std::string& utf8_text);
#endif

  // Tests whether the clipboard contains a certain format
  bool IsFormatAvailable(const FormatType& format, Buffer buffer) const;

  // As above, but instead of interpreting |format| by some platform-specific
  // definition, interpret it as a literal MIME type.
  bool IsFormatAvailableByString(const std::string& format,
                                 Buffer buffer) const;

  // Reads UNICODE text from the clipboard, if available.
  void ReadText(Buffer buffer, string16* result) const;

  // Reads ASCII text from the clipboard, if available.
  void ReadAsciiText(Buffer buffer, std::string* result) const;

  // Reads HTML from the clipboard, if available.
  void ReadHTML(Buffer buffer, string16* markup, std::string* src_url) const;

  // Reads an image from the clipboard, if available. The returned data will be
  // encoded in PNG format.
  void ReadImage(Buffer buffer, std::string* data) const;

  // Reads a bookmark from the clipboard, if available.
  void ReadBookmark(string16* title, std::string* url) const;

  // Reads a file or group of files from the clipboard, if available, into the
  // out parameter.
  void ReadFile(FilePath* file) const;
  void ReadFiles(std::vector<FilePath>* files) const;

  // Reads raw data from the clipboard with the given format type. Stores result
  // as a byte vector.
  // TODO(dcheng): Due to platform limitations on Windows, we should make sure
  // format is never controlled by the user.
  void ReadData(const std::string& format, std::string* result);

  // Get format Identifiers for various types.
  static FormatType GetUrlFormatType();
  static FormatType GetUrlWFormatType();
  static FormatType GetMozUrlFormatType();
  static FormatType GetPlainTextFormatType();
  static FormatType GetPlainTextWFormatType();
  static FormatType GetFilenameFormatType();
  static FormatType GetFilenameWFormatType();
  static FormatType GetWebKitSmartPasteFormatType();
  // Win: MS HTML Format, Other: Generic HTML format
  static FormatType GetHtmlFormatType();
  static FormatType GetBitmapFormatType();

  // Embeds a pointer to a SharedMemory object pointed to by |bitmap_handle|
  // belonging to |process| into a shared bitmap [CBF_SMBITMAP] slot in
  // |objects|.  The pointer is deleted by DispatchObjects().
  //
  // On non-Windows platforms, |process| is ignored.
  static void ReplaceSharedMemHandle(ObjectMap* objects,
                                     base::SharedMemoryHandle bitmap_handle,
                                     base::ProcessHandle process);
#if defined(OS_WIN)
  // Firefox text/html
  static FormatType GetTextHtmlFormatType();
  static FormatType GetCFHDropFormatType();
  static FormatType GetFileDescriptorFormatType();
  static FormatType GetFileContentFormatZeroType();
#endif

 private:
  FRIEND_TEST_ALL_PREFIXES(ClipboardTest, SharedBitmapTest);
  FRIEND_TEST_ALL_PREFIXES(ClipboardTest, EmptyHTMLTest);

  void DispatchObject(ObjectType type, const ObjectMapParams& params);

  void WriteText(const char* text_data, size_t text_len);

  void WriteHTML(const char* markup_data,
                 size_t markup_len,
                 const char* url_data,
                 size_t url_len);

  void WriteBookmark(const char* title_data,
                     size_t title_len,
                     const char* url_data,
                     size_t url_len);

  void WriteWebSmartPaste();

  void WriteBitmap(const char* pixel_data, const char* size_data);

#if !defined(OS_MACOSX)
  // |format_name| is an ASCII string and should be NULL-terminated.
  // TODO(estade): port to mac.
  void WriteData(const char* format_name, size_t format_len,
                 const char* data_data, size_t data_len);
#endif
#if defined(OS_WIN)
  void WriteBitmapFromHandle(HBITMAP source_hbitmap,
                             const gfx::Size& size);

  // Safely write to system clipboard. Free |handle| on failure.
  void WriteToClipboard(unsigned int format, HANDLE handle);

  static void ParseBookmarkClipboardFormat(const string16& bookmark,
                                           string16* title,
                                           std::string* url);

  // Free a handle depending on its type (as intuited from format)
  static void FreeData(unsigned int format, HANDLE data);

  // Return the window that should be the clipboard owner, creating it
  // if neccessary.  Marked const for lazily initialization by const methods.
  HWND GetClipboardWindow() const;

  // Mark this as mutable so const methods can still do lazy initialization.
  mutable HWND clipboard_owner_;

  // True if we can create a window.
  bool create_window_;
#elif !defined(OS_MACOSX)
  // The public API is via WriteObjects() which dispatches to multiple
  // Write*() calls, but on GTK we must write all the clipboard types
  // in a single GTK call.  To support this we store the current set
  // of data we intend to put on the clipboard on clipboard_data_ as
  // WriteObjects is running, and then at the end call SetGtkClipboard
  // which replaces whatever is on the system clipboard with the
  // contents of clipboard_data_.

 public:
  typedef std::map<FormatType, std::pair<char*, size_t> > TargetMap;

 private:
  // Write changes to gtk clipboard.
  void SetGtkClipboard();
  // Insert a mapping into clipboard_data_.
  void InsertMapping(const char* key, char* data, size_t data_len);

  // Find the gtk clipboard for the passed buffer enum.
  GtkClipboard* LookupBackingClipboard(Buffer clipboard) const;

  TargetMap* clipboard_data_;
  GtkClipboard* clipboard_;
  GtkClipboard* primary_selection_;
#endif

  DISALLOW_COPY_AND_ASSIGN(Clipboard);
};

}  // namespace ui

#endif  // UI_BASE_CLIPBOARD_CLIPBOARD_H_
