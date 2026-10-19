# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Release")
  file(REMOVE_RECURSE
  "CMakeFiles\\NiBench_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\NiBench_autogen.dir\\ParseCache.txt"
  "NiBench_autogen"
  )
endif()
