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

BOOLEAN FspFuseProcess(
    FSP_FUSE_CONTEXT **PContext, FSP_FSCTL_TRANSACT_REQ *InternalRequest,
    FSP_FUSE_PROTO_RSP *FuseResponse, FSP_FUSE_PROTO_REQ *FuseRequest);
NTSTATUS FspVolumeTransactFuse(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
VOID FspFuseInitialize(VOID);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFuseProcess)
#pragma alloc_text(PAGE, FspVolumeTransactFuse)
#pragma alloc_text(INIT, FspFuseInitialize)
#endif

static FSP_FUSE_PROCESS_DISPATCH *FspFuseProcessFunction[FspFsctlTransactKindCount];

VOID FspFuseInitialize(VOID)
{
    FspFuseProcessFunction[FspFsctlTransactCreateKind] = FspFuseOpCreate;
    //FspFuseProcessFunction[FspFsctlTransactOverwriteKind] = FspFuseOpOverwrite;
    FspFuseProcessFunction[FspFsctlTransactCleanupKind] = FspFuseOpCleanup;
    FspFuseProcessFunction[FspFsctlTransactCloseKind] = FspFuseOpClose;
    //FspFuseProcessFunction[FspFsctlTransactReadKind] = FspFuseOpRead;
    //FspFuseProcessFunction[FspFsctlTransactWriteKind] = FspFuseOpWrite;
    //FspFuseProcessFunction[FspFsctlTransactQueryInformationKind] = FspFuseOpQueryInformation;
    //FspFuseProcessFunction[FspFsctlTransactSetInformationKind] = FspFuseOpSetInformation;
    //FspFuseProcessFunction[FspFsctlTransactQueryEaKind] = FspFuseOpQueryEa;
    //FspFuseProcessFunction[FspFsctlTransactSetEaKind] = FspFuseOpSetEa;
    //FspFuseProcessFunction[FspFsctlTransactFlushBuffersKind] = FspFuseOpFlushBuffers;
    //FspFuseProcessFunction[FspFsctlTransactQueryVolumeInformationKind] = FspFuseOpQueryVolumeInformation;
    //FspFuseProcessFunction[FspFsctlTransactSetVolumeInformationKind] = FspFuseOpSetVolumeInformation;
    //FspFuseProcessFunction[FspFsctlTransactQueryDirectoryKind] = FspFuseOpQueryDirectory;
    //FspFuseProcessFunction[FspFsctlTransactFileSystemControlKind] = FspFuseOpFileSystemControl;
    //FspFuseProcessFunction[FspFsctlTransactDeviceControlKind] = FspFuseOpDeviceControl;
    //FspFuseProcessFunction[FspFsctlTransactShutdownKind] = FspFuseOpShutdown;
    //FspFuseProcessFunction[FspFsctlTransactLockControlKind] = FspFuseOpLockControl;
    //FspFuseProcessFunction[FspFsctlTransactQuerySecurityKind] = FspFuseOpQuerySecurity;
    //FspFuseProcessFunction[FspFsctlTransactSetSecurityKind] = FspFuseOpSetSecurity;
    //FspFuseProcessFunction[FspFsctlTransactQueryStreamInformationKind] = FspFuseOpQueryStreamInformation;
}

BOOLEAN FspFuseProcess(
    FSP_FUSE_CONTEXT **PContext, FSP_FSCTL_TRANSACT_REQ *InternalRequest,
    FSP_FUSE_PROTO_RSP *FuseResponse, FSP_FUSE_PROTO_REQ *FuseRequest)
{
    PAGED_CODE();

    FSP_FUSE_CONTEXT *Context = *PContext;
    UINT32 Kind;

    if (0 == Context)
    {
        ASSERT(FspFuseProcessFini != InternalRequest && FspFuseProcessNorm != InternalRequest);
        Kind = InternalRequest->Kind;
    }
    else if (FspFuseContextIsStatus(Context))
        return FALSE;
    else if (FspFuseProcessFini == InternalRequest)
    {
        /* delete context */
        if ((PVOID)&Context->InternalResponseBuf != Context->InternalResponse)
            FspFree(Context->InternalResponse);
        if (0 != Context->PosixPath)
            FspFree(Context->PosixPath);
        FspFree(Context);

        *PContext = 0;
        return FALSE;
    }
    else
        Kind = Context->InternalRequest->Kind;

    ASSERT(FspFsctlTransactKindCount > Kind);
    if (0 == FspFuseProcessFunction[Kind])
    {
        *PContext = FspFuseContextStatus(STATUS_INVALID_DEVICE_REQUEST);
        return FALSE;
    }

    if (0 == Context)
    {
        /* create context */
        Context = FspAlloc(sizeof *Context);
        if (0 == Context)
        {
            *PContext = FspFuseContextStatus(STATUS_INSUFFICIENT_RESOURCES);
            return FALSE;
        }

        RtlZeroMemory(Context, sizeof *Context);
        Context->InternalRequest = InternalRequest;
        Context->InternalResponse = (PVOID)&Context->InternalResponseBuf;
        Context->InternalResponse->Size = sizeof(FSP_FSCTL_TRANSACT_RSP);
        Context->InternalResponse->Kind = InternalRequest->Kind;
        Context->InternalResponse->Hint = InternalRequest->Hint;
        *PContext = Context;
    }

    Context->FuseRequest = FuseRequest;
    Context->FuseResponse = FuseResponse;

    return FspFuseProcessFunction[Kind](Context);
}

NTSTATUS FspVolumeTransactFuse(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    ASSERT(IRP_MJ_FILE_SYSTEM_CONTROL == IrpSp->MajorFunction);
    ASSERT(IRP_MN_USER_FS_REQUEST == IrpSp->MinorFunction);
    ASSERT(FSP_FSCTL_TRANSACT_FUSE == IrpSp->Parameters.FileSystemControl.FsControlCode);
    ASSERT(METHOD_BUFFERED == (IrpSp->Parameters.FileSystemControl.FsControlCode & 3));
    ASSERT(0 != IrpSp->FileObject->FsContext2);

    /* check parameters */
    ULONG InputBufferLength = IrpSp->Parameters.FileSystemControl.InputBufferLength;
    ULONG OutputBufferLength = IrpSp->Parameters.FileSystemControl.OutputBufferLength;
    FSP_FUSE_PROTO_RSP *FuseResponse = 0 != InputBufferLength ? Irp->AssociatedIrp.SystemBuffer : 0;
    FSP_FUSE_PROTO_REQ *FuseRequest = 0 != OutputBufferLength ? Irp->AssociatedIrp.SystemBuffer : 0;
    if (0 != FuseResponse)
    {
        if (FSP_FUSE_PROTO_RSP_HEADER_SIZE > InputBufferLength ||
            FSP_FUSE_PROTO_RSP_HEADER_SIZE > FuseResponse->len ||
            FuseResponse->len > InputBufferLength)
            return STATUS_INVALID_PARAMETER;
    }
    if (0 != FuseRequest)
    {
        if (FSP_FUSE_PROTO_REQ_SIZEMIN > OutputBufferLength)
            return STATUS_BUFFER_TOO_SMALL;
    }

    PDEVICE_OBJECT FsvolDeviceObject = IrpSp->FileObject->FsContext2;
    if (!FspDeviceReference(FsvolDeviceObject))
        return STATUS_CANCELLED;

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);
    FSP_FSCTL_TRANSACT_REQ *InternalRequest = 0;
    FSP_FSCTL_TRANSACT_RSP InternalResponse;
    FSP_FUSE_CONTEXT *Context;
    BOOLEAN Continue;
    NTSTATUS Result;

    if (0 != FuseResponse)
    {
        Context = FspFuseIoqEndProcessing(FsvolDeviceExtension->FuseIoq, FuseResponse->unique);
        if (0 == Context)
            goto request;

        Continue = FspFuseProcess(&Context, FspFuseProcessNorm, FuseResponse, 0);

        if (Continue)
            FspFuseIoqPostPending(FsvolDeviceExtension->FuseIoq, Context);
        else
        {
            Result = FspSendTransactInternalIrp(
                FsvolDeviceObject, IrpSp->FileObject, Context->InternalResponse, 0);
            FspFuseProcess(&Context, FspFuseProcessFini, 0, 0);
            if (!NT_SUCCESS(Result))
                goto exit;
        }
    }

request:
    if (0 != FuseRequest)
    {
        RtlZeroMemory(FuseRequest, FSP_FUSE_PROTO_REQ_HEADER_SIZE);

        Context = FspFuseIoqNextPending(FsvolDeviceExtension->FuseIoq);
        if (0 == Context)
        {
            Result = FspSendTransactInternalIrp(
                FsvolDeviceObject, IrpSp->FileObject, 0, &InternalRequest);
            if (0 == InternalRequest)
                goto exit;

            Continue = FspFuseProcess(&Context, InternalRequest, 0, FuseRequest);
            ASSERT(0 != Context);
            if (!FspFuseContextIsStatus(Context))
                InternalRequest = 0;
        }
        else
            Continue = FspFuseProcess(&Context, FspFuseProcessNorm, 0, FuseRequest);

        if (Continue)
        {
            ASSERT(!FspFuseContextIsStatus(Context));
            FspFuseIoqStartProcessing(FsvolDeviceExtension->FuseIoq, Context);
        }
        else
        {
            if (FspFuseContextIsStatus(Context))
            {
                RtlZeroMemory(&InternalResponse, sizeof InternalResponse);
                InternalResponse.Size = sizeof InternalResponse;
                InternalResponse.Kind = InternalRequest->Kind;
                InternalResponse.Hint = InternalRequest->Hint;
                InternalResponse.IoStatus.Status = FspFuseContextToStatus(Context);
                Result = FspSendTransactInternalIrp(
                    FsvolDeviceObject, IrpSp->FileObject, &InternalResponse, 0);
            }
            else
            {
                Result = FspSendTransactInternalIrp(
                    FsvolDeviceObject, IrpSp->FileObject, Context->InternalResponse, 0);
                FspFuseProcess(&Context, FspFuseProcessFini, 0, 0);
            }

            if (!NT_SUCCESS(Result))
                goto exit;
        }

        Irp->IoStatus.Information = FuseRequest->len;
    }
    else
        Irp->IoStatus.Information = 0;

    Result = STATUS_SUCCESS;

exit:
    if (0 != InternalRequest)
        FspFree(InternalRequest);

    FspDeviceDereference(FsvolDeviceObject);

    return Result;
}

#endif
