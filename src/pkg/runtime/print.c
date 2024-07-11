// Copyright 2009 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "runtime.h"
#include "type.h"

//static Lock debuglock;

static void vprintf(int8*, byte*);

// write to goroutine-local buffer if diverting output,
// or else standard error.
static void
gwrite(void *v, int32 n)
{
	if(g == nil || g->writebuf == nil) {
		runtime_write(2, v, n);
		return;
	}

	if(g->writenbuf == 0)
		return;

	if(n > g->writenbuf)
		n = g->writenbuf;
	runtime_memmove(g->writebuf, v, n);
	g->writebuf += n;
	g->writenbuf -= n;
}

void
runtime_dump(byte *p, int32 n)
{
	int32 i;

	for(i=0; i<n; i++) {
		runtime_printpointer((byte*)(p[i]>>4));
		runtime_printpointer((byte*)(p[i]&0xf));
		if((i&15) == 15)
			runtime_prints("\n");
		else
			runtime_prints(" ");
	}
	if(n & 15)
		runtime_prints("\n");
}

void
runtime_prints(int8 *s)
{
	gwrite(s, runtime_findnull((byte*)s));
}

#pragma textflag 7
void
runtime_printf(int8 *s, ...)
{
	byte *arg;

	arg = (byte*)(&s+1);
	vprintf(s, arg);
}

// Very simple printf.  Only for debugging prints.
// Do not add to this without checking with Rob.
static void
vprintf(int8 *s, byte *base)
{
	int8 *p, *lp;
	uintptr arg, narg;
	byte *v;

	//runtime_lock(&debuglock);

	lp = p = s;
	arg = 0;
	for(; *p; p++) {
		if(*p != '%')
			continue;
		if(p > lp)
			gwrite(lp, p-lp);
		p++;
		narg = 0;
		switch(*p) {
		case 't':
			narg = arg + 1;
			break;
		case 'd':	// 32-bit
		case 'x':
			arg = ROUND(arg, 4);
			narg = arg + 4;
			break;
		case 'D':	// 64-bit
		case 'U':
		case 'X':
		case 'f':
			arg = ROUND(arg, sizeof(uintptr));
			narg = arg + 8;
			break;
		case 'C':
			arg = ROUND(arg, sizeof(uintptr));
			narg = arg + 16;
			break;
		case 'p':	// pointer-sized
		case 's':
			arg = ROUND(arg, sizeof(uintptr));
			narg = arg + sizeof(uintptr);
			break;
		case 'S':	// pointer-aligned but bigger
			arg = ROUND(arg, sizeof(uintptr));
			narg = arg + sizeof(String);
			break;
		case 'a':	// pointer-aligned but bigger
			arg = ROUND(arg, sizeof(uintptr));
			narg = arg + sizeof(Slice);
			break;
		case 'i':	// pointer-aligned but bigger
		case 'e':
			arg = ROUND(arg, sizeof(uintptr));
			narg = arg + sizeof(Eface);
			break;
		}
		v = base+arg;
		switch(*p) {
		case 'a':
			runtime_printslice(*(Slice*)v);
			break;
		case 'd':
			runtime_printint(*(int32*)v);
			break;
		case 'D':
			runtime_printint(*(int64*)v);
			break;
		case 'e':
			runtime_printeface(*(Eface*)v);
			break;
		case 'f':
			runtime_printfloat(*(float64*)v);
			break;
		case 'C':
			runtime_printcomplex(*(Complex128*)v);
			break;
		case 'i':
			runtime_printiface(*(Iface*)v);
			break;
		case 'p':
			runtime_printpointer(*(void**)v);
			break;
		case 's':
			runtime_prints(*(int8**)v);
			break;
		case 'S':
			runtime_printstring(*(String*)v);
			break;
		case 't':
			runtime_printbool(*(bool*)v);
			break;
		case 'U':
			runtime_printuint(*(uint64*)v);
			break;
		case 'x':
			runtime_printhex(*(uint32*)v);
			break;
		case 'X':
			runtime_printhex(*(uint64*)v);
			break;
		}
		arg = narg;
		lp = p+1;
	}
	if(p > lp)
		gwrite(lp, p-lp);

	//runtime_unlock(&debuglock);
}

#pragma textflag 7
void
runtime_goprintf(String s, ...)
{
	// Can assume s has terminating NUL because only
	// the Go compiler generates calls to runtime_goprintf, using
	// string constants, and all the string constants have NULs.
	vprintf((int8*)s.str, (byte*)(&s+1));
}

void
runtime_printpc(void *p)
{
	runtime_prints("PC=");
	runtime_printhex((uint64)runtime_getcallerpc(p));
}

void
runtime_printbool(bool v)
{
	if(v) {
		gwrite((byte*)"true", 4);
		return;
	}
	gwrite((byte*)"false", 5);
}

void
runtime_printfloat(float64 v)
{
	byte buf[20];
	int32 e, s, i, n;
	float64 h;

	if(runtime_isNaN(v)) {
		gwrite("NaN", 3);
		return;
	}
	if(runtime_isInf(v, 1)) {
		gwrite("+Inf", 4);
		return;
	}
	if(runtime_isInf(v, -1)) {
		gwrite("-Inf", 4);
		return;
	}

	n = 7;	// digits printed
	e = 0;	// exp
	s = 0;	// sign
	if(v != 0) {
		// sign
		if(v < 0) {
			v = -v;
			s = 1;
		}

		// normalize
		while(v >= 10) {
			e++;
			v /= 10;
		}
		while(v < 1) {
			e--;
			v *= 10;
		}

		// round
		h = 5;
		for(i=0; i<n; i++)
			h /= 10;

		v += h;
		if(v >= 10) {
			e++;
			v /= 10;
		}
	}

	// format +d.dddd+edd
	buf[0] = '+';
	if(s)
		buf[0] = '-';
	for(i=0; i<n; i++) {
		s = v;
		buf[i+2] = s+'0';
		v -= s;
		v *= 10.;
	}
	buf[1] = buf[2];
	buf[2] = '.';

	buf[n+2] = 'e';
	buf[n+3] = '+';
	if(e < 0) {
		e = -e;
		buf[n+3] = '-';
	}

	buf[n+4] = (e/100) + '0';
	buf[n+5] = (e/10)%10 + '0';
	buf[n+6] = (e%10) + '0';
	gwrite(buf, n+7);
}

void
runtime_printcomplex(Complex128 v)
{
	gwrite("(", 1);
	runtime_printfloat(v.real);
	runtime_printfloat(v.imag);
	gwrite("i)", 2);
}

void
runtime_printuint(uint64 v)
{
	byte buf[100];
	int32 i;

	for(i=nelem(buf)-1; i>0; i--) {
		buf[i] = v%10 + '0';
		if(v < 10)
			break;
		v = v/10;
	}
	gwrite(buf+i, nelem(buf)-i);
}

void
runtime_printint(int64 v)
{
	if(v < 0) {
		gwrite("-", 1);
		v = -v;
	}
	runtime_printuint(v);
}

void
runtime_printhex(uint64 v)
{
	static int8 *dig = "0123456789abcdef";
	byte buf[100];
	int32 i;

	i=nelem(buf);
	for(; v>0; v/=16)
		buf[--i] = dig[v%16];
	if(i == nelem(buf))
		buf[--i] = '0';
	buf[--i] = 'x';
	buf[--i] = '0';
	gwrite(buf+i, nelem(buf)-i);
}

void
runtime_printpointer(void *p)
{
	runtime_printhex((uint64)p);
}

void
runtime_printstring(String v)
{
	extern uint32 runtime_maxstring;

	if(v.len > runtime_maxstring) {
		gwrite("[invalid string]", 16);
		return;
	}
	if(v.len > 0)
		gwrite(v.str, v.len);
}

void
runtime_printsp(void)
{
	gwrite(" ", 1);
}

void
runtime_printnl(void)
{
	gwrite("\n", 1);
}

void
runtime_typestring(Eface e, String s)
{
	s = *e.type->string;
	FLUSH(&s);
}

