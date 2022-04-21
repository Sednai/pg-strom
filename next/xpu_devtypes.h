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
#ifndef XPU_BASETYPES_H
#define XPU_BASETYPES_H
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
		bool	isnull;										\
		int		length;		/* -1, if PG varlena */			\
	} pg_##NAME##_t;

#ifdef __STROM_HOST__
#define PGSTROM_SIMPLE_DEVTYPE_TEMPLATE(NAME,BASETYPE)	\
	__PGSTROM_DEVTYPE_SIMPLE_DECLARATION(NAME,BASETYPE)
#define PGSTROM_VARLENA_DEVTYPE_TEMPLATE(NAME)			\
	__PGSTROM_DEVTYPE_VARLENA_DECLARATION(NAME)
#else
#define __PGSTROM_DEVTYPE_SUPPORT_FUNCTIONS(NAME)					\
	extern int pg_##NAME##_datum_ref(kern_context *kcxt,			\
									 pg_##NAME##_t *rv,				\
									 void *addr);					\
	extern int pg_##NAME##_datum_ref_arrow(kern_context *kcxt,		\
										   pg_##NAME##_t *rv,		\
										   kern_data_store *kds,	\
										   uint32_t colidx,			\
										   uint32_t rowidx);		\
	extern int pg_##NAME##_datum_ref_slot(kern_context *kcxt,		\
										  pg_##NAME##_t *rv,		\
										  kern_data_store *kds,		\
										  int8_t dclass,			\
										  Datum value);				\
	extern int pg_##NAME##_param_ref(kern_context *kcxt,			\
									 pg_##NAME##_t *datum,			\
									 uint32_t param_id);			\
	extern uint32_t pg_##NAME##_hash(kern_context *kcxt,			\
									 pg_##NAME##_t *datum);
#define PGSTROM_SIMPLE_DEVTYPE_TEMPLATE(NAME,BASETYPE)				\
	PGSTROM_DEVTYPE_SIMPLE_DECLARATION(NAME,BASETYPE)				\
	__PGSTROM_DEVTYPE_SUPPORT_FUNCTIONS(NAME)
#define PGSTROM_VARLENA_DEVTYPE_TEMPLATE(NAME)						\
	__PGSTROM_DEVTYPE_VARLENA_DECLARATION(NAME)						\
	__PGSTROM_DEVTYPE_SUPPORT_FUNCTIONS(NAME)
#endif

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

#endif	/* XPU_BASETYPES_H */
