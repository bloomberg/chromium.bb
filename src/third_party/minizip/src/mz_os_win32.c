/* mz_os_win32.c -- System functions for Windows
   Version 2.7.5, November 13, 2018
   part of the MiniZip project

   Copyright (C) 2010-2018 Nathan Moinvaziri
     https://github.com/nmoinvaz/minizip

   This program is distributed under the terms of the same license as zlib.
   See the accompanying LICENSE file for the full text of the license.
*/

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <direct.h>
#include <errno.h>

#include <windows.h>

#include "mz.h"

#include "mz_os.h"
#include "mz_strm_os.h"

/***************************************************************************/

#if defined(WINAPI_FAMILY_PARTITION) && (!(defined(MZ_WINRT_API)))
#  if !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
#    define MZ_WINRT_API 1
#  endif
#endif

/***************************************************************************/

typedef struct DIR_int_s {
    void            *find_handle;
    WIN32_FIND_DATAW find_data;
    struct dirent    entry;
    uint8_t          end;
} DIR_int;

/***************************************************************************/

wchar_t *mz_os_unicode_string_create(const char *string, int32_t encoding)
{
    wchar_t *string_wide = NULL;
    uint32_t string_wide_size = 0;

    string_wide_size = MultiByteToWideChar(encoding, 0, string, -1, NULL, 0);
    if (string_wide_size == 0)
        return NULL;
    string_wide = (wchar_t *)MZ_ALLOC((string_wide_size + 1) * sizeof(wchar_t));
    memset(string_wide, 0, sizeof(wchar_t) * (string_wide_size + 1));

    MultiByteToWideChar(encoding, 0, string, -1, string_wide, string_wide_size);

    return string_wide;
}

void mz_os_unicode_string_delete(wchar_t **string)
{
    if (string != NULL)
    {
        MZ_FREE(*string);
        *string = NULL;
    }
}

uint8_t *mz_os_utf8_string_create(const char *string, int32_t encoding)
{
    wchar_t *string_wide = NULL;
    uint8_t *string_utf8 = NULL;
    uint32_t string_utf8_size = 0;

    string_wide = mz_os_unicode_string_create(string, encoding);
    if (string_wide)
    {
        string_utf8_size = WideCharToMultiByte(CP_UTF8, 0, string_wide, -1, NULL, 0, NULL, NULL);
        string_utf8 = (uint8_t *)MZ_ALLOC((string_utf8_size + 1) * sizeof(wchar_t));

        if (string_utf8)
        {
            memset(string_utf8, 0, string_utf8_size + 1);
            WideCharToMultiByte(CP_UTF8, 0, string_wide, -1, (char *)string_utf8, string_utf8_size, NULL, NULL);
        }

        mz_os_unicode_string_delete(&string_wide);
    }

    return string_utf8;
}

void mz_os_utf8_string_delete(uint8_t **string)
{
    if (string != NULL)
    {
        MZ_FREE(*string);
        *string = NULL;
    }
}

/***************************************************************************/

int32_t mz_os_rand(uint8_t *buf, int32_t size)
{
    unsigned __int64 pentium_tsc[1];
    int32_t len = 0;

    for (len = 0; len < (int)size; len += 1)
    {
        if (len % 8 == 0)
            QueryPerformanceCounter((LARGE_INTEGER *)pentium_tsc);
        buf[len] = ((unsigned char*)pentium_tsc)[len % 8];
    }

    return len;
}

int32_t mz_os_rename(const char *source_path, const char *target_path)
{
    wchar_t *source_path_wide = NULL;
    wchar_t *target_path_wide = NULL;
    int32_t result = 0;
    int32_t err = MZ_OK;

    if (source_path == NULL || target_path == NULL)
        return MZ_PARAM_ERROR;

    source_path_wide = mz_os_unicode_string_create(source_path, MZ_ENCODING_UTF8);
    if (source_path_wide == NULL)
    {
        err = MZ_PARAM_ERROR;
    }
    else
    {
        target_path_wide = mz_os_unicode_string_create(target_path, MZ_ENCODING_UTF8);
        if (target_path_wide == NULL)
            err = MZ_PARAM_ERROR;
    }

    if (err == MZ_OK)
    {
        result = MoveFileW(source_path_wide, target_path_wide);
        if (result == 0)
            err = MZ_EXIST_ERROR;
    }

    if (target_path_wide)
        mz_os_unicode_string_delete(&target_path_wide);
    if (source_path_wide)
        mz_os_unicode_string_delete(&source_path_wide);
    
    return err;
}

int32_t mz_os_delete(const char *path)
{
    wchar_t *path_wide = NULL;
    int32_t result = 0;

    if (path == NULL)
        return MZ_PARAM_ERROR;
    path_wide = mz_os_unicode_string_create(path, MZ_ENCODING_UTF8);
    if (path_wide == NULL)
        return MZ_PARAM_ERROR;

    result = DeleteFileW(path_wide);
    mz_os_unicode_string_delete(&path_wide);

    if (result == 0)
        return MZ_EXIST_ERROR;

    return MZ_OK;
}

int32_t mz_os_file_exists(const char *path)
{
    wchar_t *path_wide = NULL;
    DWORD attribs = 0;

    if (path == NULL)
        return MZ_PARAM_ERROR;
    path_wide = mz_os_unicode_string_create(path, MZ_ENCODING_UTF8);
    if (path_wide == NULL)
        return MZ_PARAM_ERROR;

    attribs = GetFileAttributesW(path_wide);
    mz_os_unicode_string_delete(&path_wide);

    if (attribs == 0xFFFFFFFF)
        return MZ_EXIST_ERROR;

    return MZ_OK;
}

int64_t mz_os_get_file_size(const char *path)
{
    HANDLE handle = NULL;
    LARGE_INTEGER large_size;
    wchar_t *path_wide = NULL;

    if (path == NULL)
        return MZ_PARAM_ERROR;
    path_wide = mz_os_unicode_string_create(path, MZ_ENCODING_UTF8);
    if (path_wide == NULL)
        return MZ_PARAM_ERROR;
#ifdef MZ_WINRT_API
    handle = CreateFile2W(path_wide, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
#else
    handle = CreateFileW(path_wide, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
#endif
    mz_os_unicode_string_delete(&path_wide);

    large_size.QuadPart = 0;

    if (handle != INVALID_HANDLE_VALUE)
    {
        GetFileSizeEx(handle, &large_size);
        CloseHandle(handle);
    }

    return large_size.QuadPart;
}

static void mz_os_file_to_unix_time(FILETIME file_time, time_t *unix_time)
{
    uint64_t quad_file_time = 0;
    quad_file_time = file_time.dwLowDateTime;
    quad_file_time |= ((uint64_t)file_time.dwHighDateTime << 32);
    *unix_time = (time_t)((quad_file_time - 116444736000000000LL) / 10000000);
}

static void mz_os_unix_to_file_time(time_t unix_time, FILETIME *file_time)
{
    uint64_t quad_file_time = 0;
    quad_file_time = ((uint64_t)unix_time * 10000000) + 116444736000000000LL;
    file_time->dwHighDateTime = (quad_file_time >> 32);
    file_time->dwLowDateTime = (uint32_t)(quad_file_time);
}

int32_t mz_os_get_file_date(const char *path, time_t *modified_date, time_t *accessed_date, time_t *creation_date)
{
    WIN32_FIND_DATAW ff32;
    HANDLE handle = NULL;
    wchar_t *path_wide = NULL;
    int32_t err = MZ_INTERNAL_ERROR;

    if (path == NULL)
        return MZ_PARAM_ERROR;
    path_wide = mz_os_unicode_string_create(path, MZ_ENCODING_UTF8);
    if (path_wide == NULL)
        return MZ_PARAM_ERROR;

    handle = FindFirstFileW(path_wide, &ff32);
    MZ_FREE(path_wide);

    if (handle != INVALID_HANDLE_VALUE)
    {
        if (modified_date != NULL)
            mz_os_file_to_unix_time(ff32.ftLastWriteTime, modified_date);
        if (accessed_date != NULL)
            mz_os_file_to_unix_time(ff32.ftLastAccessTime, accessed_date);
        if (creation_date != NULL)
            mz_os_file_to_unix_time(ff32.ftCreationTime, creation_date);

        FindClose(handle);
        err = MZ_OK;
    }

    return err;
}

int32_t mz_os_set_file_date(const char *path, time_t modified_date, time_t accessed_date, time_t creation_date)
{
    HANDLE handle = NULL;
    FILETIME ftm_creation, ftm_accessed, ftm_modified;
    wchar_t *path_wide = NULL;
    int32_t err = MZ_OK;

    if (path == NULL)
        return MZ_PARAM_ERROR;
    path_wide = mz_os_unicode_string_create(path, MZ_ENCODING_UTF8);
    if (path_wide == NULL)
        return MZ_PARAM_ERROR;

#ifdef MZ_WINRT_API
    handle = CreateFile2W(path_wide, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
#else
    handle = CreateFileW(path_wide, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
#endif
    mz_os_unicode_string_delete(&path_wide);

    if (handle != INVALID_HANDLE_VALUE)
    {
        GetFileTime(handle, &ftm_creation, &ftm_accessed, &ftm_modified);

        if (modified_date != 0)
            mz_os_unix_to_file_time(modified_date, &ftm_modified);
        if (accessed_date != 0)
            mz_os_unix_to_file_time(accessed_date, &ftm_accessed);
        if (creation_date != 0)
            mz_os_unix_to_file_time(creation_date, &ftm_creation);

        if (SetFileTime(handle, &ftm_creation, &ftm_accessed, &ftm_modified) == 0)
            err = MZ_INTERNAL_ERROR;

        CloseHandle(handle);
    }

    return err;
}

int32_t mz_os_get_file_attribs(const char *path, uint32_t *attributes)
{
    wchar_t *path_wide = NULL;
    int32_t err = MZ_OK;

    if (path == NULL || attributes == NULL)
        return MZ_PARAM_ERROR;
    path_wide = mz_os_unicode_string_create(path, MZ_ENCODING_UTF8);
    if (path_wide == NULL)
        return MZ_PARAM_ERROR;

    *attributes = GetFileAttributesW(path_wide);
    mz_os_unicode_string_delete(&path_wide);

    if (*attributes == INVALID_FILE_ATTRIBUTES)
        err = MZ_INTERNAL_ERROR;

    return err;
}

int32_t mz_os_set_file_attribs(const char *path, uint32_t attributes)
{
    wchar_t *path_wide = NULL;
    int32_t err = MZ_OK;

    if (path == NULL)
        return MZ_PARAM_ERROR;
    path_wide = mz_os_unicode_string_create(path, MZ_ENCODING_UTF8);
    if (path_wide == NULL)
        return MZ_PARAM_ERROR;

    if (SetFileAttributesW(path_wide, attributes) == 0)
        err = MZ_INTERNAL_ERROR;
    mz_os_unicode_string_delete(&path_wide);

    return err;
}

int32_t mz_os_make_dir(const char *path)
{
    wchar_t *path_wide = NULL;
    int32_t err = 0;

    if (path == NULL)
        return MZ_PARAM_ERROR;
    path_wide = mz_os_unicode_string_create(path, MZ_ENCODING_UTF8);
    if (path_wide == NULL)
        return MZ_PARAM_ERROR;

    err = _wmkdir(path_wide);
    mz_os_unicode_string_delete(&path_wide);

    if (err != 0 && errno != EEXIST)
        return MZ_INTERNAL_ERROR;

    return MZ_OK;
}

DIR *mz_os_open_dir(const char *path)
{
    WIN32_FIND_DATAW find_data;
    DIR_int *dir_int = NULL;
    wchar_t *path_wide = NULL;
    char fixed_path[320];
    void *handle = NULL;


    if (path == NULL)
        return NULL;

    fixed_path[0] = 0;
    mz_path_combine(fixed_path, path, sizeof(fixed_path));
    mz_path_combine(fixed_path, "*", sizeof(fixed_path));

    path_wide = mz_os_unicode_string_create(fixed_path, MZ_ENCODING_UTF8);
    if (path_wide == NULL)
        return NULL;

    handle = FindFirstFileW(path_wide, &find_data);
    mz_os_unicode_string_delete(&path_wide);

    if (handle == INVALID_HANDLE_VALUE)
        return NULL;

    dir_int = (DIR_int *)MZ_ALLOC(sizeof(DIR_int));
    dir_int->find_handle = handle;
    dir_int->end = 0;

    memcpy(&dir_int->find_data, &find_data, sizeof(dir_int->find_data));

    return (DIR *)dir_int;
}

struct dirent* mz_os_read_dir(DIR *dir)
{
    DIR_int *dir_int;

    if (dir == NULL)
        return NULL;

    dir_int = (DIR_int *)dir;
    if (dir_int->end)
        return NULL;

    WideCharToMultiByte(CP_UTF8, 0, dir_int->find_data.cFileName, -1,
        dir_int->entry.d_name, sizeof(dir_int->entry.d_name), NULL, NULL);

    if (FindNextFileW(dir_int->find_handle, &dir_int->find_data) == 0)
    {
        if (GetLastError() != ERROR_NO_MORE_FILES)
            return NULL;

        dir_int->end = 1;
    }

    return &dir_int->entry;
}

int32_t mz_os_close_dir(DIR *dir)
{
    DIR_int *dir_int;

    if (dir == NULL)
        return MZ_PARAM_ERROR;

    dir_int = (DIR_int *)dir;
    if (dir_int->find_handle != INVALID_HANDLE_VALUE)
        FindClose(dir_int->find_handle);
    MZ_FREE(dir_int);
    return MZ_OK;
}

int32_t mz_os_is_dir(const char *path)
{
    wchar_t *path_wide = NULL;
    uint32_t attribs = 0;

    if (path == NULL)
        return MZ_PARAM_ERROR;
    path_wide = mz_os_unicode_string_create(path, MZ_ENCODING_UTF8);
    if (path_wide == NULL)
        return MZ_PARAM_ERROR;

    attribs = GetFileAttributesW(path_wide);
    mz_os_unicode_string_delete(&path_wide);

    if (attribs != 0xFFFFFFFF)
    {
        if (attribs & FILE_ATTRIBUTE_DIRECTORY)
            return MZ_OK;
    }

    return MZ_EXIST_ERROR;
}

uint64_t mz_os_ms_time(void)
{
    SYSTEMTIME system_time;
    FILETIME file_time;
    uint64_t quad_file_time = 0;

    GetSystemTime(&system_time);
    SystemTimeToFileTime(&system_time, &file_time);

    quad_file_time = file_time.dwLowDateTime;
    quad_file_time |= ((uint64_t)file_time.dwHighDateTime << 32);

    return quad_file_time / 10000 - 11644473600000LL;
}
