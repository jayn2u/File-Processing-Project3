cmake_minimum_required(VERSION 3.30)
project(FileProcessingProject3 C)

set(CMAKE_C_STANDARD 11)

# 기본 프로젝트 빌드 설정
add_executable(File_Processing_Project3 main.c ftlmgr.c fdevicedriver.c)

# 테스트 파일들을 위한 실행 파일 추가
add_executable(test_ftl_open tests/test_ftl_open.c ftlmgr.c fdevicedriver.c)
add_executable(test_ftl_read tests/test_ftl_read.c ftlmgr.c fdevicedriver.c)
add_executable(test_ftl_write tests/test_ftl_write.c ftlmgr.c fdevicedriver.c)
