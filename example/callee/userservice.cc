#include <iostream>
#include <string>
#include "user.pb.h"
#include "mprpcapplication.h"   
#include "rpcprovider.h"

/*
UserService原来是一个本地服务，提供了两个进程内的本地方法，Login和GetFriendLists
*/

class UserService  : public fixbug::UserServiceRpc  // 使用rpc服务发布端（rpc服务提供者）
{
public:
    bool Login(std::string name, std::string pwd){
        std::cout << "doing local service : Login" << std::endl;
        std::cout << "name:" << name << " pwd:" << pwd << std::endl;
        return true;
    }

    bool Register(uint32_t id, std::string name, std::string pwd){
        std::cout << "doing local service : Register" << std::endl;
        std::cout << "id:" << id << "name:" << name << " pwd:" << pwd << std::endl;
        return true;
    }

    /*
    重写基类UserServiceRpc的虚函数，下面这些方法都是框架直接调用的
    1. caller ===> Login(LoginRequest) ==> muduo => callee
    2. callee ===> Login(LoginRequest) ==> 交到下面重写的这个Login方法上了
    */
    
    // 这个Login函数是框架调用的
    void Login(::google::protobuf::RpcController* controller,
                       const ::fixbug::LoginRequest* request,
                       ::fixbug::LoginResponse* response,
                       ::google::protobuf::Closure* done)
    {
        // 框架给业务上报了请求参数LoginRequest，业务获取相应数据做本地业务
        std::string name = request->name();
        std::string pwd = request->pwd();

        // 做本地业务
        bool login_result = Login(name, pwd);

        // 把响应写入，框架创建了一个LoginResponse对象然后将指针传给Login函数，响应消息的序列化以及网络发送都是由框架来做
        // 响应包括错误码，错误消息，返回值
        fixbug::ResultCode* code = response->mutable_result();
        code->set_errcode(0);
        code->set_errmsg("");
        response->set_success(login_result);

        // 执行回调操作
        // Closure里面的Run是纯虚函数，所有它是抽象类，我们需要定义一个类继承Closure，并在类中重写Run函数
        done->Run();    // 执行响应对象数据的序列化，并通过网络发送出去（都是由框架来完成的）

    }

    void Register(google::protobuf::RpcController* controller,
                       const ::fixbug::RegisterRequest* request,
                       ::fixbug::RegisterResponse* response,
                       ::google::protobuf::Closure* done)
    {
        uint32_t id = request->id();
        std::string name = request->name();
        std::string pwd = request->pwd();

        bool ret = Register(id, name, pwd);

        response->mutable_result()->set_errcode(0);
        response->mutable_result()->set_errmsg("");
        response->set_success(ret);

        done->Run();
    }
};

int main(int argc, char** argv){
    // 调用框架的初始化操作 ：配置文件的读取（rpc服务站点的ip地址和端口号以及zookeeper的IP地址和端口号）、日志模块的读写分离，读写控制
    MprpcApplication::Init(argc, argv); 

    // provider是一个rpc网络服务对象（负责数据的序列化和反序列化以及网络数据的收发），并把UserService对象发布到rpc节点上
    RpcProvider provider;   // rpc服务对象，可能很多人同时都是请求rpc服务调用，所以要求高并发，使用muduo库
    provider.NotifyService(new UserService());

    // 启动一个rpc服务发布节点 Run以后，进程进入阻塞状态，等待远程的rpc调用请求
    provider.Run();

    return 0;
}