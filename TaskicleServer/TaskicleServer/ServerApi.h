#pragma once
#include "CServer.h"

struct API_CTX
{
    IHttpServer* pSender{};
    CONNID dwConnId{};
    ConnectionData* pExtra{};
};

enum class ApiResult
{
    Ok,
    Unknown,        // 未知错误

    BadPayload,     // 携带的数据格式错误
    RequiredFieldMissing,   // 缺少必要字段
    TypeMismatch,   // 数据类型不匹配
    NoField,        // 至少需要一个字段
    InvalidEnum,    // 枚举值无效

    Database,       // 数据库错误
    NoEffect,       // 操作无影响
    File,           // 文件操作失败
    NotFound,       // 找不到目标实体
    Crypt,          // 加解密失败
    AccessDenied,   // 访问被拒绝
    InvalidPassword,// 密码错误
};

EnHttpParseResult ApiGet_Index(const API_CTX& Ctx) noexcept;
EnHttpParseResult ApiGet_ResourceFile(const API_CTX& Ctx) noexcept;

// Project

EnHttpParseResult ApiPost_InsertProject(const API_CTX& Ctx) noexcept;
EnHttpParseResult ApiPost_DeleteProject(const API_CTX& Ctx) noexcept;
EnHttpParseResult ApiPost_UpdateProject(const API_CTX& Ctx) noexcept;
EnHttpParseResult ApiGet_ProjectList(const API_CTX& Ctx) noexcept;

// Task

EnHttpParseResult ApiPost_InsertTask(const API_CTX& Ctx) noexcept;
EnHttpParseResult ApiPost_DeleteTask(const API_CTX& Ctx) noexcept;
EnHttpParseResult ApiPost_UpdateTask(const API_CTX& Ctx) noexcept;
EnHttpParseResult ApiGet_TaskList(const API_CTX& Ctx) noexcept;

EnHttpParseResult ApiGet_TaskLogList(const API_CTX& Ctx) noexcept;

EnHttpParseResult ApiPost_InsertTaskRelation(const API_CTX& Ctx) noexcept;
EnHttpParseResult ApiPost_DeleteTaskRelation(const API_CTX& Ctx) noexcept;
EnHttpParseResult ApiGet_TaskRelationList(const API_CTX& Ctx) noexcept;

EnHttpParseResult ApiPost_InsertTaskComment(const API_CTX& Ctx) noexcept;
EnHttpParseResult ApiPost_DeleteTaskComment(const API_CTX& Ctx) noexcept;
EnHttpParseResult ApiPost_UpdateTaskComment(const API_CTX& Ctx) noexcept;
EnHttpParseResult ApiGet_TaskCommentList(const API_CTX& Ctx) noexcept;

// PageGroup

EnHttpParseResult ApiPost_InsertPageGroup(const API_CTX& Ctx) noexcept;
EnHttpParseResult ApiPost_DeletePageGroup(const API_CTX& Ctx) noexcept;
EnHttpParseResult ApiPost_UpdatePageGroup(const API_CTX& Ctx) noexcept;
EnHttpParseResult ApiGet_PageGroupList(const API_CTX& Ctx) noexcept;

// Page

EnHttpParseResult ApiPost_InsertPage(const API_CTX& Ctx) noexcept;
EnHttpParseResult ApiPost_DeletePage(const API_CTX& Ctx) noexcept;
EnHttpParseResult ApiPost_UpdatePage(const API_CTX& Ctx) noexcept;
EnHttpParseResult ApiGet_PageList(const API_CTX& Ctx) noexcept;

EnHttpParseResult ApiPost_PageSave(const API_CTX& Ctx) noexcept;
EnHttpParseResult ApiGet_PageLoad(const API_CTX& Ctx) noexcept;

EnHttpParseResult ApiGet_PageVersionList(const API_CTX& Ctx) noexcept;
EnHttpParseResult ApiGet_PageVersionContent(const API_CTX& Ctx) noexcept;

// Auth

EnHttpParseResult ApiGet_Login(const API_CTX& Ctx) noexcept;
EnHttpParseResult ApiPost_Register(const API_CTX& Ctx) noexcept;

// Search

EnHttpParseResult ApiGet_SearchEntity(const API_CTX& Ctx) noexcept;

// Acl

EnHttpParseResult ApiPost_ModifyAccess(const API_CTX& Ctx) noexcept;
EnHttpParseResult ApiPost_ModifyAccessUser(const API_CTX& Ctx) noexcept;
EnHttpParseResult ApiGet_Acl(const API_CTX& Ctx) noexcept;