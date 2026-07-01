#include <winfsp/winfsp.hpp>
#include <strsafe.h>
#include "FsCrypto.h"

#define PROGNAME                        "bytelockfs"
#define ALLOCATION_UNIT                 4096
#define FULLPATH_SIZE                   (MAX_PATH + FSP_FSCTL_TRANSACT_PATH_SIZEMAX / sizeof(WCHAR))

#define info(format, ...)               Service::Log(EVENTLOG_INFORMATION_TYPE, format, __VA_ARGS__)
#define warn(format, ...)               Service::Log(EVENTLOG_WARNING_TYPE, format, __VA_ARGS__)
#define fail(format, ...)               Service::Log(EVENTLOG_ERROR_TYPE, format, __VA_ARGS__)

#define ConcatPath(FN, FP)              (0 == StringCbPrintfW(FP, sizeof FP, L"%s%s", _Path, FN))
#define HandleFromFileDesc(FD)          ((BlfsFileDesc *)(FD))->Handle

using namespace Fsp;

class Blfs : public FileSystemBase
{
public:
    Blfs();
    ~Blfs();

    NTSTATUS SetPath(PWSTR Path);

protected:
    static NTSTATUS GetFileInfoInternal(HANDLE Handle, FileInfo* FileInfo);

    NTSTATUS Init(PVOID Host);

    NTSTATUS GetVolumeInfo(VolumeInfo* Info);

    NTSTATUS GetSecurityByName(
        PWSTR FileName,
        PUINT32 PFileAttributes,
        PSECURITY_DESCRIPTOR SecurityDescriptor,
        SIZE_T* PSecurityDescriptorSize);

    NTSTATUS Create(
        PWSTR FileName,
        UINT32 CreateOptions,
        UINT32 GrantedAccess,
        UINT32 FileAttributes,
        PSECURITY_DESCRIPTOR SecurityDescriptor,
        UINT64 AllocationSize,
        PVOID* PFileNode,
        PVOID* PFileDesc,
        OpenFileInfo* OpenFileInfo);

    NTSTATUS Open(
        PWSTR FileName,
        UINT32 CreateOptions,
        UINT32 GrantedAccess,
        PVOID* PFileNode,
        PVOID* PFileDesc,
        OpenFileInfo* OpenFileInfo);

    NTSTATUS Overwrite(
        PVOID FileNode,
        PVOID FileDesc,
        UINT32 FileAttributes,
        BOOLEAN ReplaceFileAttributes,
        UINT64 AllocationSize,
        FileInfo* FileInfo);

    VOID Cleanup(
        PVOID FileNode,
        PVOID FileDesc,
        PWSTR FileName,
        ULONG Flags);

    VOID Close(
        PVOID FileNode,
        PVOID FileDesc);

    NTSTATUS Read(
        PVOID FileNode,
        PVOID FileDesc,
        PVOID Buffer,
        UINT64 Offset,
        ULONG Length,
        PULONG PBytesTransferred);

    NTSTATUS Write(
        PVOID FileNode,
        PVOID FileDesc,
        PVOID Buffer,
        UINT64 Offset,
        ULONG Length,
        BOOLEAN WriteToEndOfFile,
        BOOLEAN ConstrainedIo,
        PULONG PBytesTransferred,
        FileInfo* FileInfo);

    NTSTATUS Flush(
        PVOID FileNode,
        PVOID FileDesc,
        FileInfo* FileInfo);

    NTSTATUS GetFileInfo(
        PVOID FileNode,
        PVOID FileDesc,
        FileInfo* FileInfo);

    NTSTATUS SetBasicInfo(
        PVOID FileNode,
        PVOID FileDesc,
        UINT32 FileAttributes,
        UINT64 CreationTime,
        UINT64 LastAccessTime,
        UINT64 LastWriteTime,
        UINT64 ChangeTime,
        FileInfo* FileInfo);

    NTSTATUS SetFileSize(
        PVOID FileNode,
        PVOID FileDesc,
        UINT64 NewSize,
        BOOLEAN SetAllocationSize,
        FileInfo* FileInfo);

    NTSTATUS CanDelete(
        PVOID FileNode,
        PVOID FileDesc,
        PWSTR FileName);

    NTSTATUS Rename(
        PVOID FileNode,
        PVOID FileDesc,
        PWSTR FileName,
        PWSTR NewFileName,
        BOOLEAN ReplaceIfExists);

    NTSTATUS GetSecurity(
        PVOID FileNode,
        PVOID FileDesc,
        PSECURITY_DESCRIPTOR SecurityDescriptor,
        SIZE_T* PSecurityDescriptorSize);

    NTSTATUS SetSecurity(
        PVOID FileNode,
        PVOID FileDesc,
        SECURITY_INFORMATION SecurityInformation,
        PSECURITY_DESCRIPTOR ModificationDescriptor);

    NTSTATUS ReadDirectory(
        PVOID FileNode,
        PVOID FileDesc,
        PWSTR Pattern,
        PWSTR Marker,
        PVOID Buffer,
        ULONG Length,
        PULONG PBytesTransferred);

    NTSTATUS ReadDirectoryEntry(
        PVOID FileNode,
        PVOID FileDesc,
        PWSTR Pattern,
        PWSTR Marker,
        PVOID* PContext,
        DirInfo* DirInfo);

private:
    PWSTR _Path;
    UINT64 _CreationTime;
};

struct BlfsFileDesc
{
    BlfsFileDesc() : Handle(INVALID_HANDLE_VALUE), DirBuffer()
    {
    }
    ~BlfsFileDesc()
    {
        CloseHandle(Handle);
        Blfs::DeleteDirectoryBuffer(&DirBuffer);
    }

    HANDLE Handle;
    PVOID DirBuffer;
};

Blfs::Blfs() : FileSystemBase(), _Path()
{
}

Blfs::~Blfs()
{
    delete[] _Path;
}

NTSTATUS Blfs::SetPath(PWSTR Path)
{
    WCHAR FullPath[FULLPATH_SIZE];
    ULONG Length;
    HANDLE Handle;
    FILETIME CreationTime;
    DWORD LastError;

    Handle = CreateFileW(
        Path, FILE_READ_ATTRIBUTES, 0, 0,
        OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
    if (INVALID_HANDLE_VALUE == Handle)
        return NtStatusFromWin32(GetLastError());

    Length = GetFinalPathNameByHandleW(Handle, FullPath, FULLPATH_SIZE - 1, 0);
    if (0 == Length)
    {
        LastError = GetLastError();
        CloseHandle(Handle);
        return NtStatusFromWin32(LastError);
    }
    if (L'\\' == FullPath[Length - 1])
        FullPath[--Length] = L'\0';

    if (!GetFileTime(Handle, &CreationTime, 0, 0))
    {
        LastError = GetLastError();
        CloseHandle(Handle);
        return NtStatusFromWin32(LastError);
    }

    CloseHandle(Handle);

    Length++;
    _Path = new WCHAR[Length];
    memcpy(_Path, FullPath, Length * sizeof(WCHAR));

    _CreationTime = ((PLARGE_INTEGER)&CreationTime)->QuadPart;

    return STATUS_SUCCESS;
}

NTSTATUS Blfs::GetFileInfoInternal(HANDLE Handle, FileInfo* FileInfo)
{
    BY_HANDLE_FILE_INFORMATION ByHandleFileInfo;

    if (!GetFileInformationByHandle(Handle, &ByHandleFileInfo))
        return NtStatusFromWin32(GetLastError());

    UINT64 CipherSize =
        ((UINT64)ByHandleFileInfo.nFileSizeHigh << 32) | (UINT64)ByHandleFileInfo.nFileSizeLow;

    FileInfo->FileAttributes = ByHandleFileInfo.dwFileAttributes;
    FileInfo->ReparseTag = 0;
    if (FileInfo->FileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        FileInfo->FileSize = CipherSize;
    else
        FileInfo->FileSize = FsCrypto::PlainSizeFromCipherSize(CipherSize);
    FileInfo->AllocationSize = (FileInfo->FileSize + ALLOCATION_UNIT - 1)
        / ALLOCATION_UNIT * ALLOCATION_UNIT;
    FileInfo->CreationTime = ((PLARGE_INTEGER)&ByHandleFileInfo.ftCreationTime)->QuadPart;
    FileInfo->LastAccessTime = ((PLARGE_INTEGER)&ByHandleFileInfo.ftLastAccessTime)->QuadPart;
    FileInfo->LastWriteTime = ((PLARGE_INTEGER)&ByHandleFileInfo.ftLastWriteTime)->QuadPart;
    FileInfo->ChangeTime = FileInfo->LastWriteTime;
    FileInfo->IndexNumber = 0;
    FileInfo->HardLinks = 0;

    return STATUS_SUCCESS;
}

NTSTATUS Blfs::Init(PVOID Host0)
{
    FileSystemHost* Host = (FileSystemHost*)Host0;

    Host->SetSectorSize(ALLOCATION_UNIT);
    Host->SetSectorsPerAllocationUnit(1);
    Host->SetFileInfoTimeout(1000);
    Host->SetCaseSensitiveSearch(FALSE);
    Host->SetCasePreservedNames(TRUE);
    Host->SetUnicodeOnDisk(TRUE);
    Host->SetPersistentAcls(TRUE);
    Host->SetPostCleanupWhenModifiedOnly(TRUE);
    Host->SetPassQueryDirectoryPattern(TRUE);
    Host->SetVolumeCreationTime(_CreationTime);
    Host->SetVolumeSerialNumber(0);
    Host->SetFlushAndPurgeOnCleanup(TRUE);

    return STATUS_SUCCESS;
}

NTSTATUS Blfs::GetVolumeInfo(VolumeInfo* Info)
{
    WCHAR Root[MAX_PATH];
    ULARGE_INTEGER TotalSize, FreeSize;

    if (!GetVolumePathName(_Path, Root, MAX_PATH))
        return NtStatusFromWin32(GetLastError());

    if (!GetDiskFreeSpaceEx(Root, 0, &TotalSize, &FreeSize))
        return NtStatusFromWin32(GetLastError());

    Info->TotalSize = TotalSize.QuadPart;
    Info->FreeSize = FreeSize.QuadPart;

    return STATUS_SUCCESS;
}

NTSTATUS Blfs::GetSecurityByName(
    PWSTR FileName,
    PUINT32 PFileAttributes,
    PSECURITY_DESCRIPTOR SecurityDescriptor,
    SIZE_T* PSecurityDescriptorSize)
{
    WCHAR FullPath[FULLPATH_SIZE];
    HANDLE Handle;
    FILE_ATTRIBUTE_TAG_INFO AttributeTagInfo;
    DWORD SecurityDescriptorSizeNeeded;
    NTSTATUS Result;

    if (!ConcatPath(FileName, FullPath))
        return STATUS_OBJECT_NAME_INVALID;

    Handle = CreateFileW(FullPath,
        FILE_READ_ATTRIBUTES | READ_CONTROL, 0, 0,
        OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
    if (INVALID_HANDLE_VALUE == Handle)
    {
        Result = NtStatusFromWin32(GetLastError());
        goto exit;
    }

    if (0 != PFileAttributes)
    {
        if (!GetFileInformationByHandleEx(Handle,
            FileAttributeTagInfo, &AttributeTagInfo, sizeof AttributeTagInfo))
        {
            Result = NtStatusFromWin32(GetLastError());
            goto exit;
        }

        *PFileAttributes = AttributeTagInfo.FileAttributes;
    }

    if (0 != PSecurityDescriptorSize)
    {
        if (!GetKernelObjectSecurity(Handle,
            OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
            SecurityDescriptor, (DWORD)*PSecurityDescriptorSize, &SecurityDescriptorSizeNeeded))
        {
            *PSecurityDescriptorSize = SecurityDescriptorSizeNeeded;
            Result = NtStatusFromWin32(GetLastError());
            goto exit;
        }

        *PSecurityDescriptorSize = SecurityDescriptorSizeNeeded;
    }

    Result = STATUS_SUCCESS;

exit:
    if (INVALID_HANDLE_VALUE != Handle)
        CloseHandle(Handle);

    return Result;
}

NTSTATUS Blfs::Create(
    PWSTR FileName,
    UINT32 CreateOptions,
    UINT32 GrantedAccess,
    UINT32 FileAttributes,
    PSECURITY_DESCRIPTOR SecurityDescriptor,
    UINT64 AllocationSize,
    PVOID* PFileNode,
    PVOID* PFileDesc,
    OpenFileInfo* OpenFileInfo)
{
    WCHAR FullPath[FULLPATH_SIZE];
    SECURITY_ATTRIBUTES SecurityAttributes;
    ULONG CreateFlags;
    BlfsFileDesc* FileDesc;

    if (!ConcatPath(FileName, FullPath))
        return STATUS_OBJECT_NAME_INVALID;

    FileDesc = new BlfsFileDesc;

    SecurityAttributes.nLength = sizeof SecurityAttributes;
    SecurityAttributes.lpSecurityDescriptor = SecurityDescriptor;
    SecurityAttributes.bInheritHandle = FALSE;

    CreateFlags = FILE_FLAG_BACKUP_SEMANTICS;
    if (CreateOptions & FILE_DELETE_ON_CLOSE)
        CreateFlags |= FILE_FLAG_DELETE_ON_CLOSE;

    if (CreateOptions & FILE_DIRECTORY_FILE)
    {
        CreateFlags |= FILE_FLAG_POSIX_SEMANTICS;
        FileAttributes |= FILE_ATTRIBUTE_DIRECTORY;
    }
    else
        FileAttributes &= ~FILE_ATTRIBUTE_DIRECTORY;

    if (0 == FileAttributes)
        FileAttributes = FILE_ATTRIBUTE_NORMAL;

    FileDesc->Handle = CreateFileW(FullPath,
        GrantedAccess, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, &SecurityAttributes,
        CREATE_NEW, CreateFlags | FileAttributes, 0);
    if (INVALID_HANDLE_VALUE == FileDesc->Handle)
    {
        delete FileDesc;
        return NtStatusFromWin32(GetLastError());
    }

    *PFileDesc = FileDesc;

    return GetFileInfoInternal(FileDesc->Handle, &OpenFileInfo->FileInfo);
}

NTSTATUS Blfs::Open(
    PWSTR FileName,
    UINT32 CreateOptions,
    UINT32 GrantedAccess,
    PVOID* PFileNode,
    PVOID* PFileDesc,
    OpenFileInfo* OpenFileInfo)
{
    WCHAR FullPath[FULLPATH_SIZE];
    ULONG CreateFlags;
    BlfsFileDesc* FileDesc;

    if (!ConcatPath(FileName, FullPath))
        return STATUS_OBJECT_NAME_INVALID;

    FileDesc = new BlfsFileDesc;

    CreateFlags = FILE_FLAG_BACKUP_SEMANTICS;
    if (CreateOptions & FILE_DELETE_ON_CLOSE)
        CreateFlags |= FILE_FLAG_DELETE_ON_CLOSE;

    FileDesc->Handle = CreateFileW(FullPath,
        GrantedAccess, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0,
        OPEN_EXISTING, CreateFlags, 0);
    if (INVALID_HANDLE_VALUE == FileDesc->Handle)
    {
        delete FileDesc;
        return NtStatusFromWin32(GetLastError());
    }

    *PFileDesc = FileDesc;

    return GetFileInfoInternal(FileDesc->Handle, &OpenFileInfo->FileInfo);
}

NTSTATUS Blfs::Overwrite(
    PVOID FileNode,
    PVOID FileDesc,
    UINT32 FileAttributes,
    BOOLEAN ReplaceFileAttributes,
    UINT64 AllocationSize,
    FileInfo* FileInfo)
{
    HANDLE Handle = HandleFromFileDesc(FileDesc);
    FILE_BASIC_INFO BasicInfo = { 0 };
    FILE_ALLOCATION_INFO AllocationInfo = { 0 };
    FILE_ATTRIBUTE_TAG_INFO AttributeTagInfo;

    if (ReplaceFileAttributes)
    {
        if (0 == FileAttributes)
            FileAttributes = FILE_ATTRIBUTE_NORMAL;

        BasicInfo.FileAttributes = FileAttributes;
        if (!SetFileInformationByHandle(Handle,
            FileBasicInfo, &BasicInfo, sizeof BasicInfo))
            return NtStatusFromWin32(GetLastError());
    }
    else if (0 != FileAttributes)
    {
        if (!GetFileInformationByHandleEx(Handle,
            FileAttributeTagInfo, &AttributeTagInfo, sizeof AttributeTagInfo))
            return NtStatusFromWin32(GetLastError());

        BasicInfo.FileAttributes = FileAttributes | AttributeTagInfo.FileAttributes;
        if (BasicInfo.FileAttributes ^ FileAttributes)
        {
            if (!SetFileInformationByHandle(Handle,
                FileBasicInfo, &BasicInfo, sizeof BasicInfo))
                return NtStatusFromWin32(GetLastError());
        }
    }

    if (!SetFileInformationByHandle(Handle,
        FileAllocationInfo, &AllocationInfo, sizeof AllocationInfo))
        return NtStatusFromWin32(GetLastError());

    return GetFileInfoInternal(Handle, FileInfo);
}

VOID Blfs::Cleanup(
    PVOID FileNode,
    PVOID FileDesc,
    PWSTR FileName,
    ULONG Flags)
{
    HANDLE Handle = HandleFromFileDesc(FileDesc);

    if (Flags & CleanupDelete)
    {
        CloseHandle(Handle);
        HandleFromFileDesc(FileDesc) = INVALID_HANDLE_VALUE;
    }
}

VOID Blfs::Close(
    PVOID FileNode,
    PVOID FileDesc0)
{
    BlfsFileDesc* FileDesc = (BlfsFileDesc*)FileDesc0;
    delete FileDesc;
}

NTSTATUS Blfs::Read(
    PVOID FileNode,
    PVOID FileDesc,
    PVOID Buffer,
    UINT64 Offset,
    ULONG Length,
    PULONG PBytesTransferred)
{
    HANDLE Handle = HandleFromFileDesc(FileDesc);
    LARGE_INTEGER CipherSize;

    if (!GetFileSizeEx(Handle, &CipherSize))
        return NtStatusFromWin32(GetLastError());

    UINT64 PlainSize = FsCrypto::PlainSizeFromCipherSize((UINT64)CipherSize.QuadPart);

    if (Offset >= PlainSize)
    {
        *PBytesTransferred = 0;
        return STATUS_SUCCESS;
    }
    if (Offset + Length > PlainSize)
        Length = (ULONG)(PlainSize - Offset);

    ULONG Transferred = 0;
    UINT64 CurOffset = Offset;
    PUINT8 Out = (PUINT8)Buffer;

    static thread_local uint8_t CipherBuf[FsCrypto::CipherChunkMax];
    static thread_local uint8_t PlainBuf[FsCrypto::ChunkSize];

    while (Transferred < Length)
    {
        UINT64 Idx = CurOffset / FsCrypto::ChunkSize;
        UINT32 PlainChunkLen = FsCrypto::PlainChunkLen(PlainSize, Idx);
        if (0 == PlainChunkLen)
            break;

        UINT64 CipherOff = FsCrypto::ChunkCipherOffset(Idx);
        UINT32 CipherLen = PlainChunkLen + FsCrypto::ChunkOverhead;

        OVERLAPPED Ov = { 0 };
        Ov.Offset = (DWORD)CipherOff;
        Ov.OffsetHigh = (DWORD)(CipherOff >> 32);
        DWORD Got = 0;
        if (!ReadFile(Handle, CipherBuf, CipherLen, &Got, &Ov) || Got != CipherLen)
            return NtStatusFromWin32(GetLastError());

        if (!FsCrypto::DecryptChunk(CipherBuf, CipherLen, PlainBuf))
            return STATUS_UNSUCCESSFUL;

        UINT64 ChunkPlainStart = Idx * FsCrypto::ChunkSize;
        UINT64 WithinChunk = CurOffset - ChunkPlainStart;
        UINT32 Avail = PlainChunkLen - (UINT32)WithinChunk;
        UINT32 Remaining = Length - Transferred;
        UINT32 ToCopy = Avail < Remaining ? Avail : Remaining;

        memcpy(Out + Transferred, PlainBuf + WithinChunk, ToCopy);
        Transferred += ToCopy;
        CurOffset += ToCopy;
    }

    *PBytesTransferred = Transferred;
    return STATUS_SUCCESS;
}

NTSTATUS Blfs::Write(
    PVOID FileNode,
    PVOID FileDesc,
    PVOID Buffer,
    UINT64 Offset,
    ULONG Length,
    BOOLEAN WriteToEndOfFile,
    BOOLEAN ConstrainedIo,
    PULONG PBytesTransferred,
    FileInfo* FileInfo)
{
    HANDLE Handle = HandleFromFileDesc(FileDesc);
    LARGE_INTEGER CipherSize;

    if (!GetFileSizeEx(Handle, &CipherSize))
        return NtStatusFromWin32(GetLastError());

    UINT64 PlainSize = FsCrypto::PlainSizeFromCipherSize((UINT64)CipherSize.QuadPart);

    if (WriteToEndOfFile)
        Offset = PlainSize;

    if (ConstrainedIo)
    {
        if (Offset >= PlainSize)
        {
            *PBytesTransferred = 0;
            return GetFileInfoInternal(Handle, FileInfo);
        }
        if (Offset + Length > PlainSize)
            Length = (ULONG)(PlainSize - Offset);
    }

    static thread_local uint8_t PlainBuf[FsCrypto::ChunkSize];
    static thread_local uint8_t CipherBuf[FsCrypto::CipherChunkMax];
    static thread_local uint8_t CipherOutBuf[FsCrypto::CipherChunkMax];

    // gap-fill: extend with zero chunks if Offset lands beyond current EOF non-contiguously
    if (Offset > PlainSize)
    {
        UINT64 TargetChunkIdx = Offset / FsCrypto::ChunkSize;

        if (PlainSize > 0)
        {
            UINT64 OldLastChunkIdx = (PlainSize - 1) / FsCrypto::ChunkSize;
            UINT32 OldLastLen = FsCrypto::PlainChunkLen(PlainSize, OldLastChunkIdx);

            if (OldLastLen < FsCrypto::ChunkSize && TargetChunkIdx > OldLastChunkIdx)
            {
                memset(PlainBuf, 0, FsCrypto::ChunkSize);
                UINT64 Co = FsCrypto::ChunkCipherOffset(OldLastChunkIdx);
                UINT32 Cl = OldLastLen + FsCrypto::ChunkOverhead;
                OVERLAPPED Ov = { 0 };
                Ov.Offset = (DWORD)Co;
                Ov.OffsetHigh = (DWORD)(Co >> 32);
                DWORD Got = 0;
                if (!ReadFile(Handle, CipherBuf, Cl, &Got, &Ov) || Got != Cl)
                    return NtStatusFromWin32(GetLastError());
                if (!FsCrypto::DecryptChunk(CipherBuf, Cl, PlainBuf))
                    return STATUS_UNSUCCESSFUL;
                if (!FsCrypto::EncryptChunk(PlainBuf, FsCrypto::ChunkSize, CipherOutBuf))
                    return STATUS_UNSUCCESSFUL;
                DWORD Wr = 0;
                if (!WriteFile(Handle, CipherOutBuf, FsCrypto::CipherChunkMax, &Wr, &Ov)
                    || Wr != FsCrypto::CipherChunkMax)
                    return NtStatusFromWin32(GetLastError());
            }
        }

        UINT64 StartGap = (PlainSize == 0) ? 0 : ((PlainSize - 1) / FsCrypto::ChunkSize) + 1;
        for (UINT64 G = StartGap; G < TargetChunkIdx; G++)
        {
            memset(PlainBuf, 0, FsCrypto::ChunkSize);
            if (!FsCrypto::EncryptChunk(PlainBuf, FsCrypto::ChunkSize, CipherOutBuf))
                return STATUS_UNSUCCESSFUL;
            UINT64 Co = FsCrypto::ChunkCipherOffset(G);
            OVERLAPPED Ov = { 0 };
            Ov.Offset = (DWORD)Co;
            Ov.OffsetHigh = (DWORD)(Co >> 32);
            DWORD Wr = 0;
            if (!WriteFile(Handle, CipherOutBuf, FsCrypto::CipherChunkMax, &Wr, &Ov)
                || Wr != FsCrypto::CipherChunkMax)
                return NtStatusFromWin32(GetLastError());
        }
    }

    UINT64 NewPlainSize = PlainSize;
    if (Offset + Length > NewPlainSize)
        NewPlainSize = Offset + Length;

    ULONG Transferred = 0;
    UINT64 CurOffset = Offset;
    const uint8_t* In = (const uint8_t*)Buffer;

    while (Transferred < Length)
    {
        UINT64 Idx = CurOffset / FsCrypto::ChunkSize;
        UINT64 ChunkPlainStart = Idx * FsCrypto::ChunkSize;
        UINT32 ExistingPlainLen = FsCrypto::PlainChunkLen(PlainSize, Idx);
        UINT32 NewPlainLen = FsCrypto::PlainChunkLen(NewPlainSize, Idx);

        memset(PlainBuf, 0, FsCrypto::ChunkSize);

        if (ExistingPlainLen > 0)
        {
            UINT64 Co = FsCrypto::ChunkCipherOffset(Idx);
            UINT32 Cl = ExistingPlainLen + FsCrypto::ChunkOverhead;
            OVERLAPPED Ov = { 0 };
            Ov.Offset = (DWORD)Co;
            Ov.OffsetHigh = (DWORD)(Co >> 32);
            DWORD Got = 0;
            if (!ReadFile(Handle, CipherBuf, Cl, &Got, &Ov) || Got != Cl)
                return NtStatusFromWin32(GetLastError());
            if (!FsCrypto::DecryptChunk(CipherBuf, Cl, PlainBuf))
                return STATUS_UNSUCCESSFUL;
        }

        UINT64 WithinChunk = CurOffset - ChunkPlainStart;
        UINT32 Avail = FsCrypto::ChunkSize - (UINT32)WithinChunk;
        UINT32 Remaining = Length - Transferred;
        UINT32 ToCopy = Remaining < Avail ? Remaining : Avail;
        memcpy(PlainBuf + WithinChunk, In + Transferred, ToCopy);

        UINT32 FinalPlainLen = NewPlainLen;
        if (0 == FinalPlainLen)
            FinalPlainLen = (UINT32)(WithinChunk + ToCopy);

        if (!FsCrypto::EncryptChunk(PlainBuf, FinalPlainLen, CipherOutBuf))
            return STATUS_UNSUCCESSFUL;

        UINT64 Co = FsCrypto::ChunkCipherOffset(Idx);
        UINT32 OutLen = FinalPlainLen + FsCrypto::ChunkOverhead;
        OVERLAPPED Ov2 = { 0 };
        Ov2.Offset = (DWORD)Co;
        Ov2.OffsetHigh = (DWORD)(Co >> 32);
        DWORD Written = 0;
        if (!WriteFile(Handle, CipherOutBuf, OutLen, &Written, &Ov2) || Written != OutLen)
            return NtStatusFromWin32(GetLastError());

        Transferred += ToCopy;
        CurOffset += ToCopy;
    }

    *PBytesTransferred = Transferred;
    return GetFileInfoInternal(Handle, FileInfo);
}

NTSTATUS Blfs::Flush(
    PVOID FileNode,
    PVOID FileDesc,
    FileInfo* FileInfo)
{
    HANDLE Handle = HandleFromFileDesc(FileDesc);

    if (0 == Handle)
        return STATUS_SUCCESS;

    if (!FlushFileBuffers(Handle))
        return NtStatusFromWin32(GetLastError());

    return GetFileInfoInternal(Handle, FileInfo);
}

NTSTATUS Blfs::GetFileInfo(
    PVOID FileNode,
    PVOID FileDesc,
    FileInfo* FileInfo)
{
    HANDLE Handle = HandleFromFileDesc(FileDesc);
    return GetFileInfoInternal(Handle, FileInfo);
}

NTSTATUS Blfs::SetBasicInfo(
    PVOID FileNode,
    PVOID FileDesc,
    UINT32 FileAttributes,
    UINT64 CreationTime,
    UINT64 LastAccessTime,
    UINT64 LastWriteTime,
    UINT64 ChangeTime,
    FileInfo* FileInfo)
{
    HANDLE Handle = HandleFromFileDesc(FileDesc);
    FILE_BASIC_INFO BasicInfo = { 0 };

    if (INVALID_FILE_ATTRIBUTES == FileAttributes)
        FileAttributes = 0;
    else if (0 == FileAttributes)
        FileAttributes = FILE_ATTRIBUTE_NORMAL;

    BasicInfo.FileAttributes = FileAttributes;
    BasicInfo.CreationTime.QuadPart = CreationTime;
    BasicInfo.LastAccessTime.QuadPart = LastAccessTime;
    BasicInfo.LastWriteTime.QuadPart = LastWriteTime;

    if (!SetFileInformationByHandle(Handle,
        FileBasicInfo, &BasicInfo, sizeof BasicInfo))
        return NtStatusFromWin32(GetLastError());

    return GetFileInfoInternal(Handle, FileInfo);
}

NTSTATUS Blfs::SetFileSize(
    PVOID FileNode,
    PVOID FileDesc,
    UINT64 NewSize,
    BOOLEAN SetAllocationSize,
    FileInfo* FileInfo)
{
    HANDLE Handle = HandleFromFileDesc(FileDesc);

    if (SetAllocationSize)
        return GetFileInfoInternal(Handle, FileInfo);

    LARGE_INTEGER CipherSize;
    if (!GetFileSizeEx(Handle, &CipherSize))
        return NtStatusFromWin32(GetLastError());

    UINT64 PlainSize = FsCrypto::PlainSizeFromCipherSize((UINT64)CipherSize.QuadPart);

    if (NewSize == PlainSize)
        return GetFileInfoInternal(Handle, FileInfo);

    if (NewSize < PlainSize)
    {
        static thread_local uint8_t PlainBuf[FsCrypto::ChunkSize];
        static thread_local uint8_t CipherBuf[FsCrypto::CipherChunkMax];
        static thread_local uint8_t CipherOutBuf[FsCrypto::CipherChunkMax];

        UINT64 NewCipherEOF = 0;

        if (NewSize > 0)
        {
            UINT64 NewLastChunkIdx = (NewSize - 1) / FsCrypto::ChunkSize;
            UINT32 NewLastLen = FsCrypto::PlainChunkLen(NewSize, NewLastChunkIdx);
            UINT32 OldLen = FsCrypto::PlainChunkLen(PlainSize, NewLastChunkIdx);

            memset(PlainBuf, 0, FsCrypto::ChunkSize);
            if (OldLen > 0)
            {
                UINT64 Co = FsCrypto::ChunkCipherOffset(NewLastChunkIdx);
                UINT32 Cl = OldLen + FsCrypto::ChunkOverhead;
                OVERLAPPED Ov = { 0 };
                Ov.Offset = (DWORD)Co;
                Ov.OffsetHigh = (DWORD)(Co >> 32);
                DWORD Got = 0;
                if (!ReadFile(Handle, CipherBuf, Cl, &Got, &Ov) || Got != Cl)
                    return NtStatusFromWin32(GetLastError());
                if (!FsCrypto::DecryptChunk(CipherBuf, Cl, PlainBuf))
                    return STATUS_UNSUCCESSFUL;
            }

            if (!FsCrypto::EncryptChunk(PlainBuf, NewLastLen, CipherOutBuf))
                return STATUS_UNSUCCESSFUL;

            UINT64 Co = FsCrypto::ChunkCipherOffset(NewLastChunkIdx);
            UINT32 OutLen = NewLastLen + FsCrypto::ChunkOverhead;
            OVERLAPPED Ov2 = { 0 };
            Ov2.Offset = (DWORD)Co;
            Ov2.OffsetHigh = (DWORD)(Co >> 32);
            DWORD Wr = 0;
            if (!WriteFile(Handle, CipherOutBuf, OutLen, &Wr, &Ov2) || Wr != OutLen)
                return NtStatusFromWin32(GetLastError());

            NewCipherEOF = Co + OutLen;
        }

        FILE_END_OF_FILE_INFO Eofi;
        Eofi.EndOfFile.QuadPart = (LONGLONG)NewCipherEOF;
        if (!SetFileInformationByHandle(Handle, FileEndOfFileInfo, &Eofi, sizeof Eofi))
            return NtStatusFromWin32(GetLastError());
    }
    else
    {
        UINT64 GrowBy = NewSize - PlainSize;
        UINT64 Written = 0;
        static thread_local uint8_t Zero[FsCrypto::ChunkSize];
        memset(Zero, 0, sizeof Zero);

        while (Written < GrowBy)
        {
            ULONG Chunk = (ULONG)((GrowBy - Written) > FsCrypto::ChunkSize
                ? FsCrypto::ChunkSize : (GrowBy - Written));
            ULONG Bt = 0;
            NTSTATUS St = Write(FileNode, FileDesc, Zero, PlainSize + Written, Chunk,
                FALSE, FALSE, &Bt, FileInfo);
            if (!NT_SUCCESS(St))
                return St;
            if (0 == Bt)
                break;
            Written += Bt;
        }
    }

    return GetFileInfoInternal(Handle, FileInfo);
}

NTSTATUS Blfs::CanDelete(
    PVOID FileNode,
    PVOID FileDesc,
    PWSTR FileName)
{
    HANDLE Handle = HandleFromFileDesc(FileDesc);
    FILE_DISPOSITION_INFO DispositionInfo;

    DispositionInfo.DeleteFile = TRUE;
    if (!SetFileInformationByHandle(Handle,
        FileDispositionInfo, &DispositionInfo, sizeof DispositionInfo))
        return NtStatusFromWin32(GetLastError());

    return STATUS_SUCCESS;
}

NTSTATUS Blfs::Rename(
    PVOID FileNode,
    PVOID FileDesc,
    PWSTR FileName,
    PWSTR NewFileName,
    BOOLEAN ReplaceIfExists)
{
    WCHAR FullPath[FULLPATH_SIZE], NewFullPath[FULLPATH_SIZE];

    if (!ConcatPath(FileName, FullPath))
        return STATUS_OBJECT_NAME_INVALID;

    if (!ConcatPath(NewFileName, NewFullPath))
        return STATUS_OBJECT_NAME_INVALID;

    if (!MoveFileExW(FullPath, NewFullPath, ReplaceIfExists ? MOVEFILE_REPLACE_EXISTING : 0))
        return NtStatusFromWin32(GetLastError());

    return STATUS_SUCCESS;
}

NTSTATUS Blfs::GetSecurity(
    PVOID FileNode,
    PVOID FileDesc,
    PSECURITY_DESCRIPTOR SecurityDescriptor,
    SIZE_T* PSecurityDescriptorSize)
{
    HANDLE Handle = HandleFromFileDesc(FileDesc);
    DWORD SecurityDescriptorSizeNeeded;

    if (!GetKernelObjectSecurity(Handle,
        OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
        SecurityDescriptor, (DWORD)*PSecurityDescriptorSize, &SecurityDescriptorSizeNeeded))
    {
        *PSecurityDescriptorSize = SecurityDescriptorSizeNeeded;
        return NtStatusFromWin32(GetLastError());
    }

    *PSecurityDescriptorSize = SecurityDescriptorSizeNeeded;
    return STATUS_SUCCESS;
}

NTSTATUS Blfs::SetSecurity(
    PVOID FileNode,
    PVOID FileDesc,
    SECURITY_INFORMATION SecurityInformation,
    PSECURITY_DESCRIPTOR ModificationDescriptor)
{
    HANDLE Handle = HandleFromFileDesc(FileDesc);

    if (!SetKernelObjectSecurity(Handle, SecurityInformation, ModificationDescriptor))
        return NtStatusFromWin32(GetLastError());

    return STATUS_SUCCESS;
}

NTSTATUS Blfs::ReadDirectory(
    PVOID FileNode,
    PVOID FileDesc0,
    PWSTR Pattern,
    PWSTR Marker,
    PVOID Buffer,
    ULONG Length,
    PULONG PBytesTransferred)
{
    BlfsFileDesc* FileDesc = (BlfsFileDesc*)FileDesc0;
    return BufferedReadDirectory(&FileDesc->DirBuffer,
        FileNode, FileDesc, Pattern, Marker, Buffer, Length, PBytesTransferred);
}

NTSTATUS Blfs::ReadDirectoryEntry(
    PVOID FileNode,
    PVOID FileDesc0,
    PWSTR Pattern,
    PWSTR Marker,
    PVOID* PContext,
    DirInfo* DirInfo)
{
    BlfsFileDesc* FileDesc = (BlfsFileDesc*)FileDesc0;
    HANDLE Handle = FileDesc->Handle;
    WCHAR FullPath[FULLPATH_SIZE];
    ULONG Length, PatternLength;
    HANDLE FindHandle;
    WIN32_FIND_DATAW FindData;

    if (0 == *PContext)
    {
        if (0 == Pattern)
            Pattern = L"*";

        PatternLength = (ULONG)wcslen(Pattern);
        Length = GetFinalPathNameByHandleW(Handle, FullPath, FULLPATH_SIZE - 1, 0);
        if (0 == Length)
            return NtStatusFromWin32(GetLastError());

        if (Length + 1 + PatternLength >= FULLPATH_SIZE)
            return STATUS_OBJECT_NAME_INVALID;

        if (L'\\' != FullPath[Length - 1])
            FullPath[Length++] = L'\\';
        memcpy(FullPath + Length, Pattern, PatternLength * sizeof(WCHAR));
        FullPath[Length + PatternLength] = L'\0';

        FindHandle = FindFirstFileW(FullPath, &FindData);
        if (INVALID_HANDLE_VALUE == FindHandle)
            return STATUS_NO_MORE_FILES;

        *PContext = FindHandle;
    }
    else
    {
        FindHandle = *PContext;
        if (!FindNextFileW(FindHandle, &FindData))
        {
            FindClose(FindHandle);
            return STATUS_NO_MORE_FILES;
        }
    }

    memset(DirInfo, 0, sizeof * DirInfo);

    Length = (ULONG)wcslen(FindData.cFileName);
    DirInfo->Size = (UINT16)(FIELD_OFFSET(Blfs::DirInfo, FileNameBuf) + Length * sizeof(WCHAR));
    DirInfo->FileInfo.FileAttributes = FindData.dwFileAttributes;
    DirInfo->FileInfo.ReparseTag = 0;

    UINT64 CipherSize =
        ((UINT64)FindData.nFileSizeHigh << 32) | (UINT64)FindData.nFileSizeLow;
    if (FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        DirInfo->FileInfo.FileSize = CipherSize;
    else
        DirInfo->FileInfo.FileSize = FsCrypto::PlainSizeFromCipherSize(CipherSize);
    DirInfo->FileInfo.AllocationSize = (DirInfo->FileInfo.FileSize + ALLOCATION_UNIT - 1)
        / ALLOCATION_UNIT * ALLOCATION_UNIT;
    DirInfo->FileInfo.CreationTime = ((PLARGE_INTEGER)&FindData.ftCreationTime)->QuadPart;
    DirInfo->FileInfo.LastAccessTime = ((PLARGE_INTEGER)&FindData.ftLastAccessTime)->QuadPart;
    DirInfo->FileInfo.LastWriteTime = ((PLARGE_INTEGER)&FindData.ftLastWriteTime)->QuadPart;
    DirInfo->FileInfo.ChangeTime = DirInfo->FileInfo.LastWriteTime;
    DirInfo->FileInfo.IndexNumber = 0;
    DirInfo->FileInfo.HardLinks = 0;
    memcpy(DirInfo->FileNameBuf, FindData.cFileName, Length * sizeof(WCHAR));

    return STATUS_SUCCESS;
}

class BlfsService : public Service
{
public:
    BlfsService();

protected:
    NTSTATUS OnStart(ULONG Argc, PWSTR* Argv);
    NTSTATUS OnStop();

private:
    Blfs _Blfs;
    FileSystemHost _Host;
};

static NTSTATUS EnableBackupRestorePrivileges(VOID)
{
    union
    {
        TOKEN_PRIVILEGES P;
        UINT8 B[sizeof(TOKEN_PRIVILEGES) + sizeof(LUID_AND_ATTRIBUTES)];
    } Privileges;
    HANDLE Token;

    Privileges.P.PrivilegeCount = 2;
    Privileges.P.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    Privileges.P.Privileges[1].Attributes = SE_PRIVILEGE_ENABLED;
    if (!LookupPrivilegeValueW(0, SE_BACKUP_NAME, &Privileges.P.Privileges[0].Luid) ||
        !LookupPrivilegeValueW(0, SE_RESTORE_NAME, &Privileges.P.Privileges[1].Luid))
        return FspNtStatusFromWin32(GetLastError());

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &Token))
        return FspNtStatusFromWin32(GetLastError());

    if (!AdjustTokenPrivileges(Token, FALSE, &Privileges.P, 0, 0, 0))
    {
        CloseHandle(Token);
        return FspNtStatusFromWin32(GetLastError());
    }

    CloseHandle(Token);

    return STATUS_SUCCESS;
}

static ULONG wcstol_deflt(wchar_t* w, ULONG deflt)
{
    wchar_t* endp;
    ULONG ul = wcstol(w, &endp, 0);
    return L'\0' != w[0] && L'\0' == *endp ? ul : deflt;
}

BlfsService::BlfsService() : Service(L"" PROGNAME), _Blfs(), _Host(_Blfs)
{
}

NTSTATUS BlfsService::OnStart(ULONG argc, PWSTR* argv)
{
#define argtos(v) if (arge > ++argp) v = *argp; else goto usage
#define argtol(v) if (arge > ++argp) v = wcstol_deflt(*argp, v); else goto usage

    wchar_t** argp, ** arge;
    PWSTR DebugLogFile = 0;
    ULONG DebugFlags = 0;
    PWSTR VolumePrefix = 0;
    PWSTR PassThrough = 0;
    PWSTR MountPoint = 0;
    PWSTR Key = 0;
    NTSTATUS Result;

    for (argp = argv + 1, arge = argv + argc; arge > argp; argp++)
    {
        if (L'-' != argp[0][0])
            break;

        switch (argp[0][1])
        {
        case L'?':
            goto usage;
        case L'd':
            argtol(DebugFlags);
            break;
        case L'D':
            argtos(DebugLogFile);
            break;
        case L'k':
            argtos(Key);
            break;
        case L'm':
            argtos(MountPoint);
            break;
        case L'p':
            argtos(PassThrough);
            break;
        case L'u':
            argtos(VolumePrefix);
            break;
        default:
            goto usage;
        }
    }

    if (arge > argp)
        goto usage;

    if (0 == PassThrough || 0 == MountPoint || 0 == Key)
        goto usage;

    FsCrypto::SetKeyFromPassword(Key);

    EnableBackupRestorePrivileges();

    if (0 != DebugLogFile)
    {
        Result = FileSystemHost::SetDebugLogFile(DebugLogFile);
        if (!NT_SUCCESS(Result))
        {
            fail(L"cannot open debug log file");
            goto usage;
        }
    }

    Result = _Blfs.SetPath(PassThrough);
    if (!NT_SUCCESS(Result))
    {
        fail(L"cannot create file system");
        return Result;
    }

    _Host.SetPrefix(VolumePrefix);
    Result = _Host.Mount(MountPoint, 0, FALSE, DebugFlags);
    if (!NT_SUCCESS(Result))
    {
        fail(L"cannot mount file system");
        return Result;
    }

    MountPoint = _Host.MountPoint();
    info(L"%s -p %s -m %s",
        L"" PROGNAME,
        PassThrough,
        MountPoint);

    return STATUS_SUCCESS;

usage:
    static wchar_t usage[] = L""
        "usage: " PROGNAME " OPTIONS\n"
        "\n"
        "options:\n"
        "    -d DebugFlags       [-1: enable all debug logs]\n"
        "    -D DebugLogFile     [file path; use - for stderr]\n"
        "    -k Password         [encryption password]\n"
        "    -p Directory        [directory to expose as a drive]\n"
        "    -m MountPoint       [X: or *: for first available drive letter]\n";

    fail(usage);

    return STATUS_UNSUCCESSFUL;
}

NTSTATUS BlfsService::OnStop()
{
    _Host.Unmount();
    return STATUS_SUCCESS;
}

int wmain(int argc, wchar_t** argv)
{
    if (!NT_SUCCESS(Fsp::Initialize()))
        return ERROR_DELAY_LOAD_FAILED;

    BlfsService Service;
    return Service.Run();
}