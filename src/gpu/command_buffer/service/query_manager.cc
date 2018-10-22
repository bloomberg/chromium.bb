// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/query_manager.h"

#include <stddef.h>
#include <stdint.h>

#include "base/atomicops.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/shared_memory.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_fence.h"

namespace gpu {

namespace {

class CommandsIssuedQuery : public QueryManager::Query {
 public:
  CommandsIssuedQuery(QueryManager* manager,
                      GLenum target,
                      scoped_refptr<gpu::Buffer> buffer,
                      QuerySync* sync);

  void Begin() override;
  void End(base::subtle::Atomic32 submit_count) override;
  void QueryCounter(base::subtle::Atomic32 submit_count) override;
  void Pause() override;
  void Resume() override;
  void Process(bool did_finish) override;
  void Destroy(bool have_context) override;

 protected:
  ~CommandsIssuedQuery() override;

 private:
  base::TimeTicks begin_time_;
};

CommandsIssuedQuery::CommandsIssuedQuery(QueryManager* manager,
                                         GLenum target,
                                         scoped_refptr<gpu::Buffer> buffer,
                                         QuerySync* sync)
    : Query(manager, target, std::move(buffer), sync) {}

void CommandsIssuedQuery::Begin() {
  MarkAsActive();
  begin_time_ = base::TimeTicks::Now();
}

void CommandsIssuedQuery::Pause() {
  MarkAsPaused();
}

void CommandsIssuedQuery::Resume() {
  MarkAsActive();
}

void CommandsIssuedQuery::End(base::subtle::Atomic32 submit_count) {
  const base::TimeDelta elapsed = base::TimeTicks::Now() - begin_time_;
  MarkAsPending(submit_count);
  MarkAsCompleted(elapsed.InMicroseconds());
}

void CommandsIssuedQuery::QueryCounter(base::subtle::Atomic32 submit_count) {
  NOTREACHED();
}

void CommandsIssuedQuery::Process(bool did_finish) {
  NOTREACHED();
}

void CommandsIssuedQuery::Destroy(bool /* have_context */) {
  if (!IsDeleted()) {
    MarkAsDeleted();
  }
}

CommandsIssuedQuery::~CommandsIssuedQuery() = default;

class CommandsCompletedQuery : public QueryManager::Query {
 public:
  CommandsCompletedQuery(QueryManager* manager,
                         GLenum target,
                         scoped_refptr<gpu::Buffer> buffer,
                         QuerySync* sync);

  // Overridden from QueryManager::Query:
  void Begin() override;
  void End(base::subtle::Atomic32 submit_count) override;
  void QueryCounter(base::subtle::Atomic32 submit_count) override;
  void Pause() override;
  void Resume() override;
  void Process(bool did_finish) override;
  void Destroy(bool have_context) override;

 protected:
  ~CommandsCompletedQuery() override;

 private:
  std::unique_ptr<gl::GLFence> fence_;
  base::TimeTicks begin_time_;
};

CommandsCompletedQuery::CommandsCompletedQuery(
    QueryManager* manager,
    GLenum target,
    scoped_refptr<gpu::Buffer> buffer,
    QuerySync* sync)
    : Query(manager, target, std::move(buffer), sync) {}

void CommandsCompletedQuery::Begin() {
  MarkAsActive();
  begin_time_ = base::TimeTicks::Now();
}

void CommandsCompletedQuery::Pause() {
  MarkAsPaused();
}

void CommandsCompletedQuery::Resume() {
  MarkAsActive();
}

void CommandsCompletedQuery::End(base::subtle::Atomic32 submit_count) {
  if (fence_ && fence_->ResetSupported()) {
    fence_->ResetState();
  } else {
    fence_ = gl::GLFence::Create();
  }
  DCHECK(fence_);
  AddToPendingQueue(submit_count);
}

void CommandsCompletedQuery::QueryCounter(base::subtle::Atomic32 submit_count) {
  NOTREACHED();
}

void CommandsCompletedQuery::Process(bool did_finish) {
  // Note: |did_finish| guarantees that the GPU has passed the fence but
  // we cannot assume that GLFence::HasCompleted() will return true yet as
  // that's not guaranteed by all GLFence implementations.
  if (!did_finish && fence_ && !fence_->HasCompleted())
    return;

  const base::TimeDelta elapsed = base::TimeTicks::Now() - begin_time_;
  MarkAsCompleted(elapsed.InMicroseconds());
}

void CommandsCompletedQuery::Destroy(bool have_context) {
  if (have_context && !IsDeleted()) {
    fence_.reset();
    MarkAsDeleted();
  } else if (fence_ && !have_context) {
    fence_->Invalidate();
  }
}

CommandsCompletedQuery::~CommandsCompletedQuery() = default;

}  // namespace

QueryManager::QueryManager() : query_count_(0) {}

QueryManager::~QueryManager() {
  DCHECK(queries_.empty());

  // If this triggers, that means something is keeping a reference to
  // a Query belonging to this.
  CHECK_EQ(query_count_, 0u);
}

void QueryManager::Destroy(bool have_context) {
  active_queries_.clear();
  pending_queries_.clear();
  while (!queries_.empty()) {
    Query* query = queries_.begin()->second.get();
    query->Destroy(have_context);
    queries_.erase(queries_.begin());
  }
}

QueryManager::Query* QueryManager::CreateQuery(
    GLenum target,
    GLuint client_id,
    scoped_refptr<gpu::Buffer> buffer,
    QuerySync* sync) {
  scoped_refptr<Query> query;
  switch (target) {
    case GL_COMMANDS_ISSUED_CHROMIUM:
      query = new CommandsIssuedQuery(this, target, std::move(buffer), sync);
      break;
    case GL_READBACK_SHADOW_COPIES_UPDATED_CHROMIUM:
    case GL_COMMANDS_COMPLETED_CHROMIUM:
      query = new CommandsCompletedQuery(this, target, std::move(buffer), sync);
      break;
    default: {
      NOTREACHED();
    }
  }
  std::pair<QueryMap::iterator, bool> result =
      queries_.insert(std::make_pair(client_id, query));
  DCHECK(result.second);
  return query.get();
}

void QueryManager::GenQueries(GLsizei n, const GLuint* queries) {
  DCHECK_GE(n, 0);
  for (GLsizei i = 0; i < n; ++i) {
    generated_query_ids_.insert(queries[i]);
  }
}

bool QueryManager::IsValidQuery(GLuint id) {
  GeneratedQueryIds::iterator it = generated_query_ids_.find(id);
  return it != generated_query_ids_.end();
}

QueryManager::Query* QueryManager::GetQuery(GLuint client_id) {
  QueryMap::iterator it = queries_.find(client_id);
  return it != queries_.end() ? it->second.get() : nullptr;
}

QueryManager::Query* QueryManager::GetActiveQuery(GLenum target) {
  ActiveQueryMap::iterator it = active_queries_.find(target);
  return it != active_queries_.end() ? it->second.get() : nullptr;
}

void QueryManager::RemoveQuery(GLuint client_id) {
  QueryMap::iterator it = queries_.find(client_id);
  if (it != queries_.end()) {
    Query* query = it->second.get();

    // Remove from active query map if it is active.
    ActiveQueryMap::iterator active_it = active_queries_.find(query->target());
    bool is_active = (active_it != active_queries_.end() &&
                      query == active_it->second.get());
    DCHECK(is_active == query->IsActive());
    if (is_active)
      active_queries_.erase(active_it);

    query->Destroy(true);
    RemovePendingQuery(query);
    query->MarkAsDeleted();
    queries_.erase(it);
  }
  generated_query_ids_.erase(client_id);
}

void QueryManager::StartTracking(QueryManager::Query* /* query */) {
  ++query_count_;
}

void QueryManager::StopTracking(QueryManager::Query* /* query */) {
  --query_count_;
}

GLenum QueryManager::AdjustTargetForEmulation(GLenum target) {
  return target;
}

void QueryManager::BeginQueryHelper(GLenum target, GLuint id) {
  target = AdjustTargetForEmulation(target);
  glBeginQuery(target, id);
}

void QueryManager::EndQueryHelper(GLenum target) {
  target = AdjustTargetForEmulation(target);
  glEndQuery(target);
}

QueryManager::Query::Query(QueryManager* manager,
                           GLenum target,
                           scoped_refptr<gpu::Buffer> buffer,
                           QuerySync* sync)
    : manager_(manager),
      target_(target),
      buffer_(std::move(buffer)),
      sync_(sync),
      submit_count_(0),
      query_state_(kQueryState_Initialize),
      deleted_(false) {
  DCHECK(manager);
  manager_->StartTracking(this);
}

void QueryManager::Query::RunCallbacks() {
  for (size_t i = 0; i < callbacks_.size(); i++) {
    std::move(callbacks_[i]).Run();
  }
  callbacks_.clear();
}

void QueryManager::Query::AddCallback(base::OnceClosure callback) {
  if (query_state_ == kQueryState_Pending) {
    callbacks_.push_back(std::move(callback));
  } else {
    std::move(callback).Run();
  }
}

QueryManager::Query::~Query() {
  // The query is getting deleted, either by the client or
  // because the context was lost. Call any outstanding
  // callbacks to avoid leaks.
  RunCallbacks();
  if (manager_) {
    manager_->StopTracking(this);
    manager_ = nullptr;
  }
}

void QueryManager::Query::MarkAsCompleted(uint64_t result) {
  UnmarkAsPending();

  sync_->result = result;
  base::subtle::Release_Store(&sync_->process_count, submit_count_);
  RunCallbacks();
}

void QueryManager::ProcessPendingQueries(bool did_finish) {
  while (!pending_queries_.empty()) {
    Query* query = pending_queries_.front().get();
    query->Process(did_finish);
    if (query->IsPending()) {
      break;
    }
    pending_queries_.pop_front();
  }
  // If glFinish() has been called, all of our queries should be completed.
  DCHECK(!did_finish || pending_queries_.empty());
}

bool QueryManager::HavePendingQueries() {
  return !pending_queries_.empty();
}

void QueryManager::AddPendingQuery(Query* query,
                                   base::subtle::Atomic32 submit_count) {
  DCHECK(query);
  DCHECK(!query->IsDeleted());
  RemovePendingQuery(query);
  query->MarkAsPending(submit_count);
  pending_queries_.push_back(query);
}

void QueryManager::RemovePendingQuery(Query* query) {
  DCHECK(query);
  if (query->IsPending()) {
    // TODO(gman): Speed this up if this is a common operation. This would only
    // happen if you do being/end begin/end on the same query without waiting
    // for the first one to finish.
    for (QueryQueue::iterator it = pending_queries_.begin();
         it != pending_queries_.end(); ++it) {
      if (it->get() == query) {
        pending_queries_.erase(it);
        break;
      }
    }
    query->MarkAsCompleted(0);
  }
}

void QueryManager::BeginQuery(Query* query) {
  DCHECK(query);
  RemovePendingQuery(query);
  query->Begin();
  active_queries_[query->target()] = query;
}

void QueryManager::EndQuery(Query* query, base::subtle::Atomic32 submit_count) {
  DCHECK(query);
  RemovePendingQuery(query);

  // Remove from active query map if it is active.
  ActiveQueryMap::iterator active_it = active_queries_.find(query->target());
  DCHECK(active_it != active_queries_.end());
  DCHECK(query == active_it->second.get());
  active_queries_.erase(active_it);

  query->End(submit_count);
}

void QueryManager::QueryCounter(Query* query,
                                base::subtle::Atomic32 submit_count) {
  DCHECK(query);
  RemovePendingQuery(query);
  query->QueryCounter(submit_count);
}

void QueryManager::PauseQueries() {
  for (std::pair<const GLenum, scoped_refptr<Query> >& it : active_queries_) {
    if (it.second->IsActive()) {
      it.second->Pause();
      DCHECK(it.second->IsPaused());
    }
  }
}

void QueryManager::ResumeQueries() {
  for (std::pair<const GLenum, scoped_refptr<Query> >& it : active_queries_) {
    if (it.second->IsPaused()) {
      it.second->Resume();
      DCHECK(it.second->IsActive());
    }
  }
}

}  // namespace gpu
