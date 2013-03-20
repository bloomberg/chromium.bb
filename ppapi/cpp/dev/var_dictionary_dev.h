// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_VAR_DICTIONARY_DEV_H_
#define PPAPI_CPP_DEV_VAR_DICTIONARY_DEV_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/cpp/dev/var_array_dev.h"
#include "ppapi/cpp/var.h"

/// @file
/// This file defines the API for interacting with dictionary vars.

namespace pp {

class VarDictionary_Dev : public Var {
 public:
  /// Constructs a new dictionary var.
  VarDictionary_Dev();

  /// Contructs a <code>VarDictionary_Dev</code> given a var for which
  /// is_dictionary() is true. This will refer to the same dictionary var, but
  /// allow you to access methods specific to dictionary.
  ///
  /// @param[in] var A dictionary var.
  explicit VarDictionary_Dev(const Var& var);

  /// Contructs a <code>VarDictionary_Dev</code> given a <code>PP_Var</code>
  /// of type PP_VARTYPE_DICTIONARY.
  ///
  /// @param[in] var A <code>PP_Var</code> of type PP_VARTYPE_DICTIONARY.
  explicit VarDictionary_Dev(const PP_Var& var);

  /// Copy constructor.
  VarDictionary_Dev(const VarDictionary_Dev& other);

  virtual ~VarDictionary_Dev();

  /// Assignment operator.
  VarDictionary_Dev& operator=(const VarDictionary_Dev& other);

  /// The <code>Var</code> assignment operator is overridden here so that we can
  /// check for assigning a non-dictionary var to a
  /// <code>VarDictionary_Dev</code>.
  ///
  /// @param[in] other The dictionary var to be assigned.
  ///
  /// @return The resulting <code>VarDictionary_Dev</code> (as a
  /// <code>Var</code>&).
  virtual Var& operator=(const Var& other);

  /// Gets the value associated with the specified key.
  ///
  /// @param[in] key A string var.
  ///
  /// @return The value that is associated with <code>key</code>. If
  /// <code>key</code> is not a string var, or it doesn't exist in the
  /// dictionary, an undefined var is returned.
  Var Get(const Var& key) const;

  /// Sets the value associated with the specified key.
  ///
  /// @param[in] key A string var. If this key hasn't existed in the dictionary,
  /// it is added and associated with <code>value</code>; otherwise, the
  /// previous value is replaced with <code>value</code>.
  /// @param[in] value The value to set.
  ///
  /// @return A <code>PP_Bool</code> indicating whether the operation succeeds.
  PP_Bool Set(const Var& key, const Var& value);

  /// Deletes the specified key and its associated value, if the key exists.
  ///
  /// @param[in] key A string var.
  void Delete(const Var& key);

  /// Checks whether a key exists.
  ///
  /// @param[in] key A string var.
  ///
  /// @return A <code>PP_Bool</code> indicating whether the key exists.
  PP_Bool HasKey(const Var& key) const;

  /// Gets all the keys in the dictionary.
  ///
  /// @return An array var which contains all the keys of the dictionary.
  /// The elements are string vars. Returns an empty array var if failed.
  VarArray_Dev GetKeys() const;
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_VAR_DICTIONARY_DEV_H_
