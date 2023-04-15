/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the 
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh 
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

// ./tiny 5684
int main(int argc, char **argv) {   // 첫 매개변수 argc는 옵션의 개수, argv는 옵션 문자열의 배열
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
	      clientlen = sizeof(clientaddr);                             // accept 함수 인자에 넣기 위한 주소 길이 계산

        /* 반복적 연결 요청 접수 */
        // Accept(듣기 식별자, 소켓 주소 구조체 주소, 해당 주소 길이)
	      connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);   //line:netp:tiny:accept

        // Getaddrinfo => 호스트 이름: 호스트 주소, 서비스 이름: 포트 번호의 스트링 표시를 소켓 주소 구조체로 변환
        // Getnameinfo => 위의 Getaddrinfo의 반대로, 소켓 주소 구조체 -> 스트링 표시로 변환
        Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);
        
        /* 트랜잭션 수행 */
	      doit(connfd);                                               //line:netp:tiny:doit
	      /* 트랜잭션이 수행된 후, 자신 쪽의 연결 끝(소켓)을 닫음*/
        Close(connfd);                                              //line:netp:tiny:close
    }
}
/* $end tinymain */

/*
 * doit - handle one HTTP request/response transaction
 * 하나의 HTTP 트랜잭션을 처리
 */
/* $begin doit */
void doit(int fd) {
    int is_static;
    struct stat sbuf;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], cgiargs[MAXLINE];
    rio_t rio;    // rio_readlineb를 위해 rio_t 타입(구조체)의 읽기 버퍼 선언

    /* Read request line and headers */
    /* Rio = Robust I/O */
    Rio_readinitb(&rio, fd);                              // &rio 주소를 가지는 읽기 버퍼와 식별자 connfd 연결

    if (!Rio_readlineb(&rio, buf, MAXLINE))               //line:netp:doit:readrequest
        return;
    
    printf("%s", buf);                                    // "GET / HTTP/1.1 "
    sscanf(buf, "%s %s %s", method, uri, version);        //line:netp:doit:parserequest   // 버퍼에서 자료형 읽고, 분석

    if (strcasecmp(method, "GET")) {                      //line:netp:doit:beginrequesterr
        clienterror(fd, method, "501", "Not Implemented", "Tiny does not implement this method");
        return;
    }                                                     //line:netp:doit:endrequesterr

    /* GET method라면 읽어들이고, 다른 요청 헤더 무시 */
    read_requesthdrs(&rio);                               //line:netp:doit:readrequesthdrs

    /* Parse URI from GET request */
    /* URI를 filename과 비어 있을 수도 있는 CGI 인자 스트링으로 분석 -> 요청이 정적 또는 동적 컨텐츠를 위한 것인지 나타내는 플래그 설정 */
    is_static = parse_uri(uri, filename, cgiargs);        //line:netp:doit:staticcheck
    if (stat(filename, &sbuf) < 0) {                      //line:netp:doit:beginnotfound
	      clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
	      return;
    }                                                     //line:netp:doit:endnotfound

    /* Serve static content */
    if (is_static) {
        // S_ISREG: 일반 파일인지? || S_IRUSR: 읽기 권한이 있는지?
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) { //line:netp:doit:readable
            // 권한 없음 -> 클라이언트에 에러 전달
            clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
            return;
        }
        // 일반 파일이며, 권한 있음 -> 파일 제공
	      serve_static(fd, filename, sbuf.st_size);         //line:netp:doit:servestatic
    }
    /* Serve dynamic content */
    else {
        // S_ISREG: 일반 파일인지? || S_IXUSR: 실행 권한이 있는지?
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) { //line:netp:doit:executable
            // 실행 불가 -> 클라이언트에 에러 전달
            clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
            return;
        }
        // 일반 파일이며, 실행 가능 -> 파일 제공
        serve_dynamic(fd, filename, cgiargs);             //line:netp:doit:servedynamic
    }
}
/* $end doit */

/*
 * read_requesthdrs - read HTTP request headers
 * Tiny는 요청 헤더 내의 어떠한 정보도 사용하지 않고, 단순히 읽고 무시
 */
/* $begin read_requesthdrs */
void read_requesthdrs(rio_t *rp) {
    char buf[MAXLINE];

    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);

    /* 헤더의 마지막 줄은 비어있기 때문에, buf와 개행 문자열을 비교하여 같다면 while 탈출, return void */
    while(strcmp(buf, "\r\n")) {          //line:netp:readhdrs:checkterm
	      Rio_readlineb(rp, buf, MAXLINE);
	      printf("%s", buf);
    }
    return;
}
/* $end read_requesthdrs */

/*
 * parse_uri - parse URI into filename and CGI args
 *             return 0 if dynamic content, 1 if static
 */
/* $begin parse_uri */
int parse_uri(char *uri, char *filename, char *cgiargs) {
    char *ptr;

    /* cgi-bin이 들어있는지 확인 -> 양수 값 리턴 시, dynamic content 요구 -> 조건문 탈출 */
    /* Static content */
    if (!strstr(uri, "cgi-bin")) {                        //line:netp:parseuri:isstatic
        strcpy(cgiargs, "");                              //line:netp:parseuri:clearcgi
        strcpy(filename, ".");                            //line:netp:parseuri:beginconvert1
        strcat(filename, uri);                            //line:netp:parseuri:endconvert1

        // uri 문자열 끝이 /일 경우, home.html을 filename에 붙임
        if (uri[strlen(uri) - 1] == '/')                    //line:netp:parseuri:slashcheck
            strcat(filename, "home.html");                //line:netp:parseuri:appenddefault
        if (strstr(uri, "/mp4"))
            strcpy(filename, "go.mp4");
        return 1;
    }
    /* Dynamic content */
    else {                                                //line:netp:parseuri:isdynamic
        // uri 예시: dynamic: /cgi-bin/adder?first=1213&second
        ptr = index(uri, '?');                            //line:netp:parseuri:beginextract   // index() -> 문자열에서 특정 문자의 위치 반환
        // CGI 인자 추출
        if (ptr) {
            strcpy(cgiargs, ptr+1);                       // ? 뒤의 인자 모두 가져다 붙임, 인자로 주어진 값들을 cgiargs 변수에 넣음
            *ptr = '\0';                                  // ? 뒤 모두 삭제
        }
        else 
            strcpy(cgiargs, "");                          //line:netp:parseuri:endextract     // 물음표 뒤 인자들 전부 넣기

        strcpy(filename, ".");                            //line:netp:parseuri:beginconvert2  // 나머지 부분 상대 uri로 바꿈
        strcat(filename, uri);                            //line:netp:parseuri:endconvert2    // ./uri로 변경
        return 0;
    }
}
/* $end parse_uri */

/*
 * serve_static - copy a file back to the client 
 */
/* $begin serve_static */
void serve_static(int fd, char *filename, int filesize) {
    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXBUF];

    /* Send response headers to client */
    get_filetype(filename, filetype);    //line:netp:servestatic:getfiletype
    sprintf(buf, "HTTP/1.0 200 OK\r\n"); //line:netp:servestatic:beginserve
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Tiny Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n", filesize);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: %s\r\n\r\n", filetype);
    Rio_writen(fd, buf, strlen(buf));    //line:netp:servestatic:endserve

    /* Send response body to client */
    srcfd = Open(filename, O_RDONLY, 0); //line:netp:servestatic:open
    // srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); //line:netp:servestatic:mmap
    srcp = Malloc(filesize);
    Rio_readn(srcfd, srcp, filesize);
    Close(srcfd);                       //line:netp:servestatic:close
    Rio_writen(fd, srcp, filesize);     //line:netp:servestatic:write
    // Munmap(srcp, filesize);             //line:netp:servestatic:munmap
    Free(srcp);
}

/*
 * get_filetype - derive file type from file name
 */
void get_filetype(char *filename, char *filetype) {
    if (strstr(filename, ".html"))
	      strcpy(filetype, "text/html");
    else if (strstr(filename, ".gif"))
	      strcpy(filetype, "image/gif");
    else if (strstr(filename, ".png"))
	      strcpy(filetype, "image/png");
    else if (strstr(filename, ".jpg"))
	      strcpy(filetype, "image/jpeg");
    else if (strstr(filename, ".mp4"))
        strcpy(filetype, "video/mp4");    // Tiny 서버가 video type의 파일을 처리하도록 함
    else
	      strcpy(filetype, "text/plain");
}  
/* $end serve_static */

/*
 * serve_dynamic - run a CGI program on behalf of the client
 */
/* $begin serve_dynamic */
void serve_dynamic(int fd, char *filename, char *cgiargs) {
    char buf[MAXLINE], *emptylist[] = { NULL };

    /* Return first part of HTTP response */
    sprintf(buf, "HTTP/1.0 200 OK\r\n"); 
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Tiny Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));
  
    if (Fork() == 0) { /* Child */ //line:netp:servedynamic:fork
        /* Real server would set all CGI vars here */
        setenv("QUERY_STRING", cgiargs, 1); //line:netp:servedynamic:setenv
        Dup2(fd, STDOUT_FILENO);         /* Redirect stdout to client */ //line:netp:servedynamic:dup2
        Execve(filename, emptylist, environ); /* Run CGI program */ //line:netp:servedynamic:execve
    }
    Wait(NULL); /* Parent waits for and reaps child */ //line:netp:servedynamic:wait
}
/* $end serve_dynamic */

/*
 * clienterror - returns an error message to the client
 */
/* $begin clienterror */
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
/* $end clienterror */