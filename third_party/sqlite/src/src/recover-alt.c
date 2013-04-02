/*
** 2012 Jan 11
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
*/
/* TODO(shess): THIS MODULE IS STILL EXPERIMENTAL.  DO NOT USE IT. */
/* Implements a virtual table "recover" which can be used to recover
 * data from a corrupt table.  The table is walked manually, with
 * corrupt items skipped.  Additionally, any errors while reading will
 * be skipped.
 *
 * Given a table with this definition:
 *
 * CREATE TABLE Stuff (
 *   name TEXT PRIMARY KEY,
 *   value TEXT NOT NULL
 * );
 *
 * to recover the data from teh table, you could do something like:
 *
 * -- Attach another database, the original is not trustworthy.
 * ATTACH DATABASE '/tmp/db.db' AS rdb;
 * -- Create a new version of the table.
 * CREATE TABLE rdb.Stuff (
 *   name TEXT PRIMARY KEY,
 *   value TEXT NOT NULL
 * );
 * -- This will read the original table's data.
 * CREATE VIRTUAL TABLE temp.recover_Stuff using recover(
 *   main.Stuff,
 *   name TEXT STRICT NOT NULL,  -- only real TEXT data allowed
 *   value TEXT STRICT NOT NULL
 * );
 * -- Corruption means the UNIQUE constraint may no longer hold for
 * -- Stuff, so either OR REPLACE or OR IGNORE must be used.
 * INSERT OR REPLACE INTO rdb.Stuff (rowid, name, value )
 *   SELECT rowid, name, value FROM temp.recover_Stuff;
 * DROP TABLE temp.recover_Stuff;
 * DETACH DATABASE rdb;
 * -- Move db.db to replace original db in filesystem.
 *
 *
 * Usage
 *
 * Given the goal of dealing with corruption, it would not be safe to
 * create a recovery table in the database being recovered.  So
 * recovery tables must be created in the temp database.  They are not
 * appropriate to persist, in any case.  [As a bonus, sqlite_master
 * tables can be recovered.  Perhaps more cute than useful, though.]
 *
 * The parameters are a specifier for the table to read, and a column
 * definition for each bit of data stored in that table.  The named
 * table must be convertable to a root page number by reading the
 * sqlite_master table.  Bare table names are assumed to be in
 * database 0 ("main"), other databases can be specified in db.table
 * fashion.
 *
 * Column definitions are similar to BUT NOT THE SAME AS those
 * provided to CREATE statements:
 *  column-def: column-name [type-name [STRICT] [NOT NULL]]
 *  type-name: (ANY|ROWID|INTEGER|FLOAT|NUMERIC|TEXT|BLOB)
 *
 * Only those exact type names are accepted, there is no type
 * intuition.  The only constraints accepted are STRICT (see below)
 * and NOT NULL.  Anything unexpected will cause the create to fail.
 *
 * ANY is a convenience to indicate that manifest typing is desired.
 * It is equivalent to not specifying a type at all.  The results for
 * such columns will have the type of the data's storage.  The exposed
 * schema will contain no type for that column.
 *
 * ROWID is used for columns representing aliases to the rowid
 * (INTEGER PRIMARY KEY, with or without AUTOINCREMENT), to make the
 * concept explicit.  Such columns are actually stored as NULL, so
 * they cannot be simply ignored.  The exposed schema will be INTEGER
 * for that column.
 *
 * NOT NULL causes rows with a NULL in that column to be skipped.  It
 * also adds NOT NULL to the column in the exposed schema.  If the
 * table has ever had columns added using ALTER TABLE, then those
 * columns implicitly contain NULL for rows which have not been
 * updated.  [Workaround using COALESCE() in your SELECT statement.]
 *
 * The created table is read-only, with no indices.  Any SELECT will
 * be a full-table scan, returning each valid row read from the
 * storage of the backing table.  The rowid will be the rowid of the
 * row from the backing table.  "Valid" means:
 * - The cell metadata for the row is well-formed.  Mainly this means that
 *   the cell header info describes a payload of the size indicated by
 *   the cell's payload size.
 * - The cell does not run off the page.
 * - The cell does not overlap any other cell on the page.
 * - The cell contains doesn't contain too many columns.
 * - The types of the serialized data match the indicated types (see below).
 *
 *
 * Type affinity versus type storage.
 *
 * http://www.sqlite.org/datatype3.html describes SQLite's type
 * affinity system.  The system provides for automated coercion of
 * types in certain cases, transparently enough that many developers
 * do not realize that it is happening.  Importantly, it implies that
 * the raw data stored in the database may not have the obvious type.
 *
 * Differences between the stored data types and the expected data
 * types may be a signal of corruption.  This module makes some
 * allowances for automatic coercion.  It is important to be concious
 * of the difference between the schema exposed by the module, and the
 * data types read from storage.  The following table describes how
 * the module interprets things:
 *
 * type     schema   data                     STRICT
 * ----     ------   ----                     ------
 * ANY      <none>   any                      any
 * ROWID    INTEGER  n/a                      n/a
 * INTEGER  INTEGER  integer                  integer
 * FLOAT    FLOAT    integer or float         float
 * NUMERIC  NUMERIC  integer, float, or text  integer or float
 * TEXT     TEXT     text or blob             text
 * BLOB     BLOB     blob                     blob
 *
 * type is the type provided to the recover module, schema is the
 * schema exposed by the module, data is the acceptable types of data
 * decoded from storage, and STRICT is a modification of that.
 *
 * A very loose recovery system might use ANY for all columns, then
 * use the appropriate sqlite3_column_*() calls to coerce to expected
 * types.  This doesn't provide much protection if a page from a
 * different table with the same column count is linked into an
 * inappropriate btree.
 *
 * A very tight recovery system might use STRICT to enforce typing on
 * all columns, preferring to skip rows which are valid at the storage
 * level but don't contain the right types.  Note that FLOAT STRICT is
 * almost certainly not appropriate, since integral values are
 * transparently stored as integers, when that is more efficient.
 *
 * Another option is to use ANY for all columns and inspect each
 * result manually (using sqlite3_column_*).  This should only be
 * necessary in cases where developers have used manifest typing (test
 * to make sure before you decide that you aren't using manifest
 * typing!).
 *
 *
 * Caveats
 *
 * Leaf pages not referenced by interior nodes will not be found.
 *
 * Leaf pages referenced from interior nodes of other tables will not
 * be resolved.
 *
 * Rows referencing invalid overflow pages will be skipped.
 *
 * SQlite rows have a header which describes how to interpret the rest
 * of the payload.  The header can be valid in cases where the rest of
 * the record is actually corrupt (in the sense that the data is not
 * the intended data).  This can especially happen WRT overflow pages,
 * as lack of atomic updates between pages is the primary form of
 * corruption I have seen in the wild.
 */
/* The implementation is via a series of cursors.  The cursor
 * implementations follow the pattern:
 *
 * // Creates the cursor using various initialization info.
 * int cursorCreate(...);
 *
 * // Returns 1 if there is no more data, 0 otherwise.
 * int cursorEOF(Cursor *pCursor);
 *
 * // Various accessors can be used if not at EOF.
 *
 * // Move to the next item.
 * int cursorNext(Cursor *pCursor);
 *
 * // Destroy the memory associated with the cursor.
 * void cursorDestroy(Cursor *pCursor);
 *
 * References in the following are to sections at
 * http://www.sqlite.org/fileformat2.html .
 *
 * RecoverLeafCursor iterates the records in a leaf table node
 * described in section 1.5 "B-tree Pages".  When the node is
 * exhausted, an interior cursor is used to get the next leaf node,
 * and iteration continues there.
 *
 * RecoverInteriorCursor iterates the child pages in an interior table
 * node described in section 1.5 "B-tree Pages".  When the node is
 * exhausted, a parent interior cursor is used to get the next
 * interior node at the same level, and iteration continues there.
 *
 * Together these record the path from the leaf level to the root of
 * the tree.  Iteration happens from the leaves rather than the root
 * both for efficiency and putting the special case at the front of
 * the list is easier to implement.
 *
 * RecoverCursor uses a RecoverLeafCursor to iterate the rows of a
 * table, returning results via the SQLite virtual table interface.
 */
/* TODO(shess): It might be useful to allow DEFAULT in types to
 * specify what to do for NULL when an ALTER TABLE case comes up.
 * Unfortunately, simply adding it to the exposed schema and using
 * sqlite3_result_null() does not cause the default to be generate.
 * Handling it ourselves seems hard, unfortunately.
 */
/* TODO(shess): Account for the reserved region on pages (value from
 * offset 20 in the database header).
 */

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

/* Internal SQLite things that are used:
 * u32, u64, i64 types.
 * Btree, Pager, and DbPage structs.
 * DbPage.pData, .pPager, and .pgno
 * sqlite3 struct.
 * sqlite3BtreePager() and sqlite3BtreeGetPageSize()
 * sqlite3PagerAcquire() and sqlite3PagerUnref()
 * getVarint().
 */
#include "sqliteInt.h"

/* For debugging. */
#if 0
#define FNENTRY() fprintf(stderr, "In %s\n", __FUNCTION__)
#else
#define FNENTRY()
#endif

/* Generic constants and helper functions. */

static const unsigned char kTableLeafPage = 0x0D;
static const unsigned char kTableInteriorPage = 0x05;

/* From section 1.5. */
static const unsigned kiPageTypeOffset = 0;
static const unsigned kiPageFreeBlockOffset = 1;
static const unsigned kiPageCellCountOffset = 3;
static const unsigned kiPageCellContentOffset = 5;
static const unsigned kiPageFragmentedBytesOffset = 7;
static const unsigned knPageLeafHeaderBytes = 8;
/* Interior pages contain an additional field. */
static const unsigned kiPageRightChildOffset = 8;
static const unsigned kiPageInteriorHeaderBytes = 12;

/* Accepted types are specified by a mask. */
#define MASK_ROWID (1<<0)
#define MASK_INTEGER (1<<1)
#define MASK_FLOAT (1<<2)
#define MASK_TEXT (1<<3)
#define MASK_BLOB (1<<4)
#define MASK_NULL (1<<5)

/* Helpers to decode fixed-size fields. */
static u32 decodeUnsigned16(const unsigned char *pData){
  return (pData[0]<<8) + pData[1];
}
static u32 decodeUnsigned32(const unsigned char *pData){
  return (decodeUnsigned16(pData)<<16) + decodeUnsigned16(pData+2);
}
static i64 decodeSigned(const unsigned char *pData, unsigned nBytes){
  i64 r = (char)(*pData);
  while( --nBytes ){
    r <<= 8;
    r += *(++pData);
  }
  return r;
}
/* Derived from vdbeaux.c, sqlite3VdbeSerialGet(), case 7. */
/* TODO(shess): Determine if swapMixedEndianFloat() applies. */
static double decodeFloat64(const unsigned char *pData){
#if !defined(NDEBUG)
  static const u64 t1 = ((u64)0x3ff00000)<<32;
  static const double r1 = 1.0;
  u64 t2 = t1;
  assert( sizeof(r1)==sizeof(t2) && memcmp(&r1, &t2, sizeof(r1))==0 );
#endif
  i64 x = decodeSigned(pData, 8);
  double d;
  memcpy(&d, &x, sizeof(x));
  return d;
}

/* Return true if a varint can safely be read from pData/nData. */
/* TODO(shess): DbPage points into the middle of a buffer which
 * contains the page data before DbPage.  So code should always be
 * able to read a small number of varints safely.  Consider whether to
 * trust that or not.
 */
static int checkVarint(const unsigned char *pData, unsigned nData){
  /* In the worst case the decoder takes all 8 bits of the 9th byte. */
  if( nData>=9 ){
    return 1;
  }

  /* Look for a high-bit-clear byte in what's left. */
  unsigned i;
  for( i=0; i<nData; ++i ){
    if( !(pData[i]&0x80) ){
      return 1;
    }
  }

  /* Cannot decode in the space given. */
  return 0;
}

/* Return 1 if n varints can be read from pData/nData. */
static int checkVarints(const unsigned char *pData, unsigned nData,
                        unsigned n){
  /* In the worst case the decoder takes all 8 bits of the 9th byte. */
  if( nData>=9*n ){
    return 1;
  }

  unsigned nCur = 0, nFound = 0;
  unsigned i;
  for( i=0; nFound<n && i<nData; ++i ){
    nCur++;
    if( nCur==9 || !(pData[i]&0x80) ){
      nFound++;
      nCur = 0;
    }
  }

  return nFound==n;
}

/* For some reason I kept making mistakes with offset calculations. */
static const unsigned char *PageData(DbPage *pPage, unsigned iOffset){
  assert( iOffset<=pPage->nPageSize );
  return pPage->pData + iOffset;
}

/* The first page in the file contains a file header in the first 100
 * bytes.  The page's header information comes after that.  Note that
 * the offsets in the page's header information are relative to the
 * beginning of the page, NOT the end of the page header.
 */
static const unsigned char *PageHeader(DbPage *pPage){
  if( pPage->pgno==1 ){
    const unsigned nDatabaseHeader = 100;
    return PageData(pPage, nDatabaseHeader);
  }else{
    return PageData(pPage, 0);
  }
}

/* Helper to fetch the pager and page size for the named database. */
static int GetPager(sqlite3 *db, const char *zName,
                    Pager **pPager, unsigned *pnPageSize){
  Btree *pBt = NULL;
  int i;
  for( i=0; i<db->nDb; ++i ){
    if( strcasecmp(db->aDb[i].zName, zName)==0 ){
      pBt = db->aDb[i].pBt;
      break;
    }
  }
  if( !pBt ){
    return SQLITE_ERROR;
  }

  *pPager = sqlite3BtreePager(pBt);
  *pnPageSize = sqlite3BtreeGetPageSize(pBt);
  return SQLITE_OK;
}

/* iSerialType is a type read from a record header.  See "2.1 Record Format".
 */

/* Storage size of iSerialType in bytes.  My interpretation of SQLite
 * documentation is that text and blob fields can have 32-bit length.
 * Values past 2^31-12 will need more than 32 bits to encode, which is
 * why iSerialType is u64.
 */
static u32 SerialTypeLength(u64 iSerialType){
  switch( iSerialType ){
    case 0 : return 0;  /* NULL */
    case 1 : return 1;  /* Various integers. */
    case 2 : return 2;
    case 3 : return 3;
    case 4 : return 4;
    case 5 : return 6;
    case 6 : return 8;
    case 7 : return 8;  /* 64-bit float. */
    case 8 : return 0;  /* Constant 0. */
    case 9 : return 0;  /* Constant 1. */
    case 10 : case 11 : assert( !"RESERVED TYPE"); return 0;
  }
  return (u32)((iSerialType>>1) - 6);
}

/* True if iSerialType refers to a blob. */
static int SerialTypeIsBlob(u64 iSerialType){
  assert( iSerialType>=12 );
  return (iSerialType%2)==0;
}

/* Returns true if the serialized type represented by iSerialType is
 * compatible with the given type mask.
 */
static int SerialTypeIsCompatible(u64 iSerialType, unsigned char mask){
  switch( iSerialType ){
    case 0  : return (mask&MASK_NULL)!=0;
    case 1  : return (mask&MASK_INTEGER)!=0;
    case 2  : return (mask&MASK_INTEGER)!=0;
    case 3  : return (mask&MASK_INTEGER)!=0;
    case 4  : return (mask&MASK_INTEGER)!=0;
    case 5  : return (mask&MASK_INTEGER)!=0;
    case 6  : return (mask&MASK_INTEGER)!=0;
    case 7  : return (mask&MASK_FLOAT)!=0;
    case 8  : return (mask&MASK_INTEGER)!=0;
    case 9  : return (mask&MASK_INTEGER)!=0;
    case 10 : assert( !"RESERVED TYPE"); return 0;
    case 11 : assert( !"RESERVED TYPE"); return 0;
  }
  return (mask&(SerialTypeIsBlob(iSerialType) ? MASK_BLOB : MASK_TEXT));
}

/* Versions of strdup() with return values appropriate for
 * sqlite3_free().  malloc.c has sqlite3DbStrDup()/NDup(), but those
 * need sqlite3DbFree(), which seems intrusive.
 */
static char *sqlite3_strndup(const char *z, unsigned n){
  if( z==NULL ){
    return NULL;
  }

  char *zNew = sqlite3_malloc(n+1);
  if( zNew!=NULL ){
    memcpy(zNew, z, n);
    zNew[n] = '\0';
  }
  return zNew;
}
static char *sqlite3_strdup(const char *z){
  if( z==NULL ){
    return NULL;
  }
  return sqlite3_strndup(z, strlen(z));
}

/* Fetch the page number of zTable in zDb from sqlite_master in zDb,
 * and put it in *piRootPage.
 */
static int getRootPage(sqlite3 *db, const char *zDb, const char *zTable,
                       u32 *piRootPage){
  if( strcmp(zTable, "sqlite_master")==0 ){
    *piRootPage = 1;
    return SQLITE_OK;
  }

  char *zSql = sqlite3_mprintf("SELECT rootpage FROM %s.sqlite_master "
                               "WHERE type = 'table' AND tbl_name = %Q",
                               zDb, zTable);
  if( !zSql ){
    return SQLITE_NOMEM;
  }

  sqlite3_stmt *pStmt = 0;
  int rc = sqlite3_prepare_v2(db, zSql, -1, &pStmt, 0);
  sqlite3_free(zSql);
  if( rc!=SQLITE_OK ){
    return rc;
  }

  /* Require a result. */
  rc = sqlite3_step(pStmt);
  if( rc==SQLITE_DONE ){
    rc = SQLITE_CORRUPT;
  }else if( rc==SQLITE_ROW ){
    *piRootPage = sqlite3_column_int(pStmt, 0);

    /* Require only one result. */
    rc = sqlite3_step(pStmt);
    if( rc==SQLITE_DONE ){
      rc = SQLITE_OK;
    }else if( rc==SQLITE_ROW ){
      rc = SQLITE_CORRUPT;
    }
  }
  sqlite3_finalize(pStmt);
  return rc;
}

/* Primary structure for iterating the contents of a table.
 *
 * TODO(shess): Handle interior nodes by iterating them for new leaf
 * pages, with interior nodes iterating their parents, and so on to
 * the root.  For now, only handles tables with one leaf node.
 *
 * TODO(shess): Account for overflow pages.
 *
 * leafCursorDestroy - release all resources associated with the cursor.
 * leafCursorCreate - create a cursor to iterate items from tree at
 *                    the provided root page.
 * leafCursorNextValidCell - get the cursor ready to access data from
 *                           the next valid cell in the table.
 * leafCursorCellRowid - get the current cell's rowid.
 * leafCursorCellColumns - get current cell's column count.
 * leafCursorCellColInfo - get type and data for a column in current cell.
 *
 * leafCursorNextValidCell skips cells which fail simple integrity
 * checks, such as overlapping other cells, or being located at
 * impossible offsets, or where header data doesn't correctly describe
 * payload data.  Returns SQLITE_ROW if a valid cell is found,
 * SQLITE_DONE if all pages in the tree were exhausted.
 */
typedef struct RecoverLeafCursor RecoverLeafCursor;
struct RecoverLeafCursor {
  /* TODO(shess): Something to handle interior nodes. */
  DbPage *pPage;                   /* Reference to leaf page. */
  unsigned nPageSize;              /* Size of pPage. */
  unsigned nCells;                 /* Number of cells in pPage. */
  unsigned iCell;                  /* Current cell. */

  /* Info parsed from data in iCell. */
  i64 iRowid;                      /* rowid parsed. */
  unsigned nRecordCols;            /* how many items in the record. */
  u64 iRecordOffset;               /* offset to record data. */
  /* TODO(shess): nRecordBytes and nRecordHeaderBytes are used in
   * leafCursorCellColInfo() to prevent buffer overruns.
   * leafCursorCellDecode() already verified that the cell is valid, so
   * those checks should be redundant.
   */
  u64 nRecordBytes;                /* Size of record data. */
  unsigned nRecordHeaderBytes;     /* Size of record header data. */
  unsigned char *pRecordHeader;    /* Pointer to record header data. */
  /* TODO(shess): Something to handle overflow pages. */
};

/* Internal helper shared between next-page and create-cursor.  If
 * pPage is a leaf page, it will be stored in the cursor and state
 * initialized for reading cells.
 *
 * TODO(shess): leafCursorNextPage() will use this when handling
 * interior pages.  It will loop over pages returned from the parent
 * interior node until one of them "sticks".
 *
 * If SQLITE_OK is returned, the caller no longer owns pPage,
 * otherwise the caller is responsible for discarding it.
 */
static int leafCursorLoadPage(RecoverLeafCursor *pCursor, DbPage *pPage){
  /* Release the current page. */
  if( pCursor->pPage ){
    sqlite3PagerUnref(pCursor->pPage);
    pCursor->pPage = NULL;
    pCursor->iCell = pCursor->nCells = 0;
  }

  /* TODO(shess): If the page is an interior node, use it to generate
   * leaf pages.  For now, bail out.
   */
  const unsigned char *pPageHeader = PageHeader(pPage);
  if( pPageHeader[kiPageTypeOffset]==kTableInteriorPage ){
    return SQLITE_ERROR;
  }

  /* If the page is not a leaf node, skip it. */
  if( pPageHeader[kiPageTypeOffset]!=kTableLeafPage ){
    sqlite3PagerUnref(pPage);
    return SQLITE_OK;
  }

  /* Take ownership of the page and start decoding. */
  pCursor->pPage = pPage;
  pCursor->iCell = 0;
  pCursor->nCells = decodeUnsigned16(pPageHeader + kiPageCellCountOffset);
  return SQLITE_OK;
}

/* Get the next leaf-level page in the tree.  Returns SQLITE_ROW when
 * a leaf page is found, SQLITE_DONE when no more leaves exist, or any
 * error which occurred.
 */
static int leafCursorNextPage(RecoverLeafCursor *pCursor){
  /* TODO(shess): Get the next leaf page and load it. */
  return SQLITE_DONE;
}

static void leafCursorDestroy(RecoverLeafCursor *pCursor){
  pCursor->pRecordHeader = NULL;

  if( pCursor->pPage ){
    sqlite3PagerUnref(pCursor->pPage);
    pCursor->pPage = NULL;
  }

  memset(pCursor, 0xA5, sizeof(*pCursor));
  sqlite3_free(pCursor);
}

/* Create a cursor to iterate the rows from the leaf pages of a table
 * rooted at iRootPage.
 */
/* TODO(shess): recoverOpen() calls this to setup the cursor, and I
 * think that recoverFilter() may make a hard assumption that the
 * cursor returned will turn up at least one valid cell.
 *
 * The cases I can think of which break this assumption are:
 * - pPage is a valid leaf page with no valid cells.
 * - pPage is a valid interior page with no valid leaves.
 * - pPage is a valid interior page who's leaves contain no valid cells.
 * - pPage is not a valid leaf or interior page.
 */
static int leafCursorCreate(Pager *pPager, unsigned nPageSize,
                            u32 iRootPage, RecoverLeafCursor **ppCursor){
  /* Start out with the root page. */
  DbPage *pPage;
  int rc = sqlite3PagerAcquire(pPager, iRootPage, &pPage, 0);
  if( rc!=SQLITE_OK ){
    return rc;
  }

  RecoverLeafCursor *pCursor = sqlite3_malloc(sizeof(RecoverLeafCursor));
  if( !pCursor ){
    sqlite3PagerUnref(pPage);
    return SQLITE_NOMEM;
  }
  memset(pCursor, 0, sizeof(*pCursor));

  pCursor->nPageSize = nPageSize;

  rc = leafCursorLoadPage(pCursor, pPage);
  if( rc!=SQLITE_OK ){
    sqlite3PagerUnref(pPage);
    leafCursorDestroy(pCursor);
    return rc;
  }

  /* pPage wasn't a leaf page, find the next leaf page. */
  /* TODO(shess): When interior nodes are handled, this will scan
   * forward to the first leaf node.
   */
  if( !pCursor->pPage ){
    rc = leafCursorNextPage(pCursor);
    if( rc!=SQLITE_DONE && rc!=SQLITE_ROW ){
      leafCursorDestroy(pCursor);
      return rc;
    }
  }

  *ppCursor = pCursor;
  return SQLITE_OK;
}

/* Useful for setting breakpoints. */
static int ValidateError(){
  return SQLITE_ERROR;
}

/* Setup the cursor for reading the information from cell iCell. */
static int leafCursorCellDecode(RecoverLeafCursor *pCursor){
  assert( pCursor->iCell<pCursor->nCells );

  pCursor->pRecordHeader = NULL;

  /* Find the offset to the row. */
  const unsigned char *pPageHeader = PageHeader(pCursor->pPage);
  const unsigned char *pCellOffsets = pPageHeader + knPageLeafHeaderBytes;
  const unsigned iCellOffset =
      decodeUnsigned16(pCellOffsets + pCursor->iCell*2);
  if( iCellOffset>=pCursor->nPageSize ){
    return ValidateError();
  }

  const unsigned char *pCell = PageData(pCursor->pPage, iCellOffset);
  const unsigned nCellMaxBytes = pCursor->nPageSize - iCellOffset;

  /* B-tree leaf cells lead with varint record size, varint rowid and
   * varint header size.
   */
  /* TODO(shess): The smallest page size is 512 bytes, which has an m
   * of 39.  Three varints need at most 27 bytes to encode.  I think.
   */
  if( !checkVarints(pCell, nCellMaxBytes, 3) ){
    return ValidateError();
  }

  u64 nRecordBytes;
  unsigned nRead = getVarint(pCell, &nRecordBytes);
  assert( iCellOffset+nRead<=pCursor->nPageSize );
  pCursor->nRecordBytes = nRecordBytes;

  u64 iRowid;
  nRead += getVarint(pCell + nRead, &iRowid);
  assert( iCellOffset+nRead<=pCursor->nPageSize );
  /* TODO(shess): This will not be true with overflow support. */
  assert( iCellOffset+nRead+nRecordBytes<=pCursor->nPageSize );
  pCursor->iRowid = (i64)iRowid;

  pCursor->iRecordOffset = iCellOffset + nRead;

  /* Check that no other cell starts within this cell. */
  unsigned i;
  const unsigned iEndOffset = iCellOffset + nRecordBytes;
  for( i=0; i<pCursor->nCells; ++i ){
    const unsigned iOtherOffset = decodeUnsigned16(pCellOffsets + i*2);
    if( iOtherOffset>iCellOffset && iOtherOffset<iEndOffset ){
      return ValidateError();
    }
  }

  unsigned nRecordHeaderRead = 0;
  u64 nRecordHeaderBytes;
  nRecordHeaderRead = getVarint(pCell + nRead, &nRecordHeaderBytes);
  assert( nRecordHeaderBytes<=nRecordBytes );
  pCursor->nRecordHeaderBytes = nRecordHeaderBytes;

  /* TODO(shess): The header data potentially could be large enough to
   * overflow.  For now just point directly into the page.
   */
  pCursor->pRecordHeader = (unsigned char *)PageData(pCursor->pPage,
                                                     iCellOffset + nRead);

  /* Tally up the column count and size of data. */
  unsigned nRecordCols = 0;
  u64 nRecordColBytes = 0;
  while( nRecordHeaderRead<nRecordHeaderBytes ){
    if( !checkVarint(pCursor->pRecordHeader + nRecordHeaderRead,
                     nRecordHeaderBytes - nRecordHeaderRead) ){
      return ValidateError();
    }
    u64 iSerialType;
    nRecordHeaderRead += getVarint(pCursor->pRecordHeader + nRecordHeaderRead,
                                   &iSerialType);
    if( iSerialType==10 || iSerialType==11 ){
      return ValidateError();
    }
    nRecordColBytes += SerialTypeLength(iSerialType);
    nRecordCols++;
  }
  pCursor->nRecordCols = nRecordCols;

  /* Parsing the header used as many bytes as expected. */
  if( nRecordHeaderRead!=nRecordHeaderBytes ){
    return ValidateError();
  }

  /* Calculated record is size of expected record. */
  if( nRecordHeaderBytes+nRecordColBytes!=nRecordBytes ){
    return ValidateError();
  }

  return SQLITE_OK;
}

static i64 leafCursorCellRowid(RecoverLeafCursor *pCursor){
  return pCursor->iRowid;
}

static unsigned leafCursorCellColumns(RecoverLeafCursor *pCursor){
  return pCursor->nRecordCols;
}

/* Get the column info for the cell.  Pass NULL for ppBase to prevent
 * retrieving the data segment.  If *pbFree is true, *ppBase must be
 * freed by the caller using sqlite3_free().
 */
/* TODO(shess): *pbFree will be necessary when supporting overflow. */
static int leafCursorCellColInfo(RecoverLeafCursor *pCursor,
                                 unsigned iCol, u64 *piColType,
                                 unsigned char **ppBase, int *pbFree){
  /* Implicit NULL for columns past the end.  This case happens when
   * rows have not been updated since an ALTER TABLE added columns.
   * It is more convenient to address here than in callers.
   */
  if( iCol>=pCursor->nRecordCols ){
    *piColType = 0;
    if( ppBase ){
      *ppBase = 0;
      *pbFree = 0;
    }
    return SQLITE_OK;
  }

  /* Must be able to decode header size. */
  const unsigned char *pRecordHeader = pCursor->pRecordHeader;
  if( !checkVarint(pRecordHeader, pCursor->nRecordHeaderBytes) ){
    return SQLITE_CORRUPT;
  }

  /* Rather than caching the header size and how many bytes it took,
   * decode it every time.
   */
  u64 nRecordHeaderBytes;
  unsigned nRead = getVarint(pRecordHeader, &nRecordHeaderBytes);
  assert( nRecordHeaderBytes==pCursor->nRecordHeaderBytes );

  /* Scan forward to the indicated column.  Scans to _after_ column
   * for later range checking.
   */
  /* TODO(shess): This could get expensive for very wide tables.  An
   * array of iSerialType could be built in leafCursorCellDecode(), but
   * the number of columns is dynamic per row, so it would add memory
   * management complexity.  Enough info to efficiently forward
   * iterate could be kept, if all clients forward iterate
   * (recoverColumn() may not).
   */
  u64 iColEndOffset = 0;
  unsigned nColsSkipped = 0;
  u64 iSerialType;
  while( nColsSkipped<=iCol && nRead<nRecordHeaderBytes ){
    if( !checkVarint(pRecordHeader + nRead, nRecordHeaderBytes - nRead) ){
      return SQLITE_CORRUPT;
    }
    nRead += getVarint(pRecordHeader + nRead, &iSerialType);
    iColEndOffset += SerialTypeLength(iSerialType);
    nColsSkipped++;
  }

  /* Column's data extends past record's end. */
  if( nRecordHeaderBytes+iColEndOffset>pCursor->nRecordBytes ){
    return SQLITE_CORRUPT;
  }

  *piColType = iSerialType;
  if( ppBase ){
    /* Offset from end of headers to beginning of column. */
    const u32 nColBytes = SerialTypeLength(iSerialType);
    const unsigned iColOffset = iColEndOffset-nColBytes;

    /* Start of record, plus the header, plus the column offset. */
    const unsigned iOffset =
        pCursor->iRecordOffset+nRecordHeaderBytes+iColOffset;

    /* TODO(shess): Deal with overflow pages. */
    *ppBase = (unsigned char *)PageData(pCursor->pPage, iOffset);
    *pbFree = 0;
  }
  return SQLITE_OK;
}

static int leafCursorNextValidCell(RecoverLeafCursor *pCursor){
  while( 1 ){
    /* Move to the next cell. */
    pCursor->iCell++;

    /* No more cells, get the next leaf. */
    if( pCursor->iCell>=pCursor->nCells ){
      int rc = leafCursorNextPage(pCursor);
      if( rc!=SQLITE_ROW ){
        return rc;
      }
      assert( pCursor->iCell==0 );
    }

    /* If the cell is valid, indicate that a row is available. */
    int rc = leafCursorCellDecode(pCursor);
    if( rc==SQLITE_OK ){
      return SQLITE_ROW;
    }

    /* Iterate until done or a valid row is found. */
    /* TODO(shess): Remove debugging output. */
    fprintf(stderr, "Skipping invalid cell\n");
  }
  return SQLITE_ERROR;
}

typedef struct Recover Recover;
struct Recover {
  sqlite3_vtab base;
  sqlite3 *db;                /* Host database connection */
  char *zDb;                  /* Database containing target table */
  char *zTable;               /* Target table */
  unsigned nCols;             /* Number of columns in target table */
  unsigned char *pTypes;      /* Types of columns in target table */
};

/* Internal helper for deleting the module. */
static void recoverRelease(Recover *pRecover){
  sqlite3_free(pRecover->zDb);
  sqlite3_free(pRecover->zTable);
  sqlite3_free(pRecover->pTypes);
  memset(pRecover, 0xA5, sizeof(*pRecover));
  sqlite3_free(pRecover);
}

/* Helper function for initializing the module.  Forward-declared so
 * recoverCreate() and recoverConnect() can see it.
 */
static int recoverInit(
  sqlite3 *, void *, int, const char *const*, sqlite3_vtab **, char **
);

static int recoverCreate(
  sqlite3 *db,
  void *pAux,
  int argc, const char *const*argv,
  sqlite3_vtab **ppVtab,
  char **pzErr
){
  FNENTRY();
  return recoverInit(db, pAux, argc, argv, ppVtab, pzErr);
}

/* This should never be called. */
static int recoverConnect(
  sqlite3 *db,
  void *pAux,
  int argc, const char *const*argv,
  sqlite3_vtab **ppVtab,
  char **pzErr
){
  FNENTRY();
  return recoverInit(db, pAux, argc, argv, ppVtab, pzErr);
}

/* No indices supported. */
static int recoverBestIndex(sqlite3_vtab *tab, sqlite3_index_info *pIdxInfo){
  FNENTRY();
  return SQLITE_OK;
}

/* Logically, this should never be called. */
static int recoverDisconnect(sqlite3_vtab *pVtab){
  FNENTRY();
  recoverRelease((Recover*)pVtab);
  return SQLITE_OK;
}

static int recoverDestroy(sqlite3_vtab *pVtab){
  FNENTRY();
  recoverRelease((Recover*)pVtab);
  return SQLITE_OK;
}

typedef struct RecoverCursor RecoverCursor;
struct RecoverCursor {
  sqlite3_vtab_cursor base;
  RecoverLeafCursor *pLeafCursor;
  int bEOF;
};

static int recoverOpen(sqlite3_vtab *pVTab, sqlite3_vtab_cursor **ppCursor){
  FNENTRY();

  Recover *pRecover = (Recover*)pVTab;

  u32 iRootPage = 0;
  int rc = getRootPage(pRecover->db, pRecover->zDb, pRecover->zTable,
                       &iRootPage);
  if( rc!=SQLITE_OK ){
    return rc;
  }

  unsigned nPageSize;
  Pager *pPager;
  rc = GetPager(pRecover->db, pRecover->zDb, &pPager, &nPageSize);
  if( rc!=SQLITE_OK ){
    return rc;
  }

  RecoverLeafCursor *pLeafCursor;
  rc = leafCursorCreate(pPager, nPageSize, iRootPage, &pLeafCursor);
  if( rc!=SQLITE_OK ){
    return rc;
  }

  RecoverCursor *pCursor = sqlite3_malloc(sizeof(RecoverCursor));
  if( !pCursor ){
    leafCursorDestroy(pLeafCursor);
    return SQLITE_NOMEM;
  }
  memset(pCursor, 0, sizeof(*pCursor));
  pCursor->base.pVtab = pVTab;
  pCursor->pLeafCursor = pLeafCursor;

  *ppCursor = (sqlite3_vtab_cursor*)pCursor;
  return SQLITE_OK;
}

static int recoverClose(sqlite3_vtab_cursor *cur){
  FNENTRY();
  RecoverCursor *pCursor = (RecoverCursor*)cur;
  if( pCursor->pLeafCursor ){
    leafCursorDestroy(pCursor->pLeafCursor);
    pCursor->pLeafCursor = NULL;
  }
  memset(pCursor, 0xA5, sizeof(*pCursor));
  sqlite3_free(cur);
  return SQLITE_OK;
}

/* Helpful place to set a breakpoint. */
static int RecoverInvalidCell(){
  return SQLITE_ERROR;
}

/* Returns SQLITE_OK if the cell has an appropriate number of columns
 * with the appropriate types of data.
 */
static int recoverValidateLeafCell(Recover *pRecover, RecoverCursor *pCursor){
  /* If the row's storage has too many columns, skip it. */
  if( leafCursorCellColumns(pCursor->pLeafCursor)>pRecover->nCols ){
    return RecoverInvalidCell();
  }

  /* Skip rows with unexpected types. */
  unsigned i;
  for( i=0; i<pRecover->nCols; ++i ){
    /* ROWID alias. */
    if( (pRecover->pTypes[i]&MASK_ROWID) ){
      continue;
    }

    u64 iType;
    int rc = leafCursorCellColInfo(pCursor->pLeafCursor, i, &iType, NULL, NULL);
    assert( rc==SQLITE_OK );
    if( rc!=SQLITE_OK || !SerialTypeIsCompatible(iType, pRecover->pTypes[i]) ){
      return RecoverInvalidCell();
    }
  }

  return SQLITE_OK;
}

static int recoverNext(sqlite3_vtab_cursor *pVtabCursor){
  FNENTRY();
  RecoverCursor *pCursor = (RecoverCursor*)pVtabCursor;
  Recover *pRecover = (Recover*)pCursor->base.pVtab;
  int rc;

  /* Scan forward to the next cell with valid storage, then check that
   * the stored data matches the schema.
   */
  while( (rc = leafCursorNextValidCell(pCursor->pLeafCursor))==SQLITE_ROW ){
    if( recoverValidateLeafCell(pRecover, pCursor)==SQLITE_OK ){
      return SQLITE_OK;
    }
  }

  if( rc==SQLITE_DONE ){
    pCursor->bEOF = 1;
    return SQLITE_OK;
  }

  assert( rc!=SQLITE_OK );
  return rc;
}

static int recoverFilter(
  sqlite3_vtab_cursor *pVtabCursor,
  int idxNum, const char *idxStr,
  int argc, sqlite3_value **argv
){
  FNENTRY();
  RecoverCursor *pCursor = (RecoverCursor*)pVtabCursor;
  Recover *pRecover = (Recover*)pCursor->base.pVtab;

  /* Load the first cell, and iterate forward if it's not valid. */
  /* TODO(shess): What happens if no cells at all are valid? */
  int rc = leafCursorCellDecode(pCursor->pLeafCursor);
  if( rc!=SQLITE_OK || recoverValidateLeafCell(pRecover, pCursor)!=SQLITE_OK ){
    return recoverNext(pVtabCursor);
  }

  return SQLITE_OK;
}

static int recoverEof(sqlite3_vtab_cursor *pVtabCursor){
  FNENTRY();
  RecoverCursor *pCursor = (RecoverCursor*)pVtabCursor;
  return pCursor->bEOF;
}

static int recoverColumn(sqlite3_vtab_cursor *cur, sqlite3_context *ctx, int i){
  FNENTRY();
  RecoverCursor *pCursor = (RecoverCursor*)cur;
  Recover *pRecover = (Recover*)pCursor->base.pVtab;

  if( i>=pRecover->nCols ){
    return SQLITE_ERROR;
  }

  /* ROWID alias. */
  if( (pRecover->pTypes[i]&MASK_ROWID) ){
    sqlite3_result_int64(ctx, leafCursorCellRowid(pCursor->pLeafCursor));
    return SQLITE_OK;
  }

  u64 iColType;
  unsigned char *pColData = NULL;
  int shouldFree = 0;
  int rc = leafCursorCellColInfo(pCursor->pLeafCursor, i, &iColType,
                                 &pColData, &shouldFree);
  if( rc!=SQLITE_OK ){
    return rc;
  }
  /* recoverValidateLeafCell() should guarantee that this will never
   * occur.
   */
  if( !SerialTypeIsCompatible(iColType, pRecover->pTypes[i]) ){
    if( shouldFree ){
      sqlite3_free(pColData);
    }
    return SQLITE_ERROR;
  }

  switch( iColType ){
    case 0 : sqlite3_result_null(ctx); break;
    case 1 : sqlite3_result_int64(ctx, decodeSigned(pColData, 1)); break;
    case 2 : sqlite3_result_int64(ctx, decodeSigned(pColData, 2)); break;
    case 3 : sqlite3_result_int64(ctx, decodeSigned(pColData, 3)); break;
    case 4 : sqlite3_result_int64(ctx, decodeSigned(pColData, 4)); break;
    case 5 : sqlite3_result_int64(ctx, decodeSigned(pColData, 6)); break;
    case 6 : sqlite3_result_int64(ctx, decodeSigned(pColData, 8)); break;
    case 7 : sqlite3_result_double(ctx, decodeFloat64(pColData)); break;
    case 8 : sqlite3_result_int(ctx, 0); break;
    case 9 : sqlite3_result_int(ctx, 1); break;
    case 10 : assert( iColType!=10 ); break;
    case 11 : assert( iColType!=11 ); break;

    default : {
      /* If pColData was already allocated, arrange to pass ownership. */
      sqlite3_destructor_type pFn = SQLITE_TRANSIENT;
      if( shouldFree ){
        pFn = sqlite3_free;
        shouldFree = 0;
      }

      u32 l = SerialTypeLength(iColType);
      if( SerialTypeIsBlob(iColType) ){
        sqlite3_result_blob(ctx, pColData, l, pFn);
      }else{
        sqlite3_result_text(ctx, (const char*)pColData, l, pFn);
      }
    } break;
  }
  if( shouldFree ){
    sqlite3_free(pColData);
  }
  return SQLITE_OK;
}

static int recoverRowid(sqlite3_vtab_cursor *pVtabCursor, sqlite_int64 *pRowid){
  FNENTRY();
  RecoverCursor *pCursor = (RecoverCursor*)pVtabCursor;
  *pRowid = leafCursorCellRowid(pCursor->pLeafCursor);
  return SQLITE_OK;
}

static sqlite3_module recoverModule = {
  0,                         /* iVersion */
  recoverCreate,             /* xCreate - create a table */
  recoverConnect,            /* xConnect - connect to an existing table */
  recoverBestIndex,          /* xBestIndex - Determine search strategy */
  recoverDisconnect,         /* xDisconnect - Disconnect from a table */
  recoverDestroy,            /* xDestroy - Drop a table */
  recoverOpen,               /* xOpen - open a cursor */
  recoverClose,              /* xClose - close a cursor */
  recoverFilter,             /* xFilter - configure scan constraints */
  recoverNext,               /* xNext - advance a cursor */
  recoverEof,                /* xEof */
  recoverColumn,             /* xColumn - read data */
  recoverRowid,              /* xRowid - read data */
  0,                         /* xUpdate - write data */
  0,                         /* xBegin - begin transaction */
  0,                         /* xSync - sync transaction */
  0,                         /* xCommit - commit transaction */
  0,                         /* xRollback - rollback transaction */
  0,                         /* xFindFunction - function overloading */
  0,                         /* xRename - rename the table */
};

int recoverVtableInit(sqlite3 *db){
  return sqlite3_create_module_v2(db, "recover", &recoverModule, NULL, 0);
}

/* This section of code is for parsing the create input and
 * initializing the module.
 */

/* Find the next word in zText and place the endpoints in pzWord*.
 * Returns true if the word is non-empty.  "Word" is defined as
 * alphanumeric plus '_' at this time.
 */
static int findWord(const char *zText,
                    const char **pzWordStart, const char **pzWordEnd){
  while( isspace(*zText) ){
    zText++;
  }
  *pzWordStart = zText;
  while( isalnum(*zText) || *zText=='_' ){
    zText++;
  }
  int r = zText>*pzWordStart;  /* In case pzWordStart==pzWordEnd */
  *pzWordEnd = zText;
  return r;
}

/* Return true if the next word in zText is zWord, also setting
 * *pzContinue to the character after the word.
 */
static int expectWord(const char *zText, const char *zWord,
                      const char **pzContinue){
  const char *zWordStart, *zWordEnd;
  if( findWord(zText, &zWordStart, &zWordEnd) &&
      strncasecmp(zWord, zWordStart, zWordEnd - zWordStart)==0 ){
    *pzContinue = zWordEnd;
    return 1;
  }
  return 0;
}

/* Parse the name and type information out of parameter.  In case of
 * success, *pzNameStart/End contain the name of the column,
 * *pzTypeStart/End contain the top-level type, and *pTypeMask has the
 * type mask to use for the column.
 */
static int findNameAndType(const char *parameter,
                           const char **pzNameStart, const char **pzNameEnd,
                           const char **pzTypeStart, const char **pzTypeEnd,
                           unsigned char *pTypeMask){
  if( !findWord(parameter, pzNameStart, pzNameEnd) ){
    return SQLITE_MISUSE;
  }

  /* Manifest typing, accept any storage type. */
  if( !findWord(*pzNameEnd, pzTypeStart, pzTypeEnd) ){
    *pzTypeEnd = *pzTypeStart = "";
    *pTypeMask = MASK_INTEGER | MASK_FLOAT | MASK_BLOB | MASK_TEXT | MASK_NULL;
    return SQLITE_OK;
  }

  /* strictMask is used for STRICT, strictMask|otherMask if STRICT is
   * not supplied.  zReplace provides an alternate type to expose to
   * the caller.
   */
  static struct {
    const char *zName;
    unsigned char strictMask;
    unsigned char otherMask;
    const char *zReplace;
  } kTypeInfo[] = {
    { "ANY",
      MASK_INTEGER | MASK_FLOAT | MASK_BLOB | MASK_TEXT | MASK_NULL,
      0, "",
    },
    { "ROWID",   MASK_INTEGER | MASK_ROWID,             0, "INTEGER", },
    { "INTEGER", MASK_INTEGER | MASK_NULL,              0, NULL, },
    { "FLOAT",   MASK_FLOAT | MASK_NULL,                MASK_INTEGER, NULL, },
    { "NUMERIC", MASK_INTEGER | MASK_FLOAT | MASK_NULL, MASK_TEXT, NULL, },
    { "TEXT",    MASK_TEXT | MASK_NULL,                 MASK_BLOB, NULL, },
    { "BLOB",    MASK_BLOB | MASK_NULL,                 0, NULL, },
  };

  unsigned i, nNameLen = *pzTypeEnd - *pzTypeStart;
  for( i=0; i<ArraySize(kTypeInfo); ++i ){
    if( strncasecmp(kTypeInfo[i].zName, *pzTypeStart, nNameLen)==0 ){
      break;
    }
  }
  if( i==ArraySize(kTypeInfo) ){
    return SQLITE_MISUSE;
  }

  const char *zEnd = *pzTypeEnd;
  int bStrict = 0;
  if( expectWord(zEnd, "STRICT", &zEnd) ){
    /* TODO(shess): Ick.  But I don't want another single-purpose
     * flag, either.
     */
    if( kTypeInfo[i].zReplace && !kTypeInfo[i].zReplace[0] ){
      return SQLITE_MISUSE;
    }
    bStrict = 1;
  }

  int bNotNull = 0;
  if( expectWord(zEnd, "NOT", &zEnd) ){
    if( expectWord(zEnd, "NULL", &zEnd) ){
      bNotNull = 1;
    }else{
      /* Anything other than NULL after NOT is an error. */
      return SQLITE_MISUSE;
    }
  }

  /* Anything else is an error. */
  const char *zDummy;
  if( findWord(zEnd, &zDummy, &zDummy) ){
    return SQLITE_MISUSE;
  }

  *pTypeMask = kTypeInfo[i].strictMask;
  if( !bStrict ){
    *pTypeMask |= kTypeInfo[i].otherMask;
  }
  if( bNotNull ){
    *pTypeMask &= ~MASK_NULL;
  }
  if( kTypeInfo[i].zReplace ){
    *pzTypeStart = kTypeInfo[i].zReplace;
    *pzTypeEnd = *pzTypeStart + strlen(*pzTypeStart);
  }
  return SQLITE_OK;
}

/* Parse the arguments, placing type masks in *pTypes and the exposed
 * schema in *pzCreateSql (for sqlite3_declare_vtab).
 */
static int ParseColumnsAndGenerateCreate(unsigned nCols,
                                         const char *const *pCols,
                                         char **pzCreateSql,
                                         unsigned char *pTypes,
                                         char **pzErr){
  char *zCreateSql = sqlite3_mprintf("CREATE TABLE x(");
  if( !zCreateSql ){
    return SQLITE_NOMEM;
  }

  unsigned i;
  for( i=0; i<nCols; i++ ){
    const char *zNameStart, *zNameEnd;
    const char *zTypeStart, *zTypeEnd;
    int rc = findNameAndType(pCols[i],
                             &zNameStart, &zNameEnd,
                             &zTypeStart, &zTypeEnd,
                             &pTypes[i]);
    if( rc!=SQLITE_OK ){
      *pzErr = sqlite3_mprintf("unable to parse column %d", i);
      sqlite3_free(zCreateSql);
      return rc;
    }

    const char *zNotNull = "";
    if( !(pTypes[i]&MASK_NULL) ){
      zNotNull = " NOT NULL";
    }

    /* Add name and type to the create statement. */
    const char *zSep = (i < nCols - 1 ? ", " : ")");
    zCreateSql = sqlite3_mprintf("%z%.*s %.*s%s%s",
                                 zCreateSql,
                                 zNameEnd - zNameStart, zNameStart,
                                 zTypeEnd - zTypeStart, zTypeStart,
                                 zNotNull, zSep);
    if( !zCreateSql ){
      return SQLITE_NOMEM;
    }
  }

  *pzCreateSql = zCreateSql;
  return SQLITE_OK;
}

/* Helper function for initializing the module. */
/* TODO(shess): Since connect isn't supported, could inline into
 * recoverCreate().
 */
/* TODO(shess): Explore cases where it would make sense to set *pzErr. */
static int recoverInit(
  sqlite3 *db,                        /* Database connection */
  void *pAux,                         /* unused */
  int argc, const char *const*argv,   /* Parameters to CREATE TABLE statement */
  sqlite3_vtab **ppVtab,              /* OUT: New virtual table */
  char **pzErr                        /* OUT: Error message, if any */
){
  /* argv[0] module name
   * argv[1] db name for virtual table
   * argv[2] virtual table name
   * argv[3] backing table name
   * argv[4] columns
   */
  const unsigned kTypeCol = 4;

  /* Require to be in the temp database. */
  if( strcasecmp(argv[1], "temp")!=0 ){
    *pzErr = sqlite3_mprintf("recover table must be in temp database");
    return SQLITE_MISUSE;
  }

  /* Need the backing table and at least one column. */
  if( argc<=kTypeCol ){
    *pzErr = sqlite3_mprintf("no columns specified");
    return SQLITE_MISUSE;
  }

  Recover *pRecover = sqlite3_malloc(sizeof(Recover));
  if( !pRecover ){
    return SQLITE_NOMEM;
  }
  memset(pRecover, 0, sizeof(*pRecover));
  pRecover->base.pModule = &recoverModule;
  pRecover->db = db;

  /* Parse out db.table, assuming main if no dot. */
  char *zDot = strchr(argv[3], '.');
  if( !zDot ){
    pRecover->zDb = sqlite3_strdup(db->aDb[0].zName);
    pRecover->zTable = sqlite3_strdup(argv[3]);
  }else if( zDot>argv[3] && zDot[1]!='\0' ){
    pRecover->zDb = sqlite3_strndup(argv[3], zDot - argv[3]);
    pRecover->zTable = sqlite3_strdup(zDot + 1);
  }else{
    /* ".table" or "db." not allowed. */
    *pzErr = sqlite3_mprintf("ill-formed table specifier");
    recoverRelease(pRecover);
    return SQLITE_ERROR;
  }

  pRecover->nCols = argc - kTypeCol;
  pRecover->pTypes = sqlite3_malloc(pRecover->nCols);
  if( !pRecover->zDb || !pRecover->zTable || !pRecover->pTypes ){
    recoverRelease(pRecover);
    return SQLITE_NOMEM;
  }

  /* Require the backing table to exist. */
  /* TODO(shess): Be more pedantic about the form of the descriptor
   * string.  This already fails for poorly-formed strings, simply
   * because there won't be a root page, but it would make more sense
   * to be explicit.
   */
  u32 iRootPage;
  int rc = getRootPage(pRecover->db, pRecover->zDb, pRecover->zTable,
                       &iRootPage);
  if( rc!=SQLITE_OK ){
    *pzErr = sqlite3_mprintf("unable to find backing table");
    recoverRelease(pRecover);
    return rc;
  }

  /* Parse the column definitions. */
  char *zCreateSql;
  rc = ParseColumnsAndGenerateCreate(pRecover->nCols, argv + kTypeCol,
                                     &zCreateSql, pRecover->pTypes, pzErr);
  if( rc!=SQLITE_OK ){
    recoverRelease(pRecover);
    return rc;
  }

  rc = sqlite3_declare_vtab(db, zCreateSql);
  sqlite3_free(zCreateSql);
  if( rc!=SQLITE_OK ){
    recoverRelease(pRecover);
    return rc;
  }

  *ppVtab = (sqlite3_vtab *)pRecover;
  return SQLITE_OK;
}
