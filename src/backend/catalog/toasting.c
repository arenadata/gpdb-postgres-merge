/*-------------------------------------------------------------------------
 *
 * toasting.c
 *	  This file contains routines to support creation of toast tables
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/catalog/toasting.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/heapam.h"
#include "access/xact.h"
#include "catalog/binary_upgrade.h"
#include "catalog/catalog.h"
#include "catalog/dependency.h"
#include "catalog/heap.h"
#include "catalog/index.h"
#include "catalog/namespace.h"
#include "catalog/oid_dispatch.h"
#include "catalog/pg_am.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_opclass.h"
#include "catalog/pg_type.h"
#include "catalog/toasting.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "storage/lmgr.h"
#include "storage/lock.h"
#include "utils/builtins.h"
#include "utils/rel.h"
#include "utils/syscache.h"

/* Potentially set by pg_upgrade_support functions */
Oid			binary_upgrade_next_toast_pg_type_oid = InvalidOid;

static void CheckAndCreateToastTable(Oid relOid, Datum reloptions,
									 LOCKMODE lockmode, bool check);
static bool create_toast_table(Relation rel, Oid toastOid, Oid toastIndexOid,
							   Datum reloptions, LOCKMODE lockmode, bool check);
static bool needs_toast_table(Relation rel);


/*
 * CreateToastTable variants
 *		If the table needs a toast table, and doesn't already have one,
 *		then create a toast table for it.
 *
 * reloptions for the toast table can be passed, too.  Pass (Datum) 0
 * for default reloptions.
 *
 * We expect the caller to have verified that the relation is a table and have
 * already done any necessary permission checks.  Callers expect this function
 * to end with CommandCounterIncrement if it makes any changes.
 *
 * If 'is_create' is true, we are creating the toast table for a new table,
 * rather than adding it to an existing one. The difference is that the
 * caller is expected to already hold an AccessExclusiveLock, if we're
 * creating a new table.
 *
 * If 'is_part_child' is true, we are creating a toast table for a non-root
 * table in a partition hierarchy.  This determines if array type gets created
 * for the table or not.  If 'is_part_parent' is true, then we are creating a
 * toast table for a non-leaf table in a partition hierarchy.  This is used to
 * determine the value of relfrozenxid for the toast table.  Non-leaf tables do
 * not contain data, so their relfrozenxid need not interfere in database age
 * computation.
 */
void
AlterTableCreateToastTable(Oid relOid, Datum reloptions, LOCKMODE lockmode,
						   bool is_part_child, bool is_part_parent)
{
	CheckAndCreateToastTable(relOid, reloptions, lockmode, true,
							 is_part_child, is_part_parent);
}

void
NewHeapCreateToastTable(Oid relOid, Datum reloptions, LOCKMODE lockmode,
							bool is_part_child, bool is_part_parent)
{
	CheckAndCreateToastTable(relOid, reloptions, lockmode, false,
							 is_part_child, is_part_parent);
}

void
NewRelationCreateToastTable(Oid relOid, Datum reloptions,
							bool is_part_child, bool is_part_parent)
{
	LOCKMODE	lockmode;

	/*
	 * Grab a DDL-exclusive lock on the target table, since we'll update the
	 * pg_class tuple.	This is redundant for all present users.  Tuple
	 * toasting behaves safely in the face of a concurrent TOAST table add.
	 *
	 * When we're creating a new table, we should already hold an AccessExclusiveLock
	 * on it. It may seem silly to acquire a *stronger* lock in that, case, but
	 * the idea is that it's cheaper to hold an AccessExclusiveLock twice, rather
	 * than both an AccessExlusiveLock and a ShareUpdateExclusiveLock.
	 */
	if (is_part_child)
		lockmode = NoLock;
	else
		lockmode = AccessExclusiveLock;

	CheckAndCreateToastTable(relOid, reloptions, AccessExclusiveLock, false,
							 is_part_child, is_part_parent);
}

static void
CheckAndCreateToastTable(Oid relOid, Datum reloptions, LOCKMODE lockmode, bool check,
							bool is_part_child, bool is_part_parent)
{
	Relation	rel;

	rel = table_open(relOid, lockmode);

	/* create_toast_table does all the work */
	(void) create_toast_table(rel, InvalidOid, InvalidOid, reloptions, lockmode, check,
							  is_part_child, is_part_parent);

	table_close(rel, NoLock);
}

/*
 * Create a toast table during bootstrap
 *
 * Here we need to prespecify the OIDs of the toast table and its index
 */
void
BootstrapToastTable(char *relName, Oid toastOid, Oid toastIndexOid)
{
	Relation	rel;

	rel = table_openrv(makeRangeVar(NULL, relName, -1), AccessExclusiveLock);

	if (rel->rd_rel->relkind != RELKIND_RELATION &&
		rel->rd_rel->relkind != RELKIND_MATVIEW)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("\"%s\" is not a table or materialized view",
						relName)));

	/* create_toast_table does all the work */
	if (!create_toast_table(rel, toastOid, toastIndexOid, (Datum) 0,
							AccessExclusiveLock, false,
							false, false))
		elog(ERROR, "\"%s\" does not require a toast table",
			 relName);

	table_close(rel, NoLock);
}


/*
 * create_toast_table --- internal workhorse
 *
 * rel is already opened and locked
 * toastOid and toastIndexOid are normally InvalidOid, but during
 * bootstrap they can be nonzero to specify hand-assigned OIDs
 */
static bool
create_toast_table(Relation rel, Oid toastOid, Oid toastIndexOid,
				   Datum reloptions, LOCKMODE lockmode, bool check,
				   bool is_part_child, bool is_part_parent)
{
	Oid			relOid = RelationGetRelid(rel);
	HeapTuple	reltup;
	TupleDesc	tupdesc;
	bool		shared_relation;
	bool		mapped_relation;
	Relation	toast_rel;
	Relation	class_rel;
	Oid			toast_relid;
	Oid			toast_idxid;
	Oid			toast_typid = InvalidOid;
	Oid			namespaceid;
	char		toast_relname[NAMEDATALEN];
	char		toast_idxname[NAMEDATALEN];
	IndexInfo  *indexInfo;
	Oid			collationObjectId[2];
	Oid			classObjectId[2];
	int16		coloptions[2];
	ObjectAddress baseobject,
				toastobject;

	/*
	 * Is it already toasted?
	 */
	if (rel->rd_rel->reltoastrelid != InvalidOid)
		return false;

	/*
	 * Check to see whether the table actually needs a TOAST table.
	 */
	if (!IsBinaryUpgrade)
	{
		/* Normal mode, normal check */
		if (!needs_toast_table(rel))
			return false;
	}
	else
	{
		/*
		 * In binary-upgrade mode, create a TOAST table if and only if
		 * pg_upgrade told us to (ie, a TOAST table OID has been provided).
		 *
		 * This indicates that the old cluster had a TOAST table for the
		 * current table.  We must create a TOAST table to receive the old
		 * TOAST file, even if the table seems not to need one.
		 *
		 * Contrariwise, if the old cluster did not have a TOAST table, we
		 * should be able to get along without one even if the new version's
		 * needs_toast_table rules suggest we should have one.  There is a lot
		 * of daylight between where we will create a TOAST table and where
		 * one is really necessary to avoid failures, so small cross-version
		 * differences in the when-to-create heuristic shouldn't be a problem.
		 * If we tried to create a TOAST table anyway, we would have the
		 * problem that it might take up an OID that will conflict with some
		 * old-cluster table we haven't seen yet.
		 */
		/*
		 * In Greenplum, partitioned tables are created in a single CREATE
		 * TABLE statement instead of each member table individually. The
		 * Oid preassignments are all done before the CREATE TABLE, so we
		 * can't use and reset a single oid variable, but instead we use them
		 * as a reference counter. Await the actuall preassign all before we
		 * decide whether to require a toast table or not.
		 *
		 * if (!OidIsValid(binary_upgrade_next_toast_pg_class_oid))
		 *	!OidIsValid(binary_upgrade_next_toast_pg_type_oid))
		 *	return false;
		 */
	}

	/*
	 * If requested check lockmode is sufficient. This is a cross check in
	 * case of errors or conflicting decisions in earlier code.
	 */
	if (check && lockmode != AccessExclusiveLock)
		elog(ERROR, "AccessExclusiveLock required to add toast table.");

	/*
	 * Create the toast table and its index
	 */
	snprintf(toast_relname, sizeof(toast_relname),
			 "pg_toast_%u", relOid);
	snprintf(toast_idxname, sizeof(toast_idxname),
			 "pg_toast_%u_index", relOid);

	/* this is pretty painful...  need a tuple descriptor */
	tupdesc = CreateTemplateTupleDesc(3);
	TupleDescInitEntry(tupdesc, (AttrNumber) 1,
					   "chunk_id",
					   OIDOID,
					   -1, 0);
	TupleDescInitEntry(tupdesc, (AttrNumber) 2,
					   "chunk_seq",
					   INT4OID,
					   -1, 0);
	TupleDescInitEntry(tupdesc, (AttrNumber) 3,
					   "chunk_data",
					   BYTEAOID,
					   -1, 0);

	/*
	 * Ensure that the toast table doesn't itself get toasted, or we'll be
	 * toast :-(.  This is essential for chunk_data because type bytea is
	 * toastable; hit the other two just to be sure.
	 */
	TupleDescAttr(tupdesc, 0)->attstorage = 'p';
	TupleDescAttr(tupdesc, 1)->attstorage = 'p';
	TupleDescAttr(tupdesc, 2)->attstorage = 'p';

	/*
	 * Toast tables for regular relations go in pg_toast; those for temp
	 * relations go into the per-backend temp-toast-table namespace.
	 */
	if (isTempOrTempToastNamespace(rel->rd_rel->relnamespace))
		namespaceid = GetTempToastNamespace();
	else
		namespaceid = PG_TOAST_NAMESPACE;

	/*
	 * Use binary-upgrade override for pg_type.oid, if supplied.  We might be
	 * in the post-schema-restore phase where we are doing ALTER TABLE to
	 * create TOAST tables that didn't exist in the old cluster.
	 */
	if (IsBinaryUpgrade)
	{
		toastOid = GetPreassignedOidForRelation(namespaceid, toast_relname);
		if (!OidIsValid(toastOid))
			return false;
		toast_typid = GetPreassignedOidForType(namespaceid, toast_relname, true);
	}

	/* Toast table is shared if and only if its parent is. */
	shared_relation = rel->rd_rel->relisshared;

	/* It's mapped if and only if its parent is, too */
	mapped_relation = RelationIsMapped(rel);

	toast_relid = heap_create_with_catalog(toast_relname,
										   namespaceid,
										   rel->rd_rel->reltablespace,
										   toastOid,
										   toast_typid,
										   InvalidOid,
										   rel->rd_rel->relowner,
										   rel->rd_rel->relam,
										   tupdesc,
										   NIL,
										   RELKIND_TOASTVALUE,
										   rel->rd_rel->relpersistence,
										   RELSTORAGE_HEAP,
										   shared_relation,
										   mapped_relation,
										   ONCOMMIT_NOOP,
										   NULL, /* CDB POLICY */
										   reloptions,
										   false,
										   true,
										   true,
										   InvalidOid,
										   NULL,
										   /* valid_opts */ false);
	Assert(toast_relid != InvalidOid);

	/* make the toast relation visible, else table_open will fail */
	CommandCounterIncrement();

	/* ShareLock is not really needed here, but take it anyway */
	toast_rel = table_open(toast_relid, ShareLock);

	/*
	 * Create unique index on chunk_id, chunk_seq.
	 *
	 * NOTE: the normal TOAST access routines could actually function with a
	 * single-column index on chunk_id only. However, the slice access
	 * routines use both columns for faster access to an individual chunk. In
	 * addition, we want it to be unique as a check against the possibility of
	 * duplicate TOAST chunk OIDs. The index might also be a little more
	 * efficient this way, since btree isn't all that happy with large numbers
	 * of equal keys.
	 */

	indexInfo = makeNode(IndexInfo);
	indexInfo->ii_NumIndexAttrs = 2;
	indexInfo->ii_NumIndexKeyAttrs = 2;
	indexInfo->ii_IndexAttrNumbers[0] = 1;
	indexInfo->ii_IndexAttrNumbers[1] = 2;
	indexInfo->ii_Expressions = NIL;
	indexInfo->ii_ExpressionsState = NIL;
	indexInfo->ii_Predicate = NIL;
	indexInfo->ii_PredicateState = NULL;
	indexInfo->ii_ExclusionOps = NULL;
	indexInfo->ii_ExclusionProcs = NULL;
	indexInfo->ii_ExclusionStrats = NULL;
	indexInfo->ii_Unique = true;
	indexInfo->ii_ReadyForInserts = true;
	indexInfo->ii_Concurrent = false;
	indexInfo->ii_BrokenHotChain = false;
	indexInfo->ii_ParallelWorkers = 0;
	indexInfo->ii_Am = BTREE_AM_OID;
	indexInfo->ii_AmCache = NULL;
	indexInfo->ii_Context = CurrentMemoryContext;

	collationObjectId[0] = InvalidOid;
	collationObjectId[1] = InvalidOid;

	classObjectId[0] = OID_BTREE_OPS_OID;
	classObjectId[1] = INT4_BTREE_OPS_OID;

	coloptions[0] = 0;
	coloptions[1] = 0;

	if (IsBinaryUpgrade)
		toastIndexOid = GetPreassignedOidForRelation(namespaceid, toast_idxname);

	toast_idxid = index_create(toast_rel, toast_idxname, toastIndexOid, InvalidOid,
				 InvalidOid, InvalidOid,
				 indexInfo,
				 list_make2("chunk_id", "chunk_seq"),
				 BTREE_AM_OID,
				 rel->rd_rel->reltablespace,
				 collationObjectId, classObjectId, coloptions, (Datum) 0,
				 INDEX_CREATE_IS_PRIMARY, 0, true, true, NULL);

	table_close(toast_rel, NoLock);

	/*
	 * If this is a partitioned child, we can unlock since the master is
	 * already locked.
	 */
	if (is_part_child)
	{
		UnlockRelationOid(toast_relid, ShareLock);
		UnlockRelationOid(toast_idxid, AccessExclusiveLock);
	}

	/*
	 * Store the toast table's OID in the parent relation's pg_class row
	 */
	class_rel = table_open(RelationRelationId, RowExclusiveLock);

	reltup = SearchSysCacheCopy1(RELOID, ObjectIdGetDatum(relOid));
	if (!HeapTupleIsValid(reltup))
		elog(ERROR, "cache lookup failed for relation %u", relOid);

	((Form_pg_class) GETSTRUCT(reltup))->reltoastrelid = toast_relid;

	if (!IsBootstrapProcessingMode())
	{
		/* normal case, use a transactional update */
		CatalogTupleUpdate(class_rel, &reltup->t_self, reltup);
	}
	else
	{
		/* While bootstrapping, we cannot UPDATE, so overwrite in-place */
		heap_inplace_update(class_rel, reltup);
	}

	heap_freetuple(reltup);

	table_close(class_rel, RowExclusiveLock);

	/*
	 * Register dependency from the toast table to the master, so that the
	 * toast table will be deleted if the master is.  Skip this in bootstrap
	 * mode.
	 */
	if (!IsBootstrapProcessingMode())
	{
		baseobject.classId = RelationRelationId;
		baseobject.objectId = relOid;
		baseobject.objectSubId = 0;
		toastobject.classId = RelationRelationId;
		toastobject.objectId = toast_relid;
		toastobject.objectSubId = 0;

		recordDependencyOn(&toastobject, &baseobject, DEPENDENCY_INTERNAL);
	}

	/*
	 * Make changes visible
	 */
	CommandCounterIncrement();

	return true;
}

/*
 * Check to see whether the table needs a TOAST table.
 */
static bool
needs_toast_table(Relation rel)
{
	/*
	 * No need to create a TOAST table for partitioned tables.
	 */
	if (rel->rd_rel->relkind == RELKIND_PARTITIONED_TABLE)
		return false;

	/*
	 * We cannot allow toasting a shared relation after initdb (because
	 * there's no way to mark it toasted in other databases' pg_class).
	 */
	if (rel->rd_rel->relisshared && !IsBootstrapProcessingMode())
		return false;

	/*
	 * Ignore attempts to create toast tables on catalog tables after initdb.
	 * Which catalogs get toast tables is explicitly chosen in
	 * catalog/toasting.h.  (We could get here via some ALTER TABLE command if
	 * the catalog doesn't have a toast table.)
	 */
	if (IsCatalogRelation(rel) && !IsBootstrapProcessingMode())
		return false;

	/* Otherwise, let the AM decide. */
	return table_relation_needs_toast_table(rel);
}
