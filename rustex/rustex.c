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

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <math.h>
#include "rustex.h"

static inline unsigned int rext_var_type(const struct rext_var * rvar)
{
	return rvar->mvar_type & MVALUE_TYPE_MASK;
}

static inline unsigned int rext_var_strlen(const struct rext_var * rvar)
{
	if (rext_var_type(rvar) == MVALUE_TYPE_STRING)
		return (rvar->mvar_type & RUSTEX_STRLEN_MASK) >> 0x4;
	return 0;
}

void rext_var_free(struct rext_var * mvar)
{
	unsigned int mtype;
	if (mvar == NULL)
		return;

	mtype = rext_var_type(mvar);
	if (mtype == MVALUE_TYPE_STRING) {
		free(mvar->mvar_val.mval_strp);
		mvar->mvar_val.mval_strp = NULL;
	}

	rext_var_init(mvar);
}

void rext_var_init(struct rext_var * mvar)
{
	if (mvar != NULL) {
		mvar->mvar_type = MVALUE_TYPE_NIL;
		mvar->mvar_val.mval_uint = 0;
	}
}

int rext_var_strbuf(struct rext_var * mvar,
	const char * pbuf, unsigned int plen)
{
	char * buf;

	buf = NULL;
	if (mvar == NULL)
		return -1;
	if (pbuf == NULL && plen > 0)
		return -2;
	if (plen >= RUSTEX_STRLEN_MAX) {
		fprintf(stderr, "Error, string buffer size to long: %u\n", plen);
		fflush(stderr);
		return -3;
	}

	rext_var_free(mvar);
	buf = (char *) malloc((size_t) (plen + 1));
	if (buf == NULL) {
		fputs("Error, system out of memory!\n", stderr);
		fflush(stderr);
		return -4;
	}

	if (plen > 0)
		memcpy(buf, pbuf, plen);
	buf[plen] = '\0';
	mvar->mvar_type = MVALUE_TYPE_STRING | (plen << 0x4);
	mvar->mvar_val.mval_strp = buf;
	return 0;
}

char * rext_var_str(const struct rext_var * mvar, unsigned int * plen)
{
	char * retval;
	unsigned int slen;

	retval = NULL;
	if (mvar == NULL || rext_var_type(mvar) != MVALUE_TYPE_STRING) {
		if (plen != NULL)
			*plen = 0;
		return retval;
	}

	slen = rext_var_strlen(mvar);
	if (plen != NULL)
		*plen = slen;
	retval = (char *) malloc((size_t) (slen + 0x1));
	if (retval == NULL) {
		if (plen != NULL)
			*plen = 0;
		fprintf(stderr, "Error, system out of memory: %#x\n", slen);
		fflush(stderr);
	} else if (slen > 0) {
		memcpy(retval, mvar->mvar_val.mval_strp, (size_t) slen);
		retval[slen] = '\0';
	} else {
		retval[0] = '\0';
	}
	return retval;
}

const char * rext_bor_str(const struct rext_var * mvar, unsigned int * plen)
{
	unsigned int slen;
	const char * retval;

	slen = 0;
	retval = NULL;
	if (mvar != NULL && rext_var_type(mvar) == MVALUE_TYPE_STRING) {
		slen = rext_var_strlen(mvar);
		retval = mvar->mvar_val.mval_strp;
	}

	if (plen != NULL)
		*plen = retval ? slen : 0;
	return retval;
}

double rext_strtof(const struct rext_var * mvar, int * errp)
{
	int error;
	double rval;
	char * strp, * nextp;

	nextp = NULL;
	strp = rext_var_str(mvar, NULL);
	if (strp == NULL || strp[0] == '\0') {
		if (strp != NULL)
			free(strp);
		*errp = EINVAL;
		return 0;
	}

	errno = 0;
	rval = strtod(strp, &nextp);
	error = errno;
	if (error != 0 || strp == nextp) {
		free(strp);
		*errp = (error > 0) ? error : EINVAL;
		return 0;
	}

	*errp = 0;
	free(strp);
	return rval;
}

long long rext_strtol(const struct rext_var * mvar, int * errp, int base)
{
	int error;
	long long rval;
	char * strp, * nextp;

	nextp = NULL;
	strp = rext_var_str(mvar, NULL);
	if (strp == NULL || strp[0] == '\0') {
		if (strp != NULL)
			free(strp);
		*errp = EINVAL;
		return 0;
	}

	errno = 0;
	rval = (long long) strtoll(strp, &nextp, base);
	error = errno;
	if (error != 0 || strp == nextp) {
		free(strp);
		*errp = (error > 0) ? error : EINVAL;
		return 0;
	}

	*errp = 0;
	free(strp);
	return rval;
}

unsigned long long rext_strtoul(const struct rext_var * mvar, int * errp, int base)
{
	int error;
	char * strp, * nextp;
	unsigned long long rval;

	nextp = NULL;
	strp = rext_var_str(mvar, NULL);
	if (strp == NULL || strp[0] == '\0') {
		if (strp != NULL)
			free(strp);
		*errp = EINVAL;
		return 0;
	}

	errno = 0;
	rval = (unsigned long long) strtoull(strp, &nextp, base);
	error = errno;
	if (error != 0 || strp == nextp) {
		free(strp);
		*errp = (error > 0) ? error : EINVAL;
		return 0;
	}

	*errp = 0;
	free(strp);
	return rval;
}

rext_any_t rext_lua_new(const struct rext_var * lvar, int * errp)
{
	lua_State * L;
	char * script;

	L = NULL;
	script = NULL;
	if (lvar != NULL)
		script = rext_var_str(lvar, NULL);

	*errp = 0;
	L = luaL_newstate();
	if (L == NULL) {
		if (script != NULL)
			free(script);
		*errp = ENOMEM;
		return L;
	}

	luaL_openlibs(L); /* load standard library */

	if (script != NULL && script[0] != '\0') {
		int ret;
		ret = luaL_dostring(L, script);
		if (ret != 0) {
			*errp = EINVAL;
			lua_close(L);
			L = NULL;
		}
	}

	if (script != NULL)
		free(script);
	return L;
}

int rext_gettop(rext_any_t anyt)
{
	lua_State * L;

	L = (lua_State *) anyt;
	if (L == NULL)
		return -1;

	return lua_gettop(L);
}

int rext_settop(rext_any_t anyt, int ntop)
{
	int ret;
	lua_State * L;

	L = (lua_State *) anyt;
	if (L == NULL)
		return -1;

	ret = lua_gettop(L);
	if (ntop >= 0)
		lua_settop(L, ntop);
	return ret;
}

int rext_pushval(rext_any_t anyt, const struct rext_var * lvar)
{
	lua_State * L;

	L = (lua_State *) anyt;
	if (L == NULL)
		return -1;

	if (lvar == NULL)
		return -2;

	switch (rext_var_type(lvar)) {
	case MVALUE_TYPE_NIL:
		lua_pushnil(L);
		break;

	case MVALUE_TYPE_BOOL:
		lua_pushboolean(L, lvar->mvar_val.mval_bool != 0);
		break;

	case MVALUE_TYPE_INT64:
		if (lvar->mvar_val.mval_int > 2147483647 && lvar->mvar_val.mval_int < -2147483648)
			lua_pushnumber(L, (lua_Number) lvar->mvar_val.mval_int);
		else
			lua_pushinteger(L, (lua_Integer) lvar->mvar_val.mval_int);
		break;

	case MVALUE_TYPE_UINT64:
		if (lvar->mvar_val.mval_uint > 2147483647)
			lua_pushnumber(L, (lua_Number) lvar->mvar_val.mval_uint);
		else
			lua_pushinteger(L, (lua_Integer) lvar->mvar_val.mval_uint);
		break;

	case MVALUE_TYPE_F32:
		lua_pushnumber(L, (lua_Number) lvar->mvar_val.mval_f32);
		break;

	case MVALUE_TYPE_F64:
		lua_pushnumber(L, (lua_Number) lvar->mvar_val.mval_f64);
		break;

	case MVALUE_TYPE_STRING: {
		unsigned int slen = 0;
		const char * pstr = rext_bor_str(lvar, &slen);
		if (pstr == NULL)
			return -3;
		lua_pushlstring(L, pstr, (size_t) slen);
		break;
	}

	default:
		return -4;
	}

	return 0;
}

int rext_upval_set(rext_any_t anyt, const struct rext_var * rfunc,
	int upindex, const struct rext_var * rval)
{
	lua_State * L;
	const char * valp;
	int oldtop, ntop, ret;

	L = (lua_State *) anyt;
	if (L == NULL)
		return -1;

	if (upindex < 0)
		return -2;

	if (rval == NULL)
		return -3;

	ntop = oldtop = lua_gettop(L);
	if (rfunc != NULL) {
		char * pfunc;
		pfunc = rext_var_str(rfunc, NULL);
		if (pfunc == NULL || pfunc[0] == '\0') {
			if (pfunc != NULL)
				free(pfunc);
			return -4;
		}

		ntop++;
		lua_getglobal(L, pfunc);
		ret = lua_gettop(L);
		if (ret != ntop || lua_type(L, ntop) != LUA_TFUNCTION) {
			if (oldtop != ret)
				lua_settop(L, oldtop);
			free(pfunc);
			return -5;
		}

		free(pfunc);
	} else if (lua_type(L, oldtop) != LUA_TFUNCTION) {
		return -6;
	}

	ret = rext_pushval(anyt, rval);
	if (ret < 0) {
		lua_settop(L, oldtop);
		return -7;
	}

	valp = lua_setupvalue(L, ntop, upindex);
	if (valp == NULL) {
		lua_settop(L, oldtop);
		return -8;
	}

	ret = lua_gettop(L);
	if (ret != oldtop)
		lua_settop(L, oldtop);
	return 0;
}

int rext_upval_get(rext_any_t anyt, const struct rext_var * rfunc,
	int upindex, struct rext_var * valname)
{
	int ntop, ret;
	lua_State * L;
	unsigned int flen;
	const char * fname;
	const char * upname;

	flen = 0;
	upname = NULL;
	L = (lua_State *) anyt;
	if (L == NULL)
		return -1;

	ntop = lua_gettop(L);
	fname = rext_bor_str(rfunc, &flen);
	if (fname == NULL || flen == 0)
		return -2;

	lua_getglobal(L, fname);
	ret = lua_gettop(L);
	if (ret != (ntop + 1) || lua_type(L, -1) != LUA_TFUNCTION) {
		if (ret != ntop)
			lua_settop(L, ntop);
		return -3;
	}

	upname = lua_getupvalue(L, ret, upindex);
	if (upname != NULL) {
		ret = rext_var_strbuf(valname, upname, strlen(upname));
		if (ret < 0)
			ret = -4;
	} else {
		ret = -5;
		rext_var_free(valname);
	}

	lua_settop(L, ntop);
	return ret;
}

int rext_call(rext_any_t anyt, const struct rext_var * rfunc,
	const struct rext_var * args, struct rext_var * resp)
{
	char * pfunc;
	int ntop, ret;
	lua_State * L;

	pfunc = NULL;
	L = (lua_State *) anyt;
	if (L == NULL)
		return -1;

	if (rfunc == NULL)
		return -2;

	ntop = lua_gettop(L);
	pfunc = rext_var_str(rfunc, NULL);
	if (pfunc == NULL || pfunc[0] == '\0') {
		if (pfunc != NULL)
			free(pfunc);
		return -3;
	}

	lua_getglobal(L, pfunc);
	ret = lua_gettop(L);
	if (ret != (ntop + 1) || lua_type(L, ret) != LUA_TFUNCTION) {
		free(pfunc);
		lua_settop(L, ntop);
		return -4;
	}

	/* remove dynamically allocated function name */
	free(pfunc);
	pfunc = NULL;

	if (args != NULL) {
		ret = rext_pushval(anyt, args);
		if (ret < 0) {
			lua_settop(L, ntop);
			return -5;
		}
	}

	/* only get one return value from called function */
	ret = lua_pcall(L, (args != NULL) ? 1 : 0, 1, 0);
	if (ret != 0) {
		if (lua_gettop(L) != ntop)
			lua_settop(L, ntop);
		return -6;
	}

	if (lua_type(L, -1) == LUA_TTABLE) {
		/* functions might return a table */
		ntop++;
	} else if (resp != NULL && rext_getval(anyt, -1, resp) < 0) {
		if (lua_gettop(L) != ntop)
			lua_settop(L, ntop);
		return -7;
	}

	if (lua_gettop(L) != ntop)
		lua_settop(L, ntop);
	return 0;
}

int rext_istable(rext_any_t anyt, int stkidx)
{
	int ntop;
	lua_State * L;

	L = (lua_State *) anyt;
	if (L == NULL)
		return 0;

	ntop = lua_gettop(L);
	if (stkidx > ntop)
		return 0;

	return lua_type(L, stkidx) == LUA_TTABLE;
}

int rext_isfunc(rext_any_t anyt, int stkidx)
{
	int ntop;
	lua_State * L;

	L = (lua_State *) anyt;
	if (L == NULL)
		return 0;

	ntop = lua_gettop(L);
	if (stkidx > ntop)
		return 0;

	return lua_type(L, stkidx) == LUA_TFUNCTION;
}

int rext_getval(rext_any_t anyt, int stkidx, struct rext_var * resp)
{
	int mtype;
	lua_State * L;

	L = (lua_State *) anyt;
	if (L == NULL)
		return -1;

	if (resp == NULL)
		return -2;

	rext_var_free(resp);
	mtype = lua_type(L, stkidx);
	switch (mtype) {
	case LUA_TNIL:
		resp->mvar_type = MVALUE_TYPE_NIL;
		resp->mvar_val.mval_uint = 0;
		break;

	case LUA_TBOOLEAN:
		resp->mvar_type = MVALUE_TYPE_BOOL;
		resp->mvar_val.mval_bool = lua_toboolean(L, stkidx);
		break;

	case LUA_TNUMBER: {
		lua_Number lua_n = lua_tonumber(L, stkidx);
		lua_Integer lua_i = lua_tointeger(L, stkidx);
		const long long ll_min = LLONG_MIN;
		const unsigned long long ll_max = ULLONG_MAX;

		if (lua_n == (lua_Number) lua_i) {
			if (lua_i <= 0x7FFFFFFF) {
				resp->mvar_type = MVALUE_TYPE_INT64;
				resp->mvar_val.mval_int = (long long) lua_i;
			} else {
				resp->mvar_type = MVALUE_TYPE_UINT64;
				resp->mvar_val.mval_uint = (unsigned long long) lua_i;
			}
		} else if (lua_n != floor(lua_n) || isnan(lua_n) || isinf(lua_n)) {
			resp->mvar_type = MVALUE_TYPE_F64;
			resp->mvar_val.mval_f64 = (double) lua_n;
		} else if (lua_n < (lua_Number) ll_min || lua_n > (lua_Number) ll_max) {
			resp->mvar_type = MVALUE_TYPE_F64;
			resp->mvar_val.mval_f64 = (double) lua_n;
		} else if (lua_n <= 0) {
			resp->mvar_type = MVALUE_TYPE_INT64;
			resp->mvar_val.mval_int = (long long) lua_n;
		} else {
			resp->mvar_type = MVALUE_TYPE_UINT64;
			resp->mvar_val.mval_uint = (unsigned long long) lua_n;
		}
		break;
	}

	case LUA_TSTRING: {
		size_t slen = 0;
		const char * strp = lua_tolstring(L, stkidx, &slen);
		if (strp == NULL || slen > RUSTEX_STRLEN_MAX) {
			fprintf(stderr, "Error, invalid string or too long: %lu\n",
				(unsigned long) slen);
			fflush(stderr);
			return -3;
		}

		resp->mvar_val.mval_strp = (char *) malloc(slen + 0x1);
		if (resp->mvar_val.mval_strp == NULL) {
			fprintf(stderr, "Error, system out of memory: %lu\n",
				(unsigned long) (slen + 1));
			fflush(stderr);
			return -4;
		}

		if (slen > 0)
			memcpy(resp->mvar_val.mval_strp, strp, slen);
		resp->mvar_val.mval_strp[slen] = '\0';
		resp->mvar_type = MVALUE_TYPE_STRING | ((unsigned int) (slen << 0x4));
		break;
	}

	default:
		/* ignored for Lua table and other types */
		break;
	}
	return 0;
}

int rext_tpushval(rext_any_t anyt, int maxnum,
	const struct rext_var * tabkey, const struct rext_var * tabval)
{
	int ntop, ret;
	lua_State * L;

	L = (lua_State *) anyt;
	if (L == NULL || tabkey == NULL || tabval == NULL)
		return -1;

	ntop = lua_gettop(L);
	if (ntop <= 0 || lua_type(L, ntop) != LUA_TTABLE) {
		if (maxnum < 0)
			maxnum = 8;
		lua_createtable(L, maxnum, maxnum);
		ret = lua_gettop(L);
		if (ret != (ntop + 1)) {
			fprintf(stderr, "Error, system out of memory: %d\n", maxnum);
			fflush(stderr);
			if (ret != ntop)
				lua_settop(L, ntop);
			return -2;
		}
		ntop++;
	}

	ret = rext_pushval(anyt, tabkey);
	if (ret < 0) {
		if (ntop != lua_gettop(L))
			lua_settop(L, ntop);
		return -3;
	}

	ret = rext_pushval(anyt, tabval);
	if (ret < 0) {
		if (ntop != lua_gettop(L))
			lua_settop(L, ntop);
		return -4;
	}

	lua_rawset(L, ntop);
	if (ntop != lua_gettop(L))
		lua_settop(L, ntop);
	return 0;
}

int rext_tgetval(rext_any_t anyt, int tabidx,
	const struct rext_var * tabkey, struct rext_var * tabval)
{
	int ntop, ret;
	lua_State * L;

	L = (lua_State *) anyt;
	if (L == NULL || tabidx <= 0 || tabkey == NULL || tabval == NULL)
		return -1;

	ntop = lua_gettop(L);
	if (ntop <= 0 || lua_type(L, tabidx) != LUA_TTABLE)
		return -2;

	ret = rext_pushval(anyt, tabkey);
	if (ret < 0 || lua_type(L, -1) == LUA_TNIL) {
		if (ntop != lua_gettop(L))
			lua_settop(L, ntop);
		return -3;
	}

	lua_rawget(L, tabidx);
	ret = lua_gettop(L);
	rext_var_free(tabval);
	if (ret == (ntop + 1) && rext_getval(anyt, ret, tabval) < 0) {
		if (ntop != lua_gettop(L))
			lua_settop(L, ntop);
		return -4;
	}

	lua_settop(L, ntop);
	return 0;
}

int rext_getglobal(rext_any_t anyt, const struct rext_var * gvar)
{
	int ntop;
	char * gptr;
	lua_State * L;

	L = (lua_State *) anyt;
	if (L == NULL || gvar == NULL)
		return -1;

	gptr = rext_var_str(gvar, NULL);
	if (gptr == NULL || gptr[0] == '\0') {
		if (gptr != NULL)
			free(gptr);
		return -2;
	}

	ntop = lua_gettop(L);
	lua_getglobal(L, gptr);
	if (lua_gettop(L) != (ntop + 1)) {
		free(gptr);
		return -3;
	}

	free(gptr);
	return 0;
}
