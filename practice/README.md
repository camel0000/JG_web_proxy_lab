# Implementation for echo server and echo client for CSAPP3rd 11.4.9

Compile with make.
```
make
```

Execute the following commands in two different shells.
```
./echoserver 5000
./echoclient localhost 5000
```

Type anything on the shell running echoclient and you will receive echo from the server that's running on another shell.


따로 컴파일해서 사용하는법
gcc -g -Wall -c ~~~~.c	: c언어 파일 컴파일(.o 파일 생성)
gcc -g -Wall -o ${새로운 파일 이름} ${~~.o} ${~~~.o} 	: c언어 파일 링킹( 실행파일 )