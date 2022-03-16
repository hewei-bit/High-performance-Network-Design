# 加载所有的源码，和makefile wildcard类似
AUX_SOURCE_DIRECTORY(. DIR_SRCS)

# 默认是静态库
ADD_LIBRARY (kcp_test ${DIR_SRCS})