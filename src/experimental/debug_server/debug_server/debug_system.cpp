#include "StdAfx.h"
#include "debug_system.h"
#include <windows.h>

namespace debug {
std::string System::GetLastErrorDescription(int error_code) {
  if (0 == error_code)
		error_code = ::GetLastError();

  char buff[ 2000 ] = { '?', 0 };
	::FormatMessage(FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM,
		              0,
                  error_code,
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
					        buff,
                  sizeof(buff),
                  0);

	return std::string(buff);
}
}  // namespace debug