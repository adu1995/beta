#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sqlite3.h>
#include <pthread.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include "server.h"

/* 外部变量 */
extern struct epoll_event retEvent[EVENTSIZE];
extern struct epoll_event addEvent;
extern int epfd;
extern int sameNameFile;

/* 内部函数
 */
static int logInCallBack(void *arg, int n, char **val, char **key);//数据库sqlite3_exec()的回调函数
static int userNameExistent(const char *user);// 判断用户是否存在
static int updateMessage(const char *user);
static int fileIsExistent(const char *ptr);//查询库中文件是否存在
static int userIsVip(const char *user);

/* 功能：显示文件服务器可下载的所有文件信息 */
int showAllResource(const int *sockfd)
{
	int retValue;
    DIR *dirp;
    struct fileMsg statusMsg;
    memset(&statusMsg, 0, sizeof(statusMsg));

    struct dirent *dirInfo = NULL;
    struct stat fileInfo;
	struct tm *ptime;
	char timebuf[LEN] = {0};
    char objBuf[300] = {0};

    //open directory
    dirp = opendir("./res");
    if(NULL == dirp){

		statusMsg.status = SHOW_ERR;
		send(*sockfd, &statusMsg, sizeof(statusMsg), 0);
        puts("SHOWALL: opendir error!");
        return -1;//open dir error
    }
    while(1){
		//readdir()
        dirInfo = readdir(dirp);
        if(NULL == dirInfo){
            break;
        }
		//stat()
		sprintf(objBuf, "./res/%s", dirInfo->d_name);
		retValue = stat(objBuf, &fileInfo);
		/*
		if(-1 == retValue){
			statusMsg.status = SHOW_ERR;
			send(*sockfd, &statusMsg, sizeof(statusMsg), 0);
			puts("SHOWALL:stat file error!");
			closedir(dirp);
			return -1;
		}
		*/
		// get file 
		// time if last access
		ptime = localtime(&(fileInfo.st_atime));
		if(0 == strncmp(dirInfo->d_name, ".", 1)){
			continue;
		}
		//format
		//year/month/day h:m
		sprintf(timebuf, "%d/%d/%d %d:%d", ptime->tm_year + 1900, \
				ptime->tm_mon + 1, ptime->tm_mday, ptime->tm_hour, ptime->tm_min);
		//print format:
		//filename size date
		sprintf(statusMsg.wordContent, "%-15s  %-15ld  %-s", dirInfo->d_name, fileInfo.st_size, timebuf);
		
		
		//send()
		statusMsg.status = SHOW_ON;
		send(*sockfd, &statusMsg, sizeof(statusMsg), 0);
		memset(&statusMsg, 0, sizeof(statusMsg));
    }

	statusMsg.status = SHOW_END;
	send(*sockfd, &statusMsg, sizeof(statusMsg), 0);
    closedir(dirp);
	return 0;
}


/* 功能：按文件名查找文件服务器是否存在该文件 */
int fileNameFind(const int *sockfd, const struct fileMsg *ptr)
{
	int retValue;
    DIR *dirp;
    struct fileMsg statusMsg;
    memset(&statusMsg, 0, sizeof(statusMsg));

    struct dirent *dirInfo = NULL;
    struct stat fileInfo;
	struct tm *ptime;
	char timebuf[LEN] = {0};
	char objBuf[300] = {0};

	int fileLen = strlen(ptr->filename);
	int flags = 0;
    
    //open directory
    dirp = opendir("./res");
    if(NULL == dirp){
		statusMsg.status = FIND_ERR;
		send(*sockfd, &statusMsg, sizeof(statusMsg), 0);
        puts("FINDFILE: opendir error!");
        return -1;//open dir error
    }
    while(1){
		//readdir()
        dirInfo = readdir(dirp);
        if(NULL == dirInfo){
            break;
        }
		//stat()
		sprintf(objBuf, "./res/%s", dirInfo->d_name);
		retValue = stat(objBuf, &fileInfo);
		/*
		if(-1 == retValue){
			statusMsg.status = FIND_ERR;
			send(*sockfd, &statusMsg, sizeof(statusMsg), 0);
			puts("FINDFILE:stat file error!");
			closedir(dirp);
			return -1;
		}
		*/
		if(0 == strncmp(dirInfo->d_name, ptr->filename, fileLen)){
			// get file 
			// time if last access
			ptime = localtime(&(fileInfo.st_atime));
			//printf
			//year/month/day h:m
			sprintf(timebuf, "%d/%d/%d %d:%d", ptime->tm_year + 1900, \
					ptime->tm_mon + 1, ptime->tm_mday, ptime->tm_hour, ptime->tm_min);
			//filename size date
			sprintf(statusMsg.wordContent, "%-15s  %-15ld  %-s", dirInfo->d_name, fileInfo.st_size, timebuf);

			//send()
			statusMsg.status = FIND_ON;
			send(*sockfd, &statusMsg, sizeof(statusMsg), 0);
			memset(&statusMsg, 0, sizeof(statusMsg));
			flags = 1;
		}
		else{
			continue;
		}
    }
	if(0 == flags){
		statusMsg.status = FIND_NULL;
		send(*sockfd, &statusMsg, sizeof(statusMsg), 0);
		closedir(dirp);
		return 0;
	}
	statusMsg.status = FIND_END;
	send(*sockfd, &statusMsg, sizeof(statusMsg), 0);
	closedir(dirp);
	return 0;
}

/* 功能: 根据文件名下载文件 */
int downLoadFile(const int *sockfd, const struct fileMsg *ptr)
{
	int retValue = 0;
	int fd = -1;
	int vipState = 0;
	char pathname[256] = {0};
	char objBuf[300] = {0};
    DIR *dirp;
    struct fileMsg statusMsg;
    memset(&statusMsg, 0, sizeof(statusMsg));
	//file io
    struct dirent *dirInfo = NULL;
    struct stat fileInfo;
	//file SIZE
	int fileLen = strlen(ptr->filename);
    //judge is user a VIP ?
	vipState = userIsVip(ptr->wordContent);
    //open directory
    dirp = opendir("./res");
    if(NULL == dirp){
		statusMsg.status = DOWNLOAD_ERR;
		send(*sockfd, &statusMsg, sizeof(statusMsg), 0);
        puts("DOWNLOAD: opendir error!");
        return -1;//open dir error
    }
    while(1){
		//readdir()
        dirInfo = readdir(dirp);
        if(NULL == dirInfo){
            break;
        }
		//stat()
		sprintf(objBuf, "./res/%s", dirInfo->d_name);
		retValue = stat(objBuf, &fileInfo);
		/*
		if(-1 == retValue){
			statusMsg.status = DOWNLOAD_ERR;
			send(*sockfd, &statusMsg, sizeof(statusMsg), 0);
			puts("DOWNLOAD:stat file error!");
			closedir(dirp);
			return -1;
		}
		*/
		if(0 == strncmp(dirInfo->d_name, ptr->filename, fileLen)){
			sprintf(pathname, "./res/%s", ptr->filename);
			fd = open(pathname, O_RDONLY);
			if(-1 == fd){
				statusMsg.status = DOWNLOAD_ERR;
				send(*sockfd, &statusMsg, sizeof(statusMsg), 0);
				puts("DOWNLOAD: open file error!");
				closedir(dirp);
				return -1;//open dir error
			}
			while(true){
				retValue = read(fd, statusMsg.wordContent, MSGSIZE);
				if(0 == retValue){
					statusMsg.status = DOWNLOAD_END;
					send(*sockfd, &statusMsg, sizeof(statusMsg), 0);
					puts("DOWNLOAD:send end");
					close(fd);
					closedir(dirp);
					return 0;
				}
				else if(retValue < 0){
					statusMsg.status = DOWNLOAD_ERR;
					send(*sockfd, &statusMsg, sizeof(statusMsg), 0);
					puts("DOWNLOAD:read error");
					close(fd);
					closedir(dirp);
					return -1;
				}
				statusMsg.status = DOWNLOAD_ON;
				statusMsg.filesize = fileInfo.st_size;
				send(*sockfd, &statusMsg, sizeof(statusMsg), 0);
				//如果用户不是VIP，下载速度会变慢...
				if(1 == vipState){
					sleep(0.5);
				}
				memset(&statusMsg, 0, sizeof(statusMsg));
			}
		}
		else{
			continue;
		}
    }

	statusMsg.status = DOWNLOAD_NULL;
	send(*sockfd, &statusMsg, sizeof(statusMsg), 0);
	closedir(dirp);
	return 0;
}

/* 处理客户端上传的文件 */
int upLoadFile(int *sockfd, const char *filename)
{
	int retValue;
	int fd;
	char pathBuf[LEN] = {0};
	struct fileMsg statusMsg;
	memset(&statusMsg, 0, sizeof(statusMsg));
	
	//判断库文件是否有与client上传的文件同名的
	//如果有，就给上传文件名后加上数字sameNameFile
	//以此区分，并且不覆盖原来的文件
	retValue = fileIsExistent(filename);
	if(1 == retValue){
		sprintf(pathBuf, "./res/%s%d", filename, ++sameNameFile);
	}
	else{
		//format file path name
		sprintf(pathBuf, "./res/%s", filename);
	}
	//open file
	fd = open(pathBuf, O_RDWR | O_CREAT | O_APPEND, 0666);
	if(-1 == fd){
		statusMsg.status = UPLOAD_ERR;
		send(*sockfd, &statusMsg, sizeof(statusMsg), 0);
		perror("UPLOAD:open");
		puts("UPLOAD:open file error");
		return -1;
	}

	while(1){
		retValue = recv(*sockfd, &statusMsg, sizeof(statusMsg), 0);
		if(-1 == retValue){
			perror("UPLOAD:1st recv error!");
			close(fd);
			removeUser(sockfd);
			return -1;
		}
		else if(0 == retValue){
			perror("UPLOAD:2st recv error");
			puts("UPLOAD:1st Connect Error");
			close(fd);
			removeUser(sockfd);
			return -1;
		}
		//
		if(UPLOAD_ON == statusMsg.status){
			retValue = write(fd, statusMsg.wordContent, MSGSIZE);
			if(retValue < 0){
				statusMsg.status = UPLOAD_ERR;
				send(*sockfd, &statusMsg, sizeof(statusMsg), 0);
				puts("UPLOAD:write error!");
				return -1;
			}
			memset(&statusMsg, 0, sizeof(statusMsg));
		}
		else if(UPLOAD_END == statusMsg.status){
			printf("Uploading recv completes\n");
			close(fd);
			return 0;
		}
		else{
			close(fd);
			return -1;
		}
	}	
}

bool removeUser(int *sockfd)
{
	int retValue;
    retValue = epoll_ctl(epfd, EPOLL_CTL_DEL, *sockfd, NULL);
	close(*sockfd);
    if(retValue == -1){
		perror("REMOVE:epoll_ctl delete error!");
		return false;
    }
	return true;
}

/* 功能：非VIP用户成为VIP用户
 * 结果：成为VIP用户，但是需要重新登录
 */
int toBeVip(int *sockfd, const char *ptr)
{
	int retValue;
	struct fileMsg statusMsg;
	memset(&statusMsg, 0, sizeof(statusMsg));
	
	
	retValue = userNameExistent(ptr);
	if(0 == retValue){
		statusMsg.status = VIP_OFF;
		send(*sockfd, &statusMsg, sizeof(statusMsg), 0);
		return 0;
	}

	retValue = updateMessage(ptr);
	if(-1 == retValue){
		statusMsg.status = VIP_OFF;
		send(*sockfd, &statusMsg, sizeof(statusMsg), 0);
		return 0;
	}

	statusMsg.status = VIP_ON;
	send(*sockfd, &statusMsg, sizeof(statusMsg), 0);
	//removeUser(sockfd);
	return 0;
}


/* 0表示用户不存在 1表示用户存在 */
static int userNameExistent(const char *user)
{
	sqlite3 *db = NULL;
	int retValue;//return value
	char *errmsg;//error number
	
	int result = 0;
	//sql语句
	char sql[256] = {0};
	while(true){
		retValue = sqlite3_open("./db/message.db", &db);
		if(retValue == -1){
			puts("VIPGET:sqlite3_open ERROR!");
			continue;
		}
		else{
			break;
		}
	}
	
	sprintf(sql, "select *from account where usr=\"%s\";", user);
	puts(sql);//test

	if(0 != sqlite3_exec(db, sql, logInCallBack, (void *)&result, &errmsg)){
		puts("VIPGET:search failure");
		printf("error=%s\n",errmsg);
		sqlite3_close(db);
		return -1;
	}
	//judge 
	if(0 == result){
		sqlite3_close(db);
		return 0;
	}
	else{
		sqlite3_close(db);
		return 1;
	}
}

/* -1 Failure 0 success */
static int updateMessage(const char *user)
{
	sqlite3 *db = NULL;
	int retValue;//return value
	char *errmsg;//error number
	
	int result = 0;
	//sql语句
	char sql[256] = {0};
	while(true){
		retValue = sqlite3_open("./db/message.db", &db);
		if(retValue == -1){
			puts("vipget: sqlite3_open ERROR!");
			continue;
		}
		else{
			break;
		}
	}
	
	sprintf(sql, "update account set vip=1 where usr=\"%s\";", user);

	if(0 != sqlite3_exec(db, sql, NULL, NULL, &errmsg)){
		puts("vipget:update failure");
		printf("error=%s\n",errmsg);
		sqlite3_close(db);
		return -1;
	}

	sqlite3_close(db);
	return 0;
}

static int logInCallBack(void *arg, int n, char **val, char **key)
{
	*(int *)arg = n;

	return 0;
}


//file weither existent
static int fileIsExistent(const char *ptr)
{
	int retValue;
    DIR *dirp;
    struct dirent *dirInfo = NULL;
    
    //open directory
    dirp = opendir("./res");
    if(NULL == dirp){
        puts("Upload fileFind: opendir error!");
        return -1;//open dir error
    }
    while(1){
		//readdir()
        dirInfo = readdir(dirp);
        if(NULL == dirInfo){
            break;
        }
		if(0 == strcmp(dirInfo->d_name, ptr)){
			return 1;
		}
    }
	return 0;
}

/* -1表示错误 0表示是VIP 1表示不是VIP */
static int userIsVip(const char *user)
{	
	sqlite3 *db = NULL;
	int retValue;//return value
	char *errmsg;//error number
	
	int result = 0;
	//sql语句
	char sql[256] = {0};
	while(true){
		retValue = sqlite3_open("./db/message.db", &db);
		if(retValue == -1){
			puts("server.c:2#sqlite3_open ERROR!");
			continue;
		}
		else{
			break;
		}
	}
	
	sprintf(sql, "select *from account where usr=\"%s\" and vip=1;", user);

	if(0 != sqlite3_exec(db, sql, logInCallBack, (void *)&result, &errmsg)){
		puts("server.c:3#select failure");
		printf("error=%s\n",errmsg);
		sqlite3_close(db);
		return -1;
	}

	if(result == 0){
		sqlite3_close(db);
		return 1;//bushi VIP
	}

	sqlite3_close(db);
	return 0;//shi VIP
}

