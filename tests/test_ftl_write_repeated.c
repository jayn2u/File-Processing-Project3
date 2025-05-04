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
  printf("===== FTL Write 반복 테스트 시작 =====\n");

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

  // 같은 LSN에 대한 반복 쓰기 테스트
  printf("\n동일한 LSN에 대한 반복 쓰기 테스트(10회)\n");
  int test_lsn = 5;  // 테스트에 사용할 LSN

  for (int i = 0; i < 10; i++) {
    // 버퍼 초기화 및 테스트 데이터 입력
    memset(sectorbuf, 'A' + i, SECTOR_SIZE);
    snprintf(sectorbuf, SECTOR_SIZE, "LSN %d 반복 테스트 데이터 #%d", test_lsn,
             i + 1);

    printf("\n[%d번째 쓰기] LSN %d에 데이터 쓰기: %s\n", i + 1, test_lsn,
           sectorbuf);
    ftl_write(test_lsn, sectorbuf);

    // 매핑 테이블 출력
    printf("\n[%d번째 쓰기] 매핑 테이블 출력:\n", i + 1);
    ftl_print();

    // 프리 블록 리스트 출력
    printf("\n[%d번째 쓰기] 프리 블록 리스트 출력:\n", i + 1);
    ftl_print_free_blocks();

    // 데이터 읽기 테스트
    memset(readbuf, 0, SECTOR_SIZE);
    ftl_read(test_lsn, readbuf);
    printf("[%d번째 쓰기] LSN %d에서 데이터 읽기: %s\n", i + 1, test_lsn,
           readbuf);

    // 쓰기와 읽기 결과 비교
    if (strncmp(sectorbuf, readbuf, SECTOR_SIZE) != 0) {
      printf("오류: LSN %d의 쓰기와 읽기 결과가 일치하지 않습니다!\n",
             test_lsn);
    } else {
      printf("[%d번째 쓰기] LSN %d 쓰기/읽기 테스트 성공\n", i + 1, test_lsn);
    }
  }

  // 블록 상태 요약 출력
  printf("\n=== 테스트 후 블록 상태 요약 ===\n");
  ftl_print();
  ftl_print_free_blocks();

  // 다른 LSN에 대한 쓰기 테스트 (유효성 검증)
  printf("\n다른 LSN에 대한 쓰기 테스트 (검증용)\n");
  int verify_lsn = test_lsn + 1;

  memset(sectorbuf, 'Z', SECTOR_SIZE);
  snprintf(sectorbuf, SECTOR_SIZE, "LSN %d 검증 데이터", verify_lsn);

  printf("LSN %d에 데이터 쓰기: %s\n", verify_lsn, sectorbuf);
  ftl_write(verify_lsn, sectorbuf);

  memset(readbuf, 0, SECTOR_SIZE);
  ftl_read(verify_lsn, readbuf);
  printf("LSN %d에서 데이터 읽기: %s\n", verify_lsn, readbuf);

  // 최종 상태 출력
  printf("\n=== 최종 상태 ===\n");
  ftl_print();
  ftl_print_free_blocks();

  // 모든 LSN 데이터 읽기 테스트
  printf("\n테스트된 모든 LSN 데이터 확인\n");

  // 반복 테스트한 LSN 데이터 확인
  memset(readbuf, 0, SECTOR_SIZE);
  ftl_read(test_lsn, readbuf);
  printf("LSN %d 최종 데이터: %s\n", test_lsn, readbuf);

  // 검증용 LSN 데이터 확인
  memset(readbuf, 0, SECTOR_SIZE);
  ftl_read(verify_lsn, readbuf);
  printf("LSN %d 최종 데이터: %s\n", verify_lsn, readbuf);

  // 플래시 메모리 파일 닫기
  fclose(flashmemoryfp);

  printf("===== FTL Write 반복 테스트 완료 =====\n");
  return 0;
}
