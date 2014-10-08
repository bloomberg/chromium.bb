// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FileUploadControlPainter_h
#define FileUploadControlPainter_h

namespace blink {

struct PaintInfo;
class LayoutPoint;
class RenderFileUploadControl;

class FileUploadControlPainter {
public:
    FileUploadControlPainter(RenderFileUploadControl& renderFileUploadControl) : m_renderFileUploadControl(renderFileUploadControl) { }

    void paintObject(PaintInfo&, const LayoutPoint&);

private:
    RenderFileUploadControl& m_renderFileUploadControl;
};

} // namespace blink

#endif // FileUploadControlPainter_h
