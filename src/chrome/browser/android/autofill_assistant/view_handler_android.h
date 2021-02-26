// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_VIEW_HANDLER_ANDROID_H_
#define CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_VIEW_HANDLER_ANDROID_H_

#include <map>
#include <memory>
#include <string>

#include "base/android/jni_android.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"

namespace autofill_assistant {

// Manages a map of view-identifier -> android view instances.
class ViewHandlerAndroid {
 public:
  ViewHandlerAndroid();
  ~ViewHandlerAndroid();
  ViewHandlerAndroid(const ViewHandlerAndroid&) = delete;
  ViewHandlerAndroid& operator=(const ViewHandlerAndroid&) = delete;

  base::WeakPtr<ViewHandlerAndroid> GetWeakPtr();

  // Returns the view associated with |view_identifier| or base::nullopt if
  // there is no such view.
  base::Optional<base::android::ScopedJavaGlobalRef<jobject>> GetView(
      const std::string& view_identifier) const;

  // Adds a view to the set of managed views.
  void AddView(const std::string& view_identifier,
               base::android::ScopedJavaGlobalRef<jobject> jview);

 private:
  std::map<std::string, base::android::ScopedJavaGlobalRef<jobject>> views_;
  base::WeakPtrFactory<ViewHandlerAndroid> weak_ptr_factory_{this};
};

}  //  namespace autofill_assistant

#endif  //  CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_VIEW_HANDLER_ANDROID_H_
