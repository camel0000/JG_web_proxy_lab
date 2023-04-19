#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

typedef struct cache_node {
    char *file_path;
    char *content;  // response_header + content
    int content_length;
    struct cache_node *prev, *next;
} cache_node;

typedef struct {
    cache_node *header, *trailer;
    int total_size;
} cache;

cache *_cache;

void doit(int fd);
void modify_http_header(char *http_header, char *hostname, int port, char *path, rio_t *rio_server);
void parse_uri(char *uri, char *host, char *port, char *path);
void *thread(void *vargp);

cache_node *find_cache(cache *_cache, char *file_path);                                     // return NULL or found location pointer
void insert_cache(cache *_cache, char *file_path, char *content, int content_length);       // if none hit -> call insert_cache()
void delete_cache(cache *_cache);
void hit_cache(cache *_cache, cache_node *hit_p);


int main(int argc, char **argv) {
    int listenfd, *connfdp;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;

    _cache = Malloc(sizeof(cache));
    _cache->header = _cache->trailer = NULL;
    _cache->total_size = 0;
    
    /* Check command line args */
    if (argc != 2) {
	      fprintf(stderr, "usage: %s <port>\n", argv[0]);
	      exit(1);
    }

    /* Open_listenfd 함수 호출 -> 듣기 식별자 오픈, 인자를 통해 port번호 넘김 */
    listenfd = Open_listenfd(argv[1]);

    /* 무한 서버 루프 실행 */
    while (1) {
        clientlen = sizeof(clientaddr);

        connfdp = Malloc(sizeof(int));
        *connfdp = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        Pthread_create(&tid, NULL, thread, connfdp);
    }

    printf("%s", user_agent_hdr);
    return 0;
}

void doit(int fd) {
    // doit 호출 -> main에서 연결이 되었고, request를 accept한 대로 transaction 수행을 해야한다는 것
    char host[MAXLINE];
    char port[MAXLINE];
    char path[MAXLINE];
    int socket_fd;
    char proxy_buf[MAXLINE], server_buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    rio_t rio_client, rio_server;
    
    char *cache_path = Malloc(MAXLINE);

    /* Receive the request from client */
    Rio_readinitb(&rio_client, fd);                         // initialize rio_client for buffered I/O
    Rio_readlineb(&rio_client, proxy_buf, MAXLINE);         // read client request
    sscanf(proxy_buf, "%s %s %s", method, uri, version);    // parse the request for each role

    printf("*** From Client ***\n");
    printf("Request headers:\n");
    printf("%s", proxy_buf);                                // GET http://localhost:5684/home.html HTTP/1.1

    /* Only receive method "GET" */
    if (strcasecmp(method, "GET")) {
        printf("Proxy does not implement the method\n");
        return;
    }

    /* Extract info of destination host and port from request */
    parse_uri(uri, host, port, path);
    strcpy(cache_path, path);

    cache_node *tmp = find_cache(_cache, cache_path);

    if (tmp != NULL) {                                          // forward to client the response in cache
        Rio_writen(fd, tmp->content, tmp->content_length);
        return; 
    }

    /* Forward the extracted info to the destination server */
    socket_fd = Open_clientfd(host, port);                  // open a new socket for the proxy to server connection
    sprintf(server_buf, "%s %s %s\r\n", method, path, version);
    printf("*** To Server ***\n");
    printf("%s\n", server_buf);

    Rio_readinitb(&rio_server, socket_fd);                  // initialize rio_server for buffered I/O
    modify_http_header(server_buf, host, port, path, &rio_client);
    Rio_writen(socket_fd, server_buf, strlen(server_buf));

    /* Return the response from server to the client */
    // size_t n;
    // while ((n = Rio_readlineb(&rio_server, server_buf, MAXLINE)) != 0) {
    //     printf("Proxy received %d bytes from server\n", n);
    //     Rio_writen(fd, server_buf, n);
    // }


    char *tmp_buf = (char *)Malloc(MAX_OBJECT_SIZE);
    strcpy(tmp_buf, "");

    size_t n;
    while ((n = Rio_readlineb(&rio_server, server_buf, MAXLINE)) != 0) {
        printf("Proxy received %d bytes from server\n", n);
        Rio_writen(fd, server_buf, n);
        strcat(tmp_buf, server_buf);
    }
    insert_cache(_cache, cache_path, tmp_buf, strlen(tmp_buf));
    

    printf("cache_header: %s\n", _cache->header->file_path);
    printf("cache_content: %s\n", _cache->header->content);
    Close(socket_fd);
}

/*
* modify_http_header - Modifying the info, extracted from client request, to HTTP request header for dest server
*/
void modify_http_header(char *http_header, char *hostname, int port, char *path, rio_t *rio_server) {
    char buf[MAXLINE], other_hdr[MAXLINE], host_hdr[MAXLINE];

    sprintf(http_header, "GET %s HTTP/1.0\r\n", path);          // save "GET 'path' HTTP/1.0" on http_header buffer

    /* Process of making modified request header */
    while (Rio_readlineb(rio_server, buf, MAXLINE) > 0) {
        if (strcmp(buf, "\r\n") == 0)                           // mean end of the buffer(empty line)
            break;
        
        if (!strncasecmp(buf, "Host", strlen("Host"))) {        // if found "Host" header
            strcpy(host_hdr, buf);                              // copy & paste the "Host" header line on host_hdr buffer
            continue;
        }

        // if not found "Connection" & "Proxy-Connection" & "User-Agent"
        if (strncasecmp(buf, "Connection", strlen("Connection")) && strncasecmp(buf, "Proxy-Connection", strlen("Proxy-Connection")) && strncasecmp(buf, "User-Agent", strlen("User-Agent"))) {
            strcat(other_hdr, buf);                             // add the line on other_hdr buffer
        }
    }

    if (strlen(host_hdr) == 0) {                                // if host_hdr is empty
        sprintf(host_hdr, "Host: %s:%d\r\n", hostname, port);   // save "Host: 'hostname':'port'" on host_hdr buffer
    }

    // save the contents of request header for sending to server
    sprintf(http_header, "%s%s%s%s%s%s%s", http_header, host_hdr, "Connection: close\r\n", "Proxy-Connection: close\r\n", user_agent_hdr, other_hdr, "\r\n");
    return;
}

/*
* parse_uri - Extract info of destination host and port from request
*/
void parse_uri(char *uri, char *host, char *port, char *path) {
    char *ptr = strstr(uri, "//");
    ptr = ptr != NULL ? ptr + 2 : uri;
    char *host_ptr = ptr;
    char *port_ptr = strchr(ptr, ':');
    char *path_ptr = strchr(ptr, '/');

    if (port_ptr != NULL && (path_ptr == NULL || port_ptr < path_ptr)) {
        strncpy(host, host_ptr, port_ptr - host_ptr);
        strncpy(port, port_ptr + 1, path_ptr - port_ptr - 1);
    }
    else {
        strcpy(port, "80");
        strncpy(host, host_ptr, path_ptr - host_ptr);
    }
    strcpy(path, path_ptr);
    return;
}

void *thread(void *vargp) {
    int connfd = *((int *)vargp);
    Pthread_detach(pthread_self());
    Free(vargp);
    doit(connfd);
    Close(connfd);
    return NULL;
}

/*
* hit 여부 확인
* hit 했을 시, hit_cache() 호출
* hit 못 했을 시, NULL 반환
*/
cache_node *find_cache(cache *_cache, char *file_path) {
    printf("cache_header: %p\n", _cache->header);
    for (cache_node *p = _cache->header; p != NULL; p = p->next) {
        printf("file_path: %s, cache_paht: %s\n", file_path, p->file_path);
        if (strcmp(p->file_path, file_path) == 0) {
            hit_cache(_cache, p);
            return p;
        }
    }
    return NULL;
}

/*
* find_cache()를 통해서 cache에 저장된 같은 response가 없는 것을 확인한 뒤,
* cache list에 insert되는 new_node가 리스트의 가장 앞(header 노드)으로 와야함
*/
void insert_cache(cache *_cache, char *file_path, char *content, int content_length) {
    cache_node *new_node = (cache_node *)Malloc(sizeof(cache_node));
    new_node->file_path = file_path;
    new_node->content = content;
    new_node->content_length = content_length;
    new_node->prev = new_node->next = NULL;

    if (new_node->content_length > MAX_OBJECT_SIZE) return;     // 해당 버퍼 폐기

    while (_cache->total_size + new_node->content_length > MAX_CACHE_SIZE) {
        delete_cache(_cache);
    }

    /* 가장 앞 노드로 연결(insert)하기 */
    if (_cache->header == NULL) {                       // nothing in cache list
        // new_node->prev = _cache->header;
        // new_node->next = _cache->trailer;
        _cache->header = new_node;
        _cache->trailer = new_node;
    }
    else {                                              // something in cache list (normal case)
        _cache->header->prev = new_node;
        new_node->next = _cache->header;
        _cache->header = new_node;
    }
    _cache->total_size += new_node->content_length;     // renew total_size
}

void delete_cache(cache *_cache) {
    cache_node *del_node = _cache->trailer;

    _cache->trailer = _cache->trailer->prev;
    _cache->trailer->next = NULL;

    _cache->total_size -= del_node->content_length;     // renew total_size
    free(del_node->file_path);
    free(del_node);
}

/*
* hit_cache - move the cache_node hit on find_cache to the left of header node
*/
void hit_cache(cache *_cache, cache_node *hit_p) {
    cache_node *p = hit_p;

    if (p == _cache->header) {         // hit_p == _cache->header
        return;
    }
    else if (p == _cache->trailer) {    // hit_p == _cache->trailer
        _cache->trailer = _cache->trailer->prev;
        p->next = _cache->header;
        _cache->header->prev = p;
        _cache->trailer->next = NULL;
        p->prev = NULL;
        _cache->header = p;
    }
    else {          // hit_p is in the middle of the list
        p->prev->next = p->next;
        p->next->prev = p->prev;
        p->prev = NULL;
        p->next = _cache->header;
        _cache->header->prev = p;
        _cache->header = p;
    }
    return;
}