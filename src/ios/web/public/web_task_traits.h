// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_WEB_TASK_TRAITS_H_
#define IOS_WEB_PUBLIC_WEB_TASK_TRAITS_H_

#include "base/task/task_traits.h"
#include "base/task/task_traits_extension.h"
#include "ios/web/public/web_thread.h"

namespace web {

// TaskTraits for running tasks on a WebThread.
//
// These traits enable the use of the //base/task/post_task.h APIs to post tasks
// to a WebThread.
//
// To post a task to the UI thread (analogous for IO thread):
//     base::PostTaskWithTraits(FROM_HERE, {WebThread::UI}, task);
//
// To obtain a TaskRunner for the UI thread (analogous for the IO thread):
//     base::CreateSingleThreadTaskRunnerWithTraits({WebThread::UI});
//
// See //base/task/post_task.h for more detailed documentation.
//
// Posting to a WebThread must only be done after it was initialized (ref.
// WebMainLoop::CreateThreads() phase).
class WebTaskTraitsExtension {
 public:
  static constexpr uint8_t kExtensionId =
      base::TaskTraitsExtensionStorage::kFirstEmbedderExtensionId;

  struct ValidTrait {
    ValidTrait(WebThread::ID) {}
  };

  template <
      class... ArgTypes,
      class CheckArgumentsAreValid = std::enable_if_t<
          base::trait_helpers::AreValidTraits<ValidTrait, ArgTypes...>::value>>
  constexpr WebTaskTraitsExtension(ArgTypes... args)
      : web_thread_(base::trait_helpers::GetValueFromArgList(
            base::trait_helpers::RequiredEnumArgGetter<WebThread::ID>(),
            args...)) {}

  constexpr base::TaskTraitsExtensionStorage Serialize() const {
    static_assert(4 == sizeof(WebTaskTraitsExtension),
                  "Update Serialize() and Parse() when changing "
                  "WebTaskTraitsExtension");
    return {kExtensionId, {static_cast<uint8_t>(web_thread_)}};
  }

  static const WebTaskTraitsExtension Parse(
      const base::TaskTraitsExtensionStorage& extension) {
    return WebTaskTraitsExtension(
        static_cast<WebThread::ID>(extension.data[0]));
  }

  constexpr WebThread::ID web_thread() const { return web_thread_; }

 private:
  WebThread::ID web_thread_;
};

template <class... ArgTypes,
          class = std::enable_if_t<base::trait_helpers::AreValidTraits<
              WebTaskTraitsExtension::ValidTrait,
              ArgTypes...>::value>>
constexpr base::TaskTraitsExtensionStorage MakeTaskTraitsExtension(
    ArgTypes&&... args) {
  return WebTaskTraitsExtension(std::forward<ArgTypes>(args)...).Serialize();
}

}  // namespace web

#endif  // IOS_WEB_PUBLIC_WEB_TASK_TRAITS_H_
