# include<stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <string.h>

typedef int bool;

#define false 0
#define true 1
#define maxprocessN 15  /*最大进程数*/
#define maxblocksize  (64 * 1024) /*bytes*/

int filesize = (300*1024*1024);
struct timeval starttime, endtime;
char *Diskpath[maxprocessN] = {"/usr/test/file1.txt", "/usr/test/file2.txt", "/usr/test/file3.txt", "/usr/test/file4.txt", "/usr/test/file5.txt", "/usr/test/file6.txt", "/usr/test/file7.txt", "/usr/test/file8.txt", "/usr/test/file9.txt", "/usr/test/file10.txt", "/usr/test/file11.txt", "/usr/test/file12.txt", "/usr/test/file13.txt", "/usr/test/file14.txt", "/usr/test/file15.txt"};
char *RAMpath[maxprocessN] = {"/root/myram/test/file1.txt", "/root/myram/test/file2.txt", "/root/myram/test/file3.txt", "/root/myram/test/file4.txt", "/root/myram/test/file5.txt", "/root/myram/test/file6.txt", "/root/myram/test/file7.txt", "/root/myram/test/file8.txt", "/root/myram/test/file9.txt", "/root/myram/test/file10.txt", "/root/myram/test/file11.txt", "/root/myram/test/file12.txt", "/root/myram/test/file13.txt", "/root/myram/test/file14.txt", "/root/myram/test/file15.txt"};
char writebuf[maxblocksize];

/*写文件:打开文件，判断返回值，如果正常打开文件就判断是否随机写，进行写操作*/
void write_file(int blocksize, bool isrand, char *filepath, int fs)
{
    int times = fs/blocksize;  /*计算出要写多少次*/
    // printf("times=%d\n", times);
    int fp = open(filepath, O_WRONLY|O_CREAT|O_SYNC, 755);
    if(fp<0){
        printf("open testfile failed.\n");
        return ;
    }
    // printf("open %s success!\n", filepath);
    lseek(fp, 0, SEEK_SET);
    for(int i=0; i<times; i++){
        int count = write(fp, writebuf, blocksize);
        if(count<0){
            printf("write error.\n");
            return;
        }
        if(isrand){
            lseek(fp, rand()%(fs-blocksize), SEEK_SET);  /*一开始是对fs取余，但还是有可能写出文件*/
        }
    }
}
/*读文件:打开文件，判断返回值，如果正常打开就判断是否随机读，进行读操作*/
void read_file(int blocksize, bool isrand, char *filepath, int fs)
{
    char buf[maxblocksize];
    int times = fs / blocksize; /*计算出要读多少次*/
    int fp = open(filepath, O_RDONLY|O_SYNC, 755);
    if (fp < 0){
        printf("open testfile failed.\n");
        return;
    }
    lseek(fp, 0, SEEK_SET);
    for (int i = 0; i < times; i++){
        int count = read(fp, buf, blocksize);
        if (count < 0){
            printf("read error.\n");
            return;
        }
        if (isrand){
            lseek(fp, rand() % (fs - blocksize), SEEK_SET); /*一开始是对filesize取余，但还是有可能读出文件*/
        }
    }
}

// 计算时间差，在读或写操作前后分别取系统时间，然后计算差值即为时间差，
long get_time_left(struct timeval starttime, struct timeval endtime)
{
    long spendtime;
    spendtime = (long)(endtime.tv_sec - starttime.tv_sec) * 1000 + (endtime.tv_usec - starttime.tv_usec) / 1000;
    return spendtime;
}

/*主函数: 首先创建和命名文件，通过循环执行read file和write file函数测试读写差异,
  测试blocksize和concurrency对测试读写速度的影响，最后输出结果。*/
int main()
{
    for(int i=0; i<(maxblocksize/16); i++){
        strcat(writebuf, "abcd1abcd2abcd3a");
    }
    int concurrency = 7;   /*设置并发进程数*/
    srand((unsigned)time(NULL));
    for(int blocksize=64; blocksize<=(64*1024); blocksize*=4){
        printf("============== blocksize=%d =======================\n", blocksize);
        gettimeofday(&starttime, NULL);
        // printf("get starttime=%d\n", starttime.tv_usec);
        for (int i = 0; i < concurrency; i++){
            if(fork()==0){
                // 随机写
                // write_file(blocksize, true, RAMpath[i], filesize/concurrency);
                // write_file(blocksize, true, Diskpath[i], filesize/concurrency);

                // 顺序写
                write_file(blocksize, false, RAMpath[i], filesize / concurrency); /*filesize/concurrency表示一共要读或写filesize，那么分配到每个并行进程上是多少*/
                // write_file(blocksize, false, Diskpath[i], filesize/concurrency);

                // 随机读
                // read_file(blocksize, true, RAMpath[i], filesize / concurrency);
                // read_file(blocksize, true, Diskpath[i], filesize / concurrency);

                // 顺序读
                // read_file(blocksize, false, RAMpath[i], filesize / concurrency);
                // read_file(blocksize, false, Diskpath[i], filesize/concurrency);

                exit(0);
            }
        }
        while ((wait(NULL)) >= 0); /*等待所有子进程结束*/
        gettimeofday(&endtime, NULL);
        // printf("get endtime=%d\n", endtime.tv_usec);
        long spendtime = get_time_left(starttime, endtime);
        printf("blocksize=%d B, concurrency=%d, speed=%f B/s \n", blocksize, concurrency, (double)filesize/spendtime);
    }
    return 0;
}