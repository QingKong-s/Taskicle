#include "pch.h"
#include "ServerApi.h"
#include "ApiPriv.h"

static void ApipDatabaseError(const API_CTX& Ctx, int r) noexcept
{
    Ctx.pSender->SendResponse(Ctx.dwConnId,
        HSC_INTERNAL_SERVER_ERROR, "Internal Server Error");
    LOGE << "OpenDatabase Failed: " << r <<
        "(" << sqlite3_errmsg(Ctx.pExtra->pSqlite) << ")";
}
BOOL ApiPreAction(const API_CTX& Ctx) noexcept
{
    auto r = Ctx.pExtra->OpenDatabase();
    if (r != SQLITE_OK)
    {
        ApipDatabaseError(Ctx, r);
        return FALSE;
    }
    r = Ctx.pExtra->OpenPageDatabase();
    if (r != SQLITE_OK)
    {
        ApipDatabaseError(Ctx, r);
        return FALSE;
    }
    return TRUE;
}
void ApiPostAction(const API_CTX& Ctx) noexcept
{
    Ctx.pExtra->DecRef();
}

void ApiParseQueryString(const API_CTX& Ctx, std::vector<QUERY_KV>& vKv) noexcept
{
    const auto pszQuery = Ctx.pSender->GetUrlField(Ctx.dwConnId, HUF_QUERY);
    if (!pszQuery || !*pszQuery)
        return;
    std::string_view svQuery{ pszQuery };
    size_t i = 0;
    while (i < svQuery.size())
    {
        const auto posEq = svQuery.find('=', i);
        const auto posAmp = svQuery.find('&', i);
        if (posEq == std::string_view::npos ||
            (posAmp != std::string_view::npos && posAmp < posEq))
            break;
        QUERY_KV kv{};
        kv.K = svQuery.substr(i, posEq - i);
        if (posAmp == std::string_view::npos)
        {
            kv.V = svQuery.substr(posEq + 1);
            vKv.emplace_back(kv);
            break;
        }
        else
        {
            kv.V = svQuery.substr(posEq + 1, posAmp - posEq - 1);
            vKv.emplace_back(kv);
            i = posAmp + 1;
        }
    }
}

void ApiParseInt(std::string_view sv, _Inout_ int& i) noexcept
{
    int j;
    if (eck::TcsToInt(sv.data(), sv.size(), j, 10) == eck::TcsCvtErr::Ok)
        i = j;
}

void ApiSendResponseJson(const API_CTX& Ctx, Json::CMutDoc& j,
    USHORT usStatusCode, const THeader* pHeader, size_t cHeader) noexcept
{
    size_t cchJson;
    const auto pszJson = j.Write(cchJson, 0);
    Ctx.pSender->SendResponse(Ctx.dwConnId, usStatusCode, nullptr,
        pHeader, (int)cHeader, (const BYTE*)pszJson, (int)cchJson);
    free(pszJson);
}
void ApiSendResponseBin(const API_CTX& Ctx, std::span<const BYTE> sp,
    USHORT usStatusCode, const THeader* pHeader, size_t cHeader) noexcept
{
    Ctx.pSender->SendResponse(Ctx.dwConnId, usStatusCode, nullptr,
        pHeader, (int)cHeader, sp.data(), (int)sp.size());
}

void ApiMakeResourcePath(eck::CRefStrW& rsPath, std::wstring_view svResPath) noexcept
{
    rsPath = eck::GetRunningPath();
    rsPath.PushBack(EckStrAndLen(LR"(\res\dist)"));
    rsPath.PushBack(svResPath);
}


static void AwSendFileResource(const API_CTX& Ctx, std::wstring_view svResPath) noexcept
{
    eck::CRefStrW rsPath{};
    ApiMakeResourcePath(rsPath, svResPath);

    NTSTATUS nts;
    const auto rb = eck::ReadInFile(rsPath.Data(), &nts);
    if (!NT_SUCCESS(nts))
    {
        Ctx.pSender->SendResponse(Ctx.dwConnId, HSC_NOT_FOUND, "Not Found",
            nullptr, 0, nullptr, 0);
        return;
    }
    Ctx.pSender->SendResponse(Ctx.dwConnId,
        HSC_OK, "OK",
        nullptr, 0,
        rb.Data(), (int)rb.Size());
}
EnHttpParseResult ApiGet_Index(const API_CTX& Ctx) noexcept
{
    Ctx.pExtra->IncRef();
    eck::TpSubmitSimpleCallback(nullptr, [Ctx](PTP_CALLBACK_INSTANCE) noexcept
        {
            if (ApiPreAction(Ctx))
            {
                AwSendFileResource(Ctx, L"\\index.html"sv);
                ApiPostAction(Ctx);
            }
        });
    return HPR_OK;
}
EnHttpParseResult ApiGet_ResourceFile(const API_CTX& Ctx) noexcept
{
    Ctx.pExtra->IncRef();
    eck::TpSubmitSimpleCallback(nullptr, [Ctx](PTP_CALLBACK_INSTANCE) noexcept
        {
            if (ApiPreAction(Ctx))
            {
                const auto pszPath = Ctx.pSender->GetUrlField(Ctx.dwConnId, HUF_PATH);
                if (pszPath && *pszPath)
                {
                    const auto rsPathW = eck::StrU82W(pszPath);
                    AwSendFileResource(Ctx, rsPathW.ToStringView());
                }
                else
                {
                    Ctx.pSender->SendResponse(Ctx.dwConnId, HSC_NOT_FOUND, "Not Found",
                        nullptr, 0, nullptr, 0);
                }
                ApiPostAction(Ctx);
            }
        });
    return HPR_OK;
}