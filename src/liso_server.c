#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>// POSIX标准：close(), read(), write()
#include <sys/socket.h>// socket(), bind(), listen(), accept()
#include <netinet/in.h> // sockaddr_in, htons()
#include <arpa/inet.h>// inet_pton() 等IP转换函数
#include <signal.h>
#include <string.h>
#include "parse.h"
#include <sys/stat.h> // stat函数获取文件元信息
#include <time.h>
#include <fcntl.h> // 文件控制 open ORDONLY
#define ECHO_PORT 9999
#define BUF_SIZE 4096

int sock = -1, client_sock = -1;
char buf[BUF_SIZE];

int close_socket(int sock) {
    if (close(sock)) {
        fprintf(stderr, "Failed closing socket.\n");
        return 1;
    }
    return 0;
}
void handle_signal(const int sig) {
    if (sock != -1) {
        fprintf(stderr, "\nReceived signal %d. Closing socket.\n", sig);
        close_socket(sock);
    }
    exit(0);
}
void handle_sigpipe(const int sig) 
{
    if (sock != -1) {
        return;
    }
    exit(0);
}
int main(int argc, char *argv[]) {
    int sock, client_sock;
    ssize_t readret; 
//size_t = signed size_t，有符号的整数类型 readret > 0  → 成功读到了 readret 个字节
// readret == 0 → 对方关闭了连接
// readret == -1 → 出错了

    socklen_t cli_size;
// socket 地址长度类型
    struct sockaddr_in addr, cli_addr;
//     struct sockaddr_in {  IPv4 
//     sa_family_t    sin_family;   // 地址族，必须是 AF_INET socket internet a.k.a sin
//     in_port_t      sin_port;     // 端口号（网络字节序）
//     struct in_addr sin_addr;     // IP 地址
// };

// struct in_addr {
//     in_addr_t s_addr;            // 32位 IP 地址
// };
    char buf[BUF_SIZE];

    fprintf(stdout, "----- Echo Server -----\n");//等价于Printf
    
    /* all networked programs must create a socket */
    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) == -1) 
// PF_INET  → IPv4（如 192.168.1.1） 协议族
// PF_INET6 → IPv6（如 2001:db8::1）
// PF_UNIX  → 本地进程间通信
// SOCK_STREAM → TCP（可靠、有序、面向连接） 套接字类型
// SOCK_DGRAM  → UDP（不可靠、无连接）
// SOCK_RAW    → 原始套接字（直接操作IP层）
//0 系统根据前两个参数自动选择 返回一个整数（文件描述符），之后所有网络操作都通过这个 sock 来进行。如果创建失败则返回 -1
    {
        fprintf(stderr, "Failed creating socket.\n"); 
        return EXIT_FAILURE;
    }

    int opt = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
//  参数	值	含义
// 第1个	sock	哪个 socket 上设置这个选项 
// 第2个	SOL_SOCKET	在 socket 层面设置（不是 TCP 层，也不是 IP 层）
// 第3个	SO_REUSEADDR | SO_REUSEPORT	要开启的选项（用位或同时开两个） #attension
//here is a change
// 第4个	&opt	一个 int 变量的地址，值为 1 表示开启
// 第5个	sizeof(opt)	这个变量的大小
// 配置了socket选项，允许重新绑定端口和地址，防止address already in use错误
// SO_REUSEADDR允许在 socket 关闭后立即重新绑定相同地址。
// SO_REUSEPORT允许多个 socket 绑定到同一地址和端口，提高负载均衡。
        fprintf(stderr, "Failed setting socket options.\n");
        return EXIT_FAILURE;
    }
//
    addr.sin_family = AF_INET; //确定协议族 AF_INET = PF_INET
    addr.sin_port = htons(ECHO_PORT); //h = host（本机）to = 转换为n = network（网络）s = short（16位整数）把本机字节序转成网络字节序(大端 0x1234 是 12 34) 不同机器存储多字节方式不同
    addr.sin_addr.s_addr = INADDR_ANY; //绑定本机的所有网卡 IP 本机所有 IP 都接受连接

    /* servers bind sockets to ports---notify the OS they accept connections */
    if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)))
// bind(sock, addr, addrlen)
// sock： socket函数调用返回的套接字
// addr：地址结构体，调用bind之后这个地址与参数sockfd指定的套接字关联
    {
        close_socket(sock);
        fprintf(stderr, "Failed binding socket.\n");
        return EXIT_FAILURE;
    }


    if (listen(sock, 5))
// listen(sock, backlog)
// scok： socket函数调用返回的套接字 
// backlog: 指定排列在队列中的最大的连接数量,不得小于1.
    {
        close_socket(sock);
        fprintf(stderr, "Error listening on socket.\n");
        return EXIT_FAILURE;
    }

    /* finally, loop waiting for input and then write it back */
    while (1)
    {
        cli_size = sizeof(cli_addr);
        if ((client_sock = accept(sock, (struct sockaddr *) &cli_addr,
                                    &cli_size)) == -1)
        {
            close(sock);
            fprintf(stderr, "Error accepting connection.\n");
            return EXIT_FAILURE;
        }

        readret = 0;
//###区别recv和send 都是操作同一个socket 一个往流上发数据 一个取数据
        while((readret = recv(client_sock, buf, BUF_SIZE, 0)) >= 1)
//  参数	值	         含义
// 第1个	client_sock	 从哪个 socket 读数据   
// 第2个	buf	数据存到哪里
// 第3个	BUF_SIZE	最多读多少字节
// 第4个	0	额外选项，0 表示默认行为
// 第4个参数 flags 详解
// 传 0 时表示阻塞读取（没数据就等着），其他常用选项：
// 值	含义
// 0	阻塞模式，没有数据就一直等
// MSG_DONTWAIT	非阻塞，没数据立刻返回 -1 发送缓冲区满了 直接走
// MSG_PEEK	看一眼数据但不取走，下次还能读到
// MSG_WAITALL	一定要读满指定字节数才返回
//返回读取的字节数
        {
            Request* request = parse(buf,readret,client_sock);
             
            //stat的使用
            if(request == NULL){
            //请求有错误 向客户端发送400格式的响应
                sprintf(buf, "HTTP/1.1 400 Bad request\r\n\r\n");
            }
            else if(strcmp(request->http_version, "HTTP/1.1") != 0){
            //向客户端发送505格式的响应
            sprintf(buf, "HTTP/1.1 505 HTTP Version not supported\r\n\r\n");
            }else {
            struct stat file_stat;
            char full_path[4096];
            if (strcmp(request->http_uri, "/") == 0) {
                sprintf(full_path, "static_site/index.html");
            } else {
                sprintf(full_path, "static_site%s", request->http_uri);
            }
            //向客户端发送404格式的响应
            if(stat(full_path,&file_stat) < 0){
                sprintf(buf, "HTTP/1.1 404 Not Found\r\n\r\n");
            }
            
            else if(strcmp(request->http_method, "GET") == 0){
            //向客户端发送get格式的响应 
            //#a't't
            // GET: 头 + body
                    time_t now = time(NULL);
                    struct tm *t = gmtime(&now);
                    char date_buf[128];
                    strftime(date_buf, 128, "%a, %d %b %Y %H:%M:%S GMT", t);

                    struct tm *mt = gmtime(&file_stat.st_mtime);
                    char mod_buf[128];
                    strftime(mod_buf, 128, "%a, %d %b %Y %H:%M:%S GMT", mt);

                    int header_len = sprintf(buf,
                        "HTTP/1.1 200 OK\r\n"
                        "Server: liso/1.1\r\n"
                        "Date: %s\r\n"
                        "Content-Length: %ld\r\n"
                        "Content-type: text/html\r\n"
                        "Last-modified: %s\r\n"
                        "Connection: keep-alive\r\n"
                        "\r\n",
                        date_buf, file_stat.st_size, mod_buf
                    );

                    // 读文件内容追加到 buf
                    int fd = open(full_path, O_RDONLY);
                    if (fd >= 0) {
                        read(fd, buf + header_len, BUF_SIZE - header_len);
                        close(fd);
                    }
            }else if(strcmp(request->http_method, "POST")== 0){
            //向客户端发送post格式的响应
            //查找body起始位置（在\r\n\r\n之后）
            char *body_start = strstr(buf, "\r\n\r\n");
            if (body_start != NULL) {
                body_start += 4; //跳过\r\n\r\n
                int body_len = (buf + readret) - body_start;

                //先复制body到临时缓冲区（因为sprintf会覆盖buf）
                char body_buf[BUF_SIZE];
                int copy_len = body_len < BUF_SIZE ? body_len : BUF_SIZE - 1;
                memcpy(body_buf, body_start, copy_len);
                body_buf[copy_len] = '\0';

                sprintf(buf,
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: text/plain\r\n"
                    "Content-Length: %d\r\n"
                    "\r\n"
                    "%s",
                    copy_len,
                    body_buf
                );
            } else {
                sprintf(buf, "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n");
            }
            //这里原样返回
            }else if(strcmp(request->http_method, "HEAD") == 0){
            //向客户端发送head格式的响应
            time_t now = time(NULL);
                    struct tm *t = gmtime(&now);
                    char date_buf[128];
                    strftime(date_buf, 128, "%a, %d %b %Y %H:%M:%S GMT", t);
                    struct tm *mt = gmtime(&file_stat.st_mtime);
                    char mod_buf[128];
                    strftime(mod_buf, 128, "%a, %d %b %Y %H:%M:%S GMT", mt);
                    sprintf(buf,
                        "HTTP/1.1 200 OK\r\n"
                        "Server: liso/1.1\r\n"
                        "Date: %s\r\n"
                        "Content-Length: %ld\r\n"
                        "Content-type: \r\n"
                        "Last-modified: %s\r\n"
                        "Connection: keep-alive\r\n"
                        "\r\n",
                        date_buf, file_stat.st_size, mod_buf
                    );

            }else{
            //向客户端发送501格式的响应
            sprintf(buf, "HTTP/1.1 501 Not Implemented\r\n\r\n");
            }
        }

//释放request 空间
        if(request != NULL){
            if (request->headers != NULL) {
            free(request->headers);
            }
            free(request);
        }
            send(client_sock, buf, strlen(buf), 0);
            memset(buf, 0, BUF_SIZE);
        } 
        if (readret == -1)
        {
            close_socket(client_sock);
            close_socket(sock);
            fprintf(stderr, "Error reading from client socket.\n");
            return EXIT_FAILURE;
        }
//close_socket close_socket 自定义函数
        if (close_socket(client_sock))
        {
            close_socket(sock);
            fprintf(stderr, "Error closing client socket.\n");
            return EXIT_FAILURE;
        }
    }

    close_socket(sock);

    return EXIT_SUCCESS;

}
