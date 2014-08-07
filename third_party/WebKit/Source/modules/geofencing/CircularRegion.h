// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CircularRegion_h
#define CircularRegion_h

#include "modules/geofencing/GeofencingRegion.h"

namespace blink {

class Dictionary;

struct CircularRegionInit {
    CircularRegionInit() : latitude(0), longitude(0), radius(0) { }
    explicit CircularRegionInit(const Dictionary& init);

    String id;
    double latitude;
    double longitude;
    double radius;
};

class CircularRegion FINAL : public GeofencingRegion {
    WTF_MAKE_NONCOPYABLE(CircularRegion);
public:
    static CircularRegion* create(const Dictionary& init);
    virtual ~CircularRegion() { }

    double latitude() const { return m_latitude; }
    double longitude() const { return m_longitude; }
    double radius() const { return m_radius; }

    virtual void trace(Visitor* visitor) OVERRIDE { GeofencingRegion::trace(visitor); }

private:
    explicit CircularRegion(const CircularRegionInit&);

    double m_latitude;
    double m_longitude;
    double m_radius;
};

} // namespace blink

#endif
