/*
 * xpu_numeric.h
 *
 * Collection of numeric functions for xPU (GPU/DPU/SPU)
 * --
 * Copyright 2011-2021 (C) KaiGai Kohei <kaigai@kaigai.gr.jp>
 * Copyright 2014-2021 (C) PG-Strom Developers Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the PostgreSQL License.
 */
#ifndef XPU_NUMERIC_H
#define XPU_NUMERIC_H

/* PostgreSQL numeric data type */
#if 0
#define PG_DEC_DIGITS		1
#define PG_NBASE			10
typedef int8_t		NumericDigit;
#endif

#if 0
#define PG_DEC_DIGITS		2
#define PG_NBASE			100
typedef int8_t		NumericDigit;
#endif

#if 1
#define PG_DEC_DIGITS		4
#define PG_NBASE			10000
typedef int16_t	NumericDigit;
#endif

#define PG_MAX_DIGITS		40	/* Max digits of 128bit integer */
#define PG_MAX_DATA			(PG_MAX_DIGITS / PG_DEC_DIGITS)

struct NumericShort
{
	uint16_t		n_header;				/* Sign + display scale + weight */
	NumericDigit	n_data[PG_MAX_DATA];	/* Digits */
};
typedef struct NumericShort	NumericShort;

struct NumericLong
{
	uint16_t		n_sign_dscale;			/* Sign + display scale */
	int16_t			n_weight;				/* Weight of 1st digit	*/
	NumericDigit	n_data[PG_MAX_DATA];	/* Digits */
};
typedef struct NumericLong	NumericLong;

typedef union
{
	uint16_t		n_header;			/* Header word */
	NumericLong		n_long;				/* Long form (4-byte header) */
	NumericShort	n_short;			/* Short form (2-byte header) */
} NumericChoice;

struct NumericData
{
 	uint32_t		vl_len_;		/* varlena header */
	NumericChoice	choice;			/* payload */
};
typedef struct NumericData	NumericData;


typedef struct {
	int128_t		value;		/* 128bit value */
	int16_t			weight;
	bool			isnull;
} pg_numeric_t;
__PGSTROM_DEVTYPE_SUPPORT_FUNCTIONS(numeric)

INLINE_FUNCTION(uint32_t)
NUMERIC_NDIGITS(varlena *numeric)
{
	NumericChoice *nc = (NumericChoice *)VARDATA_ANY(numeric);
	int32_t		nc_len = VARSIZE_ANY_EXHDR(numeric);
	uint16_t	n_head = __Fetch(&nc->n_header);

	return (NUMERIC_IS_SHORT(n_head)
			? (nc_len - offsetof(NumericChoice, n_short.n_data))
			: (nc_len - offsetof(NumericChoice, n_long.n_data)))
		/ sizeof(NumericDigit);
}

INLINE_FUNCTION(NumericDigit *)
NUMERIC_DIGITS(varlena *numeric)		/* may not be aligned */
{
	NumericChoice *nc = (NumericChoice *)VARDATA_ANY(numeric);
	uint16_t	n_head = __Fetch(&nc->n_header);

	return NUMERIC_IS_SHORT(n_head) ? nc->n_short.n_data : nc->n_long.n_data;
}

INLINE_FUNCTION(int)
NUMERIC_SIGN(varlena *numeric)
{
	NumericChoice *nc = (NumericChoice *)VARDATA_ANY(numeric);
	uint16_t	n_head = __Fetch(&nc->n_header);

	if (NUMERIC_IS_SHORT(n_head))
		return (n_head & NUMERIC_SHORT_SIGN_MASK) ? NUMERIC_NEG : NUMERIC_POS;
	return NUMERIC_FLAGBITS(n_head);
}

INLINE_FUNCTION(uint32_t)
NUMERIC_DSCALE(varlena *numeric)
{
	NumericChoice *nc = (NumericChoice *)VARDATA_ANY(numeric);
	uint16_t	n_head = __Fetch(&nc->n_header);
	uint32_t	dscale;

	if (NUMERIC_IS_SHORT(n_head))
	{
		dscale = (n_head & NUMERIC_SHORT_DSCALE_MASK) >> NUMERIC_SHORT_DSCALE_SHIFT;
	}
	else
	{
		dscale = __Fetch(&nc->n_long.n_sign_dscale) & NUMERIC_DSCALE_MASK;
	}
	return dscale;
}

INLINE_FUNCTION(int)
NUMERIC_WEIGHT(varlena *numeric)
{
	NumericChoice *nc = (NumericChoice *)VARDATA_ANY(numeric);
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

#endif /* XPU_NUMERIC_H */
