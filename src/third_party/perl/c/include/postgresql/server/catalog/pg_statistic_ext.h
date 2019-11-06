/*-------------------------------------------------------------------------
 *
 * pg_statistic_ext.h
 *	  definition of the "extended statistics" system catalog (pg_statistic_ext)
 *
 *
 * Portions Copyright (c) 1996-2018, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/catalog/pg_statistic_ext.h
 *
 * NOTES
 *	  The Catalog.pm module reads this file and derives schema
 *	  information.
 *
 *-------------------------------------------------------------------------
 */
#ifndef PG_STATISTIC_EXT_H
#define PG_STATISTIC_EXT_H

#include "catalog/genbki.h"
#include "catalog/pg_statistic_ext_d.h"

/* ----------------
 *		pg_statistic_ext definition.  cpp turns this into
 *		typedef struct FormData_pg_statistic_ext
 * ----------------
 */
CATALOG(pg_statistic_ext,3381,StatisticExtRelationId)
{
	Oid			stxrelid;		/* relation containing attributes */

	/* These two fields form the unique key for the entry: */
	NameData	stxname;		/* statistics object name */
	Oid			stxnamespace;	/* OID of statistics object's namespace */

	Oid			stxowner;		/* statistics object's owner */

	/*
	 * variable-length fields start here, but we allow direct access to
	 * stxkeys
	 */
	int2vector	stxkeys;		/* array of column keys */

#ifdef CATALOG_VARLEN
	char		stxkind[1] BKI_FORCE_NOT_NULL;	/* statistics kinds requested
												 * to build */
	pg_ndistinct stxndistinct;	/* ndistinct coefficients (serialized) */
	pg_dependencies stxdependencies;	/* dependencies (serialized) */
#endif

} FormData_pg_statistic_ext;

/* ----------------
 *		Form_pg_statistic_ext corresponds to a pointer to a tuple with
 *		the format of pg_statistic_ext relation.
 * ----------------
 */
typedef FormData_pg_statistic_ext *Form_pg_statistic_ext;

#ifdef EXPOSE_TO_CLIENT_CODE

#define STATS_EXT_NDISTINCT			'd'
#define STATS_EXT_DEPENDENCIES		'f'

#endif							/* EXPOSE_TO_CLIENT_CODE */

#endif							/* PG_STATISTIC_EXT_H */
