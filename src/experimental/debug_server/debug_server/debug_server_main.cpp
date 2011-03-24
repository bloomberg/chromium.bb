#include "stdafx.h"
#include <conio.h>

#include "..\common\rsp_session_log.h"
#include "debug_core.h"
#include "debug_utils.h"
#include "debug_debug_event.h"
#include "rsp_parser.h"
#include "debug_server.h"
#include "debug_simple_inside_observer.h"

#pragma warning(disable : 4996)

void loc_PrintProcMemory(debug::DebuggeeProcess* proc);
bool FindNexeThread(debug::Core& dbg_core, int* pid, int* tid);
FILE* file = 0;

void LogEvent(const json::Value& event_obj) {
  json::StructTextCreator text_creator;
  text_creator.SetGenerateComments(true, debug::DEBUG_EVENT_struct_defs);
  
  std::string text;
  text_creator.CreateText(event_obj, &text);
  printf("%s\n", text.c_str());
  if (file) {
    fprintf(file, "%s\n", text.c_str());
    fflush(file);
  }
}

bool print_events = true;
class MyCoreEventObserver : public debug::CoreEventObserver {
 public:
  MyCoreEventObserver(debug::Core* dbg_core) : dbg_core_(dbg_core) {}

  void OnDebugEvent(DEBUG_EVENT de) {
  bool print_ev = print_events;
  if (!print_ev) {
    debug::DebuggeeProcess* proc = dbg_core_->GetProcess(de.dwProcessId);
    if (NULL != proc) {
      debug::DebuggeeThread* thread = proc->GetThread(de.dwThreadId);
      if (thread && thread->is_nexe_)
        print_ev = true;
    }
  }
  if (!print_ev)
    return;

  static int nexe_proc = 0;
  json::Value* obj = debug::DEBUG_EVENT_ToJSON(de);
  if (NULL != obj) {
    printf("----------------------------------------------------------\n");
//    if (nexe_proc && ( nexe_proc == de.dwProcessId))
    LogEvent(*obj);
    delete obj;

/*
    debug::DebuggeeProcess* proc = dbg_core_->GetProcess(de.dwProcessId);
    if (NULL != proc) {
      debug::DebuggeeThread* thread = proc->GetThread(de.dwThreadId);
      if (NULL != thread) {
        CONTEXT ct;
        thread->GetContext(&ct, NULL);
        json::Value* val = debug::CONTEXT_ToJSON(ct);
        if (NULL != val) {
          LogEvent(*val);
          delete val;
        }
      }
    }
*/
  }
}
  debug::Core* dbg_core_;
};

void* BlobToCBuff(const debug::Blob& blob) {
  SIZE_T num = blob.Size();
  char* buff = static_cast<char*>(malloc(num));
  if (NULL != buff) {
    for (size_t i = 0; i < num; i++)
      buff[i] = blob[i];
  }
  return buff;
}

namespace {
const char* GetDebugeeStateName(debug::DebuggeeProcess::State st) {
  switch (st) {
    case debug::DebuggeeProcess::RUNNING: return "RUNNING";
    case debug::DebuggeeProcess::PAUSED: return "PAUSED";
    case debug::DebuggeeProcess::DEAD: return "DEAD";
    case debug::DebuggeeProcess::HIT_BREAKPOINT: return "HIT_BREAKPOINT";
  }
  return "N/A";
}

void PrintLog( const std::string& log_file_name) {
  rsp::SessionLog log;
  log.OpenToRead(log_file_name);

  char record_type = 0;
  debug::Blob record;
  rsp::Parser prs;

  while (log.GetNextRecord(&record_type, &record)) {
    std::string str = record.ToString();
    printf("%c>[%s]\n", record_type, str.c_str());
    if ('H' == record_type) {
      json::Object obj;
      if (prs.FromRspRequestToJson(record, &obj)) {
        json::StructTextCreator tc;
        std::string json_str;
        tc.CreateText(obj, &json_str);
        //printf("%c>[%s]\n", record_type, str.c_str());
        printf("%json>[%s]\n", json_str.c_str());
      }
    }
  }
}
}  // namespace

int main(int argc, char* argv[]) {

//  long long ptr = 0x1122334455667788;
//  char tmp[sizeof(ptr)];
//  memcpy(tmp, &ptr, sizeof(ptr));
  
  file = fopen("log.txt", "wt");

  std::string error;
  debug::DEBUG_EVENT_ToJSON_Init();

  if (true) {
    debug::BlobUniTest but;
    std::string error_text;
    std::string s;
    bool dd = error_text == s;

    int res = but.Run(&error_text);
    if (0 != res) {
      printf("BlobUniTest::Run failed: %d [%s]\n", res, error_text.c_str());
      getchar();
      return res;
    }
  }

  debug::Core dbg_core;
  MyCoreEventObserver ev_observer(&dbg_core);
  dbg_core.SetEventObserver(&ev_observer);
  debug::StandardContinuePolicy continue_policy(dbg_core);
  dbg_core.SetContinuePolicy(&continue_policy);
  debug::SimpleInsideObserver ins_observer;
  dbg_core.SetInsideObserver(&ins_observer);

#ifdef _WIN64
  const char* cmd = "D:\\chromuim_648_12\\src\\build\\Debug\\chrome.exe"; // --no-sandbox";
  const char* work_dir = "D:\\chromuim_648_12\\src\\build\\Debug";
#else
  const char* cmd = "C:\\work\\chromuim_648_12\\src\\build\\Debug\\chrome.exe";
  const char* work_dir = "C:\\work\\chromuim_648_12\\src\\build\\Debug";
#endif

  bool start_res = dbg_core.StartProcess(cmd, work_dir, &error);
  if (!start_res) {
    printf("Can't start [%s] in [%s]. Error: [%s]\n", cmd, work_dir, error.c_str());
  } else {
    int current_process = 0;
    int current_thread = 0;
    do {
      dbg_core.DoWork(0);

      if (_kbhit()) {
        if (!current_process)
          FindNexeThread(dbg_core, &current_process, &current_thread);

        debug::DebuggeeProcess* process = dbg_core.GetProcess(current_process);
        debug::DebuggeeThread* thread = NULL;
        if (NULL != process)
          thread = process->GetThread(current_thread);
                
        char cmd[100] = {0};
        gets(cmd);
        if (0 == strcmp(cmd, "quit")) {
          dbg_core.Stop();
          break;
        } else if (0 == strncmp(cmd, "thread", strlen("thread"))) {
          int pid = 0;
          int tid = 0;
          int sc = sscanf(cmd + strlen("thread") + 1, "%d:%d", &pid, &tid);
          if (sc >= 2) {
            current_process = pid;
            current_thread = tid;
          } else if (sc >= 1) {
            current_process = pid;
          }
        } else if (0 == strncmp(cmd, "break", 5)) {
            if (NULL != process)
              process->Break();
        } else if (0 == strncmp(cmd, "br ", strlen("br "))) {
          void* addr = 0;
          int sn = sscanf(cmd + strlen("br "), "%p", &addr);
          if ((NULL != process) && (sn != 0))
            process->SetBreakpoint(addr);
        } else if (0 == strncmp(cmd, "rmbr ", strlen("rmbr "))) {
          void* addr = 0;
          int sn = sscanf(cmd + strlen("rmbr "), "%p", &addr);
          if ((NULL != process) && (sn != 0))
            process->RemoveBreakpoint(addr);
        } else if (0 == strcmp(cmd, "e-")) {
          print_events = false;
        } else if (0 == strcmp(cmd, "e+")) {
          print_events = true;
        } else if (0 == strcmp(cmd, "info threads")) {
          std::deque<int> processes;
          dbg_core.GetProcessesIds(&processes);
          for (size_t p = 0; p < processes.size(); p++) {
            int pid = processes[p];
            debug::DebuggeeProcess* proc = dbg_core.GetProcess(pid);
            if (proc == process)
              printf("process * %d", pid);
            else
              printf("process   %d", pid);
            printf(" %d-bits", debug::Utils::GetProcessorWordSizeInBits(proc->Handle()));

            if (NULL != proc) {
              debug::DebuggeeProcess::State st = proc->GetState();
              printf(" %s ", proc->IsHalted() ? "Halted" : GetDebugeeStateName(st));
              std::string name;
              std::string cmd;
              debug::Utils::GetProcessName(proc->id(), &name);
              debug::Utils::GetProcessCmdLine(proc->Handle(), &cmd);
              printf(" exe=[%s] cmd_line=[%s]\n", name.c_str(), cmd.c_str());
            } else {
              printf("\n");
            }
            if (NULL != proc) {
              std::deque<int> threads;
              proc->GetThreadIds(&threads);
              for (size_t i = 0; i < threads.size(); i++) {
                int id = threads[i];
                debug::DebuggeeThread* thr = proc->GetThread(id);
                bool current = (thr == thread);
                const char* status = thr->GetStateName(thr->GetState());
                const char* is_nexe = thr->is_nexe_ ? "[I'm nexe thread]" : "";
                printf("   %s%d "/*"start=0x%p "*/"ip=%p %s %s\n",
                    current ? "*" : " ",
                    id,
//                    thr->lpStartAddress_,
                    (void*)thr->GetIP(), status, is_nexe);
              }
            }
          }
        } else if (0 == strcmp(cmd, "c")) {
          if (NULL != process)
            process->Continue();
        } else if (0 == strncmp(cmd, "ct", 2)) {
          if (NULL != thread) {
            CONTEXT ct;
            thread->GetContext(&ct, NULL);
            json::Value* val = debug::CONTEXT_ToJSON(ct);
            if (NULL != val) {
              json::StructTextCreator text_creator;
              text_creator.SetGenerateComments(true, debug::DEBUG_EVENT_struct_defs);
              std::string text;
              text_creator.CreateText(*val, &text);
              printf("%s\n", text.c_str());
              delete val;
            }
          }          
        } else if (0 == strncmp(cmd, "m", 1)) {
          void* addr = 0;
          int sz = 0;
          sscanf(cmd + 2, "%p %d", &addr, &sz);
          char data[2000];
          if (NULL != process) {
            process->ReadMemory(addr, sz, data);
            debug::Blob blob(data, sz);
            std::string str = blob.ToHexString(false);
            printf("[%s]\n", str.c_str());
          }
        } else if (0 == strncmp(cmd, "M", 1)) {
          long long addr = 0;
          sscanf(cmd + 2, "%llx", &addr);
          char* p = strchr(cmd, ' ');
          p = strchr(p + 1, ' ');
          debug::Blob blob;
          blob.LoadFromHexString(std::string(p));
          if (NULL != process) {
            void* data = BlobToCBuff(blob);
            process->WriteMemory((const void*)addr, data, blob.Size());
            free(data);
          }
        } else if (0 == strcmp(cmd, "s")) {
          if (NULL != process)
            process->SingleStep();
        } else if (0 == strcmp(cmd, "ip")) {
          if (NULL != thread) {
            long long ip = thread->GetIP();
            printf("%p\n", (void*)ip);
          }
        } else if (0 == strcmp(cmd, "ip++")) {
          if (NULL != thread) {
            long long ip = thread->GetIP();
            ip++;
            thread->SetIP(ip);
            printf("%p\n", (void*)ip);
          }
        } else if (0 == strcmp(cmd, "quit")) {
          dbg_core.Stop();
          break;
        }
      }
    } while(true);
  }

  fclose(file);
  printf("Done. Ready to exit...");
  getchar();
	return 0;
}

unsigned char* my_strstr(unsigned char* str, size_t str_length, unsigned char* substr, size_t substr_length) {
  for (size_t i = 0; i < str_length; i++ ) {
    unsigned char* p = str + i;
    size_t str_length_now = str_length - i;
    if (str_length_now < substr_length)
      return NULL;

    if (memcmp(p, substr, substr_length) == 0)
      return p;
  }
  return 0;
}

void loc_PrintProcMemory(debug::DebuggeeProcess* proc) {
  char* addr = 0;
  char* max_addr = (char*)(2UL * 1024UL * 1024UL * 1024UL);
  FILE* file = fopen("mem_pages.txt", "wt");
  int count = 0;
  do {
    MEMORY_BASIC_INFORMATION32 mbi;
    memset(&mbi, 0, sizeof(mbi));
    SIZE_T sz = ::VirtualQueryEx(proc->Handle(), addr, (MEMORY_BASIC_INFORMATION*)&mbi, sizeof(mbi));
    if (sz != sizeof(mbi))
      break;
    addr += mbi.RegionSize;

    json::Value* obj = debug::MEMORY_BASIC_INFORMATION32_ToJSON(mbi);
    json::StructTextCreator text_creator;
    text_creator.SetGenerateComments(true, debug::DEBUG_EVENT_struct_defs);
    std::string text;
    text_creator.CreateText(*obj, &text);
    fprintf(file, "%s\n", text.c_str());

    if ((mbi.Protect == PAGE_EXECUTE) ||
       (mbi.Protect == PAGE_EXECUTE_READ) ||
       (mbi.Protect == PAGE_EXECUTE_READWRITE)) {
      if (mbi.RegionSize) {
        unsigned char* copy = (unsigned char*)malloc(mbi.RegionSize);
        memset(copy, 0, mbi.RegionSize);
        if (copy) {
          proc->ReadMemory((void*)mbi.AllocationBase, mbi.RegionSize, copy);
          unsigned char br_instr[] = {0xf7, 0x7d, 0xe4, 0x89 , 0x45 , 0xe8 , 0x8b , 0x45 , 0x08, 0x89, 0x04,0x24};
          unsigned char* brk_addr = my_strstr(copy, mbi.RegionSize, br_instr, sizeof(br_instr));
          if (brk_addr) {
            size_t offs = brk_addr - copy;
            byte* brk_proc_addr = (byte*)mbi.AllocationBase + offs;
            printf( "Ohoho #%d at %p membase=%p memsize=%d\n", count++, brk_proc_addr, mbi.AllocationBase, mbi.RegionSize);

            debug::Blob blob(brk_addr, sizeof(br_instr));
            std::string str = blob.ToHexString(false);
            printf( "[%s]\n", str.c_str());
          }
          free(copy);
        }
      }
    }
  } while (addr < max_addr);

  fclose(file);
  printf("done.\n");
}

bool FindNexeThread(debug::Core& dbg_core, int* pid, int* tid) {
  std::deque<int> processes;
  dbg_core.GetProcessesIds(&processes);
  for (size_t p = 0; p < processes.size(); p++) {
    debug::DebuggeeProcess* proc = dbg_core.GetProcess(processes[p]);
    if (NULL != proc) {
      std::deque<int> threads;
      proc->GetThreadIds(&threads);
      for (size_t i = 0; i < threads.size(); i++) {
        debug::DebuggeeThread* thr = proc->GetThread(threads[i]);
        if (thr && thr->is_nexe_) {
          *pid = processes[p];
          *tid = threads[i];
          return false;
        }
      }
    }
  }
  return false;
}
