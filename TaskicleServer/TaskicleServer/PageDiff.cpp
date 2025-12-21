#include "pch.h"
#include "PageDiff.h"
#include "Database.h"


/*
res\page
    <page_id>
        content.txt     当前版本
        draft.txt       当前草稿
        s<ver_id>.txt   快照，指定版本id的全文内容
*/

// 超出最大编辑次数时生成一个快照
constexpr static int DiffMaxEditCount = 800;

// WARNING 不要使用除构造、compose、getSes之外的任何函数
using TDtlDiff = dtl::Diff<BYTE, std::span<const BYTE>>;

// 序列化的最短编辑距离(SES)结构
// 以下述结构开头，后跟若干编辑动作

constexpr BYTE DiffHdrMagic_1 = '1';
struct DIFF_SES_HDR
{
    BYTE Magic;
};
// 不要修改枚举值
enum class DiffEdit : BYTE
{
    Delete, // 后跟USHORT，指示删除长度
    Common, // 后跟USHORT，指示跳过长度
    Add,    // 后跟USHORT，指示新增序列的长度，后跟新增序列

    Invalid = 0xFF,// 结尾标记
};

// 快照文件名，s<int>.txt
struct DIFF_SNAPSHOT_NAME
{
    WCHAR sz[eck::TcsCvtCalcBufferSize<int>() + 6];
};

EckInlineNdCe DiffEdit DiffpDtlEditToDiffEdit(int i) noexcept
{
    EckAssert(i == dtl::SES_DELETE || i == dtl::SES_COMMON || i == dtl::SES_ADD);
    return DiffEdit(i + 1);
}

// 序列化压缩SES，返回编辑次数
static int DiffpSesSerialize(const TDtlDiff& Diff, eck::CRefBin& rb) noexcept
{
    rb.Clear();
    const auto& Ses = Diff.getSes();
    if (Ses.isOnlyCopy())
        return 0;// 与上一版本比未编辑
    const auto& vSeq = Ses.getSequence();

    rb.Reserve(rb.Size() + sizeof(DIFF_SES_HDR) +
        vSeq.size() * sizeof(BYTE));
    const auto pHdr = rb.PushBack<DIFF_SES_HDR>();
    pHdr->Magic = DiffHdrMagic_1;

    size_t posCount{};
    USHORT* pCount{};
    DiffEdit eLastEdit{ DiffEdit::Invalid };
    int cEdit{};
    for (const auto& e : vSeq)
    {
        const auto eEdit = DiffpDtlEditToDiffEdit(e.second.type);
        if (eEdit != eLastEdit)
        {
            eLastEdit = eEdit;
            rb.PushBackByte((BYTE)eEdit);
            posCount = rb.Size();
            pCount = rb.PushBack<USHORT>();
            *pCount = 0;
        }

#pragma warning(suppress:26813)// 使用按位与
        if (eEdit == DiffEdit::Add)
        {
            ++(*pCount);
            rb.PushBackByte(e.first);
            pCount = (USHORT*)(rb.Data() + posCount);
            // 如果当前编辑操作长度超过USHORT最大值，另起一个
            if (*pCount == 0xFFFF)
                eLastEdit = DiffEdit::Invalid;
        }
        else
        {
#pragma warning(suppress:6011)// 解引用NULL
            if (++(*pCount) == 0xFFFF)
                eLastEdit = DiffEdit::Invalid;
        }

        if (eEdit != DiffEdit::Common)
            ++cEdit;
    }
    rb.PushBackByte((BYTE)DiffEdit::Invalid);// 结束标记
    return cEdit;
}

// 给定原文，使用压缩SES编辑，得到修改后内容
// 若格式验证错误，返回FALSE，格式验证仅对头执行，后续格式必须良好
static BOOL DiffpSesPatch(
    const eck::CRefBin& rbLastContent,
    const eck::CRefBin& rbSes,
    eck::CRefBin& rbNewContent) noexcept
{
    if (rbSes.Size() < sizeof(DIFF_SES_HDR))
        return FALSE;
    const auto pHdr = (DIFF_SES_HDR*)rbSes.Data();
    if (pHdr->Magic != DiffHdrMagic_1)
        return FALSE;
    rbNewContent = rbLastContent;
    if (rbSes.Size() == sizeof(DIFF_SES_HDR))
        return TRUE;// 空SES

    size_t posLast{};
    for (auto p = rbSes.Data() + sizeof(DIFF_SES_HDR); p < rbSes.Data() + rbSes.Size(); )
    {
        const auto eEdit = (DiffEdit)*p++;
        const auto Count = *(USHORT*)p;
        p += sizeof(USHORT);
        switch (eEdit)
        {
        case DiffEdit::Delete:
            rbNewContent.Erase(posLast, Count);
            break;
        case DiffEdit::Common:
            posLast += Count;
            break;
        case DiffEdit::Add:
            rbNewContent.Insert(posLast, p, Count);
            p += Count;
            posLast += Count;
            break;
        case DiffEdit::Invalid:// 终止标记
            return TRUE;
        default: ECK_UNREACHABLE;
        }
    }
    return TRUE;
}

// 取指定版本的快照文件名
// 返回文件名的视图，保证以0结尾
static std::wstring_view DiffpMakeSnapshotFileName(
    int iVerId,
    _Out_ DIFF_SNAPSHOT_NAME& Name) noexcept
{
    PWCH p = Name.sz;
    *p++ = 's';
    eck::TcsFromInt(p, ARRAYSIZE(Name.sz) - 1, iVerId, 10, FALSE, &p);
    constexpr WCHAR szExt[]{ L".txt" };
    EckCopyConstStringW(p, szExt);
    p += (ARRAYSIZE(szExt) - 1);
    *p = L'\0';
    return { Name.sz,p };
}

NTSTATUS DiffSaveFile(
    CNtTransaction& Tx,
    _In_ HANDLE hDirParent,
    std::wstring_view svFileName,
    const eck::CRefBin& rbContent) noexcept
{
    NTSTATUS r;

    eck::CFile File{};
    Tx.BeginCreateFile();
    r = File.CreateRelative(hDirParent,
        svFileName,
        FILE_OVERWRITE_IF,
        FILE_GENERIC_WRITE,
        FILE_SHARE_READ,
        FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT | FILE_SEQUENTIAL_ONLY,
        FILE_ATTRIBUTE_NORMAL);
    Tx.EndCreateFile();
    if (!NT_SUCCESS(r))
        return r;

    File.Write(rbContent.Data(), (DWORD)rbContent.Size(), nullptr, &r);
    return r;
}

NTSTATUS DiffLoadFile(
    CNtTransaction& Tx,
    _In_ HANDLE hDirParent,
    std::wstring_view svFileName,
    eck::CRefBin& rbContent) noexcept
{
    NTSTATUS r;
    rbContent.Clear();

    eck::CFile File{};
    Tx.BeginCreateFile(TRUE);
    r = File.CreateRelative(hDirParent,
        svFileName,
        FILE_OPEN,
        FILE_GENERIC_READ,
        FILE_SHARE_READ,
        FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT | FILE_SEQUENTIAL_ONLY);
    Tx.EndCreateFile();
    if (!NT_SUCCESS(r))
        return r;

    const auto cbFile = (size_t)File.GetSize(&r);
    if (!NT_SUCCESS(r))
        return r;

    rbContent.ReSize(cbFile);
    DWORD cbRead;
    File.Read(rbContent.Data(), (DWORD)cbFile, &cbRead, &r);
    if (!NT_SUCCESS(r))
        return r;
    if (cbRead != cbFile)
        return STATUS_UNSUCCESSFUL;

    return STATUS_SUCCESS;
}

NTSTATUS DiffDbGetVersionContent(
    _In_ sqlite3* pSqlite,
    CNtTransaction& TxFile,
    _In_ HANDLE hDirPage,
    int iPageId,
    int iVerId,
    eck::CRefBin& rbContent,
    _Out_ int& rSql) noexcept
{
    rbContent.Clear();

    constexpr char Sql[]{ R"(
SELECT last_ver_id, has_snapshot, diff FROM PageVersion 
WHERE page_id = ? AND ver_id = ?
)" };
    sqlite3_stmt* pStmt;
    rSql = sqlite3_prepare_v3(pSqlite, EckStrAndLen(Sql), 0, &pStmt, 0);
    if (rSql != SQLITE_OK)
        return STATUS_UNSUCCESSFUL;
    sqlite3_bind_int(pStmt, 2, iVerId);
    sqlite3_bind_int(pStmt, 1, iPageId);

    if ((rSql = sqlite3_step(pStmt)) == SQLITE_ROW)
    {
        rSql = SQLITE_OK;
        const auto iLastVerId = sqlite3_column_int(pStmt, 0);
        if (sqlite3_column_int(pStmt, 1))// 存在快照
        {
            sqlite3_finalize(pStmt);
            DIFF_SNAPSHOT_NAME Name;
            return DiffLoadFile(
                TxFile, hDirPage,
                DiffpMakeSnapshotFileName(iVerId, Name),
                rbContent);
        }
        eck::CRefBin rbSes{}, rbLastContent{};
        rbSes.ReSize((size_t)sqlite3_column_bytes(pStmt, 2));
        memcpy(rbSes.Data(), sqlite3_column_blob(pStmt, 2), rbSes.Size());
        sqlite3_finalize(pStmt);

        const auto nts = DiffDbGetVersionContent(pSqlite, TxFile, hDirPage,
            iPageId, iLastVerId, rbLastContent, rSql);
        if (!NT_SUCCESS(nts))
            return nts;

        if (!DiffpSesPatch(rbLastContent, rbSes, rbContent))
            return NTSTATUS_FROM_WIN32(ERROR_BAD_FORMAT);
        return STATUS_SUCCESS;
    }
    else
    {
        sqlite3_finalize(pStmt);
        return STATUS_NOT_FOUND;
    }
}

/// <summary>
/// 查询最新版本信息
/// </summary>
/// <param name="pSqlite">连接</param>
/// <param name="iPageId">文章ID</param>
/// <param name="iVerId">返回版本ID，失败返回DbIdInvalid，无版本返回DbPvIdVersionLatest</param>
/// <param name="cEdit">返回从最近的快照到当前版本的编辑次数</param>
/// <returns>sqlite错误码</returns>
static int DiffpDbQueryLatestVersion(
    _In_ sqlite3* pSqlite,
    int iPageId,
    _Out_ int& iVerId,
    _Out_ int& cEdit) noexcept
{
    constexpr char Sql[]{ R"(
SELECT ver_id, edit_count FROM PageVersion
WHERE page_id = ?
ORDER BY ver_id DESC
LIMIT 1
)" };
    cEdit = 0;

    sqlite3_stmt* pStmt;
    auto r = sqlite3_prepare_v3(pSqlite,
        EckStrAndLen(Sql), 0, &pStmt, 0);
    if (r != SQLITE_OK)
    {
        iVerId = DbIdInvalid;
        return r;
    }
    sqlite3_bind_int(pStmt, 1, iPageId);

    if ((r = sqlite3_step(pStmt)) == SQLITE_ROW)
    {
        iVerId = sqlite3_column_int(pStmt, 0);
        cEdit = sqlite3_column_int(pStmt, 1);
        sqlite3_finalize(pStmt);
        return SQLITE_OK;
    }
    else
    {
        iVerId = DbPvIdVersionLatest;
        sqlite3_finalize(pStmt);
        return SQLITE_OK;
    }
}

NTSTATUS DiffDbCreateVersion(
    _In_ sqlite3* pSqlite,
    CNtTransaction& TxFile,
    _In_ HANDLE hDirPage,
    int iPageId,
    int iUserId,
    const eck::CRefBin& rbContent,
    _Out_ int& rSql) noexcept
{
    NTSTATUS nts;

    int iLastVerId;
    int cEdit;
    rSql = DiffpDbQueryLatestVersion(pSqlite, iPageId, iLastVerId, cEdit);
    if (rSql != SQLITE_OK)
        return STATUS_UNSUCCESSFUL;

    const auto bNoVersion = (iLastVerId == DbPvIdVersionLatest);
    eck::CRefBin rbLastContent{};
    if (!bNoVersion)
    {
        nts = DiffLoadFile(TxFile, hDirPage, L"content.txt"sv, rbLastContent);
        if (!NT_SUCCESS(nts))
            return nts;
    }

    BOOL bCreateSnapshot;
    eck::CRefBin rbSes{};
    if (bNoVersion)// 首次创建版本
        bCreateSnapshot = TRUE;
    else
    {
        TDtlDiff Diff{ rbLastContent.ToSpan(), rbContent.ToSpan() };
        Diff.compose();
        const auto cNewEdit = DiffpSesSerialize(Diff, rbSes);
        if (!cNewEdit)
            return STATUS_ABANDONED;
        cEdit += cNewEdit;
        bCreateSnapshot = (cEdit > DiffMaxEditCount);
    }

    constexpr char Sql[]{ R"(
INSERT INTO PageVersion (page_id, user_id, last_ver_id, has_snapshot, diff, edit_count)
VALUES (?, ?, ?, ?, ?, ?);
)" };
    sqlite3_stmt* pStmt;
    rSql = sqlite3_prepare_v3(pSqlite,
        EckStrAndLen(Sql), 0, &pStmt, nullptr);
    if (rSql != SQLITE_OK)
        return STATUS_UNSUCCESSFUL;
    sqlite3_bind_int(pStmt, 1, iPageId);
    sqlite3_bind_int(pStmt, 2, iUserId);
    sqlite3_bind_int(pStmt, 3, iLastVerId);
    sqlite3_bind_int(pStmt, 4, bCreateSnapshot);
    if (bNoVersion)
    {
        sqlite3_bind_null(pStmt, 5);
        sqlite3_bind_int(pStmt, 6, 0);
    }
    else
    {
        sqlite3_bind_blob(pStmt, 5, rbSes.Data(),
            (int)rbSes.Size(), SQLITE_STATIC);
        sqlite3_bind_int(pStmt, 6, cEdit);
    }

    rSql = sqlite3_step(pStmt);
    sqlite3_finalize(pStmt);
    if (rSql != SQLITE_DONE)
        return STATUS_UNSUCCESSFUL;
    rSql = SQLITE_OK;

    nts = DiffSaveFile(TxFile, hDirPage, L"content.txt"sv, rbContent);
    if (!NT_SUCCESS(nts))
        return nts;
    if (bCreateSnapshot)
    {
        const auto iNewVerId = (int)sqlite3_last_insert_rowid(pSqlite);
        DIFF_SNAPSHOT_NAME Name;
        return DiffSaveFile(
            TxFile, hDirPage,
            DiffpMakeSnapshotFileName(iNewVerId, Name),
            rbContent);
    }
    return STATUS_SUCCESS;
}

BOOL CNtTransaction::BeginCreateFile(BOOL bReadOnly) noexcept
{
    if (!IsValid())
        return FALSE;
    // IoCreateFile对象名称解析时调用tm!TmCurrentTransactionExt
    // 函数读取Teb->CurrTxHnd，Rtl(G/S)etCurrentTransaction是此字段的包装器
    if (RtlGetCurrentTransaction())
        return FALSE;
    if (RtlSetCurrentTransaction(Get()))
    {
        if (!bReadOnly)
        {
            m_uLastTxFsCtx = NtCurrentTeb()->TxFsContext;
            NtCurrentTeb()->TxFsContext = TXFS_MINIVERSION_DIRTY_VIEW;
        }
        m_bReadOnly = bReadOnly;
#ifdef _DEBUG
        m_bCreated = TRUE;
#endif
        return TRUE;
    }
    else
        return FALSE;
}

void CNtTransaction::EndCreateFile() noexcept
{
#ifdef _DEBUG
    EckAssert(m_bCreated);
    m_bCreated = FALSE;
#endif
    if (!m_bReadOnly)
        NtCurrentTeb()->TxFsContext = m_uLastTxFsCtx;
    RtlSetCurrentTransaction(nullptr);
}