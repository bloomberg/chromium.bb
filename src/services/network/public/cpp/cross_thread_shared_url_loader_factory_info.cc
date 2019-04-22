// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cross_thread_shared_url_loader_factory_info.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace network {

// State and methods that need to live on the same sequence |task_runner_|
// as the wrapped SharedURLLoaderFactory |base_factory_|.
//
// Used by both CrossThreadSharedURLLoaderFactoryInfo and
// CrossThreadSharedURLLoaderFactory, and shared across chains of
// CreateFactory() and Clone() calls. Ref count accommodates both this sharing,
// as well as lifetime management for cross-thread calls into the object.
class CrossThreadSharedURLLoaderFactoryInfo::State
    : public base::RefCountedThreadSafe<State, StateDeleterTraits> {
 public:
  explicit State(scoped_refptr<SharedURLLoaderFactory> base_factory);

  // |this| must be deleted on same thread as |base_factory_| as the refcount
  // on SharedURLLoaderFactory is not thread-safe.
  void DeleteOnCorrectThread() const;

  // Delegation for mojom::URLLoaderFactory API.
  void CreateLoaderAndStart(
      mojom::URLLoaderRequest loader,
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      const ResourceRequest& request,
      mojom::URLLoaderClientPtrInfo client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation);

  void Clone(mojom::URLLoaderFactoryRequest request);

  // Sequence |base_factory()| and |this| run on.
  base::SequencedTaskRunner* task_runner() const { return task_runner_.get(); }
  SharedURLLoaderFactory* base_factory() const { return base_factory_.get(); }

 private:
  // To permit use of TaskRunner::DeleteSoon.
  friend class base::DeleteHelper<State>;
  ~State();

  scoped_refptr<SharedURLLoaderFactory> base_factory_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  SEQUENCE_CHECKER(sequence_checker_);
};

struct CrossThreadSharedURLLoaderFactoryInfo::StateDeleterTraits {
  static void Destruct(const State* state) { state->DeleteOnCorrectThread(); }
};

// The implementation of SharedURLLoaderFactory provided by
// CrossThreadSharedURLLoaderFactoryInfo::CreateFactory(). Uses the exact same
// State object, and posts URLLoaderFactory API calls to it on the appropriate
// thread.
class CrossThreadSharedURLLoaderFactory : public SharedURLLoaderFactory {
 public:
  using State = CrossThreadSharedURLLoaderFactoryInfo::State;

  // |state| contains information on the SharedURLLoaderFactory to wrap, and
  // what thread it runs on, and may be shared with other
  // CrossThreadSharedURLLoaderFactory and CrossThreadSharedURLLoaderFactoryInfo
  // objects wrapping the same SharedURLLoaderFactory.
  explicit CrossThreadSharedURLLoaderFactory(scoped_refptr<State> state);

  // mojom::URLLoaderFactory implementation:
  void CreateLoaderAndStart(mojom::URLLoaderRequest loader,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const ResourceRequest& request,
                            mojom::URLLoaderClientPtr client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override;
  void Clone(mojom::URLLoaderFactoryRequest request) override;

  // SharedURLLoaderFactory implementation:
  std::unique_ptr<SharedURLLoaderFactoryInfo> Clone() override;

 private:
  ~CrossThreadSharedURLLoaderFactory() override;
  scoped_refptr<State> state_;

  // CrossThreadSharedURLLoaderFactory should live on a consistent thread as
  // well, though one that may be different from |state_->task_runner()|.
  SEQUENCE_CHECKER(sequence_checker_);
};

CrossThreadSharedURLLoaderFactory::CrossThreadSharedURLLoaderFactory(
    scoped_refptr<CrossThreadSharedURLLoaderFactoryInfo::State> state)
    : state_(std::move(state)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

CrossThreadSharedURLLoaderFactory::~CrossThreadSharedURLLoaderFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void CrossThreadSharedURLLoaderFactory::CreateLoaderAndStart(
    mojom::URLLoaderRequest loader,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const ResourceRequest& request,
    mojom::URLLoaderClientPtr client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::SequencedTaskRunner* runner = state_->task_runner();
  if (runner->RunsTasksInCurrentSequence()) {
    state_->base_factory()->CreateLoaderAndStart(
        std::move(loader), routing_id, request_id, options, request,
        std::move(client), traffic_annotation);
  } else {
    state_->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&State::CreateLoaderAndStart, state_, std::move(loader),
                       routing_id, request_id, options, request,
                       client.PassInterface(), traffic_annotation));
  }
}

void CrossThreadSharedURLLoaderFactory::Clone(
    mojom::URLLoaderFactoryRequest request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::SequencedTaskRunner* runner = state_->task_runner();
  if (runner->RunsTasksInCurrentSequence()) {
    state_->base_factory()->Clone(std::move(request));
  } else {
    state_->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&State::Clone, state_, std::move(request)));
  }
}

std::unique_ptr<SharedURLLoaderFactoryInfo>
CrossThreadSharedURLLoaderFactory::Clone() {
  return base::WrapUnique(new CrossThreadSharedURLLoaderFactoryInfo(state_));
}

CrossThreadSharedURLLoaderFactoryInfo::CrossThreadSharedURLLoaderFactoryInfo(
    scoped_refptr<SharedURLLoaderFactory> url_loader_factory)
    : state_(base::MakeRefCounted<State>(std::move(url_loader_factory))) {}

CrossThreadSharedURLLoaderFactoryInfo::
    ~CrossThreadSharedURLLoaderFactoryInfo() = default;

scoped_refptr<SharedURLLoaderFactory>
CrossThreadSharedURLLoaderFactoryInfo::CreateFactory() {
  return base::MakeRefCounted<CrossThreadSharedURLLoaderFactory>(state_);
}

CrossThreadSharedURLLoaderFactoryInfo::CrossThreadSharedURLLoaderFactoryInfo(
    scoped_refptr<State> state)
    : state_(std::move(state)) {}

CrossThreadSharedURLLoaderFactoryInfo::State::State(
    scoped_refptr<SharedURLLoaderFactory> base_factory)
    : base_factory_(std::move(base_factory)),
      task_runner_(base::SequencedTaskRunnerHandle::Get()) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

CrossThreadSharedURLLoaderFactoryInfo::State::~State() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void CrossThreadSharedURLLoaderFactoryInfo::State::DeleteOnCorrectThread()
    const {
  if (task_runner_->RunsTasksInCurrentSequence())
    delete this;
  else
    task_runner_->DeleteSoon(FROM_HERE, this);
}

void CrossThreadSharedURLLoaderFactoryInfo::State::CreateLoaderAndStart(
    mojom::URLLoaderRequest loader,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const ResourceRequest& request,
    mojom::URLLoaderClientPtrInfo client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base_factory_->CreateLoaderAndStart(
      std::move(loader), routing_id, request_id, options, request,
      mojom::URLLoaderClientPtr(std::move(client)), traffic_annotation);
}

void CrossThreadSharedURLLoaderFactoryInfo::State::Clone(
    mojom::URLLoaderFactoryRequest request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base_factory_->Clone(std::move(request));
}

}  // namespace network
