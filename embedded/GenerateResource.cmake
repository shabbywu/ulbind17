file(READ "${INPUT}" _bytes HEX)
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," _bytes "${_bytes}")
get_filename_component(_directory "${OUTPUT}" DIRECTORY)
file(MAKE_DIRECTORY "${_directory}")
file(WRITE "${OUTPUT}"
  "// Generated from the selected Ultralight SDK.\n#include <ulbind17/resources/sdk_resources.hpp>\nnamespace ulbind17::resources {\nalignas(16) const unsigned char ${NAME}_data[] = {${_bytes}};\nconst std::size_t ${NAME}_size = sizeof(${NAME}_data);\n}\n")
