#pragma once
#include <string>
#include <map>

namespace debug {
class RegisterDescription {
 public:
    RegisterDescription(int number,
                        const std::string& name,
                        size_t offset_in_context,
                        size_t size)
        : number_(number),
          name_(name),
          offset_in_context_(offset_in_context),
          size_(size) {}

    RegisterDescription()
        : number_(0),
          offset_in_context_(0),
          size_(0) {}

 int number_;
 std::string name_;
 size_t offset_in_context_;
 size_t size_;
};

void GetRegisterDescriptions(std::map<int, RegisterDescription>* descriptions);

}  // namespace debug