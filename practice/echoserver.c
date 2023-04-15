#include "csapp.h"

void echo(int connfd);

/* 서버의 포트 번호를 1번째 인자로 받는다. */
int main(int argc, char **argv)
{
    int listenfd, connfd;
    socklen_t clientlen;
    /* Accept로 보내지는 client 소켓 구조체. */
    struct sockaddr_storage clientaddr; /* sockaddr_storage 구조체: 모든 프로토콜의 주소에 대해 Enough room for any addr */                                                                                                               
    char client_hostname[MAXLINE], client_port[MAXLINE];

    // 인자 2개 다 받아야 함.
    if (argc != 2){
        fprintf(stderr, "usage: %s <port> \n", argv[0]);
        exit(0);
    }


    /* 해당 포트 번호에 적합한 듣기 식별자를 만들어 준다. */
    listenfd = Open_listenfd(argv[1]);


    while (1) {
        /* 클라이언트의 연결 요청을 계속 받아서 연결 식별자를 만든다. */
        clientlen = sizeof(struct sockaddr_storage); /* Important! 길이가 충분히 커서 프로토콜-독립적!*/
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); // 클라이언트와 통신하는 연결 식별자
        
        /* 클라이언트와 제대로 연결됐다는 것을 출력해준다. */
        Getnameinfo((SA *) &clientaddr, clientlen, 
                        client_hostname, MAXLINE, client_port, MAXLINE, 0);
        printf("Connected to (%s, %s)\n", client_hostname, client_port);
        
        /* 클라이언트가 보낸 데이터를 그대로 버퍼에 저장하고 그 데이터를 그대로 쓴다. */
        echo(connfd);

        /* 클라이언트 식별자가 닫히면 연결 식별자를 닫아준다. */
        Close(connfd);
    }
    
    /* 서버 종료 */
    exit(0);
}