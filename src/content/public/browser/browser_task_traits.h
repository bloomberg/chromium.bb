// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BROWSER_TASK_TRAITS_H_
#define CONTENT_PUBLIC_BROWSER_BROWSER_TASK_TRAITS_H_

#include "base/task/task_traits.h"
#include "base/task/task_traits_extension.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"

namespace content {

// Tasks with this trait will not be executed inside a nested RunLoop.
//
// Note: This should rarely be required. Drivers of nested loops should instead
// make sure to be reentrant when allowing nested application tasks (also rare).
//
// TODO(https://crbug.com/876272): Investigate removing this trait -- and any
// logic for deferred tasks in MessageLoop.
struct NonNestable {};

// Semantic annotations which tell the scheduler what type of task it's dealing
// with. This will be used by the scheduler for dynamic prioritization and for
// attribution in traces, etc... In general, BrowserTaskType::kDefault is what
// you want (it's implicit if you don't specify this trait). Only explicitly
// specify this trait if you carefully isolated a set of tasks that have no
// ordering requirements with anything else (in doubt, consult with
// scheduler-dev@chromium.org).
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.content_public.browser
enum class BrowserTaskType {
  // A catch all for tasks that don't fit the types below.
  kDefault,

  // Critical startup tasks.
  kBootstrap,

  // A subset of network tasks related to preconnection.
  kPreconnect,

  // Used to validate values in Java
  kBrowserTaskType_Last
};

// TaskTraits for running tasks on the browser threads.
//
// These traits enable the use of the //base/task/post_task.h APIs to post tasks
// to a BrowserThread.
//
// To post a task to the UI thread (analogous for IO thread):
//     base::PostTask(FROM_HERE, {BrowserThread::UI}, task);
//
// To obtain a TaskRunner for the UI thread (analogous for the IO thread):
//     base::CreateSingleThreadTaskRunner({BrowserThread::UI});
//
// Tasks posted to the same BrowserThread with the same traits will be executed
// in the order they were posted, regardless of the TaskRunners they were
// posted via.
//
// See //base/task/post_task.h for more detailed documentation.
//
// Posting to a BrowserThread must only be done after it was initialized (ref.
// BrowserMainLoop::CreateThreads() phase).
class CONTENT_EXPORT BrowserTaskTraitsExtension {
 public:
  static constexpr uint8_t kExtensionId =
      base::TaskTraitsExtensionStorage::kFirstEmbedderExtensionId;

  struct ValidTrait : public base::TaskTraits::ValidTrait {
    using base::TaskTraits::ValidTrait::ValidTrait;

    ValidTrait(BrowserThread::ID);
    ValidTrait(BrowserTaskType);
    ValidTrait(NonNestable);
  };

  template <
      class... ArgTypes,
      class CheckArgumentsAreValid = std::enable_if_t<
          base::trait_helpers::AreValidTraits<ValidTrait, ArgTypes...>::value>>
  constexpr BrowserTaskTraitsExtension(ArgTypes... args)
      : browser_thread_(
            base::trait_helpers::GetEnum<BrowserThread::ID,
                                         BrowserThread::ID_COUNT>(args...)),
        task_type_(
            base::trait_helpers::GetEnum<BrowserTaskType,
                                         BrowserTaskType::kDefault>(args...)),
        nestable_(!base::trait_helpers::HasTrait<NonNestable, ArgTypes...>()) {}

  // Keep in sync with UiThreadTaskTraits.java
  constexpr base::TaskTraitsExtensionStorage Serialize() const {
    static_assert(12 == sizeof(BrowserTaskTraitsExtension),
                  "Update Serialize() and Parse() when changing "
                  "BrowserTaskTraitsExtension");
    return {
        kExtensionId,
        {static_cast<uint8_t>(browser_thread_),
         static_cast<uint8_t>(task_type_), static_cast<uint8_t>(nestable_)}};
  }

  static const BrowserTaskTraitsExtension Parse(
      const base::TaskTraitsExtensionStorage& extension) {
    return BrowserTaskTraitsExtension(
        static_cast<BrowserThread::ID>(extension.data[0]),
        static_cast<BrowserTaskType>(extension.data[1]),
        static_cast<bool>(extension.data[2]));
  }

  constexpr BrowserThread::ID browser_thread() const {
    // TODO(1026641): Migrate to BrowserTaskTraits under which BrowserThread is
    // not a trait. Until then, only code that knows traits have explicitly set
    // the BrowserThread trait should check this field.
    DCHECK_NE(browser_thread_, BrowserThread::ID_COUNT);
    return browser_thread_;
  }

  constexpr BrowserTaskType task_type() const { return task_type_; }

  // Returns true if tasks with these traits may run in a nested RunLoop.
  constexpr bool nestable() const { return nestable_; }

 private:
  BrowserTaskTraitsExtension(BrowserThread::ID browser_thread,
                             BrowserTaskType task_type,
                             bool nestable)
      : browser_thread_(browser_thread),
        task_type_(task_type),
        nestable_(nestable) {}

  BrowserThread::ID browser_thread_;
  BrowserTaskType task_type_;
  bool nestable_;
};

template <class... ArgTypes,
          class = std::enable_if_t<base::trait_helpers::AreValidTraits<
              BrowserTaskTraitsExtension::ValidTrait,
              ArgTypes...>::value>>
constexpr base::TaskTraitsExtensionStorage MakeTaskTraitsExtension(
    ArgTypes&&... args) {
  return BrowserTaskTraitsExtension(std::forward<ArgTypes>(args)...)
      .Serialize();
}

class CONTENT_EXPORT BrowserTaskTraits : public base::TaskTraits {
 public:
  struct ValidTrait : public base::TaskTraits::ValidTrait {
    ValidTrait(BrowserTaskType);
    ValidTrait(NonNestable);

    // TODO(1026641): Reconsider whether BrowserTaskTraits should really be
    // supporting base::TaskPriority.
    ValidTrait(base::TaskPriority);
  };

  // TODO(1026641): Get rid of BrowserTaskTraitsExtension and store its members
  // (|task_type_| & |nestable_|) directly in BrowserTaskTraits.
  template <
      class... ArgTypes,
      class CheckArgumentsAreValid = std::enable_if_t<
          base::trait_helpers::AreValidTraits<ValidTrait, ArgTypes...>::value>>
  constexpr BrowserTaskTraits(ArgTypes... args) : base::TaskTraits(args...) {}

  BrowserTaskType task_type() {
    return GetExtension<BrowserTaskTraitsExtension>().task_type();
  }

  // Returns true if tasks with these traits may run in a nested RunLoop.
  bool nestable() const {
    return GetExtension<BrowserTaskTraitsExtension>().nestable();
  }
};

static_assert(sizeof(BrowserTaskTraits) == sizeof(base::TaskTraits),
              "During the migration away from BrowserTasktraitsExtension, "
              "BrowserTaskTraits must only use base::TaskTraits for storage "
              "to prevent slicing.");

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BROWSER_TASK_TRAITS_H_
