#!/usr/bin/env bash
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script will check out llvm and clang into third_party/llvm and build it.

# Do NOT CHANGE this if you don't know what you're doing -- see
# https://code.google.com/p/chromium/wiki/UpdatingClang
# Reverting problematic clang rolls is safe, though.
CLANG_REVISION=220284

THIS_DIR="$(dirname "${0}")"
LLVM_DIR="${THIS_DIR}/../../../third_party/llvm"
LLVM_BUILD_DIR="${LLVM_DIR}/../llvm-build/Release+Asserts"
COMPILER_RT_BUILD_DIR="${LLVM_DIR}/../llvm-build/compiler-rt"
LLVM_BOOTSTRAP_DIR="${LLVM_DIR}/../llvm-bootstrap"
LLVM_BOOTSTRAP_INSTALL_DIR="${LLVM_DIR}/../llvm-bootstrap-install"
CLANG_DIR="${LLVM_DIR}/tools/clang"
COMPILER_RT_DIR="${LLVM_DIR}/compiler-rt"
LIBCXX_DIR="${LLVM_DIR}/projects/libcxx"
LIBCXXABI_DIR="${LLVM_DIR}/projects/libcxxabi"
ANDROID_NDK_DIR="${THIS_DIR}/../../../third_party/android_tools/ndk"
STAMP_FILE="${LLVM_DIR}/../llvm-build/cr_build_revision"
CHROMIUM_TOOLS_DIR="${THIS_DIR}/.."

ABS_CHROMIUM_TOOLS_DIR="${PWD}/${CHROMIUM_TOOLS_DIR}"
ABS_LIBCXX_DIR="${PWD}/${LIBCXX_DIR}"
ABS_LIBCXXABI_DIR="${PWD}/${LIBCXXABI_DIR}"
ABS_LLVM_DIR="${PWD}/${LLVM_DIR}"
ABS_LLVM_BUILD_DIR="${PWD}/${LLVM_BUILD_DIR}"
ABS_COMPILER_RT_DIR="${PWD}/${COMPILER_RT_DIR}"


# Use both the clang revision and the plugin revisions to test for updates.
BLINKGCPLUGIN_REVISION=\
$(grep 'set(LIBRARYNAME' "$THIS_DIR"/../blink_gc_plugin/CMakeLists.txt \
    | cut -d ' ' -f 2 | tr -cd '[0-9]')
CLANG_AND_PLUGINS_REVISION="${CLANG_REVISION}-${BLINKGCPLUGIN_REVISION}"

# ${A:-a} returns $A if it's set, a else.
LLVM_REPO_URL=${LLVM_URL:-https://llvm.org/svn/llvm-project}

if [[ -z "$GYP_DEFINES" ]]; then
  GYP_DEFINES=
fi
if [[ -z "$GYP_GENERATORS" ]]; then
  GYP_GENERATORS=
fi


# Die if any command dies, error on undefined variable expansions.
set -eu

OS="$(uname -s)"

# Parse command line options.
if_needed=
force_local_build=
run_tests=
bootstrap=
with_android=yes
chrome_tools="plugins;blink_gc_plugin"
gcc_toolchain=

if [[ "${OS}" = "Darwin" ]]; then
  with_android=
fi

while [[ $# > 0 ]]; do
  case $1 in
    --bootstrap)
      bootstrap=yes
      ;;
    --if-needed)
      if_needed=yes
      ;;
    --force-local-build)
      force_local_build=yes
      ;;
    --print-revision)
      echo $CLANG_REVISION
      exit 0
      ;;
    --run-tests)
      run_tests=yes
      ;;
    --without-android)
      with_android=
      ;;
    --with-chrome-tools)
      shift
      if [[ $# == 0 ]]; then
        echo "--with-chrome-tools requires an argument."
        exit 1
      fi
      chrome_tools=$1
      ;;
    --gcc-toolchain)
      shift
      if [[ $# == 0 ]]; then
        echo "--gcc-toolchain requires an argument."
        exit 1
      fi
      if [[ -x "$1/bin/gcc" ]]; then
        gcc_toolchain=$1
      else
        echo "Invalid --gcc-toolchain: '$1'."
        echo "'$1/bin/gcc' does not appear to be valid."
        exit 1
      fi
      ;;

    --help)
      echo "usage: $0 [--force-local-build] [--if-needed] [--run-tests] "
      echo "--bootstrap: First build clang with CC, then with itself."
      echo "--force-local-build: Don't try to download prebuilt binaries."
      echo "--if-needed: Download clang only if the script thinks it is needed."
      echo "--run-tests: Run tests after building. Only for local builds."
      echo "--print-revision: Print current clang revision and exit."
      echo "--without-android: Don't build ASan Android runtime library."
      echo "--with-chrome-tools: Select which chrome tools to build." \
           "Defaults to plugins;blink_gc_plugin."
      echo "    Example: --with-chrome-tools plugins;empty-string"
      echo "--gcc-toolchain: Set the prefix for which GCC version should"
      echo "    be used for building. For example, to use gcc in"
      echo "    /opt/foo/bin/gcc, use '--gcc-toolchain '/opt/foo"
      echo
      exit 1
      ;;
    *)
      echo "Unknown argument: '$1'."
      echo "Use --help for help."
      exit 1
      ;;
  esac
  shift
done

if [[ -n "$if_needed" ]]; then
  if [[ "${OS}" == "Darwin" ]]; then
    # clang is used on Mac.
    true
  elif [[ "$GYP_DEFINES" =~ .*(clang|tsan|asan|lsan|msan)=1.* ]]; then
    # clang requested via $GYP_DEFINES.
    true
  elif [[ -d "${LLVM_BUILD_DIR}" ]]; then
    # clang previously downloaded, remove third_party/llvm-build to prevent
    # updating.
    true
  elif [[ "${OS}" == "Linux" ]]; then
    # Temporarily use clang on linux. Leave a stamp file behind, so that
    # this script can remove clang again on machines where it was autoinstalled.
    mkdir -p "${LLVM_BUILD_DIR}"
    touch "${LLVM_BUILD_DIR}/autoinstall_stamp"
    true
  else
    # clang wasn't needed, not doing anything.
    exit 0
  fi
fi


# Check if there's anything to be done, exit early if not.
if [[ -f "${STAMP_FILE}" ]]; then
  PREVIOUSLY_BUILT_REVISON=$(cat "${STAMP_FILE}")
  if [[ -z "$force_local_build" ]] && \
       [[ "${PREVIOUSLY_BUILT_REVISON}" = \
          "${CLANG_AND_PLUGINS_REVISION}" ]]; then
    echo "Clang already at ${CLANG_AND_PLUGINS_REVISION}"
    exit 0
  fi
fi
# To always force a new build if someone interrupts their build half way.
rm -f "${STAMP_FILE}"


if [[ -z "$force_local_build" ]]; then
  # Check if there's a prebuilt binary and if so just fetch that. That's faster,
  # and goma relies on having matching binary hashes on client and server too.
  CDS_URL=https://commondatastorage.googleapis.com/chromium-browser-clang
  CDS_FILE="clang-${CLANG_REVISION}.tgz"
  CDS_OUT_DIR=$(mktemp -d -t clang_download.XXXXXX)
  CDS_OUTPUT="${CDS_OUT_DIR}/${CDS_FILE}"
  if [ "${OS}" = "Linux" ]; then
    CDS_FULL_URL="${CDS_URL}/Linux_x64/${CDS_FILE}"
  elif [ "${OS}" = "Darwin" ]; then
    CDS_FULL_URL="${CDS_URL}/Mac/${CDS_FILE}"
  fi
  echo Trying to download prebuilt clang
  if which curl > /dev/null; then
    curl -L --fail "${CDS_FULL_URL}" -o "${CDS_OUTPUT}" || \
        rm -rf "${CDS_OUT_DIR}"
  elif which wget > /dev/null; then
    wget "${CDS_FULL_URL}" -O "${CDS_OUTPUT}" || rm -rf "${CDS_OUT_DIR}"
  else
    echo "Neither curl nor wget found. Please install one of these."
    exit 1
  fi
  if [ -f "${CDS_OUTPUT}" ]; then
    rm -rf "${LLVM_BUILD_DIR}"
    mkdir -p "${LLVM_BUILD_DIR}"
    tar -xzf "${CDS_OUTPUT}" -C "${LLVM_BUILD_DIR}"
    echo clang "${CLANG_REVISION}" unpacked
    echo "${CLANG_AND_PLUGINS_REVISION}" > "${STAMP_FILE}"
    rm -rf "${CDS_OUT_DIR}"
    exit 0
  else
    echo Did not find prebuilt clang at r"${CLANG_REVISION}", building
  fi
fi

if [[ -n "${with_android}" ]] && ! [[ -d "${ANDROID_NDK_DIR}" ]]; then
  echo "Android NDK not found at ${ANDROID_NDK_DIR}"
  echo "The Android NDK is needed to build a Clang whose -fsanitize=address"
  echo "works on Android. See "
  echo "http://code.google.com/p/chromium/wiki/AndroidBuildInstructions for how"
  echo "to install the NDK, or pass --without-android."
  exit 1
fi

# Check that cmake and ninja are available.
if ! which cmake > /dev/null; then
  echo "CMake needed to build clang; please install"
  exit 1
fi
if ! which ninja > /dev/null; then
  echo "ninja needed to build clang, please install"
  exit 1
fi

echo Reverting previously patched files
for i in \
      "${CLANG_DIR}/test/Index/crash-recovery-modules.m" \
      "${CLANG_DIR}/unittests/libclang/LibclangTest.cpp" \
      "${LLVM_DIR}/include/llvm/IR/IRBuilder.h" \
      "${LLVM_DIR}/include/llvm/IR/InstrTypes.h" \
      "${LLVM_DIR}/lib/Analysis/Loads.cpp" \
      "${LLVM_DIR}/lib/IR/Instructions.cpp" \
      "${LLVM_DIR}/lib/Transforms/InstCombine/InstCombineLoadStoreAlloca.cpp" \
      "${LLVM_DIR}/lib/Transforms/Scalar/JumpThreading.cpp" \
      "${LLVM_DIR}/test/Transforms/InstCombine/select.ll" \
      "${LLVM_DIR}/test/Bindings/Go/go.test" \
      "${COMPILER_RT_DIR}/lib/asan/asan_rtl.cc" \
      "${COMPILER_RT_DIR}/test/asan/TestCases/Linux/new_array_cookie_test.cc" \
      "${LLVM_DIR}/test/DebugInfo/gmlt.ll" \
      "${LLVM_DIR}/lib/CodeGen/SpillPlacement.cpp" \
      "${LLVM_DIR}/lib/CodeGen/SpillPlacement.h" \
      "${CLANG_DIR}/lib/Basic/SanitizerBlacklist.cpp" \
      "${CLANG_DIR}/test/CodeGen/address-safety-attr.cpp" \
      ; do
  if [[ -e "${i}" ]]; then
    svn revert "${i}"
  fi;
done

echo Remove the Clang tools shim dir
CHROME_TOOLS_SHIM_DIR=${ABS_LLVM_DIR}/tools/chrometools
rm -rfv ${CHROME_TOOLS_SHIM_DIR}

echo Getting LLVM r"${CLANG_REVISION}" in "${LLVM_DIR}"
if ! svn co --force "${LLVM_REPO_URL}/llvm/trunk@${CLANG_REVISION}" \
                    "${LLVM_DIR}"; then
  echo Checkout failed, retrying
  rm -rf "${LLVM_DIR}"
  svn co --force "${LLVM_REPO_URL}/llvm/trunk@${CLANG_REVISION}" "${LLVM_DIR}"
fi

echo Getting clang r"${CLANG_REVISION}" in "${CLANG_DIR}"
svn co --force "${LLVM_REPO_URL}/cfe/trunk@${CLANG_REVISION}" "${CLANG_DIR}"

# We have moved from building compiler-rt in the LLVM tree, to a separate
# directory. Nuke any previous checkout to avoid building it.
rm -rf "${LLVM_DIR}/projects/compiler-rt"

echo Getting compiler-rt r"${CLANG_REVISION}" in "${COMPILER_RT_DIR}"
svn co --force "${LLVM_REPO_URL}/compiler-rt/trunk@${CLANG_REVISION}" \
               "${COMPILER_RT_DIR}"

# clang needs a libc++ checkout, else -stdlib=libc++ won't find includes
# (i.e. this is needed for bootstrap builds).
if [ "${OS}" = "Darwin" ]; then
  echo Getting libc++ r"${CLANG_REVISION}" in "${LIBCXX_DIR}"
  svn co --force "${LLVM_REPO_URL}/libcxx/trunk@${CLANG_REVISION}" \
                 "${LIBCXX_DIR}"
fi

# While we're bundling our own libc++ on OS X, we need to compile libc++abi
# into it too (since OS X 10.6 doesn't have libc++abi.dylib either).
if [ "${OS}" = "Darwin" ]; then
  echo Getting libc++abi r"${CLANG_REVISION}" in "${LIBCXXABI_DIR}"
  svn co --force "${LLVM_REPO_URL}/libcxxabi/trunk@${CLANG_REVISION}" \
                 "${LIBCXXABI_DIR}"
fi

# Apply patch for tests failing with --disable-pthreads (llvm.org/PR11974)
pushd "${CLANG_DIR}"
cat << 'EOF' |
--- third_party/llvm/tools/clang/test/Index/crash-recovery-modules.m	(revision 202554)
+++ third_party/llvm/tools/clang/test/Index/crash-recovery-modules.m	(working copy)
@@ -12,6 +12,8 @@
 
 // REQUIRES: crash-recovery
 // REQUIRES: shell
+// XFAIL: *
+//    (PR11974)
 
 @import Crash;
EOF
patch -p4
popd

pushd "${CLANG_DIR}"
cat << 'EOF' |
--- unittests/libclang/LibclangTest.cpp (revision 215949)
+++ unittests/libclang/LibclangTest.cpp (working copy)
@@ -431,7 +431,7 @@
   EXPECT_EQ(0U, clang_getNumDiagnostics(ClangTU));
 }

-TEST_F(LibclangReparseTest, ReparseWithModule) {
+TEST_F(LibclangReparseTest, DISABLED_ReparseWithModule) {
   const char *HeaderTop = "#ifndef H\n#define H\nstruct Foo { int bar;";
   const char *HeaderBottom = "\n};\n#endif\n";
   const char *MFile = "#include \"HeaderFile.h\"\nint main() {"
EOF
patch -p0
popd

# This Go bindings test doesn't work after the bootstrap build on Linux. (PR21552)
pushd "${LLVM_DIR}"
cat << 'EOF' |
Index: test/Bindings/Go/go.test
===================================================================
--- test/Bindings/Go/go.test    (revision 220284)
+++ test/Bindings/Go/go.test    (working copy)
@@ -1,8 +1,9 @@
-; RUN: cd %S/../../../bindings/go/llvm && \
-; RUN: env CGO_CPPFLAGS="$(llvm-config --cppflags)" \
-; RUN:     CGO_CXXFLAGS=-std=c++11 \
-; RUN:     CGO_LDFLAGS="$(llvm-config --ldflags --libs --system-libs \
-; RUN:                                $(../build.sh --print-components)) $CGO_LDFLAGS" \
-; RUN:     %go test -tags byollvm .
+; X: cd %S/../../../bindings/go/llvm && \
+; X: env CGO_CPPFLAGS="$(llvm-config --cppflags)" \
+; X:     CGO_CXXFLAGS=-std=c++11 \
+; X:     CGO_LDFLAGS="$(llvm-config --ldflags --libs --system-libs \
+; X:                                $(../build.sh --print-components)) $CGO_LDFLAGS" \
+; X:     %go test -tags byollvm .
+; RUN: true
 
 ; REQUIRES: shell
EOF
patch -p0
popd

# Apply 220340: Revert "Teach the load analysis to allow finding available values which require" (r220277)
pushd "${LLVM_DIR}"
cat << 'EOF' |
--- a/include/llvm/IR/IRBuilder.h
+++ b/include/llvm/IR/IRBuilder.h
@@ -1246,18 +1246,6 @@ public:
       return Insert(Folder.CreateIntCast(VC, DestTy, isSigned), Name);
     return Insert(CastInst::CreateIntegerCast(V, DestTy, isSigned), Name);
   }
-
-  Value *CreateBitOrPointerCast(Value *V, Type *DestTy,
-                                const Twine &Name = "") {
-    if (V->getType() == DestTy)
-      return V;
-    if (V->getType()->isPointerTy() && DestTy->isIntegerTy())
-      return CreatePtrToInt(V, DestTy, Name);
-    if (V->getType()->isIntegerTy() && DestTy->isPointerTy())
-      return CreateIntToPtr(V, DestTy, Name);
-
-    return CreateBitCast(V, DestTy, Name);
-  }
 private:
   // \brief Provided to resolve 'CreateIntCast(Ptr, Ptr, "...")', giving a
   // compile time error, instead of converting the string to bool for the
diff --git a/include/llvm/IR/InstrTypes.h b/include/llvm/IR/InstrTypes.h
index 1186857..7e98fe1 100644
--- a/include/llvm/IR/InstrTypes.h
+++ b/include/llvm/IR/InstrTypes.h
@@ -490,19 +490,6 @@ public:
     Instruction *InsertBefore = 0 ///< Place to insert the instruction
   );
 
-  /// @brief Create a BitCast, a PtrToInt, or an IntToPTr cast instruction.
-  ///
-  /// If the value is a pointer type and the destination an integer type,
-  /// creates a PtrToInt cast. If the value is an integer type and the
-  /// destination a pointer type, creates an IntToPtr cast. Otherwise, creates
-  /// a bitcast.
-  static CastInst *CreateBitOrPointerCast(
-    Value *S,                ///< The pointer value to be casted (operand 0)
-    Type *Ty,          ///< The type to which cast should be made
-    const Twine &Name = "", ///< Name for the instruction
-    Instruction *InsertBefore = 0 ///< Place to insert the instruction
-  );
-
   /// @brief Create a ZExt, BitCast, or Trunc for int -> int casts.
   static CastInst *CreateIntegerCast(
     Value *S,                ///< The pointer value to be casted (operand 0)
@@ -565,17 +552,6 @@ public:
     Type *DestTy ///< The Type to which the value should be cast.
   );
 
-  /// @brief Check whether a bitcast, inttoptr, or ptrtoint cast between these
-  /// types is valid and a no-op.
-  ///
-  /// This ensures that any pointer<->integer cast has enough bits in the
-  /// integer and any other cast is a bitcast.
-  static bool isBitOrNoopPointerCastable(
-    Type *SrcTy, ///< The Type from which the value should be cast.
-    Type *DestTy, ///< The Type to which the value should be cast.
-    const DataLayout *Layout = 0 ///< Optional DataLayout.
-  );
-
   /// Returns the opcode necessary to cast Val into Ty using usual casting
   /// rules.
   /// @brief Infer the opcode for cast operand and type
diff --git a/lib/Analysis/Loads.cpp b/lib/Analysis/Loads.cpp
index 5042eb9..bb0d60e 100644
--- a/lib/Analysis/Loads.cpp
+++ b/lib/Analysis/Loads.cpp
@@ -176,13 +176,8 @@ Value *llvm::FindAvailableLoadedValue(Value *Ptr, BasicBlock *ScanBB,
 
   Type *AccessTy = cast<PointerType>(Ptr->getType())->getElementType();
 
-  // Try to get the DataLayout for this module. This may be null, in which case
-  // the optimizations will be limited.
-  const DataLayout *DL = ScanBB->getDataLayout();
-
-  // Try to get the store size for the type.
-  uint64_t AccessSize = DL ? DL->getTypeStoreSize(AccessTy)
-                           : AA ? AA->getTypeStoreSize(AccessTy) : 0;
+  // If we're using alias analysis to disambiguate get the size of *Ptr.
+  uint64_t AccessSize = AA ? AA->getTypeStoreSize(AccessTy) : 0;
 
   Value *StrippedPtr = Ptr->stripPointerCasts();
 
@@ -207,7 +202,7 @@ Value *llvm::FindAvailableLoadedValue(Value *Ptr, BasicBlock *ScanBB,
     if (LoadInst *LI = dyn_cast<LoadInst>(Inst))
       if (AreEquivalentAddressValues(
               LI->getPointerOperand()->stripPointerCasts(), StrippedPtr) &&
-          CastInst::isBitOrNoopPointerCastable(LI->getType(), AccessTy, DL)) {
+          CastInst::isBitCastable(LI->getType(), AccessTy)) {
         if (AATags)
           LI->getAAMetadata(*AATags);
         return LI;
@@ -219,8 +214,7 @@ Value *llvm::FindAvailableLoadedValue(Value *Ptr, BasicBlock *ScanBB,
       // (This is true even if the store is volatile or atomic, although
       // those cases are unlikely.)
       if (AreEquivalentAddressValues(StorePtr, StrippedPtr) &&
-          CastInst::isBitOrNoopPointerCastable(SI->getValueOperand()->getType(),
-                                               AccessTy, DL)) {
+          CastInst::isBitCastable(SI->getValueOperand()->getType(), AccessTy)) {
         if (AATags)
           SI->getAAMetadata(*AATags);
         return SI->getOperand(0);
diff --git a/lib/IR/Instructions.cpp b/lib/IR/Instructions.cpp
index 9da0eb4..1497aa8 100644
--- a/lib/IR/Instructions.cpp
+++ b/lib/IR/Instructions.cpp
@@ -2559,17 +2559,6 @@ CastInst *CastInst::CreatePointerBitCastOrAddrSpaceCast(
   return Create(Instruction::BitCast, S, Ty, Name, InsertBefore);
 }
 
-CastInst *CastInst::CreateBitOrPointerCast(Value *S, Type *Ty,
-                                           const Twine &Name,
-                                           Instruction *InsertBefore) {
-  if (S->getType()->isPointerTy() && Ty->isIntegerTy())
-    return Create(Instruction::PtrToInt, S, Ty, Name, InsertBefore);
-  if (S->getType()->isIntegerTy() && Ty->isPointerTy())
-    return Create(Instruction::IntToPtr, S, Ty, Name, InsertBefore);
-
-  return Create(Instruction::BitCast, S, Ty, Name, InsertBefore);
-}
-
 CastInst *CastInst::CreateIntegerCast(Value *C, Type *Ty,
                                       bool isSigned, const Twine &Name,
                                       Instruction *InsertBefore) {
@@ -2727,18 +2716,6 @@ bool CastInst::isBitCastable(Type *SrcTy, Type *DestTy) {
   return true;
 }
 
-bool CastInst::isBitOrNoopPointerCastable(Type *SrcTy, Type *DestTy,
-                                          const DataLayout *DL) {
-  if (auto *PtrTy = dyn_cast<PointerType>(SrcTy))
-    if (auto *IntTy = dyn_cast<IntegerType>(DestTy))
-      return DL && IntTy->getBitWidth() >= DL->getPointerTypeSizeInBits(PtrTy);
-  if (auto *PtrTy = dyn_cast<PointerType>(DestTy))
-    if (auto *IntTy = dyn_cast<IntegerType>(SrcTy))
-      return DL && IntTy->getBitWidth() >= DL->getPointerTypeSizeInBits(PtrTy);
-
-  return isBitCastable(SrcTy, DestTy);
-}
-
 // Provide a way to get a "cast" where the cast opcode is inferred from the
 // types and size of the operand. This, basically, is a parallel of the
 // logic in the castIsValid function below.  This axiom should hold:
diff --git a/lib/Transforms/InstCombine/InstCombineLoadStoreAlloca.cpp b/lib/Transforms/InstCombine/InstCombineLoadStoreAlloca.cpp
index c0df914..f3ac44c 100644
--- a/lib/Transforms/InstCombine/InstCombineLoadStoreAlloca.cpp
+++ b/lib/Transforms/InstCombine/InstCombineLoadStoreAlloca.cpp
@@ -418,8 +418,7 @@ Instruction *InstCombiner::visitLoadInst(LoadInst &LI) {
   BasicBlock::iterator BBI = &LI;
   if (Value *AvailableVal = FindAvailableLoadedValue(Op, LI.getParent(), BBI,6))
     return ReplaceInstUsesWith(
-        LI, Builder->CreateBitOrPointerCast(AvailableVal, LI.getType(),
-                                            LI.getName() + ".cast"));
+        LI, Builder->CreateBitCast(AvailableVal, LI.getType()));
 
   // load(gep null, ...) -> unreachable
   if (GetElementPtrInst *GEPI = dyn_cast<GetElementPtrInst>(Op)) {
diff --git a/lib/Transforms/Scalar/JumpThreading.cpp b/lib/Transforms/Scalar/JumpThreading.cpp
index c37a4c9..25a8b0c 100644
--- a/lib/Transforms/Scalar/JumpThreading.cpp
+++ b/lib/Transforms/Scalar/JumpThreading.cpp
@@ -902,8 +902,8 @@ bool JumpThreading::SimplifyPartiallyRedundantLoad(LoadInst *LI) {
     // only happen in dead loops.
     if (AvailableVal == LI) AvailableVal = UndefValue::get(LI->getType());
     if (AvailableVal->getType() != LI->getType())
-      AvailableVal =
-          CastInst::CreateBitOrPointerCast(AvailableVal, LI->getType(), "", LI);
+      AvailableVal = CastInst::Create(CastInst::BitCast, AvailableVal,
+                                      LI->getType(), "", LI);
     LI->replaceAllUsesWith(AvailableVal);
     LI->eraseFromParent();
     return true;
@@ -1040,8 +1040,8 @@ bool JumpThreading::SimplifyPartiallyRedundantLoad(LoadInst *LI) {
     // predecessor use the same bitcast.
     Value *&PredV = I->second;
     if (PredV->getType() != LI->getType())
-      PredV = CastInst::CreateBitOrPointerCast(PredV, LI->getType(), "",
-                                               P->getTerminator());
+      PredV = CastInst::Create(CastInst::BitCast, PredV, LI->getType(), "",
+                               P->getTerminator());
 
     PN->addIncoming(PredV, I->first);
   }
diff --git a/test/Transforms/InstCombine/select.ll b/test/Transforms/InstCombine/select.ll
index 9c8286b..6cf9f0f 100644
--- a/test/Transforms/InstCombine/select.ll
+++ b/test/Transforms/InstCombine/select.ll
@@ -1256,7 +1256,7 @@ define i32 @test76(i1 %flag, i32* %x) {
   ret i32 %v
 }
 
-declare void @scribble_on_i32(i32*)
+declare void @scribble_on_memory(i32*)
 
 define i32 @test77(i1 %flag, i32* %x) {
 ; The load here must not be speculated around the select. One side of the
@@ -1264,13 +1264,13 @@ define i32 @test77(i1 %flag, i32* %x) {
 ; load does.
 ; CHECK-LABEL: @test77(
 ; CHECK: %[[A:.*]] = alloca i32, align 1
-; CHECK: call void @scribble_on_i32(i32* %[[A]])
+; CHECK: call void @scribble_on_memory(i32* %[[A]])
 ; CHECK: store i32 0, i32* %x
 ; CHECK: %[[P:.*]] = select i1 %flag, i32* %[[A]], i32* %x
 ; CHECK: load i32* %[[P]]
 
   %under_aligned = alloca i32, align 1
-  call void @scribble_on_i32(i32* %under_aligned)
+  call void @scribble_on_memory(i32* %under_aligned)
   store i32 0, i32* %x
   %p = select i1 %flag, i32* %under_aligned, i32* %x
   %v = load i32* %p
@@ -1327,8 +1327,8 @@ define i32 @test80(i1 %flag) {
 entry:
   %x = alloca i32
   %y = alloca i32
-  call void @scribble_on_i32(i32* %x)
-  call void @scribble_on_i32(i32* %y)
+  call void @scribble_on_memory(i32* %x)
+  call void @scribble_on_memory(i32* %y)
   %tmp = load i32* %x
   store i32 %tmp, i32* %y
   %p = select i1 %flag, i32* %x, i32* %y
@@ -1351,8 +1351,8 @@ entry:
   %y = alloca i32
   %x1 = bitcast float* %x to i32*
   %y1 = bitcast i32* %y to float*
-  call void @scribble_on_i32(i32* %x1)
-  call void @scribble_on_i32(i32* %y)
+  call void @scribble_on_memory(i32* %x1)
+  call void @scribble_on_memory(i32* %y)
   %tmp = load i32* %x1
   store i32 %tmp, i32* %y
   %p = select i1 %flag, float* %x, float* %y1
@@ -1377,63 +1377,11 @@ entry:
   %y = alloca i32
   %x1 = bitcast float* %x to i32*
   %y1 = bitcast i32* %y to float*
-  call void @scribble_on_i32(i32* %x1)
-  call void @scribble_on_i32(i32* %y)
+  call void @scribble_on_memory(i32* %x1)
+  call void @scribble_on_memory(i32* %y)
   %tmp = load float* %x
   store float %tmp, float* %y1
   %p = select i1 %flag, i32* %x1, i32* %y
   %v = load i32* %p
   ret i32 %v
 }
-
-declare void @scribble_on_i64(i64*)
-
-define i8* @test83(i1 %flag) {
-; Test that we can speculate the load around the select even though they use
-; differently typed pointers and requires inttoptr casts.
-; CHECK-LABEL: @test83(
-; CHECK:         %[[X:.*]] = alloca i8*
-; CHECK-NEXT:    %[[Y:.*]] = alloca i8*
-; CHECK:         %[[V:.*]] = load i64* %[[X]]
-; CHECK-NEXT:    %[[C1:.*]] = inttoptr i64 %[[V]] to i8*
-; CHECK-NEXT:    store i8* %[[C1]], i8** %[[Y]]
-; CHECK-NEXT:    %[[C2:.*]] = inttoptr i64 %[[V]] to i8*
-; CHECK-NEXT:    %[[S:.*]] = select i1 %flag, i8* %[[C2]], i8* %[[C1]]
-; CHECK-NEXT:    ret i8* %[[S]]
-entry:
-  %x = alloca i8*
-  %y = alloca i64
-  %x1 = bitcast i8** %x to i64*
-  %y1 = bitcast i64* %y to i8**
-  call void @scribble_on_i64(i64* %x1)
-  call void @scribble_on_i64(i64* %y)
-  %tmp = load i64* %x1
-  store i64 %tmp, i64* %y
-  %p = select i1 %flag, i8** %x, i8** %y1
-  %v = load i8** %p
-  ret i8* %v
-}
-
-define i64 @test84(i1 %flag) {
-; Test that we can speculate the load around the select even though they use
-; differently typed pointers and requires a ptrtoint cast.
-; CHECK-LABEL: @test84(
-; CHECK:         %[[X:.*]] = alloca i8*
-; CHECK-NEXT:    %[[Y:.*]] = alloca i8*
-; CHECK:         %[[V:.*]] = load i8** %[[X]]
-; CHECK-NEXT:    store i8* %[[V]], i8** %[[Y]]
-; CHECK-NEXT:    %[[C:.*]] = ptrtoint i8* %[[V]] to i64
-; CHECK-NEXT:    ret i64 %[[C]]
-entry:
-  %x = alloca i8*
-  %y = alloca i64
-  %x1 = bitcast i8** %x to i64*
-  %y1 = bitcast i64* %y to i8**
-  call void @scribble_on_i64(i64* %x1)
-  call void @scribble_on_i64(i64* %y)
-  %tmp = load i8** %x
-  store i8* %tmp, i8** %y1
-  %p = select i1 %flag, i64* %x1, i64* %y
-  %v = load i64* %p
-  ret i64 %v
-}
EOF
patch -p1
popd


# Apply 220403: SanitizerBlacklist: Use spelling location for blacklisting purposes.
pushd "${CLANG_DIR}"
cat << 'EOF' |
--- a/lib/Basic/SanitizerBlacklist.cpp
+++ b/lib/Basic/SanitizerBlacklist.cpp
@@ -40,6 +40,7 @@ bool SanitizerBlacklist::isBlacklistedFile(StringRef FileName,
 
 bool SanitizerBlacklist::isBlacklistedLocation(SourceLocation Loc,
                                                StringRef Category) const {
-  return !Loc.isInvalid() && isBlacklistedFile(SM.getFilename(Loc), Category);
+  return !Loc.isInvalid() &&
+         isBlacklistedFile(SM.getFilename(SM.getSpellingLoc(Loc)), Category);
 }

--- a/test/CodeGen/address-safety-attr.cpp
+++ b/test/CodeGen/address-safety-attr.cpp
@@ -64,6 +64,15 @@ int AddressSafetyOk(int *a) { return *a; }
 // ASAN:  BlacklistedFunction{{.*}}) [[WITH]]
 int BlacklistedFunction(int *a) { return *a; }
 
+#define GENERATE_FUNC(name) \
+    int name(int *a) { return *a; }
+
+// WITHOUT: GeneratedFunction{{.*}}) [[NOATTR]]
+// BLFILE:  GeneratedFunction{{.*}}) [[NOATTR]]
+// BLFUNC:  GeneratedFunction{{.*}}) [[WITH]]
+// ASAN:    GeneratedFunction{{.*}}) [[WITH]]
+GENERATE_FUNC(GeneratedFunction)
+
 // WITHOUT:  TemplateAddressSafetyOk{{.*}}) [[NOATTR]]
 // BLFILE:  TemplateAddressSafetyOk{{.*}}) [[NOATTR]]
 // BLFUNC:  TemplateAddressSafetyOk{{.*}}) [[WITH]]
EOF
patch -p1
popd

# Apply 220407: Fixup for r220403: Use getFileLoc() instead of getSpellingLoc() in SanitizerBlacklist.
pushd "${CLANG_DIR}"
cat << 'EOF' |
--- a/lib/Basic/SanitizerBlacklist.cpp
+++ b/lib/Basic/SanitizerBlacklist.cpp
@@ -41,6 +41,6 @@ bool SanitizerBlacklist::isBlacklistedFile(StringRef FileName,
 bool SanitizerBlacklist::isBlacklistedLocation(SourceLocation Loc,
                                                StringRef Category) const {
   return !Loc.isInvalid() &&
-         isBlacklistedFile(SM.getFilename(SM.getSpellingLoc(Loc)), Category);
+         isBlacklistedFile(SM.getFilename(SM.getFileLoc(Loc)), Category);
 }
 
diff --git a/test/CodeGen/address-safety-attr.cpp b/test/CodeGen/address-safety-attr.cpp
index 0d585c7..031d013 100644
--- a/test/CodeGen/address-safety-attr.cpp
+++ b/test/CodeGen/address-safety-attr.cpp
@@ -66,13 +66,19 @@ int BlacklistedFunction(int *a) { return *a; }
 
 #define GENERATE_FUNC(name) \
     int name(int *a) { return *a; }
-
 // WITHOUT: GeneratedFunction{{.*}}) [[NOATTR]]
 // BLFILE:  GeneratedFunction{{.*}}) [[NOATTR]]
 // BLFUNC:  GeneratedFunction{{.*}}) [[WITH]]
 // ASAN:    GeneratedFunction{{.*}}) [[WITH]]
 GENERATE_FUNC(GeneratedFunction)
 
+#define GENERATE_NAME(name) name##_generated
+// WITHOUT: Function_generated{{.*}}) [[NOATTR]]
+// BLFILE:  Function_generated{{.*}}) [[NOATTR]]
+// BLFUNC:  Function_generated{{.*}}) [[WITH]]
+// ASAN:    Function_generated{{.*}}) [[WITH]]
+int GENERATE_NAME(Function)(int *a) { return *a; }
+
 // WITHOUT:  TemplateAddressSafetyOk{{.*}}) [[NOATTR]]
 // BLFILE:  TemplateAddressSafetyOk{{.*}}) [[NOATTR]]
 // BLFUNC:  TemplateAddressSafetyOk{{.*}}) [[WITH]]
EOF
patch -p1
popd


# Echo all commands.
set -x

# Set default values for CC and CXX if they're not set in the environment.
CC=${CC:-cc}
CXX=${CXX:-c++}

if [[ -n "${gcc_toolchain}" ]]; then
  # Use the specified gcc installation for building.
  CC="$gcc_toolchain/bin/gcc"
  CXX="$gcc_toolchain/bin/g++"
  # Set LD_LIBRARY_PATH to make auxiliary targets (tablegen, bootstrap compiler,
  # etc.) find the .so.
  export LD_LIBRARY_PATH="$(dirname $(${CXX} -print-file-name=libstdc++.so.6))"
fi

CFLAGS=""
CXXFLAGS=""
LDFLAGS=""

# LLVM uses C++11 starting in llvm 3.5. On Linux, this means libstdc++4.7+ is
# needed, on OS X it requires libc++. clang only automatically links to libc++
# when targeting OS X 10.9+, so add stdlib=libc++ explicitly so clang can run on
# OS X versions as old as 10.7.
# TODO(thakis): Some bots are still on 10.6, so for now bundle libc++.dylib.
# Remove this once all bots are on 10.7+, then use --enable-libcpp=yes and
# change deployment_target to 10.7.
deployment_target=""

if [ "${OS}" = "Darwin" ]; then
  # When building on 10.9, /usr/include usually doesn't exist, and while
  # Xcode's clang automatically sets a sysroot, self-built clangs don't.
  CFLAGS="-isysroot $(xcrun --show-sdk-path)"
  CPPFLAGS="${CFLAGS}"
  CXXFLAGS="-stdlib=libc++ -nostdinc++ -I${ABS_LIBCXX_DIR}/include ${CFLAGS}"

  if [[ -n "${bootstrap}" ]]; then
    deployment_target=10.6
  fi
fi

# Build bootstrap clang if requested.
if [[ -n "${bootstrap}" ]]; then
  ABS_INSTALL_DIR="${PWD}/${LLVM_BOOTSTRAP_INSTALL_DIR}"
  echo "Building bootstrap compiler"
  mkdir -p "${LLVM_BOOTSTRAP_DIR}"
  pushd "${LLVM_BOOTSTRAP_DIR}"

  cmake -GNinja \
      -DCMAKE_BUILD_TYPE=Release \
      -DLLVM_ENABLE_ASSERTIONS=ON \
      -DLLVM_TARGETS_TO_BUILD=host \
      -DLLVM_ENABLE_THREADS=OFF \
      -DCMAKE_INSTALL_PREFIX="${ABS_INSTALL_DIR}" \
      -DCMAKE_C_COMPILER="${CC}" \
      -DCMAKE_CXX_COMPILER="${CXX}" \
      -DCMAKE_C_FLAGS="${CFLAGS}" \
      -DCMAKE_CXX_FLAGS="${CXXFLAGS}" \
      ../llvm

  ninja
  if [[ -n "${run_tests}" ]]; then
    ninja check-all
  fi

  ninja install
  if [[ -n "${gcc_toolchain}" ]]; then
    # Copy that gcc's stdlibc++.so.6 to the build dir, so the bootstrap
    # compiler can start.
    cp -v "$(${CXX} -print-file-name=libstdc++.so.6)" \
      "${ABS_INSTALL_DIR}/lib/"
  fi

  popd
  CC="${ABS_INSTALL_DIR}/bin/clang"
  CXX="${ABS_INSTALL_DIR}/bin/clang++"

  if [[ -n "${gcc_toolchain}" ]]; then
    # Tell the bootstrap compiler to use a specific gcc prefix to search
    # for standard library headers and shared object file.
    CFLAGS="--gcc-toolchain=${gcc_toolchain}"
    CXXFLAGS="--gcc-toolchain=${gcc_toolchain}"
  fi

  echo "Building final compiler"
fi

# Build clang (in a separate directory).
# The clang bots have this path hardcoded in built/scripts/slave/compile.py,
# so if you change it you also need to change these links.
mkdir -p "${LLVM_BUILD_DIR}"
pushd "${LLVM_BUILD_DIR}"

# Build libc++.dylib while some bots are still on OS X 10.6.
if [ "${OS}" = "Darwin" ]; then
  rm -rf libcxxbuild
  LIBCXXFLAGS="-O3 -std=c++11 -fstrict-aliasing"

  # libcxx and libcxxabi both have a file stdexcept.cpp, so put their .o files
  # into different subdirectories.
  mkdir -p libcxxbuild/libcxx
  pushd libcxxbuild/libcxx
  ${CXX:-c++} -c ${CXXFLAGS} ${LIBCXXFLAGS} "${ABS_LIBCXX_DIR}"/src/*.cpp
  popd

  mkdir -p libcxxbuild/libcxxabi
  pushd libcxxbuild/libcxxabi
  ${CXX:-c++} -c ${CXXFLAGS} ${LIBCXXFLAGS} "${ABS_LIBCXXABI_DIR}"/src/*.cpp -I"${ABS_LIBCXXABI_DIR}/include"
  popd

  pushd libcxxbuild
  ${CC:-cc} libcxx/*.o libcxxabi/*.o -o libc++.1.dylib -dynamiclib \
    -nodefaultlibs -current_version 1 -compatibility_version 1 \
    -lSystem -install_name @executable_path/libc++.dylib \
    -Wl,-unexported_symbols_list,${ABS_LIBCXX_DIR}/lib/libc++unexp.exp \
    -Wl,-force_symbols_not_weak_list,${ABS_LIBCXX_DIR}/lib/notweak.exp \
    -Wl,-force_symbols_weak_list,${ABS_LIBCXX_DIR}/lib/weak.exp
  ln -sf libc++.1.dylib libc++.dylib
  popd
  LDFLAGS+="-stdlib=libc++ -L${PWD}/libcxxbuild"
fi

# Hook the Chromium tools into the LLVM build. Several Chromium tools have
# dependencies on LLVM/Clang libraries. The LLVM build detects implicit tools
# in the tools subdirectory, so install a shim CMakeLists.txt that forwards to
# the real directory for the Chromium tools.
# Note that the shim directory name intentionally has no _ or _. The implicit
# tool detection logic munges them in a weird way.
mkdir -v ${CHROME_TOOLS_SHIM_DIR}
cat > ${CHROME_TOOLS_SHIM_DIR}/CMakeLists.txt << EOF
# Since tools/clang isn't actually a subdirectory, use the two argument version
# to specify where build artifacts go. CMake doesn't allow reusing the same
# binary dir for multiple source dirs, so the build artifacts have to go into a
# subdirectory...
add_subdirectory(\${CHROMIUM_TOOLS_SRC} \${CMAKE_CURRENT_BINARY_DIR}/a)
EOF
rm -fv CMakeCache.txt
MACOSX_DEPLOYMENT_TARGET=${deployment_target} cmake -GNinja \
    -DCMAKE_BUILD_TYPE=Release \
    -DLLVM_ENABLE_ASSERTIONS=ON \
    -DLLVM_ENABLE_THREADS=OFF \
    -DCMAKE_C_COMPILER="${CC}" \
    -DCMAKE_CXX_COMPILER="${CXX}" \
    -DCMAKE_C_FLAGS="${CFLAGS}" \
    -DCMAKE_CXX_FLAGS="${CXXFLAGS}" \
    -DCMAKE_EXE_LINKER_FLAGS="${LDFLAGS}" \
    -DCMAKE_SHARED_LINKER_FLAGS="${LDFLAGS}" \
    -DCMAKE_MODULE_LINKER_FLAGS="${LDFLAGS}" \
    -DCMAKE_INSTALL_PREFIX="${ABS_LLVM_BUILD_DIR}" \
    -DCHROMIUM_TOOLS_SRC="${ABS_CHROMIUM_TOOLS_DIR}" \
    -DCHROMIUM_TOOLS="${chrome_tools}" \
    "${ABS_LLVM_DIR}"
env

if [[ -n "${gcc_toolchain}" ]]; then
  # Copy in the right stdlibc++.so.6 so clang can start.
  mkdir -p lib
  cp -v "$(${CXX} ${CXXFLAGS} -print-file-name=libstdc++.so.6)" lib/
fi

ninja
# If any Chromium tools were built, install those now.
if [[ -n "${chrome_tools}" ]]; then
  ninja cr-install
fi

STRIP_FLAGS=
if [ "${OS}" = "Darwin" ]; then
  # See http://crbug.com/256342
  STRIP_FLAGS=-x

  cp libcxxbuild/libc++.1.dylib bin/
fi
strip ${STRIP_FLAGS} bin/clang
popd

# Build compiler-rt out-of-tree.
mkdir -p "${COMPILER_RT_BUILD_DIR}"
pushd "${COMPILER_RT_BUILD_DIR}"

rm -fv CMakeCache.txt
MACOSX_DEPLOYMENT_TARGET=${deployment_target} cmake -GNinja \
    -DCMAKE_BUILD_TYPE=Release \
    -DLLVM_ENABLE_ASSERTIONS=ON \
    -DLLVM_ENABLE_THREADS=OFF \
    -DCMAKE_C_COMPILER="${CC}" \
    -DCMAKE_CXX_COMPILER="${CXX}" \
    -DLLVM_CONFIG_PATH="${ABS_LLVM_BUILD_DIR}/bin/llvm-config" \
    "${ABS_COMPILER_RT_DIR}"

ninja

# Copy selected output to the main tree.
# Darwin doesn't support cp --parents, so pipe through tar instead.
CLANG_VERSION=$("${ABS_LLVM_BUILD_DIR}/bin/clang" --version | \
     sed -ne 's/clang version \([0-9]\.[0-9]\.[0-9]\).*/\1/p')
ABS_LLVM_CLANG_LIB_DIR="${ABS_LLVM_BUILD_DIR}/lib/clang/${CLANG_VERSION}"
tar -c *blacklist.txt | tar -C ${ABS_LLVM_CLANG_LIB_DIR} -xv
tar -c include/sanitizer | tar -C ${ABS_LLVM_CLANG_LIB_DIR} -xv
if [[ "${OS}" = "Darwin" ]]; then
  tar -c lib/darwin | tar -C ${ABS_LLVM_CLANG_LIB_DIR} -xv
else
  tar -c lib/linux | tar -C ${ABS_LLVM_CLANG_LIB_DIR} -xv
fi

popd

if [[ -n "${with_android}" ]]; then
  # Make a standalone Android toolchain.
  ${ANDROID_NDK_DIR}/build/tools/make-standalone-toolchain.sh \
      --platform=android-14 \
      --install-dir="${LLVM_BUILD_DIR}/android-toolchain" \
      --system=linux-x86_64 \
      --stl=stlport \
      --toolchain=arm-linux-androideabi-4.8 \

  # Android NDK r9d copies a broken unwind.h into the toolchain, see
  # http://crbug.com/357890
  rm -v "${LLVM_BUILD_DIR}"/android-toolchain/include/c++/*/unwind.h

  # Build ASan runtime for Android in a separate build tree.
  mkdir -p ${LLVM_BUILD_DIR}/android
  pushd ${LLVM_BUILD_DIR}/android
  rm -fv CMakeCache.txt
  MACOSX_DEPLOYMENT_TARGET=${deployment_target} cmake -GNinja \
      -DCMAKE_BUILD_TYPE=Release \
      -DLLVM_ENABLE_ASSERTIONS=ON \
      -DLLVM_ENABLE_THREADS=OFF \
      -DCMAKE_C_COMPILER=${PWD}/../bin/clang \
      -DCMAKE_CXX_COMPILER=${PWD}/../bin/clang++ \
      -DLLVM_CONFIG_PATH=${PWD}/../bin/llvm-config \
      -DCMAKE_C_FLAGS="--target=arm-linux-androideabi --sysroot=${PWD}/../android-toolchain/sysroot -B${PWD}/../android-toolchain" \
      -DCMAKE_CXX_FLAGS="--target=arm-linux-androideabi --sysroot=${PWD}/../android-toolchain/sysroot -B${PWD}/../android-toolchain" \
      -DANDROID=1 \
      "${ABS_COMPILER_RT_DIR}"
  ninja libclang_rt.asan-arm-android.so

  # And copy it into the main build tree.
  cp "$(find -name libclang_rt.asan-arm-android.so)" "${ABS_LLVM_CLANG_LIB_DIR}/lib/linux/"
  popd
fi

if [[ -n "$run_tests" ]]; then
  # Run Chrome tool tests.
  ninja -C "${LLVM_BUILD_DIR}" cr-check-all
  # Run the LLVM and Clang tests.
  ninja -C "${LLVM_BUILD_DIR}" check-all
fi

# After everything is done, log success for this revision.
echo "${CLANG_AND_PLUGINS_REVISION}" > "${STAMP_FILE}"
