/*
 * xpu_numeric.c
 *
 * Collection of numeric type support on XPU(GPU/DPU/SPU)
 * ----
 * Copyright 2011-2022 (C) KaiGai Kohei <kaigai@kaigai.gr.jp>
 * Copyright 2014-2022 (C) PG-Strom Developers Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the PostgreSQL License.
 *
 */
#include "xpu_devtypes.h"

#define NUMERIC_SIGN_MASK	0xC000
#define NUMERIC_POS			0x0000
#define NUMERIC_NEG			0x4000
#define NUMERIC_SHORT		0x8000
#define NUMERIC_NAN			0xC000

#define NUMERIC_FLAGBITS(n_head)	((n_head) & NUMERIC_SIGN_MASK)
#define NUMERIC_IS_NAN(n_head)		(NUMERIC_FLAGBITS(n_head) == NUMERIC_NAN)
#define NUMERIC_IS_SHORT(n_head)	(NUMERIC_FLAGBITS(n_head) == NUMERIC_SHORT)

#define NUMERIC_SHORT_SIGN_MASK		0x2000
#define NUMERIC_SHORT_DSCALE_MASK	0x1F80
#define NUMERIC_SHORT_DSCALE_SHIFT	7
#define NUMERIC_SHORT_DSCALE_MAX	(NUMERIC_SHORT_DSCALE_MASK >> \
									 NUMERIC_SHORT_DSCALE_SHIFT)
#define NUMERIC_SHORT_WEIGHT_SIGN_MASK 0x0040
#define NUMERIC_SHORT_WEIGHT_MASK	0x003F
#define NUMERIC_SHORT_WEIGHT_MAX	NUMERIC_SHORT_WEIGHT_MASK
#define NUMERIC_SHORT_WEIGHT_MIN	(-(NUMERIC_SHORT_WEIGHT_MASK+1))

#define NUMERIC_DSCALE_MASK         0x3FFF

INLINE_FUNCTION(uint32_t)
NUMERIC_NDIGITS(NumericChoice *nc, uint32_t nc_len)
{
	uint16_t	n_head = __Fetch(&nc->n_header);

	return (NUMERIC_IS_SHORT(n_head)
			? (nc_len - offsetof(NumericChoice, n_short.n_data))
			: (nc_len - offsetof(NumericChoice, n_long.n_data)))
		/ sizeof(NumericDigit);
}

INLINE_FUNCTION(NumericDigit *)
NUMERIC_DIGITS(NumericChoice *nc)
{
	uint16_t	n_head = __Fetch(&nc->n_header);

	return NUMERIC_IS_SHORT(n_head) ? nc->n_short.n_data : nc->n_long.n_data;
}

INLINE_FUNCTION(int)
NUMERIC_SIGN(NumericChoice *nc)
{
	uint16_t	n_head = __Fetch(&nc->n_header);

	if (NUMERIC_IS_SHORT(n_head))
		return (n_head & NUMERIC_SHORT_SIGN_MASK) ? NUMERIC_NEG : NUMERIC_POS;
	return NUMERIC_FLAGBITS(n_head);
}

INLINE_FUNCTION(uint32_t)
NUMERIC_DSCALE(NumericChoice *nc)
{
	uint16_t	n_head = __Fetch(&nc->n_header);

	if (NUMERIC_IS_SHORT(n_head))
		return ((n_head & NUMERIC_SHORT_DSCALE_MASK) >> NUMERIC_SHORT_DSCALE_SHIFT);
	return (__Fetch(&nc->n_long.n_sign_dscale) & NUMERIC_DSCALE_MASK);
}

INLINE_FUNCTION(int)
NUMERIC_WEIGHT(NumericChoice *nc)
{
	uint16_t	n_head = __Fetch(&nc->n_header);
	int			weight;

	if (NUMERIC_IS_SHORT(n_head))
	{
		weight = (n_head) & NUMERIC_SHORT_WEIGHT_MASK;
		if (n_head & NUMERIC_SHORT_WEIGHT_SIGN_MASK)
			weight |= ~NUMERIC_SHORT_WEIGHT_MASK;
	}
	else
	{
		weight = __Fetch(&nc->n_long.n_weight);
	}
	return weight;
}

INLINE_FUNCTION(void)
set_normalized_numeric(pg_numeric_t *result, int128_t value, int16_t weight)
{
	if (value == 0)
		weight = 0;
	else
	{
		while (value % 10 == 0)
		{
			value /= 10;
			weight--;
		}
	}
	result->isnull = false;
	result->weight = weight;
	result->value = value;
}

STATIC_FUNCTION(int)
pg_numeric_from_varlena(kern_context *kcxt,
						pg_numeric_t *result,
						varlena *addr)
{
	uint32_t	len;

	memset(result, 0, sizeof(pg_numeric_t));
	if (!addr)
	{
		result->isnull = true;
		return 0;
	}
	len = VARSIZE_ANY_EXHDR(addr);
	if (len < sizeof(uint16_t))
	{
		STROM_EREPORT(kcxt, ERRCODE_DATA_CORRUPTED,
					  "corrupted numeric header");
		return -1;
	}
	if (len <= sizeof(NumericChoice))
	{
		NumericChoice *nc = (NumericChoice *)VARDATA_ANY(addr);
		NumericDigit *digits = NUMERIC_DIGITS(nc);
		int			weight  = NUMERIC_WEIGHT(nc) + 1;
		int			i, ndigits = NUMERIC_NDIGITS(nc, len);
		int128_t	value = 0;

		for (i=0; i < ndigits; i++)
		{
			NumericDigit dig = __Fetch(&digits[i]);

			value = value * PG_NBASE + dig;
			if (value < 0)
				goto out_of_range;
		}
		if (NUMERIC_SIGN(nc) == NUMERIC_NEG)
			value = -value;
		weight = PG_DEC_DIGITS * (ndigits - weight);

		set_normalized_numeric(result, value, weight);
		return VARSIZE_ANY(addr);
	}
out_of_range:
	STROM_CPU_FALLBACK(kcxt, ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE,
					   "numeric value is out of range");
	return -1;
}

STATIC_FUNCTION(int32_t)
pg_numeric_to_varlena(char *buffer, int16_t weight, int128_t value)
{
	NumericData	   *numData = (NumericData *)buffer;
	NumericLong	   *numBody = &numData->choice.n_long;
	NumericDigit	n_data[PG_MAX_DATA];
	int				ndigits;
	int32_t			len;
	uint16_t		n_header = (Max(weight, 0) & NUMERIC_DSCALE_MASK);
	bool			is_negative = (value < 0);

	if (is_negative)
		value = -value;

	switch (weight % PG_DEC_DIGITS)
	{
		case 3:
		case -1:
			value *= 10;
			weight += 1;
			break;
		case 2:
		case -2:
			value *= 100;
			weight += 2;
			break;
		case 1:
		case -3:
			value *= 1000;
			weight += 3;
			break;
		default:
			/* ok */
			break;
	}
	Assert(weight % PG_DEC_DIGITS == 0);

	ndigits = 0;
	while (value != 0)
    {
		int		mod;

		mod = (value % PG_NBASE);
		value /= PG_NBASE;
		Assert(ndigits < PG_MAX_DATA);
		ndigits++;
		n_data[PG_MAX_DATA - ndigits] = mod;
	}
	len = offsetof(NumericData, choice.n_long.n_data[ndigits]);

	if (buffer)
	{
		memcpy(numBody->n_data,
			   n_data + PG_MAX_DATA - ndigits,
			   sizeof(NumericDigit) * ndigits);
		if (is_negative)
			n_header |= NUMERIC_NEG;
		numBody->n_sign_dscale = n_header;
		numBody->n_weight = ndigits - (weight / PG_DEC_DIGITS) - 1;

		SET_VARSIZE(numData, len);
	}
	return len;
}

PUBLIC_FUNCTION(int)
pg_numeric_datum_ref(kern_context *kcxt,
					 pg_numeric_t *result,
					 void *addr)
{
	Assert(result != NULL);
	return pg_numeric_from_varlena(kcxt, result, (varlena *)addr);
}

PUBLIC_FUNCTION(int)
pg_numeric_param_ref(kern_context *kcxt,
					 pg_numeric_t *result,
					 uint32_t param_id)
{
	void   *addr = kparam_get_value(kcxt->kparams, param_id);

	return pg_numeric_from_varlena(kcxt, result, (varlena *)addr);
}

PUBLIC_FUNCTION(int)
pg_numeric_datum_ref_arrow(kern_context *kcxt,
						   pg_numeric_t *result,
						   kern_data_store *kds,
						   kern_colmeta *cmeta,
						   uint32_t rowidx)
{
	int128_t   *addr;

	addr = KDS_ARROW_REF_SIMPLE_DATUM(kds, cmeta, rowidx,
									  sizeof(int128_t));
	if (!addr)
		result->isnull = true;
	else
	{
		/*
		 * Note that Decimal::scale is equivalent to numeric::weight.
		 * It is the number of digits after the decimal point.
		 */
		set_normalized_numeric(result, *addr, cmeta->attopts.decimal.scale);
	}
	return sizeof(int128_t);
}

PUBLIC_FUNCTION(int32_t)
pg_numeric_datum_store(kern_context *kcxt,
					   char *buffer,
					   pg_numeric_t *datum)
{
	if (datum->isnull)
		return 0;
	return pg_numeric_to_varlena(buffer, datum->weight, datum->value);
}

PUBLIC_FUNCTION(uint32_t)
pg_numeric_hash(kern_context *kcxt,
				pg_numeric_t *datum)
{
	if (datum->isnull)
		return 0;
	return pg_hash_any((unsigned char *)&datum->value,
					   offsetof(pg_numeric_t, weight) + sizeof(int16_t));
}
