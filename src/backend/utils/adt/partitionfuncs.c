/*-------------------------------------------------------------------------
 *
 * partitionfuncs.c
 *	  Functions for accessing partition-related metadata
 *
 * Portions Copyright (c) 1996-2018, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/partitionfuncs.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/htup_details.h"
#include "catalog/partition.h"
#include "catalog/pg_class.h"
#include "catalog/pg_inherits.h"
#include "catalog/pg_type.h"
#include "funcapi.h"
#include "utils/fmgrprotos.h"
#include "utils/lsyscache.h"


/*
 * pg_partition_tree
 *
 * Produce a view with one row per member of a partition tree, beginning
 * from the top-most parent given by the caller.  This gives information
 * about each partition, its immediate partitioned parent, if it is
 * a leaf partition and its level in the hierarchy.
 */
Datum
pg_partition_tree(PG_FUNCTION_ARGS)
{
#define PG_PARTITION_TREE_COLS	4
	Oid			rootrelid = PG_GETARG_OID(0);
	char		relkind = get_rel_relkind(rootrelid);
	FuncCallContext *funcctx;
	ListCell  **next;

	/* Only allow relation types that can appear in partition trees. */
	if (relkind != RELKIND_RELATION &&
		relkind != RELKIND_FOREIGN_TABLE &&
		relkind != RELKIND_INDEX &&
		relkind != RELKIND_PARTITIONED_TABLE &&
		relkind != RELKIND_PARTITIONED_INDEX)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("\"%s\" is not a table, a foreign table, or an index",
						get_rel_name(rootrelid))));

	/* stuff done only on the first call of the function */
	if (SRF_IS_FIRSTCALL())
	{
		MemoryContext oldcxt;
		TupleDesc	tupdesc;
		List	   *partitions;

		/* create a function context for cross-call persistence */
		funcctx = SRF_FIRSTCALL_INIT();

		/* switch to memory context appropriate for multiple function calls */
		oldcxt = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

		/*
		 * Find all members of inheritance set.  We only need AccessShareLock
		 * on the children for the partition information lookup.
		 */
		partitions = find_all_inheritors(rootrelid, AccessShareLock, NULL);

		tupdesc = CreateTemplateTupleDesc(PG_PARTITION_TREE_COLS, false);
		TupleDescInitEntry(tupdesc, (AttrNumber) 1, "relid",
						   REGCLASSOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 2, "parentid",
						   REGCLASSOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 3, "isleaf",
						   BOOLOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 4, "level",
						   INT4OID, -1, 0);

		funcctx->tuple_desc = BlessTupleDesc(tupdesc);

		/* allocate memory for user context */
		next = (ListCell **) palloc(sizeof(ListCell *));
		*next = list_head(partitions);
		funcctx->user_fctx = (void *) next;

		MemoryContextSwitchTo(oldcxt);
	}

	/* stuff done on every call of the function */
	funcctx = SRF_PERCALL_SETUP();
	next = (ListCell **) funcctx->user_fctx;

	if (*next != NULL)
	{
		Datum		result;
		Datum		values[PG_PARTITION_TREE_COLS];
		bool		nulls[PG_PARTITION_TREE_COLS];
		HeapTuple	tuple;
		Oid			parentid = InvalidOid;
		Oid			relid = lfirst_oid(*next);
		char		relkind = get_rel_relkind(relid);
		int			level = 0;
		List	   *ancestors = get_partition_ancestors(lfirst_oid(*next));
		ListCell   *lc;

		/*
		 * Form tuple with appropriate data.
		 */
		MemSet(nulls, 0, sizeof(nulls));
		MemSet(values, 0, sizeof(values));

		/* relid */
		values[0] = ObjectIdGetDatum(relid);

		/* parentid */
		if (ancestors != NIL)
			parentid = linitial_oid(ancestors);
		if (OidIsValid(parentid))
			values[1] = ObjectIdGetDatum(parentid);
		else
			nulls[1] = true;

		/* isleaf */
		values[2] = BoolGetDatum(relkind != RELKIND_PARTITIONED_TABLE &&
								 relkind != RELKIND_PARTITIONED_INDEX);

		/* level */
		if (relid != rootrelid)
		{
			foreach(lc, ancestors)
			{
				level++;
				if (lfirst_oid(lc) == rootrelid)
					break;
			}
		}
		values[3] = Int32GetDatum(level);

		*next = lnext(*next);

		tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);
		result = HeapTupleGetDatum(tuple);
		SRF_RETURN_NEXT(funcctx, result);
	}

	/* done when there are no more elements left */
	SRF_RETURN_DONE(funcctx);
}
