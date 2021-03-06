#include "zookeeperutil.h"
#include "mprpcapplication.h"
#include <semaphore.h>
#include <iostream>
#include "logger.h"

// 全局的watcher观察器 zkserver给zkclient的通知（也就是回调函数）
void global_watcher(zhandle_t* zh, int type, 
                    int state, const char* path, void* watchCtx)
{
    if(type == ZOO_SESSION_EVENT) // 回调的消息类型是和会话相关的消息类型
    {
        if (state == ZOO_CONNECTED_STATE)   // zkclient和zkserver连接成功
        {
            sem_t* sem = (sem_t*)zoo_get_context(zh);   // 从指定的句柄上获取信号量
            sem_post(sem);
        }
    }
}

ZkClient::ZkClient() : m_zhandle(nullptr){

}

ZkClient::~ZkClient(){
    if(m_zhandle != nullptr){
        zookeeper_close(m_zhandle); // 关闭句柄，释放资源，类比于MySQL_Conn
    }
}

// 连接zkserver
void ZkClient::Start()
{
    std::string host = MprpcApplication::GetInstance().GetConfig().Load("zookeeperip");
    std::string port = MprpcApplication::GetInstance().GetConfig().Load("zookeeperport");
    std::string connstr = host + ":" + port;

    /*
    zookeeper_mt : 多线程版本
    zookeeper的API客户端程序提供了三个线程
    1. API调用线程——主线程（也就是调用zookeeper_init函数的过程中会开启下面两个线程）
    2. 网络IO线程 通过pthread_create创建，通过poll（客户端程序，不需要高并发）
    3. watcher回调线程 pthread_create
    */
    // zookeeper_init是异步的，调用后立即返回，也就是并不是调用了这个函数就会建立zk连接，调用返回只是创建了该句柄，分配了相应的本地资源（如内存）
    m_zhandle = zookeeper_init(connstr.c_str(), global_watcher, 30000, nullptr, nullptr, 0);
    if(nullptr == m_zhandle)
    {
        // std::cout << "zookeeper_init error!" << std::endl;
        LOG_ERR("zookeeper_init error!");
        exit(EXIT_FAILURE);
    }

    sem_t sem;
    sem_init(&sem, 0, 0);
    zoo_set_context(m_zhandle, &sem);   //给指定的文件句柄设置信号量

    sem_wait(&sem);
    // std::cout << "zookeeper_init seccess!" << std::endl;
    LOG_INFO("zookeeper_init seccess!");
}

// 在zkserver上根据指定的path创建znode节点，state=0永久性节点，state=1临时性节点
void ZkClient::Create(const char* path, const char* data, int datalen, int state){
// state : 临时性节点还是永久性节点

    char path_buffer[128];
    int bufferlen = sizeof(path_buffer);
    int flag;
    // 先判断path表示的znode节点是否存在，如果存在，就不再重复创建了
    flag = zoo_exists(m_zhandle, path, 0, nullptr);
    if(ZNONODE == flag){    // 表示path的znode节点并不存在
        // 创建指定path的znode节点了 
        flag = zoo_create(m_zhandle, path, data, datalen,
            &ZOO_OPEN_ACL_UNSAFE, state, path_buffer, bufferlen);
        if(flag == ZOK)
        {
            // std::cout << "znode create success... path: " << path << std::endl;
            LOG_INFO("znode create success... path: %s", path);
        }
        else
        {
            // std::cout << "flag" << flag << std::endl;
            // std::cout << "znode create error... path: " << path << std::endl;
            LOG_ERR("flag : %d", flag);
            LOG_ERR("znode create error... path:  %s", path);
            exit(EXIT_FAILURE);
        }
    }
}

// 根据参数指定的znode节点路径，获取znode节点的值
std::string ZkClient::GetData(const char* path){
    char buffer[64];
    int bufferlen = sizeof(buffer);
    int flag = zoo_get(m_zhandle, path, 0, buffer, &bufferlen, nullptr);
    if(flag != ZOK){
        // std::cout << "get zmode error... path: " << path << std::endl;
        LOG_ERR("get zmode error... path: %s", path);
        return "";
    }
    else
    {
        return buffer;
    }
}