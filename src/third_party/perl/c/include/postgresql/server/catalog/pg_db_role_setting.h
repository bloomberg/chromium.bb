/*-------------------------------------------------------------------------
 *
 * pg_db_role_setting.h
 *	  definition of the system catalog for per-database/per-user
 *	  configuration settings (pg_db_role_setting)
 *
 *
 * Portions Copyright (c) 1996-2018, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/catalog/pg_db_role_setting.h
 *
 * NOTES
 *	  The Catalog.pm module reads this file and derives schema
 *	  information.
 *
 *-------------------------------------------------------------------------
 */
#ifndef PG_DB_ROLE_SETTING_H
#define PG_DB_ROLE_SETTING_H

#include "catalog/genbki.h"
#include "catalog/pg_db_role_setting_d.h"

#include "utils/guc.h"
#include "utils/relcache.h"
#include "utils/snapshot.h"

/* ----------------
 *		pg_db_role_setting definition.  cpp turns this into
 *		typedef struct FormData_pg_db_role_setting
 * ----------------
 */
CATALOG(pg_db_role_setting,2964,DbRoleSettingRelationId) BKI_SHARED_RELATION BKI_WITHOUT_OIDS
{
	Oid			setdatabase;	/* database */
	Oid			setrole;		/* role */

#ifdef CATALOG_VARLEN			/* variable-length fields start here */
	text		setconfig[1];	/* GUC settings to apply at login */
#endif
} FormData_pg_db_role_setting;

typedef FormData_pg_db_role_setting * Form_pg_db_role_setting;

/*
 * prototypes for functions in pg_db_role_setting.h
 */
extern void AlterSetting(Oid databaseid, Oid roleid, VariableSetStmt *setstmt);
extern void DropSetting(Oid databaseid, Oid roleid);
extern void ApplySetting(Snapshot snapshot, Oid databaseid, Oid roleid,
			 Relation relsetting, GucSource source);

#endif							/* PG_DB_ROLE_SETTING_H */
