// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_VIEW_ANDROID_H_
#define UI_ANDROID_VIEW_ANDROID_H_

#include <list>

#include "base/android/scoped_java_ref.h"
#include "base/memory/ref_counted.h"
#include "ui/android/ui_android_export.h"

namespace cc {
class Layer;
}

namespace ui {

class WindowAndroid;

// A simple container for a UI layer.
// At the root of the hierarchy is a WindowAndroid.
//
class UI_ANDROID_EXPORT ViewAndroid {
 public:
  // Used to construct a root view.
  ViewAndroid(const base::android::JavaRef<jobject>& delegate,
              WindowAndroid* root_window);

  // Used to construct a child view.
  ViewAndroid();
  ~ViewAndroid();

  // Returns the window at the root of this hierarchy, or |null|
  // if disconnected.
  WindowAndroid* GetWindowAndroid() const;

  // Set the root |WindowAndroid|. This is only valid for root
  // nodes and must not be called for children.
  void SetWindowAndroid(WindowAndroid* root_window);

  // Returns the Java delegate for this view. This is used to delegate work
  // up to the embedding view (or the embedder that can deal with the
  // implementation details).
  const base::android::JavaRef<jobject>& GetViewAndroidDelegate() const;

  // Used to return and set the layer for this view. May be |null|.
  cc::Layer* GetLayer() const;
  void SetLayer(scoped_refptr<cc::Layer> layer);

  // Add/remove this view as a child of another view.
  void AddChild(ViewAndroid* child);
  void RemoveChild(ViewAndroid* child);

 private:
  ViewAndroid* parent_;
  std::list<ViewAndroid*> children_;
  WindowAndroid* window_;
  scoped_refptr<cc::Layer> layer_;
  base::android::ScopedJavaGlobalRef<jobject> delegate_;

  DISALLOW_COPY_AND_ASSIGN(ViewAndroid);
};

}  // namespace ui

#endif  // UI_ANDROID_VIEW_ANDROID_H_
