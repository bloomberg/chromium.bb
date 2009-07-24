/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// This file defines the JSON Object class.

#include "import/cross/json_object.h"

namespace o3d {

O3D_OBJECT_BASE_DEFN_CLASS("o3djs.JSONObject", JSONObject, ParamObject);

JSONObject::JSONObject(ServiceLocator* service_locator)
    : ParamObject(service_locator) {
}

void JSONObject::AddProperty(const String& name, JSONValue* value) {
  std::pair<NameValueMap::iterator, bool> result = properties_.insert(
      std::pair<String, JSONValue::Ref>(name, JSONValue::Ref(value)));
  DCHECK(result.second);
}

void JSONObject::Serialize(StructuredWriter* writer) const {
  writer->WritePropertyName("object");
  writer->OpenObject();
  for (NameValueMap::const_iterator it(properties_.begin());
       it != properties_.end();
       ++it) {
    if (it->second->exists()) {
      writer->WritePropertyName(it->first);
      it->second->Serialize(writer);
    }
  }
  writer->CloseObject();
}

}  // namespace o3d


