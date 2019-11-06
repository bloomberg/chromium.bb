/*
   Copyright (c) 1994-2006  Tim Bunce

   See the COPYRIGHT section in the Oracle.pm file for terms.

*/

/* ====== Include Oracle Header Files ====== */

#ifndef CAN_PROTOTYPE
#define signed	/* Oracle headers use signed */
#endif

/* The following define avoids a problem with Oracle >=7.3 where
 * ociapr.h has the line:
 *	sword  obindps(struct cda_def *cursor, ub1 opcode, text *sqlvar, ...
 * In some compilers that clashes with perls 'opcode' enum definition.
 */
#define opcode opcode_redefined

/* Hack to fix broken Oracle oratypes.h on OSF Alpha. Sigh.	*/
#if defined(__osf__) && defined(__alpha)
#ifndef A_OSF
#define A_OSF
#endif
#endif

/* egcs-1.1.2 does not have _int64 */
#if defined(__MINGW32__) || defined(__CYGWIN32__)
#define _int64 long long
#endif


/* ori.h uses 'dirty' as an arg name in prototypes so we use this */
/* hack to prevent ori.h being read (since we don't need it)	  */
/*#define ORI_ORACLE*/
#include <oci.h>
#include <oratypes.h>
#include <ocidfn.h>
#include <orid.h>
#include <ori.h>

/* ------ end of Oracle include files ------ */


#define NEED_DBIXS_VERSION 93

#define PERL_NO_GET_CONTEXT  /*for Threaded Perl */

#include <DBIXS.h>		/* installed by the DBI module	*/

#include "dbdimp.h"

#include "dbivport.h"

#include <dbd_xsh.h>		/* installed by the DBI module	*/

/* These prototypes are for dbdimp.c funcs used in the XS file          */
/* These names are #defined to driver specific names in dbdimp.h        */

void	dbd_init _((dbistate_t *dbistate));
void	dbd_init_oci_drh _((imp_drh_t * imp_drh));
void	dbd_dr_destroy _((SV *drh, imp_drh_t *imp_drh));

int	 dbd_db_login  _((SV *dbh, imp_dbh_t *imp_dbh, char *dbname, char *user, char *pwd));
int	 dbd_db_do _((SV *sv, char *statement));
int	 dbd_db_commit     _((SV *dbh, imp_dbh_t *imp_dbh));
int	 dbd_db_rollback   _((SV *dbh, imp_dbh_t *imp_dbh));
int dbd_st_bind_col(SV *sth, imp_sth_t *imp_sth, SV *col, SV *ref, IV type, SV *attribs);
int	 dbd_db_disconnect _((SV *dbh, imp_dbh_t *imp_dbh));
void dbd_db_destroy    _((SV *dbh, imp_dbh_t *imp_dbh));
int	 dbd_db_STORE_attrib _((SV *dbh, imp_dbh_t *imp_dbh, SV *keysv, SV *valuesv));
SV	*dbd_db_FETCH_attrib _((SV *dbh, imp_dbh_t *imp_dbh, SV *keysv));

int	 dbd_st_prepare _((SV *sth, imp_sth_t *imp_sth,
		char *statement, SV *attribs));
int	 dbd_st_rows	_((SV *sth, imp_sth_t *imp_sth));
int	 dbd_st_execute _((SV *sth, imp_sth_t *imp_sth));
int	 dbd_st_cancel  _((SV *sth, imp_sth_t *imp_sth));
AV	*dbd_st_fetch	_((SV *sth, imp_sth_t *imp_sth));

int	 dbd_st_finish	_((SV *sth, imp_sth_t *imp_sth));
void dbd_st_destroy _((SV *sth, imp_sth_t *imp_sth));
int	 dbd_st_blob_read _((SV *sth, imp_sth_t *imp_sth,
		int field, long offset, long len, SV *destrv, long destoffset));
int	 dbd_st_STORE_attrib _((SV *sth, imp_sth_t *imp_sth, SV *keysv, SV *valuesv));
SV	*dbd_st_FETCH_attrib _((SV *sth, imp_sth_t *imp_sth, SV *keysv));
int	 dbd_bind_ph  _((SV *sth, imp_sth_t *imp_sth,
		SV *param, SV *value, IV sql_type, SV *attribs, int is_inout, IV maxlen));

int	 dbd_db_login6 _((SV *dbh, imp_dbh_t *imp_dbh, char *dbname, char *user, char *pwd, SV *attr));
int	 dbd_describe _((SV *sth, imp_sth_t *imp_sth));
ub4	 ora_blob_read_piece _((SV *sth, imp_sth_t *imp_sth, imp_fbh_t *fbh, SV *dest_sv,
                   long offset, UV len, long destoffset));
ub4	 ora_blob_read_mb_piece _((SV *sth, imp_sth_t *imp_sth, imp_fbh_t *fbh, SV *dest_sv,
		   long offset, ub4 len, long destoffset));

/* Oracle types */
#define ORA_VARCHAR2		1
#define ORA_STRING			5
#define ORA_NUMBER			2
#define ORA_LONG			8
#define ORA_ROWID			11
#define ORA_DATE			12
#define ORA_RAW				23
#define ORA_LONGRAW			24
#define ORA_CHAR			96
#define ORA_CHARZ			97
#define ORA_MLSLABEL		105
#define ORA_CLOB 			112
#define ORA_BLOB			113
#define ORA_BFILE			114
#define ORA_RSET			116
#define ORA_VARCHAR2_TABLE	201
#define ORA_NUMBER_TABLE	202
#define ORA_XMLTYPE			108




/* other Oracle not in noraml API defines

most of these are largly undocumented XML functions that are in the API but not defined
not noramlly found in the  defines the prototypes of OCI functions in most clients
Normally can be found in ociap.h (Oracle Call Interface - Ansi Prototypes
) and ocikp.h (functions in K&R style)

They will be added when needed

*/

sword  OCIXMLTypeCreateFromSrc( OCISvcCtx *svchp, OCIError *errhp,
                     OCIDuration dur, ub1 src_type, dvoid *src_ptr,
                     sb4 ind, OCIXMLType **retInstance );


/* end of Oracle.h */
