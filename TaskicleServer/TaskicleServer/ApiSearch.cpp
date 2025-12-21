#include "pch.h"
#include "ServerApi.h"
#include "ApiPriv.h"
#include "Database.h"
#include "SqliteUtils.h"
#include "AccessCheck.h"

static void AwSearchEntity(const API_CTX& Ctx) noexcept
{
    ApiResult rApi{ ApiResult::Ok };
    int r{};
    PCSTR pszErrMsg{};

    std::vector<QUERY_KV> vKv{};
    ApiParseQueryString(Ctx, vKv);
    int cEntry{}, nPage{};
    std::string_view svKeyword{};
    for (const auto& e : vKv)
    {
        if (TKK_API_HIT_QUERY("count"))
            ApiParseInt(e.V, cEntry);
        else if (TKK_API_HIT_QUERY("page"))
            ApiParseInt(e.V, nPage);
        else if (TKK_API_HIT_QUERY("keyword"))
            svKeyword = e.V;
    }
    if (cEntry <= 0 || cEntry > MaxQueryCount)
        cEntry = MaxQueryCount;

    Json::CMutDoc j{};
    const auto Arr = j.NewArr();

    if (!svKeyword.empty())
    {
        constexpr char Sql[]{ R"(
SELECT s.*
FROM CoreEntity s
JOIN Acl a ON a.entity_id = s.entity_id
WHERE
    a.user_id = ?
    AND (a.access & ?) != 0
    AND s.name LIKE ?
ORDER BY s.entity_id
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
        eck::CRefStrA rsKeyword{};
        rsKeyword.PushBackChar('%');
        SuEscapeLikeQuery(svKeyword, rsKeyword);
        rsKeyword.PushBackChar('%');
        sqlite3_bind_text(pStmt, 3, rsKeyword.Data(), rsKeyword.Size(), nullptr);
        sqlite3_bind_int(pStmt, 4, cEntry);
        sqlite3_bind_int(pStmt, 5, nPage * cEntry);
        while ((r = sqlite3_step(pStmt)) == SQLITE_ROW)
        {
            const auto Obj = j.NewObj();
            Obj = {
                "entity_id", sqlite3_column_int(pStmt, 0),
                "type", sqlite3_column_int(pStmt, 1),
                "name", SuColumnStringView(pStmt, 2),
                "create_at", sqlite3_column_int64(pStmt, 3),
                "container_id", sqlite3_column_int(pStmt, 4),
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
TKK_API_DEF_ENTRY(ApiGet_SearchEntity, AwSearchEntity)