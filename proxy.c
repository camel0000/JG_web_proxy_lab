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
    char buf1[MAXLINE], buf2[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    rio_t rio_client, rio;

    proxyfd = Open_clientfd(host, port);                    // new socket openning for proxy to server 'tiny' connection
    
    Rio_readinitb(&rio_client, fd);                         // 클라이언트->proxy 버퍼에 연결 fd 연결
    Rio_readinitb(&rio, proxyfd);                           // proxy->server 버퍼에 proxy fd 연결

    // *** request 전체 내용 읽어들이기
    if (!Rio_readlineb(&rio_client, buf1, MAXLINE))
        return;
    
    printf("%s", buf1);
    sscanf(buf1, "%s %s %s", method, uri, version);

    if (strcasecmp(method, "GET")) {                        // method가 GET이 아니면 -> 501 ERROR
        clienterror(fd, method, "501", "Not Implemented", "Tiny does not implement this method");
        return;
    }

    read_requesthdrs(&rio_client);                                 // GET 요청이면 읽어들이고, 다른 요청이면 무시
    
    // *** request를 파싱 (-> static or dynamic 결정) => sequential에선 필요 X
    /*
    * pass on implementing sequential proxy server
    */

    // *** client가 유효한 HTTP 요청을 보냈는지 확인 => 무조건 유효하다는 가정하에 유효 O일 때의 작업 코드화
    // *** 유효 X -> error msg (유효하지 않음)
    // *** 유효 O -> connection of proxy to web server(tiny) 자체적 설정 후, 
    // client object를 요청 (to server)
    


    // *** server로부터의 response를 읽어들이기
    if (!Rio_readlineb(&rio, buf2, MAXLINE))
        return;

    printf("%s", buf2);
    sscanf(buf2, "%s %s %s", method, uri, version);

    // *** 읽어들인 response를 client에 전달(forward the content of response to the client)
    // serve_static, serve_dynamic에서의 작업과 유사하게 작동해야 함
    // serve(fd, filename, filesize, cgiargs, method);
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