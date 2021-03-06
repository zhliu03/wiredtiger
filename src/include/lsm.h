/*-
 * Copyright (c) 2008-2012 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

/*
 * WT_CURSOR_LSM --
 *	An LSM cursor.
 */
struct __wt_cursor_lsm {
	WT_CURSOR iface;

	WT_LSM_TREE *lsm_tree;
	uint64_t dsk_gen;

	int nchunks;
	WT_BLOOM **blooms;
	WT_CURSOR **cursors;
	WT_CURSOR *current;     	/* The current cursor for iteration */

	WT_LSM_CHUNK *primary_chunk;	/* The current primary chunk. */

#define	WT_CLSM_ITERATE_NEXT    0x01    /* Forward iteration */
#define	WT_CLSM_ITERATE_PREV    0x02    /* Backward iteration */
#define	WT_CLSM_MERGE           0x04    /* Merge cursor, don't update. */
#define	WT_CLSM_MINOR_MERGE	0x08    /* Minor merge, include tombstones. */
#define	WT_CLSM_MULTIPLE        0x10    /* Multiple cursors have values for the
					   current key */
#define	WT_CLSM_UPDATED		0x20    /* Cursor has done updates */
	uint32_t flags;
};

/*
 * WT_LSM_CHUNK --
 *	A single chunk (file) in an LSM tree.
 */
struct __wt_lsm_chunk {
	const char *uri;		/* Data source for this chunk. */
	const char *bloom_uri;		/* URI of Bloom filter, if any. */
	uint64_t count;			/* Approximate count of records. */
	uint32_t generation;		/* Merge generation. */

	uint32_t ncursor;		/* Cursors with the chunk as primary. */
#define	WT_LSM_CHUNK_ONDISK	0x01
#define	WT_LSM_CHUNK_BLOOM	0x02
	uint32_t flags;
};

/*
 * WT_LSM_TREE --
 *	An LSM tree.
 */
struct __wt_lsm_tree {
	const char *name, *config, *filename;
	const char *key_format, *value_format;
	const char *bloom_config, *file_config;

	WT_COLLATOR *collator;

	int refcnt;			/* Number of users of the tree. */
	WT_RWLOCK *rwlock;
	TAILQ_ENTRY(__wt_lsm_tree) q;

	WT_LSM_STATS *stats;		/* LSM statistics */

	WT_SPINLOCK lock;
	uint64_t dsk_gen;
	uint32_t *memsizep;

	/* Configuration parameters */
	uint32_t bloom_bit_count;
	uint32_t bloom_hash_count;
	uint32_t chunk_size;
	uint32_t merge_max;
#define	WT_LSM_BLOOM_MERGED				0x00000001
#define	WT_LSM_BLOOM_NEWEST				0x00000002
#define	WT_LSM_BLOOM_OFF				0x00000004
#define	WT_LSM_BLOOM_OLDEST				0x00000008
	uint32_t bloom;			/* Bloom creation policy. */

	WT_SESSION_IMPL *worker_session;/* Passed to thread_create */
	pthread_t worker_tid;		/* LSM worker thread */
	WT_SESSION_IMPL *ckpt_session;	/* For checkpoint worker */
	pthread_t ckpt_tid;		/* LSM checkpoint worker thread */

	int nchunks;			/* Number of active chunks */
	int last;			/* Last allocated ID. */
	WT_LSM_CHUNK **chunk;		/* Array of active LSM chunks */
	size_t chunk_alloc;		/* Space allocated for chunks */

	WT_LSM_CHUNK **old_chunks;	/* Array of old LSM chunks */
	size_t old_alloc;		/* Space allocated for old chunks */
	int nold_chunks;		/* Number of old chunks */
	int old_avail;			/* Available old chunk slots */

#define	WT_LSM_TREE_WORKING	0x01
#define	WT_LSM_TREE_OPEN	0x02
	uint32_t flags;
};

/*
 * WT_LSM_DATA_SOURCE --
 *	Implementation of the WT_DATA_SOURCE interface for LSM.
 */
struct __wt_lsm_data_source {
	WT_DATA_SOURCE iface;

	WT_RWLOCK *rwlock;
};

/*
 * WT_LSM_WORKER_COOKIE --
 *	State for an LSM worker thread.
 */
struct __wt_lsm_worker_cookie {
    WT_LSM_CHUNK **chunk_array;
    size_t chunk_alloc;
    int nchunks;
};
