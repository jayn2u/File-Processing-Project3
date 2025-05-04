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

// ftlmgr.c의 MappingEntry 구조체와 정확히 동일하게 정의 (중복 정의 문제를
// 해결하기 위해) 단, 다른 파일에서는 접근할 수 없도록 이름을 변경
typedef struct {
  int lbn;                         // 논리 블록 번호
  int pbn;                         // 물리 블록 번호
  int last_offset;                 // 마지막으로 사용된 페이지의 오프셋
  int page_lsns[PAGES_PER_BLOCK];  // 각 페이지에 저장된 LSN 정보
} TestMappingEntry;

// 외부 매핑 테이블 변수 참조 - TestMappingEntry로 캐스팅하여 사용
extern void* mapping_table;

// 테스트 통과 여부를 추적하기 위한 카운터
typedef struct {
  int total_tests;
  int passed_tests;
} TestResult;

// 플래시 메모리 파일에서 직접 특정 페이지를 읽어 LSN 추출
int extract_lsn_from_flash(int ppn) {
  char pagebuf[PAGE_SIZE];
  int stored_lsn = -1;

  // 파일의 해당 위치로 이동
  fseek(flashmemoryfp, PAGE_SIZE * ppn, SEEK_SET);

  // 페이지 읽기
  if (fread(pagebuf, PAGE_SIZE, 1, flashmemoryfp) != 1) {
    printf("❌ 오류: PPN %d의 페이지를 읽을 수 없습니다.\n", ppn);
    return -1;
  }

  // spare 영역에서 LSN 추출 (spare 영역은 섹터 이후에 위치)
  memcpy(&stored_lsn, pagebuf + SECTOR_SIZE, sizeof(int));

  return stored_lsn;
}

// 테스트 헤더 출력
void print_test_header(const char* test_name) {
  printf("\n");
  printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
  printf("🔍 테스트 시작: %s\n", test_name);
  printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
}

// 테스트 결과 출력
void print_test_result(const char* test_name, int expected, int actual,
                       TestResult* result) {
  result->total_tests++;

  if (expected == actual) {
    printf("✅ 성공: %s (예상: %d, 실제: %d)\n", test_name, expected, actual);
    result->passed_tests++;
  } else {
    printf("❌ 실패: %s (예상: %d, 실제: %d)\n", test_name, expected, actual);
  }
}

// LSN에 해당하는 PPN 찾기 (매핑 테이블을 통해)
int find_ppn_for_lsn(int lsn, TestMappingEntry* mapping_table, int table_size) {
  int lbn = lsn / PAGES_PER_BLOCK;

  // 매핑 테이블 범위 체크
  if (lbn < 0 || lbn >= table_size) {
    return -1;
  }

  // 물리 블록이 할당되지 않은 경우
  if (mapping_table[lbn].pbn == -1) {
    return -1;
  }

  // 매핑 테이블에서 해당 LSN이 저장된 페이지 오프셋 찾기
  for (int i = 0; i <= mapping_table[lbn].last_offset; i++) {
    if (mapping_table[lbn].page_lsns[i] == lsn) {
      // PPN = 물리 블록 번호 * 블록당 페이지 수 + 페이지 오프셋
      return mapping_table[lbn].pbn * PAGES_PER_BLOCK + i;
    }
  }

  return -1;  // LSN을 찾지 못함
}

// Spare 영역 LSN 확인 테스트 실행
void run_spare_area_lsn_test() {
  print_test_header("Spare 영역 LSN 저장 테스트");

  TestResult result = {0, 0};
  char sectorbuf[SECTOR_SIZE];

  // 1. 여러 LSN에 데이터 쓰기
  printf("1️⃣ 여러 LSN에 데이터 쓰기 테스트 중...\n");
  for (int i = 0; i < 5; i++) {
    memset(sectorbuf, 'A' + i, SECTOR_SIZE);
    snprintf(sectorbuf, SECTOR_SIZE, "LSN %d 테스트 데이터", i);
    printf("  ► LSN %d에 데이터 쓰기: %s\n", i, sectorbuf);
    ftl_write(i, sectorbuf);
  }

  // 매핑 테이블 상태 출력
  printf("\n2️⃣ 매핑 테이블 상태:\n");
  ftl_print();

  // 2. 각 LSN에 대해 PPN 확인 후 spare 영역에서 LSN 추출하여 검증
  printf("\n3️⃣ Spare 영역의 LSN 값 검증 중...\n");
  for (int lsn = 0; lsn < 5; lsn++) {
    int ppn = find_ppn_for_lsn(lsn, (TestMappingEntry*)mapping_table,
                               DATABLKS_PER_DEVICE);
    if (ppn != -1) {
      int stored_lsn = extract_lsn_from_flash(ppn);
      printf("  ► LSN %d는 PPN %d에 매핑됨\n", lsn, ppn);

      char test_name[100];
      snprintf(test_name, sizeof(test_name), "LSN %d의 spare 영역 검증", lsn);
      print_test_result(test_name, lsn, stored_lsn, &result);
    } else {
      printf("  ► LSN %d에 대한 PPN을 찾을 수 없음\n", lsn);
    }
  }

  // 3. 덮어쓰기 테스트 (이미 쓰여진 LSN에 다시 쓰기)
  printf("\n4️⃣ 덮어쓰기 후 Spare 영역 검증 중...\n");
  int overwrite_lsn = 2;  // LSN 2에 덮어쓰기
  memset(sectorbuf, 'Z', SECTOR_SIZE);
  snprintf(sectorbuf, SECTOR_SIZE, "LSN %d 덮어쓰기 데이터", overwrite_lsn);
  printf("  ► LSN %d에 덮어쓰기: %s\n", overwrite_lsn, sectorbuf);
  ftl_write(overwrite_lsn, sectorbuf);

  // 매핑 테이블 상태 출력
  printf("\n5️⃣ 덮어쓰기 후 매핑 테이블 상태:\n");
  ftl_print();

  // 덮어쓰기 후 LSN 확인
  int ppn_after_overwrite = find_ppn_for_lsn(
      overwrite_lsn, (TestMappingEntry*)mapping_table, DATABLKS_PER_DEVICE);
  if (ppn_after_overwrite != -1) {
    int stored_lsn = extract_lsn_from_flash(ppn_after_overwrite);
    printf("\n  ► LSN %d는 덮어쓰기 후 PPN %d에 매핑됨\n", overwrite_lsn,
           ppn_after_overwrite);

    print_test_result("덮어쓰기 후 spare 영역 검증", overwrite_lsn, stored_lsn,
                      &result);
  } else {
    printf("\n  ► 덮어쓰기 후 LSN %d에 대한 PPN을 찾을 수 없음\n",
           overwrite_lsn);
  }

  // 최종 결과 출력
  printf("\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
  printf("📊 테스트 결과: 총 %d개 중 %d개 성공 (%.1f%%)\n", result.total_tests,
         result.passed_tests,
         (result.total_tests > 0)
             ? (float)result.passed_tests * 100 / result.total_tests
             : 0);
  printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
}

// 메인 함수
int main() {
  printf("===== FTL Spare 영역 LSN 검증 테스트 =====\n");

  // 플래시 메모리 초기화
  char* blockbuf;
  flashmemoryfp = fopen("flashmemory", "w+b");
  if (flashmemoryfp == NULL) {
    printf("❌ 플래시 메모리 파일 생성 실패\n");
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

  // Spare 영역 LSN 테스트 실행
  run_spare_area_lsn_test();

  // 플래시 메모리 파일 닫기
  fclose(flashmemoryfp);

  printf("\n===== FTL Spare 영역 LSN 검증 테스트 완료 =====\n");
  return 0;
}