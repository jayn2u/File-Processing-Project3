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

// 전역 변수 정의
FILE* flashmemoryfp;

// 외부 함수 정의 - fdevicedriver.c에서 사용되는 함수들
int fdd_read(int ppn, char* pagebuf);
int fdd_write(int ppn, char* pagebuf);
int fdd_erase(int pbn);

int main() {
  printf("===== FTL Read 테스트 시작 =====\n");

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

  // 테스트용 데이터 생성 및 저장
  char write_sectorbuf[SECTOR_SIZE];
  char read_sectorbuf[SECTOR_SIZE];
  int test_passed = 1;

  printf("\n1. 데이터 쓰기 후 읽기 테스트\n");

  // 여러 LSN에 데이터 쓰기
  for (int i = 0; i < 5; i++) {
    memset(write_sectorbuf, 0, SECTOR_SIZE);
    snprintf(write_sectorbuf, SECTOR_SIZE, "LSN %d 데이터: Flash Memory Test",
             i);
    printf("LSN %d에 데이터 쓰기: %s\n", i, write_sectorbuf);
    ftl_write(i, write_sectorbuf);
  }

  // 쓴 데이터 읽기 및 검증
  printf("\n쓴 데이터 읽기 및 검증:\n");
  for (int i = 0; i < 5; i++) {
    memset(read_sectorbuf, 0, SECTOR_SIZE);
    ftl_read(i, read_sectorbuf);

    memset(write_sectorbuf, 0, SECTOR_SIZE);
    snprintf(write_sectorbuf, SECTOR_SIZE, "LSN %d 데이터: Flash Memory Test",
             i);

    printf("LSN %d 읽기 결과: %s\n", i, read_sectorbuf);

    // 데이터 검증
    if (strncmp(write_sectorbuf, read_sectorbuf, SECTOR_SIZE) != 0) {
      printf("오류: LSN %d의 데이터가 일치하지 않습니다.\n", i);
      printf("  쓴 데이터: %s\n", write_sectorbuf);
      printf("  읽은 데이터: %s\n", read_sectorbuf);
      test_passed = 0;
    }
  }

  printf("\n2. 덮어쓰기 후 읽기 테스트\n");

  // LSN 2에 데이터 덮어쓰기
  memset(write_sectorbuf, 0, SECTOR_SIZE);
  snprintf(write_sectorbuf, SECTOR_SIZE, "LSN 2 덮어쓰기 데이터");
  printf("LSN 2에 데이터 덮어쓰기: %s\n", write_sectorbuf);
  ftl_write(2, write_sectorbuf);

  // 덮어쓴 데이터 읽기 및 검증
  memset(read_sectorbuf, 0, SECTOR_SIZE);
  ftl_read(2, read_sectorbuf);
  printf("LSN 2 읽기 결과: %s\n", read_sectorbuf);

  // 데이터 검증
  if (strncmp(write_sectorbuf, read_sectorbuf, SECTOR_SIZE) != 0) {
    printf("오류: LSN 2의 덮어쓴 데이터가 일치하지 않습니다.\n");
    printf("  쓴 데이터: %s\n", write_sectorbuf);
    printf("  읽은 데이터: %s\n", read_sectorbuf);
    test_passed = 0;
  }

  printf("\n3. 쓰지 않은 LSN 읽기 테스트\n");

  // 쓰지 않은 LSN 읽기
  int unused_lsn = PAGES_PER_BLOCK * 2;  // 쓰지 않은 LSN
  memset(read_sectorbuf, 0, SECTOR_SIZE);
  ftl_read(unused_lsn, read_sectorbuf);

  printf("쓰지 않은 LSN %d 읽기 결과 확인\n", unused_lsn);

  // 쓰지 않은 LSN은 0xFF로 채워져 있어야 함
  int all_0xFF = 1;
  for (int i = 0; i < SECTOR_SIZE; i++) {
    if ((unsigned char)read_sectorbuf[i] != 0xFF) {
      all_0xFF = 0;
      break;
    }
  }

  if (!all_0xFF) {
    printf("오류: 쓰지 않은 LSN은 0xFF로 채워져 있어야 합니다.\n");
    test_passed = 0;
  } else {
    printf("성공: 쓰지 않은 LSN은 올바르게 0xFF로 채워져 있습니다.\n");
  }

  // 테스트 결과 출력
  if (test_passed) {
    printf("\n모든 테스트가 성공적으로 완료되었습니다!\n");
  } else {
    printf("\n일부 테스트가 실패했습니다.\n");
  }

  // 매핑 테이블 출력
  printf("\n현재 매핑 테이블 상태:\n");
  ftl_print();

  // 플래시 메모리 파일 닫기
  fclose(flashmemoryfp);

  printf("===== FTL Read 테스트 완료 =====\n");
  return test_passed ? 0 : 1;  // 테스트 실패 시 에러 코드 반환
}
