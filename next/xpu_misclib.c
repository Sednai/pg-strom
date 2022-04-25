/*
 * xpu_misclib.c
 *
 * Collection of misc functions and operators for xPU(GPU/DPU/SPU)
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
 *
 * Currency data type (sql_money_t), functions and operators
 *
 */
PGSTROM_SIMPLE_DEVTYPE_TEMPLATE(money, int64_t)

/*
 *
 * UUID data type (sql_uuid_t), functions and operators
 *
 */
PUBLIC_FUNCTION(bool)
sql_uuid_datum_ref(kern_context *kcxt,
				   sql_uuid_t *result,
				   void *addr)
{
	Assert(result != NULL);
	if (!addr)
		result->isnull = true;
	else
	{
		result->isnull = false;
		memcpy(&result->value, addr, UUID_LEN);
	}
	return true;
}

PUBLIC_FUNCTION(bool)
sql_uuid_param_ref(kern_context *kcxt,
				   sql_uuid_t *result,
				   uint32_t param_id)
{
	void   *addr = kparam_get_value(kcxt->kparams, param_id);

	return sql_uuid_datum_ref(kcxt, result, addr);
}

PUBLIC_FUNCTION(bool)
arrow_uuid_datum_ref(kern_context *kcxt,
					 sql_uuid_t *result,
					 kern_data_store *kds,
					 kern_colmeta *cmeta,
					 uint32_t rowidx)
{
	void   *addr;

	if (cmeta->attopts.fixed_size_binary.byteWidth != UUID_LEN)
	{
		STROM_ELOG(kcxt, "Arrow::FixedSizeBinary has wrong byteWidth");
		return false;
	}
	memset(result, 0, sizeof(sql_uuid_t));
	addr = KDS_ARROW_REF_SIMPLE_DATUM(kds, cmeta, rowidx, UUID_LEN);
	if (!addr)
		result->isnull = true;
	else
		memcpy(result->value.data, addr, UUID_LEN);
	return true;
}

PUBLIC_FUNCTION(int)
sql_uuid_datum_store(kern_context *kcxt,
					 char *buffer,
					 sql_uuid_t *datum)
{
	if (datum->isnull)
		return 0;
	if (buffer)
		memcpy(buffer, datum->value.data, UUID_LEN);
	return UUID_LEN;
}

PUBLIC_FUNCTION(bool)
sql_uuid_hash(kern_context *kcxt,
			  uint32_t *p_hash,
			  sql_uuid_t *datum)
{
	if (datum->isnull)
		*p_hash = 0;
	else
		*p_hash = pg_hash_any(datum->value.data, UUID_LEN);
	return true;
}

PUBLIC_FUNCTION(uint32_t)
devtype_uuid_hash(bool isnull, Datum value)
{
	sql_uuid_t	temp;
	uint32_t	hash;
	DECL_KERNEL_CONTEXT(u,NULL,0);
	if (!sql_uuid_datum_ref(&u.kcxt, &temp,
                            isnull ? NULL : DatumGetPointer(value)) ||
		!sql_uuid_hash(&u.kcxt, &hash, &temp))
		pg_kern_ereport(&u.kcxt);
	return hash;
}

/*
 *
 * Macaddr data type (sql_macaddr_t), functions and operators
 *
 */
PUBLIC_FUNCTION(bool)
sql_macaddr_datum_ref(kern_context *kcxt,
					  sql_macaddr_t *result,
					  void *addr)
{
	Assert(result != NULL);
	if (!addr)
		result->isnull = true;
	else
	{
		result->isnull = false;
		memcpy(&result->value, addr, sizeof(macaddr));
	}
	return true;
}

PUBLIC_FUNCTION(bool)
sql_macaddr_param_ref(kern_context *kcxt,
					  sql_macaddr_t *result,
					  uint32_t param_id)
{
	void   *addr = kparam_get_value(kcxt->kparams, param_id);

	return sql_macaddr_datum_ref(kcxt, result, addr);
}

PUBLIC_FUNCTION(bool)
arrow_macaddr_datum_ref(kern_context *kcxt,
					 sql_macaddr_t *result,
					 kern_data_store *kds,
					 kern_colmeta *cmeta,
					 uint32_t rowidx)
{
	void   *addr;

	if (cmeta->attopts.fixed_size_binary.byteWidth != sizeof(macaddr))
	{
		STROM_ELOG(kcxt, "Arrow::FixedSizeBinary has wrong byteWidth");
		return false;
	}
	addr = KDS_ARROW_REF_SIMPLE_DATUM(kds, cmeta, rowidx, sizeof(macaddr));
	if (!addr)
		result->isnull = true;
	else
	{
		result->isnull = false;
		memcpy(&result->value, addr, sizeof(macaddr));
	}
	return true;
}

PUBLIC_FUNCTION(int)
sql_macaddr_datum_store(kern_context *kcxt,
						char *buffer,
						sql_macaddr_t *datum)
{
	if (datum->isnull)
		return 0;
	if (buffer)
		memcpy(buffer, &datum->value, sizeof(macaddr));
	return sizeof(macaddr);
}

PUBLIC_FUNCTION(bool)
sql_macaddr_hash(kern_context *kcxt,
				 uint32_t *p_hash,
				 sql_macaddr_t *datum)
{
	if (datum->isnull)
		*p_hash = 0;
	else
		*p_hash = pg_hash_any(&datum->value, sizeof(macaddr));
	return true;
}

PUBLIC_FUNCTION(uint32_t)
devtype_macaddr_hash(bool isnull, Datum value)
{
	sql_macaddr_t temp;
	uint32_t	hash;
	DECL_KERNEL_CONTEXT(u,NULL,0);
	if (!sql_macaddr_datum_ref(&u.kcxt, &temp,
							   isnull ? NULL : DatumGetPointer(value)) ||
		!sql_macaddr_hash(&u.kcxt, &hash, &temp))
		pg_kern_ereport(&u.kcxt);
	return hash;
}

/*
 *
 * Inet data type (sql_iner_t), functions and operators
 *
 */
PUBLIC_FUNCTION(bool)
sql_inet_datum_ref(kern_context *kcxt,
				   sql_inet_t *result,
				   void *addr)
{
	Assert(result != NULL);
	if (!addr)
		result->isnull = true;
	else if (VARATT_IS_COMPRESSED(addr) || VARATT_IS_EXTERNAL(addr))
	{
		STROM_CPU_FALLBACK(kcxt, ERRCODE_INTERNAL_ERROR,
						   "inet value is compressed or toasted");
		return false;
	}
	else if (VARSIZE_ANY_EXHDR(addr) < offsetof(inet_struct, ipaddr))
	{
		STROM_ELOG(kcxt, "corrupted inet datum");
		return false;
	}
	else
	{
		inet_struct	   *ip_data = (inet_struct *)VARDATA_ANY(addr);
		int				ip_size = ip_addrsize(ip_data);

		if (VARSIZE_ANY_EXHDR(addr) < offsetof(inet_struct, ipaddr[ip_size]))
		{
			STROM_ELOG(kcxt, "corrupted inet datum");
			return false;
		}
		result->isnull = false;
		memcpy(&result->value, VARDATA_ANY(addr),
			   offsetof(inet_struct, ipaddr[ip_size]));
	}
	return true;
}

PUBLIC_FUNCTION(bool)
sql_inet_param_ref(kern_context *kcxt,
				   sql_inet_t *result,
				   uint32_t param_id)
{
	void   *addr = kparam_get_value(kcxt->kparams, param_id);

	return sql_inet_datum_ref(kcxt, result, addr);
}

PUBLIC_FUNCTION(bool)
arrow_inet_datum_ref(kern_context *kcxt,
                     sql_inet_t *result,
                     kern_data_store *kds,
                     kern_colmeta *cmeta,
                     uint32_t rowidx)
{
	int		byteWidth = cmeta->attopts.fixed_size_binary.byteWidth;
	void   *addr;

	if (byteWidth != 4 && byteWidth != 16)
	{
		STROM_ELOG(kcxt, "corrupted inet datum");
		return false;
	}
	addr = KDS_ARROW_REF_SIMPLE_DATUM(kds, cmeta, rowidx, byteWidth);
	if (!addr)
		result->isnull = true;
	else
	{
		result->isnull = false;
		result->value.family = (byteWidth == 4 ? PGSQL_AF_INET : PGSQL_AF_INET6);
		result->value.bits = 8 * byteWidth;
		memcpy(result->value.ipaddr, addr, byteWidth);
	}
	return true;
}

PUBLIC_FUNCTION(int)
sql_inet_datum_store(kern_context *kcxt,
					 char *buffer,
					 sql_inet_t *datum)
{
	int		len;

	if (datum->isnull)
		return 0;
	if (datum->value.family == PGSQL_AF_INET)
		len = offsetof(inet_struct, ipaddr) + 4;
	else if (datum->value.family == PGSQL_AF_INET6)
		len = offsetof(inet_struct, ipaddr) + 16;
	else
	{
		STROM_ELOG(kcxt, "corrupted inet datum");
		return -1;
	}
	if (buffer)
	{
		memcpy(buffer + VARHDRSZ, &datum->value, len);
		SET_VARSIZE(buffer, len + VARHDRSZ);
	}
	return len + VARHDRSZ;
}

PUBLIC_FUNCTION(bool)
sql_inet_hash(kern_context *kcxt,
			  uint32_t *p_hash,
			  sql_inet_t *datum)
{
	if (datum->isnull)
		*p_hash = 0;
	else if (datum->value.family == PGSQL_AF_INET)
		*p_hash = pg_hash_any(&datum->value,		/* IPv4 */
							  offsetof(inet_struct, ipaddr[4]));
	else if (datum->value.family == PGSQL_AF_INET6)
		*p_hash = pg_hash_any(&datum->value,		/* IPv6 */
							  offsetof(inet_struct, ipaddr[16]));
	else
	{
		STROM_ELOG(kcxt, "sql_inet_t has unknown IP version");
		return false;
	}
	return true;
}

PUBLIC_FUNCTION(uint32_t)
devtype_inet_hash(bool isnull, Datum value)
{
	sql_inet_t temp;
	uint32_t	hash;
	DECL_KERNEL_CONTEXT(u,NULL,0);
	if (!sql_inet_datum_ref(&u.kcxt, &temp,
							isnull ? NULL : DatumGetPointer(value)) ||
		!sql_inet_hash(&u.kcxt, &hash, &temp))
		pg_kern_ereport(&u.kcxt);
	return hash;
}
