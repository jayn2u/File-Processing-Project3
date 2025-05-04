// 주의사항
// 1. hybridmapping.h에 정의되어 있는 상수 변수를 우선적으로 사용해야 함
// (예를 들면, PAGES_PER_BLOCK의 상수값은 채점 시에 변경할 수 있으므로 반드시 이
// 상수 변수를 사용해야 함)
// 2. hybridmapping.h에 필요한 상수 변수가 정의되어 있지 않을 경우 본인이 이
// 파일에서 만들어서 사용하면 됨
// 3. 새로운 data structure가 필요하면 이 파일에서 정의해서 쓰기
// 바람(hybridmapping.h에 추가하면 안됨)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "hybridmapping.h"

/** fdevicedriver prototype(signature) function **/
int fdd_read(int ppn, char* pagebuf);

int fdd_write(int ppn, char* pagebuf);

int fdd_erase(int pbn);

extern FILE* flashmemoryfp;

/** Data structure function **/

// 매핑 테이블 구조체 정의
typedef struct {
  int lbn;                         // 논리 블록 번호
  int pbn;                         // 물리 블록 번호
  int last_offset;                 // 마지막으로 사용된 페이지의 오프셋
  int page_lsns[PAGES_PER_BLOCK];  // 각 페이지에 저장된 LSN 정보
} MappingEntry;

// 전역 매핑 테이블
MappingEntry* mapping_table;

// 프리 블록 관리를 위한 노드 구조체
typedef struct FreeBlockNode {
  int pbn;                     // 프리 블록의 물리 블록 번호
  struct FreeBlockNode* next;  // 다음 노드 포인터
} FreeBlockNode;

// 프리 블록 리스트
typedef struct {
  FreeBlockNode* head;  // 리스트 헤드
  int count;            // 프리 블록 수
} FreeBlockList;

// 전역 프리 블록 연결 리스트
FreeBlockList free_block_list;

// 초기화 상태를 추적하는 플래그
int ftl_initialized = 0;

// 프리 블록 리스트에 블록 추가
void add_free_block(int pbn) {
  FreeBlockNode* node = (FreeBlockNode*)malloc(sizeof(FreeBlockNode));
  node->pbn = pbn;
  node->next = free_block_list.head;
  free_block_list.head = node;
  free_block_list.count++;
}

// 프리 블록 리스트에서 블록 가져오기
int get_free_block() {
  if (free_block_list.count == 0) {
    return -1;  // 프리 블록이 없는 경우
  }

  FreeBlockNode* node = free_block_list.head;
  int pbn = node->pbn;
  free_block_list.head = node->next;
  free_block_list.count--;
  free(node);

  return pbn;
}

// 매핑 테이블에서 주어진 lbn에 해당하는 pbn 찾기
int get_pbn(int lbn) {
  if (lbn < 0 || lbn >= DATABLKS_PER_DEVICE) {
    return -1;  // 유효하지 않은 lbn
  }
  return mapping_table[lbn].pbn;
}

// FTL 초기화
void ftl_open() {
  // 매핑 테이블 메모리 할당
  mapping_table =
      (MappingEntry*)malloc(DATABLKS_PER_DEVICE * sizeof(MappingEntry));

  // 매핑 테이블 초기화
  for (int i = 0; i < DATABLKS_PER_DEVICE; i++) {
    mapping_table[i].lbn = i;           // 오름차순으로 lbn 할당
    mapping_table[i].pbn = -1;          // 초기 pbn은 -1 (할당되지 않음)
    mapping_table[i].last_offset = -1;  // 초기 offset도 -1 (사용되지 않음)

    // 페이지별 lsn 정보 초기화
    for (int j = 0; j < PAGES_PER_BLOCK; j++) {
      mapping_table[i].page_lsns[j] = -1;  // 아직 할당되지 않은 상태
    }
  }

  // 프리 블록 리스트 초기화
  free_block_list.head = NULL;
  free_block_list.count = 0;

  // 모든 블록을 프리 블록 리스트에 추가 (오름차순)
  // add_free_block 함수는 리스트 앞에 추가하므로, 역순으로 추가하면 오름차순
  // 정렬이 됨
  for (int i = BLOCKS_PER_DEVICE - 1; i >= 0; i--) {
    add_free_block(i);
  }

  // 초기화 완료 표시
  ftl_initialized = 1;
}

// 데이터 쓰기 함수
void ftl_write(int lsn, char* sectorbuf) {
  // 초기화 상태 확인
  if (!ftl_initialized) {
    printf("Error: FTL not initialized. Call ftl_open() first.\n");
    return;
  }

  if (lsn < 0 || lsn >= DATAPAGES_PER_DEVICE) {
    printf("Invalid LSN: %d\n", lsn);
    return;
  }

  int lbn = lsn / PAGES_PER_BLOCK;  // LSN의 논리 블록 번호
  char pagebuf[PAGE_SIZE];

  // 매핑 테이블에서 pbn 찾기
  int pbn = get_pbn(lbn);

  // 해당 lbn에 대한 pbn이 아직 할당되지 않은 경우
  if (pbn == -1) {
    // 새 물리 블록 할당
    pbn = get_free_block();
    if (pbn == -1) {
      printf("No free blocks available\n");
      return;
    }

    // 매핑 테이블 업데이트
    mapping_table[lbn].pbn = pbn;
    mapping_table[lbn].last_offset = -1;  // 새 블록이므로 last_offset 초기화

    // 페이지 LSN 초기화
    for (int i = 0; i < PAGES_PER_BLOCK; i++) {
      mapping_table[lbn].page_lsns[i] = -1;
    }
  }

  // 페이지 버퍼 준비 (0xFF로 초기화)
  memset(pagebuf, 0xFF, PAGE_SIZE);
  // 섹터 데이터 복사
  memcpy(pagebuf, sectorbuf, SECTOR_SIZE);
  // Spare 영역에 LSN 정보 저장 (바이너리 형태로)
  memcpy(pagebuf + SECTOR_SIZE, &lsn, sizeof(int));

  // 기존 데이터 업데이트 여부 확인
  int update_position = -1;
  for (int i = 0; i <= mapping_table[lbn].last_offset; i++) {
    if (mapping_table[lbn].page_lsns[i] == lsn) {
      update_position = i;
      break;
    }
  }

  // 기존 데이터가 있는 경우 (동일 LSN에 대한 업데이트)
  if (update_position != -1) {
    // 블록 내의 다른 페이지를 새 블록으로 복사
    int new_pbn = get_free_block();
    if (new_pbn == -1) {
      printf("No free blocks available\n");
      return;
    }

    // 기존 블록에서 유효한 페이지 데이터 복사 (업데이트 페이지 제외)
    int new_offset = 0;
    for (int i = 0; i <= mapping_table[lbn].last_offset; i++) {
      if (i != update_position && mapping_table[lbn].page_lsns[i] != -1) {
        char old_pagebuf[PAGE_SIZE];
        int old_ppn = pbn * PAGES_PER_BLOCK + i;

        // 기존 데이터 읽기
        if (fdd_read(old_ppn, old_pagebuf) < 0) {
          printf("Read operation failed for PPN: %d\n", old_ppn);
          return;
        }

        // 새 블록에 순차적으로 쓰기
        int new_ppn = new_pbn * PAGES_PER_BLOCK + new_offset;
        if (fdd_write(new_ppn, old_pagebuf) < 0) {
          printf("Write operation failed for PPN: %d\n", new_ppn);
          return;
        }

        // 새 블록의 페이지 LSN 정보 업데이트
        mapping_table[lbn].page_lsns[new_offset] =
            mapping_table[lbn].page_lsns[i];
        new_offset++;
      }
    }

    // 새로운 데이터를 새 블록의 다음 위치에 쓰기
    int new_ppn = new_pbn * PAGES_PER_BLOCK + new_offset;
    if (fdd_write(new_ppn, pagebuf) < 0) {
      printf("Write operation failed for PPN: %d\n", new_ppn);
      return;
    }

    // 매핑 테이블 업데이트
    mapping_table[lbn].page_lsns[new_offset] = lsn;
    mapping_table[lbn].pbn = new_pbn;
    mapping_table[lbn].last_offset = new_offset;

    // 기존 블록 삭제 후 프리 리스트에 추가
    fdd_erase(pbn);
    add_free_block(pbn);
  } else {
    // 블록 내 다음 가용 위치 찾기
    int next_offset = mapping_table[lbn].last_offset + 1;

    if (next_offset < PAGES_PER_BLOCK) {
      // 블록 내에 여유 공간이 있는 경우
      int ppn = pbn * PAGES_PER_BLOCK + next_offset;

      // 데이터 쓰기
      if (fdd_write(ppn, pagebuf) < 0) {
        printf("Write operation failed for PPN: %d\n", ppn);
        return;
      }

      // 매핑 테이블 업데이트
      mapping_table[lbn].page_lsns[next_offset] = lsn;
      mapping_table[lbn].last_offset = next_offset;
    } else {
      // 블록이 꽉 찬 경우, 새 블록으로 교체하고 데이터 복사
      int new_pbn = get_free_block();
      if (new_pbn == -1) {
        printf("No free blocks available\n");
        return;
      }

      // 기존 블록에서 유효한 페이지 데이터 복사
      int new_offset = 0;
      for (int i = 0; i < PAGES_PER_BLOCK; i++) {
        int current_lsn = mapping_table[lbn].page_lsns[i];

        if (current_lsn != -1) {
          char old_pagebuf[PAGE_SIZE];
          int old_ppn = pbn * PAGES_PER_BLOCK + i;

          // 기존 데이터 읽기
          if (fdd_read(old_ppn, old_pagebuf) < 0) {
            printf("Read operation failed for PPN: %d\n", old_ppn);
            return;
          }

          // 새 블록에 순차적으로 쓰기
          int new_ppn = new_pbn * PAGES_PER_BLOCK + new_offset;
          if (fdd_write(new_ppn, old_pagebuf) < 0) {
            printf("Write operation failed for PPN: %d\n", new_ppn);
            return;
          }

          // 새 블록의 페이지 LSN 정보 업데이트
          mapping_table[lbn].page_lsns[new_offset] = current_lsn;
          new_offset++;
        }
      }

      // 새로운 데이터를 새 블록의 다음 위치에 쓰기
      int new_ppn = new_pbn * PAGES_PER_BLOCK + new_offset;
      if (fdd_write(new_ppn, pagebuf) < 0) {
        printf("Write operation failed for PPN: %d\n", new_ppn);
        return;
      }

      // 매핑 테이블 업데이트
      mapping_table[lbn].pbn = new_pbn;
      mapping_table[lbn].page_lsns[new_offset] = lsn;
      mapping_table[lbn].last_offset = new_offset;

      // 기존 블록 삭제 후 프리 리스트에 추가
      fdd_erase(pbn);
      add_free_block(pbn);
    }
  }
}

// 데이터 읽기 함수
void ftl_read(int lsn, char* sectorbuf) {
  // 초기화 상태 확인
  if (!ftl_initialized) {
    printf("Error: FTL not initialized. Call ftl_open() first.\n");
    return;
  }

  if (lsn < 0 || lsn >= DATAPAGES_PER_DEVICE) {
    printf("Invalid LSN: %d\n", lsn);
    return;
  }

  int lbn = lsn / PAGES_PER_BLOCK;  // LSN의 논리 블록 번호
  char pagebuf[PAGE_SIZE];

  // 매핑 테이블에서 pbn 찾기
  int pbn = get_pbn(lbn);

  // 해당 lbn에 대한 pbn이 아직 할당되지 않은 경우
  if (pbn == -1) {
    memset(sectorbuf, 0xFF,
           SECTOR_SIZE);  // 할당되지 않은 블록은 0xFF로 초기화된 값 반환
    return;
  }

  // 블록 내 모든 페이지를 살펴보며 해당 LSN이 저장된 페이지 찾기
  int page_index = -1;
  for (int i = 0; i <= mapping_table[lbn].last_offset; i++) {
    if (mapping_table[lbn].page_lsns[i] == lsn) {
      page_index = i;
      break;  // 해당 LSN을 찾으면 즉시 중단
    }
  }

  // 해당 LSN에 대한 페이지를 찾지 못한 경우
  if (page_index == -1) {
    memset(sectorbuf, 0xFF, SECTOR_SIZE);
    return;
  }

  // 실제 물리 페이지 번호 계산
  int ppn = pbn * PAGES_PER_BLOCK + page_index;

  // 데이터 읽기
  if (fdd_read(ppn, pagebuf) < 0) {
    printf("Read operation failed for PPN: %d\n", ppn);
    return;
  }

  // pagebuf에서 섹터 데이터만 sectorbuf로 복사
  memcpy(sectorbuf, pagebuf, SECTOR_SIZE);
}

// 매핑 테이블과 프리 블록 리스트 출력
void ftl_print() {
  printf("lbn pbn last_offset\n");

  for (int i = 0; i < DATABLKS_PER_DEVICE; i++) {
    printf("%d %d %d\n", mapping_table[i].lbn, mapping_table[i].pbn,
           mapping_table[i].last_offset);
  }
}

// 프리 블록 리스트 조회 함수
void ftl_print_free_blocks() {
  printf("Free Blocks List: ");

  // 프리 블록이 없는 경우
  if (free_block_list.count == 0) {
    printf("No free blocks available\n");
    return;
  }

  // 프리 블록 리스트 순회하며 출력
  FreeBlockNode* node = free_block_list.head;
  while (node != NULL) {
    printf("%d ", node->pbn);
    node = node->next;
  }
  printf("\n");

  // 프리 블록 총 개수 출력
  printf("Total free blocks: %d\n", free_block_list.count);
}

// FTL 종료
void ftl_close() {
  // 매핑 테이블 메모리 해제
  free(mapping_table);

  // 프리 블록 리스트 메모리 해제
  FreeBlockNode* node = free_block_list.head;
  while (node != NULL) {
    FreeBlockNode* next = node->next;
    free(node);
    node = next;
  }

  // 초기화 상태 해제
  ftl_initialized = 0;
}