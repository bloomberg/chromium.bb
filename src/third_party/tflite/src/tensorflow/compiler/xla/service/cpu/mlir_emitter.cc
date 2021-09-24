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

#include "tensorflow/compiler/xla/service/cpu/mlir_emitter.h"

#include "llvm/Linker/Linker.h"
#include "llvm/Transforms/IPO/Internalize.h"
#include "mlir/Conversion/AffineToStandard/AffineToStandard.h"  // from @llvm-project
#include "mlir/Conversion/SCFToStandard/SCFToStandard.h"  // from @llvm-project
#include "mlir/Conversion/VectorToLLVM/ConvertVectorToLLVM.h"  // from @llvm-project
#include "mlir/Dialect/Linalg/Passes.h"  // from @llvm-project
#include "mlir/IR/BuiltinOps.h"  // from @llvm-project
#include "mlir/Pass/Pass.h"  // from @llvm-project
#include "mlir/Pass/PassManager.h"  // from @llvm-project
#include "mlir/Target/LLVMIR/Export.h"  // from @llvm-project
#include "mlir/Transforms/Passes.h"  // from @llvm-project
#include "tensorflow/compiler/mlir/xla/hlo_utils.h"

namespace xla {
namespace cpu {
namespace {

// Lower an MLIR module to an LLVM module.
std::unique_ptr<llvm::Module> MakeLLVMModule(mlir::OwningModuleRef module,
                                             llvm::LLVMContext *context) {
  // When set, the LLVM backend will be allowed to reassociate floating-point
  // reductions, which enables much more efficient "horizontal" SIMD
  // implementations.
  // TODO(kramerb): link this to the right option, command line flag, etc.
  constexpr bool kReassociateFPReductions = true;

  mlir::PassManager manager(module->getContext(),
                            mlir::OpPassManager::Nesting::Implicit);
  manager.addPass(mlir::createConvertLinalgToLoopsPass());
  manager.addPass(mlir::createLowerAffinePass());
  manager.addPass(mlir::createLowerToCFGPass());
  manager.addPass(mlir::createConvertVectorToLLVMPass(
      mlir::LowerVectorToLLVMOptions().setReassociateFPReductions(
          kReassociateFPReductions)));
  CHECK(succeeded(manager.run(*module)));
  return mlir::translateModuleToLLVMIR(*module, *context);
}

// Get arguments to pass a memref to an mlir function.
void BuildViewForBuffer(llvm::SmallVectorImpl<llvm::Value *> *args,
                        llvm::IRBuilder<> *b, const Shape &opShape,
                        llvm::Value *op_val) {
  llvm::Type *ty = op_val->getType();
  while (auto aty = llvm::dyn_cast<llvm::ArrayType>(
             llvm::cast<llvm::PointerType>(ty)->getElementType())) {
    ty = aty->getElementType()->getPointerTo();
  }
  op_val = b->CreateBitCast(op_val, ty);

  args->push_back(op_val);          // Allocated pointer.
  args->push_back(op_val);          // Aligned pointer.
  args->push_back(b->getInt64(0));  // Offset.

  // Sizes.
  for (int64_t dim : opShape.dimensions()) {
    args->push_back(b->getInt64(dim));
  }

  int64_t accumulated_stride = 1;
  llvm::SmallVector<int64_t, 4> strides(opShape.rank(), 1);
  for (int64_t dim : LayoutUtil::MinorToMajor(opShape)) {
    strides[dim] = accumulated_stride;
    accumulated_stride *= opShape.dimensions(dim);
  }

  // Strides.
  for (int64_t stride : strides) {
    args->push_back(b->getInt64(stride));
  }
}
}  // namespace

Status EmitMlirFuncAndCall(
    mlir::MLIRContext *context, llvm::IRBuilder<> *b, const Shape &result_shape,
    llvm::ArrayRef<Shape> operand_shapes, llvm::Value *result_ptr,
    llvm::ArrayRef<llvm::Value *> operand_ptrs, llvm::StringRef func_name,
    llvm::function_ref<void(mlir::OpBuilder *, mlir::FuncOp)> emitter) {
  llvm::Module *llvm_module = b->GetInsertBlock()->getParent()->getParent();
  mlir::Builder mlir_builder(context);

  // Get memref types for the inputs and output.
  TF_ASSIGN_OR_RETURN(mlir::Type ret_memref, ConvertTensorShapeToMemRefType(
                                                 result_shape, mlir_builder));
  std::vector<mlir::Type> operand_types = {ret_memref};
  for (int i = 0; i != operand_shapes.size(); ++i) {
    TF_ASSIGN_OR_RETURN(
        mlir::Type op_memref,
        ConvertTensorShapeToMemRefType(operand_shapes[i], mlir_builder));
    operand_types.push_back(op_memref);
  }

  // Create the function an call the emission callback.
  mlir::Location loc = mlir::UnknownLoc::get(context);
  auto function = mlir::FuncOp::create(
      loc, func_name, mlir::FunctionType::get(context, operand_types, {}));
  function.addEntryBlock();
  mlir::OwningModuleRef mlir_module = mlir::ModuleOp::create(loc);
  mlir_module->push_back(function);
  mlir::OpBuilder op_builder(&function.getBody());
  emitter(&op_builder, function);

  // Now link it all into the main LLVM module.
  auto mlir_llvm_module =
      MakeLLVMModule(std::move(mlir_module), &b->getContext());
  mlir_llvm_module->setDataLayout(llvm_module->getDataLayout());
  llvm::Linker::linkModules(
      *llvm_module, std::move(mlir_llvm_module), llvm::Linker::None,
      [](llvm::Module &M, const llvm::StringSet<> &GVS) {
        llvm::internalizeModule(M, [&GVS](const llvm::GlobalValue &GV) {
          return !GV.hasName() || (GVS.count(GV.getName()) == 0);
        });
      });

  // And leave behind a call to the function generated by MLIR.
  llvm::Function *func = llvm_module->getFunction(func_name);
  llvm::SmallVector<llvm::Value *, 4> op_vals;
  BuildViewForBuffer(&op_vals, b, result_shape, result_ptr);
  for (int i = 0; i != operand_shapes.size(); ++i) {
    BuildViewForBuffer(&op_vals, b, operand_shapes[i], operand_ptrs[i]);
  }
  b->CreateCall(func, op_vals);

  return Status::OK();
}

}  // namespace cpu
}  // namespace xla
