/*
 * xpu_basetype.h
 *
 * Collection of base Int/Float functions for xPU (GPU/DPU/SPU)
 * --
 * Copyright 2011-2021 (C) KaiGai Kohei <kaigai@kaigai.gr.jp>
 * Copyright 2014-2021 (C) PG-Strom Developers Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the PostgreSQL License.
 */
#ifndef XPU_BASETYPE_H
#define XPU_BASETYPE_H

#define PGSTROM_SIMPLE_DEVTYPE_TEMPLATE(NAME,BASETYPE)				\
	PUBLIC_FUNCTION(bool)											\
	sql_##NAME##_datum_ref(kern_context *kcxt,						\
						   sql_##NAME##_t *result,					\
						   void *addr)								\
	{																\
		Assert(result != NULL);										\
		if (!addr)													\
			result->isnull = true;									\
		else														\
		{															\
			result->isnull = false;									\
			result->value = *((BASETYPE *)addr);					\
		}															\
		return true;												\
	}																\
	PUBLIC_FUNCTION(bool)											\
	sql_##NAME##_param_ref(kern_context *kcxt,						\
						   sql_##NAME##_t *result,					\
						   uint32_t param_id)						\
	{																\
		void   *addr = kparam_get_value(kcxt->kparams,				\
										param_id);					\
		return sql_##NAME##_datum_ref(kcxt, result, addr);			\
	}																\
	PUBLIC_FUNCTION(bool)											\
	arrow_##NAME##_datum_ref(kern_context *kcxt,					\
							 sql_##NAME##_t *result,				\
							 kern_data_store *kds,					\
							 kern_colmeta *cmeta,					\
							 uint32_t rowidx)						\
	{																\
		void   *addr =												\
			KDS_ARROW_REF_SIMPLE_DATUM(kds, cmeta, rowidx,			\
									   sizeof(BASETYPE));			\
		return sql_##NAME##_datum_ref(kcxt, result, addr);			\
	}																\
	PUBLIC_FUNCTION(int)											\
	sql_##NAME##_datum_store(kern_context *kcxt,					\
							 char *buffer,							\
							 sql_##NAME##_t *datum)					\
	{																\
		if (datum->isnull)											\
			return 0;												\
		if (buffer)													\
			*((BASETYPE *)buffer) = datum->value;					\
		return sizeof(BASETYPE);									\
	}																\
	PUBLIC_FUNCTION(uint32_t)										\
	sql_##NAME##_hash(kern_context *kcxt, sql_##NAME##_t *datum)	\
	{																\
		if (datum->isnull)											\
			return 0;												\
		return pg_hash_any((unsigned char *)&datum->value,			\
						   sizeof(BASETYPE));						\
	}

typedef struct {
	int8_t		value;
	bool		isnull;
} sql_bool_t;
__PGSTROM_DEVTYPE_FUNCTION_DECLARATION(bool)
PGSTROM_SIMPLE_DEVTYPE_DECLARATION(int1, int8_t)
PGSTROM_SIMPLE_DEVTYPE_DECLARATION(int2, int16_t)
PGSTROM_SIMPLE_DEVTYPE_DECLARATION(int4, int32_t)
PGSTROM_SIMPLE_DEVTYPE_DECLARATION(int8, int64_t)
PGSTROM_SIMPLE_DEVTYPE_DECLARATION(float2, float2_t)
PGSTROM_SIMPLE_DEVTYPE_DECLARATION(float4, float4_t)
PGSTROM_SIMPLE_DEVTYPE_DECLARATION(float8, float8_t)

#endif	/* XPU_BASETYPE_H */

