cmake_minimum_required(VERSION 2.8.8)

include_directories(
	${PROJECT_SOURCE_DIR}
)
add_executable(banip-cli banip-cli.c)
target_link_libraries(banip-cli banip)

install(TARGETS banip-cli RUNTIME DESTINATION bin)
