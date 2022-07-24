# set(SRC_LIST calluserservice.cc ../user.pb.cc)
set(SRC_LIST callfriendservice.cc ../friend.pb.cc)
add_executable(consumer ${SRC_LIST})
target_link_libraries(consumer mprpc protobuf)  # 依赖动态库的名字有先后顺序，因为mprpc依赖于protobuf，所以protobuf写在后面    