// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/widget/drop_target_gtk.h"

#include <algorithm>
#include <iterator>
#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/utf_string_conversions.h"
#include "net/base/net_util.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/gtk_dnd_util.h"
#include "ui/base/dragdrop/os_exchange_data_provider_gtk.h"
#include "ui/gfx/point.h"
#include "views/widget/root_view.h"
#include "views/widget/widget_gtk.h"

using ui::OSExchangeData;

namespace {

std::string GdkAtomToString(GdkAtom atom) {
  gchar* c_name = gdk_atom_name(atom);
  std::string name(c_name);
  g_free(c_name);
  return name;
}

// Returns true if |name| is a known name of plain text.
bool IsTextType(const std::string& name) {
  return name == "text/plain" || name == "TEXT" ||
         name == "STRING" || name == "UTF8_STRING" ||
         name == "text/plain;charset=utf-8";
}

// Returns the OSExchangeData::Formats in |targets| and all the
// OSExchangeData::CustomFormats in |type_set|.
int CalculateTypes(GList* targets, std::set<GdkAtom>* type_set) {
  int types = 0;
  for (GList* element = targets; element;
       element = g_list_next(element)) {
    GdkAtom atom = static_cast<GdkAtom>(element->data);
    type_set->insert(atom);
    if (atom == GDK_TARGET_STRING) {
      types |= OSExchangeData::STRING;
    } else if (atom == ui::GetAtomForTarget(ui::CHROME_NAMED_URL)) {
      types |= OSExchangeData::URL;
    } else if (atom == ui::GetAtomForTarget(ui::TEXT_URI_LIST)) {
      // TEXT_URI_LIST is used for files as well as urls.
      types |= OSExchangeData::URL | OSExchangeData::FILE_NAME;
    } else {
      std::string target_name = GdkAtomToString(atom);
      if (IsTextType(target_name)) {
        types |= OSExchangeData::STRING;
      } else {
        // Assume any unknown data is pickled.
        types |= OSExchangeData::PICKLED_DATA;
      }
    }
  }
  return types;
}

}  // namespace

namespace views {

DropTargetGtk::DropTargetGtk(RootView* root_view,
                             GdkDragContext* context)
    : helper_(root_view),
      requested_formats_(0),
      waiting_for_data_(false),
      received_drop_(false),
      pending_view_(NULL) {
  std::set<GdkAtom> all_formats;
  int source_formats = CalculateTypes(context->targets, &all_formats);
  data_.reset(new OSExchangeData(new OSExchangeDataProviderGtk(
                                     source_formats, all_formats)));
}

DropTargetGtk::~DropTargetGtk() {
}

void DropTargetGtk::ResetTargetViewIfEquals(View* view) {
  helper_.ResetTargetViewIfEquals(view);
}

void DropTargetGtk::OnDragDataReceived(GdkDragContext* context,
                                       gint x,
                                       gint y,
                                       GtkSelectionData* data,
                                       guint info,
                                       guint time) {
  std::string target_name = GdkAtomToString(data->type);
  if (data->type == GDK_TARGET_STRING || IsTextType(target_name)) {
    guchar* text_data = gtk_selection_data_get_text(data);
    string16 result;
    if (text_data) {
      char* as_char = reinterpret_cast<char*>(text_data);
      UTF8ToUTF16(as_char, strlen(as_char), &result);
      g_free(text_data);
    }
    data_provider().SetString(UTF16ToWideHack(result));
  } else if (requested_custom_formats_.find(data->type) !=
             requested_custom_formats_.end()) {
    Pickle result;
    if (data->length > 0)
      result = Pickle(reinterpret_cast<char*>(data->data), data->length);
    data_provider().SetPickledData(data->type, result);
  } else if (data->type == ui::GetAtomForTarget(ui::CHROME_NAMED_URL)) {
    GURL url;
    string16 title;
    ui::ExtractNamedURL(data, &url, &title);
    data_provider().SetURL(url, UTF16ToWideHack(title));
  } else if (data->type == ui::GetAtomForTarget(ui::TEXT_URI_LIST)) {
    std::vector<GURL> urls;
    ui::ExtractURIList(data, &urls);
    if (urls.size() == 1 && urls[0].is_valid()) {
      data_provider().SetURL(urls[0], std::wstring());

      // TEXT_URI_LIST is used for files as well as urls.
      if (urls[0].SchemeIsFile()) {
        FilePath file_path;
        if (net::FileURLToFilePath(urls[0], &file_path))
          data_provider().SetFilename(file_path);
      }
    } else {
      // Consumers of OSExchangeData will see this as an invalid URL. That is,
      // when GetURL is invoked on the OSExchangeData this triggers false to
      // be returned.
      data_provider().SetURL(GURL(), std::wstring());
    }
  }

  if (!data_->HasAllFormats(requested_formats_, requested_custom_formats_))
    return;  // Waiting on more data.

  int drag_operation = ui::DragDropTypes::GdkDragActionToDragOperation(
      context->actions);
  gfx::Point root_view_location(x, y);
  drag_operation = helper_.OnDragOver(*data_, root_view_location,
                                      drag_operation);
  GdkDragAction gdk_action = static_cast<GdkDragAction>(
      ui::DragDropTypes::DragOperationToGdkDragAction(drag_operation));
  if (!received_drop_)
    gdk_drag_status(context, gdk_action, time);

  waiting_for_data_ = false;

  if (pending_view_ && received_drop_) {
    FinishDrop(context, x, y, time);
    // WARNING: we've been deleted.
    return;
  }
}

gboolean DropTargetGtk::OnDragDrop(GdkDragContext* context,
                                   gint x,
                                   gint y,
                                   guint time) {
  received_drop_ = true;
  OnDragMotion(context, x, y, time);
  if (!pending_view_) {
    // User isn't over a view, no drop can occur.
    static_cast<WidgetGtk*>(
        helper_.root_view()->GetWidget())->ResetDropTarget();
    // WARNING: we've been deleted.
    return FALSE;
  }

  if (!waiting_for_data_) {
    // We've got all the data now.
    FinishDrop(context, x, y, time);
    // WARNING: we've been deleted.
    return TRUE;
  }
  // We're waiting on data.
  return TRUE;
}

void DropTargetGtk::OnDragLeave(GdkDragContext* context, guint time) {
  helper_.OnDragExit();
}

gboolean DropTargetGtk::OnDragMotion(GdkDragContext* context,
                                     gint x,
                                     gint y,
                                     guint time) {
  waiting_for_data_ = false;
  gfx::Point root_view_location(x, y);
  pending_view_ =
      helper_.CalculateTargetView(root_view_location, *data_, false);
  if (pending_view_ &&
      (received_drop_ || (pending_view_ != helper_.target_view() &&
                          pending_view_->AreDropTypesRequired()))) {
    // The target requires drop types before it can answer CanDrop,
    // ask for the data now.
    int formats = 0;
    std::set<GdkAtom> custom_formats;
    pending_view_->GetDropFormats(&formats, &custom_formats);
    IntersectFormats(data_provider().known_formats(),
                     data_provider().known_custom_formats(),
                     &formats, &custom_formats);
    if (!data_provider().HasDataForAllFormats(formats, custom_formats)) {
      if (!received_drop_)
        helper_.OnDragExit();

      // The target needs data for all the types before it can test if the
      // drop is valid, but we don't have all the data. Request the data
      // now. When we get back the data we'll update the target.
      RequestFormats(context, formats, custom_formats, time);

      waiting_for_data_ = true;

      return TRUE;
    }
  }

  int drag_operation = ui::DragDropTypes::GdkDragActionToDragOperation(
      context->actions);
  drag_operation = helper_.OnDragOver(*data_, root_view_location,
                                      drag_operation);
  if (!received_drop_) {
    GdkDragAction gdk_action =
        static_cast<GdkDragAction>(
            ui::DragDropTypes::DragOperationToGdkDragAction(drag_operation));
    gdk_drag_status(context, gdk_action, time);
  }
  return TRUE;
}

void DropTargetGtk::FinishDrop(GdkDragContext* context,
                               gint x, gint y, guint time) {
  gfx::Point root_view_location(x, y);
  int drag_operation = ui::DragDropTypes::GdkDragActionToDragOperation(
      context->actions);
  drag_operation = helper_.OnDrop(*data_, root_view_location,
                                  drag_operation);
  GdkDragAction gdk_action =
      static_cast<GdkDragAction>(
          ui::DragDropTypes::DragOperationToGdkDragAction(drag_operation));
  gtk_drag_finish(context, gdk_action != 0, (gdk_action & GDK_ACTION_MOVE),
                  time);

  static_cast<WidgetGtk*>(helper_.root_view()->GetWidget())->ResetDropTarget();
  // WARNING: we've been deleted.
}

void DropTargetGtk::IntersectFormats(int f1, const std::set<GdkAtom>& cf1,
                                     int* f2, std::set<GdkAtom>* cf2) {
  *f2 = (*f2 & f1);
  std::set<GdkAtom> cf;
  std::set_intersection(
      cf1.begin(), cf1.end(), cf2->begin(), cf2->end(),
      std::insert_iterator<std::set<GdkAtom> >(cf, cf.begin()));
  cf.swap(*cf2);
}

void DropTargetGtk::RequestFormats(GdkDragContext* context,
                                   int formats,
                                   const std::set<GdkAtom>& custom_formats,
                                   guint time) {
  GtkWidget* widget =
      static_cast<WidgetGtk*>(helper_.root_view()->GetWidget())->
      window_contents();

  const std::set<GdkAtom>& known_formats =
      data_provider().known_custom_formats();
  if ((formats & OSExchangeData::STRING) != 0 &&
      (requested_formats_ & OSExchangeData::STRING) == 0) {
    requested_formats_ |= OSExchangeData::STRING;
    if (known_formats.count(GDK_TARGET_STRING)) {
      gtk_drag_get_data(widget, context, GDK_TARGET_STRING, time);
    } else if (known_formats.count(gdk_atom_intern("text/plain", false))) {
      gtk_drag_get_data(widget, context, gdk_atom_intern("text/plain", false),
                        time);
    } else if (known_formats.count(gdk_atom_intern("text/plain;charset=utf-8",
                                                   false))) {
      gtk_drag_get_data(widget, context,
                        gdk_atom_intern("text/plain;charset=utf-8", false),
                        time);
    } else if (known_formats.count(gdk_atom_intern("TEXT", false))) {
        gtk_drag_get_data(widget, context, gdk_atom_intern("TEXT", false),
                          time);
    } else if (known_formats.count(gdk_atom_intern("STRING", false))) {
      gtk_drag_get_data(widget, context, gdk_atom_intern("STRING", false),
                        time);
    } else if (known_formats.count(gdk_atom_intern("UTF8_STRING", false))) {
      gtk_drag_get_data(widget, context,
                        gdk_atom_intern("UTF8_STRING", false), time);
    }
  }
  if ((formats & OSExchangeData::URL) != 0 &&
      (requested_formats_ & OSExchangeData::URL) == 0) {
    requested_formats_ |= OSExchangeData::URL;
    if (known_formats.count(ui::GetAtomForTarget(ui::CHROME_NAMED_URL))) {
      gtk_drag_get_data(widget, context,
                        ui::GetAtomForTarget(ui::CHROME_NAMED_URL), time);
    } else if (known_formats.count(
                   ui::GetAtomForTarget(ui::TEXT_URI_LIST))) {
      gtk_drag_get_data(widget, context,
                        ui::GetAtomForTarget(ui::TEXT_URI_LIST), time);
    }
  }
  if (((formats & OSExchangeData::FILE_NAME) != 0) &&
      (requested_formats_ & OSExchangeData::FILE_NAME) == 0) {
    requested_formats_ |= OSExchangeData::FILE_NAME;
    gtk_drag_get_data(widget, context,
                      ui::GetAtomForTarget(ui::TEXT_URI_LIST), time);
  }
  for (std::set<GdkAtom>::const_iterator i = custom_formats.begin();
       i != custom_formats.end(); ++i) {
    if (requested_custom_formats_.find(*i) ==
        requested_custom_formats_.end()) {
      requested_custom_formats_.insert(*i);
      gtk_drag_get_data(widget, context, *i, time);
    }
  }
}

OSExchangeDataProviderGtk& DropTargetGtk::data_provider() const {
  return static_cast<OSExchangeDataProviderGtk&>(data_->provider());
}

}  // namespace views
