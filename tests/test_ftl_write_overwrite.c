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
  printf("===== FTL 동일한 LSN에 6번 업데이트 테스트 시작 =====\n");

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

  // 테스트용 버퍼 생성
  char sectorbuf[SECTOR_SIZE];
  char readbuf[SECTOR_SIZE];  // 읽기용 버퍼
  int test_lsn = 1;           // 테스트에 사용할 LSN

  printf("\n초기 매핑 테이블 출력:\n");
  ftl_print();

  printf("\n초기 프리 블록 리스트 출력:\n");
  ftl_print_free_blocks();

  printf("\n1. 동일한 LSN에 6번 업데이트 테스트\n");

  // 동일한 LSN에 6번 업데이트 수행
  for (int i = 0; i < PAGES_PER_BLOCK + 2; i++) {
    memset(sectorbuf, 'A' + i,
           SECTOR_SIZE);  // 각 업데이트마다 다른 문자로 채움
    snprintf(sectorbuf, SECTOR_SIZE, "LSN %d 업데이트 #%d", test_lsn, i);

    printf("\n[업데이트 #%d] LSN %d에 데이터 쓰기: %s\n", i, test_lsn,
           sectorbuf);
    ftl_write(test_lsn, sectorbuf);

    // 매핑 테이블 및 프리 블록 상태 출력
    printf("매핑 테이블 출력 (업데이트 #%d 후):\n", i);
    ftl_print();

    printf("\n프리 블록 리스트 출력 (업데이트 #%d 후):\n", i);
    ftl_print_free_blocks();

    // 데이터 읽기 및 검증
    memset(readbuf, 0, SECTOR_SIZE);
    ftl_read(test_lsn, readbuf);
    printf("LSN %d에서 데이터 읽기: %s\n", test_lsn, readbuf);

    if (strncmp(sectorbuf, readbuf, SECTOR_SIZE) != 0) {
      printf(
          "오류: 업데이트 #%d 후 LSN %d의 쓰기와 읽기 결과가 일치하지 "
          "않습니다!\n",
          i, test_lsn);
    } else {
      printf("업데이트 #%d 후 LSN %d 읽기 검증 성공\n", i, test_lsn);
    }
  }

  // LSN 2에 대한 3번의 업데이트 테스트 추가
  printf("\n2. LSN 2에 대한 3번 업데이트 테스트\n");

  int test_lsn2 = 2;  // 두 번째 테스트에 사용할 LSN

  // LSN 2에 3번 업데이트 수행
  for (int i = 0; i < 3; i++) {
    memset(sectorbuf, 'X' + i,
           SECTOR_SIZE);  // 각 업데이트마다 다른 문자로 채움
    snprintf(sectorbuf, SECTOR_SIZE, "LSN %d 업데이트 #%d", test_lsn2, i);

    printf("\n[업데이트 #%d] LSN %d에 데이터 쓰기: %s\n", i, test_lsn2,
           sectorbuf);
    ftl_write(test_lsn2, sectorbuf);

    // 매핑 테이블 및 프리 블록 상태 출력
    printf("매핑 테이블 출력 (LSN %d 업데이트 #%d 후):\n", test_lsn2, i);
    ftl_print();

    printf("\n프리 블록 리스트 출력 (LSN %d 업데이트 #%d 후):\n", test_lsn2, i);
    ftl_print_free_blocks();

    // 데이터 읽기 및 검증
    memset(readbuf, 0, SECTOR_SIZE);
    ftl_read(test_lsn2, readbuf);
    printf("LSN %d에서 데이터 읽기: %s\n", test_lsn2, readbuf);

    if (strncmp(sectorbuf, readbuf, SECTOR_SIZE) != 0) {
      printf(
          "오류: 업데이트 #%d 후 LSN %d의 쓰기와 읽기 결과가 일치하지 "
          "않습니다!\n",
          i, test_lsn2);
    } else {
      printf("업데이트 #%d 후 LSN %d 읽기 검증 성공\n", i, test_lsn2);
    }
  }

  // 두 LSN에 대한 최종 결과 검증
  printf("\n3. 최종 데이터 검증\n");

  // LSN 1 최종 데이터 검증
  memset(readbuf, 0, SECTOR_SIZE);
  ftl_read(test_lsn, readbuf);
  printf("최종 LSN %d 데이터: %s\n", test_lsn, readbuf);

  // LSN 2 최종 데이터 검증
  memset(readbuf, 0, SECTOR_SIZE);
  ftl_read(test_lsn2, readbuf);
  printf("최종 LSN %d 데이터: %s\n", test_lsn2, readbuf);

  // 플래시 메모리 파일 닫기
  fclose(flashmemoryfp);

  printf("\n===== FTL 동일한 LSN 테스트 완료 =====\n");
  return 0;
}
