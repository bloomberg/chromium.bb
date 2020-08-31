// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "third_party/private_membership/src/internal/rlwe_params.h"

#include "private_membership_rlwe.pb.h"
#include "third_party/private_membership/src/internal/constants.h"
#include "third_party/shell-encryption/src/montgomery.h"
#include "third_party/shell-encryption/src/status_macros.h"

namespace private_membership {
namespace rlwe {

template <>
::rlwe::StatusOr<std::unique_ptr<const ModularInt64::Params>>
CreateModulusParams<ModularInt64>(const Uint128& modulus) {
  if (modulus.hi() > 0) {
    return absl::InvalidArgumentError("Modulus cannot fit into 64-bits.");
  }
  return ModularInt64::Params::Create(modulus.lo());
}

template <>
::rlwe::StatusOr<std::unique_ptr<const ModularInt128::Params>>
CreateModulusParams<ModularInt128>(const Uint128& modulus) {
  absl::uint128 modulus128 = absl::MakeUint128(modulus.hi(), modulus.lo());
  return ModularInt128::Params::Create(modulus128);
}

template <typename ModularInt>
::rlwe::StatusOr<std::unique_ptr<const ::rlwe::NttParameters<ModularInt>>>
CreateNttParams(const RlweParameters& rlwe_params,
                const typename ModularInt::Params* modulus_params) {
  RLWE_ASSIGN_OR_RETURN(::rlwe::NttParameters<ModularInt> ntt_params,
                        ::rlwe::InitializeNttParameters<ModularInt>(
                            rlwe_params.log_degree(), modulus_params));
  return absl::make_unique<const ::rlwe::NttParameters<ModularInt>>(
      std::move(ntt_params));
}

template ::rlwe::StatusOr<
    std::unique_ptr<const ::rlwe::NttParameters<ModularInt64>>>
CreateNttParams<ModularInt64>(const RlweParameters&,
                              const ModularInt64::Params*);
template ::rlwe::StatusOr<
    std::unique_ptr<const ::rlwe::NttParameters<ModularInt128>>>
CreateNttParams<ModularInt128>(const RlweParameters&,
                               const ModularInt128::Params*);

template <typename ModularInt>
::rlwe::StatusOr<std::unique_ptr<const ::rlwe::ErrorParams<ModularInt>>>
CreateErrorParams(const RlweParameters& rlwe_params,
                  const typename ModularInt::Params* modulus_params,
                  const ::rlwe::NttParameters<ModularInt>* ntt_params) {
  RLWE_ASSIGN_OR_RETURN(auto error_params,
                        ::rlwe::ErrorParams<ModularInt>::Create(
                            rlwe_params.log_t(), rlwe_params.variance(),
                            modulus_params, ntt_params));
  return absl::make_unique<const ::rlwe::ErrorParams<ModularInt>>(error_params);
}

template ::rlwe::StatusOr<
    std::unique_ptr<const ::rlwe::ErrorParams<ModularInt64>>>
CreateErrorParams<ModularInt64>(const RlweParameters&,
                                const ModularInt64::Params*,
                                const ::rlwe::NttParameters<ModularInt64>*);
template ::rlwe::StatusOr<
    std::unique_ptr<const ::rlwe::ErrorParams<ModularInt128>>>
CreateErrorParams<ModularInt128>(const RlweParameters&,
                                 const ModularInt128::Params*,
                                 const ::rlwe::NttParameters<ModularInt128>*);

}  // namespace rlwe
}  // namespace private_membership
