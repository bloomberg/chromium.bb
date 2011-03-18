#include "StdAfx.h"
#include "my_json.h"

namespace json {
Null::Null() {}

Value* Null::Clone() const {
  return new Null;
}

void Null::Accept(Visitor* vis) {
  vis->Visit(*this);
}

  bool signed_;
  long size_;

Number::Number()
  : value_(0), signed_(false), size_(0) {
}

Number::Number(long long n) 
  : value_(n), signed_(true), size_(sizeof(n)) {
}

Number::Number(const Number& n) 
  : value_(n.value_), signed_(n.signed_), size_(n.size_) {
}

Number::Number(const debug::Blob& blob) {
  
}

Value* Number::Clone() const {
  return new Number(*this);
}

void Number::Accept(Visitor* vis) {
  vis->Visit(*this);
}
  
void Number::SetInteger(const void* i_ptr, size_t int_size, bool sign) {
  size_ = int_size;
  signed_ = sign;
  long long x = 0;
  switch (int_size) {
    case 1: x = *(static_cast<const char*>(i_ptr)); break;
    case 2: x = *(static_cast<const short*>(i_ptr)); break;
    case 4: x = *(static_cast<const int*>(i_ptr)); break;
    case 8: x = *(static_cast<const long long*>(i_ptr)); break;
  }
  value_ = x;
//  long long xx = value();
//  printf("Number::SetInteger %I64d %I64d\n", x, xx);
}

std::string Number::AsHexString() const {
  if (0 == size_)
    return "null";

  char tmp[100];
  switch (size_) {
    case 1: 
    case 2:
    case 4: _snprintf_s(tmp, sizeof(tmp), sizeof(tmp) - 1, "%X", static_cast<int>(value_)); break;
    case 8: _snprintf_s(tmp, sizeof(tmp), sizeof(tmp) - 1, "%I64X", value_); break;
  }
  tmp[sizeof(tmp) - 1] = 0;
  return tmp;
}

int Number::AsInt() const {
  return static_cast<int>(value());
}

std::string Number::AsDecString() const {
  if (0 == size_)
    return "null";

  char tmp[100];
  switch (size_) {
    case 1: 
    case 2:
    case 4: {
      int x = static_cast<int>(value_);
      _snprintf_s(tmp, sizeof(tmp), sizeof(tmp) - 1, "%d", x); 
      break;
    }
    case 8: _snprintf_s(tmp, sizeof(tmp), sizeof(tmp) - 1, "%I64d", value_); break;
  }
  tmp[sizeof(tmp) - 1] = 0;
  return tmp;
}

void Number::SetPrintHint(const std::string& hint) {
  //TODO:implement ?
}

String::String() {}

String::String(const std::string& str)
  : value_(str) {
}

Value* String::Clone() const {
  return new String(value_);
}

void String::Accept(Visitor* vis) {
  vis->Visit(*this);
}

void String::SetStr(const std::string& str) {
  value_ = str;
}

const std::string& String::GetStr() const {
  return value_;
}

std::string& String::GetStr() {
  return value_;
}

Object::Object() {
}

Object::~Object() {
  Clear();
}

Value* Object::Clone() const {
  Object* copy = new Object;
  for (size_t i = 0; i < value_.size(); i++) {
    const std::pair<std::string, Value*>& prop = value_[i];
    Value* value_copy = prop.second->Clone();
    if (NULL != value_copy)
      copy->SetProperty(prop.first, value_copy);
  }
  return copy;
}

void Object::Accept(Visitor* vis) {
  vis->Visit(*this);
}

const Value* Object::GetProperty(const std::string& name) const {
  for (size_t i = 0; i < value_.size(); i++) {
    const std::pair<std::string, Value*>& prop = value_[i];
    if (prop.first == name)
      return prop.second;
  }
  return NULL;
}

Value* Object::GetProperty(const std::string& name) {
  for (size_t i = 0; i < value_.size(); i++) {
    std::pair<std::string, Value*>& prop = value_[i];
    if (prop.first == name)
      return prop.second;
  }
  return NULL;
}

int Object::GetIntProperty(const std::string& name) const {
  int result = 0;
  const Number* num_val = dynamic_value_cast<Number>(GetProperty(name));
  if (NULL != num_val)
    result = num_val->AsInt();
  return result;
}

void Object::SetProperty(const std::string& name, Value* value) {
  DeleteProperty(name);
  value_.push_back(std::pair<std::string, Value*>(name, value));
}

void Object::SetProperty(const std::string& name, const std::string& value) {
  SetProperty(name, new String(value));
}

void Object::Clear() {
  for (size_t i = 0; i < value_.size(); i++) {
    std::pair<std::string, Value*>& prop = value_[i];
    delete prop.second;
  }
  value_.clear();
}

void Object::GetPropertyNames(std::deque<std::string>* names) const {
  for (size_t i = 0; i < value_.size(); i++) {
    const std::pair<std::string, Value*>& prop = value_[i];
    names->push_back(prop.first);
  }
}

void Object::DeleteProperty(const std::string& name) {
  TypeOf_value_::iterator it = value_.begin();
  TypeOf_value_::iterator end = value_.end();
  while (it != end) {
    std::pair<std::string, Value*>& prop = *it;
    if (prop.first == name) {
      if (NULL != prop.second)
        delete prop.second;
      value_.erase(it);
      return;
    }
    ++it;
  }
}

Blob::Blob() {
}

Blob::Blob(const Blob& other)
    : value_(other.value_) {
}

Blob::Blob(debug::Blob& blob)
    : value_(blob) {
}

Blob::~Blob() {
}

Value* Blob::Clone() const {
  return new Blob(*this);
}

void Blob::Accept(Visitor* vis) {
  vis->Visit(*this);
}

size_t Blob::Size() const {
  return value_.Size();
}

void Blob::Append(const void* data, size_t data_sz) {
  debug::Blob other(data, data_sz);
  value_.Append(other);
}

void Blob::Clear() {
  value_.Clear();
}

Array::Array() {
}

Array::~Array() {
  Clear();
}

Value* Array::Clone() const {
  Array* copy = new Array;
  if (NULL != copy) {
    size_t num = Size();
    for (size_t i = 0; i < num; i++) {
      Value* el_copy = GetAt(i)->Clone();
      if (NULL != el_copy)
        copy->SetAt(i, el_copy);
    }
  }
  return copy;
}

void Array::Accept(Visitor* vis) {
  vis->Visit(*this);
}

size_t Array::Size() const {
  return value_.size();
}

const Value* Array::GetAt(size_t i) const {
  return value_[i];
}

Value* Array::GetAt(size_t i) {
  return value_[i];
}

void Array::SetAt(size_t i, Value* value) {
  while (Size() <= i)
    Append(new Null);
  if (NULL != value_[i])
    delete value_[i];
  value_[i] = value;
}

void Array::Append(Value* value) {
  value_.push_back(value);
}

void Array::Clear() {
  size_t num = Size();
  for (size_t i = 0; i < num; i++)
    delete GetAt(i);
  value_.clear();
}

}  // namespace json
