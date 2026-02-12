#include "pch.h"
#include "ServerApi.h"
#include "ApiPriv.h"
#include "Database.h"
#include "SqliteUtils.h"
#include "AccessCheck.h"

// PageGroup: CreateEntity
static void AwInsertPage(const API_CTX& Ctx) noexcept
{
    ApiResult rApi{ ApiResult::Ok };
    int r{};
    PCSTR pszErrMsg{};
    int iNewPageId{ DbIdInvalid };

    if (Json::CDoc jIn{ Ctx.pExtra->rbBody }; jIn.IsValid())
    {
        const auto ValGroup = jIn["/page_group_id"];
        const auto ValName = jIn["/page_name"];
        if (!ValGroup.IsValid() || !ValName.IsValid())
        {
            rApi = ApiResult::RequiredFieldMissing;
            goto Exit;
        }
        if (!ValGroup.IsInt() || !ValName.IsString())
        {
            rApi = ApiResult::TypeMismatch;
            goto Exit;
        }

        const auto iUserId = CkDbGetCurrentUser(Ctx);
        if (!AclDbCheckAccess(Ctx, iUserId,
            ValGroup.GetInt(), DbAccess::CreateEntity, r))
        {
            rApi = ApiResult::AccessDenied;
            goto Exit;
        }

        constexpr char Sql[]{ R"(
INSERT INTO Page(page_id, page_group_id, page_name)
VALUES ((SELECT id FROM GlobalId), ?, ?);
)" };
        sqlite3_stmt* pStmt;
        r = sqlite3_prepare_v3(Ctx.pExtra->pSqlite,
            EckStrAndLen(Sql), 0, &pStmt, nullptr);
        if (r != SQLITE_OK)
        {
            pszErrMsg = sqlite3_errmsg(Ctx.pExtra->pSqlite);
            goto Exit;
        }
        sqlite3_bind_int(pStmt, 1, ValGroup.GetInt());
        SuBindJsonStringValue(pStmt, 2, ValName, "Untitled Page"sv);

        CSqliteTransaction Tx{ Ctx.pExtra->pSqlite };
        r = DbIncrementId(Ctx.pExtra->pSqlite);
        if (r == SQLITE_OK)
        {
            r = sqlite3_step(pStmt);
            if (r == SQLITE_DONE)
            {
                iNewPageId = (int)sqlite3_last_insert_rowid(Ctx.pExtra->pSqlite);
                r = AclDbOnEntityCreate(Ctx, iUserId);
            }
        }
        sqlite3_finalize(pStmt);
        if (r == SQLITE_OK)
            Tx.Commit();
        else
        {
            pszErrMsg = sqlite3_errmsg(Ctx.pExtra->pSqlite);
            iNewPageId = DbIdInvalid;
        }
    }
    else
        rApi = ApiResult::BadPayload;
Exit:
    Json::CMutDoc j{};
    j = {
        "r", r == SQLITE_OK ? rApi : ApiResult::Database,
        "r2", r,
        "err_msg", pszErrMsg,
        "data", {
            "page_id", iNewPageId
        }
    };
    ApiSendResponseJson(Ctx, j);
}
TKK_API_DEF_ENTRY(ApiPost_InsertPage, AwInsertPage)

// Page: Rename
static void AwUpdatePage(const API_CTX& Ctx) noexcept
{
    ApiResult rApi{ ApiResult::Ok };
    int r{};
    PCSTR pszErrMsg{};

    if (Json::CDoc jIn{ Ctx.pExtra->rbBody }; jIn.IsValid())
    {
        const auto ValId = jIn["/page_id"];
        const auto ValName = jIn["/page_name"];
        if (!ValId.IsValid() || !ValName.IsValid())
        {
            rApi = ApiResult::RequiredFieldMissing;
            goto Exit;
        }
        if (!ValId.IsInt() || !ValName.IsString())
        {
            rApi = ApiResult::TypeMismatch;
            goto Exit;
        }

        if (!AclDbCheckCurrentUserAccess(Ctx,
            ValId.GetInt(), DbAccess::Rename, r))
        {
            rApi = ApiResult::AccessDenied;
            goto Exit;
        }

        constexpr char Sql[]{ R"(UPDATE Page SET page_name = ? WHERE page_id = ?;)" };

        sqlite3_stmt* pStmt;
        r = sqlite3_prepare_v3(Ctx.pExtra->pSqlite,
            EckStrAndLen(Sql), 0, &pStmt, nullptr);
        if (r != SQLITE_OK)
        {
            pszErrMsg = sqlite3_errmsg(Ctx.pExtra->pSqlite);
            goto Exit;
        }
        SuBindJsonStringValue(pStmt, 1, ValName);
        sqlite3_bind_int(pStmt, 2, ValId.GetInt());

        r = sqlite3_step(pStmt);
        sqlite3_finalize(pStmt);
        if (r == SQLITE_DONE)
            r = SQLITE_OK;
        else
            pszErrMsg = sqlite3_errmsg(Ctx.pExtra->pSqlite);
    }
    else
        rApi = ApiResult::BadPayload;
Exit:
    Json::CMutDoc j{};
    j = {
        "r", r == SQLITE_OK ? rApi : ApiResult::Database,
        "r2", r,
        "err_msg", pszErrMsg ? pszErrMsg : "",
    };
    ApiSendResponseJson(Ctx, j);
}
TKK_API_DEF_ENTRY(ApiPost_UpdatePage, AwUpdatePage)

// Page: Delete
static void AwDeletePage(const API_CTX& Ctx) noexcept
{
    ApiResult rApi{ ApiResult::Ok };
    int r{};
    PCSTR pszErrMsg{};

    if (Json::CDoc jIn{ Ctx.pExtra->rbBody }; jIn.IsValid())
    {
        const auto ValId = jIn["/page_id"];
        if (!ValId.IsValid())
        {
            rApi = ApiResult::RequiredFieldMissing;
            goto Exit;
        }
        if (!ValId.IsInt())
        {
            rApi = ApiResult::TypeMismatch;
            goto Exit;
        }

        if (!AclDbCheckCurrentUserAccess(Ctx,
            ValId.GetInt(), DbAccess::Delete, r))
        {
            rApi = ApiResult::AccessDenied;
            goto Exit;
        }

        constexpr char Sql[]{ R"(DELETE FROM Page WHERE page_id = ?)" };
        sqlite3_stmt* pStmt;
        r = sqlite3_prepare_v3(Ctx.pExtra->pSqlite,
            EckStrAndLen(Sql), 0, &pStmt, nullptr);
        if (r != SQLITE_OK)
        {
            pszErrMsg = sqlite3_errmsg(Ctx.pExtra->pSqlite);
            goto Exit;
        }
        sqlite3_bind_int(pStmt, 1, ValId.GetInt());

        CSqliteTransaction Tx{ Ctx.pExtra->pSqlite };
        r = sqlite3_step(pStmt);
        sqlite3_finalize(pStmt);
        if (r == SQLITE_DONE)
            r = AclDbOnEntityDelete(Ctx, ValId.GetInt());
        if (r == SQLITE_OK)
            Tx.Commit();
        else
            pszErrMsg = sqlite3_errmsg(Ctx.pExtra->pSqlite);
    }
    else
        rApi = ApiResult::BadPayload;
Exit:
    Json::CMutDoc j{};
    j = {
        "r", r == SQLITE_OK ? rApi : ApiResult::Database,
        "r2", r,
        "err_msg", pszErrMsg ? pszErrMsg : "",
    };
    ApiSendResponseJson(Ctx, j);
}
TKK_API_DEF_ENTRY(ApiPost_DeletePage, AwDeletePage)

// Page: ReadContent
static void AwGetPageList(const API_CTX& Ctx) noexcept
{
    ApiResult rApi{ ApiResult::Ok };
    int r{};
    PCSTR pszErrMsg{};

    std::vector<QUERY_KV> vKv{};
    ApiParseQueryString(Ctx, vKv);
    int cEntry{}, nPage{}, iGroupId{ DbIdInvalid };
    for (const auto& e : vKv)
    {
        if (TKK_API_HIT_QUERY("count"))
            ApiParseInt(e.V, cEntry);
        else if (TKK_API_HIT_QUERY("page"))
            ApiParseInt(e.V, nPage);
        else if (TKK_API_HIT_QUERY("group_id"))
            ApiParseInt(e.V, iGroupId);
    }

    if (cEntry <= 0 || cEntry > MaxQueryCount)
        cEntry = MaxQueryCount;

    Json::CMutDoc j{};
    const auto Arr = j.NewArray();

    if (iGroupId != DbIdInvalid)
    {
        constexpr char Sql[]{ R"(
SELECT p.page_id, p.page_name, p.create_at, p.has_draft
FROM Page AS p
JOIN Acl AS a
ON a.entity_id = p.page_id
WHERE
    p.page_group_id = ? AND
    a.user_id = ? AND
    (a.access & ?) != 0
ORDER BY p.page_id ASC
LIMIT ? OFFSET ?;
)" };
        sqlite3_stmt* pStmt;
        r = sqlite3_prepare_v3(Ctx.pExtra->pSqlite,
            EckStrAndLen(Sql), 0, &pStmt, nullptr);
        if (r != SQLITE_OK)
        {
            pszErrMsg = sqlite3_errmsg(Ctx.pExtra->pSqlite);
            goto Exit;
        }
        sqlite3_bind_int(pStmt, 1, iGroupId);
        sqlite3_bind_int(pStmt, 2, CkDbGetCurrentPseudoUser(Ctx));
        sqlite3_bind_int(pStmt, 3, int(DbAccess::ReadContent | DbAccess::FullControl));
        sqlite3_bind_int(pStmt, 4, cEntry);
        sqlite3_bind_int(pStmt, 5, nPage * cEntry);
        while ((r = sqlite3_step(pStmt)) == SQLITE_ROW)
        {
            const auto Obj = j.NewObject();
            Obj = {
                "page_id", sqlite3_column_int(pStmt, 0),
                "page_name", SuColumnStringView(pStmt, 1),
                "create_at", sqlite3_column_int64(pStmt, 2),
                "has_draft", (bool)!!sqlite3_column_int(pStmt, 3),
            };
            Arr.ArrPushBack(Obj);
        }
        sqlite3_finalize(pStmt);
        if (r == SQLITE_DONE)
            r = SQLITE_OK;
        else
            pszErrMsg = sqlite3_errmsg(Ctx.pExtra->pSqlite);
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
TKK_API_DEF_ENTRY(ApiGet_PageList, AwGetPageList)