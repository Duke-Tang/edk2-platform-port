/** @file
  Quanta Platform DXE driver.

  Installs a vendor-defined SMBIOS Type 128 OEM record that carries the
  Quanta board identifier (PcdQuantaBoardId) and the firmware version string
  (PcdQuantaFirmwareVersionString).

  Type 128 record layout (fixed part):
    Offset 0:  SMBIOS_STRUCTURE Hdr  (Type=128, Length=sizeof fixed part)
    Offset 4:  UINT16 BoardId
    Offset 6:  UINT8  FwVersionStringIndex  (1-based index into string table)
    Offset 7:  UINT8  Reserved

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <IndustryStandard/SmBios.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/Smbios.h>

#pragma pack(1)
typedef struct {
  SMBIOS_STRUCTURE  Hdr;
  UINT16            BoardId;
  UINT8             FwVersionStringIndex;
  UINT8             Reserved;
} QUANTA_SMBIOS_TYPE128;
#pragma pack()

STATIC
EFI_STATUS
InstallSmbiosType128 (
  IN EFI_SMBIOS_PROTOCOL  *Smbios
  )
{
  EFI_STATUS          Status;
  CHAR16              *FwVerUcs2;
  UINTN               FwVerLen;
  CHAR8               *FwVerAscii;
  UINTN               RecordSize;
  UINT8               *Record;
  EFI_SMBIOS_HANDLE   Handle;

  FwVerUcs2 = (CHAR16 *)PcdGetPtr (PcdQuantaFirmwareVersionString);
  FwVerLen  = StrLen (FwVerUcs2);

  FwVerAscii = AllocateZeroPool (FwVerLen + 1);
  if (FwVerAscii == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }
  UnicodeStrToAsciiStrS (FwVerUcs2, FwVerAscii, FwVerLen + 1);

  // Record = fixed header + string-table (one string + double-NUL terminator)
  RecordSize = sizeof (QUANTA_SMBIOS_TYPE128) + FwVerLen + 1 + 1;
  Record     = AllocateZeroPool (RecordSize);
  if (Record == NULL) {
    FreePool (FwVerAscii);
    return EFI_OUT_OF_RESOURCES;
  }

  ((QUANTA_SMBIOS_TYPE128 *)Record)->Hdr.Type              = 128;
  ((QUANTA_SMBIOS_TYPE128 *)Record)->Hdr.Length            = sizeof (QUANTA_SMBIOS_TYPE128);
  ((QUANTA_SMBIOS_TYPE128 *)Record)->BoardId               = (UINT16)PcdGet16 (PcdQuantaBoardId);
  ((QUANTA_SMBIOS_TYPE128 *)Record)->FwVersionStringIndex  = 1;
  ((QUANTA_SMBIOS_TYPE128 *)Record)->Reserved              = 0;

  CopyMem (Record + sizeof (QUANTA_SMBIOS_TYPE128), FwVerAscii, FwVerLen);
  // Bytes at [+FwVerLen] and [+FwVerLen+1] are already 0 (AllocateZeroPool)

  Handle = SMBIOS_HANDLE_PI_RESERVED;
  Status = Smbios->Add (
                     Smbios,
                     NULL,
                     &Handle,
                     (EFI_SMBIOS_TABLE_HEADER *)Record
                     );

  FreePool (Record);
  FreePool (FwVerAscii);

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR,
      "QuantaPlatformDxe: SMBIOS Add Type 128 failed: %r\n",
      Status));
  } else {
    DEBUG ((DEBUG_INFO,
      "QuantaPlatformDxe: SMBIOS Type 128 installed "
      "(Handle=0x%04X BoardId=0x%04X FwVer=%s)\n",
      Handle,
      PcdGet16 (PcdQuantaBoardId),
      (CHAR16 *)PcdGetPtr (PcdQuantaFirmwareVersionString)));
  }

  return Status;
}

EFI_STATUS
EFIAPI
QuantaPlatformDxeEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS          Status;
  EFI_SMBIOS_PROTOCOL *Smbios;

  Status = gBS->LocateProtocol (
                  &gEfiSmbiosProtocolGuid,
                  NULL,
                  (VOID **)&Smbios
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR,
      "QuantaPlatformDxe: LocateProtocol SMBIOS failed: %r\n",
      Status));
    return Status;
  }

  return InstallSmbiosType128 (Smbios);
}
