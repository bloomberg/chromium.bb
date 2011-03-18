#pragma once

namespace debug {
class DebuggeeProcess;
class NaclInfoImpl;

class NaclInfo {
public:
	NaclInfo()
	: process_(0), thread_inf_(0), impl_(0) {}
	~NaclInfo() {}

  void Init(DebuggeeProcess* process, void* thread_inf);
	void* GetMemBase();
	void* GetEntryPoint();

 private:
  DebuggeeProcess* process_;
  void* thread_inf_;
  NaclInfoImpl* impl_;
};

}  // namespace debug