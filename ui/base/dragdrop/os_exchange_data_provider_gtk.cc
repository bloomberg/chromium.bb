// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/dragdrop/os_exchange_data_provider_gtk.h"

#include <algorithm>

#include "base/files/file_path.h"
#include "base/utf_string_conversions.h"
#include "net/base/net_util.h"
#include "ui/base/dragdrop/gtk_dnd_util.h"

namespace ui {

OSExchangeDataProviderGtk::OSExchangeDataProviderGtk(
    int known_formats,
    const std::set<GdkAtom>& known_custom_formats)
    : known_formats_(known_formats),
      known_custom_formats_(known_custom_formats),
      formats_(0),
      drag_image_(NULL) {
}

OSExchangeDataProviderGtk::OSExchangeDataProviderGtk()
    : known_formats_(0),
      formats_(0),
      drag_image_(NULL) {
}

OSExchangeDataProviderGtk::~OSExchangeDataProviderGtk() {
  if (drag_image_)
    g_object_unref(drag_image_);
}

bool OSExchangeDataProviderGtk::HasDataForAllFormats(
    int formats,
    const std::set<GdkAtom>& custom_formats) const {
  if ((formats_ & formats) != formats)
    return false;
  for (std::set<GdkAtom>::iterator i = custom_formats.begin();
       i != custom_formats.end(); ++i) {
    if (pickle_data_.find(*i) == pickle_data_.end())
      return false;
  }
  return true;
}

GtkTargetList* OSExchangeDataProviderGtk::GetTargetList() const {
  GtkTargetList* targets = gtk_target_list_new(NULL, 0);

  if ((formats_ & OSExchangeData::STRING) != 0)
    gtk_target_list_add_text_targets(targets, OSExchangeData::STRING);

  if ((formats_ & OSExchangeData::URL) != 0) {
    gtk_target_list_add_uri_targets(targets, OSExchangeData::URL);
    gtk_target_list_add(
        targets,
        ui::GetAtomForTarget(ui::CHROME_NAMED_URL),
        0,
        OSExchangeData::URL);
  }

  if ((formats_ & OSExchangeData::FILE_NAME) != 0)
    gtk_target_list_add_uri_targets(targets, OSExchangeData::FILE_NAME);

  for (PickleData::const_iterator i = pickle_data_.begin();
       i != pickle_data_.end(); ++i) {
    gtk_target_list_add(targets, i->first, 0, OSExchangeData::PICKLED_DATA);
  }

  return targets;
}

void OSExchangeDataProviderGtk::WriteFormatToSelection(
    int format,
    GtkSelectionData* selection) const {
  if ((format & OSExchangeData::STRING) != 0) {
    gtk_selection_data_set_text(
        selection,
        reinterpret_cast<const gchar*>(string_.c_str()),
        -1);
  }

  if ((format & OSExchangeData::URL) != 0) {
    Pickle pickle;
    pickle.WriteString(UTF16ToUTF8(title_));
    pickle.WriteString(url_.spec());
    gtk_selection_data_set(
        selection,
        ui::GetAtomForTarget(ui::CHROME_NAMED_URL),
        8,
        reinterpret_cast<const guchar*>(pickle.data()),
        pickle.size());

    gchar* uri_array[2];
    uri_array[0] = strdup(url_.spec().c_str());
    uri_array[1] = NULL;
    gtk_selection_data_set_uris(selection, uri_array);
    free(uri_array[0]);
  }

  if ((format & OSExchangeData::FILE_NAME) != 0) {
    gchar* uri_array[2];
    uri_array[0] = strdup(net::FilePathToFileURL(
        base::FilePath(filename_)).spec().c_str());
    uri_array[1] = NULL;
    gtk_selection_data_set_uris(selection, uri_array);
    free(uri_array[0]);
  }

  if ((format & OSExchangeData::PICKLED_DATA) != 0) {
    for (PickleData::const_iterator i = pickle_data_.begin();
         i != pickle_data_.end(); ++i) {
      const Pickle& data = i->second;
      gtk_selection_data_set(
          selection,
          i->first,
          8,
          reinterpret_cast<const guchar*>(data.data()),
          data.size());
    }
  }
}

void OSExchangeDataProviderGtk::SetString(const string16& data) {
  string_ = data;
  formats_ |= OSExchangeData::STRING;
}

void OSExchangeDataProviderGtk::SetURL(const GURL& url, const string16& title) {
  url_ = url;
  title_ = title;
  formats_ |= OSExchangeData::URL;
}

void OSExchangeDataProviderGtk::SetFilename(const base::FilePath& path) {
  filename_ = path;
  formats_ |= OSExchangeData::FILE_NAME;
}

void OSExchangeDataProviderGtk::SetPickledData(GdkAtom format,
                                               const Pickle& data) {
  pickle_data_[format] = data;
  formats_ |= OSExchangeData::PICKLED_DATA;
}

bool OSExchangeDataProviderGtk::GetString(string16* data) const {
  if ((formats_ & OSExchangeData::STRING) == 0)
    return false;
  *data = string_;
  return true;
}

bool OSExchangeDataProviderGtk::GetURLAndTitle(GURL* url,
                                               string16* title) const {
  if ((formats_ & OSExchangeData::URL) == 0) {
    title->clear();
    return GetPlainTextURL(url);
  }

  if (!url_.is_valid())
    return false;

  *url = url_;
  *title = title_;
  return true;
}

bool OSExchangeDataProviderGtk::GetFilename(base::FilePath* path) const {
  if ((formats_ & OSExchangeData::FILE_NAME) == 0)
    return false;
  *path = filename_;
  return true;
}

bool OSExchangeDataProviderGtk::GetPickledData(GdkAtom format,
                                               Pickle* data) const {
  PickleData::const_iterator i = pickle_data_.find(format);
  if (i == pickle_data_.end())
    return false;

  *data = i->second;
  return true;
}

bool OSExchangeDataProviderGtk::HasString() const {
  return (known_formats_ & OSExchangeData::STRING) != 0 ||
         (formats_ & OSExchangeData::STRING) != 0;
}

bool OSExchangeDataProviderGtk::HasURL() const {
  if ((known_formats_ & OSExchangeData::URL) != 0 ||
      (formats_ & OSExchangeData::URL) != 0) {
    return true;
  }
  // No URL, see if we have plain text that can be parsed as a URL.
  return GetPlainTextURL(NULL);
}

bool OSExchangeDataProviderGtk::HasFile() const {
  return (known_formats_ & OSExchangeData::FILE_NAME) != 0 ||
         (formats_ & OSExchangeData::FILE_NAME) != 0;
  }

bool OSExchangeDataProviderGtk::HasCustomFormat(GdkAtom format) const {
  return known_custom_formats_.find(format) != known_custom_formats_.end() ||
         pickle_data_.find(format) != pickle_data_.end();
}

bool OSExchangeDataProviderGtk::GetPlainTextURL(GURL* url) const {
  if ((formats_ & OSExchangeData::STRING) == 0)
    return false;

  GURL test_url(string_);
  if (!test_url.is_valid())
    return false;

  if (url)
    *url = test_url;
  return true;
}

void OSExchangeDataProviderGtk::SetDragImage(
    GdkPixbuf* drag_image,
    const gfx::Vector2d& cursor_offset) {
  if (drag_image_)
    g_object_unref(drag_image_);
  g_object_ref(drag_image);
  drag_image_ = drag_image;
  cursor_offset_ = cursor_offset;
}

///////////////////////////////////////////////////////////////////////////////
// OSExchangeData, public:

// static
OSExchangeData::Provider* OSExchangeData::CreateProvider() {
  return new OSExchangeDataProviderGtk();
}

GdkAtom OSExchangeData::RegisterCustomFormat(const std::string& type) {
  return gdk_atom_intern(type.c_str(), false);
}

}  // namespace ui
