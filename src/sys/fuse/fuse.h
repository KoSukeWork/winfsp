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
#include <sys/fuse/coro.h>

 /* DriverEntry */
VOID FspFuseInitialize(VOID);

/* volume management */
NTSTATUS FspVolumeTransactFuse(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);

/* FUSE processing context */
typedef struct _FSP_FUSE_CONTEXT FSP_FUSE_CONTEXT;
typedef BOOLEAN FSP_FUSE_PROCESS_DISPATCH(FSP_FUSE_CONTEXT *Context);
struct _FSP_FUSE_CONTEXT
{
    FSP_FUSE_CONTEXT *DictNext;
    LIST_ENTRY ListEntry;
    FSP_FSCTL_TRANSACT_REQ *InternalRequest;
    FSP_FSCTL_TRANSACT_RSP *InternalResponse;
    FSP_FSCTL_DECLSPEC_ALIGN UINT8 InternalResponseBuf[sizeof(FSP_FSCTL_TRANSACT_RSP)];
    UINT32 OrigUid, OrigGid, OrigPid;
    FSP_FUSE_PROTO_REQ *FuseRequest;
    FSP_FUSE_PROTO_RSP *FuseResponse;
    INT CoroState[8];
    PSTR PosixPath, PosixPathRem, PosixName;
    UINT64 Ino;
    UINT32 Uid, Gid, Mode;
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

/* utility */
NTSTATUS FspFuseNtStatusFromErrno(INT32 Errno);

#endif
