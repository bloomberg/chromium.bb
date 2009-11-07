#ifndef SANDBOX_H__
#define SANDBOX_H__

extern "C" int  SupportsSeccompSandbox(int proc_fd);
extern "C" void SeccompSandboxSetProcSelfMaps(int proc_self_maps);
extern "C" void StartSeccompSandbox();

#endif // SANDBOX_H__
