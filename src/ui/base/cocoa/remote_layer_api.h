// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_REMOTE_LAYER_API_H_
#define UI_BASE_COCOA_REMOTE_LAYER_API_H_

#if defined(__OBJC__)
#import <Cocoa/Cocoa.h>
#endif  // __OBJC__

#include <stdint.h>

#include "ui/base/ui_base_export.h"

// The CGSConnectionID is used to create the CAContext in the process that is
// going to share the CALayers that it is rendering to another process to
// display.
extern "C" {
typedef uint32_t CGSConnectionID;
CGSConnectionID CGSMainConnectionID(void);
};

// The CAContextID type identifies a CAContext across processes. This is the
// token that is passed from the process that is sharing the CALayer that it is
// rendering to the process that will be displaying that CALayer.
typedef uint32_t CAContextID;

#if defined(__OBJC__)

// The CAContext has a static CAContextID which can be sent to another process.
// When a CALayerHost is created using that CAContextID in another process, the
// content displayed by that CALayerHost will be the content of the CALayer
// that is set as the |layer| property on the CAContext.
@interface CAContext : NSObject
+ (id)contextWithCGSConnection:(CAContextID)contextId
                       options:(NSDictionary*)optionsDict;
@property(readonly) CAContextID contextId;
@property(retain) CALayer *layer;
@end

// The CALayerHost is created in the process that will display the content
// being rendered by another process. Setting the |contextId| property on
// an object of this class will make this layer display the content of the
// CALayer that is set to the CAContext with that CAContextID in the layer
// sharing process.
@interface CALayerHost : CALayer
@property CAContextID contextId;
@end

#endif // __OBJC__

namespace ui {

// This function will check if all of the interfaces listed above are supported
// on the system, and return true if they are.
bool UI_BASE_EXPORT RemoteLayerAPISupported();

}  // namespace ui

#endif  // UI_BASE_COCOA_REMOTE_LAYER_API_H_
