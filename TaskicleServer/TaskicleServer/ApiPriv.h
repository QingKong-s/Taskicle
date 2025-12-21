#pragma once
struct QUERY_KV
{
    std::string_view K{};
    std::string_view V{};
};

#define TKK_API_HIT_QUERY(x) eck::TcsEqualLen2I(e.K.data(), e.K.size(), EckStrAndLen(x))

#define TKK_API_DEF_ENTRY(Name, Worker)                 \
    EnHttpParseResult Name(const API_CTX& Ctx) noexcept \
    {                                   \
        Ctx.pExtra->IncRef();           \
        eck::TpSubmitSimpleCallback(nullptr, [Ctx](PTP_CALLBACK_INSTANCE) noexcept \
            {                           \
                if (ApiPreAction(Ctx))  \
                    Worker(Ctx);        \
                ApiPostAction(Ctx);     \
            });                         \
        return HPR_OK;                  \
    }

constexpr int MaxQueryCount = 50;

BOOL ApiPreAction(const API_CTX& Ctx) noexcept;
void ApiPostAction(const API_CTX& Ctx) noexcept;

void ApiParseQueryString(const API_CTX& Ctx, std::vector<QUERY_KV>& vKv) noexcept;
// 仅当解析成功时覆盖i的值
void ApiParseInt(std::string_view sv, _Inout_ int& i) noexcept;

void ApiSendResponseJson(const API_CTX& Ctx, Json::CMutDoc& j,
    USHORT usStatusCode = 200, const THeader* pHeader = nullptr, size_t cHeader = 0) noexcept;
void ApiSendResponseBin(const API_CTX& Ctx, std::span<const BYTE> sp,
    USHORT usStatusCode = 200, const THeader* pHeader = nullptr, size_t cHeader = 0) noexcept;

// 检查枚举值v是否在(Min, Max)内
template<class U>
EckInlineNdCe BOOL ApiEnumInRange(auto v, U Min, U Max) noexcept
{
    using TInt = eck::UnderlyingType_T<U>;
    return (TInt)v > (TInt)Min && (TInt)v < (TInt)Max;
}