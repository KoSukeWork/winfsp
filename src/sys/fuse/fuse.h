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
    INT CoroState;
};
BOOLEAN FspFuseProcess(
    FSP_FUSE_CONTEXT **PContext, FSP_FSCTL_TRANSACT_REQ *InternalRequest,
    FSP_FUSE_PROTO_RSP *FuseResponse, FSP_FUSE_PROTO_REQ *FuseRequest);
#define FspFuseProcessFini              ((FSP_FSCTL_TRANSACT_REQ *)0)   /* finalize Context */
#define FspFuseProcessNorm              ((FSP_FSCTL_TRANSACT_REQ *)1)   /* normal processing */
#define FspFuseContextInvl              ((FSP_FUSE_CONTEXT *)1) /* STATUS_INVALID_DEVICE_REQUEST */

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
 * Simple coroutines
 *
 * The coroutine state is maintained in an integer variable whose address is passed to the "block"
 * statement. This variable must be initialized to 0 on initial entry to a coroutine block.
 *     coro_block(S)
 *         This macro introduces a coroutine statement block { ... } where the "yield" statement
 *         can be used. There can only be one such block within a function.
 *     coro_yield
 *         This macro exits the current coroutine statement block. The coroutine block can be
 *         reentered in which case execution will continue after the "yield" statement. It is
 *         an error to use "yield" outside of a coroutine block.
 *     coro_exit
 *         This macro exits the current coroutine statement block and marks it as "complete".
 *         If the coroutine block is reentered it exits immediately. It is an error to use "exit"
 *         outside of a coroutine block.
 */
#define coro_block(S)           int *coroS__ = (S); if (0,0) coroY__:; else switch (*coroS__) case 0:
#define coro_yield__(N)         do { *coroS__ = N; goto coroY__; case N:; } while (0,0)
#define coro_exit               do { *coroS__ = -1; goto coroY__; } while (0,0)
#if defined(__COUNTER__)
#define coro_yield              coro_yield__(__COUNTER__ + 1)
#else
#define coro_yield              coro_yield__(__LINE__)
#endif

#endif
