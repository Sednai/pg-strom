/*
 * xpu_basetype.c
 *
 * Collection of the primitive Date/Time type support on xPU(GPU/DPU/SPU)
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
 * Misc definitions copied from PostgreSQL
 */
#ifndef DATATYPE_TIMESTAMP_H
#define DAYS_PER_YEAR		365.25	/* assumes leap year every four years */
#define MONTHS_PER_YEAR		12
#define DAYS_PER_MONTH		30		/* assumes exactly 30 days per month */
#define HOURS_PER_DAY		24		/* assume no daylight savings time changes */

#define SECS_PER_YEAR		(36525 * 864)	/* avoid floating-point computation */
#define SECS_PER_DAY		86400
#define SECS_PER_HOUR		3600
#define SECS_PER_MINUTE		60
#define MINS_PER_HOUR		60

#define USECS_PER_DAY		86400000000L
#define USECS_PER_HOUR       3600000000L
#define USECS_PER_MINUTE       60000000L
#define USECS_PER_SEC           1000000L

#define DT_NOBEGIN			(-0x7fffffffffffffffL - 1)
#define DT_NOEND			(0x7fffffffffffffffL)
#endif /* DATATYPE_TIMESTAMP_H */

/*
 * Common ref/store functions for:
 *  date/time/timetz/timestamp/timestamptz/interval
 */
PGSTROM_SIMPLE_DEVTYPE_TEMPLATE(date, DateADT)
PGSTROM_SIMPLE_DEVTYPE_TEMPLATE(time, TimeADT)
PGSTROM_SIMPLE_DEVTYPE_TEMPLATE(timetz, TimeTzADT)
PGSTROM_SIMPLE_DEVTYPE_TEMPLATE(timestamp, Timestamp)
PGSTROM_SIMPLE_DEVTYPE_TEMPLATE(timestamptz, TimestampTz)

PUBLIC_FUNCTION(bool)
sql_interval_datum_ref(kern_context *kcxt,
					   sql_interval_t *result,
					   void *addr)
{
	if (!addr)
		result->isnull = true;
	else
	{
		result->isnull = false;
		memcpy(&result->value, addr, sizeof(Interval));
	}
	return true;
}

PUBLIC_FUNCTION(bool)
sql_interval_param_ref(kern_context *kcxt,
					   sql_interval_t *result,
					   uint32_t param_id)
{
	void   *addr = kparam_get_value(kcxt->kparams, param_id);

	return sql_interval_datum_ref(kcxt, result, addr);
}

PUBLIC_FUNCTION(bool)
arrow_interval_datum_ref(kern_context *kcxt,
						 sql_interval_t *result,
						 kern_data_store *kds,
						 kern_colmeta *cmeta,
						 uint32_t rowidx)
{
	uint32_t   *ival;

	switch (cmeta->attopts.interval.unit)
	{
		case ArrowIntervalUnit__Year_Month:
			ival = (uint32_t *)
				KDS_ARROW_REF_SIMPLE_DATUM(kds, cmeta, rowidx,
										   sizeof(uint32_t));
			memset(result, 0, sizeof(sql_interval_t));
			result->value.month = *ival;
			break;
		case ArrowIntervalUnit__Day_Time:
			ival = (uint32_t *)
				KDS_ARROW_REF_SIMPLE_DATUM(kds, cmeta, rowidx,
										   2 * sizeof(uint32_t));
			memset(result, 0, sizeof(sql_interval_t));
			result->value.day = ival[0];
			result->value.time = ival[1];
			break;
		default:
			STROM_EREPORT(kcxt, ERRCODE_DATA_CORRUPTED,
						  "unknown unit-size of Arrow::Interval");
			return false;
	}
	return true;
}

PUBLIC_FUNCTION(int)
sql_interval_datum_store(kern_context *kcxt,
						 char *buffer,
						 sql_interval_t *datum)
{
	if (datum->isnull)
		return 0;
	if (buffer)
		memcpy(buffer, &datum->value, sizeof(Interval));
	return sizeof(Interval);
}

INLINE_FUNCTION(int128_t)
__interval_cmp_value(sql_interval_t *datum)
{
	int64_t		days, frac;

	frac = datum->value.time % USECS_PER_DAY;
	days = (datum->value.time / USECS_PER_DAY +
			datum->value.day +
			datum->value.month * 30L);
	return (int128_t)days * USECS_PER_DAY + (int128_t)frac;
}

PUBLIC_FUNCTION(bool)
sql_interval_hash(kern_context *kcxt,
				  uint32_t *p_hash,
				  sql_interval_t *datum)
{
	if (datum->isnull)
		*p_hash = 0;
	else
	{
		int128_t	ival = __interval_cmp_value(datum);

		*p_hash = pg_hash_any(&ival, sizeof(int128_t));
	}
	return true;
}

PUBLIC_FUNCTION(uint32_t)
devtype_interval_hash(bool isnull, Datum value)
{
	sql_interval_t temp;
	uint32_t	hash;
	DECL_KERNEL_CONTEXT(u,NULL,0);
	if (!sql_interval_datum_ref(&u.kcxt, &temp,
								isnull ? NULL : DatumGetPointer(value)) ||
        !sql_interval_hash(&u.kcxt, &hash, &temp))
		pg_kern_ereport(&u.kcxt);
	return hash;
}
