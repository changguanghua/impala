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


#ifndef IMPALA_EXPRS_STRING_LITERAL_H_
#define IMPALA_EXPRS_STRING_LITERAL_H_

#include <string>
#include "exprs/expr.h"
#include "runtime/string-value.h"

namespace impala {

class TExprNode;

class StringLiteral: public Expr {
 public:
  virtual llvm::Function* Codegen(LlvmCodeGen* code_gen);

 protected:
  friend class Expr;

  // Construct a StringLiteral expr from str.
  StringLiteral(const StringValue& str);
  StringLiteral(const std::string& str);
  StringLiteral(const TExprNode& node);

  virtual Status Prepare(RuntimeState* state, const RowDescriptor& row_desc);
  virtual std::string DebugString() const;

 private:
  static void* ComputeFn(Expr* e, TupleRow* row);
};

}

#endif
