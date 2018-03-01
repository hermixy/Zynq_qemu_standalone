

#****************************common config*******************
CC = gcc  
C++ = g++  
LINK_C = gcc
LINK_C++ = g++  

#库文件
LIBS =  
#头文件
INCLUDES =

#编译器选项
#if TARGET is .so file, must add -fPIC option  
CCFLAGS = -g -Wall
C++FLAGS = -g -Wall

#源文件
FILES_raw_socket   = ./raw_socket.c

#***************************************OBJ,LINKOBJ .etc**************
#中间目标文件

raw_socket_OBJ     = raw_socket.o 
raw_socket_LINKOBJ = raw_socket.o 
raw_socket_TARGET  = raw_socket

OBJFILE    =  $(raw_socket_OBJ) 
#可执行文件
TARGET = $(raw_socket_TARGET) 

#***************************************command***********************
.PHONY:all
all:$(TARGET) 
#清除
.PHONY:clean
clean:
	rm -rf $(TARGET) $(OBJFILE) *.d  *.d.*

#**********************************compile obj file*********************************

#---------------raw_socket------------
#------------
$(raw_socket_TARGET): $(raw_socket_OBJ)
	$(LINK_C) $(raw_socket_LINKOBJ) -o raw_socket 
raw_socket.o: $(FILES_raw_socket)
	$(CC) -c $(FILES_raw_socket) -o raw_socket.o $(CCFLAGS)

