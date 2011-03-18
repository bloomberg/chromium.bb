#include "StdAfx.h"
#include "my_struct_to_json.h"
#include "..\common\debug_blob.h"

namespace {
const int kTabSpaces = 2;
}  // namespace

namespace json {
StructTextCreator::StructTextCreator()
    : generate_comments_(true),
      struct_defs_(0) {
}

StructTextCreator::~StructTextCreator() {
}
  
void StructTextCreator::SetGenerateComments(bool on, StructDefinitions* struct_defs) {
  generate_comments_ = on;
  struct_defs_ = struct_defs;
}

void StructTextCreator::CreateText(const Value& value_in, std::string* text_out) {
  Value& value = const_cast<Value&>(value_in);
  std::ostringstream buffer;
  offset_ = 0;
  buff_out_ = &buffer;
  value.Accept(this);
  *text_out = buffer.str();
}

void  StructTextCreator::OutputValue(int offset, Value& value_in, std::ostringstream* buff_out) {
  offset_ = offset;
  buff_out_ = buff_out;
  value_in.Accept(this);
}

void  StructTextCreator::OutputOffset(int offset, std::ostringstream* buff_out) {
  for (int i = 0; i < offset; i++)
    *buff_out << " ";
}

void StructTextCreator::Visit(Null& element) {
  *buff_out_ << "null";
}

void StructTextCreator::Visit(Number& element) {
  *buff_out_ << element.AsDecString();
}

void StructTextCreator::Visit(Boolean& element) {
  *buff_out_ << (element.value() ? "true" : "false");
}

void StructTextCreator::Visit(String& element) {
  *buff_out_ << "\"" << element.value() << "\"";
}

void StructTextCreator::Visit(Blob& element) {
  std::string str = element.value().ToHexString();
//  if ((0 != str.size()) && ('0' == str[0]))
//    str.

  *buff_out_ << "\"" << str << "\"";
}

void StructTextCreator::Visit(Array& element) {
//TODO:implement
}

void StructTextCreator::Visit(Object& element) {
  OutputOffset(offset_, buff_out_);
  *buff_out_ << "{" << std::endl;
  offset_ += kTabSpaces;

  std::deque<std::string> property_names;
  element.GetPropertyNames(&property_names);

  std::string struct_type;
  String* struct_type_obj = static_cast<String*>(element.GetProperty("_type"));
  if (NULL != struct_type_obj)
    struct_type = struct_type_obj->value();

  size_t num = property_names.size();
  for (size_t i = 0; i < num; i++) {
    std::string& prop_name = property_names[i];
    Value* prop = element.GetProperty(prop_name);
    OutputOffset(offset_, buff_out_);
    *buff_out_ << "\"" << prop_name << "\": ";
    OutputValue(offset_, *prop, buff_out_);
    if ((i + 1) != num)
    *buff_out_ << ",";

    //aaa
    if (NULL != struct_defs_) {
      //struct_type + prop_name
      StructDefinition* st_def = struct_defs_->FindStructDefinition(struct_type);
      if (NULL != st_def) {
        StructFieldDefinition* fd_def = st_def->GetFieldDefinition(prop_name);
        if (NULL != fd_def) {
          comment_ = CreateCommentFor(prop, fd_def);
        }
      }
    }
    if (comment_.size()) {
      *buff_out_ << " // " << comment_;
      comment_.clear();
    }
    *buff_out_ << std::endl;
  }

  offset_ -= kTabSpaces;
  OutputOffset(offset_, buff_out_);
  *buff_out_ << "}" << std::endl;
}

std::string StructTextCreator::CreateCommentFor(Value* prop, StructFieldDefinition* fd_def) {
  std::string comment;
  if ((fd_def->type_ == StructFieldDefinition::FT_INT) ||
     (fd_def->type_ == StructFieldDefinition::FT_ENUM)) {
    Number* i_prop = static_cast<Number*>(prop);
    comment = i_prop->AsHexString();
    if (comment != "null")
      comment = std::string("0x") + comment;
  }
  if (fd_def->type_ == StructFieldDefinition::FT_HANDLE) {
    Number* i_prop = static_cast<Number*>(prop);
    comment = i_prop->AsHexString();
  }
  if (fd_def->type_ == StructFieldDefinition::FT_ENUM) {
    Number* i_prop = static_cast<Number*>(prop);
    std::string enum_name = fd_def->GetEnumName(i_prop->value());
    comment.append(" ");
    comment.append(enum_name);
  }

  //TODO: implement
  return comment;
}

void Split(const std::string& str, std::deque<std::string>* tokens, const char* delimiters) {
  std::string token;
  std::string::const_iterator it = str.begin();
  while (str.end() != it) {
    char c = *it++;
    if (strchr(delimiters, c) != 0) {
      tokens->push_back(token);
      token.clear();
    } else {
      token.push_back(c);
    }
  }
  if (0 != token.size())
    tokens->push_back(token);
}

// example: "3:CREATE_PROCESS_DEBUG_EVENT,2:CREATE_THREAD_DEBUG_EVENT"
void StructFieldDefinition::ParseEnumValues(const std::string& key_value_pairs) {
  enum_values_.clear();
  std::deque<std::string> statements;
  Split(key_value_pairs, &statements, ",");
  for (size_t i = 0; i < statements.size(); i++) {
    std::string statement = statements[i];
    std::deque<std::string> parts;
    Split(statement, &parts, ":");
    if (parts.size() >= 2) {
      int id = atoi(parts[0].c_str());
      enum_values_[id] = parts[1];
    }
  }
}

std::string StructFieldDefinition::GetEnumName(long long id) {
  if (enum_values_.find(id) != enum_values_.end())
    return enum_values_[id];
  return "";
}

StructDefinition::~StructDefinition() {
  for (size_t i = 0; i < fields_.size(); i++)
    delete fields_[i];
  fields_.clear();
}

void StructDefinition::AddFieldDefinition(StructFieldDefinition* def) {
  fields_.push_back(def);
}

size_t StructDefinition::GetFieldNum() const {
  return fields_.size();
}

StructFieldDefinition* StructDefinition::GetFieldDefinition(size_t pos) {
  return fields_[pos];
}

StructFieldDefinition* StructDefinition::GetFieldDefinition(const std::string& name) {
  size_t num = GetFieldNum();
  for (size_t i = 0; i < num; i++) {
    StructFieldDefinition* def = GetFieldDefinition(i);
    if (def->name_ == name)
      return def;
  }
  return NULL;
}

StructDefinitions::StructDefinitions() {
}

StructDefinitions::~StructDefinitions() {
  std::map<std::string, StructDefinition*>::iterator it = structs_.begin();
  while (it != structs_.end()) {
    delete it->second;
    ++it;
  }
}

void StructDefinitions::AddStructDefinition(StructDefinition* def) {
  structs_[def->name_] = def;
}

StructDefinition* StructDefinitions::FindStructDefinition(const std::string& name) {
  if (structs_.find(name) != structs_.end())
    return structs_[name];
  return NULL;
}

size_t MySafeCopy(void* dest, size_t dest_sz, const void* src) {
  //TODO:implement SAFE copy, i.e. with system exception catching (like access violation)
  strcpy_s(static_cast<char*>(dest), dest_sz, static_cast<const char*>(src));
  return strlen(static_cast<const char*>(src));
}

char* UnicodeToAscii(const void* unicode_str) {
  size_t number_of_converted_chars = 0;
  char ascii_str[4 * 1024] = {0};
  const wchar_t* in = static_cast<const wchar_t*>(unicode_str);
  mbstate_t mbstate;
  memset(&mbstate, 0, sizeof(mbstate));

  wcsrtombs_s(&number_of_converted_chars, 
              ascii_str, 
              sizeof(ascii_str) - 1, 
              &in, 
              _TRUNCATE, 
              &mbstate);
  ascii_str[sizeof(ascii_str) - 1] = 0;
  return _strdup(ascii_str);
}

Value* CreateValueFromStructField(const void* struct_ptr, const char* struct_name, json::StructFieldDefinition& field_def) {
  const char* src = static_cast<const char*>(struct_ptr);

  switch (field_def.type_) {
    case StructFieldDefinition::FT_INT: 
    case StructFieldDefinition::FT_ENUM: 
    case StructFieldDefinition::FT_FLAGS:
    case StructFieldDefinition::FT_HANDLE: {
      Number* val = new Number;
      val->SetInteger(src + field_def.offset_, field_def.size_, false);
      return val;
    } 
    case json::StructFieldDefinition::FT_PTR: {
      debug::Blob blob(src + field_def.offset_, field_def.size_);
      blob.Reverse();  // Convert from little-endian to big-endian.
      std::string hex_str = blob.ToHexString();
      std::string ptr_str = std::string("0x") + hex_str;
      return new json::String(ptr_str);
    }
    case StructFieldDefinition::FT_STR: {
      const void* str_ptr = 0;
      memcpy(&str_ptr, src + field_def.offset_, field_def.size_); //TODO: check for invalid size
      if (NULL != str_ptr) {
        char copy[16 * 1024];
        size_t num = MySafeCopy(copy, sizeof(copy) - 1, str_ptr);
        copy[sizeof(copy) - 1] = 0;
        copy[num] = 0;
        return new String(copy);
      }
      break;
    }
    case StructFieldDefinition::FT_USTR: {
/*
      unsigned short fUnicode = 0;
      memcpy(&fUnicode, src + field_def.unicode_offset_, sizeof(fUnicode));
      const void* pp_str_ptr = 0;
      memcpy(&pp_str_ptr, src + field_def.offset_, field_def.size_); //TODO: check for invalid size
      // now, pp_str_ptr is an address of string pointer in debugee address space.
      void* str_ptr = 0;

      if (NULL != str_ptr) {
        if (0 != fUnicode) {
          char* str = UnicodeToAscii(str_ptr);
          Value* obj = new String(str);
          if (NULL != str)
            free(str);
          return obj;
        } else {
          char copy[16 * 1024];
          size_t num = MySafeCopy(copy, sizeof(copy) - 1, str_ptr);
          copy[sizeof(copy) - 1] = 0;
          copy[num] = 0;
          return new String(copy);
        }
      }
*/
    }
  }
  return new Null;
}

Object* CreateFromStruct(const void* struct_ptr, const char* struct_name, json::StructDefinitions& defs) {
  const char* src = static_cast<const char*>(struct_ptr);
  json::Object* obj = new json::Object;
  obj->SetProperty("_type", new json::String(struct_name));

  json::StructDefinition* struct_def = defs.FindStructDefinition(struct_name);
  if (NULL != struct_def) {
    size_t field_num = struct_def->GetFieldNum();
    for (size_t i = 0; i < field_num; i++) {
      json::StructFieldDefinition* field_def = struct_def->GetFieldDefinition(i);
      if (NULL != field_def) {
        json::Value* val = CreateValueFromStructField(src, struct_name, *field_def);
        obj->SetProperty(field_def->name_, val);
      }
    }
  }
  return obj;
}

void AddIntConst(int id, const char* name, std::string* str) {
  char tmp[300];
  _snprintf_s(tmp, sizeof(tmp), sizeof(tmp) - 1, "%d:%s,", id, name);
  tmp[sizeof(tmp) - 1] = 0;
  str->append(tmp);
}

}  // namespace json
