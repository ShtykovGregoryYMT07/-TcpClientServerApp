cmake_minimum_required(VERSION 3.5)

project(TestTaskTcpClient LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Boost REQUIRED COMPONENTS serialization system)

set(SOURCES
    main.cpp
    )

set(HEADERS
    ../ClientTask.h
    )

if(Boost_FOUND)
  include_directories(${Boost_INCLUDE_DIRS})

  # Добавим исполняемый файл для компиляции
  add_executable(TestTaskTcpClient main.cpp ../Logger.cpp)

  # Целевая ссылка на библиотеки Boost
  target_link_libraries(TestTaskTcpClient PRIVATE Boost::serialization Boost::system)
endif()


