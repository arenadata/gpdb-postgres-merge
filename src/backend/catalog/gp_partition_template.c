/*-------------------------------------------------------------------------
 *
 * gp_partition_template.c
 *	  routines to support manipulation of the gp_partition_template relation
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/catalog/gp_partition_template.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/genam.h"
#include "access/heapam.h"
#include "access/htup_details.h"
#include "access/tableam.h"
#include "catalog/indexing.h"
#include "catalog/gp_partition_template.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"

void
StoreGpPartitionTemplate(Oid relid, int32 level,
						 GpPartitionDefinition *gpPartDef)
{
	Datum values[Natts_gp_partition_template];
	bool  nulls[Natts_gp_partition_template];
	HeapTuple tuple;
	Relation gp_template;

	/*
	 * GPDB_12_MERGE_FIXME: Currently, this function can be called multiple
	 * times for a level, hence need to perform this check. Can we avoid this
	 * check and still possibly avoid duplicate key errors.
	 */
	if (GetGpPartitionTemplate(relid, level) != NULL)
		return;

	gp_template = table_open(PartitionTemplateRelationId, RowExclusiveLock);
	memset(nulls, 0, sizeof(nulls));

	values[Anum_gp_partition_template_relid - 1] = relid;
	values[Anum_gp_partition_template_level - 1] = level;
	values[Anum_gp_partition_template_def - 1] = CStringGetTextDatum(nodeToString(gpPartDef));

	tuple = heap_form_tuple(RelationGetDescr(gp_template), values, nulls);

	CatalogTupleInsert(gp_template, tuple);
	heap_freetuple(tuple);

	table_close(gp_template, RowExclusiveLock);
}

GpPartitionDefinition *
GetGpPartitionTemplate(Oid relid, int32 level)
{
	Relation	gp_template;
	ScanKeyData key[2];
	SysScanDesc scan;
	HeapTuple	tuple;
	GpPartitionDefinition *def = NULL;

	gp_template = table_open(PartitionTemplateRelationId, RowExclusiveLock);
	ScanKeyInit(&key[0],
				Anum_gp_partition_template_relid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(relid));
	ScanKeyInit(&key[1],
				Anum_gp_partition_template_level,
				BTEqualStrategyNumber, F_OIDEQ,
				Int16GetDatum(level));

	scan = systable_beginscan(gp_template, GpPartitionTemplateRelidLevelIndexId,
							  true, NULL, 2, key);

	tuple = systable_getnext(scan);
	if (HeapTupleIsValid(tuple))
	{
		Datum       datum;
		bool        isnull;

		datum = heap_getattr(tuple, Anum_gp_partition_template_def,
							 RelationGetDescr(gp_template), &isnull);

		if (!isnull)
		{
			char *defStr = TextDatumGetCString(datum);
			def = stringToNode(defStr);
			pfree(defStr);
		}
	}

	systable_endscan(scan);
	table_close(gp_template, RowExclusiveLock);

	return def;
}

/*
 * Remove gp_patition_template entry for a relation
 */
void
RemoveGpPartitionTemplateByRelId(Oid relid)
{
	Relation	gp_template;
	ScanKeyData key;
	SysScanDesc scan;
	HeapTuple	tuple;

	gp_template = table_open(PartitionTemplateRelationId, RowExclusiveLock);
	ScanKeyInit(&key,
				Anum_gp_partition_template_relid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(relid));
	scan = systable_beginscan(gp_template, GpPartitionTemplateRelidLevelIndexId,
							  true, NULL, 1, &key);

	while (HeapTupleIsValid(tuple = systable_getnext(scan)))
		CatalogTupleDelete(gp_template, &tuple->t_self);

	systable_endscan(scan);
	table_close(gp_template, RowExclusiveLock);
}
