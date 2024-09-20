add_library(
  embedded_resources
  ulbind17/fonts/FreeUniversal-Regular.cpp
  ulbind17/fonts/FreeUniversal-Regular.h
  ulbind17/resources/cacert.cpp
  ulbind17/resources/cacert.h
  ulbind17/resources/icudt67l.cpp
  ulbind17/resources/icudt67l.h
  ulbind17/setup.hpp)

if(NOT WIN32)
  target_compile_options(embedded_resources PUBLIC -fPIC)
endif()