// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_ANDROID_FORWARDER2_FORWARDERS_MANAGER_H_
#define TOOLS_ANDROID_FORWARDER2_FORWARDERS_MANAGER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "tools/android/forwarder2/pipe_notifier.h"

namespace base {

class SingleThreadTaskRunner;

}  // namespace base

namespace forwarder2 {

class Forwarder;
class Socket;

// Creates and manages lifetime of a set of Forwarder instances. This class
// ensures in particular that the forwarder instances are deleted on their
// construction thread no matter which thread ~ForwardersManager() is called on.
class ForwardersManager {
 public:
  // Can be called on any thread.
  ForwardersManager();
  ~ForwardersManager();

  // Note that the (unique) thread that this method is called on must outlive
  // the ForwardersManager instance. This is needed since the Forwarder
  // instances are deleted on the thread they were constructed on.
  void CreateAndStartNewForwarder(scoped_ptr<Socket> socket1,
                                  scoped_ptr<Socket> socket2);

 private:
  // Ref-counted thread-safe delegate that can (by definition) outlive its outer
  // ForwarderManager instance. This is needed since the forwarder instances are
  // destroyed on a thread other than the one ~ForwardersManager() is called on.
  // The forwarder instances can also call OnForwarderError() below at any time
  // on their construction thread (which is the same as their destruction
  // thread). This means that a WeakPtr to ForwardersManager can't be used
  // instead of ref-counting since checking the validity of the WeakPtr on a
  // thread other than the one ~ForwardersManager() is called on would be racy.
  class Delegate : public base::RefCountedThreadSafe<Delegate> {
   public:
    Delegate();

    void Clear();

    void CreateAndStartNewForwarder(scoped_ptr<Socket> socket1,
                                    scoped_ptr<Socket> socket2);

   private:
    friend class base::RefCountedThreadSafe<Delegate>;

    virtual ~Delegate();

    void OnForwarderError(scoped_ptr<Forwarder> forwarder);

    void ClearOnForwarderConstructorThread();

    scoped_refptr<base::SingleThreadTaskRunner> forwarders_constructor_runner_;
    PipeNotifier deletion_notifier_;
    ScopedVector<Forwarder> forwarders_;
  };

  const scoped_refptr<Delegate> delegate_;
};

}  // namespace forwarder2

#endif  // TOOLS_ANDROID_FORWARDER2_FORWARDERS_MANAGER_H_
