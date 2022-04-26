/*
 * xpu_opcodes.h
 *
 * collection of built-in xPU opcode
 * ----
 * Copyright 2011-2022 (C) KaiGai Kohei <kaigai@kaigai.gr.jp>
 * Copyright 2014-2022 (C) PG-Strom Developers Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the PostgreSQL License.
 */
#ifndef EXPR_OPCODE
#define EXPR_OPCODE(NAME)
#endif
#ifndef TYPE_OPCODE
#define TYPE_OPCODE(NAME,OID,EXTENSION)
#endif
#ifndef FUNC_OPCODE
#define FUNC_OPCODE()
#endif

/*
 * PostgreSQL Expressions
 */
EXPR_OPCODE(Var)
EXPR_OPCODE(Const)
EXPR_OPCODE(Param)
EXPR_OPCODE(FuncExpr)
EXPR_OPCODE(OpExpr)
EXPR_OPCODE(BoolExpr)

/*
 * PostgreSQL Device Types
 */
TYPE_OPCODE(bool, BOOLOID, NULL)
TYPE_OPCODE(int1, INT1OID, "pg_strom")
TYPE_OPCODE(int2, INT2OID, NULL)
TYPE_OPCODE(int4, INT4OID, NULL)
TYPE_OPCODE(int8, INT8OID, NULL)
TYPE_OPCODE(float2, FLOAT2OID, "pg_strom")
TYPE_OPCODE(float4, FLOAT4OID, NULL)
TYPE_OPCODE(float8, FLOAT8OID, NULL)
TYPE_OPCODE(numeric, NUMERICOID, NULL)
TYPE_OPCODE(bytea, BYTEAOID, NULL)
TYPE_OPCODE(text, TEXTOID, NULL)
TYPE_OPCODE(varchar, VARCHAROID, NULL)
TYPE_OPCODE(bpchar, BPCHAROID, NULL)
TYPE_OPCODE(date, DATEOID, NULL)
TYPE_OPCODE(time, TIMEOID, NULL)
TYPE_OPCODE(timetz, TIMETZOID, NULL)
TYPE_OPCODE(timestamp, TIMESTAMPOID, NULL)
TYPE_OPCODE(timestamptz, TIMESTAMPTZOID, NULL)
TYPE_OPCODE(interval, INTERVALOID, NULL)
TYPE_OPCODE(money, MONEYOID, NULL)
TYPE_OPCODE(uuid, UUIDOID, NULL)
TYPE_OPCODE(macaddr, MACADDROID, NULL)
TYPE_OPCODE(inet, INETOID, NULL)

/*
 * PostgreSQL Device Functions / Operators
 */
