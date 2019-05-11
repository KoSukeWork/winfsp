/**
 * @file sys/fuse/fuse.h
 *
 * @copyright 2015-2019 Bill Zissimopoulos
 */
/*
 * This file is part of WinFsp.
 *
 * You can redistribute it and/or modify it under the terms of the GNU
 * General Public License version 3 as published by the Free Software
 * Foundation.
 *
 * Licensees holding a valid commercial license may use this software
 * in accordance with the commercial license agreement provided in
 * conjunction with the software.  The terms and conditions of any such
 * commercial license agreement shall govern, supersede, and render
 * ineffective any application of the GPLv3 license to this software,
 * notwithstanding of any reference thereto in the software or
 * associated repository.
 */

#ifndef WINFSP_SYS_FUSE_FUSE_H_INCLUDED
#define WINFSP_SYS_FUSE_FUSE_H_INCLUDED

#include <sys/fuse/proto.h>

/**
 * Simple coroutines
 *
 * All macros take a "state" parameter that must be a struct pointer that has a "lineno" integer
 * field. This field must be initialized to 0 on initial entry to a coroutine block.
 *     coroblock(state)
 *         This macro introduces a coroutine statement block { ... } where the "yield" statement
 *         can be used. There can only be one such block within a function.
 *     coroyield(state)
 *         This macro exits the current coroutine statement block. The coroutine block can be
 *         reentered in which case execution will continue after the "yield" statement. It is
 *         an error to use "yield" outside of a coroutine block.
 *     corofini(state)
 *         This macro exits the current coroutine statement block and marks it as "finished".
 *         If the coroutine block is reentered it exits immediately. It is an error to use "fini"
 *         outside of a coroutine block.
 */
#define coroyield__(state, num)         \
    do { (state)->lineno = num; goto coroblock__; case num:; } while (0)
#define coroblock(state)                if (0) coroblock__:; else switch ((state)->lineno) case 0:
#if defined(__COUNTER__)
#define coroyield(state)                coroyield__(state, __COUNTER__ + 1)
#else
#define coroyield(state)                coroyield__(state, __LINE__)
#endif
#define corofini(state)                 do { (state)->lineno = -1; goto coroblock__; } while (0)

#endif
