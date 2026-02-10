#include "pch.h"
#include "ServerApi.h"
#include "ApiPriv.h"
#include "Database.h"
#include "SqliteUtils.h"
#include "AccessCheck.h"

struct CK_SID
{
    char Hdr[4];
    UINT Random[6];
    ULONGLONG TimeStamp;
};
constexpr static size_t CkSidStrLen = 2 * sizeof(CK_SID);

struct UM_PW_HASH
{
    BYTE Salt[16];
    BYTE Hash[32];
};

static eck::CTimeIdGenerator s_SeedGen{};
static eck::CSrwLock s_SeedGenLock{};

static void CkGenerateSessionId(const API_CTX& Ctx,
    _Out_writes_(CkSidStrLen) PCH pszSid) noexcept
{
    CK_SID Sid{ { 'S','I','D','0' } };

    const auto ullTidPid =
        (ULONGLONG)NtCurrentProcessId() |
        ((ULONGLONG)NtCurrentThreadId() << 32);
    eck::CPcg32 Pcg{};
    {
        eck::CSrwWriteGuard _{ s_SeedGenLock };
        Pcg.Seed(s_SeedGen.Generate() ^ ullTidPid);
    }
    for (UINT* p = Sid.Random; p < Sid.Random + ARRAYSIZE(Sid.Random); ++p)
        *p = Pcg.Next32();
    GetSystemTimeAsFileTime((FILETIME*)&Sid.TimeStamp);

    eck::ToStringUpper(&Sid, sizeof(Sid), pszSid);
}

// 将覆盖先前的会话ID，返回sqlite错误码
static int CkDbStoreSessionId(const API_CTX& Ctx,
    _In_reads_(CkSidStrLen) PCCH pszSid,
    int iUserId,
    UINT cExpiredSecond) noexcept
{
    sqlite3_stmt* pStmtCleanup, * pStmtInsert;
    int r;

    constexpr char SqlCleanup[]{ R"(DELETE FROM UserSession WHERE user_id = ?;)" };
    constexpr char SqlInsert[]{ R"(
INSERT INTO UserSession (user_id, session_id, expire_at)
VALUES (?,?,?);
)" };

    r = sqlite3_prepare_v3(Ctx.pExtra->pSqlite,
        EckStrAndLen(SqlCleanup), 0, &pStmtCleanup, nullptr);
    if (r != SQLITE_OK)
        return r;
    sqlite3_bind_int(pStmtCleanup, 1, iUserId);

    r = sqlite3_prepare_v3(Ctx.pExtra->pSqlite,
        EckStrAndLen(SqlInsert), 0, &pStmtInsert, nullptr);
    if (r != SQLITE_OK)
    {
        sqlite3_finalize(pStmtCleanup);
        return r;
    }
    sqlite3_bind_int(pStmtInsert, 1, iUserId);
    sqlite3_bind_text(pStmtInsert, 2, pszSid, (int)CkSidStrLen, nullptr);
    sqlite3_bind_int64(pStmtInsert, 3,
        eck::GetUnixTimestampMs() + cExpiredSecond * 1000ull);

    CSqliteTransaction Tx{ Ctx.pExtra->pSqlite };
    r = sqlite3_step(pStmtCleanup);
    sqlite3_finalize(pStmtCleanup);
    if (r == SQLITE_DONE)
    {
        r = sqlite3_step(pStmtInsert);
        if (r == SQLITE_DONE)
            r = SQLITE_OK;
    }
    sqlite3_finalize(pStmtInsert);
    if (r == SQLITE_OK)
        Tx.Commit();
    return r;
}

// 返回sqlite错误码
static int CkDbQuerySessionId(const API_CTX& Ctx,
    _In_reads_(CkSidStrLen) PCCH pszSid,
    _Out_ int& iUserId) noexcept
{
    iUserId = DbIdInvalid;
    sqlite3_stmt* pStmt;
    int r;

    constexpr char Sql[]{ R"(
SELECT user_id, expire_at FROM UserSession
WHERE session_id = ?;
)" };

    r = sqlite3_prepare_v3(Ctx.pExtra->pSqlite,
        EckStrAndLen(Sql), 0, &pStmt, nullptr);
    if (r != SQLITE_OK)
        return r;
    sqlite3_bind_text(pStmt, 1, pszSid, (int)CkSidStrLen, nullptr);
    r = sqlite3_step(pStmt);
    if (r == SQLITE_ROW)
    {
        r = SQLITE_OK;
        iUserId = sqlite3_column_int(pStmt, 0);
        const auto tExpire = (ULONGLONG)sqlite3_column_int64(pStmt, 1);
        if (tExpire < eck::GetUnixTimestampMs())
        {
            iUserId = DbIdInvalid;
            constexpr char SqlDelete[]{ R"(
DELETE FROM UserSession WHERE session_id = ?;
)" };
            sqlite3_stmt* pStmtDelete;
            r = sqlite3_prepare_v3(Ctx.pExtra->pSqlite,
                EckStrAndLen(SqlDelete), 0, &pStmtDelete, nullptr);
            if (r == SQLITE_OK)
            {
                sqlite3_bind_text(pStmtDelete, 1, pszSid, (int)CkSidStrLen, nullptr);
                sqlite3_step(pStmtDelete);
            }
        }
    }
    sqlite3_finalize(pStmt);
    return r;
}

// 返回当前用户ID
int CkDbGetCurrentUser(const API_CTX& Ctx) noexcept
{
    PCSTR pszCookie;
    if (!Ctx.pSender->GetHeader(Ctx.dwConnId, "Cookie", &pszCookie))
        return DbIdUserEveryone;
    auto pBegin = eck::TcsStrI(pszCookie, "sid=");
    if (!pBegin)
        return DbIdUserEveryone;
    pBegin += 4;
    auto pEnd = eck::TcsChar(pBegin, ';');
    if (!pEnd)
        pEnd = pBegin + eck::TcsLen(pBegin);
    const auto cchSid = pEnd - pBegin;
    if (cchSid != CkSidStrLen)
        return DbIdUserEveryone;
    int iUserId;
    CkDbQuerySessionId(Ctx, pBegin, iUserId);
    if (iUserId != DbIdInvalid)
        return iUserId;
    return DbIdUserEveryone;
}

int CkDbGetCurrentPseudoUser(const API_CTX& Ctx) noexcept
{
    const auto id = CkDbGetCurrentUser(Ctx);
    if (UmIsAdministrator(Ctx, id))
        return DbIdUserAdmin;
    return id;
}

BOOL UmIsAdministrator(const API_CTX& Ctx, int id) noexcept
{
    int r;
    constexpr char Sql[]{ R"(SELECT role FROM User WHERE user_id = ?;)" };

    sqlite3_stmt* pStmt;
    r = sqlite3_prepare_v3(Ctx.pExtra->pSqlite,
        EckStrAndLen(Sql), 0, &pStmt, nullptr);
    if (r != SQLITE_OK)
        return FALSE;
    sqlite3_bind_int(pStmt, 1, id);
    if (sqlite3_step(pStmt) == SQLITE_ROW)
    {
        r = sqlite3_column_int(pStmt, 2);
        sqlite3_finalize(pStmt);
        return r == (int)DbUserRole::Admin;
    }
    sqlite3_finalize(pStmt);
    return FALSE;
}

static ApiResult UmDbQueryUser(
    const API_CTX& Ctx,
    std::string_view svUserName,
    _Out_ UM_PW_HASH& Hash,
    _Out_ DbUserRole& eRole,
    _Out_ int& iUserId,
    _Out_ int& r) noexcept
{
    Hash = {};
    eRole = DbUserRole::Normal;
    iUserId = DbIdInvalid;

    constexpr char Sql[]{ R"(SELECT user_id, pw_hash, role FROM User WHERE user_name = ?;)" };

    sqlite3_stmt* pStmt;
    r = sqlite3_prepare_v3(Ctx.pExtra->pSqlite,
        EckStrAndLen(Sql), 0, &pStmt, nullptr);
    if (r != SQLITE_OK)
        return ApiResult::Database;
    sqlite3_bind_text(pStmt, 1, svUserName.data(), (int)svUserName.size(), nullptr);
    while ((r = sqlite3_step(pStmt)) == SQLITE_ROW)
    {
        r = SQLITE_OK;
        iUserId = sqlite3_column_int(pStmt, 0);

        const auto pszPw = (PCSTR)sqlite3_column_text(pStmt, 1);
        const auto cchPw = sqlite3_column_bytes(pStmt, 1);

        const auto pEndVer = eck::TcsChar(pszPw, ',');
        if (!pEndVer)
            break;
        const auto pEndSalt = eck::TcsChar(pEndVer + 1, ',');
        if (!pEndSalt || (pEndSalt - pEndVer - 1) != 32)
            break;
        if (pszPw + cchPw - pEndSalt - 1 != 64)
            break;
        eck::FromString(Hash.Salt, sizeof(Hash.Salt), pEndVer + 1);
        eck::FromString(Hash.Hash, sizeof(Hash.Hash), pEndSalt + 1);

        eRole = (DbUserRole)sqlite3_column_int(pStmt, 2);

        sqlite3_finalize(pStmt);
        return ApiResult::Ok;
    }
    sqlite3_finalize(pStmt);
    return ApiResult::Unknown;
}

static NTSTATUS UmRandomBytes(
    _Out_writes_bytes_(cbSalt) void* pSalt,
    size_t cbSalt) noexcept
{
    return BCryptGenRandom(nullptr, (UCHAR*)pSalt,
        (ULONG)cbSalt, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
}

// 哈希密码，使用Hash.Salt中的盐值，输出覆盖Hash.Hash
static NTSTATUS UmHashPassword(
    _In_reads_bytes_(cbPw) PCVOID pPw,
    size_t cbPw,
    _Inout_ UM_PW_HASH& Hash) noexcept
{
    NTSTATUS nts;
    BCRYPT_ALG_HANDLE hAlg;
    nts = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM,
        nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG);
    if (!NT_SUCCESS(nts))
        return nts;
    nts = BCryptDeriveKeyPBKDF2(hAlg,
        (UCHAR*)pPw, (ULONG)cbPw,
        Hash.Salt, sizeof(Hash.Salt),
        10000,
        Hash.Hash, sizeof(Hash.Hash), 0);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return nts;
}

static ApiResult UmDbCreateUser(
    const API_CTX& Ctx,
    std::string_view svUserName,
    const UM_PW_HASH& Hash,
    DbUserRole eRole,
    _Out_ int& r) noexcept
{
    constexpr char Sql[]{ R"(
INSERT INTO User (user_name, pw_hash, role) VALUES (?,?,?);
)" };

    sqlite3_stmt* pStmt;
    r = sqlite3_prepare_v3(Ctx.pExtra->pSqlite,
        EckStrAndLen(Sql), 0, &pStmt, nullptr);
    if (r != SQLITE_OK)
        return ApiResult::Database;

    sqlite3_bind_text(pStmt, 1, svUserName.data(), (int)svUserName.size(), nullptr);

    char szPw[1 + 1 + 32 + 1 + 64 + 1]{ "1," };
    PCH p = szPw + 2;
    eck::ToStringUpper(Hash.Salt, sizeof(Hash.Salt), p);
    p += 32;
    *p++ = ',';
    eck::ToStringUpper(Hash.Hash, sizeof(Hash.Hash), p);

    sqlite3_bind_text(pStmt, 2, EckStrAndLen(szPw), nullptr);
    EckAssert(*(szPw + sizeof(szPw) - 2) != '\0');

    sqlite3_bind_int(pStmt, 3, (int)eRole);

    r = sqlite3_step(pStmt);
    sqlite3_finalize(pStmt);
    if (r == SQLITE_DONE)
        r = SQLITE_OK;
    else
        return ApiResult::Database;
    return ApiResult::Ok;
}

static void AwLogin(const API_CTX& Ctx) noexcept
{
    ApiResult rApi;
    UINT r{};
    PCSTR pszErrMsg{};

    std::vector<QUERY_KV> vKv{};
    ApiParseQueryString(Ctx, vKv);
    std::string_view svName, svPwd;
    for (const auto& e : vKv)
    {
        if (TKK_API_HIT_QUERY("user_name"))
            svName = e.V;
        else if (TKK_API_HIT_QUERY("password"))
            svPwd = e.V;
    }

    THeader HdSetCookie{ "Set-Cookie", "" };
    eck::CRefStrA rsSetCookie{};
    DbUserRole eRole{ DbUserRole::Normal };

    if (svName.empty() || svPwd.empty())
        rApi = ApiResult::RequiredFieldMissing;
    else
    {
        UM_PW_HASH Hash;
        int iUserId;

        int rSql;
        rApi = UmDbQueryUser(Ctx, svName, Hash, eRole, iUserId, rSql);
        if (rSql != SQLITE_OK)
        {
            r = (UINT)rSql;
            pszErrMsg = sqlite3_errmsg(Ctx.pExtra->pSqlite);
            goto Exit;
        }
        UM_PW_HASH HashInput{ Hash };
        const auto nts = UmHashPassword(svPwd.data(), svPwd.size(), HashInput);
        if (!NT_SUCCESS(nts))
        {
            rApi = ApiResult::Crypt;
            r = (UINT)nts;
            goto Exit;
        }

        if (memcmp(Hash.Hash, HashInput.Hash, sizeof(Hash.Hash)) == 0)
        {
            constexpr UINT CkSessionExpireSecond = 10 * 24 * 60 * 60;// 10天
            char Sid[CkSidStrLen];
            CkGenerateSessionId(Ctx, Sid);
            rSql = CkDbStoreSessionId(Ctx, Sid, iUserId, CkSessionExpireSecond);
            if (rSql != SQLITE_OK)
            {
                rApi = ApiResult::Database;
                r = (UINT)rSql;
                pszErrMsg = sqlite3_errmsg(Ctx.pExtra->pSqlite);
                goto Exit;
            }
            rApi = ApiResult::Ok;
            rsSetCookie.Reserve(120);
            rsSetCookie.Assign(EckStrAndLen("sid="));
            rsSetCookie
                .PushBack(Sid, CkSidStrLen)
                .PushBack(EckStrAndLen("; Max-Age=864000;"))// 10天
                ;
            HdSetCookie.value = rsSetCookie.Data();
        }
        else
            rApi = ApiResult::InvalidPassword;
    }
Exit:
    Json::CMutDoc j{};
    j = {
        "r", rApi,
        "r2", r,
        "err_msg", pszErrMsg,
        "data", {
            "role", eRole
        }
    };
    ApiSendResponseJson(Ctx, j, 200, &HdSetCookie, 1);
}
TKK_API_DEF_ENTRY(ApiGet_Login, AwLogin)

static void AwRegister(const API_CTX& Ctx) noexcept
{
    ApiResult rApi;
    UINT r2{};
    PCSTR pszErrMsg{};

    if (Json::CDoc jIn{ Ctx.pExtra->rbBody }; jIn.IsValid())
    {
        const auto ValName = jIn["/name"];
        const auto ValPwd = jIn["/password"];
        const auto ValKey = jIn["/key"];
        const auto ValRole = jIn["/role"];
        if (!ValName.IsValid() || !ValPwd.IsValid() ||
            !ValKey.IsValid() || !ValRole.IsValid())
        {
            rApi = ApiResult::RequiredFieldMissing;
            goto Exit;
        }
        if (!ValName.IsString() || !ValPwd.IsString() ||
            !ValKey.IsString() || !ValRole.IsInt())
        {
            rApi = ApiResult::TypeMismatch;
            goto Exit;
        }
        if (!ApiEnumInRange(ValRole.GetInt(), DbUserRole::Min, DbUserRole::Max))
        {
            rApi = ApiResult::InvalidEnum;
            goto Exit;
        }

        // TODO: 验证key

        UM_PW_HASH Hash;
        auto nts = UmRandomBytes(Hash.Salt, sizeof(Hash.Salt));
        if (!NT_SUCCESS(nts))
        {
            rApi = ApiResult::Crypt;
            r2 = (UINT)nts;
            goto Exit;
        }
        nts = UmHashPassword(ValPwd.GetString(), ValPwd.GetLength(), Hash);
        if (!NT_SUCCESS(nts))
        {
            rApi = ApiResult::Crypt;
            r2 = (UINT)nts;
            goto Exit;
        }
        int rSql;
        rApi = UmDbCreateUser(Ctx, { ValName.GetString(), ValName.GetLength() },
            Hash, (DbUserRole)ValRole.GetInt(), rSql);
        if (rSql != SQLITE_OK)
        {
            r2 = (UINT)rSql;
            pszErrMsg = sqlite3_errmsg(Ctx.pExtra->pSqlite);
            goto Exit;
        }
        rApi = ApiResult::Ok;
    }
    else
        rApi = ApiResult::BadPayload;
Exit:
    Json::CMutDoc j{};
    j = {
        "r", rApi,
        "r2", r2,
        "err_msg", pszErrMsg,
    };
    ApiSendResponseJson(Ctx, j);
}
TKK_API_DEF_ENTRY(ApiPost_Register, AwRegister)