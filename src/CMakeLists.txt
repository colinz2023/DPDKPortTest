add_executable(mlx_test
  access_cmdline.cc
  dpdk.cc
  main.cc
  utils.cc
  parse.cc
)

set(DPDK_LIBS
  "-Wl,--whole-archive"
  "-ldpdk"
  "-Wl,--no-whole-archive"
  )

set(CMAKE_CXX_FLAGS
  "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -g"
  )
add_definitions(-mssse3)
add_definitions(-msse4)

target_link_libraries(mlx_test ${DPDK_LIBS})
target_link_libraries(mlx_test pthread dl m ibverbs)
