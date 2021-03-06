// Copyright 2012 Cloudera Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "exec/select-node.h"
#include "exprs/expr.h"
#include "runtime/row-batch.h"
#include "runtime/runtime-state.h"
#include "runtime/raw-value.h"
#include "gen-cpp/PlanNodes_types.h"

using namespace std;

namespace impala {

SelectNode::SelectNode(
    ObjectPool* pool, const TPlanNode& tnode, const DescriptorTbl& descs)
    : ExecNode(pool, tnode, descs),
      child_row_batch_(NULL),
      child_row_idx_(0),
      child_eos_(false) {
}

Status SelectNode::Prepare(RuntimeState* state) {
  RETURN_IF_ERROR(ExecNode::Prepare(state));
  child_row_batch_.reset(
      new RowBatch(child(0)->row_desc(), state->batch_size()));
  return Status::OK;
}

Status SelectNode::Open(RuntimeState* state) {
  RETURN_IF_ERROR(ExecDebugAction(TExecNodePhase::OPEN));
  RETURN_IF_ERROR(child(0)->Open(state));
  return Status::OK;
}

Status SelectNode::GetNext(RuntimeState* state, RowBatch* row_batch, bool* eos) {
  RETURN_IF_ERROR(ExecDebugAction(TExecNodePhase::GETNEXT));
  RETURN_IF_CANCELLED(state);
  SCOPED_TIMER(runtime_profile_->total_time_counter());

  if (ReachedLimit() || (child_row_idx_ == child_row_batch_->num_rows() && child_eos_)) {
    // we're already done or we exhausted the last child batch and there won't be any
    // new ones
    *eos = true;
    return Status::OK;
  }

  // start (or continue) consuming row batches from child
  while (true) {
    if (child_row_idx_ == child_row_batch_->num_rows()) {
      // fetch next batch
      RETURN_IF_CANCELLED(state);
      child_row_batch_->Reset();
      RETURN_IF_ERROR(child(0)->GetNext(state, child_row_batch_.get(), &child_eos_));
      child_row_idx_ = 0;
    }

    if (CopyRows(row_batch)) {
      *eos = ReachedLimit()
          || (child_row_idx_ == child_row_batch_->num_rows() && child_eos_);
      return Status::OK;
    }
    if (child_eos_) {
      // finished w/ last child row batch, and child eos is true
      *eos = true;
      return Status::OK;
    }
  }
  return Status::OK;
}

bool SelectNode::CopyRows(RowBatch* output_batch) {
  Expr** conjuncts = &conjuncts_[0];
  int num_conjuncts = conjuncts_.size();

  for (; child_row_idx_ < child_row_batch_->num_rows(); ++child_row_idx_) {
    // Add a new row to output_batch
    int dst_row_idx = output_batch->AddRow();
    if (dst_row_idx == RowBatch::INVALID_ROW_INDEX) return true;
    TupleRow* dst_row = output_batch->GetRow(dst_row_idx);
    TupleRow* src_row = child_row_batch_->GetRow(child_row_idx_);

    if (EvalConjuncts(conjuncts, num_conjuncts, src_row)) {
      output_batch->CopyRow(src_row, dst_row);
      output_batch->CommitLastRow();
      ++num_rows_returned_;
      COUNTER_SET(rows_returned_counter_, num_rows_returned_);
      if (ReachedLimit()) return true;
    }
  }
  return output_batch->IsFull() || output_batch->AtResourceLimit();
}

 Status SelectNode::Close(RuntimeState* state) {
  return ExecNode::Close(state);
}

}
