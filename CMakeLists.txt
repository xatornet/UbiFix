cmake_minimum_required(VERSION 3.15)
project(GhostReconPathFix)

set(CMAKE_CXX_STANDARD 17)

# Descargar MinHook automáticamente
include(FetchContent)
FetchContent_Declare(
    minhook
    GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
    GIT_TAG master
)
FetchContent_MakeAvailable(minhook)

# Crear la librería compartida (el plugin ASI)
add_library(PathFix SHARED src/main.cpp)

# Enlazar MinHook
target_link_libraries(PathFix PRIVATE minhook)

# Configurar para que la salida sea un archivo .asi en lugar de .dll
set_target_properties(PathFix PROPERTIES SUFFIX ".asi")
