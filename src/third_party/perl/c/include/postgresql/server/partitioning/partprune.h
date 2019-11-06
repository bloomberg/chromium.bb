/*-------------------------------------------------------------------------
 *
 * partprune.h
 *	  prototypes for partprune.c
 *
 *
 * Portions Copyright (c) 1996-2018, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/partitioning/partprune.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef PARTPRUNE_H
#define PARTPRUNE_H

#include "nodes/execnodes.h"
#include "nodes/relation.h"


/*
 * PartitionPruneContext
 *		Stores information needed at runtime for pruning computations
 *		related to a single partitioned table.
 *
 * partrel			Relcache pointer for the partitioned table,
 *					if we have it open (else NULL).
 * strategy			Partition strategy, e.g. LIST, RANGE, HASH.
 * partnatts		Number of columns in the partition key.
 * nparts			Number of partitions in this partitioned table.
 * boundinfo		Partition boundary info for the partitioned table.
 * partcollation	Array of partnatts elements, storing the collations of the
 *					partition key columns.
 * partsupfunc		Array of FmgrInfos for the comparison or hashing functions
 *					associated with the partition keys (partnatts elements).
 *					(This points into the partrel's partition key, typically.)
 * stepcmpfuncs		Array of FmgrInfos for the comparison or hashing function
 *					for each pruning step and partition key.
 * ppccontext		Memory context holding this PartitionPruneContext's
 *					subsidiary data, such as the FmgrInfos.
 * planstate		Points to the parent plan node's PlanState when called
 *					during execution; NULL when called from the planner.
 * exprstates		Array of ExprStates, indexed as per PruneCtxStateIdx; one
 *					for each partition key in each pruning step.  Allocated if
 *					planstate is non-NULL, otherwise NULL.
 * exprhasexecparam	Array of bools, each true if corresponding 'exprstate'
 *					expression contains any PARAM_EXEC Params.  (Can be NULL
 *					if planstate is NULL.)
 * evalexecparams	True if it's safe to evaluate PARAM_EXEC Params.
 */
typedef struct PartitionPruneContext
{
	Relation	partrel;
	char		strategy;
	int			partnatts;
	int			nparts;
	PartitionBoundInfo boundinfo;
	Oid		   *partcollation;
	FmgrInfo   *partsupfunc;
	FmgrInfo   *stepcmpfuncs;
	MemoryContext ppccontext;
	PlanState  *planstate;
	ExprState **exprstates;
	bool	   *exprhasexecparam;
	bool		evalexecparams;
} PartitionPruneContext;

/*
 * PruneCxtStateIdx() computes the correct index into the stepcmpfuncs[],
 * exprstates[] and exprhasexecparam[] arrays for step step_id and
 * partition key column keyno.  (Note: there is code that assumes the
 * entries for a given step are sequential, so this is not chosen freely.)
 */
#define PruneCxtStateIdx(partnatts, step_id, keyno) \
	((partnatts) * (step_id) + (keyno))

extern PartitionPruneInfo *make_partition_pruneinfo(PlannerInfo *root,
						 RelOptInfo *parentrel,
						 List *subpaths,
						 List *partitioned_rels,
						 List *prunequal);
extern Relids prune_append_rel_partitions(RelOptInfo *rel);
extern Bitmapset *get_matching_partitions(PartitionPruneContext *context,
						List *pruning_steps);

#endif							/* PARTPRUNE_H */
