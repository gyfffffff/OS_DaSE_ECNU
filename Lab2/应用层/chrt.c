#include <lib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdlib.h>

/*
 * 设置alarm，
 * 记录进程终止时间赋值给deadline，
 * 并将deadline 通过_syscall 传到服务层
 * */
int chrt(deadline)
long deadline; /*模仿Minix源码风格*/
{
    // struct timeval tv;
    message m;
    memset(&m, 0, sizeof(m));

    alarm((unsigned int)deadline); /*设置alarm*/

    gettimeofday(&tv, NULL); /*记录当前时间*/
    deadline += tv.tv_sec        /*计算deadline*/

    m.m2_l1 = deadline; /*保存deadline*/

    return (_syscall(PM_PROC_NR, PM_CHRT, &m));
}
