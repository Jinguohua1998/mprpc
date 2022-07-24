# set(SRC_LIST userservice.cc ../user.pb.cc)
set(SRC_LIST friendservice.cc ../friend.pb.cc)
add_executable(provider ${SRC_LIST})
target_link_libraries(provider mprpc protobuf)  # 依赖动态库的名字有先后顺序，因为mprpc依赖于protobuf，所以protobuf写在后面    