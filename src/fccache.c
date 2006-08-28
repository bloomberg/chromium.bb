/*
 * $RCSId: xc/lib/fontconfig/src/fccache.c,v 1.12 2002/08/22 07:36:44 keithp Exp $
 *
 * Copyright © 2000 Keith Packard
 * Copyright © 2005 Patrick Lam
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include "fcint.h"
#include "../fc-arch/fcarch.h"
#include <stdio.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <sys/types.h>
#if defined(HAVE_MMAP) || defined(__CYGWIN__)
#  include <unistd.h>
#  include <sys/mman.h>
#elif defined(_WIN32)
#  include <windows.h>
#endif

#define ENDIAN_TEST 0x12345678
#define MACHINE_SIGNATURE_SIZE (9 + 5*20 + 1)
/* for when we don't have sysconf: */
#define FC_HARDCODED_PAGESIZE 8192 

#ifndef O_BINARY
#define O_BINARY 0
#endif

static FILE *
FcDirCacheOpen (FcConfig *config, const FcChar8 *dir, FcChar8 **cache_path);

static void *
FcDirCacheProduce (FcFontSet *set, FcCache * metadata);

static off_t
FcCacheNextOffset(off_t w);

static FcBool
FcCacheHaveBank (int bank);

static void
FcCacheAddBankDir (int bank, const char * dir);

struct MD5Context {
        FcChar32 buf[4];
        FcChar32 bits[2];
        unsigned char in[64];
};

static void MD5Init(struct MD5Context *ctx);
static void MD5Update(struct MD5Context *ctx, unsigned char *buf, unsigned len);
static void MD5Final(unsigned char digest[16], struct MD5Context *ctx);
static void MD5Transform(FcChar32 buf[4], FcChar32 in[16]);

#define FC_DBG_CACHE_REF    1024

static char *
FcCacheReadString (FILE *file, char *dest, int len)
{
    int	    c;
    char    *d = dest;

    if (len == 0)
	return 0;

    while ((c = getc (file)) != EOF && len > 0) {
	*d++ = c;
	if (c == '\0')
	    return dest;
	len--;
    }
    return 0;
}

static FcBool
FcCacheWriteString (int fd, const char *chars)
{
    if (write (fd, chars, strlen(chars)+1) != strlen(chars)+1)
	return FcFalse;
    return FcTrue;
}

/* 
 * Find the next presumably-mmapable offset after the supplied file
 * position.
 */
static off_t
FcCacheNextOffset(off_t w)
{
    static long pagesize = -1;

    if (w == -1)
        return w;

    if (pagesize == -1)
#if defined (HAVE_SYSCONF)
	pagesize = sysconf(_SC_PAGESIZE);
#else
	pagesize = FC_HARDCODED_PAGESIZE;
#endif
    if (w % pagesize == 0) 
	return w;
    else
	return ((w / pagesize)+1)*pagesize;
}

/* Does not check that the cache has the appropriate arch section. */
FcBool
FcDirCacheValid (const FcChar8 *dir, FcConfig *config)
{
    FILE *file;

    file = FcDirCacheOpen (config, dir, NULL);

    if (file != NULL)
	return FcFalse;
    fclose (file);

    return FcTrue;
}

#define CACHEBASE_LEN (1 + 32 + 1 + sizeof (FC_ARCHITECTURE) + sizeof (FC_CACHE_SUFFIX))

static const char bin2hex[] = { '0', '1', '2', '3',
				'4', '5', '6', '7',
				'8', '9', 'a', 'b',
				'c', 'd', 'e', 'f' };

static FcChar8 *
FcDirCacheBasename (const FcChar8 * dir, FcChar8 cache_base[CACHEBASE_LEN])
{
    unsigned char 	hash[16];
    FcChar8		*hex_hash;
    int			cnt;
    struct MD5Context 	ctx;

    MD5Init (&ctx);
    MD5Update (&ctx, (unsigned char *)dir, strlen ((char *) dir));

    MD5Final (hash, &ctx);

    cache_base[0] = '/';
    hex_hash = cache_base + 1;
    for (cnt = 0; cnt < 16; ++cnt)
    {
	hex_hash[2*cnt  ] = bin2hex[hash[cnt] >> 4];
	hex_hash[2*cnt+1] = bin2hex[hash[cnt] & 0xf];
    }
    hex_hash[2*cnt] = 0;
    strcat ((char *) cache_base, "-" FC_ARCHITECTURE FC_CACHE_SUFFIX);

    return cache_base;
}

FcBool
FcDirCacheUnlink (const FcChar8 *dir, FcConfig *config)
{
    FcChar8	*cache_hashed = NULL;
    FcChar8	cache_base[CACHEBASE_LEN];
    FcStrList	*list;
    FcChar8	*cache_dir;

    FcDirCacheBasename (dir, cache_base);

    list = FcStrListCreate (config->cacheDirs);
    if (!list)
        return FcFalse;
	
    while ((cache_dir = FcStrListNext (list)))
    {
        cache_hashed = FcStrPlus (cache_dir, cache_base);
        if (!cache_hashed)
	    break;
	(void) unlink ((char *) cache_hashed);
    }
    FcStrListDone (list);
    /* return FcFalse if something went wrong */
    if (cache_dir)
	return FcFalse;
    return FcTrue;
}

static int
FcCacheReadDirs (FcConfig * config,
		 FcStrList *list, FcFontSet * set, FcStrSet *processed_dirs)
{
    int			ret = 0;
    FcChar8		*dir;
    FcStrSet		*subdirs;
    FcStrList		*sublist;

    /*
     * Read in the results from 'list'.
     */
    while ((dir = FcStrListNext (list)))
    {
	if (!FcConfigAcceptFilename (config, dir))
	    continue;

	/* Skip this directory if already updated
	 * to avoid the looped directories via symlinks
	 * Clearly a dir not in fonts.conf shouldn't be globally cached.
	 */

	if (FcStrSetMember (processed_dirs, dir))
	    continue;
	if (!FcStrSetAdd (processed_dirs, dir))
	    continue;

	subdirs = FcStrSetCreate ();
	if (!subdirs)
	{
	    fprintf (stderr, "Can't create directory set\n");
	    ret++;
	    continue;
	}
	
	FcDirScanConfig (set, subdirs,
			 config->blanks, dir, FcFalse, config);
	
	sublist = FcStrListCreate (subdirs);
	FcStrSetDestroy (subdirs);
	if (!sublist)
	{
	    fprintf (stderr, "Can't create subdir list in \"%s\"\n", dir);
	    ret++;
	    continue;
	}
	ret += FcCacheReadDirs (config, sublist, set, processed_dirs);
    }
    FcStrListDone (list);
    return ret;
}

FcFontSet *
FcCacheRead (FcConfig *config)
{
    FcFontSet 	*s = FcFontSetCreate();
    FcStrSet 	*processed_dirs;

    if (!s) 
	return 0;

    processed_dirs = FcStrSetCreate();
    if (!processed_dirs)
	goto bail;

    if (FcCacheReadDirs (config, FcConfigGetConfigDirs (config), s, processed_dirs))
	goto bail1;

    FcStrSetDestroy (processed_dirs);
    return s;

 bail1:
    FcStrSetDestroy (processed_dirs);
 bail:
    FcFontSetDestroy (s);
    return 0;
}

/* Opens the cache file for 'dir' for reading.
 * This searches the list of cache dirs for the relevant cache file,
 * returning the first one found.
 */
static FILE *
FcDirCacheOpen (FcConfig *config, const FcChar8 *dir, FcChar8 **cache_path)
{
    FILE	*file = NULL;
    FcChar8	*cache_hashed = NULL;
    FcChar8	cache_base[CACHEBASE_LEN];
    FcStrList	*list;
    FcChar8	*cache_dir;
    struct stat file_stat, dir_stat;

    if (stat ((char *) dir, &dir_stat) < 0)
        return NULL;

    FcDirCacheBasename (dir, cache_base);

    list = FcStrListCreate (config->cacheDirs);
    if (!list)
        return NULL;
	
    while ((cache_dir = FcStrListNext (list)))
    {
        cache_hashed = FcStrPlus (cache_dir, cache_base);
        if (!cache_hashed)
	    break;
        file = fopen((char *) cache_hashed, "rb");
        if (file != NULL) {
	    if (fstat (fileno (file), &file_stat) >= 0 &&
		dir_stat.st_mtime <= file_stat.st_mtime)
	    {
		break;
	    }
	    fclose (file);
	    file = NULL;
	}
	FcStrFree (cache_hashed);
	cache_hashed = NULL;
    }
    FcStrListDone (list);

    if (file == NULL)
	return NULL;
    
    if (cache_path)
	*cache_path = cache_hashed;
    else
	FcStrFree (cache_hashed);

    return file;
}

/* read serialized state from the cache file */
FcBool
FcDirCacheRead (FcFontSet * set, FcStrSet * dirs, 
		const FcChar8 *dir, FcConfig *config)
{
    FILE	*file;
    FcCache	metadata;
    void	*current_dir_block;
    char	subdir_name[FC_MAX_FILE_LEN + 1 + 12 + 1];

    file = FcDirCacheOpen (config, dir, NULL);
    if (file == NULL)
	goto bail;

    if (fread(&metadata, sizeof(FcCache), 1, file) != 1)
	goto bail1;
    if (metadata.magic != FC_CACHE_MAGIC)
        goto bail1;

    while (FcCacheReadString (file, subdir_name, sizeof (subdir_name)) && 
	   strlen (subdir_name) > 0)
        FcStrSetAdd (dirs, (FcChar8 *)subdir_name);

    if (metadata.count)
    {
	int	fd = fileno (file);
#if defined(HAVE_MMAP) || defined(__CYGWIN__)
	current_dir_block = mmap (0, metadata.count, 
				  PROT_READ, MAP_SHARED, fd, metadata.pos);
	if (current_dir_block == MAP_FAILED)
	    goto bail1;
#elif defined(_WIN32)
	{
	    HANDLE hFileMap;

	    hFileMap = CreateFileMapping((HANDLE) _get_osfhandle(fd), NULL, PAGE_READONLY, 0, 0, NULL);
	    if (hFileMap == NULL)
		goto bail1;

	    current_dir_block = MapViewOfFile (hFileMap, FILE_MAP_READ, 0, metadata.pos, metadata.count);
	    if (current_dir_block == NULL)
	    {
		CloseHandle (hFileMap);
		goto bail1;
	    }
	}
#else
	if (lseek (fd, metatdata.pos, SEEK_SET) == -1)
	    goto bail1;

	current_dir_block = malloc (metadata.count);
	if (!current_dir_block)
	    goto bail1;

	/* could also use CreateMappedViewOfFile under MinGW... */
	if (read (fd, current_dir_block, metadata.count) != metadata.count)
	{
	    free (current_dir_block);
	    goto bail1;
	}
#endif
	FcCacheAddBankDir (metadata.bank, (char *) dir);
	if (!FcFontSetUnserialize (&metadata, set, current_dir_block))
	    goto bail1;
    }
    if (config)
	FcConfigAddFontDir (config, (FcChar8 *)dir);

    fclose(file);
    return FcTrue;

 bail1:
    fclose (file);
 bail:
    return FcFalse;
}

static void *
FcDirCacheProduce (FcFontSet *set, FcCache *metadata)
{
    void * current_dir_block, * final_dir_block;
    static unsigned int rand_state = 0;
    int bank, needed_bytes_no_align;

#if defined (HAVE_RAND_R)
    if (!rand_state) 
	rand_state = time(0L);
    bank = rand_r(&rand_state);

    while (FcCacheHaveBank(bank))
	bank = rand_r(&rand_state);
#else
    if (!rand_state)
    {
        rand_state = 1;
        srand (time (0L));
    }
    bank = rand();

    while (FcCacheHaveBank(bank))
        bank = rand();
#endif

    memset (metadata, 0, sizeof(FcCache));
    FcFontSetNewBank();
    needed_bytes_no_align = FcFontSetNeededBytes (set);
    metadata->count = needed_bytes_no_align + 
	FcFontSetNeededBytesAlign ();
    metadata->magic = FC_CACHE_MAGIC;
    metadata->bank = bank;

    if (!needed_bytes_no_align) /* not a failure, no fonts to write */
    {
	/* no, you don't really need to write any bytes at all. */
	metadata->count = 0;
	return 0;
    }

    current_dir_block = malloc (metadata->count);
    if (!current_dir_block)
	goto bail;
    /* shut up valgrind */
    memset (current_dir_block, 0, metadata->count);
    final_dir_block = FcFontSetDistributeBytes (metadata, current_dir_block);

    if ((void *)((char *)current_dir_block+metadata->count) < final_dir_block)
	goto bail;
			      
    if (!FcFontSetSerialize (bank, set))
	goto bail;

    return current_dir_block;

 bail:
    free (current_dir_block);
    return 0;
}

static FcBool
FcMakeDirectory (const FcChar8 *dir)
{
    FcChar8 *parent;
    FcBool  ret;
    
    if (strlen ((char *) dir) == 0)
	return FcFalse;
    
    parent = FcStrDirname (dir);
    if (!parent)
	return FcFalse;
    if (access ((char *) parent, W_OK|X_OK) == 0)
	ret = mkdir ((char *) dir, 0777) == 0;
    else if (access ((char *) parent, F_OK) == -1)
	ret = FcMakeDirectory (parent) && (mkdir ((char *) dir, 0777) == 0);
    else
	ret = FcFalse;
    FcStrFree (parent);
    return ret;
}

/* write serialized state to the cache file */
FcBool
FcDirCacheWrite (FcFontSet *set, FcStrSet *dirs, const FcChar8 *dir, FcConfig *config)
{
    FcChar8	    cache_base[CACHEBASE_LEN];
    FcChar8	    *cache_hashed;
    int 	    fd, i;
    FcAtomic 	    *atomic;
    FcCache 	    metadata;
    void 	    *current_dir_block = 0;
    FcStrList	    *list;
    FcChar8	    *cache_dir = NULL;
    FcChar8	    *test_dir;
    off_t	    header_len;

    /*
     * Write it to the first directory in the list which is writable
     */
    
    list = FcStrListCreate (config->cacheDirs);
    if (!list)
	return FcFalse;
    while ((test_dir = FcStrListNext (list))) {
	if (access ((char *) test_dir, W_OK|X_OK) == 0)
	{
	    cache_dir = test_dir;
	    break;
	}
	else
	{
	    /*
	     * If the directory doesn't exist, try to create it
	     */
	    if (access ((char *) test_dir, F_OK) == -1) {
		if (FcMakeDirectory (test_dir))
		{
		    cache_dir = test_dir;
		    break;
		}
	    }
	}
    }
    FcStrListDone (list);
    if (!cache_dir)
	return FcFalse;
    FcDirCacheBasename (dir, cache_base);
    cache_hashed = FcStrPlus (cache_dir, cache_base);
    if (!cache_hashed)
        return FcFalse;

    current_dir_block = FcDirCacheProduce (set, &metadata);

    if (metadata.count && !current_dir_block)
	goto bail1;

    if (FcDebug () & FC_DBG_CACHE)
        printf ("FcDirCacheWriteDir dir \"%s\" file \"%s\"\n",
		dir, cache_hashed);

    atomic = FcAtomicCreate ((FcChar8 *)cache_hashed);
    if (!atomic)
	goto bail1;

    if (!FcAtomicLock (atomic))
	goto bail2;

    fd = open((char *)FcAtomicNewFile (atomic), O_RDWR | O_CREAT | O_BINARY, 0666);
    if (fd == -1)
	goto bail3;
    
    /*
     * Compute file header length -- the FcCache followed by the subdir names
     */
    header_len = sizeof (FcCache);
    for (i = 0; i < dirs->size; i++)
	header_len += strlen ((char *)dirs->strs[i]) + 1;
    
    metadata.pos = FcCacheNextOffset (lseek (fd, 0, SEEK_CUR) + header_len);
    metadata.subdirs = dirs->size;
    
    /*
     * Write out the header
     */
    if (write (fd, &metadata, sizeof(FcCache)) != sizeof(FcCache)) 
    {
	perror("write metadata");
	goto bail5;
    }
    
    for (i = 0; i < dirs->size; i++)
        FcCacheWriteString (fd, (char *)dirs->strs[i]);

    if (metadata.count)
    {
	if (lseek (fd, metadata.pos, SEEK_SET) != metadata.pos)
	    perror("lseek");
	else if (write (fd, current_dir_block, metadata.count) !=
		 metadata.count)
	    perror("write current_dir_block");
	free (current_dir_block);
        current_dir_block = 0;
    }

    close(fd);
    if (!FcAtomicReplaceOrig(atomic))
        goto bail3;
    FcStrFree ((FcChar8 *)cache_hashed);
    FcAtomicUnlock (atomic);
    FcAtomicDestroy (atomic);
    return FcTrue;

 bail5:
    close (fd);
 bail3:
    FcAtomicUnlock (atomic);
 bail2:
    FcAtomicDestroy (atomic);
 bail1:
    FcStrFree ((FcChar8 *)cache_hashed);
    if (current_dir_block)
        free (current_dir_block);
    return FcFalse;
}

static int banks_ptr = 0, banks_alloc = 0;
int * _fcBankId = 0, * _fcBankIdx = 0;
static const char ** bankDirs = 0;

static FcBool
FcCacheHaveBank (int bank)
{
    int i;

    if (bank < FC_BANK_FIRST)
	return FcTrue;

    for (i = 0; i < banks_ptr; i++)
	if (_fcBankId[i] == bank)
	    return FcTrue;

    return FcFalse;
}

int
FcCacheBankToIndexMTF (int bank)
{
    int i, j;

    for (i = 0; i < banks_ptr; i++)
	if (_fcBankId[_fcBankIdx[i]] == bank)
	{
	    int t = _fcBankIdx[i];

	    for (j = i; j > 0; j--)
		_fcBankIdx[j] = _fcBankIdx[j-1];
	    _fcBankIdx[0] = t;
	    return t;
	}

    if (banks_ptr >= banks_alloc)
    {
	int * b, * bidx;
	const char ** bds;

	b = realloc (_fcBankId, (banks_alloc + 4) * sizeof(int));
	if (!b)
	    return -1;
	_fcBankId = b;

	bidx = realloc (_fcBankIdx, (banks_alloc + 4) * sizeof(int));
	if (!bidx)
	    return -1;
	_fcBankIdx = bidx;

	bds = realloc (bankDirs, (banks_alloc + 4) * sizeof (char *));
	if (!bds)
	    return -1;
	bankDirs = bds;

	banks_alloc += 4;
    }

    i = banks_ptr++;
    _fcBankId[i] = bank;
    _fcBankIdx[i] = i;
    return i;
}

static void
FcCacheAddBankDir (int bank, const char * dir)
{
    int bi = FcCacheBankToIndexMTF (bank);

    if (bi < 0)
	return;

    bankDirs[bi] = (const char *)FcStrCopy ((FcChar8 *)dir);
}

const char *
FcCacheFindBankDir (int bank)
{
    int bi = FcCacheBankToIndex (bank);
    return bankDirs[bi];
}

/*
 * This code implements the MD5 message-digest algorithm.
 * The algorithm is due to Ron Rivest.	This code was
 * written by Colin Plumb in 1993, no copyright is claimed.
 * This code is in the public domain; do with it what you wish.
 *
 * Equivalent code is available from RSA Data Security, Inc.
 * This code has been tested against that, and is equivalent,
 * except that you don't need to include two pages of legalese
 * with every copy.
 *
 * To compute the message digest of a chunk of bytes, declare an
 * MD5Context structure, pass it to MD5Init, call MD5Update as
 * needed on buffers full of bytes, and then call MD5Final, which
 * will fill a supplied 16-byte array with the digest.
 */

#ifndef HIGHFIRST
#define byteReverse(buf, len)	/* Nothing */
#else
/*
 * Note: this code is harmless on little-endian machines.
 */
void byteReverse(unsigned char *buf, unsigned longs)
{
    FcChar32 t;
    do {
	t = (FcChar32) ((unsigned) buf[3] << 8 | buf[2]) << 16 |
	    ((unsigned) buf[1] << 8 | buf[0]);
	*(FcChar32 *) buf = t;
	buf += 4;
    } while (--longs);
}
#endif

/*
 * Start MD5 accumulation.  Set bit count to 0 and buffer to mysterious
 * initialization constants.
 */
static void MD5Init(struct MD5Context *ctx)
{
    ctx->buf[0] = 0x67452301;
    ctx->buf[1] = 0xefcdab89;
    ctx->buf[2] = 0x98badcfe;
    ctx->buf[3] = 0x10325476;

    ctx->bits[0] = 0;
    ctx->bits[1] = 0;
}

/*
 * Update context to reflect the concatenation of another buffer full
 * of bytes.
 */
static void MD5Update(struct MD5Context *ctx, unsigned char *buf, unsigned len)
{
    FcChar32 t;

    /* Update bitcount */

    t = ctx->bits[0];
    if ((ctx->bits[0] = t + ((FcChar32) len << 3)) < t)
	ctx->bits[1]++; 	/* Carry from low to high */
    ctx->bits[1] += len >> 29;

    t = (t >> 3) & 0x3f;	/* Bytes already in shsInfo->data */

    /* Handle any leading odd-sized chunks */

    if (t) {
	unsigned char *p = (unsigned char *) ctx->in + t;

	t = 64 - t;
	if (len < t) {
	    memcpy(p, buf, len);
	    return;
	}
	memcpy(p, buf, t);
	byteReverse(ctx->in, 16);
	MD5Transform(ctx->buf, (FcChar32 *) ctx->in);
	buf += t;
	len -= t;
    }
    /* Process data in 64-byte chunks */

    while (len >= 64) {
	memcpy(ctx->in, buf, 64);
	byteReverse(ctx->in, 16);
	MD5Transform(ctx->buf, (FcChar32 *) ctx->in);
	buf += 64;
	len -= 64;
    }

    /* Handle any remaining bytes of data. */

    memcpy(ctx->in, buf, len);
}

/*
 * Final wrapup - pad to 64-byte boundary with the bit pattern 
 * 1 0* (64-bit count of bits processed, MSB-first)
 */
static void MD5Final(unsigned char digest[16], struct MD5Context *ctx)
{
    unsigned count;
    unsigned char *p;

    /* Compute number of bytes mod 64 */
    count = (ctx->bits[0] >> 3) & 0x3F;

    /* Set the first char of padding to 0x80.  This is safe since there is
       always at least one byte free */
    p = ctx->in + count;
    *p++ = 0x80;

    /* Bytes of padding needed to make 64 bytes */
    count = 64 - 1 - count;

    /* Pad out to 56 mod 64 */
    if (count < 8) {
	/* Two lots of padding:  Pad the first block to 64 bytes */
	memset(p, 0, count);
	byteReverse(ctx->in, 16);
	MD5Transform(ctx->buf, (FcChar32 *) ctx->in);

	/* Now fill the next block with 56 bytes */
	memset(ctx->in, 0, 56);
    } else {
	/* Pad block to 56 bytes */
	memset(p, 0, count - 8);
    }
    byteReverse(ctx->in, 14);

    /* Append length in bits and transform */
    ((FcChar32 *) ctx->in)[14] = ctx->bits[0];
    ((FcChar32 *) ctx->in)[15] = ctx->bits[1];

    MD5Transform(ctx->buf, (FcChar32 *) ctx->in);
    byteReverse((unsigned char *) ctx->buf, 4);
    memcpy(digest, ctx->buf, 16);
    memset(ctx, 0, sizeof(ctx));        /* In case it's sensitive */
}


/* The four core functions - F1 is optimized somewhat */

/* #define F1(x, y, z) (x & y | ~x & z) */
#define F1(x, y, z) (z ^ (x & (y ^ z)))
#define F2(x, y, z) F1(z, x, y)
#define F3(x, y, z) (x ^ y ^ z)
#define F4(x, y, z) (y ^ (x | ~z))

/* This is the central step in the MD5 algorithm. */
#define MD5STEP(f, w, x, y, z, data, s) \
	( w += f(x, y, z) + data,  w = w<<s | w>>(32-s),  w += x )

/*
 * The core of the MD5 algorithm, this alters an existing MD5 hash to
 * reflect the addition of 16 longwords of new data.  MD5Update blocks
 * the data and converts bytes into longwords for this routine.
 */
static void MD5Transform(FcChar32 buf[4], FcChar32 in[16])
{
    register FcChar32 a, b, c, d;

    a = buf[0];
    b = buf[1];
    c = buf[2];
    d = buf[3];

    MD5STEP(F1, a, b, c, d, in[0] + 0xd76aa478, 7);
    MD5STEP(F1, d, a, b, c, in[1] + 0xe8c7b756, 12);
    MD5STEP(F1, c, d, a, b, in[2] + 0x242070db, 17);
    MD5STEP(F1, b, c, d, a, in[3] + 0xc1bdceee, 22);
    MD5STEP(F1, a, b, c, d, in[4] + 0xf57c0faf, 7);
    MD5STEP(F1, d, a, b, c, in[5] + 0x4787c62a, 12);
    MD5STEP(F1, c, d, a, b, in[6] + 0xa8304613, 17);
    MD5STEP(F1, b, c, d, a, in[7] + 0xfd469501, 22);
    MD5STEP(F1, a, b, c, d, in[8] + 0x698098d8, 7);
    MD5STEP(F1, d, a, b, c, in[9] + 0x8b44f7af, 12);
    MD5STEP(F1, c, d, a, b, in[10] + 0xffff5bb1, 17);
    MD5STEP(F1, b, c, d, a, in[11] + 0x895cd7be, 22);
    MD5STEP(F1, a, b, c, d, in[12] + 0x6b901122, 7);
    MD5STEP(F1, d, a, b, c, in[13] + 0xfd987193, 12);
    MD5STEP(F1, c, d, a, b, in[14] + 0xa679438e, 17);
    MD5STEP(F1, b, c, d, a, in[15] + 0x49b40821, 22);

    MD5STEP(F2, a, b, c, d, in[1] + 0xf61e2562, 5);
    MD5STEP(F2, d, a, b, c, in[6] + 0xc040b340, 9);
    MD5STEP(F2, c, d, a, b, in[11] + 0x265e5a51, 14);
    MD5STEP(F2, b, c, d, a, in[0] + 0xe9b6c7aa, 20);
    MD5STEP(F2, a, b, c, d, in[5] + 0xd62f105d, 5);
    MD5STEP(F2, d, a, b, c, in[10] + 0x02441453, 9);
    MD5STEP(F2, c, d, a, b, in[15] + 0xd8a1e681, 14);
    MD5STEP(F2, b, c, d, a, in[4] + 0xe7d3fbc8, 20);
    MD5STEP(F2, a, b, c, d, in[9] + 0x21e1cde6, 5);
    MD5STEP(F2, d, a, b, c, in[14] + 0xc33707d6, 9);
    MD5STEP(F2, c, d, a, b, in[3] + 0xf4d50d87, 14);
    MD5STEP(F2, b, c, d, a, in[8] + 0x455a14ed, 20);
    MD5STEP(F2, a, b, c, d, in[13] + 0xa9e3e905, 5);
    MD5STEP(F2, d, a, b, c, in[2] + 0xfcefa3f8, 9);
    MD5STEP(F2, c, d, a, b, in[7] + 0x676f02d9, 14);
    MD5STEP(F2, b, c, d, a, in[12] + 0x8d2a4c8a, 20);

    MD5STEP(F3, a, b, c, d, in[5] + 0xfffa3942, 4);
    MD5STEP(F3, d, a, b, c, in[8] + 0x8771f681, 11);
    MD5STEP(F3, c, d, a, b, in[11] + 0x6d9d6122, 16);
    MD5STEP(F3, b, c, d, a, in[14] + 0xfde5380c, 23);
    MD5STEP(F3, a, b, c, d, in[1] + 0xa4beea44, 4);
    MD5STEP(F3, d, a, b, c, in[4] + 0x4bdecfa9, 11);
    MD5STEP(F3, c, d, a, b, in[7] + 0xf6bb4b60, 16);
    MD5STEP(F3, b, c, d, a, in[10] + 0xbebfbc70, 23);
    MD5STEP(F3, a, b, c, d, in[13] + 0x289b7ec6, 4);
    MD5STEP(F3, d, a, b, c, in[0] + 0xeaa127fa, 11);
    MD5STEP(F3, c, d, a, b, in[3] + 0xd4ef3085, 16);
    MD5STEP(F3, b, c, d, a, in[6] + 0x04881d05, 23);
    MD5STEP(F3, a, b, c, d, in[9] + 0xd9d4d039, 4);
    MD5STEP(F3, d, a, b, c, in[12] + 0xe6db99e5, 11);
    MD5STEP(F3, c, d, a, b, in[15] + 0x1fa27cf8, 16);
    MD5STEP(F3, b, c, d, a, in[2] + 0xc4ac5665, 23);

    MD5STEP(F4, a, b, c, d, in[0] + 0xf4292244, 6);
    MD5STEP(F4, d, a, b, c, in[7] + 0x432aff97, 10);
    MD5STEP(F4, c, d, a, b, in[14] + 0xab9423a7, 15);
    MD5STEP(F4, b, c, d, a, in[5] + 0xfc93a039, 21);
    MD5STEP(F4, a, b, c, d, in[12] + 0x655b59c3, 6);
    MD5STEP(F4, d, a, b, c, in[3] + 0x8f0ccc92, 10);
    MD5STEP(F4, c, d, a, b, in[10] + 0xffeff47d, 15);
    MD5STEP(F4, b, c, d, a, in[1] + 0x85845dd1, 21);
    MD5STEP(F4, a, b, c, d, in[8] + 0x6fa87e4f, 6);
    MD5STEP(F4, d, a, b, c, in[15] + 0xfe2ce6e0, 10);
    MD5STEP(F4, c, d, a, b, in[6] + 0xa3014314, 15);
    MD5STEP(F4, b, c, d, a, in[13] + 0x4e0811a1, 21);
    MD5STEP(F4, a, b, c, d, in[4] + 0xf7537e82, 6);
    MD5STEP(F4, d, a, b, c, in[11] + 0xbd3af235, 10);
    MD5STEP(F4, c, d, a, b, in[2] + 0x2ad7d2bb, 15);
    MD5STEP(F4, b, c, d, a, in[9] + 0xeb86d391, 21);

    buf[0] += a;
    buf[1] += b;
    buf[2] += c;
    buf[3] += d;
}
