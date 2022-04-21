/*
 * xpu_timelib.h
 *
 * Date and time related definitions for xPU(GPU/DPU/SPU).
 * --
 * Copyright 2011-2021 (C) KaiGai Kohei <kaigai@kaigai.gr.jp>
 * Copyright 2014-2021 (C) PG-Strom Developers Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the PostgreSQL License.
 */
#ifndef XPU_TIMELIB_H
#define XPU_TIMELIB_H

#ifndef DATE_H
typedef int32_t         DateADT;
typedef int64_t         TimeADT;
typedef struct
{
    TimeADT     time;   /* all time units other than months and years */
    int32_t     zone;   /* numeric time zone, in seconds */
} TimeTzADT;
#endif DATE_H
#ifndef DATATYPE_TIMESTAMP_H
typedef int64_t         Timestamp;
typedef int64_t         TimestampTz;
typedef int64_t         TimeOffset;
typedef int32_t			fsec_t;		/* fractional seconds (in microseconds) */

typedef struct
{
    TimeOffset  time;   /* all time units other than days, months and years */
    int32_t     day;    /* days, after time for alignment */
    int32_t     month;  /* months and years, after time for alignment */
} Interval;
#endif


#ifdef PGSTROM_SIMPLE_DEVTYPE_TEMPLATE
PGSTROM_SIMPLE_DEVTYPE_TEMPLATE(date, DateADT)
PGSTROM_SIMPLE_DEVTYPE_TEMPLATE(time, TimeADT)
PGSTROM_SIMPLE_DEVTYPE_TEMPLATE(timetz, TimeTzADT)
PGSTROM_SIMPLE_DEVTYPE_TEMPLATE(timestamp, Timestamp)
PGSTROM_SIMPLE_DEVTYPE_TEMPLATE(timestamptz, TimestampTz)
PGSTROM_SIMPLE_DEVTYPE_TEMPLATE(interval, Interval)
#endif	/* PGSTROM_SIMPLE_DEVTYPE_TEMPLATE */



#endif  /* XPU_TIMELIB_H */
