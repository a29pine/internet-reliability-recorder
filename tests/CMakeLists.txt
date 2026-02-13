set(TEST_FILES
	test_event_serialization.cpp
	test_parser.cpp
	test_parsing.cpp
	test_percentile.cpp
	test_report.cpp
)

file(GLOB IRR_CORE ${CMAKE_SOURCE_DIR}/src/core/*.cpp)
file(GLOB IRR_PROBES ${CMAKE_SOURCE_DIR}/src/probes/*.cpp)
file(GLOB IRR_REPORT ${CMAKE_SOURCE_DIR}/src/report/*.cpp)

foreach(TF IN LISTS TEST_FILES)
	get_filename_component(TNAME ${TF} NAME_WE)
	add_executable(${TNAME} ${TF} ${IRR_CORE} ${IRR_PROBES} ${IRR_REPORT})
	target_include_directories(${TNAME} PRIVATE ${CMAKE_SOURCE_DIR}/src)
	target_link_libraries(${TNAME} PRIVATE pthread)
	add_test(NAME ${TNAME} COMMAND ${TNAME})
endforeach()
