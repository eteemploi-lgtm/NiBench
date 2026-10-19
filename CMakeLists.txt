cmake_minimum_required(VERSION 3.16)
project(NiBench VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

add_executable(NiBench
    src/main.cpp
    src/MainWindow.cpp src/MainWindow.h
    src/Widgets.cpp src/Widgets.h
    src/NiApi.cpp src/NiApi.h
    src/DriverProbe.cpp src/DriverProbe.h
)
target_link_libraries(NiBench PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)
if(MSVC)
    target_compile_options(NiBench PRIVATE /utf-8)
endif()

# ---------------------------------------------------------------- NI-DAQmx
option(NIBENCH_WITH_NI "Compiler avec NI-DAQmx (acces materiel reel)" ON)
set(NI_DAQMX_DIR "C:/Program Files (x86)/National Instruments/Shared/ExternalCompilerSupport/C"
    CACHE PATH "Dossier ExternalCompilerSupport/C de NI-DAQmx (contient include/ et lib64/msvc/)")
if(NIBENCH_WITH_NI)
    target_compile_definitions(NiBench PRIVATE WITH_NI)
    target_include_directories(NiBench PRIVATE "${NI_DAQMX_DIR}/include")
    target_link_directories(NiBench PRIVATE "${NI_DAQMX_DIR}/lib64/msvc")
    target_link_libraries(NiBench PRIVATE NIDAQmx)
endif()
