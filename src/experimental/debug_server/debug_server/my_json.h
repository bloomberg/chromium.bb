#pragma once
#include <deque>
#include <string>
#include <map>
#include "..\common\debug_blob.h"

namespace json {

class Visitor;

class Value {
public:
  virtual ~Value() {}
  virtual Value* Clone() const = 0;
  virtual void Accept(Visitor* vis) {}
};

class Null : public Value {
 public:
  Null();
  virtual Value* Clone() const;
  virtual void Accept(Visitor* vis);
};

// Number does not support floating point numbers.
class Number : public Value {
 public:
  Number();
  Number(long long n);
  Number(const Number& n);
  Number(const debug::Blob& blob);

  virtual Value* Clone() const;
  virtual void Accept(Visitor* vis);
  void SetInteger(const void* ptr, size_t int_size, bool sign);
  std::string AsHexString() const;
  std::string AsDecString() const;
  int AsInt() const;

  // "h" for hex representation, "d" for decimal, "b" for binary,
  // <className>::<memberName> for enums.
  // For example, hint == "h DEBUG_EVENT::dwDebugEventCode", JSON:
  // "dwDebugEventCode" : "6", //0x6:LOAD_DLL_DEBUG_EVENT ih
  void SetPrintHint(const std::string& hint); 
  void set_value(long long x) {value_ = x;}
  long long value() const {return value_;}

 protected:
  long long value_;
//  std::deque<char> value_;
  bool signed_;
  size_t size_;
//  std::string print_hint_;
};

class String : public Value {
 public:
  String();
  String(const std::string& str);

  virtual Value* Clone() const;
  virtual void Accept(Visitor* vis);

  void SetStr(const std::string& str);
  const std::string& GetStr() const;
  std::string& GetStr();
  std::string value() const {return value_;}

 protected:
  std::string value_;
};

class Boolean : public Value {
 public:
  Boolean();
  Boolean(bool value);

  virtual Value* Clone() const;
  virtual void Accept(Visitor* vis);
  bool value() const {return value_;}

 protected:
  bool value_;
};

// JSON:
// "memory" : "00A5A567C6C688", //m

class Blob : public Value {
 public:
  Blob();
  Blob(const Blob& other);
  Blob(debug::Blob& blob);
  ~Blob();

  virtual Value* Clone() const;
  virtual void Accept(Visitor* vis);

  size_t Size() const;
  void Append(const void* data, size_t data_sz);
  void Clear();
  debug::Blob value() const {return value_;}

 protected:
  debug::Blob value_;
};

class Array : public Value {
 public:
  Array();
  ~Array();

  virtual Value* Clone() const;
  virtual void Accept(Visitor* vis);

  size_t Size() const;
  const Value* GetAt(size_t i) const;
  Value* GetAt(size_t i);
  void SetAt(size_t i, Value* value);
  void Append(Value* value);
  void Clear();

 protected:
  std::deque<Value*> value_;
};

class Object : public Value {
 public:
  Object();
  ~Object();

  virtual Value* Clone() const;
  virtual void Accept(Visitor* vis);

  const Value* GetProperty(const std::string& name) const;
  Value* GetProperty(const std::string& name);
  int GetIntProperty(const std::string& name) const;
  void SetProperty(const std::string& name, Value* value);
  void SetProperty(const std::string& name, const std::string& value);
  void DeleteProperty(const std::string& name);
  void Clear();
  void GetPropertyNames(std::deque<std::string>* names) const;

 protected:
  typedef std::deque<std::pair<std::string, Value*>> TypeOf_value_;
  TypeOf_value_ value_;
};

class Visitor {
 public:
  virtual ~Visitor() {}
  virtual void Visit(Null& element) {}
  virtual void Visit(Number& element) {}
  virtual void Visit(Boolean& element) {}
  virtual void Visit(String& element) {}
  virtual void Visit(Blob& element) {}
  virtual void Visit(Array& element) {}
  virtual void Visit(Object& element) {}
};

template <class T>
T* dynamic_value_cast(Value* value);

// Implementation.

class CastValueVisitor : public Visitor {
 public:
  CastValueVisitor() : type_(0) {}
  virtual void Visit(Null& element) {type_ = 1;}
  virtual void Visit(Number& element) {type_ = 2;}
  virtual void Visit(Boolean& element) {type_ = 3;}
  virtual void Visit(String& element) {type_ = 4;}
  virtual void Visit(Blob& element) {type_ = 5;}
  virtual void Visit(Array& element) {type_ = 6;}
  virtual void Visit(Object& element) {type_ = 7;}
  int type_;
};

template <class T>
T* dynamic_value_cast(Value* value) {
  if (NULL == value)
    return NULL;
  CastValueVisitor vis1;
  value->Accept(&vis1);

  T tmp;
  CastValueVisitor vis2;
  tmp.Accept(&vis2);
  if (vis2.type_ != vis1.type_)
    return NULL;
  return (T*)value;
}

template <class T>
const T* dynamic_value_cast(const Value* value) {
  if (NULL == value)
    return NULL;
  CastValueVisitor vis1;
  const_cast<Value*>(value)->Accept((&vis1));

  T tmp;
  CastValueVisitor vis2;
  tmp.Accept(&vis2);
  if (vis2.type_ != vis1.type_)
    return NULL;
  return (T*)value;
}
}   // namespace json