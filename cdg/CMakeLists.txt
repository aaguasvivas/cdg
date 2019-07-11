add_library(CDGPass MODULE
    cdg.cpp
    ControlDependence.cpp
    DataDependence.cpp
)

# Use C++11 to compile our pass (i.e., supply -std=c++11).
target_compile_features(CDGPass PRIVATE cxx_range_for cxx_auto_type)

# LLVM is (typically) built with no C++ RTTI. We need to match that;
# otherwise, we'll get linker errors about missing RTTI data.
set_target_properties(CDGPass PROPERTIES COMPILE_FLAGS "-fno-rtti")

set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
