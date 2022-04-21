/*
 * pg_strom.h
 *
 * Header file of pg_strom module
 * --
 * Copyright 2011-2021 (C) KaiGai Kohei <kaigai@kaigai.gr.jp>
 * Copyright 2014-2021 (C) PG-Strom Developers Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the PostgreSQL License.
 */
#ifndef PG_STROM_H
#define PG_STROM_H

#include "postgres.h"
#if PG_VERSION_NUM < 140000
#error Base PostgreSQL version must be v14 or later
#endif
#define PG_MAJOR_VERSION		(PG_VERSION_NUM / 100)
#define PG_MINOR_VERSION		(PG_VERSION_NUM % 100)

#include "access/genam.h"
#include "access/table.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/objectaccess.h"
#include "catalog/pg_depend.h"
#include "catalog/pg_extension.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_type.h"
#include "commands/typecmds.h"
#include "common/hashfn.h"
#include "common/int.h"
#include "funcapi.h"
#include "libpq/pqformat.h"
#include "lib/stringinfo.h"
#include "miscadmin.h"
#include "postmaster/postmaster.h"
#include "storage/ipc.h"
#include "storage/fd.h"
#include "storage/shmem.h"
#include "utils/builtins.h"
#include "utils/cash.h"
#include "utils/date.h"
#include "utils/datetime.h"
#include "utils/float.h"
#include "utils/fmgroids.h"
#include "utils/guc.h"
#include "utils/inet.h"
#include "utils/jsonb.h"
#include "utils/rangetypes.h"
#include "utils/rel.h"
#include "utils/resowner.h"
#include "utils/syscache.h"
#include "utils/timestamp.h"
#include "utils/typcache.h"
#include <assert.h>
#include <cuda.h>
#include <limits.h>
#include <sys/mman.h>
#include <unistd.h>
#include "pg_utils.h"
#include "heterodb_extra.h"

/* ------------------------------------------------
 *
 * Global Type Definitions
 *
 * ------------------------------------------------
 */
typedef struct GpuDevAttributes
{
	int32		NUMA_NODE_ID;
	int32		DEV_ID;
	char		DEV_NAME[256];
	char		DEV_BRAND[16];
	char		DEV_UUID[48];
	size_t		DEV_TOTAL_MEMSZ;
	size_t		DEV_BAR1_MEMSZ;
	bool		DEV_SUPPORT_GPUDIRECTSQL;
#define DEV_ATTR(LABEL,a,b,c)	\
	int32		LABEL;
#include "gpu_devattrs.h"
#undef DEV_ATTR
} GpuDevAttributes;

extern GpuDevAttributes *gpuDevAttrs;
extern int		numGpuDevAttrs;
#define GPUKERNEL_MAX_SM_MULTIPLICITY	4

/*
 * devtype/devfunc/devcast definitions
 */
#define DEVKERNEL__NVIDIA_GPU		0x0001U		/* CUDA-based GPU */
#define DEVKERNEL__NVIDIA_DPU		0x0002U		/* BlueField-X DPU */
#define DEVKERNEL__ARMv8_SPU		0x0004U		/* ARMv8-based SPU */
#define DEVKERNEL__ANY				0x0007U
struct devtype_info;
struct devfunc_info;
struct devcast_info;

typedef uint32 (*devtype_hashfunc_f)(struct devtype_info *dtype, Datum datum);

typedef struct devtype_info
{
	dlist_node	chain;
	uint32		hashvalue;
	Oid			type_oid;
	uint32		type_flags;
	int16		type_length;
	int16		type_align;
	bool		type_byval;
	const char *type_name;
	/* oid of type related functions */
	Oid			type_eqfunc;
	Oid			type_cmpfunc;
	int			type_extra_sz;	/* extra buffer size */
	/* element type of array, if type is array */
	struct devtype_info *type_element;
	/* attribute of sub-fields, if type is composite */
	int			comp_nfields;
	struct devtype_info *comp_subtypes[FLEXIBLE_ARRAY_MEMBER];
} devtype_info;









/*
 * Global variables
 */
extern long		PAGE_SIZE;
extern long		PAGE_MASK;
extern int		PAGE_SHIFT;
extern long		PHYS_PAGES;
#define PAGE_ALIGN(x)	TYPEALIGN(PAGE_SIZE,(x))

/*
 * extra.c
 */
extern void		pgstrom_init_extra(void);
extern int		gpuDirectInitDriver(void);
extern void		gpuDirectFileDescOpen(GPUDirectFileDesc *gds_fdesc,
									  File pg_fdesc);
extern void		gpuDirectFileDescOpenByPath(GPUDirectFileDesc *gds_fdesc,
											const char *pathname);
extern void		gpuDirectFileDescClose(const GPUDirectFileDesc *gds_fdesc);
extern CUresult	gpuDirectMapGpuMemory(CUdeviceptr m_segment,
									  size_t m_segment_sz,
									  unsigned long *p_iomap_handle);
extern CUresult	gpuDirectUnmapGpuMemory(CUdeviceptr m_segment,
										unsigned long iomap_handle);
extern bool		gpuDirectFileReadIOV(const GPUDirectFileDesc *gds_fdesc,
									 CUdeviceptr m_segment,
									 unsigned long iomap_handle,
									 off_t m_offset,
									 strom_io_vector *iovec);
extern void		extraSysfsSetupDistanceMap(const char *manual_config);
extern Bitmapset *extraSysfsLookupOptimalGpus(int fdesc);
extern ssize_t	extraSysfsPrintNvmeInfo(int index, char *buffer, ssize_t buffer_sz);

/*
 * shmbuf.c
 */
extern void	   *shmbufAlloc(size_t sz);
extern void	   *shmbufAllocZero(size_t sz);
extern void		shmbufFree(void *addr);
extern void		pgstrom_init_shmbuf(void);

/*
 * codegen.c
 */
extern devtype_info *pgstrom_devtype_lookup(Oid type_oid);

extern void		pgstrom_init_codegen(void);

/*
 * gpu_device.c
 */
extern bool		pgstrom_gpudirect_enabled(void);
extern Size		pgstrom_gpudirect_threshold(void);
extern CUresult	gpuOptimalBlockSize(int *p_grid_sz,
									int *p_block_sz,
									CUfunction kern_function,
									CUdevice cuda_device,
									size_t dyn_shmem_per_block,
									size_t dyn_shmem_per_thread);
extern bool		pgstrom_init_gpu_device(void);

/*
 * apache arrow related stuff
 */


/*
 * misc.c
 */
extern Oid		get_object_extension_oid(Oid class_id,
										 Oid object_id,
										 int32 objsub_id,
										 bool missing_ok);
extern ssize_t	__readFile(int fdesc, void *buffer, size_t nbytes);
extern ssize_t	__preadFile(int fdesc, void *buffer, size_t nbytes, off_t f_pos);
extern ssize_t	__writeFile(int fdesc, const void *buffer, size_t nbytes);
extern ssize_t	__pwriteFile(int fdesc, const void *buffer, size_t nbytes, off_t f_pos);
extern void	   *__mmapFile(void *addr, size_t length,
						   int prot, int flags, int fdesc, off_t offset);
extern int		__munmapFile(void *mmap_addr);
extern void	   *__mremapFile(void *mmap_addr, size_t new_size);

/*
 * main.c
 */
extern void		_PG_init(void);


#endif	/* PG_STROM_H */
