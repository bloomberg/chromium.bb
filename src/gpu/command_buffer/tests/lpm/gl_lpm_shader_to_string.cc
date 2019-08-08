// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/tests/lpm/gl_lpm_shader_to_string.h"

#include <ostream>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"

namespace gl_lpm_fuzzer {

std::string GetFunctionName(const fuzzing::FunctionName& function_name) {
  if (function_name == fuzzing::MAIN) {
    return "main";
  }
  return "f" + base::NumberToString(function_name);
}

std::string GetType(const fuzzing::Type& type) {
  switch (type) {
    case fuzzing::VOID_TYPE: {
      return "void";
    }
    case fuzzing::INT: {
      return "int";
    }
  }
  CHECK(false);
  return "";
}

std::ostream& operator<<(std::ostream& os, const fuzzing::Statement& statement);
std::ostream& operator<<(std::ostream& os, const fuzzing::Rvalue& rvalue);

std::ostream& operator<<(std::ostream& os, const fuzzing::Block& block) {
  for (const fuzzing::Statement& statement : block.statements()) {
    os << statement;
  }
  return os;
}

std::ostream& operator<<(std::ostream& os, const fuzzing::IfElse& ifelse) {
  return os << "if (" << ifelse.cond() << ") {\n"
            << ifelse.if_body() << "} else {\n"
            << ifelse.else_body() << "}\n";
}

std::ostream& operator<<(std::ostream& os, const fuzzing::Const& cons) {
  return os << base::NumberToString(cons.val());
}

std::string GetOp(const fuzzing::BinaryOp::Op op) {
  switch (op) {
    case fuzzing::BinaryOp::PLUS:
      return "+";
    case fuzzing::BinaryOp::MINUS:
      return "-";
    case fuzzing::BinaryOp::MUL:
      return "*";
    case fuzzing::BinaryOp::DIV:
      return "/";
    case fuzzing::BinaryOp::MOD:
      return "%";
    case fuzzing::BinaryOp::XOR:
      return "^";
    case fuzzing::BinaryOp::AND:
      return "&&";
    case fuzzing::BinaryOp::OR:
      return "||";
    case fuzzing::BinaryOp::EQ:
      return "==";
    case fuzzing::BinaryOp::NE:
      return "!=";
    case fuzzing::BinaryOp::LE:
      return "<=";
    case fuzzing::BinaryOp::GE:
      return ">=";
    case fuzzing::BinaryOp::LT:
      return "<";
    case fuzzing::BinaryOp::GT:
      return ">";
    default:
      DCHECK(false);
  }
  return "";
}

std::ostream& operator<<(std::ostream& os, const fuzzing::BinaryOp& binary_op) {
  return os << "(" << binary_op.left() << " " << GetOp(binary_op.op()) << " "
            << binary_op.right() << ")";
}

std::ostream& operator<<(std::ostream& os, const fuzzing::Rvalue& rvalue) {
  switch (rvalue.rvalue_case()) {
    case fuzzing::Rvalue::kVar: {
      os << rvalue.var();
      break;
    }
    case fuzzing::Rvalue::kCons: {
      os << rvalue.cons();
      break;
    }
    case fuzzing::Rvalue::kBinaryOp: {
      os << rvalue.binary_op();
      break;
    }
    case fuzzing::Rvalue::RVALUE_NOT_SET: {
      os << "1";
      break;
    }
  }
  return os;
}

std::ostream& operator<<(std::ostream& os, const fuzzing::While& while_stmt) {
  return os << "while (" << while_stmt.cond() << ") {\n"
            << while_stmt.body() << "}\n";
}

std::string GetVar(const fuzzing::Var& var) {
  return "var" + base::NumberToString(var);
}

std::ostream& operator<<(std::ostream& os, const fuzzing::Lvalue& lvalue) {
  return os << GetVar(lvalue.var());
}

std::ostream& operator<<(std::ostream& os,
                         const fuzzing::Assignment& assignment) {
  return os << assignment.lvalue() << " = " << assignment.rvalue() << ";\n";
}

std::ostream& operator<<(std::ostream& os, const fuzzing::Declare& declare) {
  return os << GetType(declare.type()) << " " << GetVar(declare.var()) << ";\n";
}

std::ostream& operator<<(std::ostream& os,
                         const fuzzing::Statement& statement) {
  switch (statement.statement_case()) {
    case fuzzing::Statement::STATEMENT_NOT_SET: {
      break;
    }
    case fuzzing::Statement::kAssignment: {
      os << statement.assignment();
      break;
    }
    case fuzzing::Statement::kIfelse: {
      os << statement.ifelse();
      break;
    }
    case fuzzing::Statement::kWhileStmt: {
      os << statement.while_stmt();
      break;
    }
    case fuzzing::Statement::kReturnStmt: {
      os << "return " << statement.return_stmt() << ";\n";
      break;
    }
    case fuzzing::Statement::kDeclare: {
      os << statement.declare();
      break;
    }
  }
  return os;
}

std::ostream& operator<<(std::ostream& os, const fuzzing::Function& function) {
  os << GetType(function.type()) << " "
     << GetFunctionName(function.function_name()) << "() {\n";
  os << function.block();
  os << "return " << function.return_stmt() << ";\n";
  os << "}";
  return os;
}

std::ostream& operator<<(std::ostream& os, const fuzzing::Shader& shader) {
  int i = 0;
  for (const fuzzing::Function& function : shader.functions()) {
    os << function;
    if (i < shader.functions().size() - 1) {
      os << "\n";
    }
    i++;
  }
  return os;
}

std::string GetShader(const fuzzing::Shader& shader) {
  std::ostringstream os;
  os << shader;
  return os.str();
}

}  // namespace gl_lpm_fuzzer
