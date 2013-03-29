// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_EXTENSIONS_EVENT_BASE_H_
#define PPAPI_CPP_EXTENSIONS_EVENT_BASE_H_

#include "ppapi/c/extensions/dev/ppb_ext_events_dev.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/cpp/extensions/from_var_converter.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/logging.h"

namespace pp {
namespace ext {
namespace internal {

// This file contains base classes for events. Usually you don't need to use
// them directly.
//
// For each event type, there is a corresponding event class derived from
// EventBase[0-3]. The event class defines a Listener interface and exposes the
// public methods of GenericEventBase.
//
// Take pp::ext::alarms::OnAlarmEvent_Dev as example, your code to listen to the
// event would look like this:
//
//   class MyListener : public pp::ext::alarms::OnAlarmEvent_Dev {
//     ...
//     // The parameter is a non-const reference so you could directly modify it
//     // if necessary.
//     virtual void OnAlarm(Alarm_Dev& alarm) {
//       ...handle the event...
//     }
//   };
//
//   MyListener on_alarm_listener;
//   // The listener is not owned by the event and must outlive it.
//   pp::ext::alarms::OnAlarmEvent_Dev on_alarm(instance, &on_alarm_listener);
//   on_alarm.StartListening();
//   ...
//   // It is guaranteed that |on_alarm_listener| won't get called after
//   // |on_alarm| goes away. So this step is optional.
//   on_alarm.StopListening();

class GenericEventBase {
 public:
  bool StartListening();
  void StopListening();

  bool IsListening() const { return listener_id_ != 0; }
  uint32_t listener_id() const { return listener_id_; }

 protected:
  GenericEventBase(const InstanceHandle& instance,
                   const PP_Ext_EventListener& pp_listener);
  ~GenericEventBase();

  InstanceHandle instance_;
  uint32_t listener_id_;
  const PP_Ext_EventListener pp_listener_;

 private:
  // Disallow copying and assignment.
  GenericEventBase(const GenericEventBase&);
  GenericEventBase& operator=(const GenericEventBase&);
};

// EventBase[0-3] are event base classes which can be instantiated with a
// pointer to a PP_Ext_EventListener creation function and the input parameter
// types of the listener callback.
//
// For example, EvenBase1<PP_Ext_Alarms_OnAlarm_Dev, Alarm_Dev> deals with
// the event type defined by the PP_Ext_Alarms_OnAlarm_Dev function pointer. And
// it defines a pure virtual method as the listener callback:
//   virtual void Callback(Alarm_Dev&) = 0;

typedef PP_Ext_EventListener (*CreatePPEventListener0)(
    void (*)(uint32_t, void*), void*);
template <const CreatePPEventListener0 kCreatePPEventListener0>
class EventBase0 : public GenericEventBase {
 public:
  explicit EventBase0(const InstanceHandle& instance)
      : PP_ALLOW_THIS_IN_INITIALIZER_LIST(
            GenericEventBase(instance,
                             kCreatePPEventListener0(&CallbackThunk, this))) {
  }

  virtual ~EventBase0() {}

 private:
  virtual void Callback() = 0;

  static void CallbackThunk(uint32_t listener_id, void* user_data) {
    EventBase0<kCreatePPEventListener0>* event_base =
        static_cast<EventBase0<kCreatePPEventListener0>*>(user_data);
    PP_DCHECK(listener_id == event_base->listener_id_);
    // Suppress unused variable warnings.
    static_cast<void>(listener_id);

    event_base->Callback();
  }

  // Disallow copying and assignment.
  EventBase0(const EventBase0<kCreatePPEventListener0>&);
  EventBase0<kCreatePPEventListener0>& operator=(
      const EventBase0<kCreatePPEventListener0>&);
};

typedef PP_Ext_EventListener (*CreatePPEventListener1)(
    void (*)(uint32_t, void*, PP_Var), void*);
template <const CreatePPEventListener1 kCreatePPEventListener1, class A>
class EventBase1 : public GenericEventBase {
 public:
  explicit EventBase1(const InstanceHandle& instance)
      : PP_ALLOW_THIS_IN_INITIALIZER_LIST(
            GenericEventBase(instance,
                             kCreatePPEventListener1(&CallbackThunk, this))) {
  }

  virtual ~EventBase1() {}

 private:
  virtual void Callback(A&) = 0;

  static void CallbackThunk(uint32_t listener_id,
                            void* user_data,
                            PP_Var var_a) {
    EventBase1<kCreatePPEventListener1, A>* event_base =
        static_cast<EventBase1<kCreatePPEventListener1, A>*>(user_data);
    PP_DCHECK(listener_id == event_base->listener_id_);
    // Suppress unused variable warnings.
    static_cast<void>(listener_id);

    FromVarConverter<A> a(var_a);
    event_base->Callback(a.value());
  }

  // Disallow copying and assignment.
  EventBase1(const EventBase1<kCreatePPEventListener1, A>&);
  EventBase1<kCreatePPEventListener1, A>& operator=(
      const EventBase1<kCreatePPEventListener1, A>&);
};

typedef PP_Ext_EventListener (*CreatePPEventListener2)(
    void (*)(uint32_t, void*, PP_Var, PP_Var), void*);
template <const CreatePPEventListener2 kCreatePPEventListener2,
          class A,
          class B>
class EventBase2 : public GenericEventBase {
 public:
  explicit EventBase2(const InstanceHandle& instance)
      : PP_ALLOW_THIS_IN_INITIALIZER_LIST(
            GenericEventBase(instance,
                             kCreatePPEventListener2(&CallbackThunk, this))) {
  }

  virtual ~EventBase2() {}

 private:
  virtual void Callback(A&, B&) = 0;

  static void CallbackThunk(uint32_t listener_id,
                            void* user_data,
                            PP_Var var_a,
                            PP_Var var_b) {
    EventBase2<kCreatePPEventListener2, A, B>* event_base =
        static_cast<EventBase2<kCreatePPEventListener2, A, B>*>(user_data);
    PP_DCHECK(listener_id == event_base->listener_id_);
    // Suppress unused variable warnings.
    static_cast<void>(listener_id);

    FromVarConverter<A> a(var_a);
    FromVarConverter<B> b(var_b);
    event_base->Callback(a.value(), b.value());
  }

  // Disallow copying and assignment.
  EventBase2(const EventBase2<kCreatePPEventListener2, A, B>&);
  EventBase2<kCreatePPEventListener2, A, B>& operator=(
      const EventBase2<kCreatePPEventListener2, A, B>&);
};

typedef PP_Ext_EventListener (*CreatePPEventListener3)(
    void (*)(uint32_t, void*, PP_Var, PP_Var, PP_Var), void*);
template <const CreatePPEventListener3 kCreatePPEventListener3,
          class A,
          class B,
          class C>
class EventBase3 : public GenericEventBase {
 public:
  explicit EventBase3(const InstanceHandle& instance)
      : PP_ALLOW_THIS_IN_INITIALIZER_LIST(
            GenericEventBase(instance,
                             kCreatePPEventListener3(&CallbackThunk, this))) {
  }

  virtual ~EventBase3() {}

 private:
  virtual void Callback(A&, B&, C&) = 0;

  static void CallbackThunk(uint32_t listener_id,
                            void* user_data,
                            PP_Var var_a,
                            PP_Var var_b,
                            PP_Var var_c) {
    EventBase3<kCreatePPEventListener3, A, B, C>* event_base =
        static_cast<EventBase3<kCreatePPEventListener3, A, B, C>*>(user_data);
    PP_DCHECK(listener_id == event_base->listener_id_);
    // Suppress unused variable warnings.
    static_cast<void>(listener_id);

    FromVarConverter<A> a(var_a);
    FromVarConverter<B> b(var_b);
    FromVarConverter<C> c(var_c);
    event_base->Callback(a.value(), b.value(), c.value());
  }

  // Disallow copying and assignment.
  EventBase3(const EventBase3<kCreatePPEventListener3, A, B, C>&);
  EventBase3<kCreatePPEventListener3, A, B, C>& operator=(
      const EventBase3<kCreatePPEventListener3, A, B, C>&);
};

}  // namespace internal
}  // namespace ext
}  // namespace pp

#endif  // PPAPI_CPP_EXTENSIONS_EVENT_BASE_H_
