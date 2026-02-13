file(GLOB IRR_SOURCES
  core/*.cpp
  probes/*.cpp
  report/*.cpp
  main.cpp
)

add_executable(irr ${IRR_SOURCES})

target_include_directories(irr PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(irr PRIVATE pthread)
