#include <string>
#include "rsp_packetizer_test.h"

int main(int argc, char* argv[]){
  rsp::Packetizer_test test2;
  std::string error;
  if (!test2.Run(&error)) {
    printf("Error in rsp::Packetizer_test [%s]\n", error.c_str());
    return 2;
  }

  printf("Tests passed.");
	return 0;
}

