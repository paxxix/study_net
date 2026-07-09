#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <string.h>
#include "parse.h"
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
//ssize_t = signed size_t，有符号的整数类型 readret > 0  → 成功读到了 readret 个字节
// readret == 0 → 对方关闭了连接
// readret == -1 → 出错了

    socklen_t cli_size;
// socket 地址长度类型
    struct sockaddr_in addr, cli_addr;
    char buf[BUF_SIZE];

    fprintf(stdout, "----- Echo Server -----\n");
    
    /* all networked programs must create a socket */
    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) == -1) 
//PF_INET  → IPv4（如 192.168.1.1） 协议族
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
// 第3个	###SO_REUSEADDR | SO_REUSEPORT	要开启的选项（用位或同时开两个）
// 第4个	&opt	一个 int 变量的地址，值为 1 表示开启
// 第5个	sizeof(opt)	这个变量的大小
// 配置了socket选项，允许重新绑定端口和地址，防止address already in use错误
// SO_REUSEADDR允许在 socket 关闭后立即重新绑定相同地址。
// SO_REUSEPORT允许多个 socket 绑定到同一地址和端口，提高负载均衡。
        fprintf(stderr, "Failed setting socket options.\n");
        return EXIT_FAILURE;
    }
//#####
    addr.sin_family = AF_INET; //确定协议族 AF_INET = PF_INET
    addr.sin_port = htons(ECHO_PORT); //h = host（本机）to = 转换为n = network（网络）s = short（16位整数）把本机字节序转成网络字节序 不同机器存储多字节方式不同
    addr.sin_addr.s_addr = INADDR_ANY; //绑定本机的所有网卡 IP 本机所有 IP 都接受连接

    /* servers bind sockets to ports---notify the OS they accept connections */
    if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)))
// bind(sock, addr, addrlen) 不返回0就是有问题
// sock： socket函数调用返回的套接字
// addr：地址结构体，调用bind之后这个地址与参数sockfd指定的套接字关联
// addrlen: addr的长度
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
            if (send(client_sock, buf, readret, 0) != readret)
            {
                close_socket(client_sock);
                close_socket(sock);
                fprintf(stderr, "Error sending to client.\n");
                return EXIT_FAILURE;
            }
            memset(buf, 0, BUF_SIZE);
        } 

        if (readret == -1)
        {
            close_socket(client_sock);
            close_socket(sock);
            fprintf(stderr, "Error reading from client socket.\n");
            return EXIT_FAILURE;
        }
//close_socket
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
