// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tracer objects uresed to record an annotated timeline of events for use in
// gathering performance data.  It wraps a TraceBuffer which is the raw data
// for a trace. Tracer is threadsafe.
//
// TraceContext is a singleton that is used to give information for the current
// trace. Clients should query TraceContext to find the current tracer and then
// use that for logging annotations. TraceContext is threadsafe.
//
// ScopedTracer pushes a new Tracer on the TraceContext.  It's scoped in that
// this tracer is popped off the context when ScopedTracer goes out of scope.
// However, if a call to NewTracedMethod is made while the ScopedTracer is in
// scope, then a reference to the Tracer will be kept in the resulting Task and
// repushed onto the stack when the Task is run.  Conceptually, this simulates
// the current context being continued when the Task is invoked.  You usually
// will want to declare a ScopedTracer at the start of a logical flow of
// operations.
//
// Example Usage:
//
// void Decoder::StartDecode() {
//   ScopedTracer tracer("decode_start");
//
//   TraceContext::tracer()->PrintString("Decode starting");
//
//   // DoDecode takes 2 parameters. The first is a callback invoked for each
//   // finished frame of output.  The second is invoked when the task is done.
//   DoDecode(NewTracedMethod(this, &Decoder::OnFrameOutput),
//            NewTracedMethod(this, &Decoder::DecodeDone));
//   }
// }
//
// void Decoder::OnFrameOutput() {
//   TraceContext::tracer()->PrintString("Frame outputed");
//   ...
// }
//
// void Decoder::DecodeDone() {
//   TraceContext::tracer()->PrintString("decode done");
//   ...
// }
//
// For each call of StartDecode(), the related calls to OnFrameOutput() and
// DecodeDone() will be annotated to the Tracer created by the ScopedTracer
// declaration allowing for creating of timing information over the related
// asynchronous Task invocations.

#ifndef REMOTING_BASE_TRACER_H_
#define REMOTING_BASE_TRACER_H_

#include <string>

#include "base/ref_counted.h"
#include "base/singleton.h"
#include "base/synchronization/lock.h"
#include "base/task.h"
#include "base/scoped_ptr.h"
#include "remoting/proto/trace.pb.h"

namespace remoting {

class Tracer : public base::RefCountedThreadSafe<Tracer> {
 public:
  // The |name| is just a label for the given tracer. It is recorder into the
  // trace buffer and printed at the end. Use it specify one logical flow such
  // as "Host Update Request".  The sample_percent is to allow for gathering a
  // random sampling of the traces. This allows the tracer to be used in a
  // high-frequency code path without spamming the log, or putting undo load on
  // the system.  Use 0.0 to disable the tracer completely, and 1.0 to log
  // everything.
  Tracer(const std::string& name, double sample_percent);

  // TODO(ajwong): Consider using an ostream interface similar to DLOG.
  void PrintString(const std::string& s);

  void OutputTrace();

 private:
  friend class base::RefCountedThreadSafe<Tracer>;
  virtual ~Tracer();

  base::Lock lock_;
  scoped_ptr<TraceBuffer> buffer_;

  DISALLOW_COPY_AND_ASSIGN(Tracer);
};

class TraceContext {
 public:
  // Get the current tracer.
  static Tracer* tracer();

  static void PushTracer(Tracer* tracer);

  static void PopTracer();

  static TraceContext* Get();

 private:
  TraceContext();

  ~TraceContext();

  void PushTracerInternal(Tracer* tracer);

  void PopTracerInternal();

  Tracer* GetTracerInternal();

  std::vector<scoped_refptr<Tracer> > tracers_;

  DISALLOW_COPY_AND_ASSIGN(TraceContext);
};

// Used to create a new tracer that NewRunnableMethod can propogate from.
//
// Declare this at the logical start of a "trace."  Calls to NewTracedMethod
// that are done with the ScopedTracer object is alive will take a reference
// to this tracer.  When such a method is invoked, it will push the trace back
// onto the top of the TraceContext stack.  The result is that all asynchronous
// tasks that are part of one logical flow will share the same trace.
class ScopedTracer {
 public:
  ScopedTracer(const std::string& name) {
#if defined(USE_TRACE)
    scoped_refptr<Tracer> tracer = new Tracer(name, 1.00);
    TraceContext::PushTracer(tracer);
#endif
  }

  ~ScopedTracer() {
#if defined(USE_TRACE)
    TraceContext::PopTracer();
#endif
  }
};

// This is experimental code.  I'm creating a set of analogues to
// the NewRunnableMethod functions called NewTracedMethod, which should be
// API equivalent to the former.  In fact, they must be enabled by setting
// USE_TRACE 1 for now.
//
// The idea is to add hooks for performance traces into the Task/Callback
// mechanisms.  If it works well enough, will think about generalizing into the
// original NewRunnableMethod and NewCallback systems.
#if defined(USE_TRACE)

template <class T, class Method, class Params>
class TracedMethod : public RunnableMethod<T, Method, Params> {
 public:
  TracedMethod(T* obj, Method meth, const Params& params)
      : RunnableMethod<T, Method, Params>(obj, meth, params),
        tracer_(TraceContext::tracer()) {
  }

  virtual ~TracedMethod() {
  }

  virtual void Run() {
    TraceContext::PushTracer(tracer_);
    RunnableMethod<T, Method, Params>::Run();
    TraceContext::PopTracer();
  }

 private:
  scoped_refptr<Tracer> tracer_;
};

template <class T, class Method>
inline CancelableTask* NewTracedMethod(T* object, Method method) {
  return new TracedMethod<T, Method, Tuple0>(object, method, MakeTuple());
}

template <class T, class Method, class A>
inline CancelableTask* NewTracedMethod(T* object, Method method, const A& a) {
  return new TracedMethod<T, Method, Tuple1<A> >(object,
                                                 method,
                                                 MakeTuple(a));
}

template <class T, class Method, class A, class B>
inline CancelableTask* NewTracedMethod(T* object, Method method,
                                       const A& a, const B& b) {
  return new TracedMethod<T, Method, Tuple2<A, B> >(object, method,
                                                    MakeTuple(a, b));
}

template <class T, class Method, class A, class B, class C>
inline CancelableTask* NewTracedMethod(T* object, Method method,
                                       const A& a, const B& b, const C& c) {
  return new TracedMethod<T, Method, Tuple3<A, B, C> >(object, method,
                                                       MakeTuple(a, b, c));
}

template <class T, class Method, class A, class B, class C, class D>
inline CancelableTask* NewTracedMethod(T* object, Method method,
                                       const A& a, const B& b,
                                       const C& c, const D& d) {
  return new TracedMethod<T, Method, Tuple4<A, B, C, D> >(object, method,
                                                          MakeTuple(a, b,
                                                                    c, d));
}

template <class T, class Method, class A, class B, class C, class D, class E>
inline CancelableTask* NewTracedMethod(T* object, Method method,
                                       const A& a, const B& b,
                                       const C& c, const D& d, const E& e) {
  return new TracedMethod<T,
  Method,
  Tuple5<A, B, C, D, E> >(object,
                          method,
                          MakeTuple(a, b, c, d, e));
}

template <class T, class Method, class A, class B, class C, class D, class E,
          class F>
inline CancelableTask* NewTracedMethod(T* object, Method method,
                                       const A& a, const B& b,
                                       const C& c, const D& d,
                                       const E& e, const F& f) {
  return new TracedMethod<T,
                          Method,
                          Tuple6<A, B, C, D, E, F> >(object,
                                                     method,
                                                     MakeTuple(a, b, c, d, e,
                                                               f));
}

template <class T, class Method, class A, class B, class C, class D, class E,
          class F, class G>
inline CancelableTask* NewTracedMethod(T* object, Method method,
                                       const A& a, const B& b,
                                       const C& c, const D& d, const E& e,
                                       const F& f, const G& g) {
  return new TracedMethod<T,
                          Method,
                          Tuple7<A, B, C, D, E, F, G> >(object,
                                                        method,
                                                        MakeTuple(a, b, c, d,
                                                                  e, f, g));
}

#else
#  define NewTracedMethod NewRunnableMethod
#endif

}  // namespace remoting

#endif  // REMOTING_BASE_TRACER_H_
