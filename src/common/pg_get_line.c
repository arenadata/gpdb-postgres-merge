/*-------------------------------------------------------------------------
 *
 * pg_get_line.c
 *	  fgets() with an expansible result buffer
 *
 * Portions Copyright (c) 1996-2020, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/common/pg_get_line.c
 *
 *-------------------------------------------------------------------------
 */
#ifndef FRONTEND
#include "postgres.h"
#else
#include "postgres_fe.h"
#endif

#include "common/string.h"
#include "lib/stringinfo.h"

/*
 * pg_get_line_append()
 *
 * This has similar behavior to pg_get_line(), and thence to fgets(),
 * except that the collected data is appended to whatever is in *buf.
 *
 * Returns true if a line was successfully collected (including the
 * case of a non-newline-terminated line at EOF).  Returns false if
 * there was an I/O error or no data was available before EOF.
 * (Check ferror(stream) to distinguish these cases.)
 *
 * In the false-result case, the contents of *buf are logically unmodified,
 * though it's possible that the buffer has been resized.
 */
bool
pg_get_line_append(FILE *stream, StringInfo buf)
{
	int			orig_len = buf->len;

	/* Read some data, appending it to whatever we already have */
	while (fgets(buf->data + buf->len, buf->maxlen - buf->len, stream) != NULL)
	{
		buf->len += strlen(buf->data + buf->len);

		/* Done if we have collected a newline */
		if (buf->len > orig_len && buf->data[buf->len - 1] == '\n')
			return true;

		/* Make some more room in the buffer, and loop to read more data */
		enlargeStringInfo(buf, 128);
	}

	/* Check for I/O errors and EOF */
	if (ferror(stream) || buf->len == orig_len)
	{
		/* Discard any data we collected before detecting error */
		buf->len = orig_len;
		buf->data[orig_len] = '\0';
		return false;
	}

	/* No newline at EOF, but we did collect some data */
	return true;
 }