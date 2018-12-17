// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A crazy linker test to test callbacks for delayed execution.

#include <stdio.h>
#include <crazy_linker.h>

#include "test_util.h"  // For Panic()

#include "crazy_linker_util_threads.h"

namespace {

typedef void (*FunctionPtr)();

// Data block passed between callback poster and callback handler.
// It is used to hold a single crazy_callback_t instance while allowing
// synchronization between two threads, i.e.:
//
//  - One thread can cann SetCallback() to set the callback.
//
//  - Another thread can call WaitForSetCallback() to wait for the first
//    one to call SetCallback() and retrieve the callback value.
//
//  - Once SetCallback() was called, RunCallback() can be called to
//    actually run the callback and clear the state.
//
class SharedState {
 public:
  SharedState() : cond_(&mutex_) {}

  // Set the callback in the shared state. This will signal WaitForSetCallback()
  void SetCallback(crazy_callback_t callback) {
    crazy::AutoLock m(&mutex_);
    callback_ = callback;
    cond_.Signal();
  }

  // Returns true iff a callback is stored in this state, e.g. after
  // calling SetCallback().
  bool HasCallback() const {
    crazy::AutoLock m(&mutex_);
    return callback_.handler != nullptr;
  }

  // Wait until SetCallback() is called from another thread.
  // Return the corresponding value and return the callback, clearing
  // the shared state.
  void WaitForCallback() {
    // Wait for the library close to call PostCallback() before returning.
    mutex_.Lock();
    while (!callback_.handler) {
      cond_.Wait();
    }
    mutex_.Unlock();
  }

  // Run the callback. This will panic if SetCallback() was not called
  // previously.
  void RunCallback() {
    if (!callback_.handler) {
      Panic("No callback set in shared state!\n");
    }
    // Run the callback, then clear it.
    crazy_callback_run(&callback_);
    callback_.handler = nullptr;
    callback_.opaque = nullptr;
  }

  crazy_callback_t callback_ = {};
  mutable crazy::Mutex mutex_;
  crazy::Condition cond_;
};

// This function is called from the crazy linker whenever a new pending
// operation must be run on the UI thread. |callback| will have to be
// run later from another thread in this sample program, by calling
// crazy_callback_run().
bool PostCallback(crazy_callback_t* callback, void* poster_opaque) {
  printf("Post callback, poster_opaque %p, handler %p, opaque %p\n",
         poster_opaque, callback->handler, callback->opaque);
  reinterpret_cast<SharedState*>(poster_opaque)->SetCallback(*callback);
  return true;
}

// A simple thread that will close a crazy_library_t instance in its
// main body then exit immediately. Note that closing the library will
// force the linker to wait for the completion of any callbacks that
// were sent through PostCallback().
class CloserThread : public crazy::ThreadBase {
 public:
  CloserThread(crazy_library_t* library, crazy_context_t* context)
      : library_(library), context_(context) {}

 private:
  void Main() override { crazy_library_close_with_context(library_, context_); }

  crazy_library_t* library_;
  crazy_context_t* context_;
};

}  // namespace

#define LIB_NAME "libcrazy_linker_tests_libfoo.so"

int main() {
  crazy_context_t* context = crazy_context_create();
  crazy_library_t* library;

  // DEBUG
  crazy_context_set_load_address(context, 0x20000000);

  // Set a callback poster, then verify it was set properly.
  SharedState shared_state;
  crazy_context_set_callback_poster(context, &PostCallback, &shared_state);
  {
    crazy_callback_poster_t poster;
    void* poster_opaque;
    crazy_context_get_callback_poster(context, &poster, &poster_opaque);
    if (poster != &PostCallback || poster_opaque != &shared_state) {
      Panic("Get callback poster error\n");
    }
  }

  // Load library, this will end up calling PostCallback() to register
  // a delayed linker operation to modify the global list of libraries.
  if (!crazy_library_open(&library, LIB_NAME, context)) {
    Panic("Could not open library: %s\n", crazy_context_get_error(context));
  }

  // Run the posted callback in the main thread. This should always work.
  if (!shared_state.HasCallback()) {
    Panic("Delayed callback was not received in main thread!\n");
  }
  shared_state.RunCallback();

  // Find the "Foo" symbol.
  FunctionPtr foo_func;
  if (!crazy_library_find_symbol(
           library, "Foo", reinterpret_cast<void**>(&foo_func))) {
    Panic("Could not find 'Foo' in %s\n", LIB_NAME);
  }

  // Call it.
  (*foo_func)();

  // Closing the library will also call PostCallback() to register another
  // callback, but the linker will then wait for its explicit completion
  // before returning (this ensures any trace of the library was removed
  // from the global list properly). To check this here, create a background
  // thread to close the library, which will block until the callback is run
  // below in the main thread.
  {
    CloserThread thread(library, context);
    printf("background closing-thread created\n");
    shared_state.WaitForCallback();
    printf("callback received from background thread\n");
    shared_state.RunCallback();
    printf("callback ran in the main thread\n");
    thread.Join();
    printf("background thread completed, library is closed\n");
  }

  crazy_context_destroy(context);
  return 0;
}
