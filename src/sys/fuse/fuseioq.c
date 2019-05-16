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

NTSTATUS FspFuseIoqCreate(FSP_FUSE_IOQ **PIoq)
{
    *PIoq = 0;

    return STATUS_SUCCESS;
}

VOID FspFuseIoqDelete(FSP_FUSE_IOQ *Ioq)
{
}

VOID FspFuseIoqStartProcessing(FSP_FUSE_IOQ *Ioq, FSP_FUSE_CONTEXT *Context)
{
}

FSP_FUSE_CONTEXT *FspFuseIoqEndProcessing(FSP_FUSE_IOQ *Ioq, UINT64 Unique)
{
    return 0;
}

VOID FspFuseIoqPostPending(FSP_FUSE_IOQ *Ioq, FSP_FUSE_CONTEXT *Context)
{
}

FSP_FUSE_CONTEXT *FspFuseIoqNextPending(FSP_FUSE_IOQ *Ioq)
{
    return 0;
}

#endif
