// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/ios/ui/scene_view.h"

#import "remoting/ios/utility.h"

namespace {

// TODO (aboone) Some of the layout is not yet set in stone, so variables have
// been used to position and turn items on and off.  Eventually these may be
// stabilized and removed.

// Scroll speed multiplier for swiping
const static int kMouseSensitivity = 2.5;

// Input Axis inversion
// 1 for standard, -1 for inverted
const static int kXAxisInversion = -1;
const static int kYAxisInversion = -1;

// Experimental value for bounding the maximum zoom ratio
const static int kMaxZoomSize = 3;
}  // namespace

@interface SceneView (Private)
// Returns the number of pixels displayed per device pixel when the scaling is
// such that the entire frame would fit perfectly in content.  Note the ratios
// are different for width and height, some people have multiple monitors, some
// have 16:9 or 4:3 while iPad is always single screen, but different iOS
// devices have different resolutions.
- (CGPoint)pixelRatio;

// Return the FrameSize in perspective of the CLIENT resolution
- (webrtc::DesktopSize)frameSizeToScale:(float)scale;

// When bounded on the top and right, this point is where the scene must be
// positioned given a scene size
- (webrtc::DesktopVector)getBoundsForSize:(const webrtc::DesktopSize&)size;

// Converts a point in the the CLIENT resolution to a similar point in the HOST
// resolution.  Additionally, CLIENT resolution is expressed in float values
// while HOST operates in integer values.
- (BOOL)convertTouchPointToMousePoint:(CGPoint)touchPoint
                          targetPoint:(webrtc::DesktopVector&)desktopPoint;

// Converts a point in the the HOST resolution to a similar point in the CLIENT
// resolution.  Additionally, CLIENT resolution is expressed in float values
// while HOST operates in integer values.
- (BOOL)convertMousePointToTouchPoint:(const webrtc::DesktopVector&)mousePoint
                          targetPoint:(CGPoint&)touchPoint;
@end

@implementation SceneView

- (id)init {
  self = [super init];
  if (self) {

    _frameSize = webrtc::DesktopSize(1, 1);
    _contentSize = webrtc::DesktopSize(1, 1);
    _mousePosition = webrtc::DesktopVector(0, 0);

    _position = GLKVector3Make(0, 0, 1);
    _margin.left = 0;
    _margin.right = 0;
    _margin.top = 0;
    _margin.bottom = 0;
    _anchored.left = false;
    _anchored.right = false;
    _anchored.top = false;
    _anchored.bottom = false;
  }
  return self;
}

- (const GLKMatrix4&)projectionMatrix {
  return _projectionMatrix;
}

- (const GLKMatrix4&)modelViewMatrix {
  // Start by using the entire scene
  _modelViewMatrix = GLKMatrix4Identity;

  // Position scene according to any panning or bounds
  _modelViewMatrix = GLKMatrix4Translate(_modelViewMatrix,
                                         _position.x + _margin.left,
                                         _position.y + _margin.bottom,
                                         0.0);

  // Apply zoom
  _modelViewMatrix = GLKMatrix4Scale(_modelViewMatrix,
                                     _position.z / self.pixelRatio.x,
                                     _position.z / self.pixelRatio.y,
                                     1.0);

  // We are directly above the screen and looking down.
  static const GLKMatrix4 viewMatrix = GLKMatrix4MakeLookAt(
      0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0);  // center view

  _modelViewMatrix = GLKMatrix4Multiply(viewMatrix, _modelViewMatrix);

  return _modelViewMatrix;
}

- (const webrtc::DesktopSize&)contentSize {
  return _contentSize;
}

- (void)setContentSize:(const CGSize&)size {

  _contentSize.set(size.width, size.height);

  _projectionMatrix = GLKMatrix4MakeOrtho(
      0.0, _contentSize.width(), 0.0, _contentSize.height(), 1.0, -1.0);

  TexturedQuad newQuad;
  newQuad.bl.geometryVertex = CGPointMake(0.0, 0.0);
  newQuad.br.geometryVertex = CGPointMake(_contentSize.width(), 0.0);
  newQuad.tl.geometryVertex = CGPointMake(0.0, _contentSize.height());
  newQuad.tr.geometryVertex =
      CGPointMake(_contentSize.width(), _contentSize.height());

  newQuad.bl.textureVertex = CGPointMake(0.0, 1.0);
  newQuad.br.textureVertex = CGPointMake(1.0, 1.0);
  newQuad.tl.textureVertex = CGPointMake(0.0, 0.0);
  newQuad.tr.textureVertex = CGPointMake(1.0, 0.0);

  _glQuad = newQuad;
}

- (const webrtc::DesktopSize&)frameSize {
  return _frameSize;
}

- (void)setFrameSize:(const webrtc::DesktopSize&)size {
  DCHECK(size.width() > 0 && size.height() > 0);
  // Don't do anything if the size has not changed.
  if (_frameSize.equals(size))
    return;

  _frameSize.set(size.width(), size.height());

  _position.x = 0;
  _position.y = 0;

  float verticalPixelScaleRatio =
      (static_cast<float>(_contentSize.height() - _margin.top -
                          _margin.bottom) /
       static_cast<float>(_frameSize.height())) /
      _position.z;

  // Anchored at the position (0,0)
  _anchored.left = YES;
  _anchored.right = NO;
  _anchored.top = NO;
  _anchored.bottom = YES;

  [self panAndZoom:CGPointMake(0.0, 0.0) scaleBy:verticalPixelScaleRatio];

  // Center the mouse on the CLIENT screen
  webrtc::DesktopVector centerMouseLocation;
  if ([self convertTouchPointToMousePoint:CGPointMake(_contentSize.width() / 2,
                                                      _contentSize.height() / 2)
                              targetPoint:centerMouseLocation]) {
    _mousePosition.set(centerMouseLocation.x(), centerMouseLocation.y());
  }

#if DEBUG
  NSLog(@"resized frame:%d:%d scale:%f",
        _frameSize.width(),
        _frameSize.height(),
        _position.z);
#endif  // DEBUG
}

- (const webrtc::DesktopVector&)mousePosition {
  return _mousePosition;
}

- (void)setPanVelocity:(const CGPoint&)delta {
  _panVelocity.x = delta.x;
  _panVelocity.y = delta.y;
}

- (void)setMarginsFromLeft:(int)left
                     right:(int)right
                       top:(int)top
                    bottom:(int)bottom {
  _margin.left = left;
  _margin.right = right;
  _margin.top = top;
  _margin.bottom = bottom;
}

- (void)draw {
  glEnableVertexAttribArray(GLKVertexAttribPosition);
  glEnableVertexAttribArray(GLKVertexAttribTexCoord0);
  glEnableVertexAttribArray(GLKVertexAttribTexCoord1);

  // Define our scene space
  glVertexAttribPointer(GLKVertexAttribPosition,
                        2,
                        GL_FLOAT,
                        GL_FALSE,
                        sizeof(TexturedVertex),
                        &(_glQuad.bl.geometryVertex));
  // Define the desktop plane
  glVertexAttribPointer(GLKVertexAttribTexCoord0,
                        2,
                        GL_FLOAT,
                        GL_FALSE,
                        sizeof(TexturedVertex),
                        &(_glQuad.bl.textureVertex));
  // Define the cursor plane
  glVertexAttribPointer(GLKVertexAttribTexCoord1,
                        2,
                        GL_FLOAT,
                        GL_FALSE,
                        sizeof(TexturedVertex),
                        &(_glQuad.bl.textureVertex));

  // Draw!
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

  [Utility logGLErrorCode:@"SceneView draw"];
}

- (CGPoint)pixelRatio {

  CGPoint r = CGPointMake(static_cast<float>(_contentSize.width()) /
                              static_cast<float>(_frameSize.width()),
                          static_cast<float>(_contentSize.height()) /
                              static_cast<float>(_frameSize.height()));
  return r;
}

- (webrtc::DesktopSize)frameSizeToScale:(float)scale {
  return webrtc::DesktopSize(_frameSize.width() * scale,
                             _frameSize.height() * scale);
}

- (webrtc::DesktopVector)getBoundsForSize:(const webrtc::DesktopSize&)size {
  webrtc::DesktopVector r(
      _contentSize.width() - _margin.left - _margin.right - size.width(),
      _contentSize.height() - _margin.bottom - _margin.top - size.height());

  if (r.x() > 0) {
    r.set((_contentSize.width() - size.width()) / 2, r.y());
  }

  if (r.y() > 0) {
    r.set(r.x(), (_contentSize.height() - size.height()) / 2);
  }

  return r;
}

- (BOOL)containsTouchPoint:(CGPoint)point {
  // Here frame is from the top-left corner, most other calculations are framed
  // from the bottom left.
  CGRect frame =
      CGRectMake(_margin.left,
                 _margin.top,
                 _contentSize.width() - _margin.left - _margin.right,
                 _contentSize.height() - _margin.top - _margin.bottom);
  return CGRectContainsPoint(frame, point);
}

- (BOOL)convertTouchPointToMousePoint:(CGPoint)touchPoint
                          targetPoint:(webrtc::DesktopVector&)mousePoint {
  if (![self containsTouchPoint:touchPoint]) {
    return NO;
  }
  // A touch location occurs in respect to the user's entire view surface.

  // The GL Context is upside down from the User's perspective so flip it.
  CGPoint glOrientedTouchPoint =
      CGPointMake(touchPoint.x, _contentSize.height() - touchPoint.y);

  // The GL surface generally is not at the same origination point as the touch,
  // so translate by the scene's position.
  CGPoint glOrientedPointInRespectToFrame =
      CGPointMake(glOrientedTouchPoint.x - _position.x,
                  glOrientedTouchPoint.y - _position.y);

  // The perspective exists in relative to the CLIENT resolution at 1:1, zoom
  // our perspective so we are relative to the HOST at 1:1
  CGPoint glOrientedPointInFrame =
      CGPointMake(glOrientedPointInRespectToFrame.x / _position.z,
                  glOrientedPointInRespectToFrame.y / _position.z);

  // Finally, flip the perspective back over to the Users, but this time in
  // respect to the HOST desktop.  Floor to ensure the result is always in
  // frame.
  CGPoint deskTopOrientedPointInFrame =
      CGPointMake(floorf(glOrientedPointInFrame.x),
                  floorf(_frameSize.height() - glOrientedPointInFrame.y));

  // Convert from float to integer
  mousePoint.set(deskTopOrientedPointInFrame.x, deskTopOrientedPointInFrame.y);

  return CGRectContainsPoint(
      CGRectMake(0, 0, _frameSize.width(), _frameSize.height()),
      deskTopOrientedPointInFrame);
}

- (BOOL)convertMousePointToTouchPoint:(const webrtc::DesktopVector&)mousePoint
                          targetPoint:(CGPoint&)touchPoint {
  // A mouse point is in respect to the desktop frame.

  // Flip the perspective back over to the Users, in
  // respect to the HOST desktop.
  CGPoint deskTopOrientedPointInFrame =
      CGPointMake(mousePoint.x(), _frameSize.height() - mousePoint.y());

  // The perspective exists in relative to the CLIENT resolution at 1:1, zoom
  // our perspective so we are relative to the HOST at 1:1
  CGPoint glOrientedPointInFrame =
      CGPointMake(deskTopOrientedPointInFrame.x * _position.z,
                  deskTopOrientedPointInFrame.y * _position.z);

  // The GL surface generally is not at the same origination point as the touch,
  // so translate by the scene's position.
  CGPoint glOrientedPointInRespectToFrame =
      CGPointMake(glOrientedPointInFrame.x + _position.x,
                  glOrientedPointInFrame.y + _position.y);

  // Convert from float to integer
  touchPoint.x = floorf(glOrientedPointInRespectToFrame.x);
  touchPoint.y = floorf(glOrientedPointInRespectToFrame.y);

  return [self containsTouchPoint:touchPoint];
}

- (void)panAndZoom:(CGPoint)translation scaleBy:(float)ratio {
  CGPoint ratios = [self pixelRatio];

  // New Scaling factor bounded by a min and max
  float resultScale = _position.z * ratio;
  float scaleUpperBound = MAX(ratios.x, MAX(ratios.y, kMaxZoomSize));
  float scaleLowerBound = MIN(ratios.x, ratios.y);

  if (resultScale < scaleLowerBound) {
    resultScale = scaleLowerBound;
  } else if (resultScale > scaleUpperBound) {
    resultScale = scaleUpperBound;
  }

  DCHECK(isnormal(resultScale) && resultScale > 0);

  // The GL perspective is upside down in relation to the User's view, so flip
  // the translation
  translation.y = -translation.y;

  // The constants here could be user options later.
  translation.x =
      translation.x * kXAxisInversion * (1 / (ratios.x * kMouseSensitivity));
  translation.y =
      translation.y * kYAxisInversion * (1 / (ratios.y * kMouseSensitivity));

  CGPoint delta = CGPointMake(0, 0);
  CGPoint scaleDelta = CGPointMake(0, 0);

  webrtc::DesktopSize currentSize = [self frameSizeToScale:_position.z];

  {
    // Closure for this variable, so the variable is not available to the rest
    // of this function
    webrtc::DesktopVector currentBounds = [self getBoundsForSize:currentSize];
    // There are rounding errors in the scope of this function, see the
    // butterfly effect.  In successive calls, the resulting position isn't
    // always exactly the calculated position. If we know we are Anchored, then
    // go ahead and reposition it to the values above.
    if (_anchored.right) {
      _position.x = currentBounds.x();
    }

    if (_anchored.top) {
      _position.y = currentBounds.y();
    }
  }

  if (_position.z != resultScale) {
    // When scaling the scene, the origination of scaling is the mouse's
    // location.  But when the frame is anchored, adjust the origination to the
    // anchor point.

    CGPoint mousePositionInClientResolution;
    [self convertMousePointToTouchPoint:_mousePosition
                            targetPoint:mousePositionInClientResolution];

    // Prefer to zoom based on the left anchor when there is a choice
    if (_anchored.left) {
      mousePositionInClientResolution.x = 0;
    } else if (_anchored.right) {
      mousePositionInClientResolution.x = _contentSize.width();
    }

    // Prefer to zoom out from the top anchor when there is a choice
    if (_anchored.top) {
      mousePositionInClientResolution.y = _contentSize.height();
    } else if (_anchored.bottom) {
      mousePositionInClientResolution.y = 0;
    }

    scaleDelta.x -=
        [SceneView positionDeltaFromScaling:ratio
                                   position:_position.x
                                     length:currentSize.width()
                                     anchor:mousePositionInClientResolution.x];

    scaleDelta.y -=
        [SceneView positionDeltaFromScaling:ratio
                                   position:_position.y
                                     length:currentSize.height()
                                     anchor:mousePositionInClientResolution.y];
  }

  delta.x = [SceneView
      positionDeltaFromTranslation:translation.x
                          position:_position.x
                         freeSpace:_contentSize.width() - currentSize.width()
             scaleingPositionDelta:scaleDelta.x
                     isAnchoredLow:_anchored.left
                    isAnchoredHigh:_anchored.right];

  delta.y = [SceneView
      positionDeltaFromTranslation:translation.y
                          position:_position.y
                         freeSpace:_contentSize.height() - currentSize.height()
             scaleingPositionDelta:scaleDelta.y
                     isAnchoredLow:_anchored.bottom
                    isAnchoredHigh:_anchored.top];
  {
    // Closure for this variable, so the variable is not available to the rest
    // of this function
    webrtc::DesktopVector bounds =
        [self getBoundsForSize:[self frameSizeToScale:resultScale]];

    delta.x = [SceneView boundDeltaFromPosition:_position.x
                                          delta:delta.x
                                     lowerBound:bounds.x()
                                     upperBound:0];

    delta.y = [SceneView boundDeltaFromPosition:_position.y
                                          delta:delta.y
                                     lowerBound:bounds.y()
                                     upperBound:0];
  }

  BOOL isLeftAndRightAnchored = _anchored.left && _anchored.right;
  BOOL isTopAndBottomAnchored = _anchored.top && _anchored.bottom;

  [self updateMousePositionAndAnchorsWithTranslation:translation
                                               scale:resultScale];

  // If both anchors were lost, then keep the one that is easier to predict
  if (isLeftAndRightAnchored && !_anchored.left && !_anchored.right) {
    delta.x = -_position.x;
    _anchored.left = YES;
  }

  // If both anchors were lost, then keep the one that is easier to predict
  if (isTopAndBottomAnchored && !_anchored.top && !_anchored.bottom) {
    delta.y = -_position.y;
    _anchored.bottom = YES;
  }

  // FINALLY, update the scene's position
  _position.x += delta.x;
  _position.y += delta.y;
  _position.z = resultScale;
}

- (void)updateMousePositionAndAnchorsWithTranslation:(CGPoint)translation
                                               scale:(float)scale {
  webrtc::DesktopVector centerMouseLocation;
  [self convertTouchPointToMousePoint:CGPointMake(_contentSize.width() / 2,
                                                  _contentSize.height() / 2)
                          targetPoint:centerMouseLocation];

  webrtc::DesktopVector currentBounds =
      [self getBoundsForSize:[self frameSizeToScale:_position.z]];
  webrtc::DesktopVector nextBounds =
      [self getBoundsForSize:[self frameSizeToScale:scale]];

  webrtc::DesktopVector predictedMousePosition(
      _mousePosition.x() - translation.x, _mousePosition.y() + translation.y);

  _mousePosition.set(
      [SceneView boundMouseGivenNextPosition:predictedMousePosition.x()
                                 maxPosition:_frameSize.width()
                              centerPosition:centerMouseLocation.x()
                               isAnchoredLow:_anchored.left
                              isAnchoredHigh:_anchored.right],
      [SceneView boundMouseGivenNextPosition:predictedMousePosition.y()
                                 maxPosition:_frameSize.height()
                              centerPosition:centerMouseLocation.y()
                               isAnchoredLow:_anchored.top
                              isAnchoredHigh:_anchored.bottom]);

  _panVelocity.x = [SceneView boundVelocity:_panVelocity.x
                                 axisLength:_frameSize.width()
                              mousePosition:_mousePosition.x()];
  _panVelocity.y = [SceneView boundVelocity:_panVelocity.y
                                 axisLength:_frameSize.height()
                              mousePosition:_mousePosition.y()];

  _anchored.left = (nextBounds.x() >= 0) ||
                   (_position.x == 0 &&
                    predictedMousePosition.x() <= centerMouseLocation.x());

  _anchored.right =
      (nextBounds.x() >= 0) ||
      (_position.x == currentBounds.x() &&
       predictedMousePosition.x() >= centerMouseLocation.x()) ||
      (_mousePosition.x() == _frameSize.width() - 1 && !_anchored.left);

  _anchored.bottom = (nextBounds.y() >= 0) ||
                     (_position.y == 0 &&
                      predictedMousePosition.y() >= centerMouseLocation.y());

  _anchored.top =
      (nextBounds.y() >= 0) ||
      (_position.y == currentBounds.y() &&
       predictedMousePosition.y() <= centerMouseLocation.y()) ||
      (_mousePosition.y() == _frameSize.height() - 1 && !_anchored.bottom);
}

+ (float)positionDeltaFromScaling:(float)ratio
                         position:(float)position
                           length:(float)length
                           anchor:(float)anchor {
  float newSize = length * ratio;
  float scaleXBy = fabs(position - anchor) / length;
  float delta = (newSize - length) * scaleXBy;
  return delta;
}

+ (int)positionDeltaFromTranslation:(int)translation
                           position:(int)position
                          freeSpace:(int)freeSpace
              scaleingPositionDelta:(int)scaleingPositionDelta
                      isAnchoredLow:(BOOL)isAnchoredLow
                     isAnchoredHigh:(BOOL)isAnchoredHigh {
  if (isAnchoredLow && isAnchoredHigh) {
    // center the view
    return (freeSpace / 2) - position;
  } else if (isAnchoredLow) {
    return 0;
  } else if (isAnchoredHigh) {
    return scaleingPositionDelta;
  } else {
    return translation + scaleingPositionDelta;
  }
}

+ (int)boundDeltaFromPosition:(float)position
                        delta:(int)delta
                   lowerBound:(int)lowerBound
                   upperBound:(int)upperBound {
  int result = position + delta;

  if (lowerBound < upperBound) {  // the view is larger than the bounds
    if (result > upperBound) {
      result = upperBound;
    } else if (result < lowerBound) {
      result = lowerBound;
    }
  } else {
    // the view is smaller than the bounds so we'll always be at the lowerBound
    result = lowerBound;
  }
  return result - position;
}

+ (int)boundMouseGivenNextPosition:(int)nextPosition
                       maxPosition:(int)maxPosition
                    centerPosition:(int)centerPosition
                     isAnchoredLow:(BOOL)isAnchoredLow
                    isAnchoredHigh:(BOOL)isAnchoredHigh {
  if (nextPosition < 0) {
    return 0;
  }
  if (nextPosition > maxPosition - 1) {
    return maxPosition - 1;
  }

  if ((isAnchoredLow && nextPosition <= centerPosition) ||
      (isAnchoredHigh && nextPosition >= centerPosition)) {
    return nextPosition;
  }

  return centerPosition;
}

+ (float)boundVelocity:(float)velocity
            axisLength:(int)axisLength
         mousePosition:(int)mousePosition {
  if (velocity != 0) {
    if (mousePosition <= 0 || mousePosition >= (axisLength - 1)) {
      return 0;
    }
  }

  return velocity;
}

- (BOOL)tickPanVelocity {
  BOOL inMotion = ((_panVelocity.x != 0.0) || (_panVelocity.y != 0.0));

  if (inMotion) {

    uint32_t divisor = 50 / _position.z;
    float reducer = .95;

    if (_panVelocity.x != 0.0 && ABS(_panVelocity.x) < divisor) {
      _panVelocity = CGPointMake(0.0, _panVelocity.y);
    }

    if (_panVelocity.y != 0.0 && ABS(_panVelocity.y) < divisor) {
      _panVelocity = CGPointMake(_panVelocity.x, 0.0);
    }

    [self panAndZoom:CGPointMake(_panVelocity.x / divisor,
                                 _panVelocity.y / divisor)
             scaleBy:1.0];

    _panVelocity.x *= reducer;
    _panVelocity.y *= reducer;
  }

  return inMotion;
}

@end