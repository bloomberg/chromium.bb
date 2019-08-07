#ifndef _RAR_RESOURCE_
#define _RAR_RESOURCE_

namespace third_party_unrar {

#ifdef RARDLL
#define St(x) (L"")
#define StF(x) (L"")
#else
const wchar *St(MSGID StringId);
const wchar *StF(MSGID StringId);
#endif

}  // namespace third_party_unrar

#endif
