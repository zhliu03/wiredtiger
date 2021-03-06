/*-
 * Copyright (c) 2008-2012 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

#include "wt_internal.h"

/*
 * __wt_schema_project_in --
 *	Given list of cursors and a projection, read columns from the
 *	application into the dependent cursors.
 */
int
__wt_schema_project_in(WT_SESSION_IMPL *session,
    WT_CURSOR **cp, const char *proj_arg, va_list ap)
{
	WT_CURSOR *c;
	WT_ITEM *buf;
	WT_PACK pack;
	WT_PACK_VALUE pv;
	char *proj;
	uint8_t *p, *end;
	const uint8_t *next;
	size_t len, offset, old_len;
	uint32_t arg;

	WT_CLEAR(pack);		/* -Wuninitialized */
	buf = NULL;		/* -Wuninitialized */
	p = end = NULL;		/* -Wuninitialized */

	/* Reset any of the buffers we will be setting. */
	for (proj = (char *)proj_arg; *proj != '\0'; proj++) {
		arg = (uint32_t)strtoul(proj, &proj, 10);
		if (*proj == WT_PROJ_KEY) {
			c = cp[arg];
			WT_RET(__wt_buf_init(session, &c->key, 0));
		} else if (*proj == WT_PROJ_VALUE) {
			c = cp[arg];
			WT_RET(__wt_buf_init(session, &c->value, 0));
		}
	}

	for (proj = (char *)proj_arg; *proj != '\0'; proj++) {
		arg = (uint32_t)strtoul(proj, &proj, 10);

		switch (*proj) {
		case WT_PROJ_KEY:
			c = cp[arg];
			if (WT_CURSOR_RECNO(c)) {
				c->key.data = &c->recno;
				c->key.size = sizeof(c->recno);
				WT_RET(__pack_init(session, &pack, "R"));
			} else
				WT_RET(__pack_init(
				    session, &pack, c->key_format));
			buf = &c->key;
			p = (uint8_t *)buf->data;
			end = p + buf->size;
			continue;

		case WT_PROJ_VALUE:
			c = cp[arg];
			WT_RET(__pack_init(session, &pack, c->value_format));
			buf = &c->value;
			p = (uint8_t *)buf->data;
			end = p + buf->size;
			continue;
		}

		/*
		 * Otherwise, the argument is a count, where a missing
		 * count means a count of 1.
		 */
		for (arg = (arg == 0) ? 1 : arg; arg > 0; arg--) {
			switch (*proj) {
			case WT_PROJ_NEXT:
			case WT_PROJ_SKIP:
				WT_RET(__pack_next(&pack, &pv));
				if (*proj == WT_PROJ_SKIP) {
					/*
					 * A nasty case: if we are inserting
					 * out-of-order, we may reach the end
					 * of the data.  That's okay: we want
					 * to append in that case, and we're
					 * positioned to do that.
					 */
					if (p == end) {
						/* Set up an empty value. */
						WT_CLEAR(pv.u);
						if (pv.type == 'S' ||
						    pv.type == 's')
							pv.u.s = "";

						len = __pack_size(session, &pv);
						WT_RET(__wt_buf_grow(session,
						    buf, buf->size + len));
						p = (uint8_t *)buf->data +
						    buf->size;
						WT_RET(__pack_write(
						    session, &pv, &p, len));
						buf->size += WT_STORE_SIZE(len);
						end = (uint8_t *)buf->data +
						    buf->size;
					} else if (*proj == WT_PROJ_SKIP)
						WT_RET(__unpack_read(session,
						    &pv, (const uint8_t **)&p,
						    (size_t)(end - p)));

					break;
				}
				WT_PACK_GET(session, pv, ap);
				/* FALLTHROUGH */

			case WT_PROJ_REUSE:
				/* Read the item we're about to overwrite. */
				next = p;
				if (p < end)
					WT_RET(__unpack_read(session, &pv,
					    &next, (size_t)(end - p)));
				old_len = (size_t)(next - p);

				len = __pack_size(session, &pv);
				offset = WT_PTRDIFF(p, buf->data);
				WT_RET(__wt_buf_grow(session,
				    buf, buf->size + len));
				p = (uint8_t *)buf->data + offset;
				end = (uint8_t *)buf->data + buf->size + len;
				/* Make room if we're inserting out-of-order. */
				if (offset + old_len < buf->size)
					memmove(p + len, p + old_len,
					    buf->size - (offset + old_len));
				WT_RET(__pack_write(session, &pv, &p, len));
				buf->size += WT_STORE_SIZE(len);
				break;

			default:
				WT_RET_MSG(session, EINVAL,
				    "unexpected projection plan: %c",
				    (int)*proj);
			}
		}
	}

	return (0);
}

/*
 * __wt_schema_project_out --
 *	Given list of cursors and a projection, read columns from the
 *	dependent cursors and return them to the application.
 */
int
__wt_schema_project_out(WT_SESSION_IMPL *session,
    WT_CURSOR **cp, const char *proj_arg, va_list ap)
{
	WT_CURSOR *c;
	WT_PACK pack;
	WT_PACK_VALUE pv;
	char *proj;
	uint8_t *p, *end;
	uint32_t arg;

	WT_CLEAR(pack);		/* -Wuninitialized */
	WT_CLEAR(pv);		/* -Wuninitialized */
	p = end = NULL;		/* -Wuninitialized */

	for (proj = (char *)proj_arg; *proj != '\0'; proj++) {
		arg = (uint32_t)strtoul(proj, &proj, 10);

		switch (*proj) {
		case WT_PROJ_KEY:
			c = cp[arg];
			p = (uint8_t *)c->key.data;
			end = p + c->key.size;
			if (WT_CURSOR_RECNO(c)) {
				c->key.data = &c->recno;
				c->key.size = sizeof(c->recno);
				WT_RET(__pack_init(session, &pack, "R"));
			} else
				WT_RET(__pack_init(
				    session, &pack, c->key_format));
			continue;

		case WT_PROJ_VALUE:
			c = cp[arg];
			p = (uint8_t *)c->value.data;
			end = p + c->value.size;
			WT_RET(__pack_init(session, &pack, c->value_format));
			continue;
		}

		/*
		 * Otherwise, the argument is a count, where a missing
		 * count means a count of 1.
		 */
		for (arg = (arg == 0) ? 1 : arg; arg > 0; arg--) {
			switch (*proj) {
			case WT_PROJ_NEXT:
			case WT_PROJ_SKIP:
				WT_RET(__pack_next(&pack, &pv));
				WT_RET(__unpack_read(session, &pv,
				    (const uint8_t **)&p, (size_t)(end - p)));
				if (*proj == WT_PROJ_SKIP)
					break;
				WT_UNPACK_PUT(session, pv, ap);
				/* FALLTHROUGH */

			case WT_PROJ_REUSE:
				/* Don't copy out the same value twice. */
				break;
			}
		}
	}

	return (0);
}

/*
 * __wt_schema_project_slice --
 *	Given list of cursors and a projection, read columns from the
 *	a raw buffer.
 */
int
__wt_schema_project_slice(WT_SESSION_IMPL *session, WT_CURSOR **cp,
    const char *proj_arg, int key_only, const char *vformat, WT_ITEM *value)
{
	WT_CURSOR *c;
	WT_ITEM *buf;
	WT_PACK pack, vpack;
	WT_PACK_VALUE pv, vpv;
	char *proj;
	uint8_t *end, *p;
	const uint8_t *next, *vp, *vend;
	size_t len, offset, old_len;
	uint32_t arg;
	int skip;

	WT_CLEAR(pack);		/* -Wuninitialized */
	WT_CLEAR(vpv);		/* -Wuninitialized */
	buf = NULL;		/* -Wuninitialized */
	p = end = NULL;		/* -Wuninitialized */

	WT_RET(__pack_init(session, &vpack, vformat));
	vp = (uint8_t *)value->data;
	vend = vp + value->size;

	/* Reset any of the buffers we will be setting. */
	for (proj = (char *)proj_arg; *proj != '\0'; proj++) {
		arg = (uint32_t)strtoul(proj, &proj, 10);
		if (*proj == WT_PROJ_KEY) {
			c = cp[arg];
			WT_RET(__wt_buf_init(session, &c->key, 0));
		} else if (*proj == WT_PROJ_VALUE && !key_only) {
			c = cp[arg];
			WT_RET(__wt_buf_init(session, &c->value, 0));
		}
	}

	skip = key_only;
	for (proj = (char *)proj_arg; *proj != '\0'; proj++) {
		arg = (uint32_t)strtoul(proj, &proj, 10);

		switch (*proj) {
		case WT_PROJ_KEY:
			skip = 0;
			c = cp[arg];
			if (WT_CURSOR_RECNO(c)) {
				c->key.data = &c->recno;
				c->key.size = sizeof(c->recno);
				WT_RET(__pack_init(session, &pack, "R"));
			} else
				WT_RET(__pack_init(
				    session, &pack, c->key_format));
			buf = &c->key;
			p = (uint8_t *)buf->data;
			end = p + buf->size;
			continue;

		case WT_PROJ_VALUE:
			if ((skip = key_only) != 0)
				continue;
			c = cp[arg];
			WT_RET(__pack_init(session, &pack, c->value_format));
			buf = &c->value;
			p = (uint8_t *)buf->data;
			end = p + buf->size;
			continue;
		}

		/*
		 * Otherwise, the argument is a count, where a missing
		 * count means a count of 1.
		 */
		for (arg = (arg == 0) ? 1 : arg; arg > 0; arg--) {
			switch (*proj) {
			case WT_PROJ_NEXT:
			case WT_PROJ_SKIP:
				if (!skip) {
					WT_RET(__pack_next(&pack, &pv));

					/*
					 * A nasty case: if we are inserting
					 * out-of-order, append a zero value
					 * to keep the buffer in the correct
					 * format.
					 */
					if (*proj == WT_PROJ_SKIP &&
					    p == end) {
						/* Set up an empty value. */
						WT_CLEAR(pv.u);
						if (pv.type == 'S' ||
						    pv.type == 's')
							pv.u.s = "";

						len = __pack_size(session, &pv);
						WT_RET(__wt_buf_grow(session,
						    buf, buf->size + len));
						p = (uint8_t *)buf->data +
						    buf->size;
						WT_RET(__pack_write(
						    session, &pv, &p, len));
						end = p;
						buf->size += WT_STORE_SIZE(len);
					} else if (*proj == WT_PROJ_SKIP)
						WT_RET(__unpack_read(session,
						    &pv, (const uint8_t **)&p,
						    (size_t)(end - p)));
				}
				if (*proj == WT_PROJ_SKIP)
					break;
				WT_RET(__pack_next(&vpack, &vpv));
				WT_RET(__unpack_read(session, &vpv,
				    &vp, (size_t)(vend - vp)));
				/* FALLTHROUGH */
			case WT_PROJ_REUSE:
				if (skip)
					break;

				/* Read the item we're about to overwrite. */
				next = p;
				if (p < end)
					WT_RET(__unpack_read(session, &pv,
					    &next, (size_t)(end - p)));
				old_len = (size_t)(next - p);

				/*
				 * There is subtlety here: the value format
				 * may not exactly match the cursor's format.
				 * In particular, we need lengths with raw
				 * columns in the middle of a packed struct,
				 * but not if they are at the end of a column.
				 */
				pv.u = vpv.u;

				len = __pack_size(session, &pv);
				offset = WT_PTRDIFF(p, buf->data);
				WT_RET(__wt_buf_grow(session,
				    buf, buf->size + len - old_len));
				p = (uint8_t *)buf->data + offset;
				/* Make room if we're inserting out-of-order. */
				if (offset + old_len < buf->size)
					memmove(p + len, p + old_len,
					    buf->size - (offset + old_len));
				WT_RET(__pack_write(session, &pv, &p, len));
				buf->size += WT_STORE_SIZE(len - old_len);
				end = (uint8_t *)buf->data + buf->size;
				break;
			default:
				WT_RET_MSG(session, EINVAL,
				    "unexpected projection plan: %c",
				    (int)*proj);
			}
		}
	}

	return (0);
}

/*
 * __wt_schema_project_merge --
 *	Given list of cursors and a projection, build a buffer containing the
 *	column values read from the cursors.
 */
int
__wt_schema_project_merge(WT_SESSION_IMPL *session,
    WT_CURSOR **cp, const char *proj_arg, const char *vformat, WT_ITEM *value)
{
	WT_CURSOR *c;
	WT_ITEM *buf;
	WT_PACK pack, vpack;
	WT_PACK_VALUE pv, vpv;
	char *proj;
	uint8_t *p, *end, *vp;
	size_t len;
	uint32_t arg;

	WT_CLEAR(pack);		/* -Wuninitialized */
	WT_CLEAR(pv);		/* -Wuninitialized */
	WT_CLEAR(vpv);		/* -Wuninitialized */
	p = end = NULL;		/* -Wuninitialized */

	WT_RET(__wt_buf_init(session, value, 0));
	WT_RET(__pack_init(session, &vpack, vformat));

	for (proj = (char *)proj_arg; *proj != '\0'; proj++) {
		arg = (uint32_t)strtoul(proj, &proj, 10);

		switch (*proj) {
		case WT_PROJ_KEY:
			c = cp[arg];
			if (WT_CURSOR_RECNO(c)) {
				c->key.data = &c->recno;
				c->key.size = sizeof(c->recno);
				WT_RET(__pack_init(session, &pack, "R"));
			} else
				WT_RET(__pack_init(
				    session, &pack, c->key_format));
			buf = &c->key;
			p = (uint8_t *)buf->data;
			end = p + buf->size;
			continue;

		case WT_PROJ_VALUE:
			c = cp[arg];
			WT_RET(__pack_init(session, &pack, c->value_format));
			buf = &c->value;
			p = (uint8_t *)buf->data;
			end = p + buf->size;
			continue;
		}

		/*
		 * Otherwise, the argument is a count, where a missing
		 * count means a count of 1.
		 */
		for (arg = (arg == 0) ? 1 : arg; arg > 0; arg--) {
			switch (*proj) {
			case WT_PROJ_NEXT:
			case WT_PROJ_SKIP:
				WT_RET(__pack_next(&pack, &pv));
				WT_RET(__unpack_read(session, &pv,
				    (const uint8_t **)&p,
				    (size_t)(end - p)));
				if (*proj == WT_PROJ_SKIP)
					break;

				WT_RET(__pack_next(&vpack, &vpv));
				vpv.u = pv.u;
				len = __pack_size(session, &vpv);
				WT_RET(__wt_buf_grow(session,
				    value, value->size + len));
				vp = (uint8_t *)value->data + value->size;
				WT_RET(__pack_write(session, &vpv, &vp, len));
				value->size += WT_STORE_SIZE(len);
				/* FALLTHROUGH */

			case WT_PROJ_REUSE:
				/* Don't copy the same value twice. */
				break;
			}
		}
	}

	return (0);
}
