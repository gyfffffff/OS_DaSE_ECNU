/*Author: gyf10215501422*/
/*date: 2023.3.3*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>   /*under Linux*/
#include <sys/types.h>  /*system calls*/
#include <string.h>    
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

/*under Minix*/
#include <minix/com.h>
#include <minix/config.h>
#include <minix/type.h>
#include <minix/endpoint.h>
#include <minix/const.h>
#include <minix/u64.h>
#include <paths.h>
#include <minix/procfs.h>

#define true 1   
#define MAXLINE 1024 /* max size of cmdline */
#define MAXPARA 32  /*max number of parameters*/
#define HISTORY_N  100 /*max number of history cmd*/ /*dynamic malloc*/
#define MEMOINFO_LEN 5
#define BUILDIN_CMD 1
#define REDIRECT_in_CMD 2
#define REDIRECT_out_CMD 3
#define PIPE_CMD 4
#define PROGRAM_CMD 5
#define STD_INPUT 0 /* file descriptor for standard input */
#define STD_OUTPUT 1 /* file descriptor for standard output */
const char *cputimenames[] = { "user", "ipc", "kernelcall" };
#define CPUTIMENAMES (sizeof(cputimenames)/sizeof(cputimenames[0]))
#define PROC_NAME_LEN   16
#define PSINFO_VERSION	0
#define  SLOT_NR(e) (_ENDPOINT_P(e) + nr_tasks)
#define MAX_NR_TASKS	1023
#define _ENDPOINT_GENERATION_SIZE (MAX_NR_TASKS+_MAX_MAGIC_PROC+1)
#define _ENDPOINT_P(e) \
	((((e)+MAX_NR_TASKS) % _ENDPOINT_GENERATION_SIZE) - MAX_NR_TASKS)
#define SELF	((endpoint_t) 0x8ace) 	/* used to indicate 'own process' */
#define _MAX_MAGIC_PROC (SELF)
#define PATH_MAX	1024
/* Process types. */
#define TYPE_TASK	'T'
#define TYPE_SYSTEM	'S'
#define TYPE_USER	'U'
#define _PATH_PROC	"/proc/"
#define ORDER_CPU	0
#define ORDER_MEMORY	1
#define CPUTIME(m, i) (m & (1L << (i)))
#define  SLOT_NR(e) (_ENDPOINT_P(e) + nr_tasks)
#define  USED		0x1
#define  IS_TASK	0x2
#define  IS_SYSTEM	0x4
#define  BLOCKED	0x8

typedef int endpoint_t;			/* process identifier */
typedef	unsigned long long int u64_t;
typedef long unsigned int vir_bytes;	/* virtual addresses/lengths in bytes */

/*global variables*/
int nr_total;
int order = ORDER_CPU;
unsigned int nr_procs, nr_tasks;

char prompt[] = "(myshell)# ";
char** history_cmd;  /*keep all cmdlines*/
int h = 0; /*number of history cmds*/
int all_his_num = 0;
unsigned int nr_procs, nr_tasks;

void print_prompt();
void read_command(char* commandline);
int parse(char commandline[], char** cmd, char** parameters, int* argc);
void exec(char* commandline);
int buildin_cmd(char* cmd, char** parameters, int argc);
void exec_cd(char** parameters, int argc);
void print_history(int argc, char* parameters[MAXPARA]);
void exec_mytop();
void parse_memoinfo(int* content);
void print_meminfo();
void callRedirect_out(char* commandline);
void callRedirect_in(char* commandline);
void parse_program(char* input, char** cmd, char** parameter_list);
void pipeline(char* commamdline);
void program(char* comandline);

struct tp
{
    struct proc *p;
    u64_t ticks;
};
struct proc {
	int p_flags;
	endpoint_t p_endpoint;
	pid_t p_pid;
	u64_t p_cpucycles[CPUTIMENAMES];
	int p_priority;
	endpoint_t p_blocked;
	time_t p_user_time;
	vir_bytes p_memory;
	uid_t p_effuid;
	int p_nice;
	char p_name[PROC_NAME_LEN+1];
};

struct proc *proc = NULL, *prev_proc = NULL;

int main(){
    char* commandline;
    history_cmd = (char**)malloc(HISTORY_N*sizeof(char*));
    while(true){
        printf("%s", prompt);  /*print prompt*/
        char* tem = (char*)malloc(MAXLINE);
        read_command(tem);
        commandline = tem;
        exec(commandline);  /*execute cmdline*/
    }
}

void read_command(char* tem){
    fgets(tem, MAXLINE, stdin);
    tem[strlen(tem)-1] = '\0';  /*fgets 获取的输入结尾会包含\n*/
    if(h==HISTORY_N){
        h = 0;
        if((history_cmd = (char**)realloc(history_cmd[h], HISTORY_N*sizeof(char*)))==NULL){
            printf("malloc failed!");
            exit(1);
        }    
    }
    history_cmd[h] = (char*)malloc(MAXLINE*sizeof(char));
    history_cmd[h++] = tem;
    all_his_num += 1;
    return ;
}

void exec(char* commandline){
    char* cmd;
    char* parameters[MAXPARA];
    int argc;
    int bg = parse(commandline, &cmd, parameters, &argc);
    if(bg == 0){  /*foreground*/
        int cmd_type = buildin_cmd(cmd, parameters, argc);
        if(cmd_type == BUILDIN_CMD){
            return ;
        }else if(cmd_type == REDIRECT_in_CMD){
            callRedirect_in(commandline);
        }else if(cmd_type == REDIRECT_out_CMD){
            callRedirect_out(commandline);
        }else if(cmd_type == PIPE_CMD){
            pipeline(commandline);
        }else if(cmd_type == PROGRAM_CMD){
            program(commandline);
        }
    }else{  /*background*/ 
        if(fork() == 0){
            char* cmdline = (char*)malloc(MAXLINE*sizeof(char));
            cmdline = strncpy(cmdline, commandline, strlen(commandline)-1);
            signal(SIGCHLD, SIG_IGN); // let parent ignore SIGCHLD
            int a = open("/dev/null", O_RDONLY);
    	    dup2(a, STD_INPUT);
            dup2(a, STD_OUTPUT);
            dup2(a, 2);  // 防止输出警告
            execvp(cmd, parameters);
            exit(0);
         }else{
         	// exit(0);
         }
         return ;
    }
}

int parse(char commandline[], char** cmd, char** parameters, int* argc){
    int bg = 0;   /*bg=0: foreground; bg=1: background*/
    char* p;
    int i = 0;
    char buf[MAXLINE];
    strcpy(buf, commandline);
    p = strtok(buf, " ");  /*partition with space*/
    if(p){
        *cmd = p;
        parameters[i++] = p;
    }
    while((p = strtok(NULL, " "))){
        parameters[i++] = p;
    }
    if(strcmp(parameters[i-1], "&")==0){
        bg = 1;
    }
    parameters[i] = NULL; 
    *argc = i;
    return bg;
}

int buildin_cmd(char* cmd, char** parameters, int argc){
    int type = BUILDIN_CMD;
    for(int i=0; i< argc; i++){  // 检查是否是Program cmd, 重定向或管道
    	for(int j=0; j<strlen(parameters[i]); j++){
        	if(parameters[i][j] == '<'){
            		type = REDIRECT_in_CMD;
            		return type;
        	}else if(parameters[i][j] == '>'){
            		type = REDIRECT_out_CMD;
            		return type;
        	}else if(parameters[i][j] == '|' ){
            		type = PIPE_CMD;
            		return type;
        	}   	
    	}
    }
    type = BUILDIN_CMD;
    if(strcmp(cmd, "cd")==0){
        exec_cd(parameters, argc);
    }else if(strcmp(cmd, "history")==0){
        print_history(argc, parameters);
    }else if(strcmp(cmd, "pwd")==0){
        printf("%s\n", getcwd(NULL, 0));
    }else if(strcmp(cmd, "exit")==0){
        exit(0);
    }else if(strcmp(cmd, "mytop")==0){
        exec_mytop();
    }else{
        type = PROGRAM_CMD;
    }
    return type;
}


void exec_cd(char** parameters, int argc){
    if(argc != 2){  /*cd 的参数一般只有一个路径*/
        printf("illegal parameter.\n");
    }else{  /*参数数量正确*/
        if(chdir(parameters[1]) < 0){  /*判断是否切换目录成功，不成功返回-1*/
            printf("can't cd to %s\n", parameters[0]);
        }
    }
}

void print_history(int argc, char* parameters[MAXPARA]){
    if(argc == 1){
        for(int i =0; history_cmd[i]!=NULL; i++){
            printf("%d  %s\n", i+1, history_cmd[i]);
        }        
    }else if(argc > 1){
        int n = atoi(parameters[1]);  /*参数是char*类型*/
        if(n > all_his_num) n= all_his_num;
        for(int i =n-1; i >=0; i--){
            printf("%d  %s\n", i+1, history_cmd[i]);
        }        
    }
}

void parse_file(pid_t pid)
{
	char path[PATH_MAX], name[256], type, state;
	int version, endpt, effuid;
	unsigned long cycles_hi, cycles_lo;
	FILE *fp;
	struct proc *p;
	int slot;
	int i;

	sprintf(path, "%d/psinfo", pid);

	if ((fp = fopen(path, "r")) == NULL)
		return;

	if (fscanf(fp, "%d", &version) != 1) {
		fclose(fp);
		return;
	}

	if (version != PSINFO_VERSION) {
		fputs("procfs version mismatch!\n", stderr);
		exit(1);
	}

	if (fscanf(fp, " %c %d", &type, &endpt) != 2) {
		fclose(fp);
		return;
	}

	slot = SLOT_NR(endpt);

	if(slot < 0 || slot >= nr_total) {
		// fprintf(stderr, "top: unreasonable endpoint number %d\n", endpt);
		fclose(fp);
		return;
	}

	p = &proc[slot];

	if (type == TYPE_TASK)
		p->p_flags |= IS_TASK;
	else if (type == TYPE_SYSTEM)
		p->p_flags |= IS_SYSTEM;

	p->p_endpoint = endpt;
	p->p_pid = pid;

	if (fscanf(fp, " %255s %c %d %d %lu %*u %lu %lu",
		name, &state, &p->p_blocked, &p->p_priority,
		&p->p_user_time, &cycles_hi, &cycles_lo) != 7) {

		fclose(fp);
		return;
	}

	strncpy(p->p_name, name, sizeof(p->p_name)-1);
	p->p_name[sizeof(p->p_name)-1] = 0;

	if (state != STATE_RUN)
		p->p_flags |= BLOCKED;
	p->p_cpucycles[0] = make64(cycles_lo, cycles_hi);
	p->p_memory = 0L;

	if (!(p->p_flags & IS_TASK)) {
		int j;
		if ((j=fscanf(fp, " %lu %*u %*u %*c %*d %*u %u %*u %d %*c %*d %*u",
			&p->p_memory, &effuid, &p->p_nice)) != 3) {

			fclose(fp);
			return;
		}

		p->p_effuid = effuid;
	} else p->p_effuid = 0;

	for(i = 1; i < CPUTIMENAMES; i++) {
		if(fscanf(fp, " %lu %lu",
			&cycles_hi, &cycles_lo) == 2) {
			p->p_cpucycles[i] = make64(cycles_lo, cycles_hi);
		} else	{
			p->p_cpucycles[i] = 0;
		}
	}

	if ((p->p_flags & IS_TASK)) {
		if(fscanf(fp, " %lu", &p->p_memory) != 1) {
			p->p_memory = 0;
		}
	}

	p->p_flags |= USED;

	fclose(fp);
}

void parse_dir(void)
{
	DIR *p_dir;
	struct dirent *p_ent;
	pid_t pid;
	char *end;

	if ((p_dir = opendir(".")) == NULL) {
		perror("opendir on" _PATH_PROC);
		exit(1);
	}

	for (p_ent = readdir(p_dir); p_ent != NULL; p_ent = readdir(p_dir)) {
		pid = strtol(p_ent->d_name, &end, 10);

		if (!end[0] && pid != 0)
			parse_file(pid);
	}

	closedir(p_dir);
}

int print_memory(void)
{
	FILE *fp;
	unsigned int pagesize;
	unsigned long total, free, largest, cached;

	if ((fp = fopen("meminfo", "r")) == NULL)
		return 0;

	if (fscanf(fp, "%u %lu %lu %lu %lu", &pagesize, &total, &free,
			&largest, &cached) != 5) {
		fclose(fp);
		return 0;
	}

	fclose(fp);

	printf("main memory: %ldK total, %ldK free, %ldK contig free, "
		"%ldK cached\n",
		(pagesize * total)/1024, (pagesize * free)/1024,
		(pagesize * largest)/1024, (pagesize * cached)/1024);

	return 1;
}
void get_procs(void)
{
	struct proc *p;
	int i;

	p = prev_proc;
	prev_proc = proc;
	proc = p;

	if (proc == NULL) {
		proc = malloc(nr_total * sizeof(proc[0]));

		if (proc == NULL) {
			fprintf(stderr, "Out of memory!\n");
			exit(1);
		}
	}

	for (i = 0; i < nr_total; i++)
		proc[i].p_flags = 0;

	parse_dir();
}
u64_t cputicks(struct proc *p1, struct proc *p2, int timemode)
{
	int i;
	u64_t t = 0;
	for(i = 0; i < CPUTIMENAMES; i++) {
		if(!CPUTIME(timemode, i))
			continue;
		if(p1->p_endpoint == p2->p_endpoint) {
			t = t + p2->p_cpucycles[i] - p1->p_cpucycles[i];
		} else {
			t = t + p2->p_cpucycles[i];
		}
	}

	return t;
}

char *ordername(int orderno)
{
	switch(orderno) {
		case ORDER_CPU: return "cpu";
		case ORDER_MEMORY: return "memory";
	}
	return "invalid order";
}
/**
 * 如果进程的USED标志位未被标记，则跳过该进程.
 * 对于不同类型的进程
 * 分别统计它们的CPU时间和对应的CPU状态的时间。
 */
void print_procs(struct proc *proc1, struct proc *proc2, int cputimemode)
{
    int p, nprocs;
    u64_t systemticks = 0;
    u64_t userticks = 0;
    u64_t total_ticks = 0;
    u64_t idleticks = 0;
    u64_t kernelticks = 0;
    int blockedseen = 0;
    static struct tp *tick_procs = NULL;
    if (tick_procs == NULL)
    {
        tick_procs = malloc(nr_total * sizeof(tick_procs[0]));
        if (tick_procs == NULL)
        {
            fprintf(stderr, "Out of memory!\n");
            exit(1);
        }
    }
    for (p = nprocs = 0; p < nr_total; p++)
    {
        u64_t uticks;
        if (!(proc2[p].p_flags & USED))
        { // 查看USED位是否被标记
            continue;
        }
        tick_procs[nprocs].p = proc2 + p;// 初始化tickprocs的第np个结构体的struct proc *p为proc2第p个文件的struct proc
        tick_procs[nprocs].ticks = cputicks(&proc1[p], &proc2[p], cputimemode);
        uticks = cputicks(&proc1[p], &proc2[p], 1);
        total_ticks = total_ticks + uticks;
        if (p - NR_TASKS == IDLE)
        {
            idleticks = uticks;
            continue;
        }
        if (p - NR_TASKS == KERNEL)
        {
            kernelticks = uticks;
        }
        if (!(proc2[p].p_flags & IS_TASK))
        {
            if (proc2[p].p_flags & IS_SYSTEM)
            {
                systemticks = systemticks + tick_procs[nprocs].ticks;
            }
            else
            {
                userticks = userticks + tick_procs[nprocs].ticks;
            }
        }
        nprocs++;
    }
    if (total_ticks == 0)
    {
        return;
    }
    printf("CPU states: %6.2f%% user, ", 100.0 * userticks / total_ticks);
    printf("%6.2f%% system, ", 100.0 * systemticks / total_ticks);
    printf("%6.2f%% kernel, ", 100.0 * kernelticks / total_ticks);
    printf("%6.2f%% idle\n", 100.0 * idleticks / total_ticks);
}




void getkinfo(void)
{
	FILE *fp;

	if ((fp = fopen("kinfo", "r")) == NULL) {
		fprintf(stderr, "opening " _PATH_PROC "kinfo failed\n");
		exit(1);
	}

	if (fscanf(fp, "%u %u", &nr_procs, &nr_tasks) != 2) {
		fprintf(stderr, "reading from " _PATH_PROC "kinfo failed\n");
		exit(1);
	}

	fclose(fp);

	nr_total = (int) (nr_procs + nr_tasks);
}

void sigwinch(int sig) { }
void exec_mytop(){
    if (chdir(_PATH_PROC) != 0) {
		    perror("chdir to " _PATH_PROC);
		    return ;
    }
    int cputimemode = 1;
    getkinfo();
    signal(SIGWINCH, sigwinch);
    print_memory();
    get_procs();
    if(prev_proc == NULL){
        get_procs();
    }  
    print_procs(prev_proc, proc, cputimemode);
}

// void parse_memoinfo(int* content){
//     int f_meminfo;   /*./proc/meminfo 文件编号*/
//     if((f_meminfo = open("./proc/meminfo", O_RDONLY))<0){  /*打开meminfo， 失败则返回-1*/
//         printf("open /proc/meminfo failed.");
//     }
//     char buf[64];  /*保存读取的内容*/
//     char* tem = (char*)malloc(10);  /*临时保存读取的内容，每遇到一个空格就转化成数字并清空*/
//     int t = 0;  /*指示tem 实际使用大小*/
//     int c = 0;  /*指示content 实际使用大小*/
//     while(read(f_meminfo, buf, 1)){   /*每次读一个字符*/
//         tem[t++] = *buf;  /*存入临时文件*/
//         if(buf[0] == ' '){  /*如果读到空格就将tem里的内容转成整型并清空*/
//             content[c++] = atoi(tem);
//             t = 0;
//             free(tem);
//         }
//     }
//     content[c++] = atoi(tem);  /*while 结束后还有最后一条记录就在tem里*/
//     if(close(f_meminfo)<0){  /*关闭文件*/
//         printf("close file failed.");
//     }    
// }

/*自己实现打印内存信息*/
// void print_meminfo(){
//     int content[MEMOINFO_LEN];
//     parse_memoinfo(content);  /*获取了meminfo 的内容*/
//     int pagesize = content[0];
//     int total = content[1];
//     int free = content[2];
//     int largest = content[3];
//     int cached = content[4];
//     int t_m = (pagesize*total)/1024;
//     int f_m = (pagesize*free)/1024;
//     int l_m = (pagesize*largest)/1024;
//     int c_m = (pagesize*cached)/1024;
//     printf("Main memory: ");
//     printf("%dk total, %dk free, %dk contig free, %dk cached", t_m, f_m, l_m, c_m);
// }

void callRedirect_out(char* commandline){
    // 先确定input 和 output
    int index = strchr(commandline, '>') - commandline + 1; // 得到'>'在commandline里出现的位置
    /*确定命令行中，输入，输出部分的长度*/
    int len_cmdline = strlen(commandline);
    int len_input = index - 1;
    int len_output = len_cmdline - index;

    /*获取输入输出*/
    char* output = (char*)malloc(1024*sizeof(char));
    char* input = (char*)malloc(1024*sizeof(char));
    int offset = index;
    if(commandline[index] == ' '){
        offset+=1;
    }
    strncpy(output, commandline+offset, len_output);
    offset = len_input;
    if(*(commandline+len_cmdline) == ' '){
        offset-=1;
    }
    strncpy(input, commandline, offset);
    output[len_output] = '\0';
    input[offset] = '\0';



    /*对输出，判断输出部分是否有效*/
    int fp = open(output, O_WRONLY, 6666);
    if(fp < 0){
        printf("File open failed: %s\n", output);
        return;
    }
    /*解析input的cmd, parameter_list*/
    char* cmd;
    char* parameter_list[MAXPARA];
    char* p;
    int i = 0;
    p = strtok(input, " ");  /*partition with space*/
    if(p){
        cmd = p;
        parameter_list[i++] = p;
    }
    while((p = strtok(NULL, " "))){
        parameter_list[i++] = p;
    }
    /*注意： 为了之后使用execvp，parameter_list要以NULL结尾*/
    parameter_list[i] = NULL;
    /*修改输出位置*/
    int output_old = dup(STD_OUTPUT);
    dup2(fp, STD_OUTPUT);
    /*执行命令*/
    if(fork() == 0){
    	execvp(cmd, parameter_list);
    	exit(0);
    }else{
   	wait(NULL);
   	close(fp);
    	dup2(output_old, STD_OUTPUT);
    	close(output_old);
    } 
}

/*重定向输入和重定向输出非常相似，代码变化很小*/
void callRedirect_in(char* commandline){
    // 先确定input 和 output
    int index = strchr(commandline, '<') - commandline + 1; // 得到'<'在commandline里出现的位置
    /*确定命令行中，输入，输出部分的长度*/
    int len_cmdline = strlen(commandline);
    int len_output = index - 1;
    int len_input = len_cmdline - index;

    /*获取输入输出*/
    char* output = (char*)malloc(1024*sizeof(char));
    char* input = (char*)malloc(1024*sizeof(char));
    int offset = index;
    if(commandline[index] == ' '){
        offset+=1;
    }
    strncpy(input, commandline+offset, len_input);
    offset = len_output;
    if(*(commandline+len_cmdline) == ' '){
        offset-=1;
    }
    strncpy(output, commandline, offset);
    input[len_input] = '\0';
    output[offset] = '\0';



    /*对输入文件，判断输出部分是否有效*/
    int fp = open(input, O_RDONLY, 6666);  // 只写变只读
    if(fp < 0){
        printf("File open failed: %s\n", input);
        return;
    }
    /*解析output的cmd, parameter_list*/
    char* cmd;
    char* parameter_list[MAXPARA];
    char* p;
    int i = 0;
    p = strtok(output, " ");  /*partition with space*/
    if(p){
        cmd = p;
        parameter_list[i++] = p;
    }
    while((p = strtok(NULL, " "))){
        parameter_list[i++] = p;
    }
    // printf("para[0]:%s\n", parameter_list[0]);
    /*注意： 为了之后使用execvp，parameter_list要以NULL结尾*/
    parameter_list[i] = NULL;
    /*修改输出位置*/
    int input_old = dup(STD_INPUT);
    dup2(fp, STD_INPUT); 
    /*执行命令*/
    pid_t pid = fork();
    if(pid == 0){
        /*子进程执行program命令*/
        execvp(cmd, parameter_list);
        exit(0);
    }else{
        wait(NULL);
        close(fp);
        dup2(input_old, STD_INPUT);
        close(input_old);
    }
}
void parse_program(char* input, char** cmd, char** parameter_list){
    int i = 0;
    char* p;
    p = strtok(input, " ");  /*partition with space*/
    if(p){
        *cmd = p;
        parameter_list[i++] = p;
    }
    while((p = strtok(NULL, " "))){
        parameter_list[i++] = p;
    }
    /*注意： 为了之后使用execvp，parameter_list要以NULL结尾*/
    parameter_list[i] = NULL;  
}
void pipeline(char* commamdline){
    /*解析命令行，找到输入输出*/
    /*确定管道符号的位置*/
    int index = strchr(commamdline, '|') - commamdline + 1;
    char input[1024];
    char output[1024];
    int input_len = index-1;
    if(commamdline[input_len] == ' '){
        input_len-= 1;
    }
    strncpy(input, commamdline, input_len);
    input[input_len] = '\0';
    int output_len = strlen(commamdline) - index;
    int offset = index;
    if(commamdline[offset] == ' '){
        offset += 1;
        output_len -= 1;
    }
    strncpy(output, commamdline+offset, output_len);
    output[output_len] = '\0';
    /*从input， output 中，解析出execvp 的参数：cmd， parameter_list*/
    char* cmd_in;
    char* cmd_out;
    char* para_list_in[MAXPARA];
    char* para_list_out[MAXPARA];
    parse_program(input, &cmd_in, para_list_in);
    parse_program(output, &cmd_out, para_list_out);
    /*一个进程写，一个进程读，由此实现管道*/
    int fd[2];  /*fd[0]读， fd[1]写*/
    pipe(&fd[0]);  /*创建一个管道*/
    if(fork() == 0){  /*avoid exiting directly*/
        if(fork()!=0){
            /*parent process 关闭读*/
            close(fd[0]);   /*执行输入部分，不需要从管道读*/
            close(STD_OUTPUT);  /*为新的标准输出让位*/
            dup(fd[1]); /*dup2(fd[1], STD_OUTPUT)*/
            close(fd[1]);  /*fd[1]不会再用到*/
            execvp(cmd_in, para_list_in);
            wait(NULL);
        }else{
            /*child process 关闭写*/
            close(fd[1]);  /*从管道中读，不需要写*/
            close(STD_INPUT); /*为新的标准输入让位*/
            dup(fd[0]);  /*dup2(fd[0], STD_INPUT)*/
            close(fd[0]); /*fd[0]不会再用到*/
            execvp(cmd_out, para_list_out);
            exit(0);
        }   	
    }else{
    	wait(NULL);
    }
}
void program(char* commandline){
    char buf[MAXLINE];
    strcpy(buf, commandline);
    char* cmd;
    char* parameter_list[MAXPARA];
    parse_program(buf, &cmd ,parameter_list);
    if(fork() == 0){
        if(execvp(cmd, parameter_list) < 0){
            printf("execute %s failed.\n", cmd);
        }  
        exit(0); 
    }else{
        wait(NULL);
    }
}
