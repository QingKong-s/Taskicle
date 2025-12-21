#pragma once
class CSqliteTransaction
{
private:
    sqlite3* m_pSqlite{};
    BOOL m_bCommitted{};
public:
    CSqliteTransaction(sqlite3* pSqlite, BOOL bImmediateLock = FALSE) noexcept : m_pSqlite{ pSqlite }
    {
        sqlite3_exec(m_pSqlite,
            bImmediateLock ? "BEGIN IMMEDIATE;" : "BEGIN;",
            nullptr, nullptr, nullptr);
    }
    ~CSqliteTransaction() noexcept
    {
        if (!m_bCommitted)
            sqlite3_exec(m_pSqlite, "ROLLBACK;", nullptr, nullptr, nullptr);
    }

    void Commit() noexcept
    {
        EckAssert(!m_bCommitted);
        m_bCommitted = TRUE;
        sqlite3_exec(m_pSqlite, "COMMIT;", nullptr, nullptr, nullptr);
    }
};

EckInline int SuBindJsonStringValue(sqlite3_stmt* pStmt,
    int idxCol, const Json::CVal& jVal, std::string_view svDef = "(Empty)"sv) noexcept
{
    EckAssert(jVal.IsValid() && jVal.IsStr());
    if (!jVal.GetLen())
        return sqlite3_bind_text(pStmt, idxCol,
            svDef.data(), (int)svDef.size(), SQLITE_STATIC);
    return sqlite3_bind_text(pStmt, idxCol,
        jVal.GetStr(), (int)jVal.GetLen(), SQLITE_STATIC);
}
EckInline int SuBindJsonStringValueSafe(sqlite3_stmt* pStmt,
    int idxCol, const Json::CVal& jVal, std::string_view svDef = "(Empty)"sv) noexcept
{
    if (!jVal.IsValid() || !jVal.IsStr() || !jVal.GetLen())
        return sqlite3_bind_text(pStmt, idxCol,
            svDef.data(), (int)svDef.size(), SQLITE_STATIC);
    return sqlite3_bind_text(pStmt, idxCol,
        jVal.GetStr(), (int)jVal.GetLen(), SQLITE_STATIC);
}

EckInline std::string_view SuColumnStringView(
    sqlite3_stmt* pStmt, int idxCol) noexcept
{
    return {
        (PCSTR)sqlite3_column_text(pStmt, idxCol),
        (size_t)sqlite3_column_bytes(pStmt, idxCol)
    };
}

inline void SuEscapeLikeQuery(std::string_view sv,
    eck::CRefStrA& rsEscaped) noexcept
{
    rsEscaped.Reserve(rsEscaped.Size() + int(sv.size() + 8));
    for (const auto ch : sv)
    {
        if (ch == '%' || ch == '_' || ch == '\\')
            rsEscaped.PushBackChar('\\');
        rsEscaped.PushBackChar(ch);
    }
}