/*
 * Copyright (©) 2024 yejq.jiaqiang@gmail.com
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef RUST_EXTEND_H
#define RUST_EXTEND_H 1

#ifdef __cplusplus
extern "C" {
#endif

#define MVALUE_TYPE_NIL       0x0
#define MVALUE_TYPE_BOOL      0x1
#define MVALUE_TYPE_INT64     0x2
#define MVALUE_TYPE_UINT64    0x3
#define MVALUE_TYPE_F32       0x4
#define MVALUE_TYPE_F64       0x5
#define MVALUE_TYPE_STRING    0x6
#define MVALUE_TYPE_MASK      0xf
#define RUSTEX_STRLEN_MAX     0x0FFFFFFFu
#define RUSTEX_STRLEN_MASK    0xFFFFFFF0u

union rext_val {
	int                       mval_bool;
	long long                 mval_int;
	unsigned long long        mval_uint;
	float                     mval_f32;
	double                    mval_f64;
	char *                    mval_strp;
};

struct rext_var {
	unsigned int              mvar_type;
	union rext_val            mvar_val;
};

typedef void * rext_any_t;

rext_any_t rext_lua_new(const struct rext_var * lvar, int * errp);

int rext_upval_set(rext_any_t anyt, const struct rext_var * rfunc,
	int upindex, const struct rext_var * rval);

/* function to retrieve upvalue name at index `upindex */
int rext_upval_get(rext_any_t anyt, const struct rext_var * rfunc,
	int upindex, struct rext_var * valname);

int rext_gettop(rext_any_t anyt);

int rext_settop(rext_any_t anyt, int ntop);

int rext_istable(rext_any_t anyt, int stkidx);

int rext_isfunc(rext_any_t anyt, int stkidx);

int rext_getglobal(rext_any_t anyt, const struct rext_var * gvar);

int rext_pushval(rext_any_t anyt, const struct rext_var * lvar);

int rext_getval(rext_any_t anyt, int stkidx, struct rext_var * resp);

int rext_tpushval(rext_any_t anyt, int maxnum,
	const struct rext_var * tabkey, const struct rext_var * tabval);

int rext_tgetval(rext_any_t anyt, int tabidx,
	const struct rext_var * tabkey, struct rext_var * tabval);

int rext_call(rext_any_t anyt, const struct rext_var * rfunc,
	const struct rext_var * args, struct rext_var * resp);

void rext_var_free(struct rext_var * mvar);

void rext_var_init(struct rext_var * mvar);

/* create a string of type `rext_var */
int rext_var_strbuf(struct rext_var * mvar,
	const char * pbuf, unsigned int plen);

char * rext_var_str(const struct rext_var * mvar, unsigned int * plen);

/* borrow string from `rext_var */
const char * rext_bor_str(const struct rext_var * mvar, unsigned int * plen);

double rext_strtof(const struct rext_var * mvar, int * errp);

long long rext_strtol(const struct rext_var * mvar, int * errp, int base);

unsigned long long rext_strtoul(const struct rext_var * mvar, int * errp, int base);

#ifdef __cplusplus
}
#endif
#endif
