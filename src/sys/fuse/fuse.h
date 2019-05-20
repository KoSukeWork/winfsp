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
#if !defined(FSP_IOQ_PROCESS_NO_CANCEL)
#error FUSE functionality has not been tested without FSP_IOQ_PROCESS_NO_CANCEL
#endif

/* DriverEntry */
VOID FspFuseInitialize(VOID);

/* volume management */
NTSTATUS FspVolumeTransactFuse(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);

/* FUSE processing context */
typedef struct _FSP_FUSE_CONTEXT FSP_FUSE_CONTEXT;
typedef BOOLEAN FSP_FUSE_PROCESS_DISPATCH(
    FSP_FUSE_CONTEXT **PContext, FSP_FSCTL_TRANSACT_REQ *InternalRequest,
    FSP_FUSE_PROTO_RSP *FuseResponse, FSP_FUSE_PROTO_REQ *FuseRequest);
    /*
     * Param:
     *     *PContext == 0 && Request != 0: *PContext = new FSP_FUSE_CONTEXT(Request)
     *     *PContext != 0 && Request != 0: process
     *     *PContext != 0 && Request == 0: delete *PContext
     * Return:
     *     TRUE: continue processing
     *     FALSE: stop processing
     */
struct _FSP_FUSE_CONTEXT
{
    FSP_FUSE_CONTEXT *DictNext;
    LIST_ENTRY ListEntry;
    FSP_FSCTL_TRANSACT_REQ *InternalRequest;
    FSP_FSCTL_TRANSACT_RSP *InternalResponse;
    INT CoroState[4];
};
BOOLEAN FspFuseProcess(
    FSP_FUSE_CONTEXT **PContext, FSP_FSCTL_TRANSACT_REQ *InternalRequest,
    FSP_FUSE_PROTO_RSP *FuseResponse, FSP_FUSE_PROTO_REQ *FuseRequest);
#define FspFuseProcessFini              ((FSP_FSCTL_TRANSACT_REQ *)0)   /* finalize Context */
#define FspFuseProcessNorm              ((FSP_FSCTL_TRANSACT_REQ *)1)   /* normal processing */
#define FspFuseContextStatus(S)         \
    (                                   \
        ASSERT(0xC0000000 == ((UINT32)(S) & 0xFFFF0000)),\
        (FSP_FUSE_CONTEXT *)(UINT_PTR)((UINT32)(S) & 0x0000FFFF)\
    )
#define FspFuseContextIsStatus(C)       ((UINT_PTR)0x0000FFFF >= (UINT_PTR)(C))
#define FspFuseContextToStatus(C)       ((NTSTATUS)(0xC0000000 | (UINT32)(UINT_PTR)(C)))

/* FUSE processing functions */
FSP_FUSE_PROCESS_DISPATCH FspFuseOpCreate;
FSP_FUSE_PROCESS_DISPATCH FspFuseOpCleanup;
FSP_FUSE_PROCESS_DISPATCH FspFuseOpClose;

/* FUSE I/O queue */
typedef struct _FSP_FUSE_IOQ FSP_FUSE_IOQ;
struct _FSP_FUSE_IOQ
{
    KSPIN_LOCK SpinLock;
    LIST_ENTRY PendingList, ProcessList;
    ULONG ProcessBucketCount;
    FSP_FUSE_CONTEXT *ProcessBuckets[];
};
NTSTATUS FspFuseIoqCreate(FSP_FUSE_IOQ **PIoq);
VOID FspFuseIoqDelete(FSP_FUSE_IOQ *Ioq);
VOID FspFuseIoqStartProcessing(FSP_FUSE_IOQ *Ioq, FSP_FUSE_CONTEXT *Context);
FSP_FUSE_CONTEXT *FspFuseIoqEndProcessing(FSP_FUSE_IOQ *Ioq, UINT64 Unique);
VOID FspFuseIoqPostPending(FSP_FUSE_IOQ *Ioq, FSP_FUSE_CONTEXT *Context);
FSP_FUSE_CONTEXT *FspFuseIoqNextPending(FSP_FUSE_IOQ *Ioq); /* does not block! */

/*
 * Nested coroutines
 *
 * This is a simple implementation of nested coroutines for C using macros. It introduces
 * the macros coro_block, coro_await, coro_yield and coro_exit that are used to create coroutines,
 * suspend/resume them and exit them. This implementation supports nested coroutines in that
 * a coroutine may invoke another coroutine and the whole stack of coroutines may be suspended
 * and later resumed.
 *
 * The implementation is able to do this by maintaining a stack of program "points" where
 * coroutine execution may be suspended and later resumed. The implementation achieves this by
 * (ab)using a peculiarity of the C switch statement in that it allows case: labels to appear
 * anywhere within the body of a switch statement including nested compound statements.
 *
 * While this implementation is able to maintain the stack of program "points" where execution
 * may be resumed, it is not able to automatically maintain important state such as activation
 * records (local variables). It is the responsibility of the programmer using these macros
 * to maintain this state. Usually this is done by moving all local variables together with the
 * space for the program "points" stack into a heap allocation that is then passed as an argument
 * to the coroutines.
 *
 * The original coroutine implementation was done some time around 2010 and did not support
 * nesting. It was used to facilitate the implementation of complex iterators in a C++ library.
 * It was inspired by Simon Tatham's "Coroutines in C":
 *     http://www.chiark.greenend.org.uk/~sgtatham/coroutines.html
 *
 * Reference
 *
 * The coroutine state is maintained in an integer array (S) whose address is passed to the
 * "block" statement. The array's number of elements must be 2 plus the number of expected
 * coroutines (e.g. if you expect to have up to 3 nested coroutines, the array must have at
 * least 5 elements). The array must be initialized to 0 on initial entry to a coroutine block.
 *
 *     coro_block(S)
 *         This macro introduces a coroutine statement block { ... } where the "await", "yield",
 *         and "exit" statements can be used. There can only be one such block within a function.
 *     coro_await(E)
 *         This macro executes the expression E, which should be an invocation of a coroutine.
 *         The nested coroutine may suspend itself, in which case the "await" statement exits
 *         the current coroutine statement block. The coroutine block can be reentered in which
 *         case execution will continue within the coroutine invoked by E, unless the coroutine
 *         is complete. It is an error to use "await" outside of a coroutine block.
 *     coro_yield
 *         This macro exits the current coroutine statement block. The coroutine block can be
 *         reentered in which case execution will continue after the "yield" statement. It is
 *         an error to use "yield" outside of a coroutine block.
 *     coro_exit
 *         This macro exits the current coroutine statement block and marks it as "complete".
 *         If the coroutine block is reentered it exits immediately. It is an error to use "exit"
 *         outside of a coroutine block.
 */
#define coro_block(S)       int *coro_S__ = (S); if (0,0) coro_X__:; else switch (coro_enter__()) case 0:
#define coro_await__(N, E)  do { E; coro_leave__(N); goto coro_X__; case N:; } while (coro_ndone__())
#define coro_yield__(N)     do { coro_below__ = 0; coro_leave__(N); goto coro_X__; case N:; } while (0,0)
#define coro_exit           do { coro_below__ = 0; coro_leave__(-1); goto coro_X__; } while (0,0)
#define coro_below__        coro_S__[coro_S__[0] + 1]
#define coro_enter__()      (coro_S__[++coro_S__[0]])
#define coro_leave__(N)     (coro_S__[coro_S__[0]--] = N)
#define coro_ndone__()      ((-1 != coro_below__) || ((coro_below__ = 0), 0))
#if defined(__COUNTER__)
#define coro_await(...)     coro_await__((__COUNTER__ + 1), (__VA_ARGS__))
#define coro_yield          coro_yield__((__COUNTER__ + 1))
#else
#define coro_await(...)     coro_await__(__LINE__, (__VA_ARGS__))
#define coro_yield          coro_yield__(__LINE__)
#endif

#endif
