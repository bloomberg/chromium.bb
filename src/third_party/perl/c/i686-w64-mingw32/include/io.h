/**
 * This file has no copyright assigned and is placed in the Public Domain.
 * This file is part of the w64 mingw-runtime package.
 * No warranty is given; refer to the file DISCLAIMER.PD within this package.
 */
#ifndef _IO_H_
#define _IO_H_

#include <_mingw.h>
#include <string.h>

#pragma pack(push,_CRT_PACKING)

#ifdef __cplusplus
extern "C" {
#endif

_CRTIMP char* __cdecl _getcwd (char*, int);
#ifndef _FSIZE_T_DEFINED
  typedef unsigned long _fsize_t;
#define _FSIZE_T_DEFINED
#endif

#ifndef _FINDDATA_T_DEFINED

  struct _finddata32_t {
    unsigned attrib;
    __time32_t time_create;
    __time32_t time_access;
    __time32_t time_write;
    _fsize_t size;
    char name[260];
  };

  struct _finddata32i64_t {
    unsigned attrib;
    __time32_t time_create;
    __time32_t time_access;
    __time32_t time_write;
    __MINGW_EXTENSION __int64 size;
    char name[260];
  };

  struct _finddata64i32_t {
    unsigned attrib;
    __time64_t time_create;
    __time64_t time_access;
    __time64_t time_write;
    _fsize_t size;
    char name[260];
  };

  struct __finddata64_t {
    unsigned attrib;
    __time64_t time_create;
    __time64_t time_access;
    __time64_t time_write;
    __MINGW_EXTENSION __int64 size;
    char name[260];
  };

#ifdef _USE_32BIT_TIME_T
#define _finddata_t _finddata32_t
#define _finddatai64_t _finddata32i64_t

#define _findfirst32 _findfirst
#define _findnext32 _findnext
#define _findfirsti64 _findfirst32i64
#define _findnexti64 _findnext32i64
#else
#define _finddata_t _finddata64i32_t
#define _finddatai64_t __finddata64_t

#define _findfirst _findfirst64i32
#define _findnext _findnext64i32
#define _findfirsti64 _findfirst64
#define _findnexti64 _findnext64
#endif /* _USE_32BIT_TIME_T */

#define _FINDDATA_T_DEFINED
#endif /* _FINDDATA_T_DEFINED */

#ifndef _WFINDDATA_T_DEFINED

  struct _wfinddata32_t {
    unsigned attrib;
    __time32_t time_create;
    __time32_t time_access;
    __time32_t time_write;
    _fsize_t size;
    wchar_t name[260];
  };

  struct _wfinddata32i64_t {
    unsigned attrib;
    __time32_t time_create;
    __time32_t time_access;
    __time32_t time_write;
    __MINGW_EXTENSION __int64 size;
    wchar_t name[260];
  };

  struct _wfinddata64i32_t {
    unsigned attrib;
    __time64_t time_create;
    __time64_t time_access;
    __time64_t time_write;
    _fsize_t size;
    wchar_t name[260];
  };

  struct _wfinddata64_t {
    unsigned attrib;
    __time64_t time_create;
    __time64_t time_access;
    __time64_t time_write;
    __MINGW_EXTENSION __int64 size;
    wchar_t name[260];
  };

#ifdef _USE_32BIT_TIME_T
#define _wfinddata_t _wfinddata32_t
#define _wfinddatai64_t _wfinddata32i64_t

#define _wfindfirst32 _wfindfirst
#define _wfindnext32 _wfindnext
#define _wfindfirst32i64 _wfindfirsti64
#define _wfindnext32i64 _wfindnexti64
#else
#define _wfinddata_t _wfinddata64i32_t
#define _wfinddatai64_t _wfinddata64_t

#define _wfindfirst _wfindfirst64i32
#define _wfindnext _wfindnext64i32
#define _wfindfirsti64 _wfindfirst64
#define _wfindnexti64 _wfindnext64
#endif /* _USE_32BIT_TIME_T */

#define _WFINDDATA_T_DEFINED
#endif /* _WFINDDATA_T_DEFINED */

#define _A_NORMAL 0x00
#define _A_RDONLY 0x01
#define _A_HIDDEN 0x02
#define _A_SYSTEM 0x04
#define _A_SUBDIR 0x10
#define _A_ARCH 0x20

#ifndef _SIZE_T_DEFINED
#define _SIZE_T_DEFINED
#undef size_t
#ifdef _WIN64
  __MINGW_EXTENSION typedef unsigned __int64 size_t;
#else
  typedef unsigned int size_t;
#endif /* _WIN64 */
#endif /* _SIZE_T_DEFINED */

#ifndef _SSIZE_T_DEFINED
#define _SSIZE_T_DEFINED
#undef ssize_t
#ifdef _WIN64
  __MINGW_EXTENSION typedef __int64 ssize_t;
#else
  typedef int ssize_t;
#endif /* _WIN64 */
#endif /* _SSIZE_T_DEFINED */

#include <_mingw_off_t.h>

  /* Some defines for _access nAccessMode (MS doesn't define them, but
  * it doesn't seem to hurt to add them). */
#define	F_OK	0	/* Check for file existence */
#define	X_OK	1	/* Check for execute permission. */
#define	W_OK	2	/* Check for write permission */
#define	R_OK	4	/* Check for read permission */

  _CRTIMP int __cdecl _access(const char *_Filename,int _AccessMode);
  _CRTIMP int __cdecl _chmod(const char *_Filename,int _Mode);
  _CRTIMP int __cdecl _chsize(int _FileHandle,long _Size) __MINGW_ATTRIB_DEPRECATED_SEC_WARN;
  _CRTIMP int __cdecl _close(int _FileHandle);
  _CRTIMP int __cdecl _commit(int _FileHandle);
  _CRTIMP int __cdecl _creat(const char *_Filename,int _PermissionMode) __MINGW_ATTRIB_DEPRECATED_SEC_WARN;
  _CRTIMP int __cdecl _dup(int _FileHandle);
  _CRTIMP int __cdecl _dup2(int _FileHandleSrc,int _FileHandleDst);
  _CRTIMP int __cdecl _eof(int _FileHandle);
  _CRTIMP long __cdecl _filelength(int _FileHandle);
  _CRTIMP intptr_t __cdecl _findfirst32(const char *_Filename,struct _finddata32_t *_FindData);
  _CRTIMP int __cdecl _findnext32(intptr_t _FindHandle,struct _finddata32_t *_FindData);
  _CRTIMP int __cdecl _findclose(intptr_t _FindHandle);
  _CRTIMP int __cdecl _isatty(int _FileHandle);
  _CRTIMP int __cdecl _locking(int _FileHandle,int _LockMode,long _NumOfBytes);
  _CRTIMP long __cdecl _lseek(int _FileHandle,long _Offset,int _Origin);
  _off64_t lseek64(int fd,_off64_t offset, int whence);
  _CRTIMP char *__cdecl _mktemp(char *_TemplateName) __MINGW_ATTRIB_DEPRECATED_SEC_WARN;
  _CRTIMP int __cdecl _pipe(int *_PtHandles,unsigned int _PipeSize,int _TextMode);
  _CRTIMP int __cdecl _read(int _FileHandle,void *_DstBuf,unsigned int _MaxCharCount);

#ifndef _CRT_DIRECTORY_DEFINED
#define _CRT_DIRECTORY_DEFINED
  int __cdecl remove(const char *_Filename);
  int __cdecl rename(const char *_OldFilename,const char *_NewFilename);
  _CRTIMP int __cdecl _unlink(const char *_Filename);
#ifndef	NO_OLDNAMES
  int __cdecl unlink(const char *_Filename) __MINGW_ATTRIB_DEPRECATED_MSVC2005;
#endif
#endif

  _CRTIMP int __cdecl _setmode(int _FileHandle,int _Mode);
  _CRTIMP long __cdecl _tell(int _FileHandle);
  _CRTIMP int __cdecl _umask(int _Mode) __MINGW_ATTRIB_DEPRECATED_SEC_WARN;
  _CRTIMP int __cdecl _write(int _FileHandle,const void *_Buf,unsigned int _MaxCharCount);

  __MINGW_EXTENSION _CRTIMP __int64 __cdecl _filelengthi64(int _FileHandle);
  _CRTIMP intptr_t __cdecl _findfirst32i64(const char *_Filename,struct _finddata32i64_t *_FindData);
  _CRTIMP intptr_t __cdecl _findfirst64(const char *_Filename,struct __finddata64_t *_FindData);
#ifdef __cplusplus
#include <string.h>
#endif
  intptr_t __cdecl _findfirst64i32(const char *_Filename,struct _finddata64i32_t *_FindData);
#ifndef __CRT__NO_INLINE
  __CRT_INLINE intptr_t __cdecl _findfirst64i32(const char *_Filename,struct _finddata64i32_t *_FindData)
  {
    struct __finddata64_t fd;
    intptr_t ret = _findfirst64(_Filename,&fd);
    if (ret == -1) {
      memset(_FindData,0,sizeof(struct _finddata64i32_t));
      return -1;
    }
    _FindData->attrib=fd.attrib;
    _FindData->time_create=fd.time_create;
    _FindData->time_access=fd.time_access;
    _FindData->time_write=fd.time_write;
    _FindData->size=(_fsize_t) fd.size;
    strncpy(_FindData->name,fd.name,260);
    return ret;
  }
#endif /* __CRT__NO_INLINE */
  _CRTIMP int __cdecl _findnext32i64(intptr_t _FindHandle,struct _finddata32i64_t *_FindData);
  _CRTIMP int __cdecl _findnext64(intptr_t _FindHandle,struct __finddata64_t *_FindData);
  int __cdecl _findnext64i32(intptr_t _FindHandle,struct _finddata64i32_t *_FindData);
#ifndef __CRT__NO_INLINE
  __CRT_INLINE int __cdecl _findnext64i32(intptr_t _FindHandle,struct _finddata64i32_t *_FindData)
  {
    struct __finddata64_t fd;
    int ret = _findnext64(_FindHandle,&fd);
    if (ret == -1) {
      memset(_FindData,0,sizeof(struct _finddata64i32_t));
      return -1;
    }
    _FindData->attrib=fd.attrib;
    _FindData->time_create=fd.time_create;
    _FindData->time_access=fd.time_access;
    _FindData->time_write=fd.time_write;
    _FindData->size=(_fsize_t) fd.size;
    strncpy(_FindData->name,fd.name,260);
    return ret;
  }
#endif /* __CRT__NO_INLINE */
  __MINGW_EXTENSION __int64 __cdecl _lseeki64(int _FileHandle,__int64 _Offset,int _Origin);
  __MINGW_EXTENSION __int64 __cdecl _telli64(int _FileHandle);

#ifndef NO_OLDNAMES
#ifndef _UWIN
  int __cdecl chdir (const char *) __MINGW_ATTRIB_DEPRECATED_MSVC2005;
  char *__cdecl getcwd (char *, int) __MINGW_ATTRIB_DEPRECATED_MSVC2005;
  int __cdecl mkdir (const char *) __MINGW_ATTRIB_DEPRECATED_MSVC2005;
  char *__cdecl mktemp(char *) __MINGW_ATTRIB_DEPRECATED_MSVC2005;
  int __cdecl rmdir (const char*) __MINGW_ATTRIB_DEPRECATED_MSVC2005;
  int __cdecl chmod (const char *, int) __MINGW_ATTRIB_DEPRECATED_MSVC2005;
#endif /* _UWIN */
#endif /* Not NO_OLDNAMES */

  _CRTIMP errno_t __cdecl _sopen_s(int *_FileHandle,const char *_Filename,int _OpenFlag,int _ShareFlag,int _PermissionMode);

  _CRTIMP int __cdecl _open(const char *_Filename,int _OpenFlag,...) __MINGW_ATTRIB_DEPRECATED_SEC_WARN;
  _CRTIMP int __cdecl _sopen(const char *_Filename,int _OpenFlag,int _ShareFlag,...) __MINGW_ATTRIB_DEPRECATED_SEC_WARN;

#ifndef _WIO_DEFINED
#define _WIO_DEFINED
  _CRTIMP int __cdecl _waccess(const wchar_t *_Filename,int _AccessMode);
  _CRTIMP int __cdecl _wchmod(const wchar_t *_Filename,int _Mode);
  _CRTIMP int __cdecl _wcreat(const wchar_t *_Filename,int _PermissionMode) __MINGW_ATTRIB_DEPRECATED_SEC_WARN;
  _CRTIMP intptr_t __cdecl _wfindfirst32(const wchar_t *_Filename,struct _wfinddata32_t *_FindData);
  _CRTIMP int __cdecl _wfindnext32(intptr_t _FindHandle,struct _wfinddata32_t *_FindData);
  _CRTIMP int __cdecl _wunlink(const wchar_t *_Filename);
  _CRTIMP int __cdecl _wrename(const wchar_t *_OldFilename,const wchar_t *_NewFilename);
  _CRTIMP wchar_t *__cdecl _wmktemp(wchar_t *_TemplateName) __MINGW_ATTRIB_DEPRECATED_SEC_WARN;

  _CRTIMP intptr_t __cdecl _wfindfirst32i64(const wchar_t *_Filename,struct _wfinddata32i64_t *_FindData);
  intptr_t __cdecl _wfindfirst64i32(const wchar_t *_Filename,struct _wfinddata64i32_t *_FindData);
  _CRTIMP intptr_t __cdecl _wfindfirst64(const wchar_t *_Filename,struct _wfinddata64_t *_FindData);
  _CRTIMP int __cdecl _wfindnext32i64(intptr_t _FindHandle,struct _wfinddata32i64_t *_FindData);
  int __cdecl _wfindnext64i32(intptr_t _FindHandle,struct _wfinddata64i32_t *_FindData);
  _CRTIMP int __cdecl _wfindnext64(intptr_t _FindHandle,struct _wfinddata64_t *_FindData);

  _CRTIMP errno_t __cdecl _wsopen_s(int *_FileHandle,const wchar_t *_Filename,int _OpenFlag,int _ShareFlag,int _PermissionFlag);

  _CRTIMP int __cdecl _wopen(const wchar_t *_Filename,int _OpenFlag,...) __MINGW_ATTRIB_DEPRECATED_SEC_WARN;
  _CRTIMP int __cdecl _wsopen(const wchar_t *_Filename,int _OpenFlag,int _ShareFlag,...) __MINGW_ATTRIB_DEPRECATED_SEC_WARN;

#endif

  int __cdecl __lock_fhandle(int _Filehandle);
  void __cdecl _unlock_fhandle(int _Filehandle);
  _CRTIMP intptr_t __cdecl _get_osfhandle(int _FileHandle);
  _CRTIMP int __cdecl _open_osfhandle(intptr_t _OSFileHandle,int _Flags);

#ifndef NO_OLDNAMES
  int __cdecl access(const char *_Filename,int _AccessMode) __MINGW_ATTRIB_DEPRECATED_MSVC2005;
  int __cdecl chmod(const char *_Filename,int _AccessMode) __MINGW_ATTRIB_DEPRECATED_MSVC2005;
  int __cdecl chsize(int _FileHandle,long _Size) __MINGW_ATTRIB_DEPRECATED_MSVC2005;
  int __cdecl close(int _FileHandle) __MINGW_ATTRIB_DEPRECATED_MSVC2005;
  int __cdecl creat(const char *_Filename,int _PermissionMode) __MINGW_ATTRIB_DEPRECATED_MSVC2005;
  int __cdecl dup(int _FileHandle) __MINGW_ATTRIB_DEPRECATED_MSVC2005;
  int __cdecl dup2(int _FileHandleSrc,int _FileHandleDst) __MINGW_ATTRIB_DEPRECATED_MSVC2005;
  int __cdecl eof(int _FileHandle) __MINGW_ATTRIB_DEPRECATED_MSVC2005;
  long __cdecl filelength(int _FileHandle) __MINGW_ATTRIB_DEPRECATED_MSVC2005;
  int __cdecl isatty(int _FileHandle) __MINGW_ATTRIB_DEPRECATED_MSVC2005;
  int __cdecl locking(int _FileHandle,int _LockMode,long _NumOfBytes) __MINGW_ATTRIB_DEPRECATED_MSVC2005;
  long __cdecl lseek(int _FileHandle,long _Offset,int _Origin) __MINGW_ATTRIB_DEPRECATED_MSVC2005;
  char *__cdecl mktemp(char *_TemplateName)  __MINGW_ATTRIB_DEPRECATED_MSVC2005;
  int __cdecl open(const char *_Filename,int _OpenFlag,...)  __MINGW_ATTRIB_DEPRECATED_MSVC2005;
  int __cdecl read(int _FileHandle,void *_DstBuf,unsigned int _MaxCharCount)  __MINGW_ATTRIB_DEPRECATED_MSVC2005;
  int __cdecl setmode(int _FileHandle,int _Mode) __MINGW_ATTRIB_DEPRECATED_MSVC2005;
  int __cdecl sopen(const char *_Filename,int _OpenFlag,int _ShareFlag,...) __MINGW_ATTRIB_DEPRECATED_MSVC2005;
  long __cdecl tell(int _FileHandle) __MINGW_ATTRIB_DEPRECATED_MSVC2005;
  int __cdecl umask(int _Mode) __MINGW_ATTRIB_DEPRECATED_MSVC2005;
  int __cdecl write(int _Filehandle,const void *_Buf,unsigned int _MaxCharCount) __MINGW_ATTRIB_DEPRECATED_MSVC2005;
#endif

#ifndef _FILE_OFFSET_BITS_SET_LSEEK
#define _FILE_OFFSET_BITS_SET_LSEEK
#if (defined(_FILE_OFFSET_BITS) && (_FILE_OFFSET_BITS == 64))
/*#define lseek(_FileHandle,_DstBuf,_MaxCharCount) lseek64(_FileHandle,_DstBuf,_MaxCharCount)*/
#define lseek lseek64
#endif /* (defined(_FILE_OFFSET_BITS) && (_FILE_OFFSET_BITS == 64)) */
#endif /* _FILE_OFFSET_BITS_SET_LSEEK */

#ifdef _POSIX

/* Misc stuff */
char *getlogin(void);
#ifdef __USE_MINGW_ALARM
unsigned int alarm(unsigned int seconds);
#endif

#endif

#ifdef __USE_MINGW_ACCESS
/*  Old versions of MSVCRT access() just ignored X_OK, while the version
    shipped with Vista, returns an error code.  This will restore the
    old behaviour  */
static inline int __mingw_access (const char *__fname, int __mode) {
  return  _access (__fname, __mode & ~X_OK);
}

#define access(__f,__m)  __mingw_access (__f, __m)
#endif


#ifdef __cplusplus
}
#endif


#pragma pack(pop)

#include <sec_api/io_s.h>

#endif /* End _IO_H_ */

