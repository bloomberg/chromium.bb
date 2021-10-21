// Copyright 2020 The Dawn Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "dawn_native/QuerySet.h"

#include "dawn_native/Device.h"
#include "dawn_native/Features.h"
#include "dawn_native/ObjectType_autogen.h"
#include "dawn_native/ValidationUtils_autogen.h"

#include <set>

namespace dawn_native {

    namespace {

        class ErrorQuerySet final : public QuerySetBase {
          public:
            ErrorQuerySet(DeviceBase* device) : QuerySetBase(device, ObjectBase::kError) {
            }

          private:
            void DestroyImpl() override {
                UNREACHABLE();
            }
        };

    }  // anonymous namespace

    MaybeError ValidateQuerySetDescriptor(DeviceBase* device,
                                          const QuerySetDescriptor* descriptor) {
        DAWN_INVALID_IF(descriptor->nextInChain != nullptr, "nextInChain must be nullptr");

        DAWN_TRY(ValidateQueryType(descriptor->type));

        DAWN_INVALID_IF(descriptor->count > kMaxQueryCount,
                        "Query count (%u) exceeds the maximum query count (%u).", descriptor->count,
                        kMaxQueryCount);

        switch (descriptor->type) {
            case wgpu::QueryType::Occlusion:
                DAWN_INVALID_IF(descriptor->pipelineStatisticsCount != 0,
                                "Pipeline statistics specified for a query of type %s.",
                                descriptor->type);
                break;

            case wgpu::QueryType::PipelineStatistics: {
                // TODO(crbug.com/1177506): Pipeline statistics query is not fully implemented.
                // Disallow it as unsafe until the implementaion is completed.
                DAWN_INVALID_IF(device->IsToggleEnabled(Toggle::DisallowUnsafeAPIs),
                                "Pipeline statistics queries are disallowed because they are not "
                                "fully implemented");

                DAWN_INVALID_IF(
                    !device->IsFeatureEnabled(Feature::PipelineStatisticsQuery),
                    "Pipeline statistics query set created without the feature being enabled.");

                DAWN_INVALID_IF(descriptor->pipelineStatisticsCount == 0,
                                "Pipeline statistics query set created with 0 statistics.");

                std::set<wgpu::PipelineStatisticName> pipelineStatisticsSet;
                for (uint32_t i = 0; i < descriptor->pipelineStatisticsCount; i++) {
                    DAWN_TRY(ValidatePipelineStatisticName(descriptor->pipelineStatistics[i]));

                    std::pair<std::set<wgpu::PipelineStatisticName>::iterator, bool> res =
                        pipelineStatisticsSet.insert((descriptor->pipelineStatistics[i]));
                    DAWN_INVALID_IF(!res.second, "Statistic %s is specified more than once.",
                                    descriptor->pipelineStatistics[i]);
                }
            } break;

            case wgpu::QueryType::Timestamp:
                DAWN_INVALID_IF(device->IsToggleEnabled(Toggle::DisallowUnsafeAPIs),
                                "Timestamp queries are disallowed because they may expose precise "
                                "timing information.");

                DAWN_INVALID_IF(!device->IsFeatureEnabled(Feature::TimestampQuery),
                                "Timestamp query set created without the feature being enabled.");

                DAWN_INVALID_IF(descriptor->pipelineStatisticsCount != 0,
                                "Pipeline statistics specified for a query of type %s.",
                                descriptor->type);
                break;

            default:
                break;
        }

        return {};
    }

    QuerySetBase::QuerySetBase(DeviceBase* device, const QuerySetDescriptor* descriptor)
        : ApiObjectBase(device, kLabelNotImplemented),
          mQueryType(descriptor->type),
          mQueryCount(descriptor->count),
          mState(QuerySetState::Available) {
        for (uint32_t i = 0; i < descriptor->pipelineStatisticsCount; i++) {
            mPipelineStatistics.push_back(descriptor->pipelineStatistics[i]);
        }

        mQueryAvailability.resize(descriptor->count);
    }

    QuerySetBase::QuerySetBase(DeviceBase* device, ObjectBase::ErrorTag tag)
        : ApiObjectBase(device, tag) {
    }

    QuerySetBase::~QuerySetBase() {
        // Uninitialized or already destroyed
        ASSERT(mState == QuerySetState::Unavailable || mState == QuerySetState::Destroyed);
    }

    // static
    QuerySetBase* QuerySetBase::MakeError(DeviceBase* device) {
        return new ErrorQuerySet(device);
    }

    ObjectType QuerySetBase::GetType() const {
        return ObjectType::QuerySet;
    }

    wgpu::QueryType QuerySetBase::GetQueryType() const {
        return mQueryType;
    }

    uint32_t QuerySetBase::GetQueryCount() const {
        return mQueryCount;
    }

    const std::vector<wgpu::PipelineStatisticName>& QuerySetBase::GetPipelineStatistics() const {
        return mPipelineStatistics;
    }

    const std::vector<bool>& QuerySetBase::GetQueryAvailability() const {
        return mQueryAvailability;
    }

    void QuerySetBase::SetQueryAvailability(uint32_t index, bool available) {
        mQueryAvailability[index] = available;
    }

    MaybeError QuerySetBase::ValidateCanUseInSubmitNow() const {
        ASSERT(!IsError());
        DAWN_INVALID_IF(mState == QuerySetState::Destroyed, "%s used while destroyed.", this);
        return {};
    }

    void QuerySetBase::APIDestroy() {
        if (GetDevice()->ConsumedError(ValidateDestroy())) {
            return;
        }
        DestroyInternal();
    }

    MaybeError QuerySetBase::ValidateDestroy() const {
        DAWN_TRY(GetDevice()->ValidateObject(this));
        return {};
    }

    void QuerySetBase::DestroyInternal() {
        if (mState != QuerySetState::Destroyed) {
            DestroyImpl();
        }
        mState = QuerySetState::Destroyed;
    }

}  // namespace dawn_native
