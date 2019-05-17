/**
 * @file sys/fuseioq.c
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

#include <sys/driver.h>

#if defined(WINFSP_SYS_FUSE)
#include <sys/fuse/fuse.h>

#define FSP_FUSE_IOQ_SIZE               1024

NTSTATUS FspFuseIoqCreate(FSP_FUSE_IOQ **PIoq)
{
    *PIoq = 0;

    FSP_FUSE_IOQ *Ioq;
    ULONG BucketCount = (FSP_FUSE_IOQ_SIZE - sizeof *Ioq) / sizeof Ioq->ProcessBuckets[0];
    Ioq = FspAllocNonPaged(FSP_FUSE_IOQ_SIZE);
    if (0 == Ioq)
        return STATUS_INSUFFICIENT_RESOURCES;
    RtlZeroMemory(Ioq, FSP_FUSE_IOQ_SIZE);

    KeInitializeSpinLock(&Ioq->SpinLock);
    InitializeListHead(&Ioq->PendingList);
    InitializeListHead(&Ioq->ProcessList);
    Ioq->ProcessBucketCount = BucketCount;

    *PIoq = Ioq;

    return STATUS_SUCCESS;
}

VOID FspFuseIoqDelete(FSP_FUSE_IOQ *Ioq)
{
    for (PLIST_ENTRY ListEntry = Ioq->PendingList.Flink;
        &Ioq->PendingList != ListEntry;
        ListEntry = ListEntry->Flink)
    {
        FSP_FUSE_CONTEXT *Context = CONTAINING_RECORD(ListEntry, FSP_FUSE_CONTEXT, ListEntry);
        FspFuseProcess(&Context, FspFuseProcessFini, 0, 0);
    }
    for (PLIST_ENTRY ListEntry = Ioq->ProcessList.Flink;
        &Ioq->ProcessList != ListEntry;
        ListEntry = ListEntry->Flink)
    {
        FSP_FUSE_CONTEXT *Context = CONTAINING_RECORD(ListEntry, FSP_FUSE_CONTEXT, ListEntry);
        FspFuseProcess(&Context, FspFuseProcessFini, 0, 0);
    }
    FspFree(Ioq);
}

VOID FspFuseIoqStartProcessing(FSP_FUSE_IOQ *Ioq, FSP_FUSE_CONTEXT *Context)
{
    KIRQL Irql;
    KeAcquireSpinLock(&Ioq->SpinLock, &Irql);

    InsertTailList(&Ioq->ProcessList, &Context->ListEntry);

    ULONG Index = FspHashMixPointer(Context) % Ioq->ProcessBucketCount;
#if DBG
    for (FSP_FUSE_CONTEXT *ContextX = Ioq->ProcessBuckets[Index]; ContextX; ContextX = ContextX->DictNext)
        ASSERT(ContextX != Context);
#endif
    ASSERT(0 == Context->DictNext);
    Context->DictNext = Ioq->ProcessBuckets[Index];
    Ioq->ProcessBuckets[Index] = Context;

    KeReleaseSpinLock(&Ioq->SpinLock, Irql);
}

FSP_FUSE_CONTEXT *FspFuseIoqEndProcessing(FSP_FUSE_IOQ *Ioq, UINT64 Unique)
{
    FSP_FUSE_CONTEXT *ContextHint = (PVOID)(UINT_PTR)Unique;
    FSP_FUSE_CONTEXT *Context = 0;

    KIRQL Irql;
    KeAcquireSpinLock(&Ioq->SpinLock, &Irql);

    ULONG Index = FspHashMixPointer(ContextHint) % Ioq->ProcessBucketCount;
    for (FSP_FUSE_CONTEXT **PContext = &Ioq->ProcessBuckets[Index];; PContext = &(*PContext)->DictNext)
    {
        ASSERT(0 != *PContext);
        if (*PContext == ContextHint)
        {
            *PContext = (*PContext)->DictNext;
            (*PContext)->DictNext = 0;

            Context = ContextHint;
            RemoveEntryList(&Context->ListEntry);

            break;
        }
    }

    KeReleaseSpinLock(&Ioq->SpinLock, Irql);

    return Context;
}

VOID FspFuseIoqPostPending(FSP_FUSE_IOQ *Ioq, FSP_FUSE_CONTEXT *Context)
{
    KIRQL Irql;
    KeAcquireSpinLock(&Ioq->SpinLock, &Irql);

    InsertTailList(&Ioq->PendingList, &Context->ListEntry);

    KeReleaseSpinLock(&Ioq->SpinLock, Irql);
}

FSP_FUSE_CONTEXT *FspFuseIoqNextPending(FSP_FUSE_IOQ *Ioq)
{
    KIRQL Irql;
    KeAcquireSpinLock(&Ioq->SpinLock, &Irql);

    PLIST_ENTRY ListEntry = Ioq->PendingList.Flink;
    FSP_FUSE_CONTEXT *Context = &Ioq->PendingList != ListEntry ?
        CONTAINING_RECORD(ListEntry, FSP_FUSE_CONTEXT, ListEntry) : 0;

    if (0 != Context)
        RemoveEntryList(&Context->ListEntry);

    KeReleaseSpinLock(&Ioq->SpinLock, Irql);

    return Context;
}

#endif
