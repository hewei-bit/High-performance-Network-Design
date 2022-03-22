# 加载所有的源码，和makefile wildcard类似
AUX_SOURCE_DIRECTORY(. DIR_SRCS)
# SET(DIR_SRCS dir1.c  dir12.c)
# 默认是静态库
#ADD_LIBRARY (dir1 SHARED  ${DIR_SRCS})
ADD_LIBRARY (client ${DIR_SRCS})