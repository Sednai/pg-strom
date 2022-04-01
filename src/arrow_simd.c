/*
 * arrow_simd.c
 *
 * Routines for experimental support of CPU SIMD operations at Arrow_Fdw.
 * ----------------------------------------------------------------
 * Copyright 2011-2021 (C) KaiGai Kohei <kaigai@kaigai.gr.jp>
 * Copyright 2014-2021 (C) PG-Strom Developers Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the PostgreSQL License.
 */
#include "pg_strom.h"

#ifdef __x86_64__
#define WITH_SIMD_SUPPORT	1
#endif
#define SIMD_UNITSZ			8
#define SIMD_DECLARE_TYPE(BASE,VECTOR)			\
	typedef BASE VECTOR __attribute__((vector_size(sizeof(BASE) * SIMD_UNITSZ)))
SIMD_DECLARE_TYPE(int8,   int8vec_t);
SIMD_DECLARE_TYPE(int16,  int16vec_t);
SIMD_DECLARE_TYPE(int32,  int32vec_t);
SIMD_DECLARE_TYPE(int64,  int64vec_t);
SIMD_DECLARE_TYPE(float4, float4vec_t);
SIMD_DECLARE_TYPE(float8, float8vec_t);
#define CONST_VECTOR(x)		{ (x), (x), (x), (x), (x), (x), (x), (x) }

struct simdExpressionState;

#define SIMD_EXPR_FUNCTION_ARGS			\
	struct pgstrom_data_store *pds,		\
	uint64_t row_index,					\
	register void *result,				\
	struct simdExpressionState *exps
typedef int8vec_t		nullmap_t;
typedef nullmap_t (*simd_expression_callback_f)(SIMD_EXPR_FUNCTION_ARGS);

typedef struct
{
	Oid		function_oid;
	simd_expression_callback_f expression_cb;
} simdFunctionInfo;

typedef struct simdExpressionState
{
	simd_expression_callback_f expression_cb;
	Node		   *node;
	uint32_t	   *row_map;
	uint32_t		row_index;
	int64_t			row_skipped;
	union {
		struct {
			int		func_nargs;
			struct simdExpressionState *func_args[FLEXIBLE_ARRAY_MEMBER];
		} func;
		struct {
			uint16	var_colidx;
			uint16	var_unitsz;
		} var;
		struct {
			bool	con_isnull;
			int		con_length;
			char	con_buffer[FLEXIBLE_ARRAY_MEMBER];
		} con;
	} u;
} simdExpressionState;

/*
 * static variables/functions
 */
static bool		arrow_fdw_simd_enabled = false;		/* GUC */
static HTAB	   *simd_function_htab = NULL;
static simdExpressionState *arrow_simd_init_expression_state(Node *node);

/*
 * SIMD operator functions
 */
static inline nullmap_t
simd__exec_expression(SIMD_EXPR_FUNCTION_ARGS)
{
	return exps->expression_cb(pds, row_index, result, exps);
}

static nullmap_t
simd__var_refs(SIMD_EXPR_FUNCTION_ARGS)
{
	kern_data_store	*kds = &pds->kds;
	kern_colmeta	*cmeta;
	nullmap_t		nullmap = CONST_VECTOR(0);
	int				i, nvalids = SIMD_UNITSZ;
	char		   *base;
	uint64_t		offset;
	uint64_t		length;

	Assert(kds->format == KDS_FORMAT_ARROW);
	if (exps->u.var.var_colidx >=  kds->ncols)
		return ((nullmap_t)CONST_VECTOR(1));	/* all null */
	cmeta = &kds->colmeta[exps->u.var.var_colidx];
	if (!cmeta->attbyval || cmeta->attlen != exps->u.var.var_unitsz)
		elog(ERROR, "Bug? column %u is not a supported data type (%s)",
			 exps->u.var.var_colidx, format_type_be(cmeta->atttypid));
	if (row_index >= kds->nitems)
		return ((nullmap_t)CONST_VECTOR(1));	/* all null */
	if (row_index + SIMD_UNITSZ > kds->nitems)
		nvalids = row_index + SIMD_UNITSZ - kds->nitems;
	if (cmeta->nullmap_offset != 0)
	{
		uint8  *base = (uint8 *)kds + __kds_unpack(cmeta->nullmap_offset);

		for (i=0; i < nvalids; i++)
			nullmap[i] = !att_isnull(row_index+i, base);
	}
	base = (char *)kds + __kds_unpack(cmeta->values_offset);
	offset = cmeta->attlen * row_index;
	length = __kds_unpack(cmeta->values_length);
	if (offset >= length)
		return ((nullmap_t)CONST_VECTOR(1));	/* all null */
	if (offset + cmeta->attlen * nvalids >= length)
		nvalids = (length - offset) / cmeta->attlen;
	if (nvalids > 0)
		memcpy(result, base + offset, cmeta->attlen * nvalids);
	if (nvalids < SIMD_UNITSZ)
	{
		memset((char *)result + cmeta->attlen * nvalids, 0,
			   cmeta->attlen * (SIMD_UNITSZ - nvalids));
		for (i=nvalids; i < SIMD_UNITSZ; i++)
			nullmap[i] = true;
	}
	return nullmap;
}

static nullmap_t
simd__const_refs(SIMD_EXPR_FUNCTION_ARGS)
{
	memcpy(result, exps->u.con.con_buffer, exps->u.con.con_length);
	return (exps->u.con.con_isnull 
			? (nullmap_t)CONST_VECTOR(1)
			: (nullmap_t)CONST_VECTOR(0));
}

static int8vec_t
simd__int2_to_int4(SIMD_EXPR_FUNCTION_ARGS)
{
	int8vec_t	nullmap;
	int16vec_t	a;
	int32vec_t	r;
	uint32_t	i;

	Assert(exps->u.func.func_nargs == 1);
	nullmap = simd__exec_expression(pds, row_index, &a, exps->u.func.func_args[0]);
	for (i=0; i < SIMD_UNITSZ; i++)
		r[i] = (int32_t)a[i];
	memcpy(result, &r, sizeof(r));
	return nullmap;
}

static int8vec_t
simd__int2_to_int8(SIMD_EXPR_FUNCTION_ARGS)
{
	int8vec_t	nullmap;
	int16vec_t	a;
	int64vec_t	r;
	uint32_t	i;

	Assert(exps->u.func.func_nargs == 1);
	nullmap = simd__exec_expression(pds, row_index, &a, exps->u.func.func_args[0]);
	for (i=0; i < SIMD_UNITSZ; i++)
		r[i] = (int64_t)a[i];
	memcpy(result, &r, sizeof(r));
	return nullmap;
}

static int8vec_t
simd__int4_to_int2(SIMD_EXPR_FUNCTION_ARGS)
{
	int8vec_t	nullmap;
	int32vec_t	a;
	int16vec_t	r;
	uint32_t	i;

	Assert(exps->u.func.func_nargs == 1);
	nullmap = simd__exec_expression(pds, row_index, &a, exps->u.func.func_args[0]);
	for (i=0; i < SIMD_UNITSZ; i++)
	{
		if (nullmap[i])
			continue;
		if (a[i] < SHRT_MIN || a[i] > SHRT_MAX)
			elog(ERROR, "%d is out of range for int16", a[i]);
		r[i] = (int16_t)a[i];
	}
	memcpy(result, &r, sizeof(r));
	return nullmap;
}

static int8vec_t
simd__int4_to_int8(SIMD_EXPR_FUNCTION_ARGS)
{
	int8vec_t	nullmap;
	int32vec_t	a;
	int64vec_t	r;
	int			i;

	Assert(exps->u.func.func_nargs == 1);
	nullmap = simd__exec_expression(pds, row_index, &a, exps->u.func.func_args[0]);
	for (i=0; i < SIMD_UNITSZ; i++)
		r[i] = (int64_t)a[i];
	memcpy(result, &r, sizeof(r));
	return nullmap;
}

static int8vec_t
simd__int8_to_int2(SIMD_EXPR_FUNCTION_ARGS)
{
	int8vec_t	nullmap;
	int64vec_t	a;
	int16vec_t	r;
	int			i;

	Assert(exps->u.func.func_nargs == 1);
	nullmap = simd__exec_expression(pds, row_index, &a, exps->u.func.func_args[0]);
	for (i=0; i < SIMD_UNITSZ; i++)
	{
		if (nullmap[i])
			continue;
		if (a[i] < SHRT_MIN || a[i] > SHRT_MAX)
			elog(ERROR, "%ld is out of range for int16", a[i]);
		r[i] = (int16_t)a[i];
	}
	memcpy(result, &r, sizeof(r));
	return nullmap;
}

static int8vec_t
simd__int8_to_int4(SIMD_EXPR_FUNCTION_ARGS)
{
	int8vec_t	nullmap;
	int64vec_t	a;
	int32vec_t	r;
	int			i;

	Assert(exps->u.func.func_nargs == 1);
	nullmap = simd__exec_expression(pds, row_index, &a, exps->u.func.func_args[0]);
	for (i=0; i < SIMD_UNITSZ; i++)
	{
		if (nullmap[i])
			continue;
		if (a[i] < INT_MIN || a[i] > INT_MAX)
			elog(ERROR, "%ld is out of range for int32", a[i]);
		r[i] = (int32_t)a[i];
	}
	memcpy(result, &r, sizeof(r));
	return nullmap;
}

#define SIMD_COMPARE_TEMPLATE(FUNC_NAME,TYPE_NAME, OPER)				\
	static int8vec_t													\
	simd__##FUNC_NAME(SIMD_EXPR_FUNCTION_ARGS)							\
	{																	\
		TYPE_NAME	a, b, c;											\
		int8vec_t	r, nullmap;											\
		int			i;													\
																		\
		Assert(exps->u.func.func_nargs == 2);							\
		nullmap = (simd__exec_expression(pds, row_index, &a,			\
										 exps->u.func.func_args[0]) |	\
				   simd__exec_expression(pds, row_index, &b,			\
										 exps->u.func.func_args[1]));	\
		c = (a OPER b);													\
		for (i=0; i < SIMD_UNITSZ; i++)									\
		{																\
			if (!nullmap[i] && c[i] != 0)								\
				r[i] = true;											\
			else														\
				r[i] = false;											\
		}																\
		memcpy(result, &r, sizeof(r));									\
		return nullmap;													\
	}

SIMD_COMPARE_TEMPLATE(int2eq, int16vec_t, ==)
SIMD_COMPARE_TEMPLATE(int4eq, int32vec_t, ==)
SIMD_COMPARE_TEMPLATE(int8eq, int64vec_t, ==)
SIMD_COMPARE_TEMPLATE(float4eq, float4vec_t, ==)
SIMD_COMPARE_TEMPLATE(float8eq, float8vec_t, ==)

SIMD_COMPARE_TEMPLATE(int2ne, int16vec_t, !=)
SIMD_COMPARE_TEMPLATE(int4ne, int32vec_t, !=)
SIMD_COMPARE_TEMPLATE(int8ne, int64vec_t, !=)
SIMD_COMPARE_TEMPLATE(float4ne, float4vec_t, !=)
SIMD_COMPARE_TEMPLATE(float8ne, float8vec_t, !=)

SIMD_COMPARE_TEMPLATE(int2ge, int16vec_t, >=)
SIMD_COMPARE_TEMPLATE(int4ge, int32vec_t, >=)
SIMD_COMPARE_TEMPLATE(int8ge, int64vec_t, >=)
SIMD_COMPARE_TEMPLATE(float4ge, float4vec_t, >=)
SIMD_COMPARE_TEMPLATE(float8ge, float8vec_t, >=)

SIMD_COMPARE_TEMPLATE(int2gt, int16vec_t, >)
SIMD_COMPARE_TEMPLATE(int4gt, int32vec_t, >)
SIMD_COMPARE_TEMPLATE(int8gt, int64vec_t, >)
SIMD_COMPARE_TEMPLATE(float4gt, float4vec_t, >)
SIMD_COMPARE_TEMPLATE(float8gt, float8vec_t, >)

SIMD_COMPARE_TEMPLATE(int2le, int16vec_t, <=)
SIMD_COMPARE_TEMPLATE(int4le, int32vec_t, <=)
SIMD_COMPARE_TEMPLATE(int8le, int64vec_t, <=)
SIMD_COMPARE_TEMPLATE(float4le, float4vec_t, <=)
SIMD_COMPARE_TEMPLATE(float8le, float8vec_t, <=)

SIMD_COMPARE_TEMPLATE(int2lt, int16vec_t, <)
SIMD_COMPARE_TEMPLATE(int4lt, int32vec_t, <)
SIMD_COMPARE_TEMPLATE(int8lt, int64vec_t, <)
SIMD_COMPARE_TEMPLATE(float4lt, float4vec_t, <)
SIMD_COMPARE_TEMPLATE(float8lt, float8vec_t, <)

static int8vec_t
simd__boolop_and(SIMD_EXPR_FUNCTION_ARGS)
{
	int8vec_t	v, rv = CONST_VECTOR(1);
	nullmap_t	n, nullmap = CONST_VECTOR(0);
	int			i, j;

	for (i=0; i < exps->u.func.func_nargs; i++)
	{
		n = simd__exec_expression(pds, row_index, &v,
								  exps->u.func.func_args[i]);
		for (j=0; j < SIMD_UNITSZ; j++)
		{
			if (n[j])
			{
				if (nullmap[j] || rv[j])
					nullmap[j] = true;
			}
			else if (nullmap[j])
			{
				if (!v[j])
				{
					nullmap[j] = false;
					rv[j] = false;
				}
			}
			else
			{
				rv[j] &= v[j];
			}
		}
	}
	memcpy(result, &rv, sizeof(rv));
	return nullmap;
}

static nullmap_t
simd__boolop_or(SIMD_EXPR_FUNCTION_ARGS)
{
	int8vec_t	v, rv = CONST_VECTOR(1);
	nullmap_t	n, nullmap = CONST_VECTOR(0);
	int			i, j;

	for (i=0; i < exps->u.func.func_nargs; i++)
	{
		n = simd__exec_expression(pds, row_index, &v,
								  exps->u.func.func_args[i]);
		for (j=0; j < SIMD_UNITSZ; j++)
		{
			if (n[j])
			{
				if (nullmap[j] || !rv[j])
					nullmap[j] = true;
			}
			else if (nullmap[j])
			{
				if (v[j])
				{
					nullmap[j] = false;
					rv[j] = true;
				}
			}
			else
			{
				rv[j] |= v[j];
			}
		}
	}
	memcpy(result, &rv, sizeof(rv));
	return nullmap;
}

static nullmap_t
simd__boolop_not(SIMD_EXPR_FUNCTION_ARGS)
{
	int8vec_t	rv;
	nullmap_t	nullmap;

	Assert(exps->u.func.func_nargs == 1);
	nullmap = simd__exec_expression(pds, row_index, &rv,
									exps->u.func.func_args[0]);
	rv = (rv == false);
	memcpy(result, &rv, sizeof(rv));
	return nullmap;
}

/*
 * new more SIMD functions here
 */

static struct {
	const char *func_name;
	int			func_nargs;
	Oid			func_argtypes[4];
	simd_expression_callback_f expression_cb;
} simd_function_catalog[] = {
	/* cast operations */
	{"int4",   1, {INT2OID},   simd__int2_to_int4 },
	{"int8",   1, {INT2OID},   simd__int2_to_int8 },
	{"int2",   1, {INT4OID},   simd__int4_to_int2 },
	{"int8",   1, {INT4OID},   simd__int4_to_int8 },
	{"int2",   1, {INT8OID},   simd__int8_to_int2 },
	{"int4",   1, {INT8OID},   simd__int8_to_int4 },
	/* '=' operators */
	{"int2eq",     2, {INT2OID, INT2OID}, simd__int2eq },
	{"int4eq",     2, {INT4OID, INT4OID}, simd__int4eq },
	{"int8eq",     2, {INT8OID, INT8OID}, simd__int8eq },
	{"float4eq",   2, {FLOAT4OID, FLOAT4OID}, simd__float4eq },
	{"float8eq",   2, {FLOAT8OID, FLOAT8OID}, simd__float8eq },
	/* '<>' operators */
	{"int2ne",     2, {INT2OID, INT2OID}, simd__int2ne },
	{"int4ne",     2, {INT4OID, INT4OID}, simd__int4ne },
	{"int8ne",     2, {INT8OID, INT8OID}, simd__int8ne },
	{"float4ne",   2, {FLOAT4OID, FLOAT4OID}, simd__float4ne },
	{"float8ne",   2, {FLOAT8OID, FLOAT8OID}, simd__float8ne },
	/* '>=' operators */
	{"int2ge",     2, {INT2OID, INT2OID}, simd__int2ge },
	{"int4ge",     2, {INT4OID, INT4OID}, simd__int4ge },
	{"int8ge",     2, {INT8OID, INT8OID}, simd__int8ge },
	{"float4ge",   2, {FLOAT4OID, FLOAT4OID}, simd__float4ge },
	{"float8ge",   2, {FLOAT8OID, FLOAT8OID}, simd__float8ge },
	/* '>' operators */
	{"int2gt",     2, {INT2OID, INT2OID}, simd__int2gt },
	{"int4gt",     2, {INT4OID, INT4OID}, simd__int4gt },
	{"int8gt",     2, {INT8OID, INT8OID}, simd__int8gt },
	{"float4gt",   2, {FLOAT4OID, FLOAT4OID}, simd__float4gt },
	{"float8gt",   2, {FLOAT8OID, FLOAT8OID}, simd__float8gt },
	/* '<=' operators */
	{"int2le",     2, {INT2OID, INT2OID}, simd__int2le },
	{"int4le",     2, {INT4OID, INT4OID}, simd__int4le },
	{"int8le",     2, {INT8OID, INT8OID}, simd__int8le },
	{"float4le",   2, {FLOAT4OID, FLOAT4OID}, simd__float4le },
	{"float8le",   2, {FLOAT8OID, FLOAT8OID}, simd__float8le },
	/* '<' operators */
	{"int2lt",     2, {INT2OID, INT2OID}, simd__int2lt },
	{"int4lt",     2, {INT4OID, INT4OID}, simd__int4lt },
	{"int8lt",     2, {INT8OID, INT8OID}, simd__int8lt },
	{"float4lt",   2, {FLOAT4OID, FLOAT4OID}, simd__float4lt },
	{"float8lt",   2, {FLOAT8OID, FLOAT8OID}, simd__float8lt },
	{NULL, 0, {}, NULL},
};
	
static simd_expression_callback_f
simdFunctionCacheLookup(Oid function_oid)
{
	simdFunctionInfo *fent;
	bool	found;
	int		i;

	if (!simd_function_htab)
	{
		HASHCTL		hctl;

		memset(&hctl, 0, sizeof(HASHCTL));
		hctl.keysize = sizeof(Oid);
		hctl.entrysize = sizeof(simdFunctionInfo);
		hctl.hcxt = CacheMemoryContext;
		simd_function_htab = hash_create("SIMD Functions Cache",
										 256, &hctl,
										 HASH_ELEM |
										 HASH_BLOBS |
										 HASH_CONTEXT);
	}

	fent = hash_search(simd_function_htab, &function_oid, HASH_ENTER, &found);
	if (!found)
	{
		HeapTuple	htup;
		Form_pg_proc proc;

		htup = SearchSysCache1(PROCOID, ObjectIdGetDatum(function_oid));
		if (!HeapTupleIsValid(htup))
			elog(ERROR, "cache lookup failed for function %u", function_oid);
		proc = (Form_pg_proc) GETSTRUCT(htup);

		fent->expression_cb = NULL;
		for (i=0; simd_function_catalog[i].func_name != NULL; i++)
		{
			const char *func_name = simd_function_catalog[i].func_name;
			int			func_nargs = simd_function_catalog[i].func_nargs;
			Oid		   *func_argtypes = simd_function_catalog[i].func_argtypes;

			if (strcmp(NameStr(proc->proname), func_name) == 0 &&
				func_nargs == proc->pronargs &&
				(func_nargs == 0 ||
				 memcmp(proc->proargtypes.values, func_argtypes,
						sizeof(Oid) * func_nargs) == 0))
			{
				fent->expression_cb = simd_function_catalog[i].expression_cb;
				break;
			}
		}
		ReleaseSysCache(htup);
	}
	return fent->expression_cb;
}

static void
simdFunctionCacheCallback(Datum arg, int cacheid, uint32 hashvalue)
{
	if (simd_function_htab)
	{
		hash_destroy(simd_function_htab);
		simd_function_htab = NULL;
	}
}

/*
 * arrow_simd_init_expression_state
 */
static simdExpressionState *
__arrow_simd_init_var_state(Var *var)
{
	simdExpressionState *exps = NULL;

	if (var->varattno > 0 && (var->vartype == INT2OID ||
							  var->vartype == INT4OID ||
							  var->vartype == INT8OID ||
							  var->vartype == FLOAT4OID ||
							  var->vartype == FLOAT8OID))
	{
		exps = palloc0(sizeof(simdExpressionState));
		exps->expression_cb = simd__var_refs;
		exps->u.var.var_colidx = var->varattno - 1;
		exps->u.var.var_unitsz = get_typlen(var->vartype);
	}
	return exps;
}

static simdExpressionState *
__arrow_simd_init_const_state(Const *con)
{
	simdExpressionState *exps = NULL;
	int		i;

#define __SETUP_CONST_EXPS(TYPE_OID,VEC_TYPE,DATUM_REF)				\
	case TYPE_OID:													\
	{																\
		VEC_TYPE	v;												\
																	\
		exps = palloc0(offsetof(simdExpressionState,				\
								u.con.con_buffer) + sizeof(v));		\
		exps->expression_cb = simd__const_refs;						\
		if (con->constisnull)										\
			exps->u.con.con_isnull = true;							\
		else														\
		{															\
			exps->u.con.con_isnull = false;							\
			for (i=0; i < SIMD_UNITSZ; i++)							\
				v[i] = DATUM_REF(con->constvalue);					\
			memcpy(exps->u.con.con_buffer, &v, sizeof(v));			\
		}															\
		exps->u.con.con_length = sizeof(v);							\
	}																\
	break

	switch (con->consttype)
	{
		__SETUP_CONST_EXPS(INT2OID,int16vec_t,DatumGetInt16);
		__SETUP_CONST_EXPS(INT4OID,int32vec_t,DatumGetInt32);
		__SETUP_CONST_EXPS(INT8OID,int64vec_t,DatumGetInt64);
		__SETUP_CONST_EXPS(FLOAT4OID,float4vec_t,DatumGetFloat4);
		__SETUP_CONST_EXPS(FLOAT8OID,float8vec_t,DatumGetFloat8);
		default:
			break;
	}
#undef __SETUP_CONST_EXPS
	return exps;
}

static simdExpressionState *
__arrow_simd_init_function_state(Oid func_oid,
								 List *func_args)
{
	simdExpressionState *exps;
	ListCell   *lc;
	int			i, nargs;
	simd_expression_callback_f expression_cb;

	expression_cb = simdFunctionCacheLookup(func_oid);
	if (!expression_cb)
		return NULL;
	nargs = list_length(func_args);
	exps = palloc0(offsetof(simdExpressionState,
							u.func.func_args[nargs]));
	exps->expression_cb = expression_cb;
	exps->u.func.func_nargs = nargs;
	i = 0;
	foreach (lc, func_args)
	{
		simdExpressionState *__exps;

		__exps = arrow_simd_init_expression_state((Node *)lfirst(lc));
		if (!__exps)
			return NULL;
		exps->u.func.func_args[i++] = __exps;
	}
	return exps;
}

static simdExpressionState *
__arrow_simd_init_bool_state(BoolExpr *b)
{
	simdExpressionState *exps = NULL;
	int			i, nargs = list_length(b->args);
	ListCell   *lc;

	exps = palloc0(offsetof(simdExpressionState,
							u.func.func_args[nargs]));
	switch (b->boolop)
	{
		case AND_EXPR:
			exps->expression_cb = simd__boolop_and;
			break;
		case OR_EXPR:
			exps->expression_cb = simd__boolop_or;
			break;
		case NOT_EXPR:
			exps->expression_cb = simd__boolop_not;
			break;
		default:
			return NULL;
	}

	i = 0;
	foreach (lc, b->args)
	{
		simdExpressionState *__exps;

		__exps = arrow_simd_init_expression_state(lfirst(lc));
		if (!__exps)
			return NULL;
		exps->u.func.func_args[i++] = __exps;
	}
	exps->u.func.func_nargs = nargs;

	return exps;
}

static simdExpressionState *
arrow_simd_init_expression_state(Node *node)
{
	if (!arrow_fdw_simd_enabled || !node)
		return NULL;
	switch (nodeTag(node))
	{
		case T_Var:
			return __arrow_simd_init_var_state((Var *)node);
		case T_Const:
			return __arrow_simd_init_const_state((Const *)node);
		case T_FuncExpr:
			{
				FuncExpr   *f = (FuncExpr *)node;
				return __arrow_simd_init_function_state(f->funcid, f->args);
			}
		case T_OpExpr:
		case T_DistinctExpr:
			{
				OpExpr	   *op = (OpExpr *)node;
				return __arrow_simd_init_function_state(get_opcode(op->opno),
														op->args);
			}
		case T_BoolExpr:
			return __arrow_simd_init_bool_state((BoolExpr *)node);
		default:
			break;
	}
	return NULL;
}

/*
 * arrowSimdExecInit
 */
void *
arrowSimdExecInit(List *quals)
{
	simdExpressionState *exps = NULL;
	List	   *node_list = NIL;
	List	   *exps_list = NIL;
	ListCell   *lc;

	foreach (lc, quals)
	{
		Node   *node = lfirst(lc);
		simdExpressionState *__exps;

		__exps = arrow_simd_init_expression_state(node);
		if (__exps)
		{
			node_list = lappend(node_list, node);
			exps_list = lappend(exps_list, __exps);
		}
	}

	if (list_length(exps_list) == 0)
		return NULL;
	if (list_length(exps_list) == 1)
	{
		exps = linitial(exps_list);
		exps->node = linitial(node_list);
	}
	else
	{
		int		i, nargs = list_length(exps_list);

		exps = palloc0(offsetof(simdExpressionState,
								u.func.func_args[nargs]));
		exps->expression_cb = simd__boolop_and;
		exps->node = (Node *)make_andclause(node_list);
		i = 0;
		foreach (lc, exps_list)
			exps->u.func.func_args[i++] = lfirst(lc);
		exps->u.func.func_nargs = nargs;
	}
	exps->row_map = NULL;
	exps->row_index = 0;
	exps->row_skipped = 0;

	return exps;
}

void
arrowSimdExecChunk(void *simd_state, pgstrom_data_store *pds)
{
	simdExpressionState *exps = simd_state;

	if (exps)
	{
		uint32_t   *row_map;
		uint32_t	i, j, k;
		int8vec_t	eval, nullmap;
		uint32_t	skipped = 0;

		row_map = MemoryContextAlloc(GetMemoryChunkContext(exps),
									 sizeof(uint32_t) * (pds->kds.nitems+1));
		for (i=0, j=0; i < pds->kds.nitems; i += SIMD_UNITSZ)
		{
			nullmap = simd__exec_expression(pds, i, &eval, exps);
			for (k=0; k < SIMD_UNITSZ; k++)
			{
				if (!nullmap[k] && eval[k])
					row_map[j++] = i+k;
				else
					skipped++;
			}
		}
		row_map[j++] = UINT_MAX;	/* terminator */
		if (exps->row_map)
			pfree(exps->row_map);
		exps->row_map = row_map;
		exps->row_index = 0;
		exps->row_skipped += skipped;
	}
}

uint32_t
arrowSimdExecNext(void *simd_state)
{
	simdExpressionState *exps = simd_state;

	Assert(exps != NULL);
	if (!exps->row_map)
		return UINT_MAX;
	return exps->row_map[exps->row_index++];
}

/*
 * arrowSimdExplain
 */
void
arrowSimdExplain(void *simd_state, ExplainState *es, List *dcontext)
{
	simdExpressionState *exps = simd_state;
	char	   *temp;

	if (!exps)
		return;
	temp = deparse_expression(exps->node, dcontext, false, false);
	ExplainPropertyText("SIMD-Quals", temp, es);
	if (exps->row_map)
		ExplainPropertyInteger("Rows Removed by SIMD", "rows", exps->row_skipped, es);
}

/*
 * pgstrom_init_arrow_simd
 */
void
pgstrom_init_arrow_simd(void)
{
#ifdef WITH_SIMD_SUPPORT
	GucContext	guc_context = PGC_INTERNAL;
	bool		guc_default = false;
	FILE	   *filp;

	/* check /proc/cpuinfo */
	filp = fopen("/proc/cpuinfo", "r");
	if (filp != NULL)
	{
		char	linebuf[2048];
		char   *tok, *pos;

		while (guc_context == PGC_INTERNAL &&
			   fgets(linebuf, sizeof(linebuf), filp) != NULL)
		{
			tok = strtok_r(linebuf, ":", &pos);
			tok = __trim(tok);
			if (strcmp(tok, "flags") != 0)
				continue;
			for (;;)
			{
				tok = strtok_r(NULL, " ", &pos);
				if (!tok)
					break;
				tok = __trim(tok);
				if (strcmp(tok, "avx") == 0)
				{
					guc_context = PGC_USERSET;
					guc_default = true;
					break;
				}
			}
		}
		fclose(filp);
	}
	DefineCustomBoolVariable("arrow_fdw.simd_enabled",
							 "Enables CPU SIMD operations",
							 NULL,
							 &arrow_fdw_simd_enabled,
							 guc_default,
							 guc_context,
							 GUC_NOT_IN_SAMPLE,
							 NULL, NULL, NULL);
	CacheRegisterSyscacheCallback(PROCOID, simdFunctionCacheCallback, 0);
#endif
}
