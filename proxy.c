#include <stdio.h>

#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

void doit(int fd);
void read_requesthdrs(rio_t *rp);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void forward_requesthdrs(rio_t *rio_client, rio_t *rio_server);
void serve(int fd, rio_t *rio_server);


int main(int argc, char **argv) {                           // 첫 매개변수 argc는 옵션의 개수, argv는 옵션 문자열의 배열
    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    /* Check command line args */
    if (argc != 2) {
	      fprintf(stderr, "usage: %s <port>\n", argv[0]);
	      exit(1);
    }

    /* Open_listenfd 함수 호출 -> 듣기 식별자 오픈, 인자를 통해 port번호 넘김 */
    listenfd = Open_listenfd(argv[1]);

    /* 무한 서버 루프 실행 */
    while (1) {
        clientlen = sizeof(clientaddr);                     // accept 함수 인자에 넣기 위한 주소 길이 계산

        /* 반복적 연결 요청 접수 */
        // Accept(듣기 식별자, 소켓 주소 구조체 주소, 해당 주소 길이)
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);

        // Getaddrinfo => 호스트 이름: 호스트 주소, 서비스 이름: 포트 번호의 스트링 표시를 소켓 주소 구조체로 변환
        // Getnameinfo => 위의 Getaddrinfo의 반대로, 소켓 주소 구조체 -> 스트링 표시로 변환
        Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);
        
        /* 트랜잭션 수행 */
        doit(connfd);
        /* 트랜잭션이 수행된 후, 자신 쪽의 연결 끝(소켓)을 닫음 */
        Close(connfd);
    }
}

void doit(int fd) {
    // doit 호출 -> main에서 연결이 되었고, request를 accept한 대로 transaction 수행을 해야한다는 것
    char host[MAXLINE] = "localhost";
    char port[MAXLINE] = "3000";
    int proxyfd;
    char buf[MAXLINE], buf2[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    rio_t rio_client, rio_server;

    // Open a new socket for the proxy to server connection
    proxyfd = Open_clientfd(host, port);
    if (proxyfd < 0) {
        fprintf(stderr, "Error: Unable to connect to server\n");
    }
    
    // Initialize rio_client and rio_server for buffered I/O
    Rio_readinitb(&rio_client, fd);
    Rio_readinitb(&rio_server, proxyfd);

    // Read the request line from the client
    if (!Rio_readlineb(&rio_client, buf, MAXLINE)) {
        Close(proxyfd);
        return;
    }
    printf("%s", buf);
    sscanf(buf, "%s %s %s", method, uri, version);

    // method가 GET이 아니면 -> 501 ERROR
    if (strcasecmp(method, "GET")) {
        clienterror(fd, method, "501", "Not Implemented", "Tiny does not implement this method");
        Close(proxyfd);
        return;
    }

    // Read the request headers from the client and forward them to the server
    read_requesthdrs(&rio_client);
    Rio_writen(proxyfd, buf, strlen(buf));
    forward_requesthdrs(&rio_client, &rio_server);

    // Forward the request body, if any, from the client to the server
    if (strcmp(version, "HTTP/1.0") && strcmp(version, "HTTP/1.1")) {
        fprintf(stderr, "Error: Invalid HTTP version\n");
        Close(proxyfd);
        return;
    }
    if (Rio_readlineb(&rio_client, buf, MAXLINE) < 0) {
        fprintf(stderr, "Error: Faild to read request body\n");
        Close(proxyfd);
        return;
    }
    while (strcmp(buf, "\r\n")) {
        printf("is the fuckin infinite loop?\n");
        Rio_writen(proxyfd, buf, strlen(buf));
        Rio_readlineb(&rio_client, buf, MAXLINE);
    }
    printf("33333333333333333333333333333\n");

    Rio_writen(proxyfd, buf, strlen(buf));

    // Forward the response from the server to the client
    serve(fd, &rio_server);

    // Close the connection to the server
    Close(proxyfd);
}

void read_requesthdrs(rio_t *rp) {
    char buf[MAXLINE];

    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);

    /* 헤더의 마지막 줄은 비어있기 때문에, buf와 개행 문자열을 비교하여 같다면 while 탈출, return void */
    while(strcmp(buf, "\r\n")) {
	      Rio_readlineb(rp, buf, MAXLINE);
	      printf("%s", buf);
    }
    return;
}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) {
    char buf[MAXLINE];

    /* Print the HTTP response headers */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n\r\n");
    Rio_writen(fd, buf, strlen(buf));

    /* Print the HTTP response body */
    sprintf(buf, "<html><title>Tiny Error</title>");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<body bgcolor=""ffffff"">\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "%s: %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<p>%s: %s\r\n", longmsg, cause);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<hr><em>The Tiny Web server</em>\r\n");
    Rio_writen(fd, buf, strlen(buf));
}

void forward_requesthdrs(rio_t *rp_client, rio_t *rp_server) {
    char buf[MAXLINE];
    Rio_readlineb(rp_client, buf, MAXLINE);
    while(strcmp(buf, "\r\n")) {
        Rio_writen(rp_server->rio_fd, buf, strlen(buf));
        Rio_readlineb(rp_client, buf, MAXLINE);
    }
    Rio_writen(rp_server->rio_fd, buf, strlen(buf));
}

void serve(int fd, rio_t *rp_server) {
    char buf[MAXLINE];
    int n;
    while((n = Rio_readlineb(rp_server, buf, MAXLINE)) != 0) {
        Rio_writen(fd, buf, n);
        printf("Proxy received %d bytes from server\n", n);
    }
}