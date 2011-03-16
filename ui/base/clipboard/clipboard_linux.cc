// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard.h"

#include <gtk/gtk.h>
#include <map>
#include <set>
#include <string>
#include <utility>

#include "base/file_path.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "ui/gfx/gtk_util.h"
#include "ui/gfx/size.h"

namespace ui {

namespace {

const char kMimeBmp[] = "image/bmp";
const char kMimeHtml[] = "text/html";
const char kMimeText[] = "text/plain";
const char kMimeMozillaUrl[] = "text/x-moz-url";
const char kMimeWebkitSmartPaste[] = "chromium/x-webkit-paste";

std::string GdkAtomToString(const GdkAtom& atom) {
  gchar* name = gdk_atom_name(atom);
  std::string rv(name);
  g_free(name);
  return rv;
}

GdkAtom StringToGdkAtom(const std::string& str) {
  return gdk_atom_intern(str.c_str(), FALSE);
}

// GtkClipboardGetFunc callback.
// GTK will call this when an application wants data we copied to the clipboard.
void GetData(GtkClipboard* clipboard,
             GtkSelectionData* selection_data,
             guint info,
             gpointer user_data) {
  Clipboard::TargetMap* data_map =
      reinterpret_cast<Clipboard::TargetMap*>(user_data);

  std::string target_string = GdkAtomToString(selection_data->target);
  Clipboard::TargetMap::iterator iter = data_map->find(target_string);

  if (iter == data_map->end())
    return;

  if (target_string == kMimeBmp) {
    gtk_selection_data_set_pixbuf(selection_data,
        reinterpret_cast<GdkPixbuf*>(iter->second.first));
  } else {
    gtk_selection_data_set(selection_data, selection_data->target, 8,
                           reinterpret_cast<guchar*>(iter->second.first),
                           iter->second.second);
  }
}

// GtkClipboardClearFunc callback.
// We are guaranteed this will be called exactly once for each call to
// gtk_clipboard_set_with_data.
void ClearData(GtkClipboard* clipboard,
               gpointer user_data) {
  Clipboard::TargetMap* map =
      reinterpret_cast<Clipboard::TargetMap*>(user_data);
  // The same data may be inserted under multiple keys, so use a set to
  // uniq them.
  std::set<char*> ptrs;

  for (Clipboard::TargetMap::iterator iter = map->begin();
       iter != map->end(); ++iter) {
    if (iter->first == kMimeBmp)
      g_object_unref(reinterpret_cast<GdkPixbuf*>(iter->second.first));
    else
      ptrs.insert(iter->second.first);
  }

  for (std::set<char*>::iterator iter = ptrs.begin();
       iter != ptrs.end(); ++iter) {
    delete[] *iter;
  }

  delete map;
}

// Called on GdkPixbuf destruction; see WriteBitmap().
void GdkPixbufFree(guchar* pixels, gpointer data) {
  free(pixels);
}

}  // namespace

Clipboard::Clipboard() : clipboard_data_(NULL) {
  clipboard_ = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
  primary_selection_ = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
}

Clipboard::~Clipboard() {
  gtk_clipboard_store(clipboard_);
}

void Clipboard::WriteObjects(const ObjectMap& objects) {
  clipboard_data_ = new TargetMap();

  for (ObjectMap::const_iterator iter = objects.begin();
       iter != objects.end(); ++iter) {
    DispatchObject(static_cast<ObjectType>(iter->first), iter->second);
  }

  SetGtkClipboard();
}

// When a URL is copied from a render view context menu (via "copy link
// location", for example), we additionally stick it in the X clipboard. This
// matches other linux browsers.
void Clipboard::DidWriteURL(const std::string& utf8_text) {
  gtk_clipboard_set_text(primary_selection_, utf8_text.c_str(),
                         utf8_text.length());
}

// Take ownership of the GTK clipboard and inform it of the targets we support.
void Clipboard::SetGtkClipboard() {
  scoped_array<GtkTargetEntry> targets(
      new GtkTargetEntry[clipboard_data_->size()]);

  int i = 0;
  for (Clipboard::TargetMap::iterator iter = clipboard_data_->begin();
       iter != clipboard_data_->end(); ++iter, ++i) {
    targets[i].target = const_cast<char*>(iter->first.c_str());
    targets[i].flags = 0;
    targets[i].info = 0;
  }

  if (gtk_clipboard_set_with_data(clipboard_, targets.get(),
                                  clipboard_data_->size(),
                                  GetData, ClearData,
                                  clipboard_data_)) {
    gtk_clipboard_set_can_store(clipboard_,
                                targets.get(),
                                clipboard_data_->size());
  }

  // clipboard_data_ now owned by the GtkClipboard.
  clipboard_data_ = NULL;
}

void Clipboard::WriteText(const char* text_data, size_t text_len) {
  char* data = new char[text_len];
  memcpy(data, text_data, text_len);

  InsertMapping(kMimeText, data, text_len);
  InsertMapping("TEXT", data, text_len);
  InsertMapping("STRING", data, text_len);
  InsertMapping("UTF8_STRING", data, text_len);
  InsertMapping("COMPOUND_TEXT", data, text_len);
}

void Clipboard::WriteHTML(const char* markup_data,
                          size_t markup_len,
                          const char* url_data,
                          size_t url_len) {
  // TODO(estade): We need to expand relative links with |url_data|.
  static const char* html_prefix = "<meta http-equiv=\"content-type\" "
                                   "content=\"text/html; charset=utf-8\">";
  size_t html_prefix_len = strlen(html_prefix);
  size_t total_len = html_prefix_len + markup_len + 1;

  char* data = new char[total_len];
  snprintf(data, total_len, "%s", html_prefix);
  memcpy(data + html_prefix_len, markup_data, markup_len);
  // Some programs expect NULL-terminated data. See http://crbug.com/42624
  data[total_len - 1] = '\0';

  InsertMapping(kMimeHtml, data, total_len);
}

// Write an extra flavor that signifies WebKit was the last to modify the
// pasteboard. This flavor has no data.
void Clipboard::WriteWebSmartPaste() {
  InsertMapping(kMimeWebkitSmartPaste, NULL, 0);
}

void Clipboard::WriteBitmap(const char* pixel_data, const char* size_data) {
  const gfx::Size* size = reinterpret_cast<const gfx::Size*>(size_data);

  guchar* data =
      gfx::BGRAToRGBA(reinterpret_cast<const uint8_t*>(pixel_data),
                      size->width(), size->height(), 0);

  GdkPixbuf* pixbuf =
      gdk_pixbuf_new_from_data(data, GDK_COLORSPACE_RGB, TRUE,
                               8, size->width(), size->height(),
                               size->width() * 4, GdkPixbufFree, NULL);
  // We store the GdkPixbuf*, and the size_t half of the pair is meaningless.
  // Note that this contrasts with the vast majority of entries in our target
  // map, which directly store the data and its length.
  InsertMapping(kMimeBmp, reinterpret_cast<char*>(pixbuf), 0);
}

void Clipboard::WriteBookmark(const char* title_data, size_t title_len,
                              const char* url_data, size_t url_len) {
  // Write as a mozilla url (UTF16: URL, newline, title).
  string16 url = UTF8ToUTF16(std::string(url_data, url_len) + "\n");
  string16 title = UTF8ToUTF16(std::string(title_data, title_len));
  int data_len = 2 * (title.length() + url.length());

  char* data = new char[data_len];
  memcpy(data, url.data(), 2 * url.length());
  memcpy(data + 2 * url.length(), title.data(), 2 * title.length());
  InsertMapping(kMimeMozillaUrl, data, data_len);
}

void Clipboard::WriteData(const char* format_name, size_t format_len,
                          const char* data_data, size_t data_len) {
  std::string format(format_name, format_len);
  // We assume that certain mapping types are only written by trusted code.
  // Therefore we must upkeep their integrity.
  if (format == kMimeBmp)
    return;
  char* data = new char[data_len];
  memcpy(data, data_data, data_len);
  InsertMapping(format.c_str(), data, data_len);
}

// We do not use gtk_clipboard_wait_is_target_available because of
// a bug with the gtk clipboard. It caches the available targets
// and does not always refresh the cache when it is appropriate.
bool Clipboard::IsFormatAvailable(const Clipboard::FormatType& format,
                                  Clipboard::Buffer buffer) const {
  GtkClipboard* clipboard = LookupBackingClipboard(buffer);
  if (clipboard == NULL)
    return false;

  bool format_is_plain_text = GetPlainTextFormatType() == format;
  if (format_is_plain_text) {
    // This tries a number of common text targets.
    if (gtk_clipboard_wait_is_text_available(clipboard))
      return true;
  }

  bool retval = false;
  GdkAtom* targets = NULL;
  GtkSelectionData* data =
      gtk_clipboard_wait_for_contents(clipboard,
                                      gdk_atom_intern("TARGETS", false));

  if (!data)
    return false;

  int num = 0;
  gtk_selection_data_get_targets(data, &targets, &num);

  // Some programs post data to the clipboard without any targets. If this is
  // the case we attempt to make sense of the contents as text. This is pretty
  // unfortunate since it means we have to actually copy the data to see if it
  // is available, but at least this path shouldn't be hit for conforming
  // programs.
  if (num <= 0) {
    if (format_is_plain_text) {
      gchar* text = gtk_clipboard_wait_for_text(clipboard);
      if (text) {
        g_free(text);
        retval = true;
      }
    }
  }

  GdkAtom format_atom = StringToGdkAtom(format);

  for (int i = 0; i < num; i++) {
    if (targets[i] == format_atom) {
      retval = true;
      break;
    }
  }

  g_free(targets);
  gtk_selection_data_free(data);

  return retval;
}

bool Clipboard::IsFormatAvailableByString(const std::string& format,
                                          Clipboard::Buffer buffer) const {
  return IsFormatAvailable(format, buffer);
}

void Clipboard::ReadAvailableTypes(Clipboard::Buffer buffer,
                                   std::vector<string16>* types,
                                   bool* contains_filenames) const {
  if (!types || !contains_filenames) {
    NOTREACHED();
    return;
  }

  // TODO(dcheng): Implement me.
  types->clear();
  *contains_filenames = false;
}


void Clipboard::ReadText(Clipboard::Buffer buffer, string16* result) const {
  GtkClipboard* clipboard = LookupBackingClipboard(buffer);
  if (clipboard == NULL)
    return;

  result->clear();
  gchar* text = gtk_clipboard_wait_for_text(clipboard);

  if (text == NULL)
    return;

  // TODO(estade): do we want to handle the possible error here?
  UTF8ToUTF16(text, strlen(text), result);
  g_free(text);
}

void Clipboard::ReadAsciiText(Clipboard::Buffer buffer,
                              std::string* result) const {
  GtkClipboard* clipboard = LookupBackingClipboard(buffer);
  if (clipboard == NULL)
    return;

  result->clear();
  gchar* text = gtk_clipboard_wait_for_text(clipboard);

  if (text == NULL)
    return;

  result->assign(text);
  g_free(text);
}

void Clipboard::ReadFile(FilePath* file) const {
  *file = FilePath();
}

// TODO(estade): handle different charsets.
// TODO(port): set *src_url.
void Clipboard::ReadHTML(Clipboard::Buffer buffer, string16* markup,
                         std::string* src_url) const {
  GtkClipboard* clipboard = LookupBackingClipboard(buffer);
  if (clipboard == NULL)
    return;
  markup->clear();

  GtkSelectionData* data = gtk_clipboard_wait_for_contents(clipboard,
      StringToGdkAtom(GetHtmlFormatType()));

  if (!data)
    return;

  // If the data starts with 0xFEFF, i.e., Byte Order Mark, assume it is
  // UTF-16, otherwise assume UTF-8.
  if (data->length >= 2 &&
      reinterpret_cast<uint16_t*>(data->data)[0] == 0xFEFF) {
    markup->assign(reinterpret_cast<uint16_t*>(data->data) + 1,
                   (data->length / 2) - 1);
  } else {
    UTF8ToUTF16(reinterpret_cast<char*>(data->data), data->length, markup);
  }

  // If there is a terminating NULL, drop it.
  if (!markup->empty() && markup->at(markup->length() - 1) == '\0')
    markup->resize(markup->length() - 1);

  gtk_selection_data_free(data);
}

void Clipboard::ReadImage(Buffer buffer, std::string* data) const {
  // TODO(dcheng): implement this.
  NOTIMPLEMENTED();
  if (!data) {
    NOTREACHED();
    return;
  }
}

void Clipboard::ReadBookmark(string16* title, std::string* url) const {
  // TODO(estade): implement this.
  NOTIMPLEMENTED();
}

void Clipboard::ReadData(const std::string& format, std::string* result) {
  GtkSelectionData* data =
      gtk_clipboard_wait_for_contents(clipboard_, StringToGdkAtom(format));
  if (!data)
    return;
  result->assign(reinterpret_cast<char*>(data->data), data->length);
  gtk_selection_data_free(data);
}

// static
Clipboard::FormatType Clipboard::GetPlainTextFormatType() {
  return GdkAtomToString(GDK_TARGET_STRING);
}

// static
Clipboard::FormatType Clipboard::GetPlainTextWFormatType() {
  return GetPlainTextFormatType();
}

// static
Clipboard::FormatType Clipboard::GetHtmlFormatType() {
  return std::string(kMimeHtml);
}

// static
Clipboard::FormatType Clipboard::GetBitmapFormatType() {
  return std::string(kMimeBmp);
}

// static
Clipboard::FormatType Clipboard::GetWebKitSmartPasteFormatType() {
  return std::string(kMimeWebkitSmartPaste);
}

void Clipboard::InsertMapping(const char* key,
                              char* data,
                              size_t data_len) {
  DCHECK(clipboard_data_->find(key) == clipboard_data_->end());
  (*clipboard_data_)[key] = std::make_pair(data, data_len);
}

GtkClipboard* Clipboard::LookupBackingClipboard(Buffer clipboard) const {
  switch (clipboard) {
    case BUFFER_STANDARD:
      return clipboard_;
    case BUFFER_SELECTION:
      return primary_selection_;
    default:
      NOTREACHED();
      return NULL;
  }
}

}  // namespace ui
