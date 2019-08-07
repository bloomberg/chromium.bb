/**
 * This file has no copyright assigned and is placed in the Public Domain.
 * This file is part of the w64 mingw-runtime package.
 * No warranty is given; refer to the file DISCLAIMER.PD within this package.
 */
#ifndef _INC_STDIO
#define _INC_STDIO

#include <_mingw.h>

#include <_mingw_print_push.h>

#pragma pack(push,_CRT_PACKING)

#ifdef __cplusplus
extern "C" {
#endif

#define BUFSIZ 512
#define _NFILE _NSTREAM_
#define _NSTREAM_ 512
#define _IOB_ENTRIES 20
#define EOF (-1)

#ifndef _FILE_DEFINED
  struct _iobuf {
    char *_ptr;
    int _cnt;
    char *_base;
    int _flag;
    int _file;
    int _charbuf;
    int _bufsiz;
    char *_tmpfname;
  };
  typedef struct _iobuf FILE;
#define _FILE_DEFINED
#endif

#ifdef _POSIX_
#define _P_tmpdir "/"
#define _wP_tmpdir L"/"
#else
#define _P_tmpdir "\\"
#define _wP_tmpdir L"\\"
#endif

#define L_tmpnam (sizeof(_P_tmpdir) + 12)

#ifdef _POSIX_
#define L_ctermid 9
#define L_cuserid 32
#endif

#define SEEK_CUR 1
#define SEEK_END 2
#define SEEK_SET 0

#define	STDIN_FILENO	0
#define	STDOUT_FILENO	1
#define	STDERR_FILENO	2

#define FILENAME_MAX 260
#define FOPEN_MAX 20
#define _SYS_OPEN 20
#define TMP_MAX 32767

#ifndef NULL
#ifdef __cplusplus
#ifndef _WIN64
#define NULL 0
#else
#define NULL 0LL
#endif  /* W64 */
#else
#define NULL ((void *)0)
#endif
#endif

#include <_mingw_off_t.h>

#ifndef _STDIO_DEFINED
#ifdef _WIN64
  _CRTIMP FILE *__cdecl __iob_func(void);
#define _iob  __iob_func()
#else
#ifdef _MSVCRT_
extern FILE _iob[];	/* A pointer to an array of FILE */
#define __iob_func()	(_iob)
#else
extern FILE (* __MINGW_IMP_SYMBOL(_iob))[];	/* A pointer to an array of FILE */
#define __iob_func()	(* __MINGW_IMP_SYMBOL(_iob))
#define _iob __iob_func()
#endif
#endif
#endif

#ifndef _FPOS_T_DEFINED
#define _FPOS_T_DEFINED
#undef _FPOSOFF

#if (!defined(NO_OLDNAMES) || defined(__GNUC__))
  __MINGW_EXTENSION typedef __int64 fpos_t;
#define _FPOSOFF(fp) ((long)(fp))
#else
  __MINGW_EXTENSION typedef long long fpos_t;
#define _FPOSOFF(fp) ((long)(fp))
#endif

#endif

#ifndef _STDSTREAM_DEFINED
#define _STDSTREAM_DEFINED

#define stdin (&__iob_func()[0])
#define stdout (&__iob_func()[1])
#define stderr (&__iob_func()[2])
#endif

#define _IOREAD 0x0001
#define _IOWRT 0x0002

#define _IOFBF 0x0000
#define _IOLBF 0x0040
#define _IONBF 0x0004

#define _IOMYBUF 0x0008
#define _IOEOF 0x0010
#define _IOERR 0x0020
#define _IOSTRG 0x0040
#define _IORW 0x0080
#ifdef _POSIX_
#define _IOAPPEND 0x0200
#endif

#define _TWO_DIGIT_EXPONENT 0x1

#ifndef _STDIO_DEFINED
extern
  __attribute__((__format__ (gnu_scanf, 2, 3))) __MINGW_ATTRIB_NONNULL(2)
  int __cdecl __mingw_sscanf(const char * __restrict__ _Src,const char * __restrict__ _Format,...);
extern
  __attribute__((__format__ (gnu_scanf, 2, 0))) __MINGW_ATTRIB_NONNULL(2)
  int __cdecl __mingw_vsscanf (const char * __restrict__ _Str,const char * __restrict__ Format,va_list argp);
extern
  __attribute__((__format__ (gnu_scanf, 1, 2))) __MINGW_ATTRIB_NONNULL(1)
  int __cdecl __mingw_scanf(const char * __restrict__ _Format,...);
extern
  __attribute__((__format__ (gnu_scanf, 1, 0))) __MINGW_ATTRIB_NONNULL(1)
  int __cdecl __mingw_vscanf(const char * __restrict__ Format, va_list argp);
extern
  __attribute__((__format__ (gnu_scanf, 2, 3))) __MINGW_ATTRIB_NONNULL(2)
  int __cdecl __mingw_fscanf(FILE * __restrict__ _File,const char * __restrict__ _Format,...);
extern
  __attribute__((__format__ (gnu_scanf, 2, 0))) __MINGW_ATTRIB_NONNULL(2)
  int __cdecl __mingw_vfscanf (FILE * __restrict__ fp, const char * __restrict__ Format,va_list argp);

extern
  __attribute__((__format__ (gnu_printf, 3, 0))) __MINGW_ATTRIB_NONNULL(3)
  int __cdecl __mingw_vsnprintf(char * __restrict__ _DstBuf,size_t _MaxCount,const char * __restrict__ _Format,
                               va_list _ArgList);
extern
  __attribute__((__format__ (gnu_printf, 3, 4))) __MINGW_ATTRIB_NONNULL(3)
  int __cdecl __mingw_snprintf(char * __restrict__ s, size_t n, const char * __restrict__  format, ...);
extern
  __attribute__((__format__ (gnu_printf, 1, 2))) __MINGW_ATTRIB_NONNULL(1)
  int __cdecl __mingw_printf(const char * __restrict__ , ... ) __MINGW_NOTHROW;
extern
  __attribute__((__format__ (gnu_printf, 1, 0))) __MINGW_ATTRIB_NONNULL(1)
  int __cdecl __mingw_vprintf (const char * __restrict__ , va_list) __MINGW_NOTHROW;
extern
  __attribute__((__format__ (gnu_printf, 2, 3))) __MINGW_ATTRIB_NONNULL(2)
  int __cdecl __mingw_fprintf (FILE * __restrict__ , const char * __restrict__ , ...) __MINGW_NOTHROW;
extern
  __attribute__((__format__ (gnu_printf, 2, 0))) __MINGW_ATTRIB_NONNULL(2)
  int __cdecl __mingw_vfprintf (FILE * __restrict__ , const char * __restrict__ , va_list) __MINGW_NOTHROW;
extern
  __attribute__((__format__ (gnu_printf, 2, 3))) __MINGW_ATTRIB_NONNULL(2)
  int __cdecl __mingw_sprintf (char * __restrict__ , const char * __restrict__ , ...) __MINGW_NOTHROW;
extern
  __attribute__((__format__ (gnu_printf, 2, 0))) __MINGW_ATTRIB_NONNULL(2)
  int __cdecl __mingw_vsprintf (char * __restrict__ , const char * __restrict__ , va_list) __MINGW_NOTHROW;
extern
  __attribute__((__format__ (gnu_printf, 2, 3))) __attribute__((nonnull (1,2)))
  int __cdecl __mingw_asprintf(char ** __restrict__ , const char * __restrict__ , ...) __MINGW_NOTHROW;
extern
  __attribute__((__format__ (gnu_printf, 2, 0))) __attribute__((nonnull (1,2)))
  int __cdecl __mingw_vasprintf(char ** __restrict__ , const char * __restrict__ , va_list) __MINGW_NOTHROW;

#if __USE_MINGW_ANSI_STDIO
/*
 * User has expressed a preference for C99 conformance...
 */

static __attribute__ ((__unused__))
__attribute__((__format__ (gnu_scanf, 2, 3))) __MINGW_ATTRIB_NONNULL(2)
int sscanf(const char *__source, const char *__format, ...)
{
  register int __retval;
  __builtin_va_list __local_argv; __builtin_va_start( __local_argv, __format );
  __retval = __mingw_vsscanf( __source, __format, __local_argv );
  __builtin_va_end( __local_argv );
  return __retval;
}

static __attribute__ ((__unused__))
__attribute__((__format__ (gnu_scanf, 1, 2))) __MINGW_ATTRIB_NONNULL(1)
int scanf(const char *__format, ...)
{
  register int __retval;
  __builtin_va_list __local_argv; __builtin_va_start( __local_argv, __format );
  __retval = __mingw_vscanf( __format, __local_argv );
  __builtin_va_end( __local_argv );
  return __retval;
}

static __attribute__ ((__unused__))
__attribute__((__format__ (gnu_scanf, 2, 3))) __MINGW_ATTRIB_NONNULL(2)
int fscanf(FILE *__stream, const char *__format, ...)
{
  register int __retval;
  __builtin_va_list __local_argv; __builtin_va_start( __local_argv, __format );
  __retval = __mingw_vfscanf( __stream, __format, __local_argv );
  __builtin_va_end( __local_argv );
  return __retval;
}

#ifndef __NO_ISOCEXT  /* externs in libmingwex.a */
__forceinline
__attribute__((__format__ (gnu_scanf, 2, 0))) __MINGW_ATTRIB_NONNULL(2)
int vsscanf (const char *__source, const char *__format, __builtin_va_list __local_argv)
{
  return __mingw_vsscanf( __source, __format, __local_argv );
}

__forceinline
__attribute__((__format__ (gnu_scanf, 1, 0))) __MINGW_ATTRIB_NONNULL(1)
int vscanf(const char *__format,  __builtin_va_list __local_argv)
{
  return __mingw_vscanf( __format, __local_argv );
}

__forceinline
__attribute__((__format__ (gnu_scanf, 2, 0))) __MINGW_ATTRIB_NONNULL(2)
int vfscanf (FILE *__stream,  const char *__format, __builtin_va_list __local_argv)
{
  return __mingw_vfscanf( __stream, __format, __local_argv );
}
#endif /* __NO_ISOCEXT */



static __attribute__ ((__unused__))
__attribute__((__format__ (gnu_printf, 2, 3))) __MINGW_ATTRIB_NONNULL(2)
int fprintf (FILE *__stream, const char *__format, ...)
{
  register int __retval;
  __builtin_va_list __local_argv; __builtin_va_start( __local_argv, __format );
  __retval = __mingw_vfprintf( __stream, __format, __local_argv );
  __builtin_va_end( __local_argv );
  return __retval;
}

static __attribute__ ((__unused__))
__attribute__((__format__ (gnu_printf, 1, 2))) __MINGW_ATTRIB_NONNULL(1)
int printf (const char *__format, ...)
{
  register int __retval;
  __builtin_va_list __local_argv; __builtin_va_start( __local_argv, __format );
  __retval = __mingw_vprintf( __format, __local_argv );
  __builtin_va_end( __local_argv );
  return __retval;
}

static __attribute__ ((__unused__))
__attribute__((__format__ (gnu_printf, 2, 3))) __MINGW_ATTRIB_NONNULL(2)
int sprintf (char *__stream, const char *__format, ...)
{
  register int __retval;
  __builtin_va_list __local_argv; __builtin_va_start( __local_argv, __format );
  __retval = __mingw_vsprintf( __stream, __format, __local_argv );
  __builtin_va_end( __local_argv );
  return __retval;
}

__forceinline
__attribute__((__format__ (gnu_printf, 2, 0))) __MINGW_ATTRIB_NONNULL(2)
int vfprintf (FILE *__stream, const char *__format, __builtin_va_list __local_argv)
{
  return __mingw_vfprintf( __stream, __format, __local_argv );
}

__forceinline
__attribute__((__format__ (gnu_printf, 1, 0))) __MINGW_ATTRIB_NONNULL(1)
int vprintf (const char *__format, __builtin_va_list __local_argv)
{
  return __mingw_vprintf( __format, __local_argv );
}

__forceinline
__attribute__((__format__ (gnu_printf, 2, 0))) __MINGW_ATTRIB_NONNULL(2)
int vsprintf (char *__stream, const char *__format, __builtin_va_list __local_argv)
{
  return __mingw_vsprintf( __stream, __format, __local_argv );
}

static __attribute__ ((__unused__))
__attribute__ ((__format__ (gnu_printf, 2, 3))) __attribute__((nonnull (1,2)))
int asprintf(char **__ret, const char *__format, ...)
{
  register int __retval;
  __builtin_va_list __local_argv; __builtin_va_start( __local_argv, __format );
  __retval = __mingw_vasprintf( __ret, __format, __local_argv );
  __builtin_va_end( __local_argv );
  return __retval;
}

__forceinline
__attribute__ ((__format__ (gnu_printf, 2, 0))) __attribute__((nonnull (1,2)))
int vasprintf(char **__ret, const char *__format, __builtin_va_list __local_argv)
{
  return __mingw_vasprintf( __ret, __format, __local_argv );
}
#ifndef __NO_ISOCEXT  /* externs in libmingwex.a */
static __attribute__ ((__unused__))
__attribute__((__format__ (gnu_printf, 3, 4))) __MINGW_ATTRIB_NONNULL(3)
int snprintf (char *__stream, size_t __n, const char *__format, ...)
{
  register int __retval;
  __builtin_va_list __local_argv; __builtin_va_start( __local_argv, __format );
  __retval = __mingw_vsnprintf( __stream, __n, __format, __local_argv );
  __builtin_va_end( __local_argv );
  return __retval;
}

__forceinline
__attribute__((__format__ (gnu_printf, 3, 0))) __MINGW_ATTRIB_NONNULL(3)
int vsnprintf (char *__stream, size_t __n, const char *__format, __builtin_va_list __local_argv)
{
  return __mingw_vsnprintf( __stream, __n, __format, __local_argv );
}
#endif /* __NO_ISOCEXT */

#else /* !__USE_MINGW_ANSI_STDIO */
/*
 * Default configuration: simply direct all calls to MSVCRT...
 */
  int __cdecl fprintf(FILE * __restrict__ _File,const char * __restrict__ _Format,...);
  int __cdecl printf(const char * __restrict__ _Format,...);
  int __cdecl sprintf(char * __restrict__ _Dest,const char * __restrict__ _Format,...) __MINGW_ATTRIB_DEPRECATED_SEC_WARN;

  int __cdecl vfprintf(FILE * __restrict__ _File,const char * __restrict__ _Format,va_list _ArgList);
  int __cdecl vprintf(const char * __restrict__ _Format,va_list _ArgList);
  int __cdecl vsprintf(char * __restrict__ _Dest,const char * __restrict__ _Format,va_list _Args) __MINGW_ATTRIB_DEPRECATED_SEC_WARN;

  int __cdecl fscanf(FILE * __restrict__ _File,const char * __restrict__ _Format,...) __MINGW_ATTRIB_DEPRECATED_SEC_WARN;
  int __cdecl scanf(const char * __restrict__ _Format,...) __MINGW_ATTRIB_DEPRECATED_SEC_WARN;
  int __cdecl sscanf(const char * __restrict__ _Src,const char * __restrict__ _Format,...) __MINGW_ATTRIB_DEPRECATED_SEC_WARN;
#ifndef __NO_ISOCEXT  /* externs in libmingwex.a */
  int __cdecl vscanf(const char * __restrict__ Format, va_list argp);
  int __cdecl vfscanf (FILE * __restrict__ fp, const char * __restrict__ Format,va_list argp);
  int __cdecl vsscanf (const char * __restrict__ _Str,const char * __restrict__ Format,va_list argp);
#endif
#endif /* __USE_MINGW_ANSI_STDIO */

  _CRTIMP int __cdecl _filbuf(FILE *_File);
  _CRTIMP int __cdecl _flsbuf(int _Ch,FILE *_File);
#ifdef _POSIX_
  _CRTIMP FILE *__cdecl _fsopen(const char *_Filename,const char *_Mode);
#else
  _CRTIMP FILE *__cdecl _fsopen(const char *_Filename,const char *_Mode,int _ShFlag);
#endif
  void __cdecl clearerr(FILE *_File);
  int __cdecl fclose(FILE *_File);
  _CRTIMP int __cdecl _fcloseall(void);
#ifdef _POSIX_
  FILE *__cdecl fdopen(int _FileHandle,const char *_Mode) __MINGW_ATTRIB_DEPRECATED_MSVC2005;
#else
  _CRTIMP FILE *__cdecl _fdopen(int _FileHandle,const char *_Mode);
#endif
  int __cdecl feof(FILE *_File);
  int __cdecl ferror(FILE *_File);
  int __cdecl fflush(FILE *_File);
  int __cdecl fgetc(FILE *_File);
  _CRTIMP int __cdecl _fgetchar(void);
  int __cdecl fgetpos(FILE * __restrict__ _File ,fpos_t * __restrict__ _Pos); /* 64bit only, no 32bit version */
  int __cdecl fgetpos64(FILE * __restrict__ _File ,fpos_t * __restrict__ _Pos); /* fgetpos already 64bit */
  char *__cdecl fgets(char * __restrict__ _Buf,int _MaxCount,FILE * __restrict__ _File);
  _CRTIMP int __cdecl _fileno(FILE *_File);
#ifdef _POSIX_
  int __cdecl fileno(FILE *_File) __MINGW_ATTRIB_DEPRECATED_MSVC2005;
#endif
  _CRTIMP char *__cdecl _tempnam(const char *_DirName,const char *_FilePrefix);
  _CRTIMP int __cdecl _flushall(void);
  FILE *__cdecl fopen(const char * __restrict__ _Filename,const char * __restrict__ _Mode) __MINGW_ATTRIB_DEPRECATED_SEC_WARN;
  FILE *fopen64(const char * __restrict__ filename,const char * __restrict__  mode);
  int __cdecl fputc(int _Ch,FILE *_File);
  _CRTIMP int __cdecl _fputchar(int _Ch);
  int __cdecl fputs(const char * __restrict__ _Str,FILE * __restrict__ _File);
  size_t __cdecl fread(void * __restrict__ _DstBuf,size_t _ElementSize,size_t _Count,FILE * __restrict__ _File);
  FILE *__cdecl freopen(const char * __restrict__ _Filename,const char * __restrict__ _Mode,FILE * __restrict__ _File) __MINGW_ATTRIB_DEPRECATED_SEC_WARN;
  int __cdecl _fscanf_l(FILE * __restrict__ _File,const char * __restrict__ _Format,_locale_t locale,...) __MINGW_ATTRIB_DEPRECATED_SEC_WARN;
  int __cdecl fsetpos(FILE *_File,const fpos_t *_Pos);
  int __cdecl fsetpos64(FILE *_File,const fpos_t *_Pos); /* fsetpos already 64bit */
  int __cdecl fseek(FILE *_File,long _Offset,int _Origin);

  /* Shouldn't be any fseeko32 in glibc, 32bit to 64bit casting should be fine */
  /* int fseeko32(FILE* stream, _off_t offset, int whence);*/ /* fseeko32 redirects to fseeko64 */
  int fseeko64(FILE* stream, _off64_t offset, int whence);
  int fseeko(FILE* stream, _off_t offset, int whence);

#ifndef _FILE_OFFSET_BITS_SET_FSEEKO
#define _FILE_OFFSET_BITS_SET_FSEEKO
#if (defined(_FILE_OFFSET_BITS) && (_FILE_OFFSET_BITS == 64))
/*#define fseeko(__stream,__offset,__whence) fseeko64(__stream,__offset,__whence)*/
#define fseeko fseeko64
#else
/* fseeko32 redirects to fseeko64, though fseeko (32bit off_t) symbol is provided */
/* #define fseeko(__stream,__offset,__whence) fseeko64(__stream,__offset,__whence) */
#endif /* (defined(_FILE_OFFSET_BITS) && (_FILE_OFFSET_BITS == 64)) */
#endif /* _FILE_OFFSET_BITS_SET_FSEEKO */

  long __cdecl ftell(FILE *_File);
  /* Returns truncated 64bit off_t */
  _off_t ftello(FILE * stream);
  _off64_t ftello64(FILE * stream);

#ifndef _FILE_OFFSET_BITS_SET_FTELLO
#define _FILE_OFFSET_BITS_SET_FTELLO
#if (defined(_FILE_OFFSET_BITS) && (_FILE_OFFSET_BITS == 64))
#define ftello ftello64
#endif /* (defined(_FILE_OFFSET_BITS) && (_FILE_OFFSET_BITS == 64)) */
#endif /* _FILE_OFFSET_BITS_SET_FTELLO */

  __MINGW_EXTENSION int __cdecl _fseeki64(FILE *_File,__int64 _Offset,int _Origin);
  __MINGW_EXTENSION __int64 __cdecl _ftelli64(FILE *_File);
  size_t __cdecl fwrite(const void * __restrict__ _Str,size_t _Size,size_t _Count,FILE * __restrict__ _File);
  int __cdecl getc(FILE *_File);
  int __cdecl getchar(void);
  _CRTIMP int __cdecl _getmaxstdio(void);
  char *__cdecl gets(char *_Buffer) __MINGW_ATTRIB_DEPRECATED_SEC_WARN;
  int __cdecl _getw(FILE *_File);
#ifndef _CRT_PERROR_DEFINED
#define _CRT_PERROR_DEFINED
  void __cdecl perror(const char *_ErrMsg);
#endif
  _CRTIMP int __cdecl _pclose(FILE *_File);
  _CRTIMP FILE *__cdecl _popen(const char *_Command,const char *_Mode);
#if !defined(NO_OLDNAMES) && !defined(popen)
#define popen	_popen
#define pclose	_pclose
#endif
  int __cdecl putc(int _Ch,FILE *_File);
  int __cdecl putchar(int _Ch);
  int __cdecl puts(const char *_Str);
  _CRTIMP int __cdecl _putw(int _Word,FILE *_File);
#ifndef _CRT_DIRECTORY_DEFINED
#define _CRT_DIRECTORY_DEFINED
  int __cdecl remove(const char *_Filename);
  int __cdecl rename(const char *_OldFilename,const char *_NewFilename);
  _CRTIMP int __cdecl _unlink(const char *_Filename);
#ifndef	NO_OLDNAMES
  int __cdecl unlink(const char *_Filename) __MINGW_ATTRIB_DEPRECATED_MSVC2005;
#endif
#endif
  void __cdecl rewind(FILE *_File);
  _CRTIMP int __cdecl _rmtmp(void);
  int __cdecl _scanf_l(const char * __restrict__ format,_locale_t locale,... ) __MINGW_ATTRIB_DEPRECATED_SEC_WARN;
  void __cdecl setbuf(FILE * __restrict__ _File,char * __restrict__ _Buffer) __MINGW_ATTRIB_DEPRECATED_SEC_WARN;
  _CRTIMP int __cdecl _setmaxstdio(int _Max);
  _CRTIMP unsigned int __cdecl _set_output_format(unsigned int _Format);
  _CRTIMP unsigned int __cdecl _get_output_format(void);
  unsigned int __cdecl __mingw_set_output_format(unsigned int _Format);
  unsigned int __cdecl __mingw_get_output_format(void);
#if __USE_MINGW_OUTPUT_FORMAT_EMU == 1
#define _set_output_format __mingw_set_output_format
#define _get_output_format __mingw_get_output_format
#endif
  int __cdecl setvbuf(FILE * __restrict__ _File,char * __restrict__ _Buf,int _Mode,size_t _Size);
  _CRTIMP int __cdecl _scprintf(const char * __restrict__ _Format,...);
  int __cdecl _sscanf_l(const char * __restrict__ buffer,const char * __restrict__ format,_locale_t locale,...) __MINGW_ATTRIB_DEPRECATED_SEC_WARN;
  _CRTIMP int __cdecl _snscanf(const char * __restrict__ _Src,size_t _MaxCount,const char * __restrict__ _Format,...) __MINGW_ATTRIB_DEPRECATED_SEC_WARN;
  _CRTIMP int __cdecl _snscanf_l(const char * __restrict__ input,size_t length,const char * __restrict__ format,_locale_t locale,...) __MINGW_ATTRIB_DEPRECATED_SEC_WARN;
  FILE *__cdecl tmpfile(void) __MINGW_ATTRIB_DEPRECATED_SEC_WARN;
  char *__cdecl tmpnam(char *_Buffer);
  int __cdecl ungetc(int _Ch,FILE *_File);

  _CRTIMP int __cdecl _snprintf(char * __restrict__ _Dest,size_t _Count,const char * __restrict__ _Format,...) __MINGW_ATTRIB_DEPRECATED_SEC_WARN;
  _CRTIMP int __cdecl _snprintf_l(char * __restrict__ buffer,size_t count,const char * __restrict__ format,_locale_t locale,...) __MINGW_ATTRIB_DEPRECATED_SEC_WARN;
  _CRTIMP int __cdecl _vsnprintf(char * __restrict__ _Dest,size_t _Count,const char * __restrict__ _Format,va_list _Args) __MINGW_ATTRIB_DEPRECATED_SEC_WARN;
  _CRTIMP int __cdecl _vsnprintf_l(char * __restrict__ buffer,size_t count,const char * __restrict__ format,_locale_t locale,va_list argptr) __MINGW_ATTRIB_DEPRECATED_SEC_WARN;
  int __cdecl _sprintf_l(char * __restrict__ buffer,const char * __restrict__ format,_locale_t locale,...) __MINGW_ATTRIB_DEPRECATED_SEC_WARN;

#if !defined (__USE_MINGW_ANSI_STDIO) || __USE_MINGW_ANSI_STDIO == 0
/* this is here to deal with software defining
 * vsnprintf as _vsnprintf, eg. libxml2.  */
#pragma push_macro("snprintf")
#pragma push_macro("vsnprintf")
# undef snprintf
# undef vsnprintf
  int __cdecl vsnprintf(char * __restrict__ d,size_t n,const char * __restrict__ format,va_list arg)
    __MINGW_ATTRIB_DEPRECATED_MSVC2005 __MINGW_ATTRIB_DEPRECATED_SEC_WARN;

#ifndef __NO_ISOCEXT
  int __cdecl snprintf(char * __restrict__ s, size_t n, const char * __restrict__  format, ...);
#ifndef __CRT__NO_INLINE
  __CRT_INLINE int __cdecl vsnprintf(char * __restrict__ d,size_t n,const char * __restrict__ format,va_list arg)
  {
    return _vsnprintf (d, n, format, arg);
  }
#endif /* !__CRT__NO_INLINE */
#endif /* !__NO_ISOCEXT */
#pragma pop_macro ("vsnprintf")
#pragma pop_macro ("snprintf")
#endif

  _CRTIMP int __cdecl _vscprintf(const char * __restrict__ _Format,va_list _ArgList);
  _CRTIMP int __cdecl _set_printf_count_output(int _Value);
  _CRTIMP int __cdecl _get_printf_count_output(void);

#ifndef _WSTDIO_DEFINED
#define _WSTDIO_DEFINED

/* __attribute__((__format__ (gnu_wscanf, 2, 3))) */ __MINGW_ATTRIB_NONNULL(2)
  int __cdecl __mingw_swscanf(const wchar_t * __restrict__ _Src,const wchar_t * __restrict__ _Format,...);
/* __attribute__((__format__ (gnu_wscanf, 2, 0))) */ __MINGW_ATTRIB_NONNULL(2)
  int __cdecl __mingw_vswscanf (const wchar_t * __restrict__ _Str,const wchar_t * __restrict__ Format,va_list argp);
/* __attribute__((__format__ (gnu_wscanf, 1, 2))) */ __MINGW_ATTRIB_NONNULL(1)
  int __cdecl __mingw_wscanf(const wchar_t * __restrict__ _Format,...);
/* __attribute__((__format__ (gnu_wscanf, 1, 0))) */ __MINGW_ATTRIB_NONNULL(1)
  int __cdecl __mingw_vwscanf(const wchar_t * __restrict__ Format, va_list argp);
/* __attribute__((__format__ (gnu_wscanf, 2, 3))) */ __MINGW_ATTRIB_NONNULL(2)
  int __cdecl __mingw_fwscanf(FILE * __restrict__ _File,const wchar_t * __restrict__ _Format,...);
/* __attribute__((__format__ (gnu_wscanf, 2, 0))) */ __MINGW_ATTRIB_NONNULL(2)
  int __cdecl __mingw_vfwscanf (FILE * __restrict__ fp, const wchar_t * __restrict__ Format,va_list argp);

/* __attribute__((__format__ (gnu_wprintf, 2, 3))) */ __MINGW_ATTRIB_NONNULL(2)
  int __cdecl __mingw_fwprintf(FILE * __restrict__ _File,const wchar_t * __restrict__ _Format,...);
/* __attribute__((__format__ (gnu_wprintf, 1, 2))) */ __MINGW_ATTRIB_NONNULL(1)
  int __cdecl __mingw_wprintf(const wchar_t * __restrict__ _Format,...);
/* __attribute__((__format__ (gnu_wprintf, 2, 0))) */__MINGW_ATTRIB_NONNULL(2)
  int __cdecl __mingw_vfwprintf(FILE * __restrict__ _File,const wchar_t * __restrict__ _Format,va_list _ArgList);
/*__attribute__((__format__ (gnu_wprintf, 1, 0))) */ __MINGW_ATTRIB_NONNULL(1)
  int __cdecl __mingw_vwprintf(const wchar_t * __restrict__ _Format,va_list _ArgList);
/* __attribute__((__format__ (gnu_wprintf, 2, 3))) */ __MINGW_ATTRIB_NONNULL(2)
  int __cdecl __mingw_swprintf(wchar_t * __restrict__ , const wchar_t * __restrict__ , ...) __MINGW_ATTRIB_DEPRECATED_SEC_WARN;
/* __attribute__((__format__ (gnu_wprintf, 2, 0))) */ __MINGW_ATTRIB_NONNULL(2)
  int __cdecl __mingw_vswprintf(wchar_t * __restrict__ , const wchar_t * __restrict__ ,va_list) __MINGW_ATTRIB_DEPRECATED_SEC_WARN;
/* __attribute__((__format__ (gnu_wprintf, 3, 4))) */ __MINGW_ATTRIB_NONNULL(3)
  int __cdecl __mingw_snwprintf (wchar_t * __restrict__ s, size_t n, const wchar_t * __restrict__ format, ...);
/* __attribute__((__format__ (gnu_wprintf, 3, 0))) */ __MINGW_ATTRIB_NONNULL(3)
  int __cdecl __mingw_vsnwprintf (wchar_t * __restrict__ , size_t, const wchar_t * __restrict__ , va_list);

#if __USE_MINGW_ANSI_STDIO
/*
 * User has expressed a preference for C99 conformance...
 */
static __attribute__ ((__unused__))
/* __attribute__((__format__ (gnu_wscanf, 2, 3))) */ __MINGW_ATTRIB_NONNULL(2)
int swscanf(const wchar_t *__source, const wchar_t *__format, ...)
{
  register int __retval;
  __builtin_va_list __local_argv; __builtin_va_start( __local_argv, __format );
  __retval = __mingw_vswscanf( __source, __format, __local_argv );
  __builtin_va_end( __local_argv );
  return __retval;
}

static __attribute__ ((__unused__))
/* __attribute__((__format__ (gnu_wscanf, 1, 2))) */ __MINGW_ATTRIB_NONNULL(1)
int wscanf(const wchar_t *__format, ...)
{
  register int __retval;
  __builtin_va_list __local_argv; __builtin_va_start( __local_argv, __format );
  __retval = __mingw_vwscanf( __format, __local_argv );
  __builtin_va_end( __local_argv );
  return __retval;
}

static __attribute__ ((__unused__))
/* __attribute__((__format__ (gnu_wscanf, 2, 3))) */ __MINGW_ATTRIB_NONNULL(2)
int fwscanf(FILE *__stream, const wchar_t *__format, ...)
{
  register int __retval;
  __builtin_va_list __local_argv; __builtin_va_start( __local_argv, __format );
  __retval = __mingw_vfwscanf( __stream, __format, __local_argv );
  __builtin_va_end( __local_argv );
  return __retval;
}

#ifndef __NO_ISOCEXT  /* externs in libmingwex.a */
__forceinline
/* __attribute__((__format__ (gnu_wscanf, 2, 0))) */ __MINGW_ATTRIB_NONNULL(2)
int vswscanf (const wchar_t *__source, const wchar_t *__format, __builtin_va_list __local_argv)
{
  return __mingw_vswscanf( __source, __format, __local_argv );
}

__forceinline
/* __attribute__((__format__ (gnu_wscanf, 1, 0))) */ __MINGW_ATTRIB_NONNULL(1)
int vwscanf(const wchar_t *__format,  __builtin_va_list __local_argv)
{
  return __mingw_vwscanf( __format, __local_argv );
}

__forceinline
/* __attribute__((__format__ (gnu_wscanf, 2, 0))) */ __MINGW_ATTRIB_NONNULL(2)
int vfwscanf (FILE *__stream,  const wchar_t *__format, __builtin_va_list __local_argv)
{
  return __mingw_vfwscanf( __stream, __format, __local_argv );
}
#endif /* __NO_ISOCEXT */



static __attribute__ ((__unused__))
/* __attribute__((__format__ (gnu_wprintf, 2, 3))) */ __MINGW_ATTRIB_NONNULL(2)
int fwprintf (FILE *__stream, const wchar_t *__format, ...)
{
  register int __retval;
  __builtin_va_list __local_argv; __builtin_va_start( __local_argv, __format );
  __retval = __mingw_vfwprintf( __stream, __format, __local_argv );
  __builtin_va_end( __local_argv );
  return __retval;
}

static __attribute__ ((__unused__))
/* __attribute__((__format__ (gnu_wprintf, 1, 2))) */ __MINGW_ATTRIB_NONNULL(1)
int wprintf (const wchar_t *__format, ...)
{
  register int __retval;
  __builtin_va_list __local_argv; __builtin_va_start( __local_argv, __format );
  __retval = __mingw_vwprintf( __format, __local_argv );
  __builtin_va_end( __local_argv );
  return __retval;
}

static __attribute__ ((__unused__))
/* __attribute__((__format__ (gnu_wprintf, 2, 3))) */ __MINGW_ATTRIB_NONNULL(2)
int swprintf (wchar_t *__stream, const wchar_t *__format, ...)
{
  register int __retval;
  __builtin_va_list __local_argv; __builtin_va_start( __local_argv, __format );
  __retval = __mingw_vswprintf( __stream, __format, __local_argv );
  __builtin_va_end( __local_argv );
  return __retval;
}

__forceinline
/* __attribute__((__format__ (gnu_wprintf, 2, 0))) */ __MINGW_ATTRIB_NONNULL(2)
int vfwprintf (FILE *__stream, const wchar_t *__format, __builtin_va_list __local_argv)
{
  return __mingw_vfwprintf( __stream, __format, __local_argv );
}

__forceinline
/* __attribute__((__format__ (gnu_wprintf, 1, 0))) */ __MINGW_ATTRIB_NONNULL(1)
int vwprintf (const wchar_t *__format, __builtin_va_list __local_argv)
{
  return __mingw_vwprintf( __format, __local_argv );
}

__forceinline
/* __attribute__((__format__ (gnu_printf, 2, 0))) */ __MINGW_ATTRIB_NONNULL(2)
int vswprintf (wchar_t *__stream, const wchar_t *__format, __builtin_va_list __local_argv)
{
  return __mingw_vswprintf( __stream, __format, __local_argv );
}

#ifndef __NO_ISOCEXT  /* externs in libmingwex.a */
static __attribute__ ((__unused__))
/* __attribute__((__format__ (gnu_wprintf, 3, 4))) */ __MINGW_ATTRIB_NONNULL(3)
int snwprintf (wchar_t *__stream, size_t __n, const wchar_t *__format, ...)
{
  register int __retval;
  __builtin_va_list __local_argv; __builtin_va_start( __local_argv, __format );
  __retval = __mingw_vsnwprintf( __stream, __n, __format, __local_argv );
  __builtin_va_end( __local_argv );
  return __retval;
}

__forceinline
/* __attribute__((__format__ (gnu_wprintf, 3, 0))) */ __MINGW_ATTRIB_NONNULL(3)
int vsnwprintf (wchar_t *__stream, size_t __n, const wchar_t *__format, __builtin_va_list __local_argv)
{
  return __mingw_vsnwprintf( __stream, __n, __format, __local_argv );
}
#endif /* __NO_ISOCEXT */

#else /* !__USE_MINGW_ANSI_STDIO */

  int __cdecl fwscanf(FILE * __restrict__ _File,const wchar_t * __restrict__ _Format,...) __MINGW_ATTRIB_DEPRECATED_SEC_WARN;
  int __cdecl swscanf(const wchar_t * __restrict__ _Src,const wchar_t * __restrict__ _Format,...) __MINGW_ATTRIB_DEPRECATED_SEC_WARN;
  int __cdecl wscanf(const wchar_t * __restrict__ _Format,...) __MINGW_ATTRIB_DEPRECATED_SEC_WARN;
#ifndef __NO_ISOCEXT  /* externs in libmingwex.a */
  int __cdecl vwscanf (const wchar_t * __restrict__ , va_list);
  int __cdecl vfwscanf (FILE * __restrict__ ,const wchar_t * __restrict__ ,va_list);
  int __cdecl vswscanf (const wchar_t * __restrict__ ,const wchar_t * __restrict__ ,va_list);
#endif /* __NO_ISOCEXT */

  int __cdecl fwprintf(FILE * __restrict__ _File,const wchar_t * __restrict__ _Format,...);
  int __cdecl wprintf(const wchar_t * __restrict__ _Format,...);
  int __cdecl vfwprintf(FILE * __restrict__ _File,const wchar_t * __restrict__ _Format,va_list _ArgList);
  int __cdecl vwprintf(const wchar_t * __restrict__ _Format,va_list _ArgList);
  _CRTIMP int __cdecl swprintf(wchar_t * __restrict__ , const wchar_t * __restrict__ , ...) __MINGW_ATTRIB_DEPRECATED_SEC_WARN;
  _CRTIMP int __cdecl vswprintf(wchar_t * __restrict__ , const wchar_t * __restrict__ ,va_list) __MINGW_ATTRIB_DEPRECATED_SEC_WARN;
#endif /* __USE_MINGW_ANSI_STDIO */

#ifndef WEOF
#define WEOF (wint_t)(0xFFFF)
#endif

#ifdef _POSIX_
  _CRTIMP FILE *__cdecl _wfsopen(const wchar_t *_Filename,const wchar_t *_Mode);
#else
  _CRTIMP FILE *__cdecl _wfsopen(const wchar_t *_Filename,const wchar_t *_Mode,int _ShFlag);
#endif

  wint_t __cdecl fgetwc(FILE *_File);
  _CRTIMP wint_t __cdecl _fgetwchar(void);
  wint_t __cdecl fputwc(wchar_t _Ch,FILE *_File);
  _CRTIMP wint_t __cdecl _fputwchar(wchar_t _Ch);
  wint_t __cdecl getwc(FILE *_File);
  wint_t __cdecl getwchar(void);
  wint_t __cdecl putwc(wchar_t _Ch,FILE *_File);
  wint_t __cdecl putwchar(wchar_t _Ch);
  wint_t __cdecl ungetwc(wint_t _Ch,FILE *_File);
  wchar_t *__cdecl fgetws(wchar_t * __restrict__ _Dst,int _SizeInWords,FILE * __restrict__ _File);
  int __cdecl fputws(const wchar_t * __restrict__ _Str,FILE * __restrict__ _File);
  _CRTIMP wchar_t *__cdecl _getws(wchar_t *_String) __MINGW_ATTRIB_DEPRECATED_SEC_WARN;
  _CRTIMP int __cdecl _putws(const wchar_t *_Str);

  _CRTIMP int __cdecl _scwprintf(const wchar_t * __restrict__ _Format,...);
  _CRTIMP int __cdecl _swprintf_l(wchar_t * __restrict__ buffer,size_t count,const wchar_t * __restrict__ format,_locale_t locale,... ) __MINGW_ATTRIB_DEPRECATED_SEC_WARN;
  _CRTIMP int __cdecl _swprintf_c(wchar_t * __restrict__ _DstBuf,size_t _SizeInWords,const wchar_t * __restrict__ _Format,...);
  _CRTIMP int __cdecl _vswprintf_c(wchar_t * __restrict__ _DstBuf,size_t _SizeInWords,const wchar_t * __restrict__ _Format,va_list _ArgList);
  _CRTIMP int __cdecl _snwprintf(wchar_t * __restrict__ _Dest,size_t _Count,const wchar_t * __restrict__ _Format,...) __MINGW_ATTRIB_DEPRECATED_SEC_WARN;
  _CRTIMP int __cdecl _vsnwprintf(wchar_t * __restrict__ _Dest,size_t _Count,const wchar_t * __restrict__ _Format,va_list _Args) __MINGW_ATTRIB_DEPRECATED_SEC_WARN;

#ifndef __NO_ISOCEXT  /* externs in libmingwex.a */

#if !defined (__USE_MINGW_ANSI_STDIO) || __USE_MINGW_ANSI_STDIO == 0
#pragma push_macro("snwprintf")
#pragma push_macro("vsnwprintf")
# undef snwprintf
# undef vsnwprintf
  int __cdecl snwprintf (wchar_t * __restrict__ s, size_t n, const wchar_t * __restrict__ format, ...);
  int __cdecl vsnwprintf (wchar_t * __restrict__ , size_t, const wchar_t * __restrict__ , va_list);
#ifndef __CRT__NO_INLINE
  __CRT_INLINE int __cdecl vsnwprintf (wchar_t * __restrict__ s, size_t n, const wchar_t * __restrict__ format, va_list arg)
  {
    return _vsnwprintf(s,n,format,arg);
  }
#endif /* !__CRT__NO_INLINE */
#pragma pop_macro ("vsnwprintf")
#pragma pop_macro ("snwprintf")
#endif

#endif /* ! __NO_ISOCEXT */
  _CRTIMP int __cdecl _fwprintf_p(FILE * __restrict__ _File,const wchar_t * __restrict__ _Format,...);
  _CRTIMP int __cdecl _wprintf_p(const wchar_t * __restrict__ _Format,...);
  _CRTIMP int __cdecl _vfwprintf_p(FILE * __restrict__ _File,const wchar_t * __restrict__ _Format,va_list _ArgList);
  _CRTIMP int __cdecl _vwprintf_p(const wchar_t * __restrict__ _Format,va_list _ArgList);
  _CRTIMP int __cdecl _swprintf_p(wchar_t * __restrict__ _DstBuf,size_t _MaxCount,const wchar_t * __restrict__ _Format,...);
  _CRTIMP int __cdecl _vswprintf_p(wchar_t * __restrict__ _DstBuf,size_t _MaxCount,const wchar_t * __restrict__ _Format,va_list _ArgList);
  _CRTIMP int __cdecl _scwprintf_p(const wchar_t * __restrict__ _Format,...);
  _CRTIMP int __cdecl _vscwprintf_p(const wchar_t * __restrict__ _Format,va_list _ArgList);
  _CRTIMP int __cdecl _wprintf_l(const wchar_t * __restrict__ _Format,_locale_t _Locale,...);
  _CRTIMP int __cdecl _wprintf_p_l(const wchar_t * __restrict__ _Format,_locale_t _Locale,...);
  _CRTIMP int __cdecl _vwprintf_l(const wchar_t * __restrict__ _Format,_locale_t _Locale,va_list _ArgList);
  _CRTIMP int __cdecl _vwprintf_p_l(const wchar_t * __restrict__ _Format,_locale_t _Locale,va_list _ArgList);
  _CRTIMP int __cdecl _fwprintf_l(FILE * __restrict__ _File,const wchar_t * __restrict__ _Format,_locale_t _Locale,...);
  _CRTIMP int __cdecl _fwprintf_p_l(FILE * __restrict__ _File,const wchar_t * __restrict__ _Format,_locale_t _Locale,...);
  _CRTIMP int __cdecl _vfwprintf_l(FILE * __restrict__ _File,const wchar_t * __restrict__ _Format,_locale_t _Locale,va_list _ArgList);
  _CRTIMP int __cdecl _vfwprintf_p_l(FILE * __restrict__ _File,const wchar_t * __restrict__ _Format,_locale_t _Locale,va_list _ArgList);
  _CRTIMP int __cdecl _swprintf_c_l(wchar_t * __restrict__ _DstBuf,size_t _MaxCount,const wchar_t * __restrict__ _Format,_locale_t _Locale,...);
  _CRTIMP int __cdecl _swprintf_p_l(wchar_t * __restrict__ _DstBuf,size_t _MaxCount,const wchar_t * __restrict__ _Format,_locale_t _Locale,...);
  _CRTIMP int __cdecl _vswprintf_c_l(wchar_t * __restrict__ _DstBuf,size_t _MaxCount,const wchar_t * __restrict__ _Format,_locale_t _Locale,va_list _ArgList);
  _CRTIMP int __cdecl _vswprintf_p_l(wchar_t * __restrict__ _DstBuf,size_t _MaxCount,const wchar_t * __restrict__ _Format,_locale_t _Locale,va_list _ArgList);
  _CRTIMP int __cdecl _scwprintf_l(const wchar_t * __restrict__ _Format,_locale_t _Locale,...);
  _CRTIMP int __cdecl _scwprintf_p_l(const wchar_t * __restrict__ _Format,_locale_t _Locale,...);
  _CRTIMP int __cdecl _vscwprintf_p_l(const wchar_t * __restrict__ _Format,_locale_t _Locale,va_list _ArgList);
  _CRTIMP int __cdecl _snwprintf_l(wchar_t * __restrict__ _DstBuf,size_t _MaxCount,const wchar_t * __restrict__ _Format,_locale_t _Locale,...);
  _CRTIMP int __cdecl _vsnwprintf_l(wchar_t * __restrict__ _DstBuf,size_t _MaxCount,const wchar_t * __restrict__ _Format,_locale_t _Locale,va_list _ArgList) __MINGW_ATTRIB_DEPRECATED_SEC_WARN;
  _CRTIMP int __cdecl _swprintf(wchar_t * __restrict__ _Dest,const wchar_t * __restrict__ _Format,...);
  _CRTIMP int __cdecl _vswprintf(wchar_t * __restrict__ _Dest,const wchar_t * __restrict__ _Format,va_list _Args);
  _CRTIMP int __cdecl __swprintf_l(wchar_t * __restrict__ _Dest,const wchar_t * __restrict__ _Format,_locale_t _Plocinfo,...) __MINGW_ATTRIB_DEPRECATED_SEC_WARN;
  _CRTIMP int __cdecl _vswprintf_l(wchar_t * __restrict__ buffer,size_t count,const wchar_t * __restrict__ format,_locale_t locale,va_list argptr) __MINGW_ATTRIB_DEPRECATED_SEC_WARN;
  _CRTIMP int __cdecl __vswprintf_l(wchar_t * __restrict__ _Dest,const wchar_t * __restrict__ _Format,_locale_t _Plocinfo,va_list _Args) __MINGW_ATTRIB_DEPRECATED_SEC_WARN;

#ifdef _CRT_NON_CONFORMING_SWPRINTFS
#ifndef __cplusplus
#define swprintf _swprintf
#define vswprintf _vswprintf
#define _swprintf_l __swprintf_l
#define _vswprintf_l __vswprintf_l
#endif
#endif

  _CRTIMP wchar_t *__cdecl _wtempnam(const wchar_t *_Directory,const wchar_t *_FilePrefix);
  _CRTIMP int __cdecl _vscwprintf(const wchar_t * __restrict__ _Format,va_list _ArgList);
  _CRTIMP int __cdecl _vscwprintf_l(const wchar_t * __restrict__ _Format,_locale_t _Locale,va_list _ArgList);
  _CRTIMP int __cdecl _fwscanf_l(FILE * __restrict__ _File,const wchar_t * __restrict__ _Format,_locale_t _Locale,...) __MINGW_ATTRIB_DEPRECATED_SEC_WARN;
  _CRTIMP int __cdecl _swscanf_l(const wchar_t * __restrict__ _Src,const wchar_t * __restrict__ _Format,_locale_t _Locale,...) __MINGW_ATTRIB_DEPRECATED_SEC_WARN;
  _CRTIMP int __cdecl _snwscanf(const wchar_t * __restrict__ _Src,size_t _MaxCount,const wchar_t * __restrict__ _Format,...);
  _CRTIMP int __cdecl _snwscanf_l(const wchar_t * __restrict__ _Src,size_t _MaxCount,const wchar_t * __restrict__ _Format,_locale_t _Locale,...);
  _CRTIMP int __cdecl _wscanf_l(const wchar_t * __restrict__ _Format,_locale_t _Locale,...) __MINGW_ATTRIB_DEPRECATED_SEC_WARN;
  _CRTIMP FILE *__cdecl _wfdopen(int _FileHandle ,const wchar_t *_Mode);
  _CRTIMP FILE *__cdecl _wfopen(const wchar_t * __restrict__ _Filename,const wchar_t *__restrict__  _Mode) __MINGW_ATTRIB_DEPRECATED_SEC_WARN;
  _CRTIMP FILE *__cdecl _wfreopen(const wchar_t * __restrict__ _Filename,const wchar_t * __restrict__ _Mode,FILE * __restrict__ _OldFile) __MINGW_ATTRIB_DEPRECATED_SEC_WARN;

#ifndef _CRT_WPERROR_DEFINED
#define _CRT_WPERROR_DEFINED
  _CRTIMP void __cdecl _wperror(const wchar_t *_ErrMsg);
#endif
  _CRTIMP FILE *__cdecl _wpopen(const wchar_t *_Command,const wchar_t *_Mode);
#if !defined(NO_OLDNAMES) && !defined(wpopen)
#define wpopen	_wpopen
#endif

  _CRTIMP int __cdecl _wremove(const wchar_t *_Filename);
  _CRTIMP wchar_t *__cdecl _wtmpnam(wchar_t *_Buffer);
  _CRTIMP wint_t __cdecl _fgetwc_nolock(FILE *_File);
  _CRTIMP wint_t __cdecl _fputwc_nolock(wchar_t _Ch,FILE *_File);
  _CRTIMP wint_t __cdecl _ungetwc_nolock(wint_t _Ch,FILE *_File);

#undef _CRT_GETPUTWCHAR_NOINLINE

#if !defined(__cplusplus) || defined(_CRT_GETPUTWCHAR_NOINLINE) || defined (__CRT__NO_INLINE)
#define getwchar() fgetwc(stdin)
#define putwchar(_c) fputwc((_c),stdout)
#else
  __CRT_INLINE wint_t __cdecl getwchar() {return (fgetwc(stdin)); }
  __CRT_INLINE wint_t __cdecl putwchar(wchar_t _C) {return (fputwc(_C,stdout)); }
#endif

#define getwc(_stm) fgetwc(_stm)
#define putwc(_c,_stm) fputwc(_c,_stm)
#define _putwc_nolock(_c,_stm) _fputwc_nolock(_c,_stm)
#define _getwc_nolock(_c) _fgetwc_nolock(_c)
#endif

#define _STDIO_DEFINED
#endif

#define _fgetc_nolock(_stream) (--(_stream)->_cnt >= 0 ? 0xff & *(_stream)->_ptr++ : _filbuf(_stream))
#define _fputc_nolock(_c,_stream) (--(_stream)->_cnt >= 0 ? 0xff & (*(_stream)->_ptr++ = (char)(_c)) : _flsbuf((_c),(_stream)))
#define _getc_nolock(_stream) _fgetc_nolock(_stream)
#define _putc_nolock(_c,_stream) _fputc_nolock(_c,_stream)
#define _getchar_nolock() _getc_nolock(stdin)
#define _putchar_nolock(_c) _putc_nolock((_c),stdout)
#define _getwchar_nolock() _getwc_nolock(stdin)
#define _putwchar_nolock(_c) _putwc_nolock((_c),stdout)

  _CRTIMP void __cdecl _lock_file(FILE *_File);
  _CRTIMP void __cdecl _unlock_file(FILE *_File);
  _CRTIMP int __cdecl _fclose_nolock(FILE *_File);
  _CRTIMP int __cdecl _fflush_nolock(FILE *_File);
  _CRTIMP size_t __cdecl _fread_nolock(void * __restrict__ _DstBuf,size_t _ElementSize,size_t _Count,FILE * __restrict__ _File);
  _CRTIMP int __cdecl _fseek_nolock(FILE *_File,long _Offset,int _Origin);
  _CRTIMP long __cdecl _ftell_nolock(FILE *_File);
  __MINGW_EXTENSION _CRTIMP int __cdecl _fseeki64_nolock(FILE *_File,__int64 _Offset,int _Origin);
  __MINGW_EXTENSION _CRTIMP __int64 __cdecl _ftelli64_nolock(FILE *_File);
  _CRTIMP size_t __cdecl _fwrite_nolock(const void * __restrict__ _DstBuf,size_t _Size,size_t _Count,FILE * __restrict__ _File);
  _CRTIMP int __cdecl _ungetc_nolock(int _Ch,FILE *_File);

#if !defined(NO_OLDNAMES) || !defined(_POSIX)
#define P_tmpdir _P_tmpdir
#define SYS_OPEN _SYS_OPEN

  char *__cdecl tempnam(const char *_Directory,const char *_FilePrefix) __MINGW_ATTRIB_DEPRECATED_MSVC2005;
  int __cdecl fcloseall(void) __MINGW_ATTRIB_DEPRECATED_MSVC2005;
  FILE *__cdecl fdopen(int _FileHandle,const char *_Format) __MINGW_ATTRIB_DEPRECATED_MSVC2005;
  int __cdecl fgetchar(void) __MINGW_ATTRIB_DEPRECATED_MSVC2005;
  int __cdecl fileno(FILE *_File) __MINGW_ATTRIB_DEPRECATED_MSVC2005;
  int __cdecl flushall(void) __MINGW_ATTRIB_DEPRECATED_MSVC2005;
  int __cdecl fputchar(int _Ch) __MINGW_ATTRIB_DEPRECATED_MSVC2005;
  int __cdecl getw(FILE *_File) __MINGW_ATTRIB_DEPRECATED_MSVC2005;
  int __cdecl putw(int _Ch,FILE *_File) __MINGW_ATTRIB_DEPRECATED_MSVC2005;
  int __cdecl rmtmp(void) __MINGW_ATTRIB_DEPRECATED_MSVC2005;
#endif

#ifndef __MINGW_MBWC_CONVERT_DEFINED
#define __MINGW_MBWC_CONVERT_DEFINED

/**
 * __mingw_str_wide_utf8
 * Converts a null terminated UCS-2 string to a multibyte (UTF-8) equivalent.
 * Caller is supposed to free allocated buffer with __mingw_str_free().
 * @param[in] wptr Pointer to wide string.
 * @param[out] mbptr Pointer to multibyte string.
 * @param[out] buflen Optional parameter for length of allocated buffer.
 * @return Number of characters converted, 0 for failure.
 *
 * WideCharToMultiByte - http://msdn.microsoft.com/en-us/library/dd374130(VS.85).aspx
 */
int __cdecl __mingw_str_wide_utf8 (const wchar_t * const wptr, char **mbptr, size_t * buflen);

/**
 * __mingw_str_utf8_wide
 * Converts a null terminated UTF-8 string to a UCS-2 equivalent.
 * Caller is supposed to free allocated buffer with __mingw_str_free().
 * @param[out] mbptr Pointer to multibyte string.
 * @param[in] wptr Pointer to wide string.
 * @param[out] buflen Optional parameter for length of allocated buffer.
 * @return Number of characters converted, 0 for failure.
 *
 * MultiByteToWideChar - http://msdn.microsoft.com/en-us/library/dd319072(VS.85).aspx
 */

int __cdecl __mingw_str_utf8_wide (const char *const mbptr, wchar_t ** wptr, size_t * buflen);

/**
 * __mingw_str_free
 * Frees buffer create by __mingw_str_wide_utf8 and __mingw_str_utf8_wide.
 * @param[in] ptr memory block to free.
 *
 */

void __cdecl __mingw_str_free(void *ptr);

#endif /* __MINGW_MBWC_CONVERT_DEFINED */

#ifdef __cplusplus
}
#endif

#pragma pack(pop)

#include <sec_api/stdio_s.h>

#include <_mingw_print_pop.h>

#endif
