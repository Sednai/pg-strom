/*
 * xpu_misclib.h
 *
 * Misc definitions for xPU(GPU/DPU/SPU).
 * --
 * Copyright 2011-2021 (C) KaiGai Kohei <kaigai@kaigai.gr.jp>
 * Copyright 2014-2021 (C) PG-Strom Developers Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the PostgreSQL License.
 */
#ifndef XPU_MISCLIB_H
#define XPU_MISCLIB_H
#include <sys/socket.h>

/* pg_uuid_t */
#ifndef UUID_H
#define UUID_LEN		16
typedef struct
{
	uint8_t		data[UUID_LEN];
} pgsql_uuid_t;
#endif	/* UUID_H */

/* pg_macaddr_t */
#ifndef INET_H
typedef struct
{
	uint8_t		a;
	uint8_t		b;
	uint8_t		c;
	uint8_t		d;
	uint8_t		e;
	uint8_t		f;
} macaddr;

/* pg_inet_t */
typedef struct
{
	uint8_t		family;		/* PGSQL_AF_INET or PGSQL_AF_INET6 */
	uint8_t		bits;		/* number of bits in netmask */
	uint8_t		ipaddr[16];	/* up to 128 bits of address */
} inet_struct;

#define PGSQL_AF_INET		(AF_INET + 0)
#define PGSQL_AF_INET6		(AF_INET + 1)

typedef struct
{
	uint32_t	vl_len_; /* Do not touch this field directly! */
	inet_struct	inet_data;
} inet;

#define ip_family(inetptr)		(inetptr)->family
#define ip_bits(inetptr)		(inetptr)->bits
#define ip_addr(inetptr)		(inetptr)->ipaddr
#define ip_addrsize(inetptr)	\
	((inetptr)->family == PGSQL_AF_INET ? 4 : 16)
#define ip_maxbits(inetptr)		\
	((inetptr)->family == PGSQL_AF_INET ? 32 : 128)
#endif	/* INET_H */

PGSTROM_SIMPLE_DEVTYPE_DECLARATION(money, int64_t)
PGSTROM_SIMPLE_DEVTYPE_DECLARATION(uuid, pgsql_uuid_t)
PGSTROM_SIMPLE_DEVTYPE_DECLARATION(macaddr, macaddr)
PGSTROM_SIMPLE_DEVTYPE_DECLARATION(inet, inet_struct)

#endif	/* XPU_MISCLIB_H */
