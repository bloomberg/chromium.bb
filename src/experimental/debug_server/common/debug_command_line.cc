#include "debug_command_line.h"

namespace {
std::string AddDashedPrefix(const std::string& name) {
  std::string result = name;
  while (0 != strncmp(result.c_str(), "--", 2)) {
    result = std::string("-") + result;
  }
  return result;
}
}  // namespace

namespace debug {
CommandLine::CommandLine(int argc, char* argv[])
    : argc_(argc),
      argv_(argv) {
}

CommandLine::~CommandLine() {}

std::string CommandLine::GetStringSwitch(
    const std::string& name, const std::string& default_value) const {
  std::string value = default_value;
  std::string name_with_two_dashes = AddDashedPrefix(name);
  for (int i = 1; (i + 1) < argc_; i++) {
    if (AddDashedPrefix(argv_[i]) == name_with_two_dashes) {
      value = argv_[i + 1];
      break;
    }
  }
  return value;
}

int CommandLine::GetIntSwitch(const std::string& name,
                              int default_value) const {
  int value = default_value;
  std::string str = GetStringSwitch(name, "");
  if (0 != str.size())
    value = atoi(str.c_str());
  return value;
}

bool CommandLine::HasSwitch(const std::string& name) const {
  std::string name_with_two_dashes = AddDashedPrefix(name);
  for (int i = 1; i < argc_; i++) {
    if (AddDashedPrefix(argv_[i]) == name_with_two_dashes)
      return true;
  }
  return false;
}
}  // namespace debug

