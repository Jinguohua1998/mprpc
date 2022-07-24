#include "test.pb.h"
#include <iostream>
#include <string>

using namespace fixbug;

int main1()
{
    // 封装了login请求对象的数据
    fixbug::LoginRequest req;
    req.set_name("zh");
    req.set_pwd("12");

    // 对象数据序列化
    // 序列化：将对象转换成字符流或者字节流
    std::string send_str;
    if(req.SerializeToString(&send_str)){
        std::cout << send_str << std::endl;
        std::cout << send_str.c_str() << std::endl;
        std::cout << send_str.length() << std::endl;
    }

    // 从send_str反序列化一个login请求对象
    LoginRequest reqB;
    if(reqB.ParseFromString(send_str)){
        std::cout << reqB.name() << std::endl;
        std::cout << reqB.pwd() << std::endl;

    }

    return 0;

}

int main2()
{
    // LoginResponse rsp;
    // ResultCode* rc = rsp.mutable_result();  // 改变protobuf对象中的protoduf对象
    // rc->set_errcode(1);
    // rc->set_errmsg("登录处理失败");

    fixbug::GetFriendListsResponse rsp;
    ResultCode* rc = rsp.mutable_result();  // 改变protobuf对象中的protoduf对象，返回的是该对象的指针，必须要加mutable，否则返回的是一个常量引用，不能修改
    rc->set_errcode(0);

    fixbug::User* user1 = rsp.add_friend_list();  // 返回一个在列表中新增对象的指针
    user1->set_name("zhang san");
    user1->set_age(20);
    user1->set_sex(fixbug::User::MAN);

    fixbug::User* user2 = rsp.add_friend_list();  // 返回一个在列表中新增对象的指针
    user2->set_name("li si");
    user2->set_age(20);
    user2->set_sex(fixbug::User::MAN);

    std::cout << rsp.friend_list_size() << std::endl;

    // 列表的对象的读取
    // rsp.friend_list(index)

    return 0;

}

