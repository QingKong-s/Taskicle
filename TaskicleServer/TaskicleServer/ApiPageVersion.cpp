#include "pch.h"
#include "ServerApi.h"
#include "ApiPriv.h"
#include "Database.h"
#include "AccessCheck.h"
#include "SqliteUtils.h"
#include "PageDiff.h"


constexpr static UINT PrhMagic = 0xDEADBEEF;

struct PAGE_REQ_HEADER
{
    UINT Magic;         // PrhMagic
    int iPageId;
    BOOL bTemp;         // TRUE = 草稿
    BOOL bCompressed;   // TRUE = gzip压缩
    DbPageType eType;
    UINT crc32;
    UINT cbContent;     // 不含本结构
    ApiResult r;
    UINT r2;
    // BYTE byContent[];
};

EckInlineNdCe BOOL PrhCheckCrc32(const PAGE_REQ_HEADER* pHdr) noexcept
{
    if (pHdr->cbContent)
        return eck::CalcCrc32(eck::PCBYTE(pHdr + 1), pHdr->cbContent) == pHdr->crc32;
    else
        return TRUE;
}
static BOOL PrhCheck(const eck::CRefBin& rbBody,
    _Out_ const PAGE_REQ_HEADER*& pHdr) noexcept
{
    pHdr = (const PAGE_REQ_HEADER*)rbBody.Data();
    if (rbBody.Size() < sizeof(PAGE_REQ_HEADER) ||
        pHdr->Magic != PrhMagic ||
        pHdr->iPageId < 0 ||
        (pHdr->bTemp != 0 && pHdr->bTemp != 1) ||
        !ApiEnumInRange(pHdr->eType, DbPageType::Min, DbPageType::Max) ||
        rbBody.Size() < pHdr->cbContent + sizeof(PAGE_REQ_HEADER) ||
        !PrhCheckCrc32(pHdr))
        return FALSE;
    return TRUE;
}

static int PrhCompressContent(eck::CRefBin& rbBody, const eck::CRefBin& rbContent) noexcept
{
    auto pHdr = (PAGE_REQ_HEADER*)rbBody.Data();
    if (rbContent.Size() < 512)
    {
        pHdr->bCompressed = FALSE;
        pHdr->cbContent = (UINT)rbContent.Size();
        if (rbContent.IsEmpty())
            pHdr->crc32 = 0;
        else
        {
            rbBody.PushBack(rbContent);
            pHdr = (PAGE_REQ_HEADER*)rbBody.Data();
            pHdr->crc32 = eck::CalcCrc32(eck::PCBYTE(pHdr + 1), pHdr->cbContent);
        }
    }
    else
    {
        eck::CRefBin rbCompressed{};
        const auto r = eck::GZipCompress(rbContent.Data(), rbContent.Size(), rbCompressed);
        if (!eck::ZLibSuccess(r))
            return r;
        rbBody.PushBack(rbCompressed);
        pHdr = (PAGE_REQ_HEADER*)rbBody.Data();
        pHdr->bCompressed = TRUE;
        pHdr->cbContent = (UINT)rbCompressed.Size();
        pHdr->crc32 = eck::CalcCrc32(eck::PCBYTE(pHdr + 1), pHdr->cbContent);
    }
    return Z_OK;
}

static int PrhDecompressContent(const eck::CRefBin& rbBody, eck::CRefBin& rbContent) noexcept
{
    const auto pHdr = (const PAGE_REQ_HEADER*)rbBody.Data();
    if (!pHdr->cbContent)
    {
        rbContent.Clear();
        return Z_OK;
    }

    if (pHdr->bCompressed)
    {
        rbContent.Clear();
        const auto r = eck::GZipDecompress(pHdr + 1, pHdr->cbContent, rbContent);
        if (!eck::ZLibSuccess(r))
            return r;
    }
    else
        rbContent.DupStream(eck::PCBYTE(pHdr + 1), pHdr->cbContent);
    return Z_OK;
}


static BOOL PageDbExists(sqlite3* pSqlite, int iPageId, _Out_ int& r) noexcept
{
    constexpr char Sql[]{ R"(SELECT COUNT(*) FROM Page WHERE page_id = ?)" };
    sqlite3_stmt* pStmt;
    r = sqlite3_prepare_v3(pSqlite, EckStrAndLen(Sql), 0, &pStmt, nullptr);
    if (r != SQLITE_OK)
        return FALSE;
    sqlite3_bind_int(pStmt, 1, iPageId);
    r = sqlite3_step(pStmt);
    if (r != SQLITE_ROW)
        return FALSE;
    r = SQLITE_OK;
    const auto b = !!sqlite3_column_int(pStmt, 0);
    sqlite3_finalize(pStmt);
    return b;
}

static int PageDbMarkDraft(sqlite3* pSqlite, int iPageId, BOOL bDraft) noexcept
{
    int r;
    constexpr char Sql[]{ R"(UPDATE Page SET has_draft = ? WHERE page_id = ?)" };
    sqlite3_stmt* pStmt;
    r = sqlite3_prepare_v3(pSqlite, EckStrAndLen(Sql), 0, &pStmt, nullptr);
    if (r != SQLITE_OK)
        return r;
    sqlite3_bind_int(pStmt, 1, !!bDraft);
    sqlite3_bind_int(pStmt, 2, iPageId);
    r = sqlite3_step(pStmt);
    if (r == SQLITE_DONE)
        r = SQLITE_OK;
    sqlite3_finalize(pStmt);
    return r;
}

static BOOL PageDbDraftExists(sqlite3* pSqlite, int iPageId, _Out_ int& r) noexcept
{
    constexpr char Sql[]{ R"(SELECT COUNT(*) FROM Page WHERE page_id = ? AND has_draft = 1)" };
    sqlite3_stmt* pStmt;
    r = sqlite3_prepare_v3(pSqlite, EckStrAndLen(Sql), 0, &pStmt, nullptr);
    if (r != SQLITE_OK)
        return FALSE;
    sqlite3_bind_int(pStmt, 1, iPageId);
    r = sqlite3_step(pStmt);
    if (r != SQLITE_ROW)
        return FALSE;
    r = SQLITE_OK;
    const auto b = !!sqlite3_column_int(pStmt, 0);
    sqlite3_finalize(pStmt);
    return b;
}


// Page: WriteContent
static void AwSavePage(const API_CTX& Ctx) noexcept
{
    ApiResult rApi{ ApiResult::Ok };
    NTSTATUS r{};
    PCSTR pszErrMsg{};

    const auto& rbBody = Ctx.pExtra->rbBody;
    const PAGE_REQ_HEADER* pHdr;
    if (!PrhCheck(rbBody, pHdr))
    {
        rApi = ApiResult::BadPayload;
        goto Exit;
    }
    else
    {
        int rTmp;

        const auto iUserId = CkDbGetCurrentUser(Ctx);
        if (!AclDbCheckAccess(Ctx, iUserId,
            pHdr->iPageId, DbAccess::WriteContent, rTmp))
        {
            rApi = ApiResult::AccessDenied;
            r = (NTSTATUS)rTmp;
            goto Exit;
        }
        // 如果此页面不存在，视为无效请求
        if (!PageDbExists(Ctx.pExtra->pSqlite, pHdr->iPageId, rTmp))
        {
            rApi = ApiResult::NotFound;
            r = (NTSTATUS)rTmp;
            goto Exit;
        }
        // 解压
        eck::CRefBin rbContent{};
        rTmp = PrhDecompressContent(rbBody, rbContent);
        if (!eck::ZLibSuccess(rTmp))
        {
            rApi = ApiResult::BadPayload;
            r = (NTSTATUS)rTmp;
            pszErrMsg = "GZipDecompress failed";
            goto Exit;
        }
        // 打开文章目录
        eck::CRefStrW rsSubDir{ EckStrAndLen(L"res\\page\\") };
        rsSubDir.AppendFormat(L"%d", pHdr->iPageId);

        eck::CNtObject Dir{};
        {
            HANDLE hDir;
            r = eck::FileEnsureDirectoryExist(
                eck::GetRunningPath().Data(),
                rsSubDir.ToStringView(), &hDir);
            if (!NT_SUCCESS(r))
            {
                rApi = ApiResult::File;
                pszErrMsg = "FileEnsureDirectoryExist failed";
                goto Exit;
            }
            Dir.Attach(hDir);
        }
        // 事务准备
        CNtTransaction TxFile{};
        r = TxFile.Create();
        if (!NT_SUCCESS(r))
        {
            rApi = ApiResult::File;
            pszErrMsg = "CNtTransaction::Create failed";
            goto Exit;
        }
        CSqliteTransaction TxDbMain{ Ctx.pExtra->pSqlite };
        CSqliteTransaction TxDb{ Ctx.pExtra->pSqlitePv,TRUE };

        if (pHdr->bTemp)// 保存草稿
        {
            r = DiffSaveFile(TxFile, Dir.Get(), L"draft.txt"sv, rbContent);
            if (!NT_SUCCESS(r))
            {
                rApi = ApiResult::File;
                pszErrMsg = "DiffSaveFile failed";
                goto Exit;
            }
            rTmp = PageDbMarkDraft(Ctx.pExtra->pSqlite, pHdr->iPageId, TRUE);
            if (rTmp != SQLITE_OK)
            {
                rApi = ApiResult::Database;
                pszErrMsg = sqlite3_errmsg(Ctx.pExtra->pSqlite);
                goto Exit;
            }
        }
        else// 创建一个版本
        {
            r = DiffDbCreateVersion(Ctx.pExtra->pSqlitePv, TxFile,
                Dir.Get(), pHdr->iPageId, iUserId, rbContent, rTmp);
            if (!NT_SUCCESS(r))
            {
                if (rTmp == SQLITE_OK)
                {
                    rApi = ApiResult::Unknown;
                    pszErrMsg = "DiffDbCreateVersion failed";
                }
                else
                {
                    rApi = ApiResult::Database;
                    pszErrMsg = sqlite3_errmsg(Ctx.pExtra->pSqlitePv);
                }
                goto Exit;
            }
            // 保存后删除草稿
            if (r != STATUS_ABANDONED)// 返回此值表示请求保存的内容与最新版本内容一致
            {
                rTmp = PageDbMarkDraft(Ctx.pExtra->pSqlite, pHdr->iPageId, FALSE);
                if (rTmp != SQLITE_OK)
                {
                    rApi = ApiResult::Database;
                    pszErrMsg = sqlite3_errmsg(Ctx.pExtra->pSqlite);
                    goto Exit;
                }
            }
        }
        if (NT_SUCCESS(r = TxFile.Commit()))
        {
            TxDb.Commit();
            TxDbMain.Commit();
        }
        else
            rApi = ApiResult::File;
    }
Exit:
    Json::CMutDoc j{};
    j = {
        "r", rApi,
        "r2", (UINT)r,
        "err_msg", pszErrMsg,
    };
    ApiSendResponseJson(Ctx, j);
}
TKK_API_DEF_ENTRY(ApiPost_PageSave, AwSavePage)

// Page: ReadContent
static void AwLoadPage(const API_CTX& Ctx) noexcept
{
    eck::CRefBin rb{};
    const auto pHdr = rb.PushBack<PAGE_REQ_HEADER>();
    ZeroMemory(pHdr, sizeof(PAGE_REQ_HEADER));

    std::vector<QUERY_KV> vKv{};
    ApiParseQueryString(Ctx, vKv);
    int iPageId{ DbIdInvalid };
    BOOL bTemp{};
    for (const auto& e : vKv)
    {
        if (TKK_API_HIT_QUERY("page_id"))
            eck::TcsToInt(e.V.data(), e.V.size(), iPageId, 10);
        else if (TKK_API_HIT_QUERY("is_draft"))
            eck::TcsToInt(e.V.data(), e.V.size(), bTemp, 10);
    }

    if (iPageId != DbIdInvalid)
    {
        int rTmp;

        if (!AclDbCheckCurrentUserAccess(Ctx,
            iPageId, DbAccess::ReadContent, rTmp))
        {
            pHdr->r = ApiResult::AccessDenied;
            pHdr->r2 = rTmp;
            goto Exit;
        }

        NTSTATUS nts;
        CNtTransaction TxFile{};
        nts = TxFile.Create();
        if (!NT_SUCCESS(nts))
        {
            pHdr->r = ApiResult::File;
            pHdr->r2 = nts;
            goto Exit;
        }

        auto rsTemp{ eck::GetRunningPath() };
        rsTemp.PushBack(EckStrAndLen(L"\\res\\page\\"));
        rsTemp.AppendFormat(L"%d", iPageId);

        eck::CFile Dir{};
        nts = Dir.Create(rsTemp.Data(),
            FILE_OPEN,
            FILE_GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            FILE_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT);
        if (!NT_SUCCESS(nts))
        {
            pHdr->r = ApiResult::File;
            pHdr->r2 = nts;
            goto Exit;
        }
        // 请求方需要草稿，但实际没有草稿，视为无效请求
        if (bTemp && !PageDbDraftExists(Ctx.pExtra->pSqlite, iPageId, rTmp))
        {
            pHdr->r = ApiResult::NotFound;
            pHdr->r2 = rTmp;
            goto Exit;
        }

        eck::CRefBin rbFile{};
        nts = DiffLoadFile(TxFile, Dir.Get(),
            bTemp ? L"draft.txt"sv : L"content.txt"sv, rbFile);
        if (!NT_SUCCESS(nts))
        {
            pHdr->r = ApiResult::File;
            pHdr->r2 = nts;
            goto Exit;
        }

        rTmp = PrhCompressContent(rb, rbFile);
        if (!eck::ZLibSuccess(rTmp))
        {
            pHdr->r = ApiResult::Unknown;
            pHdr->r2 = rTmp;
            goto Exit;
        }
    }
    else
        pHdr->r = ApiResult::RequiredFieldMissing;
Exit:
    ApiSendResponseBin(Ctx, rb.ToSpan());
}
TKK_API_DEF_ENTRY(ApiGet_PageLoad, AwLoadPage)

// Page: ReadChange
static void AwGetPageVersionList(const API_CTX& Ctx) noexcept
{
    ApiResult rApi{ ApiResult::Ok };
    int r{};
    PCSTR pszErrMsg{};

    std::vector<QUERY_KV> vKv{};
    ApiParseQueryString(Ctx, vKv);
    int cEntry{}, nPage{}, iPageId{ DbIdInvalid };
    for (const auto& e : vKv)
    {
        if (TKK_API_HIT_QUERY("count"))
            ApiParseInt(e.V, cEntry);
        else if (TKK_API_HIT_QUERY("page"))
            ApiParseInt(e.V, nPage);
        else if (TKK_API_HIT_QUERY("page_id"))
            ApiParseInt(e.V, iPageId);
    }

    if (cEntry <= 0 || cEntry > MaxQueryCount)
        cEntry = MaxQueryCount;

    Json::CMutDoc j{};
    const auto Arr = j.NewArr();

    if (iPageId != DbIdInvalid)
    {
        if (!AclDbCheckCurrentUserAccess(Ctx, iPageId,
            DbAccess::ReadChange, r))
        {
            rApi = ApiResult::AccessDenied;
            goto Exit;
        }

        constexpr char Sql[]{ R"(
SELECT ver_id, user_id, strftime('%s', create_at), description FROM PageVersion
WHERE page_id = ?
ORDER BY ver_id DESC
LIMIT ? OFFSET ?
)" };
        sqlite3_stmt* pStmt;
        r = sqlite3_prepare_v3(Ctx.pExtra->pSqlitePv,
            EckStrAndLen(Sql), 0, &pStmt, nullptr);
        if (r != SQLITE_OK)
        {
            pszErrMsg = sqlite3_errmsg(Ctx.pExtra->pSqlitePv);
            goto Exit;
        }
        sqlite3_bind_int(pStmt, 1, iPageId);
        sqlite3_bind_int(pStmt, 2, cEntry);
        sqlite3_bind_int(pStmt, 3, nPage * cEntry);
        while ((r = sqlite3_step(pStmt)) == SQLITE_ROW)
        {
            const auto Obj = j.NewObj();
            Obj = {
                "ver_id", sqlite3_column_int(pStmt, 0),
                "user_id", sqlite3_column_int(pStmt, 1),
                "create_at", sqlite3_column_int64(pStmt, 2),
                "description", SuColumnStringView(pStmt, 3)
            };
            Arr.ArrPushBack(Obj);
        }
        sqlite3_finalize(pStmt);
        if (r == SQLITE_DONE)
            r = SQLITE_OK;
        else
            pszErrMsg = sqlite3_errmsg(Ctx.pExtra->pSqlitePv);
    }
    else
        rApi = ApiResult::RequiredFieldMissing;
Exit:
    j = {
        "r", r == SQLITE_OK ? rApi : ApiResult::Database,
        "r2", r,
        "err_msg", pszErrMsg,
        "data", Arr
    };
    ApiSendResponseJson(Ctx, j);
}
TKK_API_DEF_ENTRY(ApiGet_PageVersionList, AwGetPageVersionList)

// Page: ReadContent | ReadChange
static void AwGetPageVersionContent(const API_CTX& Ctx) noexcept
{
    eck::CRefBin rb{};
    const auto pHdr = rb.PushBack<PAGE_REQ_HEADER>();
    ZeroMemory(pHdr, sizeof(PAGE_REQ_HEADER));

    std::vector<QUERY_KV> vKv{};
    ApiParseQueryString(Ctx, vKv);
    int iPageId{ DbIdInvalid }, iVerId{ DbIdInvalid };
    for (const auto& e : vKv)
    {
        if (TKK_API_HIT_QUERY("page_id"))
            ApiParseInt(e.V, iPageId);
        else if (TKK_API_HIT_QUERY("ver_id"))
            ApiParseInt(e.V, iVerId);
    }

    if (iPageId != DbIdInvalid)
    {
        int rTmp{};

        if (!AclDbCheckCurrentUserAccess(Ctx,
            iPageId, DbAccess::ReadContent | DbAccess::ReadChange, rTmp))
        {
            pHdr->r = ApiResult::AccessDenied;
            pHdr->r2 = rTmp;
            goto Exit;
        }

        NTSTATUS nts;
        CNtTransaction TxFile{};
        nts = TxFile.Create();
        if (!NT_SUCCESS(nts))
        {
            pHdr->r = ApiResult::File;
            pHdr->r2 = nts;
            goto Exit;
        }

        auto rsTemp{ eck::GetRunningPath() };
        rsTemp.PushBack(EckStrAndLen(L"\\res\\page\\"));
        rsTemp.AppendFormat(L"%d", iPageId);

        eck::CFile Dir{};
        nts = Dir.Create(rsTemp.Data(),
            FILE_OPEN,
            FILE_GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            FILE_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT);
        if (!NT_SUCCESS(nts))
        {
            pHdr->r = ApiResult::File;
            pHdr->r2 = nts;
            goto Exit;
        }

        eck::CRefBin rbFile{};
        nts = DiffDbGetVersionContent(Ctx.pExtra->pSqlitePv, TxFile, Dir.Get(),
            iPageId, iVerId, rbFile, rTmp);
        if (!NT_SUCCESS(nts))
        {
            pHdr->r = ApiResult::File;
            pHdr->r2 = nts;
            goto Exit;
        }

        rTmp = PrhCompressContent(rb, rbFile);
        if (!eck::ZLibSuccess(rTmp))
        {
            pHdr->r = ApiResult::Unknown;
            pHdr->r2 = rTmp;
            goto Exit;
        }
    }
    else
        pHdr->r = ApiResult::RequiredFieldMissing;
Exit:
    ApiSendResponseBin(Ctx, rb.ToSpan());
}
TKK_API_DEF_ENTRY(ApiGet_PageVersionContent, AwGetPageVersionContent)