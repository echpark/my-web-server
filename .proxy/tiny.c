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
void serve_dynamic(int fd, char *filename, char *cgiargs);
void get_filetype(char *filename, char *filetype);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);

/*
 * 반복실행 서버로 명령줄에서 넘겨받은 포트로의 연결 요청을 들음
 */
int main(int argc, char **argv) {
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;  // 접속한 클라이언트의 주소 정보 저장
  struct sockaddr_storage clientaddr;

  // 실행할 때 포트 번호를 인자로 하나만 받아야 함 (예: ./tiny 8000)
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);  // 두번째 인자 = 포트번호, 포트에서 연결을 받을 듣기 소켓을 오픈
  while (1) { // 무한 서버 루프를 실행
    clientlen = sizeof(clientaddr); 
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); // 연결 요청을 접수
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd); // 트랜잭션을 수행
    Close(connfd);  // 자신 쪽의 연결 끝을 닫음
  }
}

/* 
 * 클라이언트가 보낸 HTTP 요청을 읽고,
 * 동적 콘텐츠 -> HTML, 이미지 파일 그대로 읽어서 보내줌
 * 정적 콘텐츠 -> CGI 프로그램 실행 결과를 클라이언트에게 보내줌
 */
void doit(int fd)
{ 
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;

  Rio_readinitb(&rio, fd);
  Rio_readlineb(&rio, buf, MAXLINE);  // 클라이언트가 보낸 요청 라인 첫 줄을 읽음 (예: GET /index.html HTTP/1.1)
  printf("Request headers:\n");
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version);  // 메서드(GET), 요청 경로(/index.html), HTTP 버전으로 분리해 저장

  // Tiny는 GET 메소드만 지원함
  // GET 메소드가 아닌 다른 메소드를 요청했을 경우 오류 메시지 전송
  if (strcasecmp(method, "GET")) {
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }

  read_requesthdrs(&rio); // 남은 헤더 줄들은 읽기만 하고 버림

  // 정적 콘텐츠(1)인지 동적 콘텐츠(0)인지 확인
  // 실제 서버에서 열 파일 경로(filename)와 CGI 인지(cgiargs)도 분리해서 저장
  is_static = parse_uri(uri, filename, cgiargs);

  if (stat(filename, &sbuf) < 0) { // 파일이 디스크 상에 있지 않으면
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
    return;
  }

  if (is_static) {  // 요청이 정적 컨텐츠를 위한 것이면
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {  // 보통 파일이 아니거나 읽기 권한이 없다면
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
      return;
    }

    serve_static(fd, filename, sbuf.st_size); // 정적 컨텐츠를 클라이언트에게 제공
  }

  else {  // 요청이 동적 컨텐츠를 위한 것이면
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {  // 보통 파일이 아니거나 읽기 권한이 없다면
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return;
    }

    serve_dynamic(fd, filename, cgiargs); // 동적 컨텐츠를 클라이언트에게 제공
  }
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
 * URI를 정적 요청과 동적 요청으로 구분
 * CGI 인자와 파일 경로를 추출
 */
int parse_uri(char *uri, char *filename, char *cgiargs)
{
  char *ptr;

  if (!strstr(uri, "cgi-bin")) {   // 요청이 정적 컨텐츠를 위한 것이라면
    strcpy(cgiargs, "");  // CGI 인자 스트링을 지움
    strcpy(filename, ".");  // URI를 상대 리눅스 경로 이름(예: ./index.html)으로 바꿈
    strcat(filename, uri);  // URI를 상대 리눅스 경로 이름(예: ./index.html)으로 바꿈

    if (uri[strlen(uri)-1] == '/') {  // URI가 /로 끝나면
      strcat(filename, "home.html");  // 기본 파일 이름을 추가
    }

    return 1;
  }
  
  else { // 요청이 동적 컨텐츠를 위한 것이라면
    ptr = index(uri, '?');  // 첫번째 ? 문자의 주소를 찾아서 ptr에 저장

    if (ptr) {  // ?이 있으면
      strcpy(cgiargs, ptr+1); // ? 뒤에 있는 값 복사
      *ptr = '\0';  // ?를 \0으로 바꿔서 URI를 자름
    }
    
    else {  // ?이 없으면
      strcpy(cgiargs, "");
    }
    
    strcpy(filename, "./.proxy");  // URI 부분을 상대 리눅스 파일 이름으로 바꿈
    strcat(filename, uri);  // URI 부분을 상대 리눅스 파일 이름으로 바꿈
    return 0;
  }
}

/* 
 * 정적 파일의 내용을 클라이언트에게 전송 - malloc 사용
 * malloc()으로 메모리를 할당하고 read()로 직접 읽어서 전송하는 방식
 */
void serve_static(int fd, char *filename, int filesize)
{
  int srcfd;
  char *srcbuf; // 파일 내용을 저장할 메모리 공간
  char filetype[MAXLINE], header[MAXBUF];
  rio_t rio;

  // MIME 타입 설정
  get_filetype(filename, filetype);

  // 응답 헤더 작성
  sprintf(header, "HTTP/1.0 200 OK\r\n");
  sprintf(header + strlen(header), "Server: Tiny Web Server\r\n");
  sprintf(header + strlen(header), "Connection: close\r\n");
  sprintf(header + strlen(header), "Content-length: %d\r\n", filesize);
  sprintf(header + strlen(header), "Content-type: %s\r\n\r\n", filetype);

  // 헤더 전송
  Rio_writen(fd, header, strlen(header));
  printf("Response headers:\n%s", header);

  // 파일 열고 버퍼 준비
  srcfd = Open(filename, O_RDONLY, 0);
  srcbuf = (char *)malloc(filesize);

  if (!srcbuf) {
    fprintf(stderr, "malloc failed\n");
    Close(srcfd);
    return;
  }
  
  // 파일을 읽어서 버퍼에 저장
  Rio_readinitb(&rio, srcfd);
  if (Rio_readn(srcfd, srcbuf, filesize) != filesize) {
    fprintf(stderr, "Rio_readn failed\n");
    free(srcbuf);
    Close(srcfd);
    return;
  }
  Close(srcfd);

  // 버퍼 내용을 클라이언트로 전송
  Rio_writen(fd, srcbuf, filesize);
  free(srcbuf);  // 메모리 해제
}

/* 
 * CGI 실행 파일을 실행하고 결과를 클라이언트에게 전달
 */
void serve_dynamic(int fd, char *filename, char *cgiargs)
{
  char buf[MAXLINE], *emptylist[] = { NULL };

  sprintf(buf, "HTTP/1.0 200 OK\r\n");  // 클라이언트에게 성공을 알림
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  // 새로운 자식 프로세스를 fork함
  if (Fork() == 0) {  // 자식 프로세스 생성
    setenv("QUERY_STRING", cgiargs, 1);
    Dup2(fd, STDOUT_FILENO);  // 표준 출력을 클라이언트로
    Execve(filename, emptylist, environ);  // CGI 프로그램을 로드하고 실행
  }

  Wait(NULL);  // 부모는 자식 종료 대기
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
  } else if (strstr(filename, ".mpg")) {  // 11.7 숙제
    strcpy(filetype, "video/mpeg");
  } else if (strstr(filename, ".mp4")) {  // test용
    strcpy(filetype, "video/mp4");
  } else {
    strcpy(filetype, "text/plain");
  }
}

/* 
 * 요청이 잘못되었을 때, 사용자에게 에러 메시지 응답
 */
 void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];

  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

  sprintf(buf, "HTTP/1.0 %s %s\r\n...", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length:%d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}