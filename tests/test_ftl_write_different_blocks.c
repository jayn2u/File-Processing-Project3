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
  printf("===== FTL 다른 블록에 쓰기 테스트 시작 =====\n");

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

  // 첫 번째 블록에 쓰기
  printf("\n1. 첫 번째 블록에 쓰기\n");
  memset(sectorbuf, 'A', SECTOR_SIZE);
  int first_lsn = 0;
  snprintf(sectorbuf, SECTOR_SIZE, "첫 번째 블록 LSN %d 테스트", first_lsn);

  printf("LSN %d에 데이터 쓰기: %s\n", first_lsn, sectorbuf);
  ftl_write(first_lsn, sectorbuf);

  // 매핑 테이블 출력
  printf("\nLSN %d 쓰기 후 매핑 테이블 출력:\n", first_lsn);
  ftl_print();

  printf("\nLSN %d 쓰기 후 프리 블록 리스트 출력:\n", first_lsn);
  ftl_print_free_blocks();

  // 데이터 읽기 테스트
  memset(readbuf, 0, SECTOR_SIZE);
  ftl_read(first_lsn, readbuf);
  printf("LSN %d에서 데이터 읽기: %s\n", first_lsn, readbuf);

  // 다른 블록에 쓰기 테스트
  printf("\n2. 다른 블록에 쓰기 테스트\n");
  int second_lsn = PAGES_PER_BLOCK + 1;  // 다른 논리 블록의 LSN
  memset(sectorbuf, 'X', SECTOR_SIZE);
  snprintf(sectorbuf, SECTOR_SIZE, "다른 블록 LSN %d 테스트", second_lsn);

  printf("LSN %d에 데이터 쓰기: %s\n", second_lsn, sectorbuf);
  ftl_write(second_lsn, sectorbuf);

  // 매핑 테이블 출력
  printf("\nLSN %d 쓰기 후 매핑 테이블 출력:\n", second_lsn);
  ftl_print();

  printf("\nLSN %d 쓰기 후 프리 블록 리스트 출력:\n", second_lsn);
  ftl_print_free_blocks();

  // 다른 블록의 데이터 읽기 테스트
  memset(readbuf, 0, SECTOR_SIZE);
  ftl_read(second_lsn, readbuf);
  printf("LSN %d에서 데이터 읽기: %s\n", second_lsn, readbuf);

  // 최종 상태 확인
  printf("\n최종 매핑 테이블 출력:\n");
  ftl_print();

  printf("\n최종 프리 블록 리스트 출력:\n");
  ftl_print_free_blocks();

  // 플래시 메모리 파일 닫기
  fclose(flashmemoryfp);

  printf("===== FTL 다른 블록에 쓰기 테스트 완료 =====\n");
  return 0;
}
