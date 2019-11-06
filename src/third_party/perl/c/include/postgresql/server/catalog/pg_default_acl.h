/*-------------------------------------------------------------------------
 *
 * pg_default_acl.h
 *	  definition of the system catalog for default ACLs of new objects
 *	  (pg_default_acl)
 *
 *
 * Portions Copyright (c) 1996-2018, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/catalog/pg_default_acl.h
 *
 * NOTES
 *	  The Catalog.pm module reads this file and derives schema
 *	  information.
 *
 *-------------------------------------------------------------------------
 */
#ifndef PG_DEFAULT_ACL_H
#define PG_DEFAULT_ACL_H

#include "catalog/genbki.h"
#include "catalog/pg_default_acl_d.h"

/* ----------------
 *		pg_default_acl definition.  cpp turns this into
 *		typedef struct FormData_pg_default_acl
 * ----------------
 */
CATALOG(pg_default_acl,826,DefaultAclRelationId)
{
	Oid			defaclrole;		/* OID of role owning this ACL */
	Oid			defaclnamespace;	/* OID of namespace, or 0 for all */
	char		defaclobjtype;	/* see DEFACLOBJ_xxx constants below */

#ifdef CATALOG_VARLEN			/* variable-length fields start here */
	aclitem		defaclacl[1];	/* permissions to add at CREATE time */
#endif
} FormData_pg_default_acl;

/* ----------------
 *		Form_pg_default_acl corresponds to a pointer to a tuple with
 *		the format of pg_default_acl relation.
 * ----------------
 */
typedef FormData_pg_default_acl *Form_pg_default_acl;

#ifdef EXPOSE_TO_CLIENT_CODE

/*
 * Types of objects for which the user is allowed to specify default
 * permissions through pg_default_acl.  These codes are used in the
 * defaclobjtype column.
 */
#define DEFACLOBJ_RELATION		'r' /* table, view */
#define DEFACLOBJ_SEQUENCE		'S' /* sequence */
#define DEFACLOBJ_FUNCTION		'f' /* function */
#define DEFACLOBJ_TYPE			'T' /* type */
#define DEFACLOBJ_NAMESPACE		'n' /* namespace */

#endif							/* EXPOSE_TO_CLIENT_CODE */

#endif							/* PG_DEFAULT_ACL_H */
