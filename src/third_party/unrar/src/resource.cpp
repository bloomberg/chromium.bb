#include "rar.hpp"

namespace third_party_unrar {

#ifndef RARDLL
const wchar *St(MSGID StringId)
{
  return StringId;
}


// Needed for Unix swprintf to convert %s to %ls in legacy language resources.
const wchar *StF(MSGID StringId)
{
  static wchar FormattedStr[512];
  PrintfPrepareFmt(St(StringId),FormattedStr,ASIZE(FormattedStr));
  return FormattedStr;
}
#endif

}  // namespace third_party_unrar
