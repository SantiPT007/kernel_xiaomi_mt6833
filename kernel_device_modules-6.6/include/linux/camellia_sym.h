/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _CAMELLIA_SYM_H
#define _CAMELLIA_SYM_H

#include <linux/module.h>

#define CAMELLIA_SYM_CALL(sym, ...) ({				\
	typeof(&sym) __f = symbol_get(sym);			\
	if (__f) {						\
		__f(__VA_ARGS__);				\
		symbol_put(sym);				\
	}							\
	__f != NULL;						\
})

#define CAMELLIA_SYM_CALL_RET(sym, def, ...) ({			\
	typeof(&sym) __f = symbol_get(sym);			\
	typeof(sym(__VA_ARGS__)) __r = (def);			\
	if (__f) {						\
		__r = __f(__VA_ARGS__);				\
		symbol_put(sym);				\
	}							\
	__r;							\
})

#define CAMELLIA_SYM_READ(sym) ({				\
	typeof(&sym) __p = symbol_get(sym);			\
	typeof(sym) __v = 0;					\
	if (__p) {						\
		__v = *__p;					\
		symbol_put(sym);				\
	}							\
	__v;							\
})

#endif
