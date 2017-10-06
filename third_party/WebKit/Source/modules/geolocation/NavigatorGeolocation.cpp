/*
 *  Copyright (C) 2000 Harri Porten (porten@kde.org)
 *  Copyright (c) 2000 Daniel Molkentin (molkentin@kde.org)
 *  Copyright (c) 2000 Stefan Schimanski (schimmi@kde.org)
 *  Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.
 *  Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA  02110-1301 USA
 */

#include "modules/geolocation/NavigatorGeolocation.h"

#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Navigator.h"
#include "modules/geolocation/Geolocation.h"

namespace blink {

NavigatorGeolocation::NavigatorGeolocation(Navigator& navigator)
    : Supplement<Navigator>(navigator) {}

const char* NavigatorGeolocation::SupplementName() {
  return "NavigatorGeolocation";
}

NavigatorGeolocation& NavigatorGeolocation::From(Navigator& navigator) {
  NavigatorGeolocation* supplement = static_cast<NavigatorGeolocation*>(
      Supplement<Navigator>::From(navigator, SupplementName()));
  if (!supplement) {
    supplement = new NavigatorGeolocation(navigator);
    ProvideTo(navigator, SupplementName(), supplement);
  }
  return *supplement;
}

Geolocation* NavigatorGeolocation::geolocation(Navigator& navigator) {
  return NavigatorGeolocation::From(navigator).geolocation();
}

Geolocation* NavigatorGeolocation::geolocation() {
  if (!geolocation_ && GetSupplementable()->GetFrame())
    geolocation_ =
        Geolocation::Create(GetSupplementable()->GetFrame()->GetDocument());
  return geolocation_;
}

DEFINE_TRACE(NavigatorGeolocation) {
  visitor->Trace(geolocation_);
  Supplement<Navigator>::Trace(visitor);
}

DEFINE_TRACE_WRAPPERS(NavigatorGeolocation) {
  visitor->TraceWrappers(geolocation_);
}

}  // namespace blink
