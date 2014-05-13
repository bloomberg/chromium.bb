// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_UI_SCENE_VIEW_H_
#define REMOTING_IOS_UI_SCENE_VIEW_H_

#import <Foundation/Foundation.h>
#import <GLKit/GLKit.h>

#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

typedef struct {
  bool left;
  bool right;
  bool top;
  bool bottom;
} AnchorPosition;

typedef struct {
  int left;
  int right;
  int top;
  int bottom;
} MarginQuad;

typedef struct {
  CGPoint geometryVertex;
  CGPoint textureVertex;
} TexturedVertex;

typedef struct {
  TexturedVertex bl;
  TexturedVertex br;
  TexturedVertex tl;
  TexturedVertex tr;
} TexturedQuad;

@interface SceneView : NSObject {
 @private

  // GL name
  GLuint _textureId;

  GLKMatrix4 _projectionMatrix;
  GLKMatrix4 _modelViewMatrix;

  // The draw surface is a triangle strip (triangles defined by the intersecting
  // vertexes) to create a rectangle surface.
  // 1****3
  // |  / |
  // | /  |
  // 2****4
  // This also determines the resolution of our surface, being a unit (NxN) grid
  // with finite divisions.  For our surface N = 1, and the number of divisions
  // respects the CLIENT's desktop resolution.
  TexturedQuad _glQuad;

  // Cache of the CLIENT's desktop resolution.
  webrtc::DesktopSize _contentSize;
  // Cache of the HOST's desktop resolution.
  webrtc::DesktopSize _frameSize;

  // Location of the mouse according to the CLIENT in the prospective of the
  // HOST resolution
  webrtc::DesktopVector _mousePosition;

  // When a user pans they expect the view to experience acceleration after
  // they release the pan gesture.  We track that velocity vector as a position
  // delta factored over the frame rate of the GL Context.  Velocity is
  // accounted as a float.
  CGPoint _panVelocity;
}

// The position of the scene is tracked in the prospective of the CLIENT
// resolution.  The Z-axis is used to track the scale of the render, our scene
// never changes position on the Z-axis.
@property(nonatomic, readonly) GLKVector3 position;

// Space around border consumed by non-scene elements, we can not draw here
@property(nonatomic, readonly) MarginQuad margin;

@property(nonatomic, readonly) AnchorPosition anchored;

- (const GLKMatrix4&)projectionMatrix;

// calculate and return the current model view matrix
- (const GLKMatrix4&)modelViewMatrix;

- (const webrtc::DesktopSize&)contentSize;

// Update the CLIENT resolution and draw scene size, accounting for margins
- (void)setContentSize:(const CGSize&)size;

- (const webrtc::DesktopSize&)frameSize;

// Update the HOST resolution and reinitialize the scene positioning
- (void)setFrameSize:(const webrtc::DesktopSize&)size;

- (const webrtc::DesktopVector&)mousePosition;

- (void)setPanVelocity:(const CGPoint&)delta;

- (void)setMarginsFromLeft:(int)left
                     right:(int)right
                       top:(int)top
                    bottom:(int)bottom;

// Draws to a GL Context
- (void)draw;

- (BOOL)containsTouchPoint:(CGPoint)point;

// Applies translation and zoom.  Translation is bounded to screen edges.
// Zooming is bounded on the lower side to the maximum of width and height, and
// on the upper side by a constant, experimentally chosen.
- (void)panAndZoom:(CGPoint)translation scaleBy:(float)scale;

// Mouse is tracked in the perspective of the HOST desktop, but the projection
// to the user is in the perspective of the CLIENT resolution.  Find the HOST
// position that is the center of the current CLIENT view.  If the mouse is in
// the half of the CLIENT screen that is closest to an anchor, then move the
// mouse, otherwise the mouse should be centered.
- (void)updateMousePositionAndAnchorsWithTranslation:(CGPoint)translation
                                               scale:(float)scale;

// When zoom is changed the scene is translated to keep an anchored point
// (an anchored edge, or the spot the user is touching) at the same place in the
// User's perspective.  Return the delta of the position of the lower endpoint
// of the axis
+ (float)positionDeltaFromScaling:(float)ratio
                         position:(float)position
                           length:(float)length
                           anchor:(float)anchor;

// Return the delta of the position of the lower endpoint of the axis
+ (int)positionDeltaFromTranslation:(int)translation
                           position:(int)position
                          freeSpace:(int)freeSpace
              scaleingPositionDelta:(int)scaleingPositionDelta
                      isAnchoredLow:(BOOL)isAnchoredLow
                     isAnchoredHigh:(BOOL)isAnchoredHigh;

// |position + delta| is snapped to the bounds, return the delta in respect to
// the bounding.
+ (int)boundDeltaFromPosition:(float)position
                        delta:(int)delta
                   lowerBound:(int)lowerBound
                   upperBound:(int)upperBound;

// Return |nextPosition| when it is anchored and still in the respective 1/2 of
// the screen.  When |nextPosition| is outside scene's edge, snap to edge.
// Otherwise return |centerPosition|
+ (int)boundMouseGivenNextPosition:(int)nextPosition
                       maxPosition:(int)maxPosition
                    centerPosition:(int)centerPosition
                     isAnchoredLow:(BOOL)isAnchoredLow
                    isAnchoredHigh:(BOOL)isAnchoredHigh;

// If the mouse is at an edge return zero, otherwise return |velocity|
+ (float)boundVelocity:(float)velocity
            axisLength:(int)axisLength
         mousePosition:(int)mousePosition;

// Update the scene acceleration vector.
// Returns true if velocity before 'ticking' is non-zero.
- (BOOL)tickPanVelocity;

@end

#endif  // REMOTING_IOS_UI_SCENE_VIEW_H_