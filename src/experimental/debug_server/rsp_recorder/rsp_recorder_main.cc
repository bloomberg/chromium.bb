#include <conio.h>
#include "rsp_recorder.h"
#include "..\common\debug_command_line.h"

// command-line switches:
// --host-port
// --target-host
// --target-port
// --log-name <session_log_file_name>

namespace {
const char* kDefTargetHost = "localhost";
const int kDefHostPort = 4016;
const int kDefTargetPort = 4014;
const char* kDefLogfileName = "rsp_log.bin";
}  // namespace

int main(int argc, char* argv[]) {
  debug::CommandLine cmd_line(argc, argv);
  int host_port = cmd_line.GetIntSwitch("host-port", kDefHostPort);
  int target_port = cmd_line.GetIntSwitch("target-port", kDefTargetPort);
  std::string target_host = cmd_line.GetStringSwitch("target-port", kDefTargetHost);
  std::string log_name = cmd_line.GetStringSwitch("log-name", kDefLogfileName);

  rsp_recorder::Recorder rec;
  rec.SetSessionLogFileName(log_name);

  rec.Start(host_port, target_host, target_port);
  while (rec.IsRunning()) {
    rec.DoWork();
    if (_kbhit()) {
      char c = getchar();
      if ('q' == c)
        break;
    }
  }
  rec.Stop();
	return 0;
}
