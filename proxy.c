#include <stdio.h>

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000  // 프록시 캐시의 전체 크기
#define MAX_OBJECT_SIZE 102400  // 프록시에 캐시를 추가하여 최근에 사용된 웹 오브젝트

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

int main() {
  printf("%s", user_agent_hdr);
  return 0;
}
