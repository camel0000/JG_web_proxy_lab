#include "csapp.h"

void echo(int connfd)
{
    size_t n;
    char buf[MAXLINE];
    rio_t rio;

    /* 읽기 버퍼 rio와 서버의 연결 소켓 식별자를 연결해준다. clientfd도 연결되어 있다. */
    Rio_readinitb(&rio, connfd);

    /* 읽기 버퍼 rio에서 클라이언트가 보낸 데이터를 읽고, rio에 그 데이터를 고대로 쓴다.*/
    /* 읽기 버퍼 rio에서 MAXLINE만큼의 데이터를 읽어 와 buf에 넣는다. */
    while((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) { // 0이면 클라이언트 식별자가 닫혔다는 소리다.
        printf("server received %d bytes\n", (int)n);

        /* buf 안에는 클라이언트가 보낸 데이터 그대로 있다. */
        /* buf 메모리 안의 클라이언트가 보낸 바이트 만큼의(사실상 모두)를 clientfd로 보낸다. */
    	Rio_writen(connfd, buf, n);
    }

    /* 클라이언트 식별자가 닫히면 루프 종료 및 함수도 종료. */
}