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

typedef struct {
	int128_t		value;		/* 128bit value */
	int16_t			weight;
	bool			isnull;
} sql_numeric_t;

__PGSTROM_DEVTYPE_FUNCTION_DECLARATION(numeric)

#endif /* XPU_NUMERIC_H */
