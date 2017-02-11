// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSDstyle license that can be
// found in the LICENSE file.

#include "core/html/HTMLIFrameElementPermissions.h"

#include "core/html/HTMLIFrameElement.h"
#include "wtf/HashMap.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

namespace {

struct SupportedPermission {
  const char* name;
  mojom::blink::PermissionName type;
};

const SupportedPermission kSupportedPermissions[] = {
    {"geolocation", mojom::blink::PermissionName::GEOLOCATION},
    {"notifications", mojom::blink::PermissionName::NOTIFICATIONS},
    {"midi", mojom::blink::PermissionName::MIDI},
};

// Returns true if the name is valid and the type is stored in |result|.
bool getPermissionType(const AtomicString& name,
                       mojom::blink::PermissionName* result) {
  for (const SupportedPermission& permission : kSupportedPermissions) {
    if (name == permission.name) {
      if (result)
        *result = permission.type;
      return true;
    }
  }
  return false;
}

}  // namespace

HTMLIFrameElementPermissions::HTMLIFrameElementPermissions(
    HTMLIFrameElement* element)
    : DOMTokenList(this), m_element(element) {}

HTMLIFrameElementPermissions::~HTMLIFrameElementPermissions() {}

DEFINE_TRACE(HTMLIFrameElementPermissions) {
  visitor->trace(m_element);
  DOMTokenList::trace(visitor);
  DOMTokenListObserver::trace(visitor);
}

Vector<mojom::blink::PermissionName>
HTMLIFrameElementPermissions::parseDelegatedPermissions(
    String& invalidTokensErrorMessage) const {
  Vector<blink::mojom::blink::PermissionName> permissions;
  unsigned numTokenErrors = 0;
  StringBuilder tokenErrors;
  const SpaceSplitString& tokens = this->tokens();

  for (size_t i = 0; i < tokens.size(); ++i) {
    blink::mojom::blink::PermissionName type;
    if (getPermissionType(tokens[i], &type)) {
      permissions.push_back(type);
    } else {
      tokenErrors.append(tokenErrors.isEmpty() ? "'" : ", '");
      tokenErrors.append(tokens[i]);
      tokenErrors.append("'");
      ++numTokenErrors;
    }
  }

  if (numTokenErrors) {
    tokenErrors.append(numTokenErrors > 1 ? " are invalid permissions flags."
                                          : " is an invalid permissions flag.");
    invalidTokensErrorMessage = tokenErrors.toString();
  }

  return permissions;
}

bool HTMLIFrameElementPermissions::validateTokenValue(
    const AtomicString& tokenValue,
    ExceptionState&) const {
  mojom::blink::PermissionName unused;
  return getPermissionType(tokenValue, &unused);
}

void HTMLIFrameElementPermissions::valueWasSet() {
  if (m_element)
    m_element->permissionsValueWasSet();
}

}  // namespace blink
