/* 
 * tiny 프로젝트의 목표:
 * 클라이언트로부터 HTTP 요청을 받고,
 * 정적 파일(html, jpeg 등)을 반환하거나,
 * 동적 CGI 프로그램을 실행해서 그 출력을 반환한다.
 */

#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);

/*
 * 반복실행 서버로 명령줄에서 넘겨받은 포트로의 연결 요청을 들음
 */
int main(int argc, char **argv) {
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]); // 듣기 소켓 오픈
  while (1) { // 무한 서버 루프
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);  // 연결 요청 접수
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);   // 트랜잭션 수행
    Close(connfd);  // 자신의 연결 끝을 닫음
  }
}

/* 
 * 클라이언트 요청을 처리하는 핵심 함수
 * URI 파싱 -> 정적 또는 동적 컨텐츠 제공
 */
void doit(int fd)
{
  // 클라이언트 요청 줄을 읽고 GET, URI, HTTP 버전 파싱
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  rio_t rio;
  
  // GET 메서드만 지원(다른 메서드는 clienterror()로 처리)
  // parse_uri()를 호출해 파일 경로와 CGI 인자 파싱
  Rio_readinitb(&rio, fd);
  Rio_readlineb(&rio, buf, MAXLINE);
  sscanf(buf, "%s %s %s", method, uri, version);

  // 파일 존재 여부 검사 (stat() 사용)
  // 정적이면 serve_static(), 아니면 serve_dynamic() 호출
  is_static = parse_uri(uri, filename, cgiargs);
}

/* 
 * HTTP 요청 헤더를 읽고 무시함
 * (단순히 줄별로 읽기만 함)
 */
void read_requesthdrs(rio_t *rp)
{
  Rio_readlineb(rp, buf, MAXLINE);
  while(strcmp(buf, "\r\n")) {
    Rio_readlineb(rp, buf, MAXLINE);
  }
}

/*
 * URI를 정적 요청과 동적 요청으로 구분
 * CGI 인자와 파일 경로를 추출
 */
int parse_uri(char *uri, char *filename, char *cgiargs)
{
  if (!strstr(uri, "cgi-bin")) {   // 정적 컨텐츠
    strcpy(cgiargs, "");
    sprintf(filename, ".%s", uri);
    if (uri[strlen(uri)-1] == '/') {
      strcat(filename, "home.html");
    }
    return 1;
  }
  else { // 동적 컨텐츠
    ptr = index(uri, '?');
    if (ptr) {
      strcpy(cgiargs, ptr+1);
      *ptr = '\0';
    } else {
      strcpy(cgiargs, "");
      sprintf(filename, ".%s", uri);
    return 0;
    }
  }
}

/* 
 * 정적 파일의 내용을 클라이언트에게 전송
 */
void serve_static(int fd, char *filename, int filesize)
{
  // 파일을 열고 mmap()으로 메모리 매핑
  int srcfd = Open(filename, O_RDONLY, 0);
  srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);

  // 클라이언트에게 파일을 보냄 (헤더 포함)
  sprintf(buf, ...);   // HTTP 응답 헤더 작성
  Rio_writen(fd, buf, strlen(buf)); // 헤더 전송
  Rio_writen(fd, srcp, filesize);   // 파일 전송
}

/*
 * MIME 타입 결정
 */
void get_filetype(char *filename, char *filetype)
{
  if (strstr(filename, ".html")) {
    strcpy(filetype, "text/html");
  } else if (strstr(filename, ".gif")) {
    strcpy(filetype, "image/gif");
  } else if (strstr(filename, ".jpg")) {
    strcpy(filetype, "image/jpeg");
  } else {
    strcpy(filetype, "text/plain");
  }
}

/* 
 * CGI 실행 파일을 실행하고 결과를 클라이언트에게 전달
 */
void serve_dynamic(int fd, char *filename, char *cgiargs)
{
  // 클라이언트의 stdout으로 CGI 실행 결과를 바로 전달함
  if (Fork() == 0) {  // 자식 프로세스 생성
    setenv("QUERY_STRING", cgiargs, 1);
    Dup2(fd, STDOUT_FILENO);  // 표준 출력을 클라이언트로
    Execve(filename, emptylist, environ);  // CGI 실행
  }
  Wait(NULL);  // 부모는 자식 종료 대기
}

/* 
 * 요청이 잘못되었을 때, 사용자에게 에러 메시지 응답
 */
 void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  // 상태 코드 404, 403 등을 클라이언트에 반환
  sprintf(body, "<html>...%s: %s</p>...</html>", shortmsg, longmsg);
  sprintf(buf, "HTTP/1.0 %s %s\r\n...", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
}