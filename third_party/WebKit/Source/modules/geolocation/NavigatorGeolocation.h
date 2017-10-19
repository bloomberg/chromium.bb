/*
    Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef NavigatorGeolocation_h
#define NavigatorGeolocation_h

#include "core/frame/Navigator.h"
#include "platform/Supplementable.h"
#include "platform/bindings/TraceWrapperMember.h"
#include "platform/heap/Handle.h"

namespace blink {

class Geolocation;
class Navigator;

class NavigatorGeolocation final
    : public GarbageCollected<NavigatorGeolocation>,
      public Supplement<Navigator>,
      public TraceWrapperBase {
  USING_GARBAGE_COLLECTED_MIXIN(NavigatorGeolocation);

 public:
  static NavigatorGeolocation& From(Navigator&);
  static Geolocation* geolocation(Navigator&);
  Geolocation* geolocation();

  void Trace(blink::Visitor*);
  void TraceWrappers(const ScriptWrappableVisitor*) const;

 private:
  explicit NavigatorGeolocation(Navigator&);

  static const char* SupplementName();

  TraceWrapperMember<Geolocation> geolocation_;
};

}  // namespace blink

#endif  // NavigatorGeolocation_h
