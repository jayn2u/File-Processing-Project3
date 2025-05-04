#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../hybridmapping.h"

// 외부 함수 선언
void ftl_open();
void ftl_write(int lsn, char* sectorbuf);
void ftl_read(int lsn, char* sectorbuf);
void ftl_print();
void ftl_print_free_blocks();

// 전역 변수 정의
FILE* flashmemoryfp;

// 외부 함수 정의 - fdevicedriver.c에서 사용되는 함수들
int fdd_read(int ppn, char* pagebuf);
int fdd_write(int ppn, char* pagebuf);
int fdd_erase(int pbn);

int main() {
  printf("===== FTL 기존 데이터 덮어쓰기 테스트 시작 =====\n");

  // 플래시 메모리 초기화
  char* blockbuf;
  flashmemoryfp = fopen("flashmemory", "w+b");
  if (flashmemoryfp == NULL) {
    printf("플래시 메모리 파일 생성 실패\n");
    return 1;
  }

  // 플래시 메모리 초기화 (0xFF로)
  blockbuf = (char*)malloc(BLOCK_SIZE);
  memset(blockbuf, 0xFF, BLOCK_SIZE);

  for (int i = 0; i < BLOCKS_PER_DEVICE; i++) {
    fwrite(blockbuf, BLOCK_SIZE, 1, flashmemoryfp);
  }

  free(blockbuf);

  // FTL 초기화
  ftl_open();

  // 테스트용 섹터 버퍼 생성
  char sectorbuf[SECTOR_SIZE];
  char readbuf[SECTOR_SIZE];  // 읽기용 버퍼 추가

  // 초기 데이터 쓰기
  printf("\n1. 초기 데이터 쓰기\n");
  int test_lsn = 1;
  memset(sectorbuf, 'A', SECTOR_SIZE);
  snprintf(sectorbuf, SECTOR_SIZE, "LSN %d 초기 데이터", test_lsn);

  printf("LSN %d에 초기 데이터 쓰기: %s\n", test_lsn, sectorbuf);
  ftl_write(test_lsn, sectorbuf);

  // 매핑 테이블 출력
  printf("\n초기 데이터 쓰기 후 매핑 테이블 출력:\n");
  ftl_print();

  printf("\n초기 데이터 쓰기 후 프리 블록 리스트 출력:\n");
  ftl_print_free_blocks();

  // 초기 데이터 읽기 테스트
  memset(readbuf, 0, SECTOR_SIZE);
  ftl_read(test_lsn, readbuf);
  printf("LSN %d에서 초기 데이터 읽기: %s\n", test_lsn, readbuf);

  // 덮어쓰기 테스트
  printf("\n2. 기존 데이터 덮어쓰기 테스트\n");
  memset(sectorbuf, 'Z', SECTOR_SIZE);
  snprintf(sectorbuf, SECTOR_SIZE, "LSN %d 덮어쓰기 데이터", test_lsn);

  printf("LSN %d에 데이터 덮어쓰기: %s\n", test_lsn, sectorbuf);
  ftl_write(test_lsn, sectorbuf);

  // 매핑 테이블 출력
  printf("\n덮어쓰기 후 매핑 테이블 출력:\n");
  ftl_print();

  printf("\n덮어쓰기 후 프리 블록 리스트 출력:\n");
  ftl_print_free_blocks();

  // 덮어쓰기 데이터 읽기 테스트
  memset(readbuf, 0, SECTOR_SIZE);
  ftl_read(test_lsn, readbuf);
  printf("덮어쓰기 후 LSN %d에서 데이터 읽기: %s\n", test_lsn, readbuf);

  // 쓰기와 읽기 결과 비교
  if (strncmp(sectorbuf, readbuf, SECTOR_SIZE) != 0) {
    printf("오류: 덮어쓰기 후 LSN %d의 쓰기와 읽기 결과가 일치하지 않습니다!\n",
           test_lsn);
  } else {
    printf("LSN %d 덮어쓰기/읽기 테스트 성공\n", test_lsn);
  }

  // 플래시 메모리 파일 닫기
  fclose(flashmemoryfp);

  printf("===== FTL 기존 데이터 덮어쓰기 테스트 완료 =====\n");
  return 0;
}
