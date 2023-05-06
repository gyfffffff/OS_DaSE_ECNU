#include"pm.h"  // 主头文件，包含了每个目标模块都需要的位于/usr/include及其子目录下的所以系统级头文件
#include<minix/syslib.h>  // 包含所需C库函数原型，
#include<minix/callnr.h>  // 定义了系统调用编号
#include<sys/wait.h>  // wait, waitpid这两个系统调用需要用到的宏定义
#include <signal.h>  // 定义了标准信号名
#include<minix/com.h>  // 定义了在服务器和设备驱动程序间通信所用到的常用定义、 定义了如何构造消息来执行notify操作
#include <minix/vm.h>  // 
#include "mproc.h"   // 包含了与进程内存分配有关的所有字段
#include <sys/ptrace.h>  // 定义了ptrace系统调用的各种可能操作
#include <sys/resource.h>  // get_nice_value 需要用到的常量
#include<minix/sched.h>  
#include <assert.h>

int do_chrt(){
    sys_chrt(who_p, m_in.m2_l1);
    return (OK);
}