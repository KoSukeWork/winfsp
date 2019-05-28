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

static NTSTATUS FspFuseGetTokenUid(HANDLE Token, TOKEN_INFORMATION_CLASS InfoClass, PUINT32 PUid);
static NTSTATUS FspFusePrepareContextNs(FSP_FUSE_CONTEXT *Context);
static VOID FspFuseLookupPath(FSP_FUSE_CONTEXT *Context);
static VOID FspFuseOpCreate_FileCreate(FSP_FUSE_CONTEXT *Context);
static VOID FspFuseOpCreate_FileOpen(FSP_FUSE_CONTEXT *Context);
static VOID FspFuseOpCreate_FileOpenIf(FSP_FUSE_CONTEXT *Context);
static VOID FspFuseOpCreate_FileOverwrite(FSP_FUSE_CONTEXT *Context);
static VOID FspFuseOpCreate_FileOverwriteIf(FSP_FUSE_CONTEXT *Context);
static VOID FspFuseOpCreate_FileOpenTargetDirectory(FSP_FUSE_CONTEXT *Context);
BOOLEAN FspFuseOpCreate(FSP_FUSE_CONTEXT *Context);
BOOLEAN FspFuseOpCleanup(FSP_FUSE_CONTEXT *Context);
BOOLEAN FspFuseOpClose(FSP_FUSE_CONTEXT *Context);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFuseGetTokenUid)
#pragma alloc_text(PAGE, FspFusePrepareContextNs)
#pragma alloc_text(PAGE, FspFuseLookupPath)
#pragma alloc_text(PAGE, FspFuseOpCreate_FileCreate)
#pragma alloc_text(PAGE, FspFuseOpCreate_FileOpen)
#pragma alloc_text(PAGE, FspFuseOpCreate_FileOpenIf)
#pragma alloc_text(PAGE, FspFuseOpCreate_FileOverwrite)
#pragma alloc_text(PAGE, FspFuseOpCreate_FileOverwriteIf)
#pragma alloc_text(PAGE, FspFuseOpCreate_FileOpenTargetDirectory)
#pragma alloc_text(PAGE, FspFuseOpCreate)
#pragma alloc_text(PAGE, FspFuseOpCleanup)
#pragma alloc_text(PAGE, FspFuseOpClose)
#endif

static NTSTATUS FspFuseGetTokenUid(HANDLE Token, TOKEN_INFORMATION_CLASS InfoClass, PUINT32 PUid)
{
    PAGED_CODE();

    PSID Sid;
    union
    {
        TOKEN_USER U;
        TOKEN_OWNER O;
        TOKEN_PRIMARY_GROUP G;
        UINT8 B[128];
    } InfoBuf;
    PVOID Info = &InfoBuf;
    ULONG Size;
    NTSTATUS Result;

    switch (InfoClass)
    {
    case TokenUser:
        Sid = ((PTOKEN_USER)Info)->User.Sid;
        break;
    case TokenOwner:
        Sid = ((PTOKEN_OWNER)Info)->Owner;
        break;
    case TokenPrimaryGroup:
        Sid = ((PTOKEN_PRIMARY_GROUP)Info)->PrimaryGroup;
        break;
    default:
        ASSERT(0);
        return STATUS_INVALID_PARAMETER;
    }

    Result = ZwQueryInformationToken(Token, InfoClass, Info, sizeof InfoBuf, &Size);
    if (!NT_SUCCESS(Result))
    {
        if (STATUS_BUFFER_TOO_SMALL != Result)
            goto exit;

        Info = FspAlloc(Size);
        if (0 == Info)
        {
            Result = STATUS_INSUFFICIENT_RESOURCES;
            goto exit;
        }

        Result = ZwQueryInformationToken(Token, InfoClass, Info, Size, &Size);
        if (!NT_SUCCESS(Result))
            goto exit;
    }

    Result = FspPosixMapSidToUid(Sid, PUid);

exit:
    if (Info != &InfoBuf)
        FspFree(Info);

    return Result;
}

static NTSTATUS FspFusePrepareContextNs(FSP_FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    FSP_FSCTL_TRANSACT_REQ *InternalRequest = Context->InternalRequest;
    UINT32 Uid = (UINT32)-1, Gid = (UINT32)-1, Pid = (UINT32)-1;
    PSTR PosixPath = 0;
    PWSTR FileName = 0, Suffix = 0;
    WCHAR Root[2] = L"\\";
    UINT64 AccessToken = 0;
    NTSTATUS Result;

    if (FspFsctlTransactCreateKind == InternalRequest->Kind)
    {
        if (InternalRequest->Req.Create.OpenTargetDirectory)
            FspPathSuffix((PWSTR)InternalRequest->Buffer, &FileName, &Suffix, Root);
        else
            FileName = (PWSTR)InternalRequest->Buffer;
        AccessToken = InternalRequest->Req.Create.AccessToken;
    }
    else if (FspFsctlTransactSetInformationKind == InternalRequest->Kind &&
        FileRenameInformation == InternalRequest->Req.SetInformation.FileInformationClass)
    {
        FileName = (PWSTR)(InternalRequest->Buffer +
            InternalRequest->Req.SetInformation.Info.Rename.NewFileName.Offset);
        AccessToken = InternalRequest->Req.SetInformation.Info.Rename.AccessToken;
    }

    if (0 != FileName)
    {
        Result = FspPosixMapWindowsToPosixPathEx(FileName, &PosixPath, TRUE);
        if (FspFsctlTransactCreateKind == InternalRequest->Kind &&
            InternalRequest->Req.Create.OpenTargetDirectory)
            FspPathCombine((PWSTR)InternalRequest->Buffer, Suffix);
        if (!NT_SUCCESS(Result))
            goto exit;
    }

    if (0 != AccessToken)
    {
        Result = FspFuseGetTokenUid(
            FSP_FSCTL_TRANSACT_REQ_TOKEN_HANDLE(AccessToken),
            TokenUser,
            &Uid);
        if (!NT_SUCCESS(Result))
            goto exit;

        Result = FspFuseGetTokenUid(
            FSP_FSCTL_TRANSACT_REQ_TOKEN_HANDLE(AccessToken),
            TokenPrimaryGroup,
            &Gid);
        if (!NT_SUCCESS(Result))
            goto exit;

        Pid = FSP_FSCTL_TRANSACT_REQ_TOKEN_PID(AccessToken);
    }

    Context->PosixPath = PosixPath;
    Context->Uid = Uid;
    Context->Gid = Gid;
    Context->Pid = Pid; /* !!!: what about Cygwin? */

    Result = STATUS_SUCCESS;

exit:
    if (!NT_SUCCESS(Result) && 0 != PosixPath)
        FspPosixDeletePath(PosixPath);

    return Result;
}

static VOID FspFuseLookupPath(FSP_FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    coro_block (Context->CoroState)
    {
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
                coro_break;

            Context->FuseRequest->len = (UINT32)(FSP_FUSE_PROTO_REQ_SIZE(req.lookup) + (P - Name) + 1);
            ASSERT(FSP_FUSE_PROTO_REQ_SIZEMIN >= Context->FuseRequest->len);
            Context->FuseRequest->opcode = FSP_FUSE_PROTO_OPCODE_LOOKUP;
            Context->FuseRequest->unique = (UINT64)(UINT_PTR)Context;
            Context->FuseRequest->nodeid = Context->Ino;
            Context->FuseRequest->uid = Context->Uid;
            Context->FuseRequest->gid = Context->Gid;
            Context->FuseRequest->pid = Context->Pid;
            RtlCopyMemory(Context->FuseRequest->req.lookup.name, Name, P - Name);
            Context->FuseRequest->req.lookup.name[P - Name] = '\0';

            coro_yield;

            Context->Ino = Context->FuseResponse->rsp.lookup.entry.nodeid;
            // !!!: REVISIT: access control

            coro_yield;
        }
    }
}

static VOID FspFuseOpCreate_FileCreate(FSP_FUSE_CONTEXT *Context)
{
    PAGED_CODE();
}

static VOID FspFuseOpCreate_FileOpen(FSP_FUSE_CONTEXT *Context)
{
    PAGED_CODE();
}

static VOID FspFuseOpCreate_FileOpenIf(FSP_FUSE_CONTEXT *Context)
{
    PAGED_CODE();
}

static VOID FspFuseOpCreate_FileOverwrite(FSP_FUSE_CONTEXT *Context)
{
    PAGED_CODE();
}

static VOID FspFuseOpCreate_FileOverwriteIf(FSP_FUSE_CONTEXT *Context)
{
    PAGED_CODE();
}

static VOID FspFuseOpCreate_FileOpenTargetDirectory(FSP_FUSE_CONTEXT *Context)
{
    PAGED_CODE();
}

BOOLEAN FspFuseOpCreate(FSP_FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    NTSTATUS Result;
    VOID (*Fn)(FSP_FUSE_CONTEXT *) = 0;

    coro_block (Context->CoroState)
    {
        if (Context->InternalRequest->Req.Create.NamedStream)
        {
            Context->InternalResponse->IoStatus.Status = (UINT32)STATUS_OBJECT_NAME_INVALID;
            coro_break;
        }

        Result = FspFusePrepareContextNs(Context);
        if (!NT_SUCCESS(Result))
        {
            Context->InternalResponse->IoStatus.Status = Result;
            coro_break;
        }

        if (Context->InternalRequest->Req.Create.OpenTargetDirectory)
            Fn = FspFuseOpCreate_FileOpenTargetDirectory;
        else
            switch ((Context->InternalRequest->Req.Create.CreateOptions >> 24) & 0xff)
            {
            case FILE_CREATE:
                Fn = FspFuseOpCreate_FileCreate;
                break;
            case FILE_OPEN:
                Fn = FspFuseOpCreate_FileOpen;
                break;
            case FILE_OPEN_IF:
                Fn = FspFuseOpCreate_FileOpenIf;
                break;
            case FILE_OVERWRITE:
                Fn = FspFuseOpCreate_FileOverwrite;
                break;
            case FILE_OVERWRITE_IF:
            case FILE_SUPERSEDE:
                Fn = FspFuseOpCreate_FileOverwriteIf;
                break;
            }

        if (0 != Fn)
            coro_await (Fn(Context));
        else
            Context->InternalResponse->IoStatus.Status = (UINT32)STATUS_INVALID_PARAMETER;

        coro_break;
    }

    return coro_active();
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
