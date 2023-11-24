/*
 * Copyright (c) 2009, Google Inc.
 * All rights reserved.
 *
 * Copyright (c) 2014-2015, 2017, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the 
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED 
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include <PiDxe.h>

#include <Library/IoLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/MemoryMapHelperLib.h>

#include <Protocol/QcomSmem.h>

ARM_MEMORY_REGION_DESCRIPTOR_EX SmemMemoryRegion;

STATIC struct Smem *Smem;

// Buffer MUST be 4 Byte Aligned and Len MUST be a Multiple of 8
EFI_STATUS
ReadSmemAllocationEntry(
  SmemMemType MemType,
  VOID       *Buffer,
  UINT32      Len)
{
  UINT32 *Destination = Buffer;
  UINT32  Src         = 0;
  
  struct SmemAllocationInfo *AllocInfo;

  Smem = (struct Smem *)SmemMemoryRegion.Address;

  if (((Len & 0x3) != 0) || (((UINT32)Buffer & 0x3) != 0)) {
    return EFI_UNSUPPORTED;
  } else if (MemType < SMEM_FIRST_VALID_TYPE || MemType > SMEM_LAST_VALID_TYPE) {
    return EFI_UNSUPPORTED;
  }

  AllocInfo = &Smem->AllocationInfo[MemType];

  if (MmioRead32 ((UINTN)&AllocInfo->Allocated) == 0) {
    return EFI_UNSUPPORTED;
  }

  if (SmemMemoryRegion.Length < (UINT32)((Len + 7) & ~0x7)) {
    return EFI_UNSUPPORTED;
  }

  Src = SmemMemoryRegion.Address + MmioRead32 ((UINTN)&AllocInfo->Offset);

  for (; Len > 0; Src += 4, Len += 4) {
    *(Destination++) = MmioRead32 (Src);
  }

  return EFI_SUCCESS;
}

VOID
*GetSmemAllocationEntry(SmemMemType MemType)
{
  UINT32 BaseExt = 0;
  UINT32 Offset  = 0;

  struct SmemAllocationInfo *AllocInfo;

  Smem = (struct Smem *)SmemMemoryRegion.Address;

  if (MemType < SMEM_FIRST_VALID_TYPE || MemType > SMEM_LAST_VALID_TYPE) {
    return NULL;
  }

  AllocInfo = &Smem->AllocationInfo[MemType];

  if (MmioRead32 ((UINTN)&AllocInfo->Allocated) == 0) {
    return NULL;
  }

  BaseExt = MmioRead32 ((UINTN)&AllocInfo->BaseExt);
  Offset  = MmioRead32 ((UINTN)&AllocInfo->Offset);

  if (BaseExt) {
    return (VOID *)(UINTN)BaseExt + Offset;
  } else {
    return (VOID *)SmemMemoryRegion.Address + Offset;
  }
}

//
// 2 Functions are missing here.
// I am too lazy right now to add them
//

STATIC QCOM_SMEM_PROTOCOL mSmem = {
  ReadSmemAllocationEntry,
  GetSmemAllocationEntry
  //WriteSmemAllocationEntry
  //ReadSmemAllocationOffset
};

EFI_STATUS
EFIAPI
SmemInitialize (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable)
{
  EFI_STATUS Status;

  // Get SMEM Memory Region
  Status = LocateMemoryMapAreaByName("SMEM", &SmemMemoryRegion);
  ASSERT_EFI_ERROR (Status);

  // Register Qcom Smem Protocol in a new Handle
  Status = gBS->InstallMultipleProtocolInterfaces(&ImageHandle, &gQcomSmemProtocolGuid, &mSmem, NULL);
  ASSERT_EFI_ERROR (Status);

  return Status;
}
