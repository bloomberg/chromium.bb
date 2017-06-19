/*
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2008 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef DataTransfer_h
#define DataTransfer_h

#include <memory>
#include "core/CoreExport.h"
#include "core/clipboard/DataObject.h"
#include "core/clipboard/DataTransferAccessPolicy.h"
#include "core/loader/resource/ImageResourceContent.h"
#include "core/page/DragActions.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/geometry/IntPoint.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Forward.h"

namespace blink {

class DataTransferItemList;
class DragImage;
class Element;
class FileList;
class FrameSelection;
class LocalFrame;
class Node;
class PaintRecordBuilder;
class PropertyTreeState;

// Used for drag and drop and copy/paste.
// Drag and Drop:
// http://www.whatwg.org/specs/web-apps/current-work/multipage/dnd.html
// Clipboard API (copy/paste):
// http://dev.w3.org/2006/webapi/clipops/clipops.html
class CORE_EXPORT DataTransfer final
    : public GarbageCollectedFinalized<DataTransfer>,
      public ScriptWrappable,
      public DataObject::Observer {
  USING_GARBAGE_COLLECTED_MIXIN(DataTransfer);
  DEFINE_WRAPPERTYPEINFO();

 public:
  // Whether this transfer is serving a drag-drop, copy-paste, spellcheck,
  // auto-correct or similar request.
  enum DataTransferType {
    kCopyAndPaste,
    kDragAndDrop,
    kInsertReplacementText,
  };

  static DataTransfer* Create();
  static DataTransfer* Create(DataTransferType,
                              DataTransferAccessPolicy,
                              DataObject*);
  ~DataTransfer();

  bool IsForCopyAndPaste() const { return transfer_type_ == kCopyAndPaste; }
  bool IsForDragAndDrop() const { return transfer_type_ == kDragAndDrop; }

  String dropEffect() const {
    return DropEffectIsUninitialized() ? "none" : drop_effect_;
  }
  void setDropEffect(const String&);
  bool DropEffectIsUninitialized() const {
    return drop_effect_ == "uninitialized";
  }
  String effectAllowed() const { return effect_allowed_; }
  void setEffectAllowed(const String&);

  void clearData(const String& type = String());
  String getData(const String& type) const;
  void setData(const String& type, const String& data);

  // Used by the bindings code to determine whether to call types() again.
  bool hasDataStoreItemListChanged() const;

  Vector<String> types();
  FileList* files() const;

  IntPoint DragLocation() const { return drag_loc_; }
  void setDragImage(Element*, int x, int y);
  void ClearDragImage();
  void SetDragImageResource(ImageResourceContent*, const IntPoint&);
  void SetDragImageElement(Node*, const IntPoint&);

  std::unique_ptr<DragImage> CreateDragImage(IntPoint& drag_location,
                                             LocalFrame*) const;
  void DeclareAndWriteDragImage(Element*,
                                const KURL& link_url,
                                const KURL& image_url,
                                const String& title);
  void WriteURL(Node*, const KURL&, const String&);
  void WriteSelection(const FrameSelection&);

  void SetAccessPolicy(DataTransferAccessPolicy);
  bool CanReadTypes() const;
  bool CanReadData() const;
  bool CanWriteData() const;
  // Note that the spec doesn't actually allow drag image modification outside
  // the dragstart event. This capability is maintained for backwards
  // compatiblity for ports that have supported this in the past. On many ports,
  // attempting to set a drag image outside the dragstart operation is a no-op
  // anyway.
  bool CanSetDragImage() const;

  DragOperation SourceOperation() const;
  DragOperation DestinationOperation() const;
  void SetSourceOperation(DragOperation);
  void SetDestinationOperation(DragOperation);

  bool HasDropZoneType(const String&);

  DataTransferItemList* items();

  DataObject* GetDataObject() const;

  static FloatRect DeviceSpaceBounds(const FloatRect, const LocalFrame&);
  static std::unique_ptr<DragImage> CreateDragImageForFrame(
      const LocalFrame&,
      float,
      RespectImageOrientationEnum,
      const FloatRect&,
      PaintRecordBuilder&,
      const PropertyTreeState&);
  static std::unique_ptr<DragImage> NodeImage(const LocalFrame&, Node&);

  DECLARE_TRACE();

 private:
  DataTransfer(DataTransferType, DataTransferAccessPolicy, DataObject*);

  void setDragImage(ImageResourceContent*, Node*, const IntPoint&);

  bool HasFileOfType(const String&) const;
  bool HasStringOfType(const String&) const;

  // DataObject::Observer override.
  void OnItemListChanged() override;

  // Instead of using this member directly, prefer to use the can*() methods
  // above.
  DataTransferAccessPolicy policy_;
  String drop_effect_;
  String effect_allowed_;
  DataTransferType transfer_type_;
  Member<DataObject> data_object_;

  bool data_store_item_list_changed_;

  IntPoint drag_loc_;
  Member<ImageResourceContent> drag_image_;
  Member<Node> drag_image_element_;
};

DragOperation ConvertDropZoneOperationToDragOperation(
    const String& drag_operation);
String ConvertDragOperationToDropZoneOperation(DragOperation);

}  // namespace blink

#endif  // DataTransfer_h
