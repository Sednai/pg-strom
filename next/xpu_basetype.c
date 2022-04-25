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

/*
 * sql_bool_t functions
 */
PUBLIC_FUNCTION(bool)
sql_bool_datum_ref(kern_context *kcxt,
				   sql_bool_t *result,
				   void *addr)
{
	if (!addr)
		result->isnull = true;
	else
	{
		result->isnull = false;
		result->value = *((int8_t *)addr);
	}
	return true;
}

PUBLIC_FUNCTION(bool)
sql_bool_param_ref(kern_context *kcxt,
				   sql_bool_t *result,
				   uint32_t param_id)
{
	void   *addr = kparam_get_value(kcxt->kparams, param_id);

	return sql_bool_datum_ref(kcxt, result, addr);
}


PUBLIC_FUNCTION(bool)
arrow_bool_datum_ref(kern_context *kcxt,
					 sql_bool_t *result,
					 kern_data_store *kds,
					 kern_colmeta *cmeta,
					 uint32_t rowidx)
{																	\
	void   *addr = KDS_ARROW_REF_SIMPLE_DATUM(kds, cmeta, rowidx,
											  sizeof(int8_t));
	return sql_bool_datum_ref(kcxt, result, addr);
}

PUBLIC_FUNCTION(int)
sql_bool_datum_store(kern_context *kcxt,
					 char *buffer,
					 sql_bool_t *datum)
{																	\
	if (datum->isnull)
		return 0;
	if (buffer)
		*((int8_t *)buffer) = datum->value;
	return sizeof(int8_t);
}

PUBLIC_FUNCTION(bool)
sql_bool_hash(kern_context *kcxt,
			  uint32_t *p_hash,
			  sql_bool_t *datum)
{
	if (datum->isnull)
		*p_hash = 0;
	else
		*p_hash = pg_hash_any(&datum->value, sizeof(int8_t));
	return true;
}

PUBLIC_FUNCTION(uint32_t)
devtype_bool_hash(bool isnull, Datum value)
{
	sql_bool_t	temp;
	uint32_t	hash;
	DECL_KERNEL_CONTEXT(u,NULL,0);
	if (!sql_bool_datum_ref(&u.kcxt, &temp,
							isnull ? NULL : &value) ||
		!sql_bool_hash(&u.kcxt, &hash, &temp))
		pg_kern_ereport(&u.kcxt);
	return hash;
}

PGSTROM_SIMPLE_DEVTYPE_TEMPLATE(int1,int8_t)
PGSTROM_SIMPLE_DEVTYPE_TEMPLATE(int2,int16_t)
PGSTROM_SIMPLE_DEVTYPE_TEMPLATE(int4,int32_t)
PGSTROM_SIMPLE_DEVTYPE_TEMPLATE(int8,int64_t)
PGSTROM_SIMPLE_DEVTYPE_TEMPLATE(float2,float2_t)
PGSTROM_SIMPLE_DEVTYPE_TEMPLATE(float4,float4_t)
PGSTROM_SIMPLE_DEVTYPE_TEMPLATE(float8,float8_t)
