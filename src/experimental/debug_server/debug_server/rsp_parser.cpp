#include "StdAfx.h"
#include "rsp_parser.h"

#pragma warning(disable : 4996)

namespace {
debug::Blob BlobFromHexString(const debug::Blob& hex_str) {
  debug::Blob blob;
  size_t num = hex_str.Size();
  for (size_t i = num; i > 0;) {
    unsigned int c1 = 0;
    unsigned int c2 = 0;
    debug::Blob::HexCharToInt(hex_str[--i], &c1);
    if (i > 0) {
      debug::Blob::HexCharToInt(hex_str[--i], &c2);
      c1 += c2 << 4;
    }
    blob.PushFront(c1);
  }
  return blob;
}

int HexStringToInt(const debug::Blob& hex_str) {
  int result = 0;
  for (size_t i = 0; i < hex_str.Size(); i++) {
    unsigned int c = 0;
    debug::Blob::HexCharToInt(hex_str[i], &c);
    result = (result << 4) + c;
  }
  return result;
}

}  // namespace

namespace rsp {
Parser::Parser() {
}

bool Parser::FromRspRequestToJson(debug::Blob& msg, json::Object* out) {
  
  if (msg.HasPrefix("qSupported")) {
    out->SetProperty("command", "qSupported");
  } else if(msg.HasPrefix("qfDebuggeeThread")) {
    out->SetProperty("command", "qfDebuggeeThread");
  } else if(msg.HasPrefix("qsDebuggeeThread")) {
    out->SetProperty("command", "qsDebuggeeThread");
  } else if(msg.HasPrefix("qXfer:")) {
    out->SetProperty("command", "qXfer");
  } else if(msg.HasPrefix("qC")) {
    out->SetProperty("command", "GetCurrentThread");
  } else if(msg.HasPrefix("?")) {
    out->SetProperty("command", "GetHaltReason");
  } else if(msg.HasPrefix("H")) {
    out->SetProperty("command", "SetCurrentThread");
    msg.PopFront();
    msg.PopFront();
    if (msg.HasPrefix("0"))
      out->SetProperty("thread_id", "any");
    else if (msg.HasPrefix("-1"))
      out->SetProperty("thread_id", "all");
    else
//      out->SetProperty("thread_id", new json::Number(HexStringToInt(msg)));
      out->SetProperty("thread_id", new json::Blob(BlobFromHexString(msg)));

  } else if(msg.HasPrefix("g")) {
    out->SetProperty("command", "GetRegisters");
  } else if(msg.HasPrefix("s")) {
    out->SetProperty("command", "SingleStep");
  } else if(msg.HasPrefix("c")) {
    out->SetProperty("command", "Continue");
  } else if(msg.HasPrefix("m")) {
    out->SetProperty("command", "ReadMemory");
    msg.PopFront();
    std::deque<debug::Blob> tokens;
    msg.Split(",", &tokens);
    if (tokens.size() >= 2) {
      debug::Blob addr = BlobFromHexString(tokens[0]);
      int length = HexStringToInt(tokens[1]);
      out->SetProperty("address", new json::Blob(addr));
      out->SetProperty("length", new json::Number(length));
    }
  } else if(msg.HasPrefix("M")) {
    out->SetProperty("command", "WriteMemory");
    msg.PopFront();
    std::deque<debug::Blob> tokens;
    msg.Split(",:", &tokens);
    if (tokens.size() >= 3) {
      debug::Blob addr = BlobFromHexString(tokens[0]);
      debug::Blob data_blob = BlobFromHexString(tokens[2]);
      out->SetProperty("address", new json::Blob(addr));
      out->SetProperty("data", new json::Blob(data_blob));
    }
  } else if(msg.HasPrefix("T")) {
    out->SetProperty("command", "IsThreadAlive");
    msg.PopFront();
    out->SetProperty("thread_id", new json::Blob(BlobFromHexString(msg)));
// qThreadExtraInfo is not supported yet.
//  } else if(msg.HasPrefix("qThreadExtraInfo")) {
//    std::deque<debug::Blob> tokens;
//    msg.Split(",", &tokens);
//    if (tokens.size() >= 2) {
//      out->SetProperty("command", "qThreadExtraInfo");
//    out->SetProperty("thread_id", new json::Blob(BlobFromHexString(tokens[1])));
//    }
  }
  json::String* command = json::dynamic_value_cast<json::String>(out->GetProperty("command"));
  last_request_.clear();
  if (NULL != command)
    last_request_ = command->GetStr();

  return (NULL != command);
}

bool FromJsonToRspReply(json::Object& msg, debug::Blob* out) {
  std::string type;
  json::String* type_str = json::dynamic_value_cast<json::String>(msg.GetProperty("type"));
  if (NULL != type_str)
    type = type_str->GetStr();

  if ("Error" == type) {
    int error_code = msg.GetIntProperty("error_code");
    char tmp[200];
    sprintf(tmp, "E%02X", error_code);
    *out = debug::Blob(tmp);
  } else if ("Halted" == type) {
    int signal = msg.GetIntProperty("Signal");
    char tmp[200];
    sprintf(tmp, "S%02X", signal);
    *out = debug::Blob(tmp);
  } else if ("CurrentThread" == type) {
    int thread_id = msg.GetIntProperty("thread_id");
    char tmp[200];
    sprintf(tmp, "QC%X", thread_id);
    *out = debug::Blob(tmp);
  } else if ("Ok" == type) {
    *out = debug::Blob("OK");
  } else if ("AllRegisters" == type) {
    json::Array* reg_array = json::dynamic_value_cast<json::Array>(msg.GetProperty("registers"));
    if (NULL != reg_array) {
      size_t num = reg_array->Size();
      for (size_t i = 0; i < num; i++) {
        json::Blob* reg = json::dynamic_value_cast<json::Blob>(reg_array->GetAt(i));
        if (NULL != reg) {
          std::string hex_str = reg->value().ToHexString();
          out->Append(debug::Blob(hex_str.c_str()));
        }
      }
    }
  } else if ("Memory" == type) {
    json::Blob* data = json::dynamic_value_cast<json::Blob>(msg.GetProperty("registers"));
    if (NULL != data) {
      std::string hex_str = data->value().ToHexString();
      out->Append(debug::Blob(hex_str.c_str()));
    }
  } else if ("ThreadList" == type) {
    json::Array* reg_array = json::dynamic_value_cast<json::Array>(msg.GetProperty("threads"));
    if (NULL != reg_array) {
      out->Append("m");
      size_t num = reg_array->Size();
      for (size_t i = 0; i < num; i++) {
        json::Blob* reg = json::dynamic_value_cast<json::Blob>(reg_array->GetAt(i));
        if (NULL != reg) {
          std::string hex_str = reg->value().ToHexString();
          out->Append(debug::Blob(hex_str.c_str()));
        }
      }
    } else {
      out->Append("l");
    }
  }

  //TODO: implement
  return (0 != type.size());
}

}  // namespace rsp