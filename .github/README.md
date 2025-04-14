# 과제 3: Flash Memory에서의 Hybrid Mapping FTL 구현

## 1. 개요

Hybrid mapping 기법(강의자료 “Flash Memory Overview”의 21쪽)을 따르는 FTL을 구현한다. 다음과 같은 제약사항들을 지켜야 한다.
- 파일 I/O 연산: system call 또는 C 라이브러리만을 사용한다.
- 구현 파일: 아래의 (1), (2), (3), (4)의 기능을 ftlmgr.c에 구현한다.
- 수정 불가 파일: hybridmapping.h와 fdevicedriver.c는 주어진 그대로 사용하며 예외를 제외하고 수정되어서는 안된다. (자세한 사항은 두 개 파일 내부의 설명 참조)
- 테스트: 네 개의 함수를 테스트하기 위해 main() 함수를 직접 작성하며, file system의 역할을 수행하는 main() 함수 내의 시나리오로는 다음이 포함되어야 한다.
-    (1) flash memory 파일 생성 및 초기화
-    (2) ftl_open() 호출
-    (3) 여러 개의 ftl_write()나 ftl_read() 호출을 통한 테스트
테스트 프로그램 실행 시 이전에 사용했던 flash memory 파일이 존재하면 이를 재사용하지 않고 새로운 flash memory 파일을 생성하여 사용한다.

### (1) ftl_open() 구현
- FTL 구현을 위해 논리주소를 물리주소로 변환하는 address mapping table이 필요하다. Hybrid mapping FTL에 맞는 address mapping table을 구상하고 이에 맞는 data
structure를 정의하여 사용한다 (강의자료 참조). 강의 시간에 언급한 대로 해당 블록에서 첫 번째 빈 페이지를 빠르게 찾기 위해 mapping table에 last_offset과 같은 컬럼을 추가한다.
- ftl_open()에서 해야 할 일:
- 위 data structure를 이용하여 address mapping table을 생성한다. 이때 hybridmapping.h에 정의된 상수 변수를 반드시 활용한다.
- 주소 매핑 테이블에서 각 lbn에 대응되는 pbn의 값을 모두 -1로 초기화한다. 여기서 lbn은 0, 1, …, (DATABLKS_PER_DEVICE-1) 값을 갖는다 (자세한 사항은
hybridmapping.h 참조). 또한, last_offset의 값도 모두 -1로 초기화한다.
- 플래시 메모리 내의 free block list를 관리하기 위한 data structure를 만들되, 반드시 linked list 형태로 작성한다. 초기 free block linked list의 전체 노드 수는
전체 블록의 수, 즉 BLOCKS_PER_DEVICE와 동일하며, 각 노드에는 pbn이 저장된다. 초기에 linked list의 각 노드의 pbn은 0, 1, …, (BLOCKS_PER_DEVICE-1)과 같이
오름차순으로 정렬되어 있어야 한다.
- free block 요청 시 linked list의 헤더가 가리키는 노드의 pbn을 할당하며, garbage block이 발생하면 반드시 erase 연산 후 linked list의 맨 앞에 삽입한다.
-    (필요 시) free block 관리를 위한 별도의 함수들을 작성할 것.
- 주의: file system에서 ftl_write()나 ftl_read()를 최초로 호출하기 전에 반드시 초기화 관련 ftl_open()를 호출해야 한다.

### (2) ftl_write(int lsn, char *sectorbuf) 구현
- File system이 ftl_write()를 호출할 때 인자로 lsn(=lpn)과 sectorbuf에 저장된 512B 데이터를 전달한다.
- FTL은 이 데이터를 flash memory에 기록해야 하며, 어떤 물리적 페이지(ppn)에 쓸지 hybrid mapping 기법의 동작 원리에 따라 결정되어야 한다. 또한, address mapping
table의 갱신이 필요하다.
- 데이터 쓰기:
- FTL과 flash memory 파일 간의 데이터 쓰기는 반드시 페이지 단위로 이루어져야 하며, flash device driver의 fdd_write() 함수 호출을 통해 처리한다.
- fdd_write() 호출 전에 pagebuf의 sector 영역에 ftl_write()의 sectorbuf 데이터를 저장하고, spare 영역에는 lsn을 저장한다.
- lsn 저장: spare 영역 맨 왼쪽에 lsn을 저장하며, lsn의 크기는 4B이다. 이 저장 여부를 통해 해당 페이지가 비어있는지 판별할 수 있다.
- 만약 해당 블록 내에 빈 페이지가 존재하지 않는다면, 빈 블록을 하나 할당한 후 복사 등의 복잡한 연산이 필요할 수 있다.
- 단, 새로운 블록은 반드시 ftl_write()의 인자로 받은 lsn과 데이터를 포함하여 최신의 데이터만 복사되어야 함.
- 복사 대상 블록은 flash device driver의 fdd_erase()를 통해 초기화되며, free block linked list에 추가되어야 한다.
- 주의:
- fdd_write() 호출 전에 pagebuf에 sector 데이터와 spare 데이터를 저장할 때 memcpy() 등 적절한 함수를 사용할 것.
- spare 영역에 lsn을 저장할 때 반드시 binary integer 모드로 저장할 것 (ASCII 모드 사용 시 채점 시 오류 발생).
- flash memory 파일을 직접 read(fread), write(fwrite) 등으로 처리해서는 안되며, 반드시 fdevicedriver.c의 함수를 사용할 것.

### (3) ftl_read(int lsn, char *sectorbuf) 구현
- File system이 ftl_read()를 호출하면, FTL은 인자로 주어진 lsn(=lpn)을 이용하여 pbn을 구한 후, 최신 데이터를 저장한 페이지를 찾아 인자로 주어진 sectorbuf에 복사하여
전달한다. (이때 페이지의 spare 영역은 복사할 필요 없음)
- FTL은 flash memory 파일로부터 데이터를 읽을 때도 반드시 페이지 단위로 읽어와야 하며, 이를 위해 flash device driver의 fdd_read() 함수를 호출한다.
- 페이지를 읽은 후 ftl_read()의 sectorbuf로 sector 데이터를 복사할 때 memcpy() 등을 사용하여 처리할 것을 권장한다.
- 주의: flash memory 파일을 직접 read(fread), write(fwrite) 등으로 처리해서는 안 된다. 반드시 fdevicedriver.c의 함수를 사용할 것.

### (4) ftl_print() 구현
- ftl_print() 함수는 FTL의 address mapping table을 확인하기 위한 용도로 사용된다.
- 출력 내용은 화면에 lbn, pbn, last_offset을 출력해야 한다.
- 예를 들어, flash memory의 전체 블록 수가 8인 경우 아래와 같이 출력될 수 있음:

```txt
lbn pbn last_offset
0 7 0
1 -1 -1
2 1 3
3 2 1
4 3 4
5 -1 -1
6 4 2
```

- 주의: ftl_print() 호출 시 반드시 위와 같은 출력 포맷(숫자와 숫자 사이 공백 한 칸)을 사용해야 하며, 이를 지키지 않으면 채점 프로그램에서 인식하지 못함.
- Address mapping table의 pbn은 free block linked list의 관리 정책과 연동되므로, ftl_open()에서 언급한 관리 정책을 반드시 따라야 한다.

2. 개발 환경
   - OS: Linux 우분투 버전 22.04 LTS
   (Ubuntu 홈페이지에서 버전 확인 가능)
   - 컴파일러: gcc 13.2
   채점 환경이 위와 동일하므로 프로그램 개발 환경도 이에 맞추도록 권장함. 환경 미준수로 인한 불이익은 본인 책임.

3. 제출물
   - 제출 파일: 프로그래밍한 소스파일 ftlmgr.c
   - 하위 폴더 없이 최상위 위치에 저장할 것.
   - 압축 파일 제출:
   - 파일 이름은 모두 소문자로 작성한다.
   - 제출 파일은 zip 파일로 압축하여 스마트캠퍼스 lms.ssu.ac.kr의 과제 게시판에 제출할 것.
   - 파일 이름 예시: 20201084_3.zip (여기서 3은 세 번째 과제를 의미함).
   - 주의:
   - 채점은 채점 프로그램을 통해 자동으로 처리되므로, 위의 사항들을 준수하지 않을 경우 채점 점수가 0점이 될 수 있음.
   - 준수하지 않아 발생하는 불이익은 본인이 책임져야 함.

---

# 과제 3 채점 기준 (총 100점)

## 1. 업로드 양식 및 파일의 온전성
   - 업로드 양식에 이상이 없으며, 모든 C 소스파일이 온전하고 컴파일에 문제가 없으면 20점 부여
   - 파일에 이상이 있을 경우 0점 부여
   -    (여기서 ‘온전’하다는 것은 열심히 프로그래밍하였다는 흔적을 의미)
## 2. 기능별 점수 부여
   -    (1) ftl_read()
   - 기본 읽기: 15점
   - 추가 읽기: 15점
   - Address mapping table 출력이 부정확할 경우 10점 감점
   -    (2) ftl_write()
   - 기본 쓰기: 15점
   - 추가 쓰기: 25점
   - Address mapping table 출력이 부정확할 경우 10점 감점
   -    (3) ftl_open()
   - 기능 및 실행 결과가 정확할 경우 10점 부여, 그렇지 않을 경우 0점
## 3. 제출 시간
   - 하루 늦게 제출할 때마다 20점 감점
## 4. Copy Detection
   - copy detection 프로그램을 통해 copy rate가 90 이상인 경우 소스 확인 후 해당 두 학생에게 0점의 과제 점수를 부여
   - 두 번 이상 적발될 경우 F 학점 부여
   -    (자세한 내용은 강의 계획서 참조)
