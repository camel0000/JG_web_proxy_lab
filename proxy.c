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
    char *host = "localhost";
    char *port = "5684";
    int socket_fd;
    char proxy_buf[MAXLINE], server_buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    rio_t rio_client, rio_server;

    // Open a new socket for the proxy to server connection
    socket_fd = Open_clientfd(host, port);
    
    // Initialize rio_client and rio_server for buffered I/O
    Rio_readinitb(&rio_client, fd);
    Rio_readinitb(&rio_server, socket_fd);

    Rio_readlineb(&rio_client, proxy_buf, MAXLINE);
    printf("Request headers:\n");
    printf("%s", proxy_buf);
    sscanf(proxy_buf, "%s %s %s", method, uri, version);
    Rio_writen(socket_fd, proxy_buf, strlen(proxy_buf));
    
    while (strcmp(proxy_buf, "\r\n")) {
        Rio_readlineb(&rio_client, proxy_buf, MAXLINE);
        printf("%s", proxy_buf);
        Rio_writen(socket_fd, proxy_buf, strlen(proxy_buf));
    }

    while (Rio_readlineb(&rio_server, server_buf, MAXLINE) != 0) {
        printf("%s\n", server_buf);
        Rio_writen(fd, server_buf, strlen(server_buf));
    }
}