/*
 * 더해주는 CGI 프로그램
 */

#include "csapp.h"

int main(void) {
  char *buf, *p;
  char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
  int n1=0, n2=0;

  if ((buf = getenv("QUERY_STRING")) != NULL) {
    sscanf(buf, "num1=%[^&]&num2=%s", arg1, arg2);  // num1 값이랑 num2 값을 각각 arg1, arg2에 저장
    n1 = atoi(arg1);  // 문자열 숫자로 변환
    n2 = atoi(arg2);
  }

  sprintf(content, "<html><head><title>Adder Result</title></head>");
  sprintf(content + strlen(content), "<body>");
  sprintf(content + strlen(content), "<h1>Adder CGI result</h1>");
  sprintf(content + strlen(content), "<p>this is your result !</p>");
  sprintf(content + strlen(content), "<p>%d + %d = %d</p>", n1, n2, n1 + n2);
  sprintf(content + strlen(content), "</body></html>");

  printf("Content-type: text/html\r\n");
  printf("Content-length: %lu\r\n\r\n", strlen(content));
  printf("%s", content);
  fflush(stdout);

  return 0;
}