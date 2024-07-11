// Copyright 2009 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "runtime.h"
#include "arch_GOARCH.h"
#include "type.h"
#include "malloc.h"

static	int32	debug	= 0;

static	void	makeslice1(SliceType*, int32, int32, Slice*);
static	void	growslice1(SliceType*, Slice, int32, Slice *);
	void	runtime_copy(Slice to, Slice fm, uintptr width, int32 ret);

// see also unsafe_NewArray
// makeslice(typ *Type, len, cap int64) (ary []any);
void
runtime_makeslice(SliceType *t, int64 len, int64 cap, Slice ret)
{
	if(len < 0 || (int32)len != len)
		runtime_panicstring("makeslice: len out of range");
	if(cap < len || (int32)cap != cap || t->elem->size > 0 && cap > ((uintptr)-1) / t->elem->size)
		runtime_panicstring("makeslice: cap out of range");

	makeslice1(t, len, cap, &ret);

	if(debug) {
		runtime_printf("makeslice(%S, %D, %D); ret=",
			*t->string, len, cap);
		runtime_printslice(ret);
	}
}

// Dummy word to use as base pointer for make([]T, 0).
// Since you cannot take the address of such a slice,
// you can't tell that they all have the same base pointer.
static uintptr zerobase;

static void
makeslice1(SliceType *t, int32 len, int32 cap, Slice *ret)
{
	uintptr size;

	size = cap*t->elem->size;

	ret->len = len;
	ret->cap = cap;

	if(cap == 0)
		ret->array = (byte*)&zerobase;
	else if((t->elem->kind&KindNoPointers))
		ret->array = runtime_mallocgc(size, FlagNoPointers, 1, 1);
	else
		ret->array = runtime_mal(size);
}

// appendslice(type *Type, x, y, []T) []T
void
runtime_appendslice(SliceType *t, Slice x, Slice y, Slice ret)
{
	int32 m;
	uintptr w;

	m = x.len+y.len;

	if(m < x.len)
		runtime_throw("append: slice overflow");

	if(m > x.cap)
		growslice1(t, x, m, &ret);
	else
		ret = x;

	w = t->elem->size;
	runtime_memmove(ret.array + ret.len*w, y.array, y.len*w);
	ret.len += y.len;
	FLUSH(&ret);
}


// appendstr([]byte, string) []byte
void
runtime_appendstr(SliceType *t, Slice x, String y, Slice ret)
{
	int32 m;

	m = x.len+y.len;

	if(m < x.len)
		runtime_throw("append: slice overflow");

	if(m > x.cap)
		growslice1(t, x, m, &ret);
	else
		ret = x;

	runtime_memmove(ret.array + ret.len, y.str, y.len);
	ret.len += y.len;
	FLUSH(&ret);
}


// growslice(type *Type, x, []T, n int64) []T
void
runtime_growslice(SliceType *t, Slice old, int64 n, Slice ret)
{
	int64 cap;

	if(n < 1)
		runtime_panicstring("growslice: invalid n");

	cap = old.cap + n;

	if((int32)cap != cap || cap > ((uintptr)-1) / t->elem->size)
		runtime_panicstring("growslice: cap out of range");

	growslice1(t, old, cap, &ret);

	FLUSH(&ret);

	if(debug) {
		runtime_printf("growslice(%S,", *t->string);
		runtime_printslice(old);
		runtime_printf(", new cap=%D) =", cap);
		runtime_printslice(ret);
	}
}

static void
growslice1(SliceType *t, Slice x, int32 newcap, Slice *ret)
{
	int32 m;

	m = x.cap;
	if(m == 0)
		m = newcap;
	else {
		do {
			if(x.len < 1024)
				m += m;
			else
				m += m/4;
		} while(m < newcap);
	}
	makeslice1(t, x.len, m, ret);
	runtime_memmove(ret->array, x.array, ret->len * t->elem->size);
}

// sliceslice(old []any, lb uint64, hb uint64, width uint64) (ary []any);
void
runtime_sliceslice(Slice old, uint64 lb, uint64 hb, uint64 width, Slice ret)
{
	if(hb > old.cap || lb > hb) {
		if(debug) {
			runtime_prints("runtime.sliceslice: old=");
			runtime_printslice(old);
			runtime_prints("; lb=");
			runtime_printint(lb);
			runtime_prints("; hb=");
			runtime_printint(hb);
			runtime_prints("; width=");
			runtime_printint(width);
			runtime_prints("\n");

			runtime_prints("oldarray: nel=");
			runtime_printint(old.len);
			runtime_prints("; cap=");
			runtime_printint(old.cap);
			runtime_prints("\n");
		}
		runtime_panicslice();
	}

	// new array is inside old array
	ret.len = hb - lb;
	ret.cap = old.cap - lb;
	ret.array = old.array + lb*width;

	FLUSH(&ret);

	if(debug) {
		runtime_prints("runtime.sliceslice: old=");
		runtime_printslice(old);
		runtime_prints("; lb=");
		runtime_printint(lb);
		runtime_prints("; hb=");
		runtime_printint(hb);
		runtime_prints("; width=");
		runtime_printint(width);
		runtime_prints("; ret=");
		runtime_printslice(ret);
		runtime_prints("\n");
	}
}

// sliceslice1(old []any, lb uint64, width uint64) (ary []any);
void
runtime_sliceslice1(Slice old, uint64 lb, uint64 width, Slice ret)
{
	if(lb > old.len) {
		if(debug) {
			runtime_prints("runtime.sliceslice: old=");
			runtime_printslice(old);
			runtime_prints("; lb=");
			runtime_printint(lb);
			runtime_prints("; width=");
			runtime_printint(width);
			runtime_prints("\n");

			runtime_prints("oldarray: nel=");
			runtime_printint(old.len);
			runtime_prints("; cap=");
			runtime_printint(old.cap);
			runtime_prints("\n");
		}
		runtime_panicslice();
	}

	// new array is inside old array
	ret.len = old.len - lb;
	ret.cap = old.cap - lb;
	ret.array = old.array + lb*width;

	FLUSH(&ret);

	if(debug) {
		runtime_prints("runtime.sliceslice: old=");
		runtime_printslice(old);
		runtime_prints("; lb=");
		runtime_printint(lb);
		runtime_prints("; width=");
		runtime_printint(width);
		runtime_prints("; ret=");
		runtime_printslice(ret);
		runtime_prints("\n");
	}
}

// slicearray(old *any, nel uint64, lb uint64, hb uint64, width uint64) (ary []any);
void
runtime_slicearray(byte* old, uint64 nel, uint64 lb, uint64 hb, uint64 width, Slice ret)
{
	if(nel > 0 && old == nil) {
		// crash if old == nil.
		// could give a better message
		// but this is consistent with all the in-line checks
		// that the compiler inserts for other uses.
		*old = 0;
	}

	if(hb > nel || lb > hb) {
		if(debug) {
			runtime_prints("runtime.slicearray: old=");
			runtime_printpointer(old);
			runtime_prints("; nel=");
			runtime_printint(nel);
			runtime_prints("; lb=");
			runtime_printint(lb);
			runtime_prints("; hb=");
			runtime_printint(hb);
			runtime_prints("; width=");
			runtime_printint(width);
			runtime_prints("\n");
		}
		runtime_panicslice();
	}

	// new array is inside old array
	ret.len = hb-lb;
	ret.cap = nel-lb;
	ret.array = old + lb*width;

	FLUSH(&ret);

	if(debug) {
		runtime_prints("runtime.slicearray: old=");
		runtime_printpointer(old);
		runtime_prints("; nel=");
		runtime_printint(nel);
		runtime_prints("; lb=");
		runtime_printint(lb);
		runtime_prints("; hb=");
		runtime_printint(hb);
		runtime_prints("; width=");
		runtime_printint(width);
		runtime_prints("; ret=");
		runtime_printslice(ret);
		runtime_prints("\n");
	}
}

// copy(to any, fr any, wid uint32) int
void
runtime_copy(Slice to, Slice fm, uintptr width, int32 ret)
{
	if(fm.len == 0 || to.len == 0 || width == 0) {
		ret = 0;
		goto out;
	}

	ret = fm.len;
	if(to.len < ret)
		ret = to.len;

	if(ret == 1 && width == 1) {	// common case worth about 2x to do here
		*to.array = *fm.array;	// known to be a byte pointer
	} else {
		runtime_memmove(to.array, fm.array, ret*width);
	}

out:
	FLUSH(&ret);

	if(debug) {
		runtime_prints("main_copy: to=");
		runtime_printslice(to);
		runtime_prints("; fm=");
		runtime_printslice(fm);
		runtime_prints("; width=");
		runtime_printint(width);
		runtime_prints("; ret=");
		runtime_printint(ret);
		runtime_prints("\n");
	}
}

void
runtime_slicestringcopy(Slice to, String fm, int32 ret)
{
	if(fm.len == 0 || to.len == 0) {
		ret = 0;
		goto out;
	}

	ret = fm.len;
	if(to.len < ret)
		ret = to.len;

	runtime_memmove(to.array, fm.str, ret);

out:
	FLUSH(&ret);
}

void
runtime_printslice(Slice a)
{
	runtime_prints("[");
	runtime_printint(a.len);
	runtime_prints("/");
	runtime_printint(a.cap);
	runtime_prints("]");
	runtime_printpointer(a.array);
}
