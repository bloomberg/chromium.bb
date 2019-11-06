/* gdbm.h  -  The include file for dbm users.  -*- c -*- */

/*  This file is part of GDBM, the GNU data base manager, by Philip A. Nelson.
    Copyright (C) 1990-1991, 1993, 2011, 2016-2018 Free Software
    Foundation, Inc.

    GDBM is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2, or (at your option)
    any later version.

    GDBM is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with GDBM. If not, see <http://www.gnu.org/licenses/>.  

    You may contact the author by:
       e-mail:  phil@cs.wwu.edu
      us-mail:  Philip A. Nelson
                Computer Science Department
                Western Washington University
                Bellingham, WA 98226
       
*************************************************************************/

/* Protection for multiple includes. */
#ifndef _GDBM_H_
# define _GDBM_H_

# include <stdio.h>

/* GDBM C++ support */
# if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
# endif

/* Parameters to gdbm_open for READERS, WRITERS, and WRITERS who
   can create the database. */
# define GDBM_READER	0	/* A reader. */
# define GDBM_WRITER	1	/* A writer. */
# define GDBM_WRCREAT	2	/* A writer.  Create the db if needed. */
# define GDBM_NEWDB	3	/* A writer.  Always create a new db. */
# define GDBM_OPENMASK	7	/* Mask for the above. */

# define GDBM_FAST	0x010	/* Write fast! => No fsyncs.  OBSOLETE. */
# define GDBM_SYNC	0x020	/* Sync operations to the disk. */
# define GDBM_NOLOCK	0x040	/* Don't do file locking operations. */
# define GDBM_NOMMAP	0x080	/* Don't use mmap(). */
# define GDBM_CLOEXEC   0x100   /* Close the underlying fd on exec(3) */
# define GDBM_BSEXACT   0x200   /* Don't adjust block_size. Bail out with
				   GDBM_BLOCK_SIZE_ERROR error if unable to
				   set it. */  
# define GDBM_CLOERROR  0x400   /* Only for gdbm_fd_open: close fd on error. */
  
/* Parameters to gdbm_store for simple insertion or replacement in the
   case that the key is already in the database. */
# define GDBM_INSERT	0	/* Never replace old data with new. */
# define GDBM_REPLACE	1	/* Always replace old data with new. */

/* Parameters to gdbm_setopt, specifing the type of operation to perform. */
# define GDBM_SETCACHESIZE    1  /* Set the cache size. */
# define GDBM_FASTMODE	      2	/* Toggle fast mode.  OBSOLETE. */
# define GDBM_SETSYNCMODE     3  /* Turn on or off sync operations. */
# define GDBM_SETCENTFREE     4  /* Keep all free blocks in the header. */
# define GDBM_SETCOALESCEBLKS 5  /* Attempt to coalesce free blocks. */
# define GDBM_SETMAXMAPSIZE   6  /* Set maximum mapped memory size */
# define GDBM_SETMMAP         7  /* Toggle mmap mode */

/* Compatibility defines: */
# define GDBM_CACHESIZE	     GDBM_SETCACHESIZE
# define GDBM_SYNCMODE	     GDBM_SETSYNCMODE
# define GDBM_CENTFREE       GDBM_SETCENTFREE
# define GDBM_COALESCEBLKS   GDBM_SETCOALESCEBLKS

# define GDBM_GETFLAGS        8  /* Get gdbm_open flags */
# define GDBM_GETMMAP         9  /* Get mmap status */
# define GDBM_GETCACHESIZE    10 /* Get current cache side */
# define GDBM_GETSYNCMODE     11 /* Get synch mode */
# define GDBM_GETCENTFREE     12 /* Get "centfree" status */
# define GDBM_GETCOALESCEBLKS 13 /* Get free block coalesce status */
# define GDBM_GETMAXMAPSIZE   14 /* Get maximum mapped memory size */
# define GDBM_GETDBNAME       15 /* Return database file name */
# define GDBM_GETBLOCKSIZE    16 /* Return block size */

typedef unsigned long long int gdbm_count_t;
  
/* The data and key structure. */
typedef struct
{
  char *dptr;
  int   dsize;
} datum;


/* A pointer to the GDBM file. */
typedef struct gdbm_file_info *GDBM_FILE;

/* External variable, the gdbm build release string. */
extern const char *gdbm_version;	

# define GDBM_VERSION_MAJOR 1
# define GDBM_VERSION_MINOR 18
# define GDBM_VERSION_PATCH 0

extern int const gdbm_version_number[3];

/* GDBM external functions. */

extern GDBM_FILE gdbm_fd_open (int fd, const char *file_name, int block_size,
			       int flags, void (*fatal_func) (const char *));
extern GDBM_FILE gdbm_open (const char *, int, int, int,
			    void (*)(const char *));
extern int gdbm_close (GDBM_FILE);
extern int gdbm_store (GDBM_FILE, datum, datum, int);
extern datum gdbm_fetch (GDBM_FILE, datum);
extern int gdbm_delete (GDBM_FILE, datum);
extern datum gdbm_firstkey (GDBM_FILE);
extern datum gdbm_nextkey (GDBM_FILE, datum);
extern int gdbm_reorganize (GDBM_FILE);
  
extern int gdbm_sync (GDBM_FILE);
extern int gdbm_exists (GDBM_FILE, datum);
extern int gdbm_setopt (GDBM_FILE, int, void *, int);
extern int gdbm_fdesc (GDBM_FILE);
  
extern int gdbm_export (GDBM_FILE, const char *, int, int);
extern int gdbm_export_to_file (GDBM_FILE dbf, FILE *fp);
  
extern int gdbm_import (GDBM_FILE, const char *, int);
extern int gdbm_import_from_file (GDBM_FILE dbf, FILE *fp, int flag);

extern int gdbm_count (GDBM_FILE dbf, gdbm_count_t *pcount);

typedef struct gdbm_recovery_s
{
  /* Input members.
     These are initialized before call to gdbm_recover.  The flags argument
     specifies which of them are initialized. */
  void (*errfun) (void *data, char const *fmt, ...);
  void *data;

  size_t max_failed_keys;
  size_t max_failed_buckets;
  size_t max_failures;

  /* Output members.
     The gdbm_recover function fills these before returning. */
  size_t recovered_keys;
  size_t recovered_buckets;
  size_t failed_keys;
  size_t failed_buckets;
  size_t duplicate_keys;
  char *backup_name;
} gdbm_recovery;

#define GDBM_RCVR_DEFAULT              0x00  /* Default settings */
#define GDBM_RCVR_ERRFUN               0x01  /* errfun is initialized */
#define GDBM_RCVR_MAX_FAILED_KEYS      0x02  /* max_failed_keys is initialized */
#define GDBM_RCVR_MAX_FAILED_BUCKETS   0x04  /* max_failed_buckets is initialized */
#define GDBM_RCVR_MAX_FAILURES         0x08  /* max_failures is initialized */
#define GDBM_RCVR_BACKUP               0x10  /* Keep backup copy of the
						original database on success */
#define GDBM_RCVR_FORCE                0x20  /* Force recovery by skipping the
						check pass */
					       
extern int gdbm_recover (GDBM_FILE dbf, gdbm_recovery *rcvr, int flags);
  
  
#define GDBM_DUMP_FMT_BINARY 0
#define GDBM_DUMP_FMT_ASCII  1

#define GDBM_META_MASK_MODE    0x01
#define GDBM_META_MASK_OWNER   0x02
  
extern int gdbm_dump (GDBM_FILE, const char *, int fmt, int open_flags,
		      int mode);
extern int gdbm_dump_to_file (GDBM_FILE, FILE *, int fmt);

extern int gdbm_load (GDBM_FILE *, const char *, int replace,
		      int meta_flags,
		      unsigned long *line);
extern int gdbm_load_from_file (GDBM_FILE *, FILE *, int replace,
				int meta_flags,
				unsigned long *line);

extern int gdbm_copy_meta (GDBM_FILE dst, GDBM_FILE src);

# define GDBM_NO_ERROR		         0
# define GDBM_MALLOC_ERROR	         1
# define GDBM_BLOCK_SIZE_ERROR	         2
# define GDBM_FILE_OPEN_ERROR	         3
# define GDBM_FILE_WRITE_ERROR	         4
# define GDBM_FILE_SEEK_ERROR	         5
# define GDBM_FILE_READ_ERROR	         6
# define GDBM_BAD_MAGIC_NUMBER	         7
# define GDBM_EMPTY_DATABASE	         8
# define GDBM_CANT_BE_READER	         9
# define GDBM_CANT_BE_WRITER	        10
# define GDBM_READER_CANT_DELETE	11
# define GDBM_READER_CANT_STORE	        12
# define GDBM_READER_CANT_REORGANIZE	13
# define GDBM_UNKNOWN_ERROR	        14
# define GDBM_ITEM_NOT_FOUND	        15
# define GDBM_REORGANIZE_FAILED	        16
# define GDBM_CANNOT_REPLACE	        17
# define GDBM_ILLEGAL_DATA	        18
# define GDBM_OPT_ALREADY_SET	        19
# define GDBM_OPT_ILLEGAL           	20
# define GDBM_BYTE_SWAPPED	        21
# define GDBM_BAD_FILE_OFFSET	        22
# define GDBM_BAD_OPEN_FLAGS	        23
# define GDBM_FILE_STAT_ERROR           24
# define GDBM_FILE_EOF                  25
# define GDBM_NO_DBNAME                 26
# define GDBM_ERR_FILE_OWNER            27
# define GDBM_ERR_FILE_MODE             28
# define GDBM_NEED_RECOVERY             29
# define GDBM_BACKUP_FAILED             30
# define GDBM_DIR_OVERFLOW              31
# define GDBM_BAD_BUCKET                32
# define GDBM_BAD_HEADER                33
# define GDBM_BAD_AVAIL                 34
# define GDBM_BAD_HASH_TABLE            35
# define GDBM_BAD_DIR_ENTRY             36
# define GDBM_FILE_CLOSE_ERROR          37  
# define GDBM_FILE_SYNC_ERROR           38
  
# define _GDBM_MIN_ERRNO	0
# define _GDBM_MAX_ERRNO	GDBM_FILE_SYNC_ERROR

/* This one was never used and will be removed in the future */
# define GDBM_UNKNOWN_UPDATE GDBM_UNKNOWN_ERROR
  
typedef int gdbm_error;
extern int *gdbm_errno_location (void);
#define gdbm_errno (*gdbm_errno_location ())
extern const char * const gdbm_errlist[];
extern int const gdbm_syserr[];
  
extern gdbm_error gdbm_last_errno (GDBM_FILE dbf);
extern int gdbm_last_syserr (GDBM_FILE dbf);
extern void gdbm_set_errno (GDBM_FILE dbf, gdbm_error ec, int fatal);
extern void gdbm_clear_error (GDBM_FILE dbf);
extern int gdbm_needs_recovery (GDBM_FILE dbf);
extern int gdbm_check_syserr (gdbm_error n);

/* extra prototypes */

extern const char *gdbm_strerror (gdbm_error);
extern const char *gdbm_db_strerror (GDBM_FILE dbf);
  
extern int gdbm_version_cmp (int const a[], int const b[]);

#if 0
# define GDBM_DEBUG_ENABLE 1

typedef void (*gdbm_debug_printer_t) (char const *, ...);
extern gdbm_debug_printer_t gdbm_debug_printer;
extern int gdbm_debug_flags;

# define GDBM_DEBUG_ERR    0x00000001
# define GDBM_DEBUG_OPEN   0x00000002
# define GDBM_DEBUG_READ   0x00000004
# define GDBM_DEBUG_STORE  0x00000008
# define GDBM_DEBUG_LOOKUP 0x00000010
  
# define GDBM_DEBUG_ALL    0xffffffff

extern int gdbm_debug_token (char const *tok);
extern void gdbm_debug_parse_state (int (*f) (void *, int, char const *),
				    void *d);
  
extern void gdbm_debug_datum (datum dat, char const *pfx);
  
#endif
  
# if defined(__cplusplus) || defined(c_plusplus)
}
# endif

#endif
