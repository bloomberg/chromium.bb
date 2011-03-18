#ifndef NACL_SDK_BUILD_TOOLS_DEBUG_SERVER_COMMON_DEBUG_COMMAND_LINE_H_
#define NACL_SDK_BUILD_TOOLS_DEBUG_SERVER_COMMON_DEBUG_COMMAND_LINE_H_

#include <string>

namespace debug {

// This class works with command lines.
// Switches can optionally have a value attached as in "--switch value".

class CommandLine {
 public:
  CommandLine(int argc, char* argv[]);  // Initializze from argv vector.
  ~CommandLine();

  // Returns the value assosiated with the given switch.
  std::string GetStringSwitch(const std::string& name, 
                              const std::string& default_value) const;
  int GetIntSwitch(const std::string& name, int default_value) const;
  bool HasSwitch(const std::string& name) const;

 protected:
  int argc_;
  char** argv_;
};
}  // namespace debug
#endif  // NACL_SDK_BUILD_TOOLS_DEBUG_SERVER_COMMON_DEBUG_COMMAND_LINE_H_