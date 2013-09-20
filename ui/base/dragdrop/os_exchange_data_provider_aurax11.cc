// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/dragdrop/os_exchange_data_provider_aurax11.h"

#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/message_loop/message_pump_x11.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/net_util.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/x/selection_utils.h"
#include "ui/base/x/x11_util.h"

// Note: the GetBlah() methods are used immediately by the
// web_contents_view_aura.cc:PrepareDropData(), while the omnibox is a
// little more discriminating and calls HasBlah() before trying to get the
// information.

namespace ui {

namespace {

const char kDndSelection[] = "XdndSelection";

const char* kAtomsToCache[] = {
  kString,
  kText,
  kUtf8String,
  kDndSelection,
  Clipboard::kMimeTypeURIList,
  kMimeTypeMozillaURL,
  Clipboard::kMimeTypeText,
  NULL
};

}  // namespace

OSExchangeDataProviderAuraX11::OSExchangeDataProviderAuraX11(
    ::Window x_window,
    const SelectionFormatMap& selection)
    : x_display_(gfx::GetXDisplay()),
      x_root_window_(DefaultRootWindow(x_display_)),
      own_window_(false),
      x_window_(x_window),
      atom_cache_(x_display_, kAtomsToCache),
      format_map_(selection),
      selection_owner_(x_display_, x_window_,
                       atom_cache_.GetAtom(kDndSelection)) {
  // We don't know all possible MIME types at compile time.
  atom_cache_.allow_uncached_atoms();
}

OSExchangeDataProviderAuraX11::OSExchangeDataProviderAuraX11()
    : x_display_(gfx::GetXDisplay()),
      x_root_window_(DefaultRootWindow(x_display_)),
      own_window_(true),
      x_window_(XCreateWindow(
          x_display_,
          x_root_window_,
          -100, -100, 10, 10,  // x, y, width, height
          0,                   // border width
          CopyFromParent,      // depth
          InputOnly,
          CopyFromParent,      // visual
          0,
          NULL)),
      atom_cache_(x_display_, kAtomsToCache),
      format_map_(),
      selection_owner_(x_display_, x_window_,
                       atom_cache_.GetAtom(kDndSelection)) {
  // We don't know all possible MIME types at compile time.
  atom_cache_.allow_uncached_atoms();

  XStoreName(x_display_, x_window_, "Chromium Drag & Drop Window");

  base::MessagePumpX11::Current()->AddDispatcherForWindow(this, x_window_);
}

OSExchangeDataProviderAuraX11::~OSExchangeDataProviderAuraX11() {
  if (own_window_) {
    base::MessagePumpX11::Current()->RemoveDispatcherForWindow(x_window_);
    XDestroyWindow(x_display_, x_window_);
  }
}

void OSExchangeDataProviderAuraX11::TakeOwnershipOfSelection() const {
  selection_owner_.TakeOwnershipOfSelection(format_map_);
}

void OSExchangeDataProviderAuraX11::RetrieveTargets(
    std::vector<Atom>* targets) const {
  selection_owner_.RetrieveTargets(targets);
}

SelectionFormatMap OSExchangeDataProviderAuraX11::GetFormatMap() const {
  // We return the |selection_owner_|'s format map instead of our own in case
  // ours has been modified since TakeOwnershipOfSelection() was called.
  return selection_owner_.selection_format_map();
}

OSExchangeData::Provider* OSExchangeDataProviderAuraX11::Clone() const {
  OSExchangeDataProviderAuraX11* ret = new OSExchangeDataProviderAuraX11();
  ret->format_map_ = format_map_;
  return ret;
}

void OSExchangeDataProviderAuraX11::SetString(const base::string16& text_data) {
  std::string utf8 = UTF16ToUTF8(text_data);
  scoped_refptr<base::RefCountedMemory> mem(
      base::RefCountedString::TakeString(&utf8));

  format_map_.Insert(atom_cache_.GetAtom(Clipboard::kMimeTypeText), mem);
  format_map_.Insert(atom_cache_.GetAtom(kText), mem);
  format_map_.Insert(atom_cache_.GetAtom(kString), mem);
  format_map_.Insert(atom_cache_.GetAtom(kUtf8String), mem);
}

void OSExchangeDataProviderAuraX11::SetURL(const GURL& url,
                                           const base::string16& title) {
  // Mozilla's URL format: (UTF16: URL, newline, title)
  if (url.is_valid()) {
    string16 spec = UTF8ToUTF16(url.spec());

    std::vector<unsigned char> data;
    ui::AddString16ToVector(spec, &data);
    ui::AddString16ToVector(ASCIIToUTF16("\n"), &data);
    ui::AddString16ToVector(title, &data);
    scoped_refptr<base::RefCountedMemory> mem(
        base::RefCountedBytes::TakeVector(&data));

    format_map_.Insert(atom_cache_.GetAtom(kMimeTypeMozillaURL), mem);

    SetString(spec);
  }
}

void OSExchangeDataProviderAuraX11::SetFilename(const base::FilePath& path) {
  NOTIMPLEMENTED();
}

void OSExchangeDataProviderAuraX11::SetFilenames(
    const std::vector<OSExchangeData::FileInfo>& filenames) {
  NOTIMPLEMENTED();
}

void OSExchangeDataProviderAuraX11::SetPickledData(
    const OSExchangeData::CustomFormat& format,
    const Pickle& pickle) {
  const unsigned char* data =
      reinterpret_cast<const unsigned char*>(pickle.data());

  std::vector<unsigned char> bytes;
  bytes.insert(bytes.end(), data, data + pickle.size());
  scoped_refptr<base::RefCountedMemory> mem(
      base::RefCountedBytes::TakeVector(&bytes));

  format_map_.Insert(atom_cache_.GetAtom(format.ToString().c_str()), mem);
}

bool OSExchangeDataProviderAuraX11::GetString(base::string16* result) const {
  std::vector< ::Atom> text_atoms = ui::GetTextAtomsFrom(&atom_cache_);
  std::vector< ::Atom> requested_types;
  ui::GetAtomIntersection(text_atoms, GetTargets(), &requested_types);

  ui::SelectionData data(format_map_.GetFirstOf(requested_types));
  if (data.IsValid()) {
    std::string text = data.GetText();
    *result = UTF8ToUTF16(text);
    return true;
  }

  return false;
}

bool OSExchangeDataProviderAuraX11::GetURLAndTitle(
    GURL* url,
    base::string16* title) const {
  std::vector< ::Atom> url_atoms = ui::GetURLAtomsFrom(&atom_cache_);
  std::vector< ::Atom> requested_types;
  ui::GetAtomIntersection(url_atoms, GetTargets(), &requested_types);

  ui::SelectionData data(format_map_.GetFirstOf(requested_types));
  if (data.IsValid()) {
    // TODO(erg): Technically, both of these forms can accept multiple URLs,
    // but that doesn't match the assumptions of the rest of the system which
    // expect single types.

    if (data.GetType() == atom_cache_.GetAtom(kMimeTypeMozillaURL)) {
      // Mozilla URLs are (UTF16: URL, newline, title).
      base::string16 unparsed;
      data.AssignTo(&unparsed);

      std::vector<base::string16> tokens;
      size_t num_tokens = Tokenize(unparsed, ASCIIToUTF16("\n"), &tokens);
      if (num_tokens > 0) {
        if (num_tokens > 1)
          *title = tokens[1];
        else
          *title = string16();

        *url = GURL(tokens[0]);
        return true;
      }
    } else if (data.GetType() == atom_cache_.GetAtom(
                   Clipboard::kMimeTypeURIList)) {
      // uri-lists are newline separated file lists in URL encoding.
      std::string unparsed;
      data.AssignTo(&unparsed);

      std::vector<std::string> tokens;
      size_t num_tokens = Tokenize(unparsed, "\n", &tokens);
      if (!num_tokens) {
        NOTREACHED() << "Empty URI list";
        return false;
      }

      *url = GURL(tokens[0]);
      *title = base::string16();

      return true;
    }
  }

  return false;
}

bool OSExchangeDataProviderAuraX11::GetFilename(base::FilePath* path) const {
  // On X11, files are passed by URL and aren't separate.
  return false;
}

bool OSExchangeDataProviderAuraX11::GetFilenames(
    std::vector<OSExchangeData::FileInfo>* filenames) const {
  // On X11, files are passed by URL and aren't separate.
  return false;
}

bool OSExchangeDataProviderAuraX11::GetPickledData(
    const OSExchangeData::CustomFormat& format,
    Pickle* pickle) const {
  std::vector< ::Atom> requested_types;
  requested_types.push_back(atom_cache_.GetAtom(format.ToString().c_str()));

  ui::SelectionData data(format_map_.GetFirstOf(requested_types));
  if (data.IsValid()) {
    // Note that the pickle object on the right hand side of the assignment
    // only refers to the bytes in |data|. The assignment copies the data.
    *pickle = Pickle(reinterpret_cast<const char*>(data.GetData()),
                     static_cast<int>(data.GetSize()));
    return true;
  }

  return false;
}

bool OSExchangeDataProviderAuraX11::HasString() const {
  std::vector< ::Atom> text_atoms = ui::GetTextAtomsFrom(&atom_cache_);
  std::vector< ::Atom> requested_types;
  ui::GetAtomIntersection(text_atoms, GetTargets(), &requested_types);
  return !requested_types.empty();
}

bool OSExchangeDataProviderAuraX11::HasURL() const {
  std::vector< ::Atom> url_atoms = ui::GetURLAtomsFrom(&atom_cache_);
  std::vector< ::Atom> requested_types;
  ui::GetAtomIntersection(url_atoms, GetTargets(), &requested_types);
  return !requested_types.empty();
}

bool OSExchangeDataProviderAuraX11::HasFile() const {
  // On X11, files are passed by URL and aren't separate.
  return false;
}

bool OSExchangeDataProviderAuraX11::HasCustomFormat(
    const OSExchangeData::CustomFormat& format) const {
  std::vector< ::Atom> url_atoms;
  url_atoms.push_back(atom_cache_.GetAtom(format.ToString().c_str()));
  std::vector< ::Atom> requested_types;
  ui::GetAtomIntersection(url_atoms, GetTargets(), &requested_types);

  return !requested_types.empty();
}

void OSExchangeDataProviderAuraX11::SetHtml(const base::string16& html,
                                            const GURL& base_url) {
  std::vector<unsigned char> bytes;
  // Manually jam a UTF16 BOM into bytes because otherwise, other programs will
  // assume UTF-8.
  bytes.push_back(0xFF);
  bytes.push_back(0xFE);
  ui::AddString16ToVector(html, &bytes);
  scoped_refptr<base::RefCountedMemory> mem(
      base::RefCountedBytes::TakeVector(&bytes));

  format_map_.Insert(atom_cache_.GetAtom(Clipboard::kMimeTypeHTML), mem);
}

bool OSExchangeDataProviderAuraX11::GetHtml(base::string16* html,
                                            GURL* base_url) const {
  std::vector< ::Atom> url_atoms;
  url_atoms.push_back(atom_cache_.GetAtom(Clipboard::kMimeTypeHTML));
  std::vector< ::Atom> requested_types;
  ui::GetAtomIntersection(url_atoms, GetTargets(), &requested_types);

  ui::SelectionData data(format_map_.GetFirstOf(requested_types));
  if (data.IsValid()) {
    *html = data.GetHtml();
    *base_url = GURL();
    return true;
  }

  return false;
}

bool OSExchangeDataProviderAuraX11::HasHtml() const {
  std::vector< ::Atom> url_atoms;
  url_atoms.push_back(atom_cache_.GetAtom(Clipboard::kMimeTypeHTML));
  std::vector< ::Atom> requested_types;
  ui::GetAtomIntersection(url_atoms, GetTargets(), &requested_types);

  return !requested_types.empty();
}

void OSExchangeDataProviderAuraX11::SetDragImage(
    const gfx::ImageSkia& image,
    const gfx::Vector2d& cursor_offset) {
  drag_image_ = image;
  drag_image_offset_ = cursor_offset;
}

const gfx::ImageSkia& OSExchangeDataProviderAuraX11::GetDragImage() const {
  return drag_image_;
}

const gfx::Vector2d& OSExchangeDataProviderAuraX11::GetDragImageOffset() const {
  return drag_image_offset_;
}

bool OSExchangeDataProviderAuraX11::Dispatch(const base::NativeEvent& event) {
  XEvent* xev = event;
  switch (xev->type) {
    case SelectionRequest:
      selection_owner_.OnSelectionRequest(xev->xselectionrequest);
      break;
    default:
      NOTIMPLEMENTED();
  }

  return true;
}

bool OSExchangeDataProviderAuraX11::GetPlainTextURL(GURL* url) const {
  string16 text;
  if (GetString(&text)) {
    GURL test_url(text);
    if (test_url.is_valid()) {
      *url = test_url;
      return true;
    }
  }

  return false;
}

std::vector< ::Atom> OSExchangeDataProviderAuraX11::GetTargets() const {
  return format_map_.GetTypes();
}

///////////////////////////////////////////////////////////////////////////////
// OSExchangeData, public:

// static
OSExchangeData::Provider* OSExchangeData::CreateProvider() {
  return new OSExchangeDataProviderAuraX11();
}

}  // namespace ui
