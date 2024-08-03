// Copyright 2009 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "runtime.h"
#include "arch_GOARCH.h"
#include "type.h"
#include "malloc.h"

void
runtime_printiface(Iface i)
{
	runtime_printf("(%p,%p)", i.tab, i.data);
}

void
runtime_printeface(Eface e)
{
	runtime_printf("(%p,%p)", e.type, e.data);
}

/*
 * layout of Itab known to compilers
 */
struct Itab
{
	InterfaceType*	inter;
	Type*	type;
	Itab*	link;
	int32	bad;
	int32	unused;
	void	(*fun[])(void);
};

static	Itab*	hash[1009];
static	Lock	ifacelock;

static Itab*
itab(InterfaceType *inter, Type *type, int32 canfail)
{
	int32 locked;
	int32 ni;
	Method *t, *et;
	IMethod *i, *ei;
	uint32 h;
	String *iname, *ipkgPath;
	Itab *m;
	UncommonType *x;
	Type *itype;
	Eface err;

	if(inter->mhdr.len == 0)
		runtime_throw("internal error - misuse of itab");

	locked = 0;

	// easy case
	x = type->x;
	if(x == nil) {
		if(canfail)
			return nil;
		iname = inter->m[0].name;
		goto throw;
	}

	// compiler has provided some good hash codes for us.
	h = inter->hash;
	h += 17 * type->hash;
	// TODO(rsc): h += 23 * x->mhash ?
	h %= nelem(hash);

	// look twice - once without lock, once with.
	// common case will be no lock contention.
	for(locked=0; locked<2; locked++) {
		if(locked)
			runtime_lock(&ifacelock);
		for(m=runtime_atomicloadp(&hash[h]); m!=nil; m=m->link) {
			if(m->inter == inter && m->type == type) {
				if(m->bad) {
					m = nil;
					if(!canfail) {
						// this can only happen if the conversion
						// was already done once using the , ok form
						// and we have a cached negative result.
						// the cached result doesn't record which
						// interface function was missing, so jump
						// down to the interface check, which will
						// do more work but give a better error.
						goto search;
					}
				}
				if(locked)
					runtime_unlock(&ifacelock);
				return m;
			}
		}
	}

	ni = inter->mhdr.len;
	m = runtime_malloc(sizeof(*m) + ni*sizeof m->fun[0]);
	m->inter = inter;
	m->type = type;

search:
	// both inter and type have method sorted by name,
	// and interface names are unique,
	// so can iterate over both in lock step;
	// the loop is O(ni+nt) not O(ni*nt).
	i = inter->m;
	ei = i + inter->mhdr.len;
	t = x->m;
	et = t + x->mhdr.len;
	for(; i < ei; i++) {
		itype = i->type;
		iname = i->name;
		ipkgPath = i->pkgPath;
		for(;; t++) {
			if(t >= et) {
				if(!canfail) {
				throw:
					// didn't find method
					runtime_newTypeAssertionError(
						nil, type->string, inter->string,
						iname, &err);
					if(locked)
						runtime_unlock(&ifacelock);
					runtime_panic(err);
					return nil;	// not reached
				}
				m->bad = 1;
				goto out;
			}
			if(t->mtyp == itype && t->name == iname && t->pkgPath == ipkgPath)
				break;
		}
		if(m)
			m->fun[i - inter->m] = t->ifn;
	}

out:
	if(!locked)
		runtime_panicstring("invalid itab locking");
	m->link = hash[h];
	runtime_atomicstorep(&hash[h], m);
	runtime_unlock(&ifacelock);
	if(m->bad)
		return nil;
	return m;
}

static void
copyin(Type *t, void *src, void **dst)
{
	uintptr size;
	void *p;
	Alg *alg;

	size = t->size;
	alg = t->alg;

	if(size <= sizeof(*dst))
		alg->copy(size, dst, src);
	else {
		p = runtime_mal(size);
		alg->copy(size, p, src);
		*dst = p;
	}
}

static void
copyout(Type *t, void **src, void *dst)
{
	uintptr size;
	Alg *alg;

	size = t->size;
	alg = t->alg;

	if(size <= sizeof(*src))
		alg->copy(size, dst, src);
	else
		alg->copy(size, dst, *src);
}

// func convT2I(typ *byte, typ2 *byte, elem any) (ret any)
#pragma textflag 7
void
runtime_convT2I(Type *t, InterfaceType *inter, ...)
{
	byte *elem;
	Iface *ret;
	int32 wid;

	elem = (byte*)(&inter+1);
	wid = t->size;
	ret = (Iface*)(elem + ROUND(wid, Structrnd));
	ret->tab = itab(inter, t, 0);
	copyin(t, elem, &ret->data);
}

// func convT2E(typ *byte, elem any) (ret any)
#pragma textflag 7
void
runtime_convT2E(Type *t, ...)
{
	byte *elem;
	Eface *ret;
	int32 wid;

	elem = (byte*)(&t+1);
	wid = t->size;
	ret = (Eface*)(elem + ROUND(wid, Structrnd));
	ret->type = t;
	copyin(t, elem, &ret->data);
}

static void assertI2Tret(Type *t, Iface i, byte *ret);

// func ifaceI2T(typ *byte, iface any) (ret any)
#pragma textflag 7
void
runtime_assertI2T(Type *t, Iface i, ...)
{
	byte *ret;

	ret = (byte*)(&i+1);
	assertI2Tret(t, i, ret);
}

static void
assertI2Tret(Type *t, Iface i, byte *ret)
{
	Itab *tab;
	Eface err;

	tab = i.tab;
	if(tab == nil) {
		runtime_newTypeAssertionError(
			nil, nil, t->string,
			nil, &err);
		runtime_panic(err);
	}
	if(tab->type != t) {
		runtime_newTypeAssertionError(
			tab->inter->string, tab->type->string, t->string,
			nil, &err);
		runtime_panic(err);
	}
	copyout(t, &i.data, ret);
}

// func ifaceI2T2(typ *byte, iface any) (ret any, ok bool)
#pragma textflag 7
void
runtime_assertI2T2(Type *t, Iface i, ...)
{
	byte *ret;
	bool *ok;
	int32 wid;

	ret = (byte*)(&i+1);
	wid = t->size;
	ok = (bool*)(ret + wid);

	if(i.tab == nil || i.tab->type != t) {
		*ok = false;
		runtime_memclr(ret, wid);
		return;
	}

	*ok = true;
	copyout(t, &i.data, ret);
}

static void assertE2Tret(Type *t, Eface e, byte *ret);

// func ifaceE2T(typ *byte, iface any) (ret any)
#pragma textflag 7
void
runtime_assertE2T(Type *t, Eface e, ...)
{
	byte *ret;

	ret = (byte*)(&e+1);
	assertE2Tret(t, e, ret);
}

static void
assertE2Tret(Type *t, Eface e, byte *ret)
{
	Eface err;

	if(e.type == nil) {
		runtime_newTypeAssertionError(
			nil, nil, t->string,
			nil, &err);
		runtime_panic(err);
	}
	if(e.type != t) {
		runtime_newTypeAssertionError(
			nil, e.type->string, t->string,
			nil, &err);
		runtime_panic(err);
	}
	copyout(t, &e.data, ret);
}

// func ifaceE2T2(sigt *byte, iface any) (ret any, ok bool);
#pragma textflag 7
void
runtime_assertE2T2(Type *t, Eface e, ...)
{
	byte *ret;
	bool *ok;
	int32 wid;

	ret = (byte*)(&e+1);
	wid = t->size;
	ok = (bool*)(ret + wid);

	if(t != e.type) {
		*ok = false;
		runtime_memclr(ret, wid);
		return;
	}

	*ok = true;
	copyout(t, &e.data, ret);
}

// func convI2E(elem any) (ret any)
void
runtime_convI2E(Iface i, Eface ret)
{
	Itab *tab;

	ret.data = i.data;
	if((tab = i.tab) == nil)
		ret.type = nil;
	else
		ret.type = tab->type;
	FLUSH(&ret);
}

// func ifaceI2E(typ *byte, iface any) (ret any)
void
runtime_assertI2E(InterfaceType* inter, Iface i, Eface ret)
{
	Itab *tab;
	Eface err;

	tab = i.tab;
	if(tab == nil) {
		// explicit conversions require non-nil interface value.
		runtime_newTypeAssertionError(
			nil, nil, inter->string,
			nil, &err);
		runtime_panic(err);
	}
	ret.data = i.data;
	ret.type = tab->type;
	FLUSH(&ret);
}

// func ifaceI2E2(typ *byte, iface any) (ret any, ok bool)
void
runtime_assertI2E2(InterfaceType* inter, Iface i, Eface ret, bool ok)
{
	Itab *tab;

	USED(inter);
	tab = i.tab;
	if(tab == nil) {
		ret.type = nil;
		ok = 0;
	} else {
		ret.type = tab->type;
		ok = 1;
	}
	ret.data = i.data;
	FLUSH(&ret);
	FLUSH(&ok);
}

// func convI2I(typ *byte, elem any) (ret any)
void
runtime_convI2I(InterfaceType* inter, Iface i, Iface ret)
{
	Itab *tab;

	ret.data = i.data;
	if((tab = i.tab) == nil)
		ret.tab = nil;
	else if(tab->inter == inter)
		ret.tab = tab;
	else
		ret.tab = itab(inter, tab->type, 0);
	FLUSH(&ret);
}

void
runtime_ifaceI2I(InterfaceType *inter, Iface i, Iface *ret)
{
	Itab *tab;
	Eface err;

	tab = i.tab;
	if(tab == nil) {
		// explicit conversions require non-nil interface value.
		runtime_newTypeAssertionError(
			nil, nil, inter->string,
			nil, &err);
		runtime_panic(err);
	}
	ret->data = i.data;
	ret->tab = itab(inter, tab->type, 0);
}

// func ifaceI2I(sigi *byte, iface any) (ret any)
void
runtime_assertI2I(InterfaceType* inter, Iface i, Iface ret)
{
	runtime_ifaceI2I(inter, i, &ret);
}

// func ifaceI2I2(sigi *byte, iface any) (ret any, ok bool)
void
runtime_assertI2I2(InterfaceType *inter, Iface i, Iface ret, bool ok)
{
	Itab *tab;

	tab = i.tab;
	if(tab != nil && (tab->inter == inter || (tab = itab(inter, tab->type, 1)) != nil)) {
		ret.data = i.data;
		ret.tab = tab;
		ok = 1;
	} else {
		ret.data = 0;
		ret.tab = 0;
		ok = 0;
	}
	FLUSH(&ret);
	FLUSH(&ok);
}

void
runtime_ifaceE2I(InterfaceType *inter, Eface e, Iface *ret)
{
	Type *t;
	Eface err;

	t = e.type;
	if(t == nil) {
		// explicit conversions require non-nil interface value.
		runtime_newTypeAssertionError(
			nil, nil, inter->string,
			nil, &err);
		runtime_panic(err);
	}
	ret->data = e.data;
	ret->tab = itab(inter, t, 0);
}

// For reflect
//	func ifaceE2I(t *InterfaceType, e interface{}, dst *Iface)
void
reflect_ifaceE2I(InterfaceType *inter, Eface e, Iface *dst)
{
	runtime_ifaceE2I(inter, e, dst);
}

// func ifaceE2I(sigi *byte, iface any) (ret any)
void
runtime_assertE2I(InterfaceType* inter, Eface e, Iface ret)
{
	runtime_ifaceE2I(inter, e, &ret);
}

// ifaceE2I2(sigi *byte, iface any) (ret any, ok bool)
void
runtime_assertE2I2(InterfaceType *inter, Eface e, Iface ret, bool ok)
{
	if(e.type == nil) {
		ok = 0;
		ret.data = nil;
		ret.tab = nil;
	} else if((ret.tab = itab(inter, e.type, 1)) == nil) {
		ok = 0;
		ret.data = nil;
	} else {
		ok = 1;
		ret.data = e.data;
	}
	FLUSH(&ret);
	FLUSH(&ok);
}

// func ifaceE2E(typ *byte, iface any) (ret any)
void
runtime_assertE2E(InterfaceType* inter, Eface e, Eface ret)
{
	Type *t;
	Eface err;

	t = e.type;
	if(t == nil) {
		// explicit conversions require non-nil interface value.
		runtime_newTypeAssertionError(
			nil, nil, inter->string,
			nil, &err);
		runtime_panic(err);
	}
	ret = e;
	FLUSH(&ret);
}

// func ifaceE2E2(iface any) (ret any, ok bool)
void
runtime_assertE2E2(InterfaceType* inter, Eface e, Eface ret, bool ok)
{
	USED(inter);
	ret = e;
	ok = e.type != nil;
	FLUSH(&ret);
	FLUSH(&ok);
}

static uintptr
ifacehash1(void *data, Type *t)
{
	Alg *alg;
	uintptr size, h;
	Eface err;

	if(t == nil)
		return 0;

	alg = t->alg;
	size = t->size;
	if(alg->hash == runtime_nohash) {
		// calling nohash will panic too,
		// but we can print a better error.
		runtime_newErrorString(runtime_catstring(runtime_gostringnocopy((byte*)"hash of unhashable type "), *t->string), &err);
		runtime_panic(err);
	}
	h = 0;
	if(size <= sizeof(data))
		alg->hash(&h, size, &data);
	else
		alg->hash(&h, size, data);
	return h;
}

uintptr
runtime_ifacehash(Iface a)
{
	if(a.tab == nil)
		return 0;
	return ifacehash1(a.data, a.tab->type);
}

uintptr
runtime_efacehash(Eface a)
{
	return ifacehash1(a.data, a.type);
}

static bool
ifaceeq1(void *data1, void *data2, Type *t)
{
	uintptr size;
	Alg *alg;
	Eface err;
	bool eq;

	alg = t->alg;
	size = t->size;

	if(alg->equal == runtime_noequal) {
		// calling noequal will panic too,
		// but we can print a better error.
		runtime_newErrorString(runtime_catstring(runtime_gostringnocopy((byte*)"comparing uncomparable type "), *t->string), &err);
		runtime_panic(err);
	}

	eq = 0;
	if(size <= sizeof(data1))
		alg->equal(&eq, size, &data1, &data2);
	else
		alg->equal(&eq, size, data1, data2);
	return eq;
}

bool
runtime_ifaceeq_c(Iface i1, Iface i2)
{
	if(i1.tab != i2.tab)
		return false;
	if(i1.tab == nil)
		return true;
	return ifaceeq1(i1.data, i2.data, i1.tab->type);
}

bool
runtime_efaceeq_c(Eface e1, Eface e2)
{
	if(e1.type != e2.type)
		return false;
	if(e1.type == nil)
		return true;
	return ifaceeq1(e1.data, e2.data, e1.type);
}

// ifaceeq(i1 any, i2 any) (ret bool);
void
runtime_ifaceeq(Iface i1, Iface i2, bool ret)
{
	ret = runtime_ifaceeq_c(i1, i2);
	FLUSH(&ret);
}

// efaceeq(i1 any, i2 any) (ret bool)
void
runtime_efaceeq(Eface e1, Eface e2, bool ret)
{
	ret = runtime_efaceeq_c(e1, e2);
	FLUSH(&ret);
}

// ifacethash(i1 any) (ret uint32);
void
runtime_ifacethash(Iface i1, uint32 ret)
{
	Itab *tab;

	ret = 0;
	tab = i1.tab;
	if(tab != nil)
		ret = tab->type->hash;
	FLUSH(&ret);
}

// efacethash(e1 any) (ret uint32)
void
runtime_efacethash(Eface e1, uint32 ret)
{
	Type *t;

	ret = 0;
	t = e1.type;
	if(t != nil)
		ret = t->hash;
	FLUSH(&ret);
}

void
reflect_unsafe_Typeof(Eface e, Eface ret)
{
	if(e.type == nil) {
		ret.type = nil;
		ret.data = nil;
	} else {
		ret = *(Eface*)(e.type);
	}
	FLUSH(&ret);
}

void
reflect_unsafe_New(Eface typ, void *ret)
{
	Type *t;

	// Reflect library has reinterpreted typ
	// as its own kind of type structure.
	// We know that the pointer to the original
	// type structure sits before the data pointer.
	t = (Type*)((Eface*)typ.data-1);

	if(t->kind&KindNoPointers)
		ret = runtime_mallocgc(t->size, FlagNoPointers, 1, 1);
	else
		ret = runtime_mal(t->size);
	FLUSH(&ret);
}

void
reflect_unsafe_NewArray(Eface typ, uint32 n, void *ret)
{
	uint64 size;
	Type *t;

	// Reflect library has reinterpreted typ
	// as its own kind of type structure.
	// We know that the pointer to the original
	// type structure sits before the data pointer.
	t = (Type*)((Eface*)typ.data-1);

	size = n*t->size;
	if(t->kind&KindNoPointers)
		ret = runtime_mallocgc(size, FlagNoPointers, 1, 1);
	else
		ret = runtime_mal(size);
	FLUSH(&ret);
}