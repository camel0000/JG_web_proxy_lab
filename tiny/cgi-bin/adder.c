/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "csapp.h"

int main(void) {
    char *buf, *p;
    char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
    int n1 = 0, n2 = 0;

    /* Extract the two arguments */
    if ((buf = getenv("QUERY_STRING")) != NULL) {
	p = strchr(buf, '&');
	*p = '\0';

	/*strcpy(arg1, buf);
	strcpy(arg2, p+1);
	n1 = atoi(arg1);
	n2 = atoi(arg2);*/

    /* 숙제문제 11.10 */
    sscanf(buf, "first=%d", &n1);       // sscanf() : scanf()와 동일하나, 입력 대상이 표준 입력이 아닌 매개변수로 전달되는 문자열 버퍼
    sscanf(p + 1, "second=%d", &n2);    // 프로그램 내부에서 생성된 문자열, 파일에서 읽은 문자열을 내부 변수에 저장 후 분리해야 하는 경우에 유용
    }

    /* Make the response body */
    sprintf(content, "Welcome to add.com: ");
    sprintf(content, "%sTHE Internet addition portal.\r\n<p>", content);
    sprintf(content, "%sThe answer is: %d + %d = %d\r\n<p>", 
	    content, n1, n2, n1 + n2);
    sprintf(content, "%sThanks for visiting!\r\n", content);
  
    /* Generate the HTTP response */
    printf("Connection: close\r\n");
    printf("Content-length: %d\r\n", (int)strlen(content));
    printf("Content-type: text/html\r\n\r\n");
    printf("%s", content);

    fflush(stdout);

    exit(0);
}
/* $end adder */