/*
 * xpu_devtypes.h
 *
 * Definitions of device types for xPU(GPU/DPU/SPU)
 * --
 * Copyright 2011-2021 (C) KaiGai Kohei <kaigai@kaigai.gr.jp>
 * Copyright 2014-2021 (C) PG-Strom Developers Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the PostgreSQL License.
 */
#ifndef XPU_DEVTYPES_H
#define XPU_DEVTYPES_H
#include "xpu_common.h"

/* ------------------------------------------------
 * Definition of prime data types
 * ------------------------------------------------ */
#define __PGSTROM_DEVTYPE_SIMPLE_DECLARATION(NAME,BASETYPE)	\
	typedef struct {										\
		BASETYPE	value;									\
		bool		isnull;									\
	} pg_##NAME##_t;
#define __PGSTROM_DEVTYPE_VARLENA_DECLARATION(NAME)			\
	typedef struct {										\
		char   *value;										\
		int		length;		/* -1, if PG varlena */			\
		bool	isnull;										\
	} pg_##NAME##_t;

#define __PGSTROM_DEVTYPE_SUPPORT_FUNCTIONS(NAME)					\
	extern int pg_##NAME##_datum_ref(kern_context *kcxt,			\
									 pg_##NAME##_t *result,			\
									 void *addr);					\
	extern int pg_##NAME##_param_ref(kern_context *kcxt,			\
									 pg_##NAME##_t *result,			\
									 uint32_t param_id);			\
	extern int pg_##NAME##_datum_ref_arrow(kern_context *kcxt,		\
										   pg_##NAME##_t *result,	\
										   kern_data_store *kds,	\
										   kern_colmeta *cmeta,		\
										   uint32_t rowidx);		\
	extern int32_t pg_##NAME##_datum_store(kern_context *kcxt,		\
										   char *buffer,			\
										   pg_##NAME##_t *datum);	\
	extern uint32_t pg_##NAME##_hash(kern_context *kcxt,			\
									 pg_##NAME##_t *datum);

#define PGSTROM_SIMPLE_DEVTYPE_TEMPLATE(NAME,BASETYPE)				\
	__PGSTROM_DEVTYPE_SIMPLE_DECLARATION(NAME,BASETYPE)				\
	__PGSTROM_DEVTYPE_SUPPORT_FUNCTIONS(NAME)

#define PGSTROM_VARLENA_DEVTYPE_TEMPLATE(NAME)						\
	__PGSTROM_DEVTYPE_VARLENA_DECLARATION(NAME)						\
	__PGSTROM_DEVTYPE_SUPPORT_FUNCTIONS(NAME)

/* base type declarations */
PGSTROM_SIMPLE_DEVTYPE_TEMPLATE(bool, int8_t)
PGSTROM_SIMPLE_DEVTYPE_TEMPLATE(int1, int8_t)
PGSTROM_SIMPLE_DEVTYPE_TEMPLATE(int2, int16_t)
PGSTROM_SIMPLE_DEVTYPE_TEMPLATE(int4, int32_t)
PGSTROM_SIMPLE_DEVTYPE_TEMPLATE(int8, int64_t)
PGSTROM_SIMPLE_DEVTYPE_TEMPLATE(float2, float2_t)
PGSTROM_SIMPLE_DEVTYPE_TEMPLATE(float4, float4_t)
PGSTROM_SIMPLE_DEVTYPE_TEMPLATE(float8, float8_t)
#include "xpu_numeric.h"
#include "xpu_textlib.h"
#include "xpu_timelib.h"
#include "xpu_misclib.h"

/*
 * pg_array_t - array type support
 *
 * NOTE: pg_array_t is designed to store both of PostgreSQL / Arrow array
 * values. If @length < 0, it means @value points a varlena based PostgreSQL
 * array values; which includes nitems, dimension, nullmap and so on.
 * Elsewhere, @length means number of elements, from @start of the array on
 * the columnar buffer by @smeta.
 */
typedef struct {
	char	   *value;
	bool		isnull;
	int			length;
	uint32_t	start;
	kern_colmeta *smeta;
} pg_array_t;
__PGSTROM_DEVTYPE_SUPPORT_FUNCTIONS(array)

/*
 * pg_composite_t - composite type support
 *
 * NOTE: pg_composite_t is designed to store both of PostgreSQL / Arrow composite
 * values. If @nfields < 0, it means @value.htup points a varlena base PostgreSQL
 * composite values. Elsewhere (@nfields >= 0), it points composite values on
 * KDS_FORMAT_ARROW chunk. In this case, smeta[0] ... smeta[@nfields-1] describes
 * the values array on the KDS.
 */
typedef struct {
	char	   *value;
	bool		isnull;
	int16_t		nfields;
	uint32_t	rowidx;
	Oid			comp_typid;
	int			comp_typmod;
	kern_colmeta *smeta;
} pg_composite_t;
__PGSTROM_DEVTYPE_SUPPORT_FUNCTIONS(composite)

#endif	/* XPU_DEVTYPES_H */
