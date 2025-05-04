#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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

// 테스트용 상수 정의
#define MAX_TEST_ITERATIONS 100
#define PATTERN_SEQUENTIAL 0
#define PATTERN_RANDOM 1
#define PATTERN_HOTSPOT 2
#define PATTERN_MIXED 3

// 테스트 패턴을 위한 구조체
typedef struct {
  int lsn;                 // 논리 섹터 번호
  char data[SECTOR_SIZE];  // 저장할 데이터
  int read_count;          // 읽기 횟수 (통계용)
  int write_count;         // 쓰기 횟수 (통계용)
} sector_info;

// 테스트 통계 정보를 위한 구조체
typedef struct {
  int total_reads;
  int total_writes;
  int overwrite_count;
  int read_verify_success;
  int read_verify_fail;
} test_stats;

// 테스트에 사용할 랜덤 시드 초기화
void init_random_seed() { srand(time(NULL)); }

// min과 max 사이의 난수 생성
int get_random_number(int min, int max) {
  return min + rand() % (max - min + 1);
}

// 섹터 버퍼에 테스트 데이터 생성
void generate_test_data(char* buffer, int lsn, int iteration) {
  memset(buffer, 'A' + (iteration % 26), SECTOR_SIZE);
  snprintf(buffer, SECTOR_SIZE, "LSN %d, Iteration %d, Data: %d", lsn,
           iteration, get_random_number(1000, 9999));
}

// 특정 패턴에 따라 LSN 생성
int generate_lsn(int pattern, int iteration, int max_lsn, sector_info* sectors,
                 int hotspot_probability) {
  switch (pattern) {
    case PATTERN_SEQUENTIAL:
      return iteration % max_lsn;

    case PATTERN_RANDOM:
      return get_random_number(0, max_lsn - 1);

    case PATTERN_HOTSPOT: {
      // 일부 LSN에 집중 (hotspot)
      if (rand() % 100 < hotspot_probability) {
        // hotspot 영역은 총 LSN의 20%
        return get_random_number(0, max_lsn / 5);
      } else {
        return get_random_number(0, max_lsn - 1);
      }
    }

    case PATTERN_MIXED:
    default: {
      // 순차 접근과 랜덤 접근을 혼합
      if (iteration % 10 < 3) {  // 30%는 순차 접근
        return iteration % max_lsn;
      } else if (iteration % 10 < 7) {  // 40%는 랜덤 접근
        return get_random_number(0, max_lsn - 1);
      } else {                    // 30%는 핫스팟 접근
        if (rand() % 100 < 80) {  // 80% 확률로 hotspot
          return get_random_number(0, max_lsn / 5);
        } else {
          return get_random_number(0, max_lsn - 1);
        }
      }
    }
  }
}

// 섹터 정보 초기화
void init_sector_info(sector_info* sectors, int max_sectors) {
  for (int i = 0; i < max_sectors; i++) {
    sectors[i].lsn = i;
    sectors[i].read_count = 0;
    sectors[i].write_count = 0;
    memset(sectors[i].data, 0xFF, SECTOR_SIZE);
  }
}

// 읽기/쓰기 작업 실행 및 데이터 검증
void perform_operation(int op_type, int lsn, char* buffer, sector_info* sectors,
                       test_stats* stats) {
  char read_buffer[SECTOR_SIZE];

  if (op_type == 0) {  // 읽기 연산
    memset(read_buffer, 0, SECTOR_SIZE);
    ftl_read(lsn, read_buffer);
    stats->total_reads++;
    sectors[lsn].read_count++;

    // 이전에 쓰기 작업을 했는지 확인
    if (sectors[lsn].write_count > 0) {
      // 데이터 검증
      if (strncmp(read_buffer, sectors[lsn].data, SECTOR_SIZE) != 0) {
        stats->read_verify_fail++;
        printf("검증 실패: LSN %d, 예상: %.20s..., 실제: %.20s...\n", lsn,
               sectors[lsn].data, read_buffer);
      } else {
        stats->read_verify_success++;
      }
    }
  } else {  // 쓰기 연산
    // 이전에 쓰기 작업을 했는지 확인 (덮어쓰기 여부 확인)
    if (sectors[lsn].write_count > 0) {
      stats->overwrite_count++;
    }

    // 쓰기 작업 수행
    ftl_write(lsn, buffer);
    stats->total_writes++;
    sectors[lsn].write_count++;

    // 섹터 정보 업데이트
    strncpy(sectors[lsn].data, buffer, SECTOR_SIZE);
  }
}

// 테스트 통계 초기화
void init_test_stats(test_stats* stats) {
  stats->total_reads = 0;
  stats->total_writes = 0;
  stats->overwrite_count = 0;
  stats->read_verify_success = 0;
  stats->read_verify_fail = 0;
}

// 테스트 통계 출력
void print_test_stats(test_stats* stats) {
  printf("\n===== 테스트 통계 =====\n");
  printf("총 읽기 작업: %d\n", stats->total_reads);
  printf("총 쓰기 작업: %d\n", stats->total_writes);
  printf("덮어쓰기 작업: %d\n", stats->overwrite_count);
  printf("읽기 검증 성공: %d\n", stats->read_verify_success);
  printf("읽기 검증 실패: %d\n", stats->read_verify_fail);
  printf("========================\n");
}

// LSN 접근 빈도 분석
void print_access_patterns(sector_info* sectors, int max_sectors) {
  int max_reads = 0, max_writes = 0, total_reads = 0, total_writes = 0;
  int lsn_max_read = -1, lsn_max_write = -1;

  for (int i = 0; i < max_sectors; i++) {
    total_reads += sectors[i].read_count;
    total_writes += sectors[i].write_count;

    if (sectors[i].read_count > max_reads) {
      max_reads = sectors[i].read_count;
      lsn_max_read = i;
    }

    if (sectors[i].write_count > max_writes) {
      max_writes = sectors[i].write_count;
      lsn_max_write = i;
    }
  }

  printf("\n===== 접근 패턴 분석 =====\n");
  printf("가장 많이 읽은 LSN: %d (총 %d회)\n", lsn_max_read, max_reads);
  printf("가장 많이 쓴 LSN: %d (총 %d회)\n", lsn_max_write, max_writes);

  // 읽기 분포
  printf("\n읽기 분포 (상위 5개):\n");
  for (int t = 0; t < 5; t++) {
    int max_r = 0, idx = -1;
    for (int i = 0; i < max_sectors; i++) {
      if (sectors[i].read_count > max_r) {
        max_r = sectors[i].read_count;
        idx = i;
      }
    }
    if (idx != -1 && max_r > 0) {
      printf("LSN %d: %d회 (%.1f%%)\n", idx, max_r,
             (float)max_r * 100.0 / total_reads);
      sectors[idx].read_count = 0;  // 카운트 리셋
    }
  }

  // 쓰기 분포 초기화
  for (int i = 0; i < max_sectors; i++) {
    sectors[i].read_count = 0;
  }

  // 쓰기 분포
  printf("\n쓰기 분포 (상위 5개):\n");
  for (int t = 0; t < 5; t++) {
    int max_w = 0, idx = -1;
    for (int i = 0; i < max_sectors; i++) {
      if (sectors[i].write_count > max_w) {
        max_w = sectors[i].write_count;
        idx = i;
      }
    }
    if (idx != -1 && max_w > 0) {
      printf("LSN %d: %d회 (%.1f%%)\n", idx, max_w,
             (float)max_w * 100.0 / total_writes);
      sectors[idx].write_count = 0;  // 카운트 리셋
    }
  }
  printf("==========================\n");
}

// 주요 테스트 함수
void run_complex_scenario_test(int pattern, int iterations,
                               int read_write_ratio) {
  test_stats stats;
  init_test_stats(&stats);

  // 사용 가능한 최대 LSN
  int max_lsn = DATAPAGES_PER_DEVICE;

  // 섹터 정보 추적용 배열
  sector_info* sectors = (sector_info*)malloc(sizeof(sector_info) * max_lsn);
  init_sector_info(sectors, max_lsn);

  // 버퍼 생성
  char write_buffer[SECTOR_SIZE];

  printf("\n===== 복잡한 시나리오 테스트 시작 (패턴: %d) =====\n", pattern);

  // 시뮬레이션 실행
  for (int i = 0; i < iterations; i++) {
    int lsn = generate_lsn(pattern, i, max_lsn, sectors, 70);
    int operation =
        (rand() % 100) < read_write_ratio ? 0 : 1;  // 0: 읽기, 1: 쓰기

    if (operation == 1) {  // 쓰기 작업
      generate_test_data(write_buffer, lsn, i);
      printf("[%d] 쓰기: LSN %d, 데이터: %.20s...\n", i, lsn, write_buffer);
    } else {  // 읽기 작업
      printf("[%d] 읽기: LSN %d\n", i, lsn);
    }

    perform_operation(operation, lsn, write_buffer, sectors, &stats);

    // 매 10회 작업마다 상태 출력
    if (i % 10 == 9 || i == iterations - 1) {
      printf("\n현재 진행 상태 (작업 %d/%d):\n", i + 1, iterations);
      ftl_print();
    }
  }

  // 최종 상태 출력
  printf("\n최종 매핑 테이블:\n");
  ftl_print();

  printf("\n프리 블록 상태:\n");
  ftl_print_free_blocks();

  // 무작위로 10개의 LSN을 선택하여 읽기 검증
  printf("\n최종 데이터 검증 (무작위 10개 LSN):\n");
  for (int i = 0; i < 10; i++) {
    int lsn = get_random_number(0, max_lsn - 1);
    if (sectors[lsn].write_count > 0) {
      char read_buffer[SECTOR_SIZE];
      memset(read_buffer, 0, SECTOR_SIZE);
      ftl_read(lsn, read_buffer);

      printf("LSN %d 검증: ", lsn);
      if (strncmp(read_buffer, sectors[lsn].data, SECTOR_SIZE) != 0) {
        printf("실패! 예상: %.20s..., 실제: %.20s...\n", sectors[lsn].data,
               read_buffer);
        stats.read_verify_fail++;
      } else {
        printf("성공\n");
        stats.read_verify_success++;
      }
    } else {
      i--;  // 쓰지 않은 LSN은 건너뜀
    }
  }

  // 테스트 통계 출력
  print_test_stats(&stats);

  // 접근 패턴 분석
  print_access_patterns(sectors, max_lsn);

  // 메모리 해제
  free(sectors);
}

int main() {
  printf("===== FTL 복잡한 시나리오 테스트 =====\n\n");

  // 난수 생성기 초기화
  init_random_seed();

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

  // 다양한 시나리오 테스트
  printf("1. 핫스팟 모드 테스트 - 일부 LSN에 집중적 접근\n");
  run_complex_scenario_test(PATTERN_HOTSPOT, 50, 40);  // 40%는 읽기, 60%는 쓰기

  // 플래시 메모리 재초기화
  fclose(flashmemoryfp);
  flashmemoryfp = fopen("flashmemory", "w+b");
  blockbuf = (char*)malloc(BLOCK_SIZE);
  memset(blockbuf, 0xFF, BLOCK_SIZE);

  for (int i = 0; i < BLOCKS_PER_DEVICE; i++) {
    fwrite(blockbuf, BLOCK_SIZE, 1, flashmemoryfp);
  }

  free(blockbuf);
  ftl_open();

  printf("\n\n2. 혼합 모드 테스트 - 순차/랜덤/핫스팟 접근 혼합\n");
  run_complex_scenario_test(PATTERN_MIXED, 50, 30);  // 30%는 읽기, 70%는 쓰기

  // 플래시 메모리 닫기
  fclose(flashmemoryfp);

  printf("\n===== FTL 복잡한 시나리오 테스트 완료 =====\n");
  return 0;
}