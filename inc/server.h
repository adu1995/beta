#ifndef SERVER_H_
#define SERVER_H_
#include <stdbool.h>
#define EVENTSIZE 256
#define MSGSIZE 1024
#define LEN 64
/* status */
#define LOGIN_ASK 1
#define LOGIN_OK 2
#define LOGIN_USER 3
#define LOGIN_PWD 4
#define LOGIN_ERR 5
//1
#define RGTER_ASK 6
#define RGTER_OK 7
#define RGTER_USER 8
#define RGTER_PWD 9
#define RGTER_ERR 10
//2
#define VIP_ON 11
#define VIP_OFF 12
#define VIP_ASK 13

//3
#define SHOW_ASK 14
#define SHOW_END 15
#define SHOW_ERR 16
#define SHOW_ON 17
//4
#define FIND_ASK 18
#define FIND_END 19
#define FIND_ERR 20
#define FIND_ON 21
#define FIND_NULL 22
//5
#define DOWNLOAD_ASK 23
#define DOWNLOAD_END 24
#define DOWNLOAD_ERR 25
#define DOWNLOAD_ON 26
#define DOWNLOAD_NULL 27
//6
#define UPLOAD_ASK 28
#define UPLOAD_END 29
#define UPLOAD_ERR 30
#define UPLOAD_ON 31

/* 存储账户信息 */
struct accountMsg
{
    int choice;
    int vip;
    char username[LEN];
    char password[LEN];
};

/* 存储信息 */
struct fileMsg
{
    int status;
    long filesize;
    char filename[LEN];
    char wordContent[MSGSIZE]; 
};


/* * * * * * * * * * * * * * * * * * * * * * *
 * 实现用户登录与注册的函数 _Login_Part.c    *
 * * * * * * * * * * * * * * * * * * * * * * */

/* 功能: 创建服务器套接字serverfd
 * 前提: sockfd为服务器套接字,addr为服务器套接字信息结构体,len为addr的大小
 * 结果: 失败返回false，成功则创建服务器套接字
 */
bool createSockfd(int *sockfd, void *addr, socklen_t *len); 

/* 功能: 处理客户端登录或注册
 * 前提: sockfd为服务器套接字
 * 结果: 登录失败断开TCP连接，登录成功将客户端套接字加入epoll事件队列
 */
bool login(const int sockfd);



/* * * * * * * * * * * * * * * * * * * * * * * *
 * 实现文件管理主要功能的函数 _Function_Part.c *
 * * * * * * * * * * * * * * * * * * * * * * * */
/* 功能：显示文件服务器可下载的所有文件信息 */

/* 功能：显示文件服务器可下载的所有文件信息 */
/* 显示格式：文件名 大小 最新修改时间      */
int showAllResource(const int *sockfd);

/* 功能：按文件名查找文件服务器是否存在该文件 */
int fileNameFind(const int *sockfd, const struct fileMsg *ptr);

/* 功能: 根据文件名下载文件 */
int downLoadFile(const int *sockfd, const struct fileMsg *ptr);

/* 功能: 上传文件 */
int upLoadFile(int *sockfd, const char *filename);

/* 功能：非VIP用户成为VIP用户
 * 结果：成为VIP用户，但是需要重新登录
 */
int toBeVip(int *sockfd, const char *ptr);

/* 功能：将一个事件从epoll_ctl()句柄中删除*/
bool removeUser(int *sockfd);

#endif
