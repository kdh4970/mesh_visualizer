cmake_minimum_required(VERSION 3.5)
project(mesh_visualizer CXX CUDA)

# set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} --gpu-architecture compute_86")
find_package(CUDA REQUIRED)
find_package(OpenGL)
find_package(GLUT)
set(CUDA_INCLUDE_DIRS /usr/local/cuda/include)


if(CUDA_FOUND)
    message(STATUS "CUDA found")
    set(cuda_src src/main.cpp)

    # make executable
    add_executable(mesh_visualizer ${cuda_src})

else()
    message(STATUS "CUDA not found")
endif()

target_include_directories(mesh_visualizer PUBLIC ${CUDA_INCLUDE_DIRS} include)

target_link_libraries(mesh_visualizer ${OPENGL_LIBRARY} ${GLUT_LIBRARY})
target_link_libraries(mesh_visualizer ${CUDA_LIBRARIES})
