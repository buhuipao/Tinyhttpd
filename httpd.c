/* J. David's webserver */
/* This is a simple webserver.
 * Created November 1999 by J. David Blackstone.
 * CSE 4344 (Network concepts), Prof. Zeigler
 * University of Texas at Arlington
 */
/* This program compiles for Sparc Solaris 2.6.
 * To compile for Linux:
 *  1) Comment out the #include <pthread.h> line.
 *  2) Comment out the line that defines the variable newthread.
 *  3) Comment out the two lines that run pthread_create().
 *  4) Uncomment the line that runs accept_request().
 *  5) Remove -lsocket from the Makefile.
 */
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdint.h>

#define ISspace(x) isspace((int)(x))

#define SERVER_STRING "Server: jdbhttpd/0.1.0\r\n"
#define STDIN 0
#define STDOUT 1
#define STDERR 2

void accept_request(void *);
void bad_request(int);
void cat(int, FILE *);
void cannot_execute(int);
void error_die(const char *);
void execute_cgi(int, const char *, const char *, const char *);
int get_line(int, char *, int);
void headers(int, const char *);
void not_found(int);
void serve_file(int, const char *);
int startup(u_short *);
void unimplemented(int);

/**********************************************************************/
/* A request has caused a call to accept() on the server port to
 * return.  Process the request appropriately.
 * Parameters: the socket connected to the client */
/**********************************************************************/
/* 接受请求并响应 */
void accept_request(void *arg)
{
    int client = (intptr_t)arg;
    char buf[1024];
    size_t numchars;
    char method[255];
    char url[255];
    char path[512];
    size_t i, j;
    struct stat st;
    int cgi = 0; /* becomes true if server decides this is a CGI
                       * program */
    char *query_string = NULL;

    /* 读取http请求的第一行, 即：Method，空格，URL，空格，协议版本，回车符('\r'), 换行符('\n') */
    numchars = get_line(client, buf, sizeof(buf));
    i = 0;
    j = 0;
    /* 读取请求方法, 空格为终止条件 */
    while (!ISspace(buf[i]) && (i < sizeof(method) - 1))
    {
        method[i] = buf[i];
        i++;
    }
    j = i;
    method[i] = '\0';

    /* 判断请求方法是否为"GET","POST"中一种 */
    if (strcasecmp(method, "GET") && strcasecmp(method, "POST"))
    {
        /* 返回不支持的method响应 */
        unimplemented(client);
        return;
    }

    /* 请求方法为"POST", 准备用cgi处理请求 */
    if (strcasecmp(method, "POST") == 0)
        cgi = 1;

    i = 0;
    /* 跳过所有空格 */
    while (ISspace(buf[j]) && (j < numchars))
        j++;
    /* 获取请求的URL */
    while (!ISspace(buf[j]) && (i < sizeof(url) - 1) && (j < numchars))
    {
        url[i] = buf[j];
        i++;
        j++;
    }
    url[i] = '\0';

    /* GET方法 */
    if (strcasecmp(method, "GET") == 0)
    {
        query_string = url;
        /* 取出请求方法, 如果有"?"代表有查询参数，准备用cgi程序处理 */
        while ((*query_string != '?') && (*query_string != '\0'))
            query_string++;
        if (*query_string == '?')
        {
            cgi = 1;
            *query_string = '\0';
            query_string++;
        }
    }

    /* 增加静态文件夹前缀 */
    sprintf(path, "htdocs%s", url);
    /* 如果请求的是"/"，重新定向到"/index.html" */
    if (path[strlen(path) - 1] == '/')
        strcat(path, "index.html");
    /* stat返回-1, 文件不存在 */
    if (stat(path, &st) == -1)
    {
        /* 读取完header并丢弃, 返回404*/
        while ((numchars > 0) && strcmp("\n", buf)) /* read & discard headers */
            numchars = get_line(client, buf, sizeof(buf));
        not_found(client);
    }
    else
    {
        /* 请求是一个目录就重定向至目录下的index.html文件 */
        if ((st.st_mode & S_IFMT) == S_IFDIR)
            strcat(path, "/index.html");
        /* 请求的文件权限位中有可执行位, cgi设置为1 */
        if ((st.st_mode & S_IXUSR) ||
            (st.st_mode & S_IXGRP) ||
            (st.st_mode & S_IXOTH))
            cgi = 1;
        if (!cgi) /* 不需要cgi处理 */
            serve_file(client, path);
        else
            /* 执行cgi程序 */
            execute_cgi(client, path, method, query_string);
    }

    /* 关闭连接 */
    close(client);
}

/**********************************************************************/
/* Inform the client that a request it has made has a problem.
 * Parameters: client socket */
/**********************************************************************/
/* 参数错误请求 */
void bad_request(int client)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 400 BAD REQUEST\r\n"); /* 写入状态行 */
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "\r\n"); /* 响应头部结束 */
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "<P>Your browser sent a bad request, ");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "such as a POST without a Content-Length.\r\n");
    send(client, buf, sizeof(buf), 0);
}

/**********************************************************************/
/* Put the entire contents of a file out on a socket.  This function
 * is named after the UNIX "cat" command, because it might have been
 * easier just to do something like pipe, fork, and exec("cat").
 * Parameters: the client socket descriptor
 *             FILE pointer for the file to cat */
/**********************************************************************/
/* 发送整个文件 */
void cat(int client, FILE *resource)
{
    char buf[1024];

    fgets(buf, sizeof(buf), resource);
    while (!feof(resource))
    {
        send(client, buf, strlen(buf), 0);
        fgets(buf, sizeof(buf), resource);
    }
}

/**********************************************************************/
/* Inform the client that a CGI script could not be executed.
 * Parameter: the client socket descriptor. */
/**********************************************************************/
/* 响应服务器内部错误的状态 */
void cannot_execute(int client)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 500 Internal Server Error\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<P>Error prohibited CGI execution.\r\n");
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Print out an error message with perror() (for system errors; based
 * on value of errno, which indicates system call errors) and exit the
 * program indicating an error. */
/**********************************************************************/
void error_die(const char *sc)
{
    perror(sc);
    exit(1);
}

/**********************************************************************/
/* Execute a CGI script.  Will need to set environment variables as
 * appropriate.
 * Parameters: client socket descriptor
 *             path to the CGI script */
/**********************************************************************/
/* cgi脚本处理逻辑 */
void execute_cgi(int client, const char *path,
                 const char *method, const char *query_string)
{
    char buf[1024];
    int cgi_output[2];
    int cgi_input[2];
    pid_t pid;
    int status;
    int i;
    char c;
    int numchars = 1;
    int content_length = -1;

    buf[0] = 'A';
    buf[1] = '\0';
    if (strcasecmp(method, "GET") == 0) /* 处理GET请求 */
        /* 当头部全部读取完成时 */
        while ((numchars > 0) && strcmp("\n", buf)) /* read & discard headers */
            numchars = get_line(client, buf, sizeof(buf));
    else if (strcasecmp(method, "POST") == 0) /*POST*/
    {
        numchars = get_line(client, buf, sizeof(buf));
        /* 读取丢弃完整头部，顺便得到content-length */
        while ((numchars > 0) && strcmp("\n", buf))
        {
            buf[15] = '\0'; /* 截断buf，15就是字符串"Content-Length:"的长度 */
            if (strcasecmp(buf, "Content-Length:") == 0)
                content_length = atoi(&(buf[16])); /* 转换成int */
            numchars = get_line(client, buf, sizeof(buf));
        }
        if (content_length == -1) /* 请求头未发现content-length字段, 为非法post请求 */
        {
            bad_request(client);
            return;
        }
    }
    else /*HEAD or other*/
    {
    }

    if (pipe(cgi_output) < 0) /* 创建无名管道 */
    {
        cannot_execute(client);
        return;
    }
    if (pipe(cgi_input) < 0)
    {
        cannot_execute(client);
        return;
    }

    if ((pid = fork()) < 0)
    {
        cannot_execute(client);
        return;
    }
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    send(client, buf, strlen(buf), 0);
    if (pid == 0) /* child: CGI script */
    {
        char meth_env[255];
        char query_env[255];
        char length_env[255];

        dup2(cgi_output[1], STDOUT); /* 复制cgi_output的写端为标准输出，以便之后的execl输出数据到cgi_output管道的写端, 父进程将从cgi_output的读端读到输出的数据 */
        dup2(cgi_input[0], STDIN);   /* 复制cgi_input的读端为标准输入，以便之后的execl读取父进程从cgi_input管道的写端写入的数据 */
        /* 关闭管道不需要的操作端 */
        close(cgi_output[0]);
        close(cgi_input[1]);
        sprintf(meth_env, "REQUEST_METHOD=%s", method);
        putenv(meth_env); /* 设置环境变量 */
        if (strcasecmp(method, "GET") == 0)
        {
            sprintf(query_env, "QUERY_STRING=%s", query_string);
            putenv(query_env);
        }
        else
        { /* POST */
            sprintf(length_env, "CONTENT_LENGTH=%d", content_length);
            putenv(length_env);
        }
        execl(path, NULL); /* 执行path程序, 参数由STDIN得到 */
        exit(0);
    }
    else
    { /* parent */
        close(cgi_output[1]);
        close(cgi_input[0]);
        if (strcasecmp(method, "POST") == 0)
            /* 向cgi_input管道的写端写入body数据, 子进程将在cgi_input管道的读端读到数据 */
            for (i = 0; i < content_length; i++)
            {
                recv(client, &c, 1, 0);
                write(cgi_input[1], &c, 1);
            }
        /* 从cgi_output的读端读取子进程的输出，并copy到发送缓存区 */
        while (read(cgi_output[0], &c, 1) > 0)
            send(client, &c, 1, 0);
        /* 关闭管道 */
        close(cgi_output[0]);
        close(cgi_input[1]);
        waitpid(pid, &status, 0); /* 等待子进程退出 */
    }
}

/**********************************************************************/
/* Get a line from a socket, whether the line ends in a newline,
 * carriage return, or a CRLF combination.  Terminates the string read
 * with a null character.  If no newline indicator is found before the
 * end of the buffer, the string is terminated with a null.  If any of
 * the above three line terminators is read, the last character of the
 * string will be a linefeed and the string will be terminated with a
 * null character.
 * Parameters: the socket descriptor
 *             the buffer to save the data in
 *             the size of the buffer
 * Returns: the number of bytes stored (excluding null) */
/**********************************************************************/
// 从socket中读取数据, 遇到回车'\r'将返回
int get_line(int sock, char *buf, int size)
{
    int i = 0;
    char c = '\0';
    int n;

    while ((i < size - 1) && (c != '\n'))
    {
        n = recv(sock, &c, 1, 0);
        /* DEBUG printf("%02X\n", c); */
        printf("%02X\n", c);
        if (n > 0)
        {
            if (c == '\r')
            {
                /* MSG_PEEK标志设立将导致recv函数读取sock中的数据，但不会清除已读数据，
                /* 本例中读取到'\r'后尝试读取下一个是否为'\n'(http的换行符'\r\n') */
                n = recv(sock, &c, 1, MSG_PEEK);
                /* DEBUG printf("%02X\n", c); */
                printf("%02X\n", c);
                if ((n > 0) && (c == '\n'))
                    recv(sock, &c, 1, 0);
                else
                    c = '\n';
            }
            buf[i] = c;
            i++;
        }
        else
            c = '\n';
    }
    buf[i] = '\0';

    return (i);
}

/**********************************************************************/
/* Return the informational HTTP headers about a file. */
/* Parameters: the socket to print the headers on
 *             the name of the file */
/**********************************************************************/
/* 发送完整的头部信息 */
void headers(int client, const char *filename)
{
    char buf[1024];
    (void)filename; /* could use filename to determine file type */

    strcpy(buf, "HTTP/1.0 200 OK\r\n"); /* 覆盖写入字符串 */
    send(client, buf, strlen(buf), 0);
    strcpy(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    strcpy(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Give a client a 404 not found status message. */
/**********************************************************************/
/* 404处理页面 */
void not_found(int client)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n"); /* 追加写入 */
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "your request because the resource specified\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "is unavailable or nonexistent.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Send a regular file to the client.  Use headers, and report
 * errors to client if they occur.
 * Parameters: a pointer to a file structure produced from the socket
 *              file descriptor
 *             the name of the file to serve */
/**********************************************************************/
/* 向client套接字send文件内容 */
void serve_file(int client, const char *filename)
{
    /* 定义文件指针 */
    FILE *resource = NULL;
    int numchars = 1;
    char buf[1024];

    buf[0] = 'A'; /* 联合上面的numchars=1，只是为了进入下面丢弃header的循环 */
    buf[1] = '\0';
    /* 下面循环退出的条件:
        1)get_line中recv出现出错返回返回0，buf直接为"\n";
        2)读取完头部, 遇到头部结束标志(单独一行"\r\n"), get_line返回时buf等于"\n"
    */
    while ((numchars > 0) && strcmp("\n", buf)) /* read & discard headers */
        numchars = get_line(client, buf, sizeof(buf));

    resource = fopen(filename, "r");
    /* 找不到要打开文件, 虽然之前已经用stat判断过，但还是要尝试读取下，我的理解是防止在此期间文件被删除了 */
    if (resource == NULL)
        not_found(client);
    else
    {
        headers(client, filename); /* 响应头信息 */
        cat(client, resource);
    }
    fclose(resource);
}

/**********************************************************************/
/* This function starts the process of listening for web connections
 * on a specified port.  If the port is 0, then dynamically allocate a
 * port and modify the original port variable to reflect the actual
 * port.
 * Parameters: pointer to variable containing the port to connect on
 * Returns: the socket */
/**********************************************************************/
/* 启动http服务，并返回监听socket */
int startup(u_short *port)
{
    int httpd = 0;
    int on = 1;
    struct sockaddr_in name; /* 地址 */

    httpd = socket(PF_INET, SOCK_STREAM, 0); /* 建立IPV4协议族基于流的(TCP)的socket */
    if (httpd == -1)
        error_die("socket");
    memset(&name, 0, sizeof(name));                                         /* 清零操作 */
    name.sin_family = AF_INET;                                              /* 地址族 */
    name.sin_port = htons(*port);                                           /* 转换无符号短整形的端口号为网络字节顺序(大端) */
    name.sin_addr.s_addr = htonl(INADDR_ANY);                               /* 把本机字节顺序转化为网络字节顺序(大端), 转换的是INADDR_ANY(无符号的32位0.0.0.0) */
    if ((setsockopt(httpd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))) < 0) /* 设定reuseaddr, 详见UNP */
    {
        error_die("setsockopt failed");
    }
    if (bind(httpd, (struct sockaddr *)&name, sizeof(name)) < 0) /* 绑定地址 */
        error_die("bind");
    /* 如果指定的是0端口，那么就动态获取port */
    if (*port == 0) /* if dynamically allocating a port */
    {
        socklen_t namelen = sizeof(name);
        if (getsockname(httpd, (struct sockaddr *)&name, &namelen) == -1)
            error_die("getsockname");
        *port = ntohs(name.sin_port);
    }
    if (listen(httpd, 5) < 0) /* 监听并设置backlog为5, linux下backlog是指三次握手已经完成但未被accept处理的队列长度, 详见UNP or http://zdyi.com/books/unp/s2/2.5.2.html */
        error_die("listen");
    return (httpd);
}

/**********************************************************************/
/* Inform the client that the requested web method has not been
 * implemented.
 * Parameter: the client socket */
/**********************************************************************/
/* 未实现的方法返回501, 和不允许405有区别 */
void unimplemented(int client)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</TITLE></HEAD>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/

int main(void)
{
    int server_sock = -1;
    u_short port = 4000;
    int client_sock = -1;
    struct sockaddr_in client_name; /*用来存放客户端的地址 */
    socklen_t client_name_len = sizeof(client_name);
    pthread_t newthread;

    server_sock = startup(&port);
    printf("httpd running on port %d\n", port);

    while (1)
    {
        /* 接受请求, 创建出新的套接字 */
        client_sock = accept(server_sock,
                             (struct sockaddr *)&client_name,
                             &client_name_len);
        if (client_sock == -1)
            error_die("accept");
        /* accept_request(&client_sock); */
        if (pthread_create(&newthread, NULL, (void *)accept_request, (void *)(intptr_t)client_sock) != 0) /* 创建新的线程来处理请求 */

            perror("pthread_create");
    }

    close(server_sock);

    return (0);
}
