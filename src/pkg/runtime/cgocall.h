// Copyright 2009 The Go Authors.  All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

/*
 * Cgo interface.
 */

void runtime_cgocall(void (*fn)(void*), void*);
void runtime_cgocallback(void (*fn)(void), void*, uintptr);
void *runtime_cmalloc(uintptr);
void runtime_cfree(void*);
