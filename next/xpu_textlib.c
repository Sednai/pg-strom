/*
 * xpu_textlib.c
 *
 * Collection of text functions and operators for xPU(GPU/DPU/SPU)
 * ----
 * Copyright 2011-2022 (C) KaiGai Kohei <kaigai@kaigai.gr.jp>
 * Copyright 2014-2022 (C) PG-Strom Developers Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the PostgreSQL License.
 *
 */
#include "xpu_common.h"

/* ----------------------------------------------------------------
 *
 * bytea functions and operators
 *
 * ----------------------------------------------------------------
 */
PGSTROM_VARLENA_DEVTYPE_TEMPLATE(bytea)

/* ----------------------------------------------------------------
 *
 * bpchar functions and operators
 *
 * ----------------------------------------------------------------
 */
INLINE_FUNCTION(int)
bpchar_truelen(const char *s, int len)
{
	int		i;

	for (i = len - 1; i >= 0; i--)
	{
		if (s[i] != ' ')
			break;
	}
	return i + 1;
}

PUBLIC_FUNCTION(bool)
sql_bpchar_datum_ref(kern_context *kcxt,
					 sql_bpchar_t *result,
					 void *addr)
{
	memset(result, 0, sizeof(sql_bpchar_t));
	if (addr)
	{
		result->isnull = false;
		result->length = -1;
		result->value = addr;
		return VARSIZE_ANY(addr);
	}
	result->isnull = true;
	return true;
}

PUBLIC_FUNCTION(bool)
sql_bpchar_param_ref(kern_context *kcxt,
					 sql_bpchar_t *result,
					 uint32_t param_id)
{
	void   *addr = kparam_get_value(kcxt->kparams, param_id);

	return sql_bpchar_datum_ref(kcxt, result, addr);
}

PUBLIC_FUNCTION(bool)
arrow_bpchar_datum_ref(kern_context *kcxt,
					   sql_bpchar_t *result,
					   kern_data_store *kds,
					   kern_colmeta *cmeta,
					   uint32_t rowidx)
{
	int		unitsz = cmeta->atttypmod - VARHDRSZ;
	char   *addr = NULL;

	if (unitsz > 0)
		addr = KDS_ARROW_REF_SIMPLE_DATUM(kds, cmeta, rowidx, unitsz);
	memset(result, 0, sizeof(sql_bpchar_t));
	if (!addr)
		result->isnull = true;
	else
	{
		char   *pos = addr + unitsz;

		while (pos > addr && pos[-1] == ' ')
			pos--;
		result->value = addr;
		result->length = pos - addr;
	}
	return true;
}

PUBLIC_FUNCTION(int)
sql_bpchar_datum_store(kern_context *kcxt,
					   char *buffer,
					   sql_bpchar_t *datum) 
{
	char   *data;
	int		len;

	if (datum->isnull)
		return 0;
	if (datum->length < 0)
	{
		data = VARDATA_ANY(datum->value);
		len = VARSIZE_ANY_EXHDR(datum->value);
		if (!VARATT_IS_COMPRESSED(data) &&
			!VARATT_IS_EXTERNAL(data))
			len = bpchar_truelen(data, len);
	}
	else
	{
		data = datum->value;
		len = bpchar_truelen(data, datum->length);
	}
	if (buffer)
	{
		memcpy(buffer + VARHDRSZ, data, len);
		SET_VARSIZE(buffer, len + VARHDRSZ);
	}
	return len + VARHDRSZ;
}

bool
sql_bpchar_hash(kern_context*kcxt,
				uint32_t *p_hash,
				sql_bpchar_t *datum)
{
	char   *data;
	int		len;

	if (datum->isnull)
		return 0;
	if (datum->length >= 0)
	{
		data = datum->value;
        len = datum->length;
	}
	else if (!VARATT_IS_COMPRESSED(datum->value) &&
			 !VARATT_IS_EXTERNAL(datum->value))
	{
		data = VARDATA_ANY(datum->value);
		len = VARSIZE_ANY_EXHDR(datum->value);
	}
	else
	{
		STROM_CPU_FALLBACK(kcxt, ERRCODE_STROM_VARLENA_UNSUPPORTED,
						   "bpchar datum is compressed or external");
		return false;
	}
	len = bpchar_truelen(data, len);
	*p_hash = pg_hash_any(data, len);
	return true;
}

/* ----------------------------------------------------------------
 *
 * text functions and operators
 *
 * ----------------------------------------------------------------
 */
PGSTROM_VARLENA_DEVTYPE_TEMPLATE(text)
