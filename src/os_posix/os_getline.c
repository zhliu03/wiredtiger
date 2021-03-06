/*-
 * Copyright (c) 2008-2012 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

#include "wt_internal.h"

/*
 * __wt_getline --
 *	Get a line from a stream.
 *
 * Implementation of the POSIX getline or BSD fgetln functions (finding the
 * function in a portable way is hard, it's simple enough to write it instead).
 *
 * Note: Unlike the standard getline calls, this function doesn't include the
 * trailing newline character in the returned buffer and discards empty lines
 * (so the caller's EOF marker is a returned line length of 0).
 */
int
__wt_getline(WT_SESSION_IMPL *session, WT_ITEM *buf, FILE *fp)
{
	int c;

	/*
	 * We always NUL-terminate the returned string (even if it's empty),
	 * make sure there's buffer space for a trailing NUL in all cases.
	 */
	WT_RET(__wt_buf_init(session, buf, 10));

	while ((c = fgetc(fp)) != EOF) {
		/* Leave space for a trailing NUL. */
		if (buf->size + 1 >= buf->memsize)
			WT_RET(__wt_buf_grow(session, buf, buf->size + 1024));
		if (c == '\n') {
			if (buf->size == 0)
				continue;
			break;
		}
		((char *)buf->data)[buf->size++] = (char)c;
	}
	if (c == EOF && ferror(fp))
		WT_RET_MSG(session, __wt_errno(), "file read");

	((char *)buf->data)[buf->size] = '\0';

	return (0);
}
