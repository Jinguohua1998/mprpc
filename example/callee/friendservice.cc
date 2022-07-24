#include <iostream>
#include <string>
#include "friend.pb.h"
#include "mprpcapplication.h"   
#include "rpcprovider.h"
#include <vector>

class FriendService : public fixbug::FriendServiceRpc{
public:
    std::vector<std::string> GetFriendList(uint32_t userid){
        std::cout << "do GetFriendsList service! userid :"  << userid << std::endl;
        std::vector<std::string> vec;
        vec.push_back("gao yang");
        vec.push_back("zhang san");
        vec.push_back("li si");
        return vec;
    }

    // 重写基类方法
    void GetFriendsList(google::protobuf::RpcController* controller,
        const ::fixbug::GetFriendsListRequest* request,
        ::fixbug::GetFriendsListResponse* response,
        ::google::protobuf::Closure* done)
    {
        uint32_t userid = request->userid();
        std::vector<std::string> friendList = GetFriendList(userid);
        response->mutable_result()->set_errcode(0);
        response->mutable_result()->set_errmsg("");
        for(std::string& name : friendList){
            std::string* p = response->add_friends();
            *p = name;
        }
        done->Run();
    }
};

int main(int argc, char** argv){

    // 调用框架的初始化操作 ：配置文件的读取（rpc服务站点的ip地址和端口号以及zookeeper的IP地址和端口号）、日志模块的读写分离，读写控制
    MprpcApplication::Init(argc, argv); 

    // provider是一个rpc网络服务对象（负责数据的序列化和反序列化以及网络数据的收发），并把UserService对象发布到rpc节点上
    RpcProvider provider;   // rpc服务对象，可能很多人同时都是请求rpc服务调用，所以要求高并发，使用muduo库
    provider.NotifyService(new FriendService());

    // 启动一个rpc服务发布节点 Run以后，进程进入阻塞状态，等待远程的rpc调用请求
    provider.Run();

    return 0;
}

