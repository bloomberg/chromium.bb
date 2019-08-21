/*
 * Copyright (C) 2018 Bloomberg Finance L.P.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS," WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef INCLUDED_BLPWTK2_DRAGDROP_H
#define INCLUDED_BLPWTK2_DRAGDROP_H

#include <memory>
#include <vector>

#include <content/public/common/drop_data.h>
#include <third_party/blink/public/platform/web_drag_operation.h>
#include <ui/base/dragdrop/drop_target_win.h>
#include <ui/gfx/geometry/point_f.h>
#include <ui/gfx/geometry/vector2d.h>

class SkBitmap;

namespace content {
struct DragEventSourceInfo;
}  // close namespace content

namespace blpwtk2 {

class DragDropDelegate {
  public:

    virtual ~DragDropDelegate();

    virtual void DragTargetEnter(
        const std::vector<content::DropData::Metadata>& drop_data,
        const gfx::PointF& client_pt,
        const gfx::PointF& screen_pt,
        blink::WebDragOperationsMask ops_allowed,
        int key_modifiers) = 0;

    virtual void DragTargetOver(
        const gfx::PointF& client_pt,
        const gfx::PointF& screen_pt,
        blink::WebDragOperationsMask ops_allowed,
        int key_modifiers) = 0;

    virtual void DragTargetLeave() = 0;

    virtual void DragTargetDrop(
        const content::DropData& drop_data,
        const gfx::PointF& client_pt,
        const gfx::PointF& screen_pt,
        int key_modifiers) = 0;

    virtual void DragSourceEnded(
        const gfx::PointF& client_pt,
        const gfx::PointF& screen_pt,
        blink::WebDragOperation drag_operation) = 0;

    virtual void DragSourceSystemEnded() = 0;
};

class DragDrop : public ui::DropTargetWin
{
  public:

    DragDrop(HWND hwnd, DragDropDelegate *delegate);
    ~DragDrop() override;

    void StartDragging(
        const content::DropData& drop_data,
        blink::WebDragOperationsMask operations_allowed,
        const SkBitmap& bitmap,
        const gfx::Vector2d& bitmap_offset_in_dip,
        const content::DragEventSourceInfo& event_info);
    void UpdateDragCursor(
        blink::WebDragOperation drag_operation);

    DWORD OnDragEnter(
        IDataObject* data_object,
        DWORD key_state,
        POINT screen_pt,
        DWORD effect) override;
    DWORD OnDragOver(
        IDataObject* data_object,
        DWORD key_state,
        POINT screen_pt,
        DWORD effect) override;
    void OnDragLeave(
        IDataObject* data_object) override;
    DWORD OnDrop(
        IDataObject* data_object,
        DWORD key_state,
        POINT screen_pt,
        DWORD effect) override;

  private:

    HWND d_hwnd;
    DragDropDelegate *d_delegate;

    blink::WebDragOperationsMask d_current_drag_operation = blink::kWebDragOperationNone;
};

}  // close namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_RENDERWEBVIEW_H
