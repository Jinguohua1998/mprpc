#pragma once
#include <google/protobuf/service.h>
#include <string>

// 用于在caller端的RpcChannel类中调用CallMethod方法过程中遇到错误时进行控制
// 也就是通过Controller携带调用过程中的错误信息
class MprpcController : public google::protobuf::RpcController
{
public:
    MprpcController();
    void Reset();
    bool Failed() const;
    std::string ErrorText() const;
    void SetFailed(const std::string& reason);

    // 目前未实现具体的功能
    void StartCancel();
    bool IsCanceled() const;
    void NotifyOnCancel(google::protobuf::Closure* callback);

private:
    bool m_failed;  // RPC方法执行过程中的状态
    std::string m_errText;  // RPC方法执行过程中的错误信息
};