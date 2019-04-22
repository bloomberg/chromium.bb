// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_METADATA_METADATA_TYPES_H_
#define UI_VIEWS_METADATA_METADATA_TYPES_H_

#include <memory>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "ui/views/metadata/type_conversion.h"
#include "ui/views/views_export.h"

namespace views {
namespace metadata {

// Interface for classes that provide ClassMetaData (via macros in
// metadata_macros.h).  GetClassMetaData() is automatically overridden and
// implemented in the relevant macros, so a class must merely have
// MetaDataProvider somewhere in its ancestry.
class MetaDataProvider {
 public:
  virtual class ClassMetaData* GetClassMetaData() = 0;
};

class MemberMetaDataBase;

// Represents the 'meta data' that describes a class. Using the appropriate
// macros in ui/views/metadata/metadata_macros.h, a descendant of this class
// is declared within the scope of the containing class. See information about
// using the macros in the comment for the views::View class.
// When instantiated
class VIEWS_EXPORT ClassMetaData {
 public:
  ClassMetaData();
  virtual ~ClassMetaData();

  const std::string& type_name() const { return type_name_; }
  const std::vector<MemberMetaDataBase*>& members() const { return members_; }

  void AddMemberData(std::unique_ptr<MemberMetaDataBase> member_data);

  // Lookup the member data entry for a member of this class with a given name.
  // Returns the appropriate MemberMetaDataBase* if it exists, nullptr
  // otherwise.
  MemberMetaDataBase* FindMemberData(const std::string& member_name);

  ClassMetaData* parent_class_meta_data() const {
    return parent_class_meta_data_;
  }
  void SetParentClassMetaData(ClassMetaData* parent_meta_data) {
    parent_class_meta_data_ = parent_meta_data;
  }

  // Custom iterator to iterate through all member data entries associated with
  // a class (including members declared in parent classes).
  // Example:
  //    for(views::MemberMetaDataBase* member : class_meta_data) {
  //      OperateOn(member);
  //    }
  class VIEWS_EXPORT ClassMemberIterator
      : public std::iterator<std::forward_iterator_tag, MemberMetaDataBase*> {
   public:
    ClassMemberIterator(const ClassMemberIterator& other);
    ~ClassMemberIterator();

    ClassMemberIterator& operator++();
    ClassMemberIterator operator++(int);

    bool operator==(const ClassMemberIterator& rhs) const;
    bool operator!=(const ClassMemberIterator& rhs) const {
      return !(*this == rhs);
    }

    MemberMetaDataBase* operator*() {
      if (current_collection_ == nullptr ||
          current_vector_index_ >= current_collection_->members().size())
        return nullptr;

      return current_collection_->members()[current_vector_index_];
    }

   private:
    friend class ClassMetaData;
    explicit ClassMemberIterator(ClassMetaData* starting_container);
    void IncrementHelper();

    ClassMetaData* current_collection_;
    size_t current_vector_index_;
  };

  ClassMemberIterator begin();
  ClassMemberIterator end();

 protected:
  void SetTypeName(const std::string& type_name);

 private:
  std::string type_name_;
  std::vector<MemberMetaDataBase*> members_;
  ClassMetaData* parent_class_meta_data_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ClassMetaData);
};

// Abstract base class to represent meta data about class members.
// Provides basic information (such as the name of the member), and templated
// accessors to get/set the value of the member on an object.
class VIEWS_EXPORT MemberMetaDataBase {
 public:
  MemberMetaDataBase() {}
  virtual ~MemberMetaDataBase() = default;

  // Access the value of this member and return it as a string.
  // |obj| is the instance on which to obtain the value of the property this
  // metadata represents.
  virtual base::string16 GetValueAsString(void* obj) const = 0;

  // Set the value of this member through a string on a specified object.
  // |obj| is the instance on which to set the value of the property this
  // metadata represents.
  virtual void SetValueAsString(void* obj, const base::string16& new_value) = 0;

  void SetMemberName(const char* name) { member_name_ = name; }
  void SetMemberType(const char* type) { member_type_ = type; }
  const std::string& member_name() const { return member_name_; }
  const std::string& member_type() const { return member_type_; }

 private:
  std::string member_name_;
  std::string member_type_;

  DISALLOW_COPY_AND_ASSIGN(MemberMetaDataBase);
};  // class MemberMetaDataBase

// Represents meta data for a specific property member of class |TClass|, with
// underlying type |TValue|, where the type of the actual member.
// Allows for interaction with the property as if it were the underlying data
// type (|TValue|), but still uses the Property's functionality under the hood
// (so it will trigger things like property changed notifications).
template <typename TClass,
          typename TValue,
          void (TClass::*Set)(const TValue value),
          TValue (TClass::*Get)() const>
class ClassPropertyMetaData : public MemberMetaDataBase {
 public:
  ClassPropertyMetaData() {}
  virtual ~ClassPropertyMetaData() = default;

  base::string16 GetValueAsString(void* obj) const override {
    return Convert<TValue, base::string16>((static_cast<TClass*>(obj)->*Get)());
  }

  void SetValueAsString(void* obj, const base::string16& new_value) override {
    (static_cast<TClass*>(obj)->*Set)(
        Convert<base::string16, TValue>(new_value));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ClassPropertyMetaData);
};

}  // namespace metadata
}  // namespace views

#endif  // UI_VIEWS_METADATA_METADATA_TYPES_H_
