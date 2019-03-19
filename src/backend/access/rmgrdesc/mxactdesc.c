/*-------------------------------------------------------------------------
 *
 * mxactdesc.c
 *	  rmgr descriptor routines for access/transam/multixact.c
 *
 * Portions Copyright (c) 1996-2015, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/access/rmgrdesc/mxactdesc.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/multixact.h"

static void
out_member(StringInfo buf, MultiXactMember *member)
{
	appendStringInfo(buf, "%u ", member->xid);
	switch (member->status)
	{
		case MultiXactStatusForKeyShare:
			appendStringInfoString(buf, "(keysh) ");
			break;
		case MultiXactStatusForShare:
			appendStringInfoString(buf, "(sh) ");
			break;
		case MultiXactStatusForNoKeyUpdate:
			appendStringInfoString(buf, "(fornokeyupd) ");
			break;
		case MultiXactStatusForUpdate:
			appendStringInfoString(buf, "(forupd) ");
			break;
		case MultiXactStatusNoKeyUpdate:
			appendStringInfoString(buf, "(nokeyupd) ");
			break;
		case MultiXactStatusUpdate:
			appendStringInfoString(buf, "(upd) ");
			break;
		default:
			appendStringInfoString(buf, "(unk) ");
			break;
	}
}

void
<<<<<<< HEAD
multixact_desc(StringInfo buf, XLogRecord *record)
{
	uint8		info = record->xl_info & ~XLR_INFO_MASK;
	char		*rec = XLogRecGetData(record);
=======
multixact_desc(StringInfo buf, XLogReaderState *record)
{
	char	   *rec = XLogRecGetData(record);
	uint8		info = XLogRecGetInfo(record) & ~XLR_INFO_MASK;
>>>>>>> ab93f90cd3a4fcdd891cee9478941c3cc65795b8

	if (info == XLOG_MULTIXACT_ZERO_OFF_PAGE ||
		info == XLOG_MULTIXACT_ZERO_MEM_PAGE)
	{
		int			pageno;

		memcpy(&pageno, rec, sizeof(int));
		appendStringInfo(buf, "%d", pageno);
	}
	else if (info == XLOG_MULTIXACT_CREATE_ID)
	{
		xl_multixact_create *xlrec = (xl_multixact_create *) rec;
		int			i;

		appendStringInfo(buf, "%u offset %u nmembers %d: ", xlrec->mid,
						 xlrec->moff, xlrec->nmembers);
		for (i = 0; i < xlrec->nmembers; i++)
			out_member(buf, &xlrec->members[i]);
	}
}

const char *
multixact_identify(uint8 info)
{
	const char *id = NULL;

	switch (info & ~XLR_INFO_MASK)
	{
		case XLOG_MULTIXACT_ZERO_OFF_PAGE:
			id = "ZERO_OFF_PAGE";
			break;
		case XLOG_MULTIXACT_ZERO_MEM_PAGE:
			id = "ZERO_MEM_PAGE";
			break;
		case XLOG_MULTIXACT_CREATE_ID:
			id = "CREATE_ID";
			break;
	}

	return id;
}
