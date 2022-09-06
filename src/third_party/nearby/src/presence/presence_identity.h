// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef THIRD_PARTY_NEARBY_PRESENCE_PRESENCE_IDENTITY_H_
#define THIRD_PARTY_NEARBY_PRESENCE_PRESENCE_IDENTITY_H_

namespace nearby {
namespace presence {
class PresenceIdentity {
 public:
  enum class IdentityType {
    kPrivate = 0,
    kTrusted,
  };
  PresenceIdentity(IdentityType type = IdentityType::kPrivate) noexcept;
  IdentityType GetIdentityType() const;

 private:
  const IdentityType identity_type_;
};

inline bool operator==(const PresenceIdentity& i1, const PresenceIdentity& i2) {
  return i1.GetIdentityType() == i2.GetIdentityType();
}

inline bool operator!=(const PresenceIdentity& i1, const PresenceIdentity& i2) {
  return !(i1 == i2);
}
}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_PRESENCE_IDENTITY_H_
