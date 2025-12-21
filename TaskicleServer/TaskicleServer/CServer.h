#pragma once
enum class DbStmtCache
{

};

struct ConnectionData final
{
    LONG cRef{ 1 };
    eck::CRefBin rbBody{};
    sqlite3* pSqlite{};
    sqlite3* pSqlitePv{};

    ~ConnectionData();
    EckInline void IncRef() noexcept { InterlockedIncrement(&cRef); }
    EckInline void DecRef() noexcept
    {
        if (InterlockedDecrement(&cRef) == 0)
            delete this;
    }

    int OpenDatabase() noexcept;
    int OpenPageDatabase() noexcept;
};

class CServer final : public CHttpServerListener
{
private:
    using EXTRA = ConnectionData;

    static inline CServer* s_pInst{};
    static inline IHttpServer* s_pServer{};

    EnHttpParseResult OnHeadersComplete(IHttpServer* pSender, CONNID dwConnId) override;
    EnHttpParseResult OnBody(IHttpServer* pSender,
        CONNID dwConnId, const BYTE* pData, int iLength) override;
    EnHttpParseResult OnMessageComplete(IHttpServer* pSender, CONNID dwConnId) override;
    EnHttpParseResult OnParseError(IHttpServer* pSender, CONNID dwConnId,
        int iErrorCode, LPCSTR lpszErrorDesc) override;
    EnHandleResult OnAccept(ITcpServer* pSender, CONNID dwConnId, UINT_PTR soClient) override;
    EnHandleResult OnClose(ITcpServer* pSender, CONNID dwConnId,
        EnSocketOperation enOperation, int iErrorCode) override;
public:
    static EnSocketError Start() noexcept;
    static void Stop() noexcept;
};