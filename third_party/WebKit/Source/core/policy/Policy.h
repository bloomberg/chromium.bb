// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Policy_h
#define Policy_h

#include "core/CoreExport.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Member.h"
#include "third_party/WebKit/common/feature_policy/feature_policy.h"

namespace blink {

class Document;
class SecurityOrigin;

// Policy provides an interface for feature policy introspection of a document
// (DocumentPolicy) or an iframe (IFramePolicy).
class CORE_EXPORT Policy : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ~Policy() override = default;

  // Implementation of methods of the policy interface:
  // Returns whether or not the given feature is allowed on the origin of the
  // document that owns the policy.
  bool allowsFeature(const String& feature) const;
  // Returns whether or not the given feature is allowed on the origin of the
  // given URL.
  bool allowsFeature(const String& feature, const String& url) const;
  // Returns a list of feature names that are allowed on the self origin.
  Vector<String> allowedFeatures() const;
  // Returns a list of feature name that are allowed on the origin of the given
  // URL.
  Vector<String> getAllowlistForFeature(const String& url) const;

  // Inform the Policy object when the container policy on its frame element has
  // changed.
  virtual void UpdateContainerPolicy(
      const ParsedFeaturePolicy& container_policy = {},
      scoped_refptr<const SecurityOrigin> src_origin = nullptr) {}

  virtual void Trace(blink::Visitor*);

 protected:
  virtual const FeaturePolicy* GetPolicy() const = 0;
  // Get the containing document.
  virtual Document* GetDocument() const = 0;

 private:
  // Add console message to the containing document.
  void AddWarningForUnrecognizedFeature(const String& message) const;
};

}  // namespace blink

#endif  // Policy_h
