// Copyright 2009 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

extern void *runtime_LoadLibrary;
extern void *runtime_GetProcAddress;

// Call a Windows function with stdcall conventions,
// and switch to os stack during the call.
#pragma	varargck	countpos	runtime_stdcall	2
#pragma	varargck	type		runtime_stdcall	void*
#pragma	varargck	type		runtime_stdcall	uintptr
void runtime_asmstdcall(void *c);
void *runtime_stdcall(void *fn, int32 count, ...);

uint32 runtime_getlasterror(void);
void runtime_setlasterror(uint32 err);

// Function to be called by windows CreateThread
// to start new os thread.
uint32 runtime_tstart_stdcall(M *newm);

uint32 runtime_issigpanic(uint32);
void runtime_sigpanic(void);
uint32 runtime_ctrlhandler(uint32 type);

// Windows dll function to go callback entry.
byte *runtime_compilecallback(Eface fn, bool cleanstack);
void *runtime_callbackasm(void);

// TODO(brainman): should not need those
#define	NSIG	65
