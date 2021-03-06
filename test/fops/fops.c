/*-
 * Copyright (c) 2008-2012 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

#include "thread.h"

static void *fop(void *);
static void  print_stats(u_int);

typedef struct {
	int bulk;				/* bulk load */
	int ckpt;				/* session.checkpoint */
	int create;				/* session.create */
	int cursor;				/* session.open_cursor */
	int drop;				/* session.drop */
	int upgrade;				/* session.upgrade */
	int verify;				/* session.verify */
} STATS;

static STATS *run_stats;

/*
 * r --
 *	Return a 32-bit pseudo-random number.
 *
 * This is an implementation of George Marsaglia's multiply-with-carry pseudo-
 * random number generator.  Computationally fast, with reasonable randomness
 * properties.
 */
static inline uint32_t
r(void)
{
	static uint32_t m_w = 0, m_z = 0;

	if (m_w == 0) {
		struct timeval t;
		(void)gettimeofday(&t, NULL);
		m_w = (uint32_t)t.tv_sec;
		m_z = (uint32_t)t.tv_usec;
	}

	m_z = 36969 * (m_z & 65535) + (m_z >> 16);
	m_w = 18000 * (m_w & 65535) + (m_w >> 16);
	return (m_z << 16) + (m_w & 65535);
}

int
fop_start(u_int nthreads)
{
	struct timeval start, stop;
	double seconds;
	pthread_t *tids;
	u_int i;
	int ret;
	void *thread_ret;

	/* Create statistics and thread structures. */
	if ((run_stats = calloc(
	    (size_t)(nthreads), sizeof(*run_stats))) == NULL ||
	    (tids = calloc((size_t)(nthreads), sizeof(*tids))) == NULL)
		die("calloc", errno);

	(void)gettimeofday(&start, NULL);

	/* Create threads. */
	for (i = 0; i < nthreads; ++i)
		if ((ret = pthread_create(
		    &tids[i], NULL, fop, (void *)(uintptr_t)i)) != 0)
			die("pthread_create", ret);

	/* Wait for the threads. */
	for (i = 0; i < nthreads; ++i)
		(void)pthread_join(tids[i], &thread_ret);

	(void)gettimeofday(&stop, NULL);
	seconds = (stop.tv_sec - start.tv_sec) +
	    (stop.tv_usec - start.tv_usec) * 1e-6;

	print_stats(nthreads);
	printf("timer: %.2lf seconds (%d ops/second)\n",
	    seconds, (int)((nthreads * nops) / seconds));

	free(run_stats);
	free(tids);

	return (0);
}

/*
 * fop --
 *	File operation function.
 */
static void *
fop(void *arg)
{
	STATS *s;
	u_int i;
	int id;

	id = (int)(uintptr_t)arg;
	sched_yield();		/* Get all the threads created. */

	s = &run_stats[id];

	for (i = 0; i < nops; ++i, sched_yield())
		switch (r() % 7) {
		case 0:
			++s->bulk;
			obj_bulk();
			break;
		case 1:
			++s->create;
			obj_create();
			break;
		case 2:
			++s->cursor;
			obj_cursor();
			break;
		case 3:
			++s->drop;
			obj_drop();
			break;
		case 4:
			++s->ckpt;
			obj_checkpoint();
			break;
		case 5:
			++s->upgrade;
			obj_upgrade();
			break;
		case 6:
			++s->verify;
			obj_verify();
			break;
		default:
			break;
		}

	return (NULL);
}

/*
 * print_stats --
 *	Display file operation thread stats.
 */
static void
print_stats(u_int nthreads)
{
	STATS *s;
	u_int id;

	s = run_stats;
	for (id = 0; id < nthreads; ++id, ++s)
		printf(
		    "%2d: bulk %3d, ckpt %3d, create %3d, cursor %3d, "
		    "drop %3d, upg %3d, vrfy %3d\n",
		    id, s->bulk, s->ckpt, s->create, s->cursor,
		    s->drop, s->upgrade, s->verify);
}
