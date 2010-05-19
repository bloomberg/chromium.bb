/*
 * Copyright 2010, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "overlay_window_mac.h"

#import "plugin/mac/graphics_utils_mac.h"

#ifdef O3D_PLUGIN_ENABLE_FULLSCREEN_MSG

#define kTransitionTime 1.0

namespace o3d {

// A little wrapper for ATSUSetAttributes to make calling it with one attribute
// less annoying.
static void MySetAttribute(ATSUStyle style,
                           ATSUAttributeTag tag,
                           ByteCount size,
                           ATSUAttributeValuePtr value) {

  ATSUAttributeTag      tags[2]   = {tag, 0};
  ByteCount             sizes[2]  = {size, 0};
  ATSUAttributeValuePtr values[2] = {value, 0};

  ATSUSetAttributes(style, 1, tags, sizes, values);
}

// A little wrapper for ATSUSetLayoutControls to make calling it with one
// attribute less annoying.
static void MySetLayoutControl(ATSUTextLayout layout,
                               ATSUAttributeTag tag,
                               ByteCount size,
                               ATSUAttributeValuePtr value) {

  ATSUAttributeTag      tags[2]   = {tag, 0};
  ByteCount             sizes[2]  = {size, 0};
  ATSUAttributeValuePtr values[2] = {value, 0};

  ATSUSetLayoutControls(layout, 1, tags, sizes, values);
}

// Returns the unicode 16 chars that we need to display as the fullscreen
// message. Should be disposed with free() after use.
static UniChar * GetFullscreenDisplayText(int *returned_length) {
  // TODO this will need to be a localized string.
  NSString* ns_display_text = @"Press ESC to exit fullscreen";
  int count = [ns_display_text length];
  UniChar* display_text_16 = (UniChar*) calloc(count, sizeof(UniChar));

  [ns_display_text getCharacters:display_text_16];
  *returned_length = count;
  return display_text_16;
}


static void DrawToOverlayWindow(WindowRef overlayWindow) {
  CGContextRef overlayContext = NULL;
  CGFloat kWhiteOpaque[]  = {1.0, 1.0, 1.0, 1.0};
  CGFloat kBlackNotOpaque[]  = {0.0, 0.0, 0.0, 0.5};
  Rect bounds = {0, 0, 0, 0};
  const char* kOverlayWindowFontName = "Arial";
  const int kPointSize  = 22;
  const float kShadowRadius = 5.0;
  const float kRoundRectRadius = 9.0;
  const float kTextLeftMargin = 15.0;
  const float kTextBottomMargin = 22.0;

  QDBeginCGContext(GetWindowPort(overlayWindow), &overlayContext);
  GetWindowBounds(overlayWindow, kWindowContentRgn, &bounds);

  // Make the global rect local.
  bounds.right -= bounds.left;
  bounds.left = 0;
  bounds.bottom -= bounds.top;
  bounds.top = 0;

  CGRect cgTotalRect = Rect2CGRect(bounds);
  CGContextSetShouldSmoothFonts(overlayContext, true);
  CGContextClearRect(overlayContext, cgTotalRect);

  CGColorSpaceRef myColorSpace = CGColorSpaceCreateDeviceRGB();
  CGColorRef shadow = CGColorCreate(myColorSpace, kBlackNotOpaque);
  CGColorRef roundRectBackColor = CGColorCreate(myColorSpace, kBlackNotOpaque);
  CGSize shadowOffset = {0.0,0.0};

  CGContextSetFillColor(overlayContext, kWhiteOpaque);
  CGContextSetStrokeColor(overlayContext, kWhiteOpaque);

    // Draw the round rect background.
  CGContextSaveGState(overlayContext);
  CGContextSetFillColorWithColor(overlayContext, roundRectBackColor);
  CGRect cg_rounded_area =
      CGRectMake(// Offset from left and bottom to give shadow its space.
                 kShadowRadius, kShadowRadius,
                 // Increase width and height so rounded corners
                 // will be clipped out, except at bottom left.
                 (bounds.right - bounds.left) + 30,
                 (bounds.bottom - bounds.top) + 30);
  // Save state before applying shadow.
  CGContextSetShadowWithColor(overlayContext, shadowOffset,
                              kShadowRadius, shadow);
  PaintRoundedCGRect(overlayContext, cg_rounded_area, kRoundRectRadius, true);
  // Restore graphics state to remove shadow.
  CGContextRestoreGState(overlayContext);

  // Draw the text.
  int text_length = 0;
  UniChar* display_text = GetFullscreenDisplayText(&text_length);

  if ((text_length > 0) && (display_text != NULL)) {
    ATSUStyle         style;
    ATSUTextLayout    layout;
    ATSUFontID        font;
    Fixed             pointSize = Long2Fix(kPointSize);
    Boolean           is_bold = true;

    ATSUCreateStyle(&style);
    ATSUFindFontFromName(kOverlayWindowFontName, strlen(kOverlayWindowFontName),
                         kFontFullName, kFontNoPlatformCode, kFontNoScriptCode,
                         kFontNoLanguageCode, &font);

    MySetAttribute(style, kATSUFontTag, sizeof(font), &font);
    MySetAttribute(style, kATSUSizeTag, sizeof(pointSize), &pointSize);
    MySetAttribute(style, kATSUQDBoldfaceTag, sizeof(Boolean), &is_bold);


    ATSUCreateTextLayout(&layout);
    ATSUSetTextPointerLocation(layout, display_text,
                               kATSUFromTextBeginning, kATSUToTextEnd,
                               text_length);
    ATSUSetRunStyle(layout, style, kATSUFromTextBeginning, kATSUToTextEnd);

    MySetLayoutControl(layout, kATSUCGContextTag,
                       sizeof(CGContextRef),  &overlayContext);

    // Need to enable this for languages like Japanese to draw as something
    // other than a series of squares.
    ATSUSetTransientFontMatching(layout, true);


    CGContextSetFillColor(overlayContext, kWhiteOpaque);
    ATSUDrawText(layout, kATSUFromTextBeginning, kATSUToTextEnd,
                 X2Fix(kShadowRadius + kTextLeftMargin),
                 X2Fix(kShadowRadius + kTextBottomMargin));
    ATSUDisposeStyle(style);
    ATSUDisposeTextLayout(layout);
    free(display_text);
  }

  CGColorRelease(roundRectBackColor);
  CGColorRelease(shadow);
  CGColorSpaceRelease (myColorSpace);

  QDEndCGContext(GetWindowPort(overlayWindow), &overlayContext);
}

static OSStatus HandleOverlayWindow(EventHandlerCallRef inHandlerCallRef,
                                    EventRef inEvent,
                                    void *inUserData) {
  OSType event_class = GetEventClass(inEvent);
  OSType event_kind = GetEventKind(inEvent);

  if (event_class == kEventClassWindow &&
      event_kind == kEventWindowPaint) {
      WindowRef theWindow = NULL;
    GetEventParameter(inEvent, kEventParamDirectObject,
                      typeWindowRef, NULL,
                      sizeof(theWindow), NULL,
                      &theWindow);
    if (theWindow) {
      CallNextEventHandler(inHandlerCallRef, inEvent);
      DrawToOverlayWindow(theWindow);
    }
  }

  return noErr;
}



static Rect GetOverlayWindowRect(bool visible) {
#define kOverlayHeight 60
#define kOverlayWidth 340
  Rect screen_bounds = CGRect2Rect(CGDisplayBounds(CGMainDisplayID()));
  Rect hidden_window_bounds = {screen_bounds.top - kOverlayHeight,
                               screen_bounds.right - kOverlayWidth,
                               screen_bounds.top,
                               screen_bounds.right};
  Rect visible_window_bounds = {screen_bounds.top,
                                screen_bounds.right - kOverlayWidth,
                                screen_bounds.top + kOverlayHeight,
                                screen_bounds.right};

  return (visible) ? visible_window_bounds : hidden_window_bounds;
}


static WindowRef CreateOverlayWindow(void) {
  Rect        window_bounds = GetOverlayWindowRect(false);
  WindowClass wClass = kOverlayWindowClass;
  WindowRef   window = NULL;
  OSStatus    err = noErr;
  WindowAttributes  overlayAttributes = kWindowNoShadowAttribute |
                                        kWindowIgnoreClicksAttribute |
                                        kWindowNoActivatesAttribute |
                                        kWindowStandardHandlerAttribute;
  EventTypeSpec  eventTypes[] = {
    {kEventClassWindow, kEventWindowPaint},
    {kEventClassWindow, kEventWindowShown}
  };

  err = CreateNewWindow(wClass,
                        overlayAttributes,
                        &window_bounds,
                        &window);
  if (err)
    return NULL;

  SetWindowLevel(window, CGShieldingWindowLevel() + 1);
  InstallEventHandler(GetWindowEventTarget(window), HandleOverlayWindow,
                      sizeof(eventTypes)/sizeof(eventTypes[0]), eventTypes,
                      NULL, NULL);
  return window;
}

OverlayWindowMac::OverlayWindowMac()
    : overlay_window_(NULL),
      time_to_hide_overlay_(0.0) {
  overlay_window_ = CreateOverlayWindow();
  ShowWindow(overlay_window_);
  o3d::SlideWindowToRect(overlay_window_,
                         Rect2CGRect(GetOverlayWindowRect(true)),
                         kTransitionTime);

  // Hide the overlay text 4 seconds from now.
  time_to_hide_overlay_ = [NSDate timeIntervalSinceReferenceDate] + 4.0;
}

OverlayWindowMac::~OverlayWindowMac() {
  HideWindow(overlay_window_);
  ReleaseWindowGroup(GetWindowGroup(overlay_window_));
  DisposeWindow(overlay_window_);
}

void OverlayWindowMac::IdleCallback() {
  if ((overlay_window_ != NULL) &&
      (time_to_hide_overlay_ != 0.0) &&
      (time_to_hide_overlay_ < [NSDate timeIntervalSinceReferenceDate])) {
    time_to_hide_overlay_ = 0.0;
    SlideWindowToRect(overlay_window_,
                      Rect2CGRect(GetOverlayWindowRect(false)),
                      kTransitionTime);
  }
}

}  // namespace o3d

#endif  // O3D_PLUGIN_ENABLE_FULLSCREEN_MSG
