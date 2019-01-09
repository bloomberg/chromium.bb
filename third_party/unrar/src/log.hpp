#ifndef _RAR_LOG_
#define _RAR_LOG_

namespace third_party_unrar {

void InitLogOptions(const wchar *LogFileName,RAR_CHARSET CSet);

#ifdef SILENT
inline void Log(const wchar *ArcName,const wchar *fmt,...) {}
#else
void Log(const wchar *ArcName,const wchar *fmt,...);
#endif

}  // namespace third_party_unrar

#endif
