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
  printf("===== FTL 존재하지 않는 LSN 읽기 테스트 시작 =====\n");

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
  char readbuf[SECTOR_SIZE];

  // 몇 개의 LSN에 데이터 쓰기 (비교 기준)
  printf("\n1. 참조용 데이터 쓰기\n");
  for (int i = 0; i < 2; i++) {
    memset(sectorbuf, 'A' + i, SECTOR_SIZE);
    snprintf(sectorbuf, SECTOR_SIZE, "LSN %d 참조 데이터", i);
    printf("LSN %d에 참조 데이터 쓰기: %s\n", i, sectorbuf);
    ftl_write(i, sectorbuf);
  }

  // 매핑 테이블 출력
  printf("\n참조 데이터 쓰기 후 매핑 테이블 출력:\n");
  ftl_print();

  // 존재하지 않는 LSN 읽기 테스트
  printf("\n2. 존재하지 않는 LSN 읽기 테스트\n");
  int non_existent_lsn = 100;       // 아직 데이터를 쓰지 않은 LSN
  memset(readbuf, 0, SECTOR_SIZE);  // 읽기 버퍼 초기화
  ftl_read(non_existent_lsn, readbuf);
  printf("존재하지 않는 LSN %d 읽기 결과: ", non_existent_lsn);

  // 존재하지 않는 LSN은 0xFF로 채워져 있는지 확인
  int is_filled_with_ff = 1;
  for (int i = 0; i < SECTOR_SIZE; i++) {
    if (readbuf[i] != (char)0xFF) {
      is_filled_with_ff = 0;
      break;
    }
  }

  if (is_filled_with_ff) {
    printf("정상 (0xFF로 채워짐)\n");
  } else {
    printf("비정상 (0xFF로 채워지지 않음)\n");
    // 내용 출력
    printf("첫 16바이트 내용: ");
    for (int i = 0; i < 16 && i < SECTOR_SIZE; i++) {
      printf("%02X ", (unsigned char)readbuf[i]);
    }
    printf("\n");
  }

  // 다른 범위 밖의 LSN 테스트
  printf("\n3. 범위 밖의 LSN 읽기 테스트\n");
  int out_of_range_lsn = DATAPAGES_PER_DEVICE + 10;
  memset(readbuf, 0, SECTOR_SIZE);
  ftl_read(out_of_range_lsn, readbuf);
  printf("범위 밖의 LSN %d 읽기 결과: ", out_of_range_lsn);

  // 범위 밖 LSN도 0xFF로 채워져 있는지 확인
  is_filled_with_ff = 1;
  for (int i = 0; i < SECTOR_SIZE; i++) {
    if (readbuf[i] != (char)0xFF) {
      is_filled_with_ff = 0;
      break;
    }
  }

  if (is_filled_with_ff) {
    printf("정상 (0xFF로 채워짐)\n");
  } else {
    printf("비정상 (0xFF로 채워지지 않음)\n");
  }

  // 플래시 메모리 파일 닫기
  fclose(flashmemoryfp);

  printf("===== FTL 존재하지 않는 LSN 읽기 테스트 완료 =====\n");
  return 0;
}
