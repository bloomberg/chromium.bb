//
// Copyright 2019 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef ABSL_FLAGS_INTERNAL_COMMANDLINEFLAG_H_
#define ABSL_FLAGS_INTERNAL_COMMANDLINEFLAG_H_

#include <atomic>

#include "absl/base/macros.h"
#include "absl/flags/marshalling.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/optional.h"

namespace absl {
namespace flags_internal {

// Type-specific operations, eg., parsing, copying, etc. are provided
// by function specific to that type with a signature matching FlagOpFn.
enum FlagOp {
  kDelete,
  kClone,
  kCopy,
  kCopyConstruct,
  kSizeof,
  kParse,
  kUnparse
};
using FlagOpFn = void* (*)(FlagOp, const void*, void*);
using FlagMarshallingOpFn = void* (*)(FlagOp, const void*, void*, void*);

// Options that control SetCommandLineOptionWithMode.
enum FlagSettingMode {
  // update the flag's value unconditionally (can call this multiple times).
  SET_FLAGS_VALUE,
  // update the flag's value, but *only if* it has not yet been updated
  // with SET_FLAGS_VALUE, SET_FLAG_IF_DEFAULT, or "FLAGS_xxx = nondef".
  SET_FLAG_IF_DEFAULT,
  // set the flag's default value to this.  If the flag has not been updated
  // yet (via SET_FLAGS_VALUE, SET_FLAG_IF_DEFAULT, or "FLAGS_xxx = nondef")
  // change the flag's current value to the new default value as well.
  SET_FLAGS_DEFAULT
};

// Options that control SetFromString: Source of a value.
enum ValueSource {
  // Flag is being set by value specified on a command line.
  kCommandLine,
  // Flag is being set by value specified in the code.
  kProgrammaticChange,
};

// Signature for the help generation function used as an argument for the
// absl::Flag constructor.
using HelpGenFunc = std::string (*)();

// Signature for the function generating the initial flag value based (usually
// based on default value supplied in flag's definition)
using InitialValGenFunc = void* (*)();

extern const char kStrippedFlagHelp[];

// The per-type function
template <typename T>
void* FlagOps(FlagOp op, const void* v1, void* v2) {
  switch (op) {
    case kDelete:
      delete static_cast<const T*>(v1);
      return nullptr;
    case kClone:
      return new T(*static_cast<const T*>(v1));
    case kCopy:
      *static_cast<T*>(v2) = *static_cast<const T*>(v1);
      return nullptr;
    case kCopyConstruct:
      new (v2) T(*static_cast<const T*>(v1));
      return nullptr;
    case kSizeof:
      return reinterpret_cast<void*>(sizeof(T));
    default:
      return nullptr;
  }
}

template <typename T>
void* FlagMarshallingOps(FlagOp op, const void* v1, void* v2, void* v3) {
  switch (op) {
    case kParse: {
      // initialize the temporary instance of type T based on current value in
      // destination (which is going to be flag's default value).
      T temp(*static_cast<T*>(v2));
      if (!absl::ParseFlag<T>(*static_cast<const absl::string_view*>(v1), &temp,
                              static_cast<std::string*>(v3))) {
        return nullptr;
      }
      *static_cast<T*>(v2) = std::move(temp);
      return v2;
    }
    case kUnparse:
      *static_cast<std::string*>(v2) =
          absl::UnparseFlag<T>(*static_cast<const T*>(v1));
      return nullptr;
    default:
      return nullptr;
  }
}

// Functions that invoke flag-type-specific operations.
inline void Delete(FlagOpFn op, const void* obj) {
  op(flags_internal::kDelete, obj, nullptr);
}

inline void* Clone(FlagOpFn op, const void* obj) {
  return op(flags_internal::kClone, obj, nullptr);
}

inline void Copy(FlagOpFn op, const void* src, void* dst) {
  op(flags_internal::kCopy, src, dst);
}

inline void CopyConstruct(FlagOpFn op, const void* src, void* dst) {
  op(flags_internal::kCopyConstruct, src, dst);
}

inline bool Parse(FlagMarshallingOpFn op, absl::string_view text, void* dst,
                  std::string* error) {
  return op(flags_internal::kParse, &text, dst, error) != nullptr;
}

inline std::string Unparse(FlagMarshallingOpFn op, const void* val) {
  std::string result;
  op(flags_internal::kUnparse, val, &result, nullptr);
  return result;
}

inline size_t Sizeof(FlagOpFn op) {
  // This sequence of casts reverses the sequence from base::internal::FlagOps()
  return static_cast<size_t>(reinterpret_cast<intptr_t>(
      op(flags_internal::kSizeof, nullptr, nullptr)));
}

// The following struct contains the locks in a CommandLineFlag struct.
// They are in a separate struct that is lazily allocated to avoid problems
// with static initialization and to avoid multiple allocations.
struct CommandLineFlagLocks {
  absl::Mutex primary_mu;   // protects several fields in CommandLineFlag
  absl::Mutex callback_mu;  // used to serialize callbacks
};

// Holds either a pointer to help text or a function which produces it.  This is
// needed for supporting both static initialization of Flags while supporting
// the legacy registration framework.  We can't use absl::variant<const char*,
// const char*(*)()> since anybody passing 0 or nullptr in to a CommandLineFlag
// would find an ambiguity.
class HelpText {
 public:
  static constexpr HelpText FromFunctionPointer(const HelpGenFunc fn) {
    return HelpText(fn, nullptr);
  }
  static constexpr HelpText FromStaticCString(const char* msg) {
    return HelpText(nullptr, msg);
  }

  std::string GetHelpText() const;

  HelpText() = delete;
  HelpText(const HelpText&) = default;
  HelpText(HelpText&&) = default;

 private:
  explicit constexpr HelpText(const HelpGenFunc fn, const char* msg)
      : help_function_(fn), help_message_(msg) {}

  HelpGenFunc help_function_;
  const char* help_message_;
};

// Holds all information for a flag.
class CommandLineFlag {
 public:
  constexpr CommandLineFlag(
      const char* name, HelpText help_text, const char* filename,
      const flags_internal::FlagOpFn op,
      const flags_internal::FlagMarshallingOpFn marshalling_op,
      const flags_internal::InitialValGenFunc initial_value_gen, void* def,
      void* cur)
      : name_(name),
        help_(help_text),
        filename_(filename),
        op_(op),
        marshalling_op_(marshalling_op),
        make_init_value_(initial_value_gen),
        inited_(false),
        modified_(false),
        on_command_line_(false),
        def_(def),
        cur_(cur),
        counter_(0),
        locks_(nullptr) {}

  // Virtual destructor
  virtual void Destroy() const = 0;

  // Not copyable/assignable.
  CommandLineFlag(const CommandLineFlag&) = delete;
  CommandLineFlag& operator=(const CommandLineFlag&) = delete;

  // Access methods.

  // Returns true iff this object corresponds to retired flag
  virtual bool IsRetired() const { return false; }
  // Returns true iff this is a handle to an Abseil Flag.
  virtual bool IsAbseilFlag() const { return true; }

  absl::string_view Name() const { return name_; }
  std::string Help() const { return help_.GetHelpText(); }
  bool IsModified() const;
  void SetModified(bool is_modified);
  bool IsSpecifiedOnCommandLine() const;

  absl::string_view Typename() const;
  std::string Filename() const;
  std::string DefaultValue() const;
  std::string CurrentValue() const;

  // Interface to store the value in atomic if one used. This is short term
  // interface. To be reworked once cur_ is moved.
  virtual void StoreAtomic() {}

  // Interfaces to operate on validators.
  virtual bool HasValidatorFn() const { return false; }
  virtual bool InvokeValidator(const void* /*value*/) const { return true; }
  // Invoke the flag validators for old flags.
  // TODO(rogeeff): implement proper validators for Abseil Flags
  bool ValidateDefaultValue() const;
  bool ValidateInputValue(absl::string_view value) const;

  // Return true iff flag has type T.
  template <typename T>
  inline bool IsOfType() const {
    return op_ == &flags_internal::FlagOps<T>;
  }

  // Attempts to retrieve the flag value. Returns value on success,
  // absl::nullopt otherwise.
  template <typename T>
  absl::optional<T> Get() const {
    if (IsRetired() || flags_internal::FlagOps<T> != op_) return absl::nullopt;

    T res;
    Read(&res, flags_internal::FlagOps<T>);

    return res;
  }

  // Interfaces to overate on callbacks.
  virtual void InvokeCallback() {}

  // Sets the value of the flag based on specified std::string `value`. If the flag
  // was successfully set to new value, it returns true. Otherwise, sets `error`
  // to indicate the error, leaves the flag unchanged, and returns false. There
  // are three ways to set the flag's value:
  //  * Update the current flag value
  //  * Update the flag's default value
  //  * Update the current flag value if it was never set before
  // The mode is selected based on `set_mode` parameter.
  bool SetFromString(absl::string_view value,
                     flags_internal::FlagSettingMode set_mode,
                     flags_internal::ValueSource source, std::string* error);

  void CheckDefaultValueParsingRoundtrip() const;

  // Constant configuration for a particular flag.
 protected:
  ~CommandLineFlag() = default;

  const char* const name_;
  const HelpText help_;
  const char* const filename_;

  const FlagOpFn op_;                         // Type-specific handler
  const FlagMarshallingOpFn marshalling_op_;  // Marshalling ops handler
  const InitialValGenFunc make_init_value_;  // Makes initial value for the flag
  std::atomic<bool> inited_;  // fields have been lazily initialized

  // Mutable state (guarded by locks_->primary_mu).
  bool modified_;         // Has flag value been modified?
  bool on_command_line_;  // Specified on command line.
  void* def_;             // Lazily initialized pointer to default value
  void* cur_;             // Lazily initialized pointer to current value
  int64_t counter_;         // Mutation counter

  // Lazily initialized mutexes for this flag value.  We cannot inline a
  // SpinLock or Mutex here because those have non-constexpr constructors and
  // so would prevent constant initialization of this type.
  // TODO(rogeeff): fix it once Mutex has constexpr constructor
  struct CommandLineFlagLocks* locks_;  // locks, laziliy allocated.

  // Ensure that the lazily initialized fields of *flag have been initialized,
  // and return the lock which should be locked when flag's state is mutated.
  absl::Mutex* InitFlagIfNecessary() const ABSL_LOCK_RETURNED(locks_->primary_mu);

  // copy construct new value of flag's type in a memory referenced by dst
  // based on current flag's value
  void Read(void* dst, const flags_internal::FlagOpFn dst_op) const;
  // updates flag's value to *src (locked)
  void Write(const void* src, const flags_internal::FlagOpFn src_op);

  friend class FlagRegistry;
  friend class FlagPtrMap;
  friend class FlagSaverImpl;
  friend bool TryParseLocked(CommandLineFlag* flag, void* dst,
                             absl::string_view value, std::string* err);
  friend absl::Mutex* InitFlag(CommandLineFlag* flag);

  // This is a short term, until we completely rework persistent state
  // storage API.
  virtual void* GetValidator() const { return nullptr; }
  virtual bool SetValidator(void*) { return false; }
};

// Update any copy of the flag value that is stored in an atomic word.
// In addition if flag has a mutation callback this function invokes it. While
// callback is being invoked the primary flag's mutex is unlocked and it is
// re-locked back after call to callback is completed. Callback invocation is
// guarded by flag's secondary mutex instead which prevents concurrent callback
// invocation. Note that it is possible for other thread to grab the primary
// lock and update flag's value at any time during the callback invocation.
// This is by design. Callback can get a value of the flag if necessary, but it
// might be different from the value initiated the callback and it also can be
// different by the time the callback invocation is completed.
// Requires that *primary_lock be held in exclusive mode; it may be released
// and reacquired by the implementation.
void UpdateCopy(CommandLineFlag* flag);
// Return true iff flag value was changed via direct-access.
bool ChangedDirectly(CommandLineFlag* flag, const void* a, const void* b);

// This macro is the "source of truth" for the list of supported flag types we
// expect to perform lock free operations on. Specifically it generates code,
// a one argument macro operating on a type, supplied as a macro argument, for
// each type in the list.
#define ABSL_FLAGS_INTERNAL_FOR_EACH_LOCK_FREE(A) \
  A(bool)                                         \
  A(short)                                        \
  A(unsigned short)                               \
  A(int)                                          \
  A(unsigned int)                                 \
  A(long)                                         \
  A(unsigned long)                                \
  A(long long)                                    \
  A(unsigned long long)                           \
  A(double)                                       \
  A(float)

}  // namespace flags_internal
}  // namespace absl

#endif  // ABSL_FLAGS_INTERNAL_COMMANDLINEFLAG_H_
