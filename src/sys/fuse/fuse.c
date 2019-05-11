/**
 * @file sys/fuse.c
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

NTSTATUS FspVolumeTransactFuse(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspVolumeTransactFuse)
#endif

NTSTATUS FspVolumeTransactFuse(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    return STATUS_INVALID_DEVICE_REQUEST;
#if 0
    ASSERT(IRP_MJ_FILE_SYSTEM_CONTROL == IrpSp->MajorFunction);
    ASSERT(IRP_MN_USER_FS_REQUEST == IrpSp->MinorFunction);
    ASSERT(
        FSP_FSCTL_TRANSACT_FUSE == IrpSp->Parameters.FileSystemControl.FsControlCode);
    ASSERT(
        METHOD_BUFFERED == (IrpSp->Parameters.FileSystemControl.FsControlCode & 3) ||
        METHOD_OUT_DIRECT == (IrpSp->Parameters.FileSystemControl.FsControlCode & 3));
    ASSERT(0 != IrpSp->FileObject->FsContext2);

    /* check parameters */
    PDEVICE_OBJECT FsvolDeviceObject = IrpSp->FileObject->FsContext2;
    ULONG ControlCode = IrpSp->Parameters.FileSystemControl.FsControlCode;
    ULONG InputBufferLength = IrpSp->Parameters.FileSystemControl.InputBufferLength;
    ULONG OutputBufferLength = IrpSp->Parameters.FileSystemControl.OutputBufferLength;
    PVOID InputBuffer = Irp->AssociatedIrp.SystemBuffer;
    PVOID OutputBuffer = 0;
    if (0 != InputBufferLength &&
        FSP_FSCTL_DEFAULT_ALIGN_UP(sizeof(FSP_FSCTL_TRANSACT_RSP)) > InputBufferLength)
        return STATUS_INVALID_PARAMETER;
    if (0 != OutputBufferLength &&
        ((FSP_FSCTL_TRANSACT == ControlCode &&
            FSP_FSCTL_TRANSACT_BUFFER_SIZEMIN > OutputBufferLength) ||
        (FSP_FSCTL_TRANSACT_BATCH == ControlCode &&
            FSP_FSCTL_TRANSACT_BATCH_BUFFER_SIZEMIN > OutputBufferLength)))
        return STATUS_BUFFER_TOO_SMALL;

    if (!FspDeviceReference(FsvolDeviceObject))
        return STATUS_CANCELLED;

    NTSTATUS Result;
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);
    PUINT8 BufferEnd;
    FSP_FSCTL_TRANSACT_RSP *Response, *NextResponse;
    FSP_FSCTL_TRANSACT_REQ *Request, *PendingIrpRequest;
    PIRP ProcessIrp, PendingIrp, RetriedIrp, RepostedIrp;
    ULONG LoopCount;
    LARGE_INTEGER Timeout;
    PIRP TopLevelIrp = IoGetTopLevelIrp();

    /* process any user-mode file system responses */
    RepostedIrp = 0;
    Response = InputBuffer;
    BufferEnd = (PUINT8)InputBuffer + InputBufferLength;
    for (;;)
    {
        NextResponse = FspFsctlTransactConsumeResponse(Response, BufferEnd);
        if (0 == NextResponse)
            break;

        ProcessIrp = FspIoqEndProcessingIrp(FsvolDeviceExtension->Ioq, (UINT_PTR)Response->Hint);
        if (0 == ProcessIrp)
        {
            /* either IRP was canceled or a bogus Hint was provided */
            DEBUGLOG("BOGUS(Kind=%d, Hint=%p)", Response->Kind, (PVOID)(UINT_PTR)Response->Hint);
            Response = NextResponse;
            continue;
        }

        ASSERT((UINT_PTR)ProcessIrp == (UINT_PTR)Response->Hint);
        ASSERT(FspIrpRequest(ProcessIrp)->Hint == Response->Hint);

        IoSetTopLevelIrp(ProcessIrp);
        Result = FspIopDispatchComplete(ProcessIrp, Response);
        if (STATUS_PENDING == Result)
        {
            /*
             * The IRP has been reposted to our Ioq. Remember the first such IRP,
             * so that we know to break the loop if we see it again.
             */
            if (0 == RepostedIrp)
                RepostedIrp = ProcessIrp;
        }

        Response = NextResponse;
    }

    /* process any retried IRP's */
    LoopCount = FspIoqRetriedIrpCount(FsvolDeviceExtension->Ioq);
    while (0 < LoopCount--) /* upper bound on loop guarantees forward progress! */
    {
        /* get the next retried IRP, but do not go beyond the first reposted IRP! */
        RetriedIrp = FspIoqNextCompleteIrp(FsvolDeviceExtension->Ioq, RepostedIrp);
        if (0 == RetriedIrp)
            break;

        IoSetTopLevelIrp(RetriedIrp);
        Response = FspIopIrpResponse(RetriedIrp);
        Result = FspIopDispatchComplete(RetriedIrp, Response);
        if (STATUS_PENDING == Result)
        {
            /*
             * The IRP has been reposted to our Ioq. Remember the first such IRP,
             * so that we know to break the loop if we see it again.
             */
            if (0 == RepostedIrp)
                RepostedIrp = RetriedIrp;
        }
    }

    /* were we sent an output buffer? */
    switch (ControlCode & 3)
    {
    case METHOD_OUT_DIRECT:
        if (0 != Irp->MdlAddress)
            OutputBuffer = MmGetMdlVirtualAddress(Irp->MdlAddress);
        break;
    case METHOD_BUFFERED:
        if (0 != OutputBufferLength)
            OutputBuffer = Irp->AssociatedIrp.SystemBuffer;
        break;
    default:
        ASSERT(0);
        break;
    }
    if (0 == OutputBuffer)
    {
        Irp->IoStatus.Information = 0;
        Result = STATUS_SUCCESS;
        goto exit;
    }

    /* wait for an IRP to arrive */
    KeQuerySystemTime(&Timeout);
    Timeout.QuadPart += 0 == RepostedIrp ?
        FsvolDeviceExtension->VolumeParams.TransactTimeout * 10000ULL :
        FspVolumeTransactEarlyTimeout;
        /* convert millis to nanos and add to absolute time */
    while (0 == (PendingIrp = FspIoqNextPendingIrp(FsvolDeviceExtension->Ioq, 0, &Timeout, Irp)))
    {
        if (FspIoqStopped(FsvolDeviceExtension->Ioq))
        {
            Result = STATUS_CANCELLED;
            goto exit;
        }
    }
    if (FspIoqTimeout == PendingIrp || FspIoqCancelled == PendingIrp)
    {
        Irp->IoStatus.Information = 0;
        Result = FspIoqTimeout == PendingIrp ? STATUS_SUCCESS : STATUS_CANCELLED;
        goto exit;
    }

    /* send any pending IRP's to the user-mode file system */
    RepostedIrp = 0;
    Request = OutputBuffer;
    BufferEnd = (PUINT8)OutputBuffer + OutputBufferLength;
    ASSERT(FspFsctlTransactCanProduceRequest(Request, BufferEnd));
    LoopCount = FspIoqPendingIrpCount(FsvolDeviceExtension->Ioq);
    for (;;)
    {
        PendingIrpRequest = FspIrpRequest(PendingIrp);

        IoSetTopLevelIrp(PendingIrp);
        Result = FspIopDispatchPrepare(PendingIrp, PendingIrpRequest);
        if (STATUS_PENDING == Result)
        {
            /*
             * The IRP has been reposted to our Ioq. Remember the first such IRP,
             * so that we know to break the loop if we see it again.
             */
            if (0 == RepostedIrp)
                RepostedIrp = PendingIrp;
        }
        else if (!NT_SUCCESS(Result))
            FspIopCompleteIrp(PendingIrp, Result);
        else
        {
            RtlCopyMemory(Request, PendingIrpRequest, PendingIrpRequest->Size);
            Request = FspFsctlTransactProduceRequest(Request, PendingIrpRequest->Size);

            if (!FspIoqStartProcessingIrp(FsvolDeviceExtension->Ioq, PendingIrp))
            {
                /*
                 * This can only happen if the Ioq was stopped. Abandon everything
                 * and return STATUS_CANCELLED. Any IRP's in the Pending and Process
                 * queues of the Ioq will be cancelled during FspIoqStop(). We must
                 * also cancel the PendingIrp we have in our hands.
                 */
                ASSERT(FspIoqStopped(FsvolDeviceExtension->Ioq));
                FspIopCompleteCanceledIrp(PendingIrp);
                Result = STATUS_CANCELLED;
                goto exit;
            }

            /* are we doing single request or batch mode? */
            if (FSP_FSCTL_TRANSACT == ControlCode)
                break;

            /* check that we have enough space before pulling the next pending IRP off the queue */
            if (!FspFsctlTransactCanProduceRequest(Request, BufferEnd))
                break;
        }
        
        if (0 >= LoopCount--) /* upper bound on loop guarantees forward progress! */
            break;

        /* get the next pending IRP, but do not go beyond the first reposted IRP! */
        PendingIrp = FspIoqNextPendingIrp(FsvolDeviceExtension->Ioq, RepostedIrp, 0, Irp);
        if (0 == PendingIrp)
            break;
    }

    Irp->IoStatus.Information = (PUINT8)Request - (PUINT8)OutputBuffer;
    Result = STATUS_SUCCESS;

exit:
    IoSetTopLevelIrp(TopLevelIrp);
    FspDeviceDereference(FsvolDeviceObject);
    return Result;
#endif
}

#endif
