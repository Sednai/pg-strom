/*
 * xpu_textlib.h
 *
 * Misc definitions for xPU(GPU/DPU/SPU).
 * --
 * Copyright 2011-2022 (C) KaiGai Kohei <kaigai@kaigai.gr.jp>
 * Copyright 2014-2022 (C) PG-Strom Developers Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the PostgreSQL License.
 */
#ifndef XPU_TEXTLIB_H
#define XPU_TEXTLIB_H

#ifdef PGSTROM_VARLENA_DEVTYPE_TEMPLATE
PGSTROM_VARLENA_DEVTYPE_TEMPLATE(bytea)
PGSTROM_VARLENA_DEVTYPE_TEMPLATE(text)
PGSTROM_VARLENA_DEVTYPE_TEMPLATE(bpchar)
#endif

#endif  /* XPU_TEXTLIB_H */
