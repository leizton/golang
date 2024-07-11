// Copyright 2009 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "runtime.h"
#include "type.h"

#define M0 (sizeof(uintptr)==4 ? 2860486313UL : 33054211828000289ULL)
#define M1 (sizeof(uintptr)==4 ? 3267000013UL : 23344194077549503ULL)

/*
 * map and chan helpers for
 * dealing with unknown types
 */
void
runtime_memhash(uintptr *h, uintptr s, void *a)
{
	byte *b;
	uintptr hash;

	b = a;
	hash = M0;
	while(s > 0) {
		hash = (hash ^ *b) * M1;
		b++;
		s--;
	}
	*h = (*h ^ hash) * M1;
}

void
runtime_memequal(bool *eq, uintptr s, void *a, void *b)
{
	byte *ba, *bb, *aend;

	if(a == b) {
		*eq = 1;
		return;
	}
	ba = a;
	bb = b;
	aend = ba+s;
	while(ba != aend) {
		if(*ba != *bb) {
			*eq = 0;
			return;
		}
		ba++;
		bb++;
	}
	*eq = 1;
	return;
}

void
runtime_memprint(uintptr s, void *a)
{
	uint64 v;

	v = 0xbadb00b;
	switch(s) {
	case 1:
		v = *(uint8*)a;
		break;
	case 2:
		v = *(uint16*)a;
		break;
	case 4:
		v = *(uint32*)a;
		break;
	case 8:
		v = *(uint64*)a;
		break;
	}
	runtime_printint(v);
}

void
runtime_memcopy(uintptr s, void *a, void *b)
{
	if(b == nil) {
		runtime_memclr(a, s);
		return;
	}
	runtime_memmove(a, b, s);
}

void
runtime_memequal0(bool *eq, uintptr s, void *a, void *b)
{
	USED(s);
	USED(a);
	USED(b);
	*eq = true;
}

void
runtime_memcopy0(uintptr s, void *a, void *b)
{
	USED(s);
	USED(a);
	USED(b);
}

void
runtime_memequal8(bool *eq, uintptr s, void *a, void *b)
{
	USED(s);
	*eq = *(uint8*)a == *(uint8*)b;
}

void
runtime_memcopy8(uintptr s, void *a, void *b)
{
	USED(s);
	if(b == nil) {
		*(uint8*)a = 0;
		return;
	}
	*(uint8*)a = *(uint8*)b;
}

void
runtime_memequal16(bool *eq, uintptr s, void *a, void *b)
{
	USED(s);
	*eq = *(uint16*)a == *(uint16*)b;
}

void
runtime_memcopy16(uintptr s, void *a, void *b)
{
	USED(s);
	if(b == nil) {
		*(uint16*)a = 0;
		return;
	}
	*(uint16*)a = *(uint16*)b;
}

void
runtime_memequal32(bool *eq, uintptr s, void *a, void *b)
{
	USED(s);
	*eq = *(uint32*)a == *(uint32*)b;
}

void
runtime_memcopy32(uintptr s, void *a, void *b)
{
	USED(s);
	if(b == nil) {
		*(uint32*)a = 0;
		return;
	}
	*(uint32*)a = *(uint32*)b;
}

void
runtime_memequal64(bool *eq, uintptr s, void *a, void *b)
{
	USED(s);
	*eq = *(uint64*)a == *(uint64*)b;
}

void
runtime_memcopy64(uintptr s, void *a, void *b)
{
	USED(s);
	if(b == nil) {
		*(uint64*)a = 0;
		return;
	}
	*(uint64*)a = *(uint64*)b;
}

void
runtime_memequal128(bool *eq, uintptr s, void *a, void *b)
{
	USED(s);
	*eq = ((uint64*)a)[0] == ((uint64*)b)[0] && ((uint64*)a)[1] == ((uint64*)b)[1];
}

void
runtime_memcopy128(uintptr s, void *a, void *b)
{
	USED(s);
	if(b == nil) {
		((uint64*)a)[0] = 0;
		((uint64*)a)[1] = 0;
		return;
	}
	((uint64*)a)[0] = ((uint64*)b)[0];
	((uint64*)a)[1] = ((uint64*)b)[1];
}

void
runtime_f32equal(bool *eq, uintptr s, void *a, void *b)
{
	USED(s);
	*eq = *(float32*)a == *(float32*)b;
}

void
runtime_f64equal(bool *eq, uintptr s, void *a, void *b)
{
	USED(s);
	*eq = *(float64*)a == *(float64*)b;
}

void
runtime_c64equal(bool *eq, uintptr s, void *a, void *b)
{	
	Complex64 *ca, *cb;
	
	USED(s);
	ca = a;
	cb = b;
	*eq = ca->real == cb->real && ca->imag == cb->imag;
}

void
runtime_c128equal(bool *eq, uintptr s, void *a, void *b)
{	
	Complex128 *ca, *cb;
	
	USED(s);
	ca = a;
	cb = b;
	*eq = ca->real == cb->real && ca->imag == cb->imag;
}

// NOTE: Because NaN != NaN, a map can contain any
// number of (mostly useless) entries keyed with NaNs.
// To avoid long hash chains, we assign a random number
// as the hash value for a NaN.

void
runtime_f32hash(uintptr *h, uintptr s, void *a)
{
	uintptr hash;
	float32 f;

	USED(s);
	f = *(float32*)a;
	if(f == 0)
		hash = 0;  // +0, -0
	else if(f != f)
		hash = runtime_fastrand1();  // any kind of NaN
	else
		hash = *(uint32*)a;
	*h = (*h ^ hash ^ M0) * M1;
}

void
runtime_f64hash(uintptr *h, uintptr s, void *a)
{
	uintptr hash;
	float64 f;
	uint64 u;

	USED(s);
	f = *(float64*)a;
	if(f == 0)
		hash = 0;	// +0, -0
	else if(f != f)
		hash = runtime_fastrand1();  // any kind of NaN
	else {
		u = *(uint64*)a;
		if(sizeof(uintptr) == 4)
			hash = ((uint32)(u>>32) * M1) ^ (uint32)u;
		else
			hash = u;
	}
	*h = (*h ^ hash ^ M0) * M1;
}

void
runtime_c64hash(uintptr *h, uintptr s, void *a)
{
	USED(s);
	runtime_f32hash(h, 0, a);
	runtime_f32hash(h, 0, (float32*)a+1);
}

void
runtime_c128hash(uintptr *h, uintptr s, void *a)
{
	USED(s);
	runtime_f64hash(h, 0, a);
	runtime_f64hash(h, 0, (float64*)a+1);
}

void
runtime_slicecopy(uintptr s, void *a, void *b)
{
	USED(s);
	if(b == nil) {
		((Slice*)a)->array = 0;
		((Slice*)a)->len = 0;
		((Slice*)a)->cap = 0;
		return;
	}
	((Slice*)a)->array = ((Slice*)b)->array;
	((Slice*)a)->len = ((Slice*)b)->len;
	((Slice*)a)->cap = ((Slice*)b)->cap;
}

void
runtime_strhash(uintptr *h, uintptr s, void *a)
{
	USED(s);
	runtime_memhash(h, ((String*)a)->len, ((String*)a)->str);
}

void
runtime_strequal(bool *eq, uintptr s, void *a, void *b)
{
	int32 alen;

	USED(s);
	alen = ((String*)a)->len;
	if(alen != ((String*)b)->len) {
		*eq = false;
		return;
	}
	runtime_memequal(eq, alen, ((String*)a)->str, ((String*)b)->str);
}

void
runtime_strprint(uintptr s, void *a)
{
	USED(s);
	runtime_printstring(*(String*)a);
}

void
runtime_strcopy(uintptr s, void *a, void *b)
{
	USED(s);
	if(b == nil) {
		((String*)a)->str = 0;
		((String*)a)->len = 0;
		return;
	}
	((String*)a)->str = ((String*)b)->str;
	((String*)a)->len = ((String*)b)->len;
}

void
runtime_interhash(uintptr *h, uintptr s, void *a)
{
	USED(s);
	*h = (*h ^ runtime_ifacehash(*(Iface*)a)) * M1;
}

void
runtime_interprint(uintptr s, void *a)
{
	USED(s);
	runtime_printiface(*(Iface*)a);
}

void
runtime_interequal(bool *eq, uintptr s, void *a, void *b)
{
	USED(s);
	*eq = runtime_ifaceeq_c(*(Iface*)a, *(Iface*)b);
}

void
runtime_intercopy(uintptr s, void *a, void *b)
{
	USED(s);
	if(b == nil) {
		((Iface*)a)->tab = 0;
		((Iface*)a)->data = 0;
		return;
	}
	((Iface*)a)->tab = ((Iface*)b)->tab;
	((Iface*)a)->data = ((Iface*)b)->data;
}

void
runtime_nilinterhash(uintptr *h, uintptr s, void *a)
{
	USED(s);
	*h = (*h ^ runtime_efacehash(*(Eface*)a)) * M1;
}

void
runtime_nilinterprint(uintptr s, void *a)
{
	USED(s);
	runtime_printeface(*(Eface*)a);
}

void
runtime_nilinterequal(bool *eq, uintptr s, void *a, void *b)
{
	USED(s);
	*eq = runtime_efaceeq_c(*(Eface*)a, *(Eface*)b);
}

void
runtime_nilintercopy(uintptr s, void *a, void *b)
{
	USED(s);
	if(b == nil) {
		((Eface*)a)->type = 0;
		((Eface*)a)->data = 0;
		return;
	}
	((Eface*)a)->type = ((Eface*)b)->type;
	((Eface*)a)->data = ((Eface*)b)->data;
}

void
runtime_nohash(uintptr *h, uintptr s, void *a)
{
	USED(s);
	USED(a);
	USED(h);
	runtime_panicstring("hash of unhashable type");
}

void
runtime_noequal(bool *eq, uintptr s, void *a, void *b)
{
	USED(s);
	USED(a);
	USED(b);
	USED(eq);
	runtime_panicstring("comparing uncomparable types");
}

Alg
runtime_algarray[] =
{
[AMEM]		{ runtime_memhash, runtime_memequal, runtime_memprint, runtime_memcopy },
[ANOEQ]		{ runtime_nohash, runtime_noequal, runtime_memprint, runtime_memcopy },
[ASTRING]	{ runtime_strhash, runtime_strequal, runtime_strprint, runtime_strcopy },
[AINTER]	{ runtime_interhash, runtime_interequal, runtime_interprint, runtime_intercopy },
[ANILINTER]	{ runtime_nilinterhash, runtime_nilinterequal, runtime_nilinterprint, runtime_nilintercopy },
[ASLICE]	{ runtime_nohash, runtime_noequal, runtime_memprint, runtime_slicecopy },
[AFLOAT32]	{ runtime_f32hash, runtime_f32equal, runtime_memprint, runtime_memcopy },
[AFLOAT64]	{ runtime_f64hash, runtime_f64equal, runtime_memprint, runtime_memcopy },
[ACPLX64]	{ runtime_c64hash, runtime_c64equal, runtime_memprint, runtime_memcopy },
[ACPLX128]	{ runtime_c128hash, runtime_c128equal, runtime_memprint, runtime_memcopy },
[AMEM0]		{ runtime_memhash, runtime_memequal0, runtime_memprint, runtime_memcopy0 },
[AMEM8]		{ runtime_memhash, runtime_memequal8, runtime_memprint, runtime_memcopy8 },
[AMEM16]	{ runtime_memhash, runtime_memequal16, runtime_memprint, runtime_memcopy16 },
[AMEM32]	{ runtime_memhash, runtime_memequal32, runtime_memprint, runtime_memcopy32 },
[AMEM64]	{ runtime_memhash, runtime_memequal64, runtime_memprint, runtime_memcopy64 },
[AMEM128]	{ runtime_memhash, runtime_memequal128, runtime_memprint, runtime_memcopy128 },
[ANOEQ0]	{ runtime_nohash, runtime_noequal, runtime_memprint, runtime_memcopy0 },
[ANOEQ8]	{ runtime_nohash, runtime_noequal, runtime_memprint, runtime_memcopy8 },
[ANOEQ16]	{ runtime_nohash, runtime_noequal, runtime_memprint, runtime_memcopy16 },
[ANOEQ32]	{ runtime_nohash, runtime_noequal, runtime_memprint, runtime_memcopy32 },
[ANOEQ64]	{ runtime_nohash, runtime_noequal, runtime_memprint, runtime_memcopy64 },
[ANOEQ128]	{ runtime_nohash, runtime_noequal, runtime_memprint, runtime_memcopy128 },
};

// Runtime helpers.

// func equal(t *Type, x T, y T) (ret bool)
#pragma textflag 7
void
runtime_equal(Type *t, ...)
{
	byte *x, *y;
	uintptr ret;
	
	x = (byte*)(&t+1);
	y = x + t->size;
	ret = (uintptr)(y + t->size);
	ret = ROUND(ret, Structrnd);
	t->alg->equal((bool*)ret, t->size, x, y);
}
