#pragma once
#include "my_json.h"
#include <iostream>
#include <sstream>

namespace json {
class StructFieldDefinition {
 public:
  enum Type {FT_INT, FT_ENUM, FT_FLAGS, FT_PTR, FT_STR, FT_USTR, FT_HANDLE};

  StructFieldDefinition(const std::string& name, Type type, size_t size, size_t offset, size_t unicode_offset = 0) 
    : name_(name), type_(type), size_(size), offset_(offset), unicode_offset_(unicode_offset) {}

  void ParseEnumValues(const std::string& key_value_pairs);
  std::string GetEnumName(long long id);

 public:
  std::string name_;
  Type type_;
  size_t size_;
  size_t offset_; 
  size_t unicode_offset_;
  std::map<long long, std::string> enum_values_;
};

class StructDefinition {
 public:
  StructDefinition(const std::string& name) : name_(name) {}
  ~StructDefinition();

  void AddFieldDefinition(StructFieldDefinition* def);
  size_t GetFieldNum() const;
  StructFieldDefinition* GetFieldDefinition(size_t pos);
  StructFieldDefinition* GetFieldDefinition(const std::string& name);

 public:
  std::string name_;
  std::deque<StructFieldDefinition*> fields_;
};

class StructDefinitions {
 public:
  StructDefinitions();
  ~StructDefinitions();
  
  void AddStructDefinition(StructDefinition* def);
  StructDefinition* FindStructDefinition(const std::string& name);

 public:
  std::map<std::string, StructDefinition*> structs_;
};

class StructTextCreator : public Visitor {
 public:
  StructTextCreator();
  ~StructTextCreator();
  
  void SetGenerateComments(bool on, StructDefinitions* struct_defs);
  void CreateText(const Value& value_in, std::string* text_out);

  virtual void Visit(Null& element);
  virtual void Visit(Number& element);
  virtual void Visit(Boolean& element);
  virtual void Visit(String& element);
  virtual void Visit(Blob& element);
  virtual void Visit(Array& element);
  virtual void Visit(Object& element);

 protected:
  void  OutputObject(int offset, Object& value_in, std::ostringstream* buff_out);
  void  OutputValue(int offset, Value& value_in, std::ostringstream* buff_out);
  void  OutputOffset(int offset, std::ostringstream* buff_out);
  std::string CreateCommentFor(Value* prop, StructFieldDefinition* fd_def);

  bool generate_comments_;
  StructDefinitions* struct_defs_;
  std::string comment_;
  int offset_;
  std::ostringstream* buff_out_;
};

Value* CreateValueFromStructField(const void* struct_ptr, const char* struct_name, StructFieldDefinition& field_def);
Object* CreateFromStruct(const void* struct_ptr, const char* struct_name, StructDefinitions& defs);
void AddIntConst(int id, const char* name, std::string* str);

#define START_STRUCT_DEF(T) do { json::StructDefinition* sd = new json::StructDefinition(#T); T tmp

#define OFFSET_OF(Name) ((char*)&tmp.Name - (char*)&tmp)

#define DEF_ENUM_FIELD(Name, Values) do {\
  json::StructFieldDefinition* def = new json::StructFieldDefinition(#Name, json::StructFieldDefinition::FT_ENUM, sizeof(tmp.Name), OFFSET_OF(Name));\
  def->ParseEnumValues(Values); sd->AddFieldDefinition(def); } while(false)

#define DEF_HANDLE_FIELD(Name) \
  sd->AddFieldDefinition(new json::StructFieldDefinition(#Name, json::StructFieldDefinition::FT_HANDLE, sizeof(tmp.Name), OFFSET_OF(Name)))

#define DEF_PTR_FIELD(Name) \
  sd->AddFieldDefinition(new json::StructFieldDefinition(#Name, json::StructFieldDefinition::FT_PTR, sizeof(tmp.Name), OFFSET_OF(Name)))

#define DEF_STR_FIELD(Name) \
  sd->AddFieldDefinition(new json::StructFieldDefinition(#Name, json::StructFieldDefinition::FT_STR, sizeof(tmp.Name), OFFSET_OF(Name)))

#define DEF_INT_FIELD(Name) \
  sd->AddFieldDefinition(new json::StructFieldDefinition(#Name, json::StructFieldDefinition::FT_INT, sizeof(tmp.Name), OFFSET_OF(Name)))

#define DEF_USTR_FIELD(Name, FUnicode) \
  sd->AddFieldDefinition(new json::StructFieldDefinition(#Name, json::StructFieldDefinition::FT_USTR, sizeof(tmp.Name), OFFSET_OF(Name), OFFSET_OF(FUnicode)))

#define STOP_STRUCT_DEF defs->AddStructDefinition(sd); } while(false)

#define ADD_INT_C(x, str) json::AddIntConst(x, #x, &str)

}  // namespace json