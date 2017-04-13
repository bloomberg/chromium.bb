// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HostsUsingFeatures_h
#define HostsUsingFeatures_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/StringHash.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class Document;
class EventTarget;
class ScriptState;

class CORE_EXPORT HostsUsingFeatures {
  DISALLOW_NEW();

 public:
  ~HostsUsingFeatures();

  // Features for RAPPOR. Do not reorder or remove!
  enum class Feature {
    kElementCreateShadowRoot,
    kDocumentRegisterElement,
    kEventPath,
    kDeviceMotionInsecureHost,
    kDeviceOrientationInsecureHost,
    kFullscreenInsecureHost,
    kGeolocationInsecureHost,
    kGetUserMediaInsecureHost,
    kGetUserMediaSecureHost,
    kElementAttachShadow,
    kApplicationCacheManifestSelectInsecureHost,
    kApplicationCacheAPIInsecureHost,
    kRTCPeerConnectionAudio,
    kRTCPeerConnectionVideo,
    kRTCPeerConnectionDataChannel,
    kRTCPeerConnectionUsed,  // Used to compute the "unconnected PCs" feature

    kNumberOfFeatures  // This must be the last item.
  };

  static void CountAnyWorld(Document&, Feature);
  static void CountMainWorldOnly(const ScriptState*, Document&, Feature);
  static void CountHostOrIsolatedWorldHumanReadableName(const ScriptState*,
                                                        EventTarget&,
                                                        Feature);

  void DocumentDetached(Document&);
  void UpdateMeasurementsAndClear();

  class CORE_EXPORT Value {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

   public:
    Value();

    bool IsEmpty() const { return !count_bits_; }
    void Clear() { count_bits_ = 0; }

    void Count(Feature);
    bool Get(Feature feature) const {
      return count_bits_ & (1 << static_cast<unsigned>(feature));
    }

    void Aggregate(Value);
    void RecordHostToRappor(const String& host);
    void RecordNameToRappor(const String& name);
    void RecordETLDPlus1ToRappor(const KURL&);

   private:
    unsigned count_bits_ : static_cast<unsigned>(Feature::kNumberOfFeatures);
  };

  void CountName(Feature, const String&);
  HashMap<String, Value>& ValueByName() { return value_by_name_; }
  void Clear();

 private:
  void RecordHostToRappor();
  void RecordNamesToRappor();
  void RecordETLDPlus1ToRappor();

  Vector<std::pair<KURL, HostsUsingFeatures::Value>, 1> url_and_values_;
  HashMap<String, HostsUsingFeatures::Value> value_by_name_;
};

}  // namespace blink

#endif  // HostsUsingFeatures_h
