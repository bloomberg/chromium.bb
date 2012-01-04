// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_VIEW_H_
#define PPAPI_CPP_VIEW_H_

#include "ppapi/cpp/resource.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/size.h"

namespace pp {

class View : public Resource {
 public:
  /// Default constructor for creating an is_null() <code>View</code> object.
  View();

  /// Creates a View resource, taking and holding an additional reference to
  /// the given resource handle.
  View(PP_Resource view_resource);

  /// <code>GetRect()</code> returns the rectangle of the instance
  /// associated with the view changed notification relative to the upper left
  /// of the browser viewport. This position changes when the page is scrolled.
  ///
  /// The returned rectangle may not be inside the visible portion of the
  /// viewport if the instance is scrolled off the page. Therefore, the
  /// position may be negative or larger than the size of the page. The size
  /// will always reflect the size of the plugin were it to be scrolled
  /// entirely into view.
  ///
  /// In general, most plugins will not need to worry about the position of the
  /// instance in the viewport, and only need to use the size.
  ///
  /// @return The rectangle of the instance. The default return value for
  /// an invalid View is the empty rectangle.
  Rect GetRect() const;

  /// <code>IsFullscreen()</code> returns whether the instance is currently
  /// displaying in fullscreen mode.
  ///
  /// @return <code>true</code> if the instance is in full screen mode,
  /// or <code>false</code> if it's not or the resource is invalid.
  bool IsFullscreen() const;

  /// <code>IsVisible()</code> returns true if the instance is plausibly
  /// visible to the user. You should use this flag to throttle or stop updates
  /// for invisible plugins.
  ///
  /// Thie measure incorporates both whether the instance is scrolled into
  /// view (whether the clip rect is nonempty) and whether the page is
  /// plausibly visible to the user (<code>IsPageVisible()</code>).
  ///
  /// @return <code>true</code> if the instance is plausibly visible to the
  /// user, <code>false</code> if it is definitely not visible.
  bool IsVisible() const;

  /// <code>IsPageVisible()</code> determines if the page that contains the
  /// instance is visible. The most common cause of invisible pages is that
  /// the page is in a background tab in the browser.
  ///
  /// Most applications should use <code>IsVisible()</code> rather than
  /// this function since the instance could be scrolled off of a visible
  /// page, and this function will still return true. However, depending on
  /// how your plugin interacts with the page, there may be certain updates
  /// that you may want to perform when the page is visible even if your
  /// specific instance isn't.
  ///
  /// @return <code>true</code> if the instance is plausibly visible to the
  /// user, <code>false</code> if it is definitely not visible.
  bool IsPageVisible() const;

  /// <code>GetClip()</code> returns the clip rectangle relative to the upper
  /// left corner of the instance. This rectangle indicates which parts of the
  /// instance are scrolled into view.
  ///
  /// If the instance is scrolled off the view, the return value will be
  /// (0, 0, 0, 0). this state. This clip rect does <i>not</i> take into account
  /// page visibility. This means if the instance is scrolled into view but the
  /// page itself is in an invisible tab, the return rect will contain the
  /// visible rect assuming the page was visible. See
  /// <code>IsPageVisible()</code> and <code>IsVisible()</code> if you want
  /// to handle this case.
  ///
  /// Most applications will not need to worry about the clip. The recommended
  /// behavior is to do full updates if the instance is visible as determined by
  /// <code>IsUserVisible()</code> and do no updates if not.
  ///
  /// However, if the cost for computing pixels is very high for your
  /// application or the pages you're targeting frequently have very large
  /// instances with only portions visible, you may wish to optimize further.
  /// In this case, the clip rect will tell you which parts of the plugin to
  /// update.
  ///
  /// Note that painting of the page and sending of view changed updates
  /// happens asynchronously. This means when the user scrolls, for example,
  /// it is likely that the previous backing store of the instance will be used
  /// for the first paint, and will be updated later when your application
  /// generates new content. This may cause flickering at the boundaries when
  /// scrolling. If you do choose to do partial updates, you may want to think
  /// about what color the invisible portions of your backing store contain
  /// (be it transparent or some background color) or to paint a certain
  /// region outside the clip to reduce the visual distraction when this
  /// happens.
  ///
  /// @return The rectangle representing the visible part of the instance.
  /// If the resource is invalid, the empty rect is returned.
  Rect GetClipRect() const;
};

}  // namespace pp

#endif  // PPAPI_CPP_VIEW_H_
