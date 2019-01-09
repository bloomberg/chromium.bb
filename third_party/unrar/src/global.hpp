#ifndef _RAR_GLOBAL_
#define _RAR_GLOBAL_

namespace third_party_unrar {

#ifdef INCLUDEGLOBAL
  #define EXTVAR
#else
  #define EXTVAR extern
#endif

EXTVAR ErrorHandler ErrHandler;

}  // namespace third_party_unrar

#endif
