#include "rsp_player.h"
#include "..\common\debug_command_line.h"

// command-line switches:
// --target-host
// --target-port
// --log-name <session_log_file_name>
// --print-log  : prints the log and exits.
// --wait-key   : when specified, program does not exit after playing the log.

namespace {
const char* kDefTargetHost = "localhost";
const int kDefTargetPort = 4014;
const char* kDefLogfileName = "rsp_log.bin";
void PrintLog( const std::string& log_file_name);
}  // namespace

int main(int argc, char* argv[]) {
  debug::CommandLine cmd_line(argc, argv);
  int target_port = cmd_line.GetIntSwitch("target-port", kDefTargetPort);
  std::string target_host =
      cmd_line.GetStringSwitch("target-host", kDefTargetHost);
  std::string log_name = cmd_line.GetStringSwitch("log-name", kDefLogfileName);
  bool wait_for_final_keypress = cmd_line.HasSwitch("wait-key");

  if (cmd_line.HasSwitch("print-log")) {
    PrintLog(log_name);
    getchar();
    return 0;
  }

  rsp_player::Player player;
  if (!player.SetSessionLogFileName(log_name)) {
    printf("Error opening [%s] file.", log_name.c_str());
    getchar();
    return 2;
  }

  player.Start(target_host, target_port);
  while (player.IsRunning())
    player.DoWork();
 
  if (player.IsFailed()) {
    std::string error;
    player.GetErrorDescription(&error);
    printf("Test failed: [%s]\n", error.c_str());
  } else {
    printf("Test is finished.\n");
  }
  if (wait_for_final_keypress)
    getchar();
	return (player.IsFailed() ? 1 : 0);
}

namespace {
void PrintLog( const std::string& log_file_name) {
  rsp::SessionLog log;
  log.OpenToRead(log_file_name);

  char record_type = 0;
  debug::Blob record;
  while (log.GetNextRecord(&record_type, &record)) {
    std::string str = record.ToString();
    printf("%c>[%s]\n", record_type, str.c_str());
  }
}
}  // namespace