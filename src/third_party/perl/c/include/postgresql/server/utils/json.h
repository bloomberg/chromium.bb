/*-------------------------------------------------------------------------
 *
 * json.h
 *	  Declarations for JSON data type support.
 *
 * Portions Copyright (c) 1996-2018, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/utils/json.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef JSON_H
#define JSON_H

#include "lib/stringinfo.h"

/* functions in json.c */
extern void escape_json(StringInfo buf, const char *str);

#endif							/* JSON_H */
