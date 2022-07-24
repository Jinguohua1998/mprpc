aux_source_directory(. SRC_LIST)
add_library(mprpc ${SRC_LIST})  # 因为mprpc依赖的muduo库是静态库，所以mprpc也必须是静态库，不能是动态库
target_link_libraries(mprpc muduo_net muduo_base pthread zookeeper_mt)