#include "pch.h"
#include "ServerApi.h"
#include "ApiPriv.h"
#include "Database.h"
#include "SqliteUtils.h"
#include "AccessCheck.h"

// DbIdContainerProject: CreateEntity
static void AwInsertProject(const API_CTX& Ctx) noexcept
{
    ApiResult rApi{ ApiResult::Ok };
    int r{};
    PCSTR pszErrMsg{};

    const auto iUserId = CkDbGetCurrentUser(Ctx);
    if (!AclDbCheckAccess(Ctx, iUserId,
        DbIdContainerProject, DbAccess::CreateEntity, r))
    {
        rApi = ApiResult::AccessDenied;
        goto Exit;
    }

    if (Json::CDoc jIn{ Ctx.pExtra->rbBody }; jIn.IsValid())
    {
        constexpr char Sql[]{ R"(
INSERT INTO Project(project_id, project_name)
VALUES ((SELECT id FROM GlobalId), ?);
)" };
        sqlite3_stmt* pStmt;
        r = sqlite3_prepare_v3(Ctx.pExtra->pSqlite,
            EckStrAndLen(Sql), 0, &pStmt, nullptr);
        if (r != SQLITE_OK)
        {
            pszErrMsg = sqlite3_errmsg(Ctx.pExtra->pSqlite);
            goto Exit;
        }

        const auto ValName = jIn["/project_name"];
        SuBindJsonStringValueSafe(pStmt, 1, ValName, "Untitled Project"sv);

        CSqliteTransaction Tx{ Ctx.pExtra->pSqlite };
        r = DbIncrementId(Ctx.pExtra->pSqlite);
        if (r == SQLITE_OK)
        {
            r = sqlite3_step(pStmt);
            if (r == SQLITE_DONE)
                r = AclDbOnEntityCreate(Ctx, iUserId);
        }
        sqlite3_finalize(pStmt);

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
        "err_msg", pszErrMsg,
    };
    ApiSendResponseJson(Ctx, j);
}
TKK_API_DEF_ENTRY(ApiPost_InsertProject, AwInsertProject)

// Project: DeleteMe
static void AwDeleteProject(const API_CTX& Ctx) noexcept
{
    ApiResult rApi{ ApiResult::Ok };
    int r{};
    PCSTR pszErrMsg{};

    if (Json::CDoc jIn{ Ctx.pExtra->rbBody }; jIn.IsValid())
    {
        const auto ValId = jIn["/project_id"];
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
            ValId.GetInt(), DbAccess::DeleteMe, r))
        {
            rApi = ApiResult::AccessDenied;
            goto Exit;
        }

        constexpr char Sql[]{ R"(DELETE FROM Project WHERE project_id = ?)" };

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
        if (r == SQLITE_DONE)
            r = AclDbOnEntityDelete(Ctx, ValId.GetInt());
        sqlite3_finalize(pStmt);

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
        "err_msg", pszErrMsg,
    };
    ApiSendResponseJson(Ctx, j);
}
TKK_API_DEF_ENTRY(ApiPost_DeleteProject, AwDeleteProject)

// Project: Rename
static void AwUpdateProject(const API_CTX& Ctx) noexcept
{
    ApiResult rApi{ ApiResult::Ok };
    int r{};
    PCSTR pszErrMsg{};

    if (Json::CDoc jIn{ Ctx.pExtra->rbBody }; jIn.IsValid())
    {
        const auto ValId = jIn["/project_id"];
        const auto ValName = jIn["/project_name"];
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
        constexpr char Sql[]{ R"(UPDATE Project SET project_name = ? WHERE project_id = ?)" };
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
        "err_msg", pszErrMsg,
    };
    ApiSendResponseJson(Ctx, j);
}
TKK_API_DEF_ENTRY(ApiPost_UpdateProject, AwUpdateProject)

// Project: ReadContent
static void AwGetProjectList(const API_CTX& Ctx) noexcept
{
    ApiResult rApi{ ApiResult::Ok };
    int r{};
    PCSTR pszErrMsg{};

    std::vector<QUERY_KV> vKv{};
    ApiParseQueryString(Ctx, vKv);
    int cEntry{}, nPage{};
    for (const auto& e : vKv)
    {
        if (TKK_API_HIT_QUERY("count"))
            ApiParseInt(e.V, cEntry);
        else if (TKK_API_HIT_QUERY("page"))
            ApiParseInt(e.V, nPage);
    }
    if (cEntry <= 0 || cEntry > MaxQueryCount)
        cEntry = MaxQueryCount;

    Json::CMutDoc j{};
    const auto Arr = j.NewArray();

    constexpr char Sql[]{ R"(
SELECT p.project_id, p.project_name, p.create_at
FROM Project AS p
JOIN Acl AS a
ON a.entity_id = p.project_id
WHERE
    a.user_id = ? AND
    (a.access & ?) != 0
ORDER BY p.project_id ASC
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
    sqlite3_bind_int(pStmt, 1, CkDbGetCurrentUser(Ctx));
    sqlite3_bind_int(pStmt, 2, int(DbAccess::ReadContent | DbAccess::FullControl));
    sqlite3_bind_int(pStmt, 3, cEntry);
    sqlite3_bind_int(pStmt, 4, nPage * cEntry);
    while ((r = sqlite3_step(pStmt)) == SQLITE_ROW)
    {
        const auto Obj = j.NewObject();
        Obj = {
            "project_id", sqlite3_column_int(pStmt, 0),
            "project_name", SuColumnStringView(pStmt, 1),
            "create_at", sqlite3_column_int64(pStmt, 2),
        };
        Arr.ArrPushBack(Obj);
    }
    sqlite3_finalize(pStmt);
    if (r == SQLITE_DONE)
        r = SQLITE_OK;
    else
        pszErrMsg = sqlite3_errmsg(Ctx.pExtra->pSqlite);
Exit:
    j = {
        "r", ApiResult::Ok,
        "r2", r,
        "err_msg", pszErrMsg,
        "data", Arr
    };
    ApiSendResponseJson(Ctx, j);
}
TKK_API_DEF_ENTRY(ApiGet_ProjectList, AwGetProjectList)