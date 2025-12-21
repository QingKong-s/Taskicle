#pragma once

class CNtTransaction : public eck::CNtObject
{
private:
    ULONG m_uLastTxFsCtx{};
    BOOL m_bReadOnly{};
#ifdef _DEBUG
    BOOL m_bCreated{};
#endif
public:
#ifdef _DEBUG
    ~CNtTransaction()
    {
        EckAssert(!m_bCreated);
    }
#endif

    NTSTATUS Create() noexcept
    {
        return NtCreateTransaction(&m_hObject, TRANSACTION_ALL_ACCESS,
            nullptr, nullptr, nullptr, 0, 0, 0, nullptr, nullptr);
    }

    BOOL BeginCreateFile(BOOL bReadOnly = FALSE) noexcept;
    void EndCreateFile() noexcept;

    NTSTATUS Commit() noexcept { return NtCommitTransaction(m_hObject, TRUE); }
    NTSTATUS Rollback() noexcept { return NtRollbackTransaction(m_hObject, TRUE); }
};

// WARNING 必须在事务中调用
NTSTATUS DiffDbCreateVersion(
    _In_ sqlite3* pSqlite,
    CNtTransaction& TxFile,
    _In_ HANDLE hDirPage,
    int iPageId,
    int iUserId,
    const eck::CRefBin& rbContent,
    _Out_ int& rSql) noexcept;

// WARNING 必须在事务中调用
// 版本ID不能为特殊ID，如DbPvIdVersionLatest
NTSTATUS DiffDbGetVersionContent(
    _In_ sqlite3* pSqlite,
    CNtTransaction& TxFile,
    _In_ HANDLE hDirPage,
    int iPageId,
    int iVerId,
    eck::CRefBin& rbContent,
    _Out_ int& rSql) noexcept;

NTSTATUS DiffLoadFile(
    CNtTransaction& Tx,
    _In_ HANDLE hDirParent,
    std::wstring_view svFileName,
    eck::CRefBin& rbContent) noexcept;

NTSTATUS DiffSaveFile(
    CNtTransaction& Tx,
    _In_ HANDLE hDirParent,
    std::wstring_view svFileName,
    const eck::CRefBin& rbContent) noexcept;