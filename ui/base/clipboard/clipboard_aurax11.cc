// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard.h"

#include <X11/extensions/Xfixes.h>
#include <X11/Xatom.h>
#include <list>
#include <set>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/i18n/icu_string_conversions.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/message_pump_aurax11.h"
#include "base/message_pump_observer.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/utf_string_conversions.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/x/x11_atom_cache.h"
#include "ui/base/x/x11_util.h"

#include "ui/gfx/size.h"

namespace ui {

namespace {

const char kChromeSelection[] = "CHROME_SELECTION";
const char kClipboard[] = "CLIPBOARD";
const char kMimeTypeBitmap[] = "image/bmp";
const char kMimeTypeFilename[] = "chromium/filename";
const char kMimeTypeMozillaURL[] = "text/x-moz-url";
const char kMimeTypePepperCustomData[] = "chromium/x-pepper-custom-data";
const char kMimeTypeWebkitSmartPaste[] = "chromium/x-webkit-paste";
const char kMultiple[] = "MULTIPLE";
const char kSourceTagType[] = "org.chromium.source-tag";
const char kString[] = "STRING";
const char kTargets[] = "TARGETS";
const char kText[] = "TEXT";
const char kUtf8String[] = "UTF8_STRING";

const char* kAtomsToCache[] = {
  kChromeSelection,
  kClipboard,
  kMimeTypeBitmap,
  kMimeTypeFilename,
  kMimeTypeMozillaURL,
  kMimeTypeWebkitSmartPaste,
  kMultiple,
  kSourceTagType,
  kString,
  kTargets,
  kText,
  kUtf8String,
  NULL
};

///////////////////////////////////////////////////////////////////////////////

// Returns a list of all text atoms that we handle.
std::vector< ::Atom> GetTextAtomsFrom(const X11AtomCache* atom_cache) {
  std::vector< ::Atom> atoms;
  atoms.push_back(atom_cache->GetAtom(kUtf8String));
  atoms.push_back(atom_cache->GetAtom(kString));
  atoms.push_back(atom_cache->GetAtom(kText));
  return atoms;
}

///////////////////////////////////////////////////////////////////////////////

// Uses the XFixes API to provide sequence numbers for GetSequenceNumber().
class SelectionChangeObserver : public base::MessagePumpObserver {
 public:
  static SelectionChangeObserver* GetInstance();

  uint64 clipboard_sequence_number() const {
    return clipboard_sequence_number_;
  }
  uint64 primary_sequence_number() const { return primary_sequence_number_; }

 private:
  friend struct DefaultSingletonTraits<SelectionChangeObserver>;

  SelectionChangeObserver();
  ~SelectionChangeObserver();

  // Overridden from base::MessagePumpObserver:
  virtual base::EventStatus WillProcessEvent(
      const base::NativeEvent& event) OVERRIDE;
  virtual void DidProcessEvent(
      const base::NativeEvent& event) OVERRIDE {}

  int event_base_;
  Atom clipboard_atom_;
  uint64 clipboard_sequence_number_;
  uint64 primary_sequence_number_;

  DISALLOW_COPY_AND_ASSIGN(SelectionChangeObserver);
};

SelectionChangeObserver::SelectionChangeObserver()
    : event_base_(-1),
      clipboard_atom_(None),
      clipboard_sequence_number_(0),
      primary_sequence_number_(0) {
  int ignored;
  if (XFixesQueryExtension(GetXDisplay(), &event_base_, &ignored)) {
    clipboard_atom_ = XInternAtom(GetXDisplay(), kClipboard, false);
    XFixesSelectSelectionInput(GetXDisplay(), GetX11RootWindow(),
                               clipboard_atom_,
                               XFixesSetSelectionOwnerNotifyMask |
                               XFixesSelectionWindowDestroyNotifyMask |
                               XFixesSelectionClientCloseNotifyMask);
    // This seems to be semi-optional. For some reason, registering for any
    // selection notify events seems to subscribe us to events for both the
    // primary and the clipboard buffers. Register anyway just to be safe.
    XFixesSelectSelectionInput(GetXDisplay(), GetX11RootWindow(),
                               XA_PRIMARY,
                               XFixesSetSelectionOwnerNotifyMask |
                               XFixesSelectionWindowDestroyNotifyMask |
                               XFixesSelectionClientCloseNotifyMask);

    base::MessagePumpAuraX11::Current()->AddObserver(this);
  }
}

SelectionChangeObserver::~SelectionChangeObserver() {
  // We are a singleton; we will outlive our message pump.
}

SelectionChangeObserver* SelectionChangeObserver::GetInstance() {
  return Singleton<SelectionChangeObserver>::get();
}

base::EventStatus SelectionChangeObserver::WillProcessEvent(
    const base::NativeEvent& event) {
  if (event->type == event_base_ + XFixesSelectionNotify) {
    XFixesSelectionNotifyEvent* ev =
        reinterpret_cast<XFixesSelectionNotifyEvent*>(event);
    if (ev->selection == clipboard_atom_) {
      clipboard_sequence_number_++;
    } else if (ev->selection == XA_PRIMARY) {
      primary_sequence_number_++;
    } else {
      DLOG(ERROR) << "Unexpected selection atom: " << ev->selection;
    }
  }
  return base::EVENT_CONTINUE;
}

///////////////////////////////////////////////////////////////////////////////

// Represents the selection in different data formats. Binary data passed in is
// assumed to be allocated with new char[], and is owned by FormatMap.
class FormatMap {
 public:
  // Our internal data store, which we only expose through iterators.
  typedef std::map< ::Atom, std::pair<char*, size_t> > InternalMap;
  typedef std::map< ::Atom, std::pair<char*, size_t> >::const_iterator
      const_iterator;

  FormatMap();
  ~FormatMap();

  // Adds the selection in the format |atom|. Ownership of |data| is passed to
  // us.
  void Insert(::Atom atom, char* data, size_t size);

  // Pass through to STL map. Only allow non-mutation access.
  const_iterator begin() const { return data_.begin(); }
  const_iterator end() const { return data_.end(); }
  const_iterator find(::Atom atom) const { return data_.find(atom); }
  size_t size() const { return data_.size(); }

 private:
  InternalMap data_;

  DISALLOW_COPY_AND_ASSIGN(FormatMap);
};

FormatMap::FormatMap() {}

FormatMap::~FormatMap() {
  // WriteText() inserts the same pointer multiple times for different
  // representations; we need to dedupe it.
  std::set<char*> to_delete;
  for (InternalMap::iterator it = data_.begin(); it != data_.end(); ++it)
    to_delete.insert(it->second.first);

  for (std::set<char*>::iterator it = to_delete.begin(); it != to_delete.end();
       ++it) {
    delete [] *it;
  }
}

void FormatMap::Insert(::Atom atom, char* data, size_t size) {
  DCHECK(data_.find(atom) == data_.end());
  data_.insert(std::make_pair(atom, std::make_pair(data, size)));
}

///////////////////////////////////////////////////////////////////////////////

// Represents a list of possible return types. Copy constructable.
class TargetList {
 public:
  typedef std::vector< ::Atom> AtomVector;

  TargetList(const AtomVector& target_list, X11AtomCache* atom_cache);

  bool ContainsText() const;
  bool ContainsFormat(const Clipboard::FormatType& format_type) const;
  bool ContainsAtom(::Atom atom) const;

 private:
  AtomVector target_list_;
  X11AtomCache* atom_cache_;
};

TargetList::TargetList(const AtomVector& target_list,
                       X11AtomCache* atom_cache)
    : target_list_(target_list),
      atom_cache_(atom_cache) {
}

bool TargetList::ContainsText() const {
  std::vector< ::Atom> atoms = GetTextAtomsFrom(atom_cache_);
  for (std::vector< ::Atom>::const_iterator it = atoms.begin();
       it != atoms.end(); ++it) {
    if (ContainsAtom(*it))
      return true;
  }

  return false;
}

bool TargetList::ContainsFormat(
    const Clipboard::FormatType& format_type) const {
  ::Atom atom = atom_cache_->GetAtom(format_type.ToString().c_str());
  return ContainsAtom(atom);
}

bool TargetList::ContainsAtom(::Atom atom) const {
  return find(target_list_.begin(), target_list_.end(), atom)
      != target_list_.end();
}

///////////////////////////////////////////////////////////////////////////////

// A holder for data with optional X11 deletion semantics.
class SelectionData {
 public:
  // |atom_cache| is still owned by caller.
  explicit SelectionData(X11AtomCache* atom_cache);
  ~SelectionData();

  ::Atom type() const { return type_; }
  char* data() const { return data_; }
  size_t size() const { return size_; }

  void Set(::Atom type, char* data, size_t size, bool owned);

  // If |type_| is a string type, convert the data to UTF8 and return it.
  std::string GetText() const;

  // Assigns the raw data to the string.
  void AssignTo(std::string* result) const;

 private:
  ::Atom type_;
  char* data_;
  size_t size_;
  bool owned_;

  X11AtomCache* atom_cache_;
};

SelectionData::SelectionData(X11AtomCache* atom_cache)
    : type_(None),
      data_(NULL),
      size_(0),
      owned_(false),
      atom_cache_(atom_cache) {
}

SelectionData::~SelectionData() {
  if (owned_)
    XFree(data_);
}

void SelectionData::Set(::Atom type, char* data, size_t size, bool owned) {
  if (owned_)
    XFree(data_);

  type_ = type;
  data_ = data;
  size_ = size;
  owned_ = owned;
}

std::string SelectionData::GetText() const {
  if (type_ == atom_cache_->GetAtom(kUtf8String) ||
      type_ == atom_cache_->GetAtom(kText)) {
    return std::string(data_, size_);
  } else if (type_ == atom_cache_->GetAtom(kString)) {
    std::string result;
    base::ConvertToUtf8AndNormalize(std::string(data_, size_),
                                    base::kCodepageLatin1,
                                    &result);
    return result;
  } else {
    // BTW, I looked at COMPOUND_TEXT, and there's no way we're going to
    // support that. Yuck.
    NOTREACHED();
    return std::string();
  }
}

void SelectionData::AssignTo(std::string* result) const {
  result->assign(data_, size_);
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////

// I would love for the FormatType to really be a wrapper around an X11 ::Atom,
// but there are a few problems. Chromeos unit tests spawn a new X11 server for
// each test, so Atom numeric values don't persist across tests. We could still
// maybe deal with that if we didn't have static accessor methods everywhere.

Clipboard::FormatType::FormatType() {
}

Clipboard::FormatType::FormatType(const std::string& native_format)
    : data_(native_format) {
}

Clipboard::FormatType::~FormatType() {
}

std::string Clipboard::FormatType::Serialize() const {
  return data_;
}

// static
Clipboard::FormatType Clipboard::FormatType::Deserialize(
    const std::string& serialization) {
  return FormatType(serialization);
}

bool Clipboard::FormatType::Equals(const FormatType& other) const {
  return data_ == other.data_;
}

///////////////////////////////////////////////////////////////////////////////
// Clipboard::AuraX11Details

// Private implementation of our X11 integration. Keeps X11 headers out of the
// majority of chrome, which break badly.
class Clipboard::AuraX11Details : public base::MessagePumpDispatcher {
 public:
  AuraX11Details();
  ~AuraX11Details();

  X11AtomCache* atom_cache() { return &atom_cache_; }

  // Returns the X11 type that we pass to various XSelection functions for the
  // given buffer.
  ::Atom LookupSelectionForBuffer(Buffer buffer) const;

  // Finds the FormatMap for the incoming selection atom.
  FormatMap* LookupStorageForAtom(::Atom atom);

  // As we need to collect all the data types before we tell X11 that we own a
  // particular selection, we create a temporary clipboard mapping that
  // InsertMapping writes to. Then we commit it in TakeOwnershipOfSelection,
  // where we save it in one of the clipboard data slots.
  void CreateNewClipboardData();

  // Inserts a mapping into clipboard_data_.
  void InsertMapping(const std::string& key, char* data, size_t data_len);

  // Moves the temporary |clipboard_data_| to the long term data storage for
  // |buffer|.
  void TakeOwnershipOfSelection(Buffer buffer);

  // Returns the first of |types| offered by the current selection holder in
  // |data_out|, or returns NULL if none of those types are available.
  //
  // If the selection holder is us, this call is synchronous and we pull
  // the data out of |clipboard_selection_| or |primary_selection_|. If the
  // selection holder is some other window, we spin up a nested message loop
  // and do the asynchronous dance with whatever application is holding the
  // selection.
  scoped_ptr<SelectionData> RequestAndWaitForTypes(
      Buffer buffer,
      const std::vector< ::Atom>& types);

  // Retrieves the list of possible data types the current clipboard owner has.
  //
  // If the selection holder is us, this is synchronous, otherwise this runs a
  // blocking message loop.
  TargetList WaitAndGetTargetsList(Buffer buffer);

  // Does the work of requesting |target| from |selection_name|, spinning up
  // the nested message loop, and reading the resulting data back. |out_data|
  // is allocated with the X allocator and must be freed with
  // XFree(). |out_data_bytes| is the length in machine chars, while
  // |out_data_items| is the length in |out_type| items.
  bool PerformBlockingConvertSelection(::Atom selection_name,
                                       ::Atom target,
                                       unsigned char** out_data,
                                       size_t* out_data_bytes,
                                       size_t* out_data_items,
                                       ::Atom* out_type);

  // Returns a list of all text atoms that we handle.
  std::vector< ::Atom> GetTextAtoms() const;

  // Returns a vector with a |format| converted to an X11 atom.
  std::vector< ::Atom> GetAtomsForFormat(const Clipboard::FormatType& format);

  // Clears a certain data buffer.
  void Clear(Buffer buffer);

 private:
  // Called by Dispatch to handle specific types of events.
  void HandleSelectionRequest(const XSelectionRequestEvent& event);
  void HandleSelectionNotify(const XSelectionEvent& event);
  void HandleSelectionClear(const XSelectionClearEvent& event);
  void HandlePropertyNotify(const XPropertyEvent& event);

  // Overridden from base::MessagePumpDispatcher:
  virtual bool Dispatch(const base::NativeEvent& event) OVERRIDE;

  // Temporary target map that we write to during DispatchObects.
  scoped_ptr<FormatMap> clipboard_data_;

  // The current value of our clipboard and primary selections. These should be
  // non-NULL when we own the selection.
  scoped_ptr<FormatMap> clipboard_selection_;
  scoped_ptr<FormatMap> primary_selection_;

  // Our X11 state.
  Display* x_display_;
  ::Window x_root_window_;

  // Input-only window used as a selection owner.
  ::Window x_window_;

  // True if we're currently running a nested message loop, waiting for data to
  // come back from the X server.
  bool in_nested_loop_;

  // Data to the current XConvertSelection request. Used for error detection;
  // we verify it on the return message.
  ::Atom current_selection_;
  ::Atom current_target_;

  // The property in the returning SelectNotify message is used to signal
  // success. If None, our request failed somehow. If equal to the property
  // atom that we sent in the XConvertSelection call, we can read that property
  // on |x_window_| for the requested data.
  ::Atom returned_property_;

  // Called to terminate the nested message loop.
  base::Closure quit_closure_;

  X11AtomCache atom_cache_;

  DISALLOW_COPY_AND_ASSIGN(AuraX11Details);
};

Clipboard::AuraX11Details::AuraX11Details()
    : x_display_(GetXDisplay()),
      x_root_window_(DefaultRootWindow(x_display_)),
      in_nested_loop_(false),
      atom_cache_(x_display_, kAtomsToCache) {
  // We don't know all possible MIME types at compile time.
  atom_cache_.allow_uncached_atoms();

  x_window_ = XCreateWindow(
      x_display_, x_root_window_,
      -100, -100, 10, 10,  // x, y, width, height
      0,                   // border width
      CopyFromParent,      // depth
      InputOnly,
      CopyFromParent,      // visual
      0,
      NULL);
  XStoreName(x_display_, x_window_, "Chromium clipboard");
  XSelectInput(x_display_, x_window_, PropertyChangeMask);

  base::MessagePumpAuraX11::Current()->AddDispatcherForWindow(this, x_window_);
}

Clipboard::AuraX11Details::~AuraX11Details() {
  base::MessagePumpAuraX11::Current()->RemoveDispatcherForWindow(x_window_);

  XDestroyWindow(x_display_, x_window_);
}

::Atom Clipboard::AuraX11Details::LookupSelectionForBuffer(
    Buffer buffer) const {
  if (buffer == BUFFER_STANDARD)
    return atom_cache_.GetAtom(kClipboard);
  else
    return XA_PRIMARY;
}

FormatMap* Clipboard::AuraX11Details::LookupStorageForAtom(::Atom atom) {
  if (atom == XA_PRIMARY)
    return primary_selection_.get();
  else if (atom == atom_cache_.GetAtom(kClipboard))
    return clipboard_selection_.get();
  else
    return NULL;
}

void Clipboard::AuraX11Details::CreateNewClipboardData() {
  clipboard_data_.reset(new FormatMap);
}

void Clipboard::AuraX11Details::InsertMapping(const std::string& key,
                                              char* data,
                                              size_t data_len) {
  ::Atom atom_key = atom_cache_.GetAtom(key.c_str());
  clipboard_data_->Insert(atom_key, data, data_len);
}

void Clipboard::AuraX11Details::TakeOwnershipOfSelection(Buffer buffer) {
  // Tell the X server that we are now the selection owner.
  ::Atom xselection = LookupSelectionForBuffer(buffer);
  XSetSelectionOwner(x_display_, xselection, x_window_, CurrentTime);

  if (XGetSelectionOwner(x_display_, xselection) == x_window_) {
    // The X server agrees that we are the selection owner. Commit our data.
    if (buffer == BUFFER_STANDARD)
      clipboard_selection_ = clipboard_data_.Pass();
    else
      primary_selection_ = clipboard_data_.Pass();
  }
}

scoped_ptr<SelectionData> Clipboard::AuraX11Details::RequestAndWaitForTypes(
    Buffer buffer,
    const std::vector< ::Atom>& types) {
  ::Atom selection_name = LookupSelectionForBuffer(buffer);
  if (XGetSelectionOwner(x_display_, selection_name) == x_window_) {
    // We can local fastpath instead of playing the nested message loop game
    // with the X server.
    FormatMap* format_map = LookupStorageForAtom(selection_name);
    DCHECK(format_map);

    for (std::vector< ::Atom>::const_iterator it = types.begin();
         it != types.end(); ++it) {
      FormatMap::const_iterator format_map_it = format_map->find(*it);
      if (format_map_it != format_map->end()) {
        scoped_ptr<SelectionData> data_out(new SelectionData(&atom_cache_));
        data_out->Set(format_map_it->first, format_map_it->second.first,
                      format_map_it->second.second, false);
        return data_out.Pass();
      }
    }
  } else {
    TargetList targets = WaitAndGetTargetsList(buffer);

    for (std::vector< ::Atom>::const_iterator it = types.begin();
         it != types.end(); ++it) {
      unsigned char* data = NULL;
      size_t data_bytes = 0;
      ::Atom type = None;
      if (targets.ContainsAtom(*it) &&
          PerformBlockingConvertSelection(selection_name,
                                          *it,
                                          &data,
                                          &data_bytes,
                                          NULL,
                                          &type) &&
          type == *it) {
        scoped_ptr<SelectionData> data_out(new SelectionData(&atom_cache_));
        data_out->Set(type, (char*)data, data_bytes, true);
        return data_out.Pass();
      }
    }
  }

  return scoped_ptr<SelectionData>();
}

TargetList Clipboard::AuraX11Details::WaitAndGetTargetsList(
    Buffer buffer) {
  ::Atom selection_name = LookupSelectionForBuffer(buffer);
  std::vector< ::Atom> out;
  if (XGetSelectionOwner(x_display_, selection_name) == x_window_) {
    // We can local fastpath and return the list of local targets.
    FormatMap* format_map = LookupStorageForAtom(selection_name);
    DCHECK(format_map);

    for (FormatMap::const_iterator it = format_map->begin();
         it != format_map->end(); ++it) {
      out.push_back(it->first);
    }
  } else {
    unsigned char* data = NULL;
    size_t out_data_items = 0;
    ::Atom out_type = None;

    if (PerformBlockingConvertSelection(selection_name,
                                        atom_cache_.GetAtom(kTargets),
                                        &data,
                                        NULL,
                                        &out_data_items,
                                        &out_type)) {
      ::Atom* atom_array = reinterpret_cast< ::Atom*>(data);
      for (size_t i = 0; i < out_data_items; ++i)
        out.push_back(atom_array[i]);

      XFree(data);
    } else {
      // There was no target list. Most Java apps doesn't offer a TARGETS list,
      // even though they AWT to. They will offer individual text types if you
      // ask. If this is the case we attempt to make sense of the contents as
      // text. This is pretty unfortunate since it means we have to actually
      // copy the data to see if it is available, but at least this path
      // shouldn't be hit for conforming programs.
      std::vector< ::Atom> types = GetTextAtoms();
      for (std::vector< ::Atom>::const_iterator it = types.begin();
           it != types.end(); ++it) {
        ::Atom type = None;
        if (PerformBlockingConvertSelection(selection_name,
                                            *it,
                                            NULL,
                                            NULL,
                                            NULL,
                                            &type) &&
            type == *it) {
          out.push_back(*it);
        }
      }
    }
  }

  return TargetList(out, &atom_cache_);
}

bool Clipboard::AuraX11Details::PerformBlockingConvertSelection(
    ::Atom selection_name,
    ::Atom target,
    unsigned char** out_data,
    size_t* out_data_bytes,
    size_t* out_data_items,
    ::Atom* out_type) {
  // The name of the property we're asking to be set on |x_window_|.
  ::Atom property_to_set = atom_cache_.GetAtom(kChromeSelection);

  XConvertSelection(x_display_,
                    selection_name,
                    target,
                    property_to_set,
                    x_window_,
                    CurrentTime);

  // Now that we've thrown our message off to the X11 server, we block waiting
  // for a response.
  MessageLoopForUI* loop = MessageLoopForUI::current();
  MessageLoop::ScopedNestableTaskAllower allow_nested(loop);
  base::RunLoop run_loop(base::MessagePumpAuraX11::Current());

  current_selection_ = selection_name;
  current_target_ = target;
  in_nested_loop_ = true;
  quit_closure_ = run_loop.QuitClosure();
  run_loop.Run();
  in_nested_loop_ = false;
  current_selection_ = None;
  current_target_ = None;

  if (returned_property_ != property_to_set)
    return false;

  // Retrieve the data from our window.
  unsigned long nitems = 0;
  unsigned long nbytes = 0;
  Atom prop_type = None;
  int prop_format = 0;
  unsigned char* property_data = NULL;
  if (XGetWindowProperty(x_display_,
                         x_window_,
                         returned_property_,
                         0, 0x1FFFFFFF /* MAXINT32 / 4 */, False,
                         AnyPropertyType, &prop_type, &prop_format,
                         &nitems, &nbytes, &property_data) != Success) {
    return false;
  }

  if (prop_type == None)
    return false;

  if (out_data)
    *out_data = property_data;

  if (out_data_bytes) {
    // So even though we should theoretically have nbytes (and we can't
    // pass NULL there), we need to manually calculate the byte length here
    // because nbytes always returns zero.
    switch (prop_format) {
      case 8:
        *out_data_bytes = nitems;
        break;
      case 16:
        *out_data_bytes = sizeof(short) * nitems;
        break;
      case 32:
        *out_data_bytes = sizeof(long) * nitems;
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  if (out_data_items)
    *out_data_items = nitems;

  if (out_type)
    *out_type = prop_type;

  return true;
}

std::vector< ::Atom> Clipboard::AuraX11Details::GetTextAtoms() const {
  return GetTextAtomsFrom(&atom_cache_);
}

std::vector< ::Atom> Clipboard::AuraX11Details::GetAtomsForFormat(
    const Clipboard::FormatType& format) {
  std::vector< ::Atom> atoms;
  atoms.push_back(atom_cache_.GetAtom(format.ToString().c_str()));
  return atoms;
}

void Clipboard::AuraX11Details::Clear(Buffer buffer) {
  ::Atom selection_name = LookupSelectionForBuffer(buffer);
  if (XGetSelectionOwner(x_display_, selection_name) == x_window_)
    XSetSelectionOwner(x_display_, selection_name, None, CurrentTime);

  if (buffer == BUFFER_STANDARD)
    clipboard_selection_.reset();
  else
    primary_selection_.reset();
}

void Clipboard::AuraX11Details::HandleSelectionRequest(
    const XSelectionRequestEvent& event) {
  // Incrementally build our selection. By default this is a refusal, and we'll
  // override the parts indicating success in the different cases.
  XEvent reply;
  reply.xselection.type = SelectionNotify;
  reply.xselection.requestor = event.requestor;
  reply.xselection.selection = event.selection;
  reply.xselection.target = event.target;
  reply.xselection.property = None;  // Indicates failure
  reply.xselection.time = event.time;

  // Get the proper selection.
  FormatMap* format_map = LookupStorageForAtom(event.selection);
  if (format_map) {
    ::Atom targets_atom = atom_cache_.GetAtom(kTargets);
    if (event.target == targets_atom) {
      std::vector< ::Atom> targets;
      targets.push_back(targets_atom);
      for (FormatMap::const_iterator it = format_map->begin();
           it != format_map->end(); ++it) {
        targets.push_back(it->first);
      }

      XChangeProperty(x_display_, event.requestor, event.property, XA_ATOM, 32,
                      PropModeReplace,
                      reinterpret_cast<unsigned char*>(&targets.front()),
                      targets.size());
      reply.xselection.property = event.property;
    } else if (event.target == atom_cache_.GetAtom(kMultiple)) {
      // TODO(erg): Theoretically, the spec claims I'm supposed to handle the
      // MULTIPLE case, but I haven't seen it in the wild yet.
      NOTIMPLEMENTED();
    } else {
      // Try to find the data type in map.
      FormatMap::const_iterator it = format_map->find(event.target);
      if (it != format_map->end()) {
        XChangeProperty(x_display_, event.requestor, event.property,
                        event.target, 8,
                        PropModeReplace,
                        reinterpret_cast<unsigned char*>(it->second.first),
                        it->second.second);
        reply.xselection.property = event.property;
      }
      // I would put error logging here, but GTK ignores TARGETS and spams us
      // looking for its own internal types.
    }
  } else {
    DLOG(ERROR) << "Requested on a selection we don't support: "
                << XGetAtomName(x_display_, event.selection);
  }

  // Send off the reply.
  XSendEvent(x_display_, event.requestor, False, 0, &reply);
}

void Clipboard::AuraX11Details::HandleSelectionNotify(
    const XSelectionEvent& event) {
  if (!in_nested_loop_) {
    // This shouldn't happen; we're not waiting on the X server for data, but
    // any client can send any message...
    return;
  }

  if (current_selection_ == event.selection &&
      current_target_ == event.target) {
    returned_property_ = event.property;
  } else {
    // I am assuming that if some other client sent us a message after we've
    // asked for data, but it's malformed, we should just treat as if they sent
    // us an error message.
    returned_property_ = None;
  }

  quit_closure_.Run();
}

void Clipboard::AuraX11Details::HandleSelectionClear(
    const XSelectionClearEvent& event) {
  DLOG(ERROR) << "SelectionClear";

  // TODO(erg): If we receive a SelectionClear event while we're handling data,
  // we need to delay clearing.
}

void Clipboard::AuraX11Details::HandlePropertyNotify(
    const XPropertyEvent& event) {
  // TODO(erg): Must handle PropertyNotify events on our |x_window_| as part
  // of receiving data during paste.
}

bool Clipboard::AuraX11Details::Dispatch(const base::NativeEvent& event) {
  XEvent* xev = event;

  switch (xev->type) {
    case SelectionRequest:
      HandleSelectionRequest(xev->xselectionrequest);
      break;
    case SelectionNotify:
      HandleSelectionNotify(xev->xselection);
      break;
    case SelectionClear:
      HandleSelectionClear(xev->xselectionclear);
      break;
    case PropertyNotify:
      HandlePropertyNotify(xev->xproperty);
      break;
    default:
      break;
  }

  return true;
}

///////////////////////////////////////////////////////////////////////////////
// Clipboard

Clipboard::Clipboard()
    : aurax11_details_(new AuraX11Details) {
  DCHECK(CalledOnValidThread());
}

Clipboard::~Clipboard() {
  DCHECK(CalledOnValidThread());

  // TODO(erg): We need to do whatever the equivalent of
  // gtk_clipboard_store(clipboard_) is here. When we shut down, we want the
  // current selection to live on.
}

void Clipboard::WriteObjectsImpl(Buffer buffer,
                                 const ObjectMap& objects,
                                 SourceTag tag) {
  DCHECK(CalledOnValidThread());
  DCHECK(IsValidBuffer(buffer));

  aurax11_details_->CreateNewClipboardData();
  for (ObjectMap::const_iterator iter = objects.begin();
       iter != objects.end(); ++iter) {
    DispatchObject(static_cast<ObjectType>(iter->first), iter->second);
  }
  WriteSourceTag(tag);
  aurax11_details_->TakeOwnershipOfSelection(buffer);

  if (buffer == BUFFER_STANDARD) {
    ObjectMap::const_iterator text_iter = objects.find(CBF_TEXT);
    if (text_iter != objects.end()) {
      aurax11_details_->CreateNewClipboardData();
      const ObjectMapParam& char_vector = text_iter->second[0];
      WriteText(&char_vector.front(), char_vector.size());
      WriteSourceTag(tag);
      aurax11_details_->TakeOwnershipOfSelection(BUFFER_SELECTION);
    }
  }
}

bool Clipboard::IsFormatAvailable(const FormatType& format,
                                  Buffer buffer) const {
  DCHECK(CalledOnValidThread());
  DCHECK(IsValidBuffer(buffer));

  TargetList target_list = aurax11_details_->WaitAndGetTargetsList(buffer);
  return target_list.ContainsFormat(format);
}

void Clipboard::Clear(Buffer buffer) {
  DCHECK(CalledOnValidThread());
  DCHECK(IsValidBuffer(buffer));
  aurax11_details_->Clear(buffer);
}

void Clipboard::ReadAvailableTypes(Buffer buffer, std::vector<string16>* types,
    bool* contains_filenames) const {
  DCHECK(CalledOnValidThread());
  if (!types || !contains_filenames) {
    NOTREACHED();
    return;
  }

  TargetList target_list = aurax11_details_->WaitAndGetTargetsList(buffer);

  types->clear();

  if (target_list.ContainsText())
    types->push_back(UTF8ToUTF16(kMimeTypeText));
  if (target_list.ContainsFormat(GetHtmlFormatType()))
    types->push_back(UTF8ToUTF16(kMimeTypeHTML));
  if (target_list.ContainsFormat(GetRtfFormatType()))
    types->push_back(UTF8ToUTF16(kMimeTypeRTF));
  if (target_list.ContainsFormat(GetBitmapFormatType()))
    types->push_back(UTF8ToUTF16(kMimeTypePNG));
  *contains_filenames = false;

  scoped_ptr<SelectionData> data(aurax11_details_->RequestAndWaitForTypes(
      buffer,
      aurax11_details_->GetAtomsForFormat(GetWebCustomDataFormatType())));
  if (!data.get())
    return;

  ReadCustomDataTypes(data->data(), data->size(), types);
}

void Clipboard::ReadText(Buffer buffer, string16* result) const {
  DCHECK(CalledOnValidThread());

  scoped_ptr<SelectionData> data(aurax11_details_->RequestAndWaitForTypes(
      buffer, aurax11_details_->GetTextAtoms()));
  if (data.get()) {
    std::string text = data->GetText();
    *result = UTF8ToUTF16(text);
  }
}

void Clipboard::ReadAsciiText(Buffer buffer, std::string* result) const {
  DCHECK(CalledOnValidThread());

  scoped_ptr<SelectionData> data(aurax11_details_->RequestAndWaitForTypes(
      buffer, aurax11_details_->GetTextAtoms()));
  if (data.get())
    result->assign(data->GetText());
}

// TODO(estade): handle different charsets.
// TODO(port): set *src_url.
void Clipboard::ReadHTML(Buffer buffer,
                         string16* markup,
                         std::string* src_url,
                         uint32* fragment_start,
                         uint32* fragment_end) const {
  DCHECK(CalledOnValidThread());
  markup->clear();
  if (src_url)
    src_url->clear();
  *fragment_start = 0;
  *fragment_end = 0;

  scoped_ptr<SelectionData> data(aurax11_details_->RequestAndWaitForTypes(
      buffer, aurax11_details_->GetAtomsForFormat(GetHtmlFormatType())));
  if (!data.get())
    return;

  // If the data starts with 0xFEFF, i.e., Byte Order Mark, assume it is
  // UTF-16, otherwise assume UTF-8.
  if (data->size() >= 2 &&
      reinterpret_cast<const uint16_t*>(data->data())[0] == 0xFEFF) {
    markup->assign(reinterpret_cast<const uint16_t*>(data->data()) + 1,
                   (data->size() / 2) - 1);
  } else {
    UTF8ToUTF16(reinterpret_cast<const char*>(data->data()), data->size(),
                markup);
  }

  // If there is a terminating NULL, drop it.
  if (!markup->empty() && markup->at(markup->length() - 1) == '\0')
    markup->resize(markup->length() - 1);

  *fragment_start = 0;
  DCHECK(markup->length() <= kuint32max);
  *fragment_end = static_cast<uint32>(markup->length());
}

void Clipboard::ReadRTF(Buffer buffer, std::string* result) const {
  DCHECK(CalledOnValidThread());

  scoped_ptr<SelectionData> data(aurax11_details_->RequestAndWaitForTypes(
      buffer, aurax11_details_->GetAtomsForFormat(GetRtfFormatType())));
  if (data.get())
    data->AssignTo(result);
}

SkBitmap Clipboard::ReadImage(Buffer buffer) const {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
  return SkBitmap();
}

void Clipboard::ReadCustomData(Buffer buffer,
                               const string16& type,
                               string16* result) const {
  DCHECK(CalledOnValidThread());

  scoped_ptr<SelectionData> data(aurax11_details_->RequestAndWaitForTypes(
      buffer,
      aurax11_details_->GetAtomsForFormat(GetWebCustomDataFormatType())));
  if (!data.get())
    return;

  ReadCustomDataForType(data->data(), data->size(), type, result);
}

void Clipboard::ReadBookmark(string16* title, std::string* url) const {
  DCHECK(CalledOnValidThread());
  // TODO(erg): This was left NOTIMPLEMENTED() in the gtk port too.
  NOTIMPLEMENTED();
}

void Clipboard::ReadData(const FormatType& format, std::string* result) const {
  ReadDataImpl(BUFFER_STANDARD, format, result);
}

void Clipboard::ReadDataImpl(Buffer buffer,
                             const FormatType& format,
                             std::string* result) const {
  DCHECK(CalledOnValidThread());

  scoped_ptr<SelectionData> data(aurax11_details_->RequestAndWaitForTypes(
      buffer, aurax11_details_->GetAtomsForFormat(format)));
  if (data.get())
    data->AssignTo(result);
}

Clipboard::SourceTag Clipboard::ReadSourceTag(Buffer buffer) const {
  std::string result;
  ReadDataImpl(buffer, GetSourceTagFormatType(), &result);
  return Binary2SourceTag(result);
}

uint64 Clipboard::GetSequenceNumber(Buffer buffer) {
  DCHECK(CalledOnValidThread());
  if (buffer == BUFFER_STANDARD)
    return SelectionChangeObserver::GetInstance()->clipboard_sequence_number();
  else
    return SelectionChangeObserver::GetInstance()->primary_sequence_number();
}

void Clipboard::WriteText(const char* text_data, size_t text_len) {
  char* data = new char[text_len];
  memcpy(data, text_data, text_len);

  aurax11_details_->InsertMapping(kMimeTypeText, data, text_len);
  aurax11_details_->InsertMapping(kText, data, text_len);
  aurax11_details_->InsertMapping(kString, data, text_len);
  aurax11_details_->InsertMapping(kUtf8String, data, text_len);
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

  aurax11_details_->InsertMapping(kMimeTypeHTML, data, total_len);
}

void Clipboard::WriteRTF(const char* rtf_data, size_t data_len) {
  WriteData(GetRtfFormatType(), rtf_data, data_len);
}

void Clipboard::WriteBookmark(const char* title_data,
                              size_t title_len,
                              const char* url_data,
                              size_t url_len) {
  // Write as a mozilla url (UTF16: URL, newline, title).
  string16 url = UTF8ToUTF16(std::string(url_data, url_len) + "\n");
  string16 title = UTF8ToUTF16(std::string(title_data, title_len));
  int data_len = 2 * (title.length() + url.length());

  char* data = new char[data_len];
  memcpy(data, url.data(), 2 * url.length());
  memcpy(data + 2 * url.length(), title.data(), 2 * title.length());
  aurax11_details_->InsertMapping(kMimeTypeMozillaURL, data, data_len);
}

// Write an extra flavor that signifies WebKit was the last to modify the
// pasteboard. This flavor has no data.
void Clipboard::WriteWebSmartPaste() {
  aurax11_details_->InsertMapping(kMimeTypeWebkitSmartPaste, NULL, 0);
}

void Clipboard::WriteBitmap(const char* pixel_data, const char* size_data) {
  // TODO(erg): I'm not sure if we should be writting BMP data here or
  // not. It's what the GTK port does, but I'm not sure it's the right thing to
  // do.
  NOTIMPLEMENTED();
}

void Clipboard::WriteData(const FormatType& format,
                          const char* data_data,
                          size_t data_len) {
  // We assume that certain mapping types are only written by trusted code.
  // Therefore we must upkeep their integrity.
  if (format.Equals(GetBitmapFormatType()))
    return;
  char* data = new char[data_len];
  memcpy(data, data_data, data_len);
  aurax11_details_->InsertMapping(format.ToString(), data, data_len);
}

void Clipboard::WriteSourceTag(SourceTag tag) {
  if (tag != SourceTag()) {
    ObjectMapParam binary = SourceTag2Binary(tag);
    WriteData(GetSourceTagFormatType(), &binary[0], binary.size());
  }
}

// static
Clipboard::FormatType Clipboard::GetFormatType(
    const std::string& format_string) {
  return FormatType::Deserialize(format_string);
}

// static
const Clipboard::FormatType& Clipboard::GetUrlFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kMimeTypeURIList));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetUrlWFormatType() {
  return GetUrlFormatType();
}

// static
const Clipboard::FormatType& Clipboard::GetPlainTextFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kMimeTypeText));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetPlainTextWFormatType() {
  return GetPlainTextFormatType();
}

// static
const Clipboard::FormatType& Clipboard::GetFilenameFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kMimeTypeFilename));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetFilenameWFormatType() {
  return Clipboard::GetFilenameFormatType();
}

// static
const Clipboard::FormatType& Clipboard::GetHtmlFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kMimeTypeHTML));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetRtfFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kMimeTypeRTF));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetBitmapFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kMimeTypeBitmap));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetWebKitSmartPasteFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kMimeTypeWebkitSmartPaste));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetWebCustomDataFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kMimeTypeWebCustomData));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetPepperCustomDataFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kMimeTypePepperCustomData));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetSourceTagFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kSourceTagType));
  return type;
}

}  // namespace ui
