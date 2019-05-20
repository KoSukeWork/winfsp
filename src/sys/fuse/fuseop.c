/**
 * @file sys/fuseop.c
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

static BOOLEAN FspFuseOpCreate_FileOpenTargetDirectory(
    FSP_FUSE_CONTEXT **PContext, FSP_FSCTL_TRANSACT_REQ *InternalRequest,
    FSP_FUSE_PROTO_RSP *FuseResponse, FSP_FUSE_PROTO_REQ *FuseRequest);
static BOOLEAN FspFuseOpCreate_FileCreate(
    FSP_FUSE_CONTEXT **PContext, FSP_FSCTL_TRANSACT_REQ *InternalRequest,
    FSP_FUSE_PROTO_RSP *FuseResponse, FSP_FUSE_PROTO_REQ *FuseRequest);
static BOOLEAN FspFuseOpCreate_FileOpen(
    FSP_FUSE_CONTEXT **PContext, FSP_FSCTL_TRANSACT_REQ *InternalRequest,
    FSP_FUSE_PROTO_RSP *FuseResponse, FSP_FUSE_PROTO_REQ *FuseRequest);
static BOOLEAN FspFuseOpCreate_FileOpenIf(
    FSP_FUSE_CONTEXT **PContext, FSP_FSCTL_TRANSACT_REQ *InternalRequest,
    FSP_FUSE_PROTO_RSP *FuseResponse, FSP_FUSE_PROTO_REQ *FuseRequest);
static BOOLEAN FspFuseOpCreate_FileOverwrite(
    FSP_FUSE_CONTEXT **PContext, FSP_FSCTL_TRANSACT_REQ *InternalRequest,
    FSP_FUSE_PROTO_RSP *FuseResponse, FSP_FUSE_PROTO_REQ *FuseRequest);
static BOOLEAN FspFuseOpCreate_FileOverwriteIf(
    FSP_FUSE_CONTEXT **PContext, FSP_FSCTL_TRANSACT_REQ *InternalRequest,
    FSP_FUSE_PROTO_RSP *FuseResponse, FSP_FUSE_PROTO_REQ *FuseRequest);
static BOOLEAN FspFuseOpCreate_InvalidParameter(
    FSP_FUSE_CONTEXT **PContext, FSP_FSCTL_TRANSACT_REQ *InternalRequest,
    FSP_FUSE_PROTO_RSP *FuseResponse, FSP_FUSE_PROTO_REQ *FuseRequest);
BOOLEAN FspFuseOpCreate(
    FSP_FUSE_CONTEXT **PContext, FSP_FSCTL_TRANSACT_REQ *InternalRequest,
    FSP_FUSE_PROTO_RSP *FuseResponse, FSP_FUSE_PROTO_REQ *FuseRequest);
BOOLEAN FspFuseOpCleanup(
    FSP_FUSE_CONTEXT **PContext, FSP_FSCTL_TRANSACT_REQ *InternalRequest,
    FSP_FUSE_PROTO_RSP *FuseResponse, FSP_FUSE_PROTO_REQ *FuseRequest);
BOOLEAN FspFuseOpClose(
    FSP_FUSE_CONTEXT **PContext, FSP_FSCTL_TRANSACT_REQ *InternalRequest,
    FSP_FUSE_PROTO_RSP *FuseResponse, FSP_FUSE_PROTO_REQ *FuseRequest);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFuseOpCreate_FileOpenTargetDirectory)
#pragma alloc_text(PAGE, FspFuseOpCreate_FileCreate)
#pragma alloc_text(PAGE, FspFuseOpCreate_FileOpen)
#pragma alloc_text(PAGE, FspFuseOpCreate_FileOpenIf)
#pragma alloc_text(PAGE, FspFuseOpCreate_FileOverwrite)
#pragma alloc_text(PAGE, FspFuseOpCreate_FileOverwriteIf)
#pragma alloc_text(PAGE, FspFuseOpCreate_InvalidParameter)
#pragma alloc_text(PAGE, FspFuseOpCreate)
#pragma alloc_text(PAGE, FspFuseOpCleanup)
#pragma alloc_text(PAGE, FspFuseOpClose)
#endif

#if 0
static VOID FspFuseLookupPath(FSP_FUSE_CONTEXT **PContext)
{
    FSP_FUSE_CONTEXT *Context = *PContext;

    coro_block (Context->CoroState)
    {
        Context->PosixPath = FspPosixMapWindowsToPosixPathEx();

        for (;;)
        {
        }
    }

    PWSTR P, Start;
    UINT64 Ino;

    P = Path;
    for (;;)
    {
        while (L'\\' == *P)
            P++;
        Start = P;
        while (*P && L'\\' != *P)
            P++;
    }
}
#endif

static BOOLEAN FspFuseOpCreate_FileOpenTargetDirectory(
    FSP_FUSE_CONTEXT **PContext, FSP_FSCTL_TRANSACT_REQ *InternalRequest,
    FSP_FUSE_PROTO_RSP *FuseResponse, FSP_FUSE_PROTO_REQ *FuseRequest)
{
    PAGED_CODE();

    return FALSE;
}

static BOOLEAN FspFuseOpCreate_FileCreate(
    FSP_FUSE_CONTEXT **PContext, FSP_FSCTL_TRANSACT_REQ *InternalRequest,
    FSP_FUSE_PROTO_RSP *FuseResponse, FSP_FUSE_PROTO_REQ *FuseRequest)
{
    PAGED_CODE();

    FSP_FUSE_CONTEXT *Context = *PContext;

    coro_block (Context->CoroState)
    {
        coro_await (1, 2);
        coro_yield;
        coro_exit;
    }

    return FALSE;
}

static BOOLEAN FspFuseOpCreate_FileOpen(
    FSP_FUSE_CONTEXT **PContext, FSP_FSCTL_TRANSACT_REQ *InternalRequest,
    FSP_FUSE_PROTO_RSP *FuseResponse, FSP_FUSE_PROTO_REQ *FuseRequest)
{
    PAGED_CODE();

    return FALSE;
}

static BOOLEAN FspFuseOpCreate_FileOpenIf(
    FSP_FUSE_CONTEXT **PContext, FSP_FSCTL_TRANSACT_REQ *InternalRequest,
    FSP_FUSE_PROTO_RSP *FuseResponse, FSP_FUSE_PROTO_REQ *FuseRequest)
{
    PAGED_CODE();

    return FALSE;
}

static BOOLEAN FspFuseOpCreate_FileOverwrite(
    FSP_FUSE_CONTEXT **PContext, FSP_FSCTL_TRANSACT_REQ *InternalRequest,
    FSP_FUSE_PROTO_RSP *FuseResponse, FSP_FUSE_PROTO_REQ *FuseRequest)
{
    PAGED_CODE();

    return FALSE;
}

static BOOLEAN FspFuseOpCreate_FileOverwriteIf(
    FSP_FUSE_CONTEXT **PContext, FSP_FSCTL_TRANSACT_REQ *InternalRequest,
    FSP_FUSE_PROTO_RSP *FuseResponse, FSP_FUSE_PROTO_REQ *FuseRequest)
{
    PAGED_CODE();

    return FALSE;
}

static BOOLEAN FspFuseOpCreate_InvalidParameter(
    FSP_FUSE_CONTEXT **PContext, FSP_FSCTL_TRANSACT_REQ *InternalRequest,
    FSP_FUSE_PROTO_RSP *FuseResponse, FSP_FUSE_PROTO_REQ *FuseRequest)
{
    PAGED_CODE();

    return FALSE;
}

BOOLEAN FspFuseOpCreate(
    FSP_FUSE_CONTEXT **PContext, FSP_FSCTL_TRANSACT_REQ *InternalRequest,
    FSP_FUSE_PROTO_RSP *FuseResponse, FSP_FUSE_PROTO_REQ *FuseRequest)
{
    PAGED_CODE();

    FSP_FUSE_CONTEXT *Context = *PContext;

    if (FspFuseProcessFini == InternalRequest)
    {
        FspFree(Context);
        return FALSE;
    }

    if (0 == Context)
    {
        Context = FspAlloc(sizeof *Context);
        RtlZeroMemory(Context, sizeof *Context);
        Context->InternalRequest = InternalRequest;
        *PContext = Context;
    }

    if (InternalRequest->Req.Create.OpenTargetDirectory)
        return FspFuseOpCreate_FileOpenTargetDirectory(
            PContext, InternalRequest, FuseResponse, FuseRequest);

    switch ((InternalRequest->Req.Create.CreateOptions >> 24) & 0xff)
    {
    case FILE_CREATE:
        return FspFuseOpCreate_FileCreate(
            PContext, InternalRequest, FuseResponse, FuseRequest);
    case FILE_OPEN:
        return FspFuseOpCreate_FileOpen(
            PContext, InternalRequest, FuseResponse, FuseRequest);
    case FILE_OPEN_IF:
        return FspFuseOpCreate_FileOpenIf(
            PContext, InternalRequest, FuseResponse, FuseRequest);
    case FILE_OVERWRITE:
        return FspFuseOpCreate_FileOverwrite(
            PContext, InternalRequest, FuseResponse, FuseRequest);
    case FILE_OVERWRITE_IF:
    case FILE_SUPERSEDE:
        return FspFuseOpCreate_FileOverwriteIf(
            PContext, InternalRequest, FuseResponse, FuseRequest);
    default:
        return FspFuseOpCreate_InvalidParameter(
            PContext, InternalRequest, FuseResponse, FuseRequest);
    }
}

BOOLEAN FspFuseOpCleanup(
    FSP_FUSE_CONTEXT **PContext, FSP_FSCTL_TRANSACT_REQ *InternalRequest,
    FSP_FUSE_PROTO_RSP *FuseResponse, FSP_FUSE_PROTO_REQ *FuseRequest)
{
    PAGED_CODE();

    *PContext = FspFuseContextStatus(STATUS_INVALID_DEVICE_REQUEST);
    return FALSE;
}

BOOLEAN FspFuseOpClose(
    FSP_FUSE_CONTEXT **PContext, FSP_FSCTL_TRANSACT_REQ *InternalRequest,
    FSP_FUSE_PROTO_RSP *FuseResponse, FSP_FUSE_PROTO_REQ *FuseRequest)
{
    PAGED_CODE();

    *PContext = FspFuseContextStatus(STATUS_INVALID_DEVICE_REQUEST);
    return FALSE;
}

#endif
