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
// At the root of the hierarchy is a WindowAndroid, when attached.
class UI_ANDROID_EXPORT ViewAndroid {
 public:
  // A ViewAndroid may have its own delegate or otherwise will
  // use the next available parent's delegate.
  ViewAndroid(const base::android::JavaRef<jobject>& delegate);
  ViewAndroid();
  virtual ~ViewAndroid();

  // Returns the window at the root of this hierarchy, or |null|
  // if disconnected.
  virtual WindowAndroid* GetWindowAndroid() const;

  // Returns the Java delegate for this view. This is used to delegate work
  // up to the embedding view (or the embedder that can deal with the
  // implementation details).
  const base::android::JavaRef<jobject>& GetViewAndroidDelegate() const;

  // Used to return and set the layer for this view. May be |null|.
  cc::Layer* GetLayer() const;
  void SetLayer(scoped_refptr<cc::Layer> layer);

  // Adds this view as a child of another view.
  void AddChild(ViewAndroid* child);

  // Detaches this view from its parent.
  void RemoveFromParent();

  void StartDragAndDrop(const base::android::JavaRef<jstring>& jtext,
                        const base::android::JavaRef<jobject>& jimage);

 protected:
  ViewAndroid* parent_;

 private:
  void RemoveChild(ViewAndroid* child);

  std::list<ViewAndroid*> children_;
  scoped_refptr<cc::Layer> layer_;
  base::android::ScopedJavaGlobalRef<jobject> delegate_;

  DISALLOW_COPY_AND_ASSIGN(ViewAndroid);
};

}  // namespace ui

#endif  // UI_ANDROID_VIEW_ANDROID_H_
