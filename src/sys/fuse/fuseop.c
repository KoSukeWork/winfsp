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

static VOID FspFuseLookupPath(FSP_FUSE_CONTEXT *Context);
static BOOLEAN FspFuseOpCreate_FileOpenTargetDirectory(FSP_FUSE_CONTEXT *Context);
static BOOLEAN FspFuseOpCreate_FileCreate(FSP_FUSE_CONTEXT *Context);
static BOOLEAN FspFuseOpCreate_FileOpen(FSP_FUSE_CONTEXT *Context);
static BOOLEAN FspFuseOpCreate_FileOpenIf(FSP_FUSE_CONTEXT *Context);
static BOOLEAN FspFuseOpCreate_FileOverwrite(FSP_FUSE_CONTEXT *Context);
static BOOLEAN FspFuseOpCreate_FileOverwriteIf(FSP_FUSE_CONTEXT *Context);
static BOOLEAN FspFuseOpCreate_InvalidParameter(FSP_FUSE_CONTEXT *Context);
BOOLEAN FspFuseOpCreate(FSP_FUSE_CONTEXT *Context);
BOOLEAN FspFuseOpCleanup(FSP_FUSE_CONTEXT *Context);
BOOLEAN FspFuseOpClose(FSP_FUSE_CONTEXT *Context);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFuseLookupPath)
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

static VOID FspFuseLookupPath(FSP_FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    coro_block (Context->CoroState)
    {
        NTSTATUS Result;

        Result = FspPosixMapWindowsToPosixPathEx(
            (PWSTR)Context->InternalRequest->Buffer,
            &Context->PosixPath,
            TRUE);
        if (!NT_SUCCESS(Result))
        {
            Context->InternalResponse->IoStatus.Status = Result;
            Context->InternalResponse->IoStatus.Information = 0;
            coro_exit;
        }

        Context->PosixPathRem = Context->PosixPath;
        Context->Ino = FSP_FUSE_PROTO_ROOT_ID;

        for (;;)
        {
            PSTR P, Name;

            P = Context->PosixPathRem;
            while (L'\\' == *P)
                P++;
            Name = P;
            while (*P && L'\\' != *P)
                P++;
            Context->PosixPathRem = P;

            if (Name == P)
            {
                /* !!!: REVISIT */
                Context->InternalResponse->IoStatus.Status = STATUS_SUCCESS;
                Context->InternalResponse->IoStatus.Information = 0;
                coro_exit;
            }

            Context->FuseRequest->len = (UINT32)(FSP_FUSE_PROTO_REQ_SIZE(req.lookup) + (P - Name) + 1);
            ASSERT(FSP_FUSE_PROTO_REQ_SIZEMIN >= Context->FuseRequest->len);
            Context->FuseRequest->opcode = FSP_FUSE_PROTO_OPCODE_LOOKUP;
            Context->FuseRequest->unique = (UINT64)(UINT_PTR)Context;
            Context->FuseRequest->nodeid = Context->Ino;
            Context->FuseRequest->uid = 0; // !!!: REVISIT
            Context->FuseRequest->gid = 0; // !!!: REVISIT
            Context->FuseRequest->pid = 0; // !!!: REVISIT
            RtlCopyMemory(Context->FuseRequest->req.lookup.name, Name, P - Name);
            Context->FuseRequest->req.lookup.name[P - Name] = '\0';

            coro_yield;

            Context->Ino = Context->FuseResponse->rsp.lookup.entry.nodeid;
            // !!!: REVISIT: access control

            coro_yield;
        }
    }
}

static BOOLEAN FspFuseOpCreate_FileOpenTargetDirectory(FSP_FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    return FALSE;
}

static BOOLEAN FspFuseOpCreate_FileCreate(FSP_FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    return FALSE;
}

static BOOLEAN FspFuseOpCreate_FileOpen(FSP_FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    return FALSE;
}

static BOOLEAN FspFuseOpCreate_FileOpenIf(FSP_FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    return FALSE;
}

static BOOLEAN FspFuseOpCreate_FileOverwrite(FSP_FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    return FALSE;
}

static BOOLEAN FspFuseOpCreate_FileOverwriteIf(FSP_FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    return FALSE;
}

static BOOLEAN FspFuseOpCreate_InvalidParameter(FSP_FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    return FALSE;
}

BOOLEAN FspFuseOpCreate(FSP_FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    if (Context->InternalRequest->Req.Create.OpenTargetDirectory)
        return FspFuseOpCreate_FileOpenTargetDirectory(Context);

    switch ((Context->InternalRequest->Req.Create.CreateOptions >> 24) & 0xff)
    {
    case FILE_CREATE:
        return FspFuseOpCreate_FileCreate(Context);
    case FILE_OPEN:
        return FspFuseOpCreate_FileOpen(Context);
    case FILE_OPEN_IF:
        return FspFuseOpCreate_FileOpenIf(Context);
    case FILE_OVERWRITE:
        return FspFuseOpCreate_FileOverwrite(Context);
    case FILE_OVERWRITE_IF:
    case FILE_SUPERSEDE:
        return FspFuseOpCreate_FileOverwriteIf(Context);
    default:
        return FspFuseOpCreate_InvalidParameter(Context);
    }
}

BOOLEAN FspFuseOpCleanup(FSP_FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    return FALSE;
}

BOOLEAN FspFuseOpClose(FSP_FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    return FALSE;
}

#endif
