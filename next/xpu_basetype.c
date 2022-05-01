/*
 * xpu_basetype.c
 *
 * Collection of primitive Int/Float type support on XPU(GPU/DPU/SPU)
 * ----
 * Copyright 2011-2022 (C) KaiGai Kohei <kaigai@kaigai.gr.jp>
 * Copyright 2014-2022 (C) PG-Strom Developers Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the PostgreSQL License.
 *
 */
#include "xpu_common.h"

#define PGSTROM_SIMPLE_BASETYPE_TEMPLATE(NAME,BASETYPE)					\
	STATIC_FUNCTION(bool)												\
	sql_##NAME##_datum_ref(kern_context *kcxt,							\
						   sql_datum_t *__result,						\
						   const void *addr)							\
	{																	\
		sql_##NAME##_t *result = (sql_##NAME##_t *)__result;			\
																		\
		result->ops = &sql_bool_ops;									\
		if (!addr)														\
			result->isnull = true;										\
		else															\
		{																\
			result->isnull = false;										\
			result->value = *((const BASETYPE *)addr);					\
		}																\
		return true;													\
	}																	\
	STATIC_FUNCTION(bool)												\
	arrow_##NAME##_datum_ref(kern_context *kcxt,						\
							 sql_datum_t *__result,						\
							 kern_data_store *kds,						\
							 kern_colmeta *cmeta,						\
							 uint32_t rowidx)							\
	{																	\
		const void	   *addr;											\
																		\
		addr = KDS_ARROW_REF_SIMPLE_DATUM(kds, cmeta, rowidx,			\
										  sizeof(BASETYPE));			\
		return sql_##NAME##_datum_ref(kcxt, __result, addr);			\
	}																	\
	STATIC_FUNCTION(int)												\
	sql_##NAME##_datum_store(kern_context *kcxt,						\
							 char *buffer,								\
							 const sql_datum_t *__arg)					\
	{																	\
		const sql_##NAME##_t *arg = (const sql_##NAME##_t *)__arg;		\
																		\
		if (arg->isnull)												\
			return 0;													\
		*((BASETYPE *)buffer) = arg->value;								\
		return sizeof(BASETYPE);										\
	}																	\
	STATIC_FUNCTION(bool)												\
	sql_##NAME##_datum_hash(kern_context *kcxt,							\
							uint32_t *p_hash,							\
							const sql_datum_t *__arg)					\
	{																	\
		const sql_##NAME##_t *arg = (const sql_##NAME##_t *)__arg;		\
																		\
		if (arg->isnull)												\
			*p_hash = 0;												\
		else															\
			*p_hash = pg_hash_any(&arg->value, sizeof(BASETYPE));		\
		return true;													\
	}																	\
	PGSTROM_SQLTYPE_OPERATORS(NAME)

PGSTROM_SIMPLE_BASETYPE_TEMPLATE(bool, int8_t);
PGSTROM_SIMPLE_BASETYPE_TEMPLATE(int1, int8_t);
PGSTROM_SIMPLE_BASETYPE_TEMPLATE(int2, int16_t);
PGSTROM_SIMPLE_BASETYPE_TEMPLATE(int4, int32_t);
PGSTROM_SIMPLE_BASETYPE_TEMPLATE(int8, int64_t);
PGSTROM_SIMPLE_BASETYPE_TEMPLATE(float2, float2_t);
PGSTROM_SIMPLE_BASETYPE_TEMPLATE(float4, float4_t);
PGSTROM_SIMPLE_BASETYPE_TEMPLATE(float8, float8_t);
