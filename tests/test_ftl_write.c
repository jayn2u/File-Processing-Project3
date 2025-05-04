#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../hybridmapping.h"

// 외부 함수 선언
void ftl_open();
void ftl_write(int lsn, char* sectorbuf);
void ftl_print();
void ftl_print_free_blocks();

// 전역 변수 정의
FILE* flashmemoryfp;

// 외부 함수 정의 - fdevicedriver.c에서 사용되는 함수들
int fdd_read(int ppn, char* pagebuf);
int fdd_write(int ppn, char* pagebuf);
int fdd_erase(int pbn);

int main() {
  printf("===== FTL Write 테스트 시작 =====\n");

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

  // 다양한 LSN에 쓰기 테스트
  printf("\n1. 단일 블록 내 여러 페이지에 쓰기 테스트\n");
  for (int i = 0; i < 3; i++) {
    // 버퍼 초기화 및 테스트 데이터 입력
    memset(sectorbuf, 'A' + i, SECTOR_SIZE);
    snprintf(sectorbuf, SECTOR_SIZE, "LSN %d 테스트 데이터", i);

    printf("LSN %d에 데이터 쓰기: %s\n", i, sectorbuf);
    ftl_write(i, sectorbuf);
  }

  // 매핑 테이블과 프리 블록 리스트 출력
  printf("\n첫 번째 쓰기 후 매핑 테이블 출력:\n");
  ftl_print();

  printf("\n첫 번째 쓰기 후 프리 블록 리스트 출력:\n");
  ftl_print_free_blocks();

  // 다른 블록에 쓰기 테스트
  printf("\n2. 다른 블록에 쓰기 테스트\n");
  int lsn = PAGES_PER_BLOCK + 1;  // 다른 논리 블록의 LSN
  memset(sectorbuf, 'X', SECTOR_SIZE);
  snprintf(sectorbuf, SECTOR_SIZE, "다른 블록 LSN %d 테스트", lsn);

  printf("LSN %d에 데이터 쓰기: %s\n", lsn, sectorbuf);
  ftl_write(lsn, sectorbuf);

  // 매핑 테이블과 프리 블록 리스트 출력
  printf("\n두 번째 쓰기 후 매핑 테이블 출력:\n");
  ftl_print();

  printf("\n두 번째 쓰기 후 프리 블록 리스트 출력:\n");
  ftl_print_free_blocks();

  // 덮어쓰기 테스트
  printf("\n3. 기존 데이터 덮어쓰기 테스트\n");
  memset(sectorbuf, 'Z', SECTOR_SIZE);
  snprintf(sectorbuf, SECTOR_SIZE, "LSN 1 덮어쓰기 데이터");

  printf("LSN 1에 데이터 덮어쓰기: %s\n", sectorbuf);
  ftl_write(1, sectorbuf);

  // 매핑 테이블과 프리 블록 리스트 출력
  printf("\n덮어쓰기 후 매핑 테이블 출력:\n");
  ftl_print();

  printf("\n덮어쓰기 후 프리 블록 리스트 출력:\n");
  ftl_print_free_blocks();

  // 플래시 메모리 파일 닫기
  fclose(flashmemoryfp);

  printf("===== FTL Write 테스트 완료 =====\n");
  return 0;
}
