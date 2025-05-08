#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

/*
 * 반복실행 서버로 명령줄에서 넘겨받은 포트로의 연결 요청을 들음
 */
int main(int argc, char **argv) {
  int listenfd, connfd;
  socklen_t clientlen;  // 접속한 클라이언트의 주소 정보 저장
  struct sockaddr_storage clientaddr;
  char homename[MAXLINE], port[MAXLINE];  // 안씀

  // 실행할 때 포트 번호를 인자로 하나만 받아야 함 (예: ./tiny 8000)
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);  // 리스닝 소켓 오픈 (두번째 인자 : 포트번호)

  while (1) { // 무한 서버 루프를 실행
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); // 클라이언트의 연결 요청을 접수
    handle_request(connfd); // 요청 처리 + 종료
  }
}

/*
 * 클라이언트의 단일 요청 처리
 */
void handle_request(int connfd) {
  func(connfd); // 요청 처리
  Close(connfd);  // 연결 종료
}

/*
 * 스레드가 실행할 함수
 */
void *thread(void *vargp) {
  int connfd = *((int *)vargp);
  Pthread_detach(pthread_self());
  free(vargp);
  handle_request(connfd);
  return NULL;
}

/* 
 * 클라이언트가 보낸 HTTP 요청을 읽고,
 * 파싱해서 다시 전송 
 */
void func(int connfd)
{ 
  rio_t client_rio, server_rio; // 입출력
  char buf[MAXLINE], req[MAX_OBJECT_SIZE];
  char method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char host[MAXLINE], port[10], path[MAXLINE];

  Rio_readinitb(&client_rio, connfd); // 버퍼 초기화

  // 1. 요청 라인 읽기 (예: GET http://localhost:8000/home.html HTTP/1.1)
  if (!Rio_readlineb(&client_rio, buf, MAXLINE)) {
    return;
  }

  sscanf(buf, "%s %s %s", method, uri, version);  // 메서드, URI, 버전 분리

  if (parse_uri(uri, host, port, path) == -1) { // URI를 파싱해서 host, port, path 추출
    fprintf(stderr, "올바른 URI가 아닙니다: %s\n", uri);
    return;
  }

  // 2. 요청 라인 생성: HTTP/1.0 명시
  sprintf(req, "GET %s HTTP/1.0\r\n", path);

  // 3. 요청 헤더 처리: 불필요한 헤더 필터링
  while (Rio_readlineb(&client_rio, buf, MAXLINE) > 0) {  // 요청을 한 줄씩 읽어서 buf에 저장
    if (strcmp(buf, "\r\n") == 0) { // 헤더가 끝나면(= "\r\n") 종료
      break;
    }

    // buf의 헤더를 무시해도 되는지 여부
    int is_unwanted = strncasecmp(buf, "Host", 4) == 0 ||
                      strncasecmp(buf, "User-Agent", 10) == 0 ||
                      strncasecmp(buf, "Connection", 10) == 0 ||
                      strncasecmp(buf, "Proxy-Connection", 16) == 0;
    
    if (!is_unwanted) { // 무시하면 안될 경우
      strcat(req, buf); // req 뒤에 buf를 붙임
    }
  } 

  // 4. 필수 표준 헤더를 수동으로 추가
  sprintf(buf, "Host: %s\r\n", host);
  strcat(req, buf);
  sprintf(buf, "%s", user_agent_hdr);
  strcat(req, buf);
  sprintf(buf, "Connection: close\r\n");
  strcat(req, buf);
  sprintf(buf, "Proxy-Connection: close\r\n\r\n");  // 헤더 종료
  strcat(req, buf);

  // 디버깅
  printf("최종 요청:\n%s\n", req);

  // 5. 원 서버에 연결
  int serverfd = Open_clientfd(host, port);
  if (serverfd < 0) {
    fprintf(stderr, "원 서버 연결 실패\n");
    return;
  }

  Rio_readinitb(&server_rio, serverfd); // 읽기용 버퍼 초기화

  // 6. 요청 전송
  Rio_writen(serverfd, req, strlen(req)); // 프록시가 준비한 요청을 원 서버에 보냄

  // 7. 응답 헤더를 한 줄씩 읽어 클라이언트에게 전달
  int n;
  while ((n = Rio_readlineb(&server_rio, buf, MAXLINE)) > 0) {
    Rio_writen(connfd, buf, n);
    if (strcmp(buf, "\r\n") == 0) {
      break;
    }
  }

  // 8. 응답 본문(body)을 바이트 스트림으로 그대로 전달
  while ((n = Rio_readnb(&server_rio, buf, MAXBUF)) > 0) {
    Rio_writen(connfd, buf, n);
  }

  // 서버와의 연결 종료
  Close(serverfd);
}

/* 
 * HTTP 요청 헤더를 읽고 무시함
 * (단순히 줄별로 읽기만 함)
 */
void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE);
  while(strcmp(buf, "\r\n")) {
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }

  return;
}

/*
 * URI를 파싱하여 host, port, path를 분리
 * 예) http://localhost:8000/home.html 일 때,
 * host: localhost, port: 8000, path: home.html
 */
int parse_uri(char *uri, char *host, char *port, char *path)
{
  char *hostbegin, *hostend, *pathbegin, *portbegin;

  // "http://"로 시작하지 않으면 오류
  if (strncasecmp(uri, "http://", 7) != 0) {
    return -1;
  }

  hostbegin = uri + 7;  // "http://" 이후부터 시작
  
  pathbegin = strchr(hostbegin, '/'); // '/'로 경로의 시작을 찾음
  if (pathbegin) {
    strcpy(path, pathbegin);  // path 복사
    *pathbegin = '\0';  // host 문자열 종료 지점 설정
  } else {
    strcpy(path, "/");  // 경로가 없으면 기본값 "/"
  }

  // ':'가 있는 경우 포트가 명시된 것
  portbegin = strchr(hostbegin, ':');
  if (portbegin) {
    *portbegin = '\0';  // host 문자열 자르기
    strcpy(host, hostbegin);  // host 저장
    strcpy(port, portbegin + 1);  // ':' 이후가 포트
  } else {
    strcpy(host, hostbegin); // ':'가 없으면 전부 host
    strcpy(port, "80"); // 기본 포트 80 사용
  }

  return 0;
}

// 새 스레드를 생성하여 각 연결 처리
pthread_create() {

}

// 메모리 누수 방지
pthread_detach() {

}