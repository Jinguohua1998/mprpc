#include "rpcprovider.h"
#include "mprpcapplication.h"
#include "rpcheader.pb.h"
#include "logger.h"
#include "zookeeperutil.h"

/*
service_name => service描述
                        => service* 记录服务对象
                        method_name => method方法对象
*/

// 这里是框架提供给外部使用的，可以发布rpc方法的函数接口
void RpcProvider::NotifyService(google::protobuf::Service* service){
    ServiceInfo service_info;

    // 获取了服务对象的描述信息
    const google::protobuf::ServiceDescriptor* pserviceDesc = service->GetDescriptor();
    // 获取服务对象的名字
    std::string service_name = pserviceDesc->name();
    // 获取服务对象service的方法的数量
    int methodCnt = pserviceDesc->method_count();

    //std::cout << "service name: " << service_name << std::endl;
    LOG_INFO("service name: %s", service_name.c_str());

    for(int i = 0; i < methodCnt; ++i){
        // 获取了服务对象指定下标的服务方法的描述（抽象的描述）
        const google::protobuf::MethodDescriptor* pmethodDesc = pserviceDesc->method(i);
        std::string method_name = pmethodDesc->name();
        service_info.m_methodMap.insert({method_name, pmethodDesc});
        // std::cout << "method_name: " << method_name << std::endl;
        LOG_INFO("method name: %s", method_name.c_str());
    }
    service_info.m_service = service;
    m_serviceMap.insert({service_name, service_info});
}

// 启动rpc服务节点，开始提供rpc远程网络调用服务
void RpcProvider::Run(){
    std::string ip = MprpcApplication::GetInstance().GetConfig().Load("rpcserverip");
    uint16_t port = atoi(MprpcApplication::GetInstance().GetConfig().Load("rpcserverport").c_str());
    muduo::net::InetAddress address(ip, port);

    // 创建TcpServer对象
    muduo::net::TcpServer server(&m_eventLoop, address, "RpcProvider");
    // 绑定连接回调和消息读写回调方法 muoduo库的主要作用：分离了网络代码和业务代码
    server.setConnectionCallback(std::bind(&RpcProvider::OnConnection, this, std::placeholders::_1));
    server.setMessageCallback(std::bind(&RpcProvider::OnMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

    // 设置muduo库的线程数量
    server.setThreadNum(4);

    // 把当前rpc节点上要发布的服务全部注册到zk上面，让rpc client可以从zk上发现服务
    // session timeout时间30s zkclient 网络IO线程 1/3 * timeout 时间发送ping消息
    ZkClient zkCli;
    zkCli.Start();
    // service_name为永久性节点 method_name为临时性节点
    for(auto& sp : m_serviceMap)
    {
        // service_name /UserServce
        std::string service_path = "/" + sp.first;
        zkCli.Create(service_path.c_str(), nullptr, 0);
        for(auto& mp : sp.second.m_methodMap)
        {
            // /service_name/method_name /UserService/Login 存储当前这个rpc服务节点主机的ip和port
            std::string method_path = service_path + "/" + mp.first;
            char method_path_data[128] = {0};
            sprintf(method_path_data, "%s:%d", ip.c_str(), port);
            // ZOO_EPHEMERAL表示znode是一个临时性节点，如果zk通过心跳发现rpc节点挂了，删掉相应的临时性节点
            zkCli.Create(method_path.c_str(), method_path_data, strlen(method_path_data), ZOO_EPHEMERAL);
        }
    }

    // rpc服务端准备启动，打印信息
    std::cout << "RpcProvider start service at ip : " << ip << " port: " << port << std::endl;

    // 启动网络服务
    server.start();
    m_eventLoop.loop(); // 启动了epoll_wait以阻塞的方式等待远程的连接
}

// 新的socket的连接回调
void RpcProvider::OnConnection(const muduo::net::TcpConnectionPtr& conn ){
    if(!conn->connected())  // 如果和rpc_client网络连接断开
    {
        conn->shutdown();   // 对应的操作是关闭（close）相应的文件描述符
    }
}

/*
在框架内部，RpcProvider和RpcConsumer协商好之间通信用的protobuf数据类型
service_name method_name args   定义proto的message类型，进行数据头的序列化和反序列化
因为args的message在业务层已经序列化，所以只要进行service_name和method_name（数据头）的序列化
数据头包括了service_name method_name args_size三个字段，args_size是为了防止粘包

header_size + header_str + args_str 
header_size（4个字节）的目的是从字符流中截取出一个字符串用于header_str（区分header_str和args_str的边界）
之后将header_str用protobuf反序列化成message，其中包含service_name method_name args_size：防止TCP粘包

例如：10000用字符（文本）方式存储，“10000”占用5个字节；而10000用二进制的方式存储，占用4个字节
protobuf采用二进制的方式存储
*/
// 已建立连接用户的读写事件回调 如果远程有一个rpc服务的调用请求，那么Onmessage方法就会响应
// 实现数据的序列化和反序列化
void RpcProvider::OnMessage(const muduo::net::TcpConnectionPtr& conn, 
                            muduo::net::Buffer* buffer, 
                            muduo::Timestamp){
    // 网络上接收的rpc调用请求的字符流：包含rpc方法的名字，参数
    std::string recv_buf = buffer->retrieveAllAsString();   // 将网络接收到的字符流全部转换为string

    // 从字符流中读取到前4个字节的内容
    uint32_t header_size = 0;
    recv_buf.copy((char*)&header_size, 4, 0);

    // 根据header_size读取数据头的原始字符流，反序列化数据，得到rpc请求的详细信息
    std::string rpc_header_str = recv_buf.substr(4, header_size);
    mprpc::RpcHeader rpcHeader;
    std::string service_name;
    std::string method_name;
    uint32_t args_size;
    if(rpcHeader.ParseFromString(rpc_header_str)){
        // 数据头反序列化成功
        service_name = rpcHeader.service_name();
        method_name = rpcHeader.method_name();
        args_size = rpcHeader.args_size();

    } else {
        // 数据头反序列化失败
        std::cout << "rpc_header_str: " << rpc_header_str << " parse error!" << std::endl;
        return ;
    }

    // 获取rpc方法参数的字符流数据
    std::string args_str = recv_buf.substr( 4 + header_size, args_size);

    // 打印调试信息
    std::cout << "========================================="<< std::endl;
    std::cout << "header_size: " << header_size << std::endl;
    std::cout << "rpc_header_str: " << rpc_header_str << std::endl;
    std::cout << "service_name: " << service_name << std::endl;
    std::cout << "method_name: " << method_name << std::endl;
    std::cout << "args_str: " << args_str << std::endl;
    std::cout << "========================================="<< std::endl;

    // 获取service对象和method对象
    auto it = m_serviceMap.find(service_name);
    if(it == m_serviceMap.end()){
        std::cout << service_name << "does not exist" << std::endl;
        return;
    }
    auto mit = it->second.m_methodMap.find(method_name);
    if(mit == it->second.m_methodMap.end()){
        std::cout << service_name << ":" << method_name << "does not exist" << std::endl;
        return;
    }
    google::protobuf::Service* service = it->second.m_service;  // 获取service对象 例如UserService
    const google::protobuf::MethodDescriptor* method = mit->second;   // 获取method对象

    // 生成rpc方法调用的请求request和响应response参数
    google::protobuf::Message* request = service->GetRequestPrototype(method).New();
    if(!request->ParseFromString(args_str)){
        std::cout << "request parse error, content: " << args_str << std::endl;
        return;
    }
    google::protobuf::Message* response = service->GetResponsePrototype(method).New();
    
    // 给下面的method方法的调用，绑定一个Closure的回调函数
    // NewCallback有很多重载版本，都是返回Closure*，可以传入C函数指针以及函数参数或者c++类指针，类成员函数指针以及函数参数
    google::protobuf::Closure* done = 
        google::protobuf::NewCallback<RpcProvider, const muduo::net::TcpConnectionPtr&, google::protobuf::Message*>
        (this, &RpcProvider::SendRpcResponse, conn, response);
    
    // 在框架上根据远端rpc请求，调用当前rpc节点上发布的方法
    // 具体到业务层 new UserService().Login(controller, request, response, done)
    // 在框架上不能调用具体的类的对象及其方法，只能通过动态绑定操作基类指针Service* , Method*等等
    service->CallMethod(method, nullptr, request, response, done);
}   

// Closure的回调操作，用于序列化rpc的响应和网络发送
void RpcProvider::SendRpcResponse(const muduo::net::TcpConnectionPtr& conn, google::protobuf::Message* response){
    std::string response_str;
    if(response->SerializeToString(&response_str)){
        // 序列化成功后，通过网络把rpc方法执行的结果发送回rpc的调用方
        conn->send(response_str);
    } else {
        std::cout << "serialize response_str error! " << std::endl;
    }
    conn->shutdown();   // 模拟http的短链接服务，由rpcprovider主动断开连接
}
