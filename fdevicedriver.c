// 주의사항
// 1. 이 파일에 어떤 함수도 추가하지 마시오.
// 2. 이 파일에 어떤 것도 수정하지 마시오.
// 3. 이 파일에 버그가 있으면 바로 알려 주세요.
// 1과 2를 지키지 않으면 채점 시 컴파일이 제대로 되지 않을 수 있습니다.

#include <stdio.h>
#include <string.h>

#include "hybridmapping.h"

extern FILE* flashmemoryfp;  // ftlmgr.c에 정의되어 있음

int fdd_read(int ppn, char* pagebuf) {
  int ret;

  fseek(flashmemoryfp, PAGE_SIZE * ppn, SEEK_SET);
  ret = fread((void*)pagebuf, PAGE_SIZE, 1, flashmemoryfp);
  if (ret == 1) {
    return 1;
  } else {
    return -1;
  }
}

int fdd_write(int ppn, char* pagebuf) {
  int ret;

  fseek(flashmemoryfp, PAGE_SIZE * ppn, SEEK_SET);
  ret = fwrite((void*)pagebuf, PAGE_SIZE, 1, flashmemoryfp);
  if (ret == 1) {
    return 1;
  } else {
    return -1;
  }
}

int fdd_erase(int pbn) {
  char blockbuf[BLOCK_SIZE];
  int ret;

  memset((void*)blockbuf, (char)0xFF, BLOCK_SIZE);

  fseek(flashmemoryfp, BLOCK_SIZE * pbn, SEEK_SET);

  ret = fwrite((void*)blockbuf, BLOCK_SIZE, 1, flashmemoryfp);

  if (ret == 1) {
    return 1;
  } else {
    return -1;
  }
}