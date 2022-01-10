/* Copyright 2020 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "tensorflow/compiler/xla/service/gpu/nccl_utils.h"

#include <memory>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/str_format.h"
#include "absl/synchronization/blocking_counter.h"
#include "absl/synchronization/mutex.h"
#include "tensorflow/compiler/xla/refcounting_hash_map.h"
#include "tensorflow/compiler/xla/service/collective_ops_utils.h"
#include "tensorflow/compiler/xla/service/global_device_id.h"
#include "tensorflow/compiler/xla/service/gpu/gpu_executable_run_options.h"
#include "tensorflow/compiler/xla/status_macros.h"
#include "tensorflow/compiler/xla/statusor.h"
#include "tensorflow/core/platform/env.h"
#include "tensorflow/core/platform/errors.h"

namespace xla {
namespace gpu {

ncclRedOp_t ToNcclReduction(ReductionKind kind) {
  switch (kind) {
    case ReductionKind::SUM:
      return ncclSum;
    case ReductionKind::PRODUCT:
      return ncclProd;
    case ReductionKind::MIN:
      return ncclMin;
    case ReductionKind::MAX:
      return ncclMax;
  }
}

namespace {

StatusOr<ncclDataType_t> ToNcclDataType(PrimitiveType element_type) {
  switch (element_type) {
    case S8:
      return ncclInt8;
    case PRED:
    case U8:
      return ncclUint8;
    case S32:
      return ncclInt32;
    case U32:
      return ncclUint32;
    case S64:
      return ncclInt64;
    case U64:
      return ncclUint64;
    case F16:
      return ncclFloat16;
    case F32:
    case C64:
      return ncclFloat32;
    case F64:
    case C128:
      return ncclFloat64;
#if defined(__CUDA_BF16_TYPES_EXIST__)
    case BF16:
      return ncclBfloat16;
#endif
    default:
      return tensorflow::errors::InvalidArgument(absl::StrFormat(
          "Unsupported data type: %s", PrimitiveType_Name(element_type)));
  }
}

}  // namespace

StatusOr<std::pair<ncclDataType_t, int>> ToNcclDataTypeAndCountMultiplier(
    PrimitiveType element_type) {
  TF_ASSIGN_OR_RETURN(ncclDataType_t dtype, ToNcclDataType(element_type));
  bool is_complex = primitive_util::IsComplexType(element_type);
  return std::make_pair(dtype, is_complex ? 2 : 1);
}

bool IsGlobalNcclConfig() {
  static const bool global_nccl_config = std::getenv("NCCL_COMM_ID") != nullptr;
  return global_nccl_config;
}

bool IsNcclLaunchModeParallel() {
  static const bool is_launch_mode_parallel =
      absl::string_view(std::getenv("NCCL_LAUNCH_MODE")) == "PARALLEL";
  return is_launch_mode_parallel;
}

Status ToStatus(ncclResult_t s, const char* file, int64_t line,
                const char* expr) {
  if (s == ncclSuccess) {
    return Status::OK();
  }
  return tensorflow::errors::Internal(
      absl::StrFormat("%s:%d: NCCL operation %s failed: %s", file, line, expr,
                      ncclGetErrorString(s)));
}

Status ToStatus(cudaError_t s, const char* file, int64_t line,
                const char* expr) {
  if (s == cudaSuccess) {
    return Status::OK();
  }
  return tensorflow::errors::Internal(
      absl::StrFormat("%s:%d: CUDA operation %s failed: %s", file, line, expr,
                      cudaGetErrorString(s)));
}

NcclClique::NcclClique(
    absl::flat_hash_map<int, NcclComm> comms_by_device_ordinal)
    : comms_by_device_ordinal_(std::move(comms_by_device_ordinal)) {}

ncclComm_t NcclClique::GetCommForDeviceOrdinal(int device_ordinal) const {
  return comms_by_device_ordinal_.at(device_ordinal).get();
}

NcclCliqueMap& NcclCliqueCache() {
  // Global cache of NCCL cliques.  An entry in this map is always kept alive.
  //
  // A consequence of the fact that this is process-global is that we'll only
  // ever have one clique alive for a given set of GPUs.  This means that a
  // process will never do two collective operations concurrently on the same
  // set of GPUs.
  static auto& cache = *new NcclCliqueMap();
  return cache;
}

namespace {

void DestroyNcclComm(ncclComm_t comm) {
  VLOG(3) << absl::StreamFormat("Destroying comm %p", comm);
  XLA_CUDA_WARN_IF_ERROR(ncclCommDestroy(comm));
}

Status ToNcclUniqueId(const std::string& str_id, ncclUniqueId* nccl_id) {
  if (str_id.size() != NCCL_UNIQUE_ID_BYTES) {
    return InvalidArgument(
        "ncclUniqueId string must have %d bytes, got %d bytes", str_id.size(),
        NCCL_UNIQUE_ID_BYTES);
  }
  // NcclUniqueId is internally just a char[].
  static_assert(sizeof(ncclUniqueId) == NCCL_UNIQUE_ID_BYTES,
                "NCCL_UNIQUE_ID_BYTES");
  std::memcpy(static_cast<void*>(nccl_id), str_id.data(), NCCL_UNIQUE_ID_BYTES);
  return Status::OK();
}

std::string LocalParticipantsToString(
    const std::vector<LocalParticipant>& local_participants) {
  std::vector<std::string> parts;
  for (const LocalParticipant& local_participant : local_participants) {
    parts.push_back(absl::StrFormat("%d/rank=%d",
                                    local_participant.device_ordinal,
                                    local_participant.rank));
  }
  return absl::StrJoin(parts, ",");
}

StatusOr<std::unique_ptr<NcclClique>> CreateNcclClique(
    const NcclCliqueKey& key,
    const std::vector<LocalParticipant>& local_participants,
    const NcclUniqueIdCallback* callback) {
  int num_participants = key.devices().size();
  ncclUniqueId unique_id;
  bool only_local_participants = num_participants == local_participants.size();
  if (callback && !only_local_participants) {  // Multi-host collective.
    TF_ASSIGN_OR_RETURN(std::string id_string, (*callback)(key));
    TF_RETURN_IF_ERROR(ToNcclUniqueId(id_string, &unique_id));
  } else {
    TF_RET_CHECK(only_local_participants || IsGlobalNcclConfig())
        << "If non-local devices are taking part of a collective API on GPU, "
           "the nccl_unique_id_callback must be provided by the client.";
    XLA_CUDA_RETURN_IF_ERROR(ncclGetUniqueId(&unique_id));
  }

  VLOG(3) << "Initializing nccl comms for local participants: "
          << LocalParticipantsToString(local_participants);

  // Restore CUDA device after running this.  XLA shouldn't care, but maybe
  // another consumer does.
  int initial_cuda_device;
  XLA_CUDA_RETURN_IF_ERROR(cudaGetDevice(&initial_cuda_device));
  auto cuda_device_restorer = MakeCleanup(
      [&] { XLA_CUDA_WARN_IF_ERROR(cudaSetDevice(initial_cuda_device)); });

  // When using ncclGroupStart/End it seems that the ncclComm_t's are not
  // populated until the End() call.
  std::vector<ncclComm_t> raw_comms(local_participants.size(), nullptr);
  XLA_CUDA_RETURN_IF_ERROR(ncclGroupStart());
  Status status = [&] {
    for (int i = 0; i < local_participants.size(); ++i) {
      XLA_CUDA_RETURN_IF_ERROR(
          cudaSetDevice(local_participants[i].device_ordinal));
      XLA_CUDA_RETURN_IF_ERROR(ncclCommInitRank(&raw_comms[i], num_participants,
                                                unique_id,
                                                local_participants[i].rank));
    }
    return Status::OK();
  }();
  // Always call ncclGroupEnd().
  status.Update(XLA_CUDA_STATUS(ncclGroupEnd()));

  // Always copy raw comms to RAII type, so they are cleaned up properly.
  absl::flat_hash_map<int, NcclComm> comms_by_device_ordinal(raw_comms.size());
  for (int i = 0; i < raw_comms.size(); ++i) {
    int device_ordinal = local_participants[i].device_ordinal;
    VLOG(3) << absl::StreamFormat("Device ordinal %d assigned ncclComm %p",
                                  device_ordinal, raw_comms[i]);
    CHECK(raw_comms[i] != nullptr || !status.ok());
    comms_by_device_ordinal.emplace(device_ordinal,
                                    NcclComm(raw_comms[i], &DestroyNcclComm));
  }

  // Now we can check if there was an error creating the communicators.
  TF_RETURN_IF_ERROR(status);
  return std::make_unique<NcclClique>(std::move(comms_by_device_ordinal));
}

class NcclCliqueRendezvous
    : public Rendezvous<NcclCliqueParticipantData, LockedNcclClique> {
 public:
  NcclCliqueRendezvous(const RendezvousKey& rendezvous_key,
                       const std::vector<LocalParticipant>& local_participants,
                       const NcclUniqueIdCallback* callback)
      : Rendezvous(rendezvous_key),
        key_(std::move(rendezvous_key.global_devices)),
        local_participants_(local_participants),
        callback_(callback),
        counter_(nullptr) {}

  StatusOr<LockedNcclClique> RunCollectiveOp(
      const NcclCliqueParticipantData&) override {
    tensorflow::mutex_lock lock(mu_);
    bool primary = !initialized_;
    if (primary) {
      maybe_clique_ = NcclCliqueCache().GetOrTryCreateIfAbsent(
          key_, [&](const NcclCliqueKey& key) {
            return CreateNcclClique(key, local_participants_, callback_);
          });
      initialized_ = true;
    }
    TF_ASSIGN_OR_RETURN(NcclClique * clique, maybe_clique_);
    std::unique_ptr<absl::MutexLock> clique_lock;
    if (primary) {
      clique_lock = std::make_unique<absl::MutexLock>(clique->mu());
      counter_ = new absl::BlockingCounter(local_participants_.size());
    }
    return LockedNcclClique(*clique, std::move(clique_lock), counter_);
  }

 private:
  NcclCliqueKey key_;
  const std::vector<LocalParticipant>& local_participants_;
  const NcclUniqueIdCallback* callback_;

  StatusOr<NcclClique*> maybe_clique_;
  absl::BlockingCounter* counter_;
};

// Periodically checks all NCCL communicators for asynchronous errors.
// If an asynchronous error is observed, the communicator is aborted and an
// error message logged.
void CheckNcclAsyncErrors() {
  while (true) {
    absl::SleepFor(absl::Seconds(30));

    NcclCliqueCache().ForEach([](const auto&, const NcclClique& clique) {
      for (const auto& it : clique.GetComms()) {
        ncclComm_t comm = it.second.get();
        Status status = [comm] {
          ncclResult_t async_err;
          XLA_CUDA_RETURN_IF_ERROR(ncclCommGetAsyncError(comm, &async_err));
          if (async_err != ncclSuccess) {
            LOG(ERROR) << "Async NCCL error. Aborting communicator: " << comm;
            XLA_CUDA_RETURN_IF_ERROR(ncclCommAbort(comm));
          }
          return XLA_CUDA_STATUS(async_err);
        }();
        if (!status.ok()) LOG(ERROR) << status.ToString();
      }
    });
  }
}

}  // namespace

StatusOr<std::vector<LocalParticipant>> GetLocalParticipants(
    const std::vector<GlobalDeviceId>& participants,
    const std::vector<GlobalDeviceId>* local_devices) {
  std::vector<LocalParticipant> local_participants;
  if (local_devices) {
    absl::flat_hash_map<GlobalDeviceId, int> device_ranks(participants.size());
    for (int rank = 0; rank < participants.size(); ++rank) {
      auto result = device_ranks.emplace(participants[rank], rank);
      TF_RET_CHECK(result.second) << "Duplicate device found";
    }

    local_participants.reserve(local_devices->size());
    for (int device_ordinal = 0; device_ordinal < local_devices->size();
         ++device_ordinal) {
      auto it = device_ranks.find((*local_devices)[device_ordinal]);
      if (it != device_ranks.end()) {
        local_participants.push_back({device_ordinal, /*rank=*/it->second});
      }
    }
  } else {  // Single host, so use identity mapping (device ordinal == id).
    local_participants.reserve(participants.size());
    for (int rank = 0; rank < participants.size(); ++rank) {
      int device_ordinal = participants[rank].value();
      local_participants.push_back({device_ordinal, rank});
    }
  }

  return local_participants;
}

LockedNcclClique::LockedNcclClique(NcclClique& clique,
                                   std::unique_ptr<absl::MutexLock> lock,
                                   absl::BlockingCounter* counter)
    : clique(clique), lock_(std::move(lock)), counter_(counter) {}

LockedNcclClique::LockedNcclClique(LockedNcclClique&& other)
    : clique(other.clique),
      lock_(std::move(other.lock_)),
      counter_(std::exchange(other.counter_, nullptr)) {}

LockedNcclClique::~LockedNcclClique() {
  if (counter_) {
    counter_->DecrementCount();
    if (lock_) {
      counter_->Wait();  // Don't release lock until all threads are finished.
      delete counter_;
    }
  }
}

StatusOr<NcclClique*> NcclCliqueMap::GetOrTryCreateIfAbsent(
    const NcclCliqueKey& key,
    const std::function<StatusOr<std::unique_ptr<NcclClique>>(
        const NcclCliqueKey&)>& value_factory) {
  {
    absl::MutexLock lock(&mu_);
    auto it = map_.find(key);
    if (it != map_.end()) {
      return it->second.get();
    }
  }
  // We release the lock to allow different cliques to be created in parallel
  // (avoiding a potential deadlock in multi-host settings). This is safe
  // provided that there aren't two threads trying to create cliques with the
  // same key - which we know will not happen as this method is only called by
  // the primary thread from the clique rendezvous. If this assumption is not
  // valid, the method will return an error.
  TF_ASSIGN_OR_RETURN(std::unique_ptr<NcclClique> value, value_factory(key));
  absl::MutexLock lock(&mu_);
  auto result = map_.emplace(key, std::move(value));
  TF_RET_CHECK(result.second) << "Clique already in cache.";
  return result.first->second.get();
}

void NcclCliqueMap::ForEach(
    const std::function<void(const NcclCliqueKey&, const NcclClique&)>& fn) {
  absl::MutexLock lock(&mu_);
  for (const auto& kv : map_) {
    fn(kv.first, *kv.second);
  }
}

StatusOr<LockedNcclClique> AcquireNcclClique(
    const NcclCliqueParticipantData& participant,
    const std::vector<LocalParticipant>& local_participants,
    const NcclUniqueIdCallback* callback) {
  // Launch a thread to check for async NCCL errors.
  static auto check_async_error_thread =
      tensorflow::Env::Default()->StartThread(tensorflow::ThreadOptions(),
                                              "nccl_async_error_thread",
                                              CheckNcclAsyncErrors);
  (void)check_async_error_thread;  // Silence unused variable warning.

  VLOG(2) << "Rendezvous key: " << participant.rendezvous_key.ToString()
          << ", local participants: "
          << LocalParticipantsToString(local_participants);

  static auto& rendezvous_map =
      *new RefcountingHashMap<RendezvousKey, NcclCliqueRendezvous>();

  return NcclCliqueRendezvous::SubmitParticipant(
      /*rendezvous_getter=*/
      [&, participant] {
        return rendezvous_map.GetOrCreateIfAbsent(
            participant.rendezvous_key,
            [&](const RendezvousKey& rendezvous_key) {
              return std::make_unique<NcclCliqueRendezvous>(
                  participant.rendezvous_key, local_participants, callback);
            });
      },
      participant);
}

}  // namespace gpu
}  // namespace xla
