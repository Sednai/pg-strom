/*
 * xpu_common.h
 *
 * Common header portion for xPU(GPU/DPU/SPU) device code.
 * --
 * Copyright 2011-2021 (C) KaiGai Kohei <kaigai@kaigai.gr.jp>
 * Copyright 2014-2021 (C) PG-Strom Developers Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the PostgreSQL License.
 */
#ifndef XPU_COMMON_H
#define XPU_COMMON_H
/*
 * Definition of primitive types
 */
#include <stdint.h>
#include <stdbool.h>
#if defined(__CUDACC__)
typedef __half		float2_t;
#elif defined(HAVE_FLOAT2)
typedef _Float16	float2_t;
#else
typedef uint16_t	float2_t;
#endif

/*
 * Functions with qualifiers
 */
#ifdef __CUDACC__
#define INLINE_FUNCTION(RET_TYPE)				\
	__device__ __host__ __forceinline__			\
	static RET_TYPE __attribute__ ((unused))
#define STATIC_FUNCTION(RET_TYPE)				\
	__device__ __host__							\
	static RET_TYPE
#define PUBLIC_FUNCTION(RET_TYPE)				\
	__device__ __host__ RET_TYPE
#define KERNEL_FUNCTION(RET_TYPE)				\
	extern "C" __global__ RET_TYPE
#else
#define INLINE_FUNCTION(RET_TYPE)		static inline RET_TYPE
#define STATIC_FUNCTION(RET_TYPE)		static RET_TYPE
#define PUBLIC_FUNCTION(RET_TYPE)		RET_TYPE
#define KERNEL_FUNCTION(RET_TYPE)		RET_TYPE
#endif	/* __CUDACC__ */

/*
 * Several fundamental data types and macros
 */
#ifndef __STROM_HOST__
typedef uint32_t		Oid;
typedef uint64_t		Datum;
#define PointerGetDatum(X)		((Datum)(X))
#define DatumGetPointer(X)		((char *)(X))
#define NAMEDATALEN				64

#define TYPEALIGN(ALIGNVAL,LEN)         \
	(((uint64_t)(LEN) + ((ALIGNVAL) - 1)) & ~((uint64_t)((ALIGNVAL) - 1)))
#define TYPEALIGN_DOWN(ALIGNVAL,LEN)                        \
	(((uint64_t) (LEN)) & ~((uint64_t) ((ALIGNVAL) - 1)))

#define MAXIMUM_ALIGNOF		8
#define MAXALIGN(LEN)		TYPEALIGN(MAXIMUM_ALIGNOF,LEN)
#define MAXALIGN_DOWN(LEN)	TYPEALIGN_DOWN(MAXIMUM_ALIGNOF,LEN)
#endif
#define MAXIMUM_ALIGNOF_SHIFT 3

#ifdef __CUDACC__
template <typename T>
INLINE_FUNCTION(T)
__Fetch(const T *ptr)
{
	T	temp;

	memcpy(&temp, ptr, sizeof(T));

	return temp;
}
#else
#define __Fetch(PTR)		(*(PTR))
#endif

/*
 * Thread index at CUDA C
 */
#ifdef __CUDACC__
#define get_group_id()			(blockIdx.x)
#define get_num_groups()		(gridDim.x)
#define get_local_id()			(threadIdx.x)
#define get_local_size()		(blockDim.x)
#define get_global_id()			(threadIdx.x + blockIdx.x * blockDim.x)
#define get_global_size()		(blockDim.x * gridDim.x)
#define get_global_base()		(blockIdx.x * blockDim.x)
#define get_warp_id()			(threadIdx.x / warpSize)
#define get_lane_id()			(threadIdx.x & (warpSize-1))
#endif

/*
 * Error status
 */
#ifndef MAKE_SQLSTATE
#define PGSIXBIT(ch)			(((ch) - '0') & 0x3F)
#define MAKE_SQLSTATE(ch1,ch2,ch3,ch4,ch5)				\
	(PGSIXBIT(ch1) +  (PGSIXBIT(ch2) << 6) +			\
	 (PGSIXBIT(ch3) << 12) + (PGSIXBIT(ch4) << 18) +	\
	 (PGSIXBIT(ch5) << 24))
#endif  /* MAKE_SQLSTATE */
#include "utils/errcodes.h"
#define ERRCODE_FLAGS_CPU_FALLBACK			(1U<<30)
#define ERRCODE_STROM_SUCCESS				0
#define ERRCODE_STROM_DATASTORE_NOSPACE		MAKE_SQLSTATE('H','D','B','0','4')
#define ERRCODE_STROM_WRONG_CODE_GENERATION	MAKE_SQLSTATE('H','D','B','0','5')
#define ERRCODE_STROM_DATA_CORRUPTION		MAKE_SQLSTATE('H','D','B','0','7')
#define ERRCODE_STROM_VARLENA_UNSUPPORTED	MAKE_SQLSTATE('H','D','B','0','8')
#define ERRCODE_STROM_RECURSION_TOO_DEEP	MAKE_SQLSTATE('H','D','B','0','9')

#define KERN_ERRORBUF_FILENAME_LEN		24
#define KERN_ERRORBUF_FUNCNAME_LEN		64
#define KERN_ERRORBUF_MESSAGE_LEN		200
typedef struct {
	int32_t		errcode;	/* one of the ERRCODE_* */
	int32_t		lineno;
	char		filename[KERN_ERRORBUF_FILENAME_LEN];
	char		funcname[KERN_ERRORBUF_FUNCNAME_LEN];
	char		message[KERN_ERRORBUF_MESSAGE_LEN];
} kern_errorbuf;

/*
 * kern_context - a set of run-time information
 */
struct kern_parambuf;

typedef struct
{
	int			errcode;
	const char *error_filename;
	uint32_t	error_lineno;
	const char *error_funcname;
	const char *error_message;
	struct kern_parambuf *kparams;
	char	   *vlpos;
	char	   *vlend;
	char		vlbuf[1];
} kern_context;

#define DECL_KERNEL_CONTEXT(NAME,BUFSZ)					\
	union {												\
		kern_context kcxt;								\
		char __dummy__[offsetof(kern_context, vlbuf) +	\
					   MAXALIGN(BUFSZ+1)]				\
	} NAME
#define INIT_KERNEL_CONTEXT(kcxt,__kparams,__bufsz)		\
	do {												\
		memset(kcxt, 0, offsetof(kern_context, vlbuf));	\
		(kcxt)->kparams = (__kparams);					\
		(kcxt)->vlpos = (kcxt)->vlbuf;					\
		(kcxt)->vlend = (kcxt)->vlbuf + (__bufsz);		\
	} while(0)

INLINE_FUNCTION(void *)
kcxt_alloc(kern_context *kcxt, size_t len)
{
	char   *pos = (char *)MAXALIGN(kcxt->vlpos);

	if (pos >= kcxt->vlbuf && pos + len <= kcxt->vlend)
	{
		kcxt->vlpos = pos + len;
		return pos;
	}
	return NULL;
}

INLINE_FUNCTION(void)
kcxt_reset(kern_context *kcxt)
{
	kcxt->vlpos = kcxt->vlbuf;
}

/* ----------------------------------------------------------------
 *
 * Definitions related to the kernel data store
 *
 * ----------------------------------------------------------------
 */
#include "arrow_defs.h"

#define TYPE_KIND__NULL			'n'		/* unreferenced column */
#define TYPE_KIND__BASE			'b'
#define TYPE_KIND__ARRAY		'a'
#define TYPE_KIND__COMPOSITE	'c'
#define TYPE_KIND__DOMAIN		'd'
#define TYPE_KIND__ENUM			'e'
#define TYPE_KIND__PSEUDO		'p'
#define TYPE_KIND__RANGE		'r'

struct kern_colmeta {
	/* true, if column is held by value. Elsewhere, a reference */
	bool			attbyval;
	/* alignment; 1,2,4 or 8, not characters in pg_attribute */
	int8_t			attalign;
	/* length of attribute */
	int16_t			attlen;
	/* attribute number */
	int16_t			attnum;
	/* offset of attribute location, if deterministic */
	int16_t			attcacheoff;
	/* oid of the SQL data type */
	Oid				atttypid;
	/* typmod of the SQL data type */
	int32_t			atttypmod;
	/* one of TYPE_KIND__* */
	int8_t			atttypkind;
	/*
	 * (for array and composite types)
	 * Some of types contain sub-fields like array or composite type.
	 * We carry type definition information (kern_colmeta) using the
	 * kds->colmeta[] array next to the top-level fields.
	 * An array type has relevant element type. So, its @num_subattrs
	 * is always 1, and kds->colmeta[@idx_subattrs] informs properties
	 * of the element type.
	 * A composite type has several fields.
	 * kds->colmeta[@idx_subattrs ... @idx_subattrs + @num_subattrs -1]
	 * carries its sub-fields properties.
	 */
	uint16_t		idx_subattrs;
	uint16_t		num_subattrs;

	/* column name */
	char			attname[NAMEDATALEN];

	/*
	 * (only arrow/column format)
	 * @attoptions keeps extra information of Apache Arrow type. Unlike
	 * PostgreSQL types, it can have variation of data accuracy in time
	 * related data types, or precision in decimal data type.
	 */
	ArrowTypeOptions attopts;
	uint32_t		nullmap_offset;
	uint32_t		nullmap_length;
	uint32_t		values_offset;
	uint32_t		values_length;
	uint32_t		extra_offset;
	uint32_t		extra_length;
};
typedef struct kern_colmeta		kern_colmeta;

#define KDS_FORMAT_ROW			1
#define KDS_FORMAT_SLOT			2
#define KDS_FORMAT_HASH			3	/* inner hash table for GpuHashJoin */
#define KDS_FORMAT_BLOCK		4	/* raw blocks for direct loading */
#define KDS_FORMAT_COLUMN		5	/* columnar based storage format */
#define KDS_FORMAT_ARROW		6	/* apache arrow format */

struct kern_data_store {
	uint64_t		length;		/* length of this data-store */
	/*
	 * NOTE: {nitems + usage} must be aligned to 64bit because these pair of
	 * values can be updated atomically using cmpxchg.
	 */
	uint32_t		nitems; 	/* number of rows in this store */
	uint32_t		usage;		/* usage of this data-store (PACKED) */
	uint32_t		nrooms;		/* number of available rows in this store */
	uint32_t		ncols;		/* number of columns in this store */
	int8_t			format;		/* one of KDS_FORMAT_* above */
	bool			has_varlena; /* true, if any varlena attribute */
	bool			tdhasoid;	/* copy of TupleDesc.tdhasoid */
	Oid				tdtypeid;	/* copy of TupleDesc.tdtypeid */
	int32_t			tdtypmod;	/* copy of TupleDesc.tdtypmod */
	Oid				table_oid;	/* OID of the table (only if GpuScan) */
	uint32_t		nslots;		/* width of hash-slot (only HASH format) */
	uint32_t		nrows_per_block; /* average number of rows per
									  * PostgreSQL block (only BLOCK format) */
	uint32_t		nr_colmeta;	/* number of colmeta[] array elements;
								 * maybe, >= ncols, if any composite types */
	kern_colmeta	colmeta[1];	/* metadata of columns */
};
typedef struct kern_data_store		kern_data_store;

/*
 * kern_data_extra - extra buffer of KDS_FORMAT_COLUMN
 */
struct kern_data_extra
{
	uint64_t	length;
	uint64_t	usage;
	char		data[1];
};
typedef struct kern_data_extra		kern_data_extra;

/*
 * MEMO: Support of 32GB KDS - KDS with row-, hash- and column-format
 * internally uses 32bit offset value from the head or base address.
 * We have assumption here - any objects pointed by the offset value
 * is always aligned to MAXIMUM_ALIGNOF boundary (64bit).
 * It means we can use 32bit offset to represent up to 32GB range (35bit).
 */
static inline uint32_t
__kds_packed(size_t offset)
{
	assert((offset & ~(0xffffffffUL << MAXIMUM_ALIGNOF_SHIFT)) == 0);
	return (uint32_t)(offset >> MAXIMUM_ALIGNOF_SHIFT);
}

static inline size_t
__kds_unpack(uint32_t offset)
{
	return (size_t)offset << MAXIMUM_ALIGNOF_SHIFT;
}

/* attribute number of system columns */
#ifndef SYSATTR_H
#define SelfItemPointerAttributeNumber			(-1)
#define ObjectIdAttributeNumber					(-2)
#define MinTransactionIdAttributeNumber			(-3)
#define MinCommandIdAttributeNumber				(-4)
#define MaxTransactionIdAttributeNumber			(-5)
#define MaxCommandIdAttributeNumber				(-6)
#define TableOidAttributeNumber					(-7)
#define FirstLowInvalidHeapAttributeNumber		(-8)
#endif	/* !SYSATTR_H */

/*
 * kern_tupitem - individual items for KDS_FORMAT_ROW
 */
#ifndef ITEMPTR_H
typedef struct
{
	struct {
		uint16_t	bi_hi;
		uint16_t	bi_lo;
	} ip_blkid;
	uint16_t		ip_posid;
} ItemPointerData;
#endif	/* ITEMPTR_H */

#ifndef HTUP_DETAILS_H
typedef struct HeapTupleFields
{
	uint32_t		t_xmin;		/* inserting xact ID */
	uint32_t		t_xmax;		/* deleting or locking xact ID */
	union
	{
		uint32_t	t_cid;		/* inserting or deleting command ID, or both */
		uint32_t	t_xvac;		/* old-style VACUUM FULL xact ID */
	}	t_field3;
} HeapTupleFields;

typedef struct DatumTupleFields
{
    int32_t		datum_len_;		/* varlena header (do not touch directly!) */
    int32_t		datum_typmod;	/* -1, or identifier of a record type */
    Oid			datum_typeid;	/* composite type OID, or RECORDOID */
} DatumTupleFields;

struct
{
	union {
		HeapTupleFields		t_heap;
		DatumTupleFields	t_datum;
	} t_choice;

	ItemPointerData	t_ctid;			/* current TID of this or newer tuple */

	uint16_t		t_infomask2;	/* number of attributes + various flags */
    uint16_t		t_infomask;		/* various flag bits, see below */
    uint8_t			t_hoff;			/* sizeof header incl. bitmap, padding */
    /* ^ - 23 bytes - ^ */
    uint8_t			t_bits[1];		/* null-bitmap -- VARIABLE LENGTH */
} HeapTupleHeaderData;
#endif	/* HTUP_DETAILS_H */
typedef struct HeapTupleHeaderData	HeapTupleHeaderData;

struct kern_tupitem
{
	uint32_t		t_len;		/* length of tuple */
	uint32_t		rowid;		/* unique Id of this item */
	HeapTupleHeaderData	htup;
};
typedef struct kern_tupitem		kern_tupitem;

/*
 * kern_hashitem - individual items for KDS_FORMAT_HASH
 */
struct kern_hashitem
{
	uint32_t		hash;		/* 32-bit hash value */
	uint32_t		next;		/* offset of the next (PACKED) */
	kern_tupitem	t;			/* HeapTuple of this entry */
};
typedef struct kern_hashitem	kern_hashitem;

/*
 * Varlena definitions
 */
typedef struct varlena		varlena;
#ifndef VARHDRSZ
struct varlena {
    char		vl_len_[4];	/* Do not touch this field directly! */
	char		vl_dat[1];
};

#define VARHDRSZ			((int) sizeof(cl_int))
#define VARDATA(PTR)		VARDATA_4B(PTR)
#define VARSIZE(PTR)		VARSIZE_4B(PTR)
#define VARSIZE_EXHDR(PTR)	(VARSIZE(PTR) - VARHDRSZ)

#define VARSIZE_SHORT(PTR)	VARSIZE_1B(PTR)
#define VARDATA_SHORT(PTR)	VARDATA_1B(PTR)

typedef union
{
	struct						/* Normal varlena (4-byte length) */
	{
		uint32_t	va_header;
		char		va_data[1];
	}		va_4byte;
	struct						/* Compressed-in-line format */
	{
		uint32_t	va_header;
		uint32_t	va_rawsize;	/* Original data size (excludes header) */
		char		va_data[1];	/* Compressed data */
	}		va_compressed;
} varattrib_4b;

typedef struct
{
    uint8_t			va_header;
    char			va_data[1];	/* Data begins here */
} varattrib_1b;

/* inline portion of a short varlena pointing to an external resource */
typedef struct
{
	uint8_t			va_header;	/* Always 0x80 or 0x01 */
	uint8_t			va_tag;		/* Type of datum */
    char			va_data[1];	/* Data (of the type indicated by va_tag) */
} varattrib_1b_e;

typedef enum vartag_external
{
	VARTAG_INDIRECT = 1,
	VARTAG_ONDISK = 18
} vartag_external;

#define VARHDRSZ_SHORT			offsetof(varattrib_1b, va_data)
#define VARATT_SHORT_MAX		0x7F

typedef struct varatt_external
{
	int32_t		va_rawsize;		/* Original data size (includes header) */
	uint32_t	va_extsize;		/* External saved size (doesn't) */
	Oid			va_valueid;		/* Unique ID of value within TOAST table */
	Oid			va_toastrelid;	/* RelID of TOAST table containing it */
} varatt_external;

typedef struct varatt_indirect
{
	uintptr_t	pointer;		/* Host pointer to in-memory varlena */
} varatt_indirect;

#define VARTAG_SIZE(tag) \
	((tag) == VARTAG_INDIRECT ? sizeof(varatt_indirect) :   \
	 (tag) == VARTAG_ONDISK ? sizeof(varatt_external) :     \
	 0 /* should not happen */)

#define VARHDRSZ_EXTERNAL		offsetof(varattrib_1b_e, va_data)
#define VARTAG_EXTERNAL(PTR)	VARTAG_1B_E(PTR)
#define VARSIZE_EXTERNAL(PTR)	\
	(VARHDRSZ_EXTERNAL + VARTAG_SIZE(VARTAG_EXTERNAL(PTR)))

/*
 * compressed varlena format
 */
typedef struct toast_compress_header
{
	int32_t		vl_len_;		/* varlena header (do not touch directly!) */
    uint32_t	rawsize;		/* 2 bits for compression method and 30bits
								 * external size; see va_extinfo */
} toast_compress_header;

#define TOAST_COMPRESS_HDRSZ        ((cl_int)sizeof(toast_compress_header))
#define TOAST_COMPRESS_RAWSIZE(ptr)             \
    (((toast_compress_header *) (ptr))->rawsize)
#define TOAST_COMPRESS_RAWDATA(ptr)             \
    (((char *) (ptr)) + TOAST_COMPRESS_HDRSZ)
#define TOAST_COMPRESS_SET_RAWSIZE(ptr, len)    \
    (((toast_compress_header *) (ptr))->rawsize = (len))

/* basic varlena macros */
#define VARATT_IS_4B(PTR) \
	((((varattrib_1b *) (PTR))->va_header & 0x01) == 0x00)
#define VARATT_IS_4B_U(PTR) \
	((((varattrib_1b *) (PTR))->va_header & 0x03) == 0x00)
#define VARATT_IS_4B_C(PTR) \
	((((varattrib_1b *) (PTR))->va_header & 0x03) == 0x02)
#define VARATT_IS_1B(PTR) \
	((((varattrib_1b *) (PTR))->va_header & 0x01) == 0x01)
#define VARATT_IS_1B_E(PTR) \
	((((varattrib_1b *) (PTR))->va_header) == 0x01)
#define VARATT_IS_COMPRESSED(PTR)		VARATT_IS_4B_C(PTR)
#define VARATT_IS_EXTERNAL(PTR)			VARATT_IS_1B_E(PTR)
#define VARATT_IS_EXTERNAL_ONDISK(PTR)		\
	(VARATT_IS_EXTERNAL(PTR) && VARTAG_EXTERNAL(PTR) == VARTAG_ONDISK)
#define VARATT_IS_EXTERNAL_INDIRECT(PTR)	\
	(VARATT_IS_EXTERNAL(PTR) && VARTAG_EXTERNAL(PTR) == VARTAG_INDIRECT)
#define VARATT_IS_SHORT(PTR)			VARATT_IS_1B(PTR)
#define VARATT_IS_EXTENDED(PTR)			(!VARATT_IS_4B_U(PTR))
#define VARATT_NOT_PAD_BYTE(PTR)		(*((cl_uchar *) (PTR)) != 0)

#define VARSIZE_4B(PTR)                     \
	((__Fetch(&((varattrib_4b *)(PTR))->va_4byte.va_header)>>2) & 0x3FFFFFFF)
#define VARSIZE_1B(PTR) \
	((((varattrib_1b *) (PTR))->va_header >> 1) & 0x7F)
#define VARTAG_1B_E(PTR) \
	(((varattrib_1b_e *) (PTR))->va_tag)

#define VARRAWSIZE_4B_C(PTR)    \
	__Fetch(&((varattrib_4b *) (PTR))->va_compressed.va_rawsize)

#define VARSIZE_ANY_EXHDR(PTR) \
	(VARATT_IS_1B_E(PTR) ? VARSIZE_EXTERNAL(PTR)-VARHDRSZ_EXTERNAL : \
	 (VARATT_IS_1B(PTR) ? VARSIZE_1B(PTR)-VARHDRSZ_SHORT :			 \
	  VARSIZE_4B(PTR)-VARHDRSZ))

#define VARSIZE_ANY(PTR)							\
    (VARATT_IS_1B_E(PTR) ? VARSIZE_EXTERNAL(PTR) :	\
     (VARATT_IS_1B(PTR) ? VARSIZE_1B(PTR) :			\
      VARSIZE_4B(PTR)))

#define VARDATA_4B(PTR)	(((varattrib_4b *) (PTR))->va_4byte.va_data)
#define VARDATA_1B(PTR)	(((varattrib_1b *) (PTR))->va_data)
#define VARDATA_ANY(PTR) \
	(VARATT_IS_1B(PTR) ? VARDATA_1B(PTR) : VARDATA_4B(PTR))

#define SET_VARSIZE(PTR, len)                       \
	(((varattrib_4b *)(PTR))->va_4byte.va_header = (((uint32_t) (len)) << 2))
#endif	/* VARHDRSZ */

#endif	/* XPU_COMMON_H */
