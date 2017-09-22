/*
 * psql - the PostgreSQL interactive terminal
 *
 * Copyright (c) 2000-2010, PostgreSQL Global Development Group
 *
<<<<<<< HEAD
 * src/bin/psql/command.h
=======
 * $PostgreSQL: pgsql/src/bin/psql/command.h,v 1.31 2008/07/01 00:08:18 momjian Exp $
>>>>>>> 49f001d81e
 */
#ifndef COMMAND_H
#define COMMAND_H

#include "print.h"
#include "psqlscan.h"


typedef enum _backslashResult
{
	PSQL_CMD_UNKNOWN = 0,		/* not done parsing yet (internal only) */
	PSQL_CMD_SEND,				/* query complete; send off */
	PSQL_CMD_SKIP_LINE,			/* keep building query */
	PSQL_CMD_TERMINATE,			/* quit program */
	PSQL_CMD_NEWEDIT,			/* query buffer was changed (e.g., via \e) */
	PSQL_CMD_ERROR				/* the execution of the backslash command
								 * resulted in an error */
} backslashResult;


extern backslashResult HandleSlashCmds(PsqlScanState scan_state,
				PQExpBuffer query_buf);

extern int	process_file(char *filename, bool single_txn);

extern bool do_pset(const char *param,
		const char *value,
		printQueryOpt *popt,
		bool quiet);

<<<<<<< HEAD
extern void connection_warnings(bool in_startup);
=======
extern void connection_warnings(void);
>>>>>>> 49f001d81e

extern void SyncVariables(void);

extern void UnsyncVariables(void);

#endif   /* COMMAND_H */
