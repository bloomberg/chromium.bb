/*-------------------------------------------------------------------------
 *
 * pg_range.h
 *	  definition of the "range type" system catalog (pg_range)
 *
 *
 * Portions Copyright (c) 1996-2018, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/catalog/pg_range.h
 *
 * NOTES
 *	  The Catalog.pm module reads this file and derives schema
 *	  information.
 *
 *-------------------------------------------------------------------------
 */
#ifndef PG_RANGE_H
#define PG_RANGE_H

#include "catalog/genbki.h"
#include "catalog/pg_range_d.h"

/* ----------------
 *		pg_range definition.  cpp turns this into
 *		typedef struct FormData_pg_range
 * ----------------
 */
CATALOG(pg_range,3541,RangeRelationId) BKI_WITHOUT_OIDS
{
	/* OID of owning range type */
	Oid			rngtypid BKI_LOOKUP(pg_type);

	/* OID of range's element type (subtype) */
	Oid			rngsubtype BKI_LOOKUP(pg_type);

	/* collation for this range type, or 0 */
	Oid			rngcollation BKI_DEFAULT(0);

	/* subtype's btree opclass */
	Oid			rngsubopc BKI_LOOKUP(pg_opclass);

	/* canonicalize range, or 0 */
	regproc		rngcanonical BKI_LOOKUP(pg_proc);

	/* subtype difference as a float8, or 0 */
	regproc		rngsubdiff BKI_LOOKUP(pg_proc);
} FormData_pg_range;

/* ----------------
 *		Form_pg_range corresponds to a pointer to a tuple with
 *		the format of pg_range relation.
 * ----------------
 */
typedef FormData_pg_range *Form_pg_range;

/*
 * prototypes for functions in pg_range.c
 */

extern void RangeCreate(Oid rangeTypeOid, Oid rangeSubType, Oid rangeCollation,
			Oid rangeSubOpclass, RegProcedure rangeCanonical,
			RegProcedure rangeSubDiff);
extern void RangeDelete(Oid rangeTypeOid);

#endif							/* PG_RANGE_H */
