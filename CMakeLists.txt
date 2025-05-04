cmake_minimum_required(VERSION 3.30)
project(FileProcessingProject3 C)

set(CMAKE_C_STANDARD 11)

# 기본 프로젝트 빌드 설정
add_executable(File_Processing_Project3 main.c ftlmgr.c fdevicedriver.c)

# 테스트 파일들을 위한 실행 파일 추가
add_executable(test_ftl_open tests/test_ftl_open.c ftlmgr.c fdevicedriver.c)
add_executable(test_ftl_read tests/test_ftl_read.c ftlmgr.c fdevicedriver.c)
add_executable(test_ftl_write_single_block tests/test_ftl_write_single_block.c ftlmgr.c fdevicedriver.c)
add_executable(test_ftl_write_different_blocks tests/test_ftl_write_different_blocks.c ftlmgr.c fdevicedriver.c)
add_executable(test_ftl_write_overwrite tests/test_ftl_write_overwrite.c ftlmgr.c fdevicedriver.c)
add_executable(test_ftl_read_nonexistent tests/test_ftl_read_nonexistent.c ftlmgr.c fdevicedriver.c)
add_executable(test_ftl_write_repeated tests/test_ftl_write_repeated.c ftlmgr.c fdevicedriver.c)
