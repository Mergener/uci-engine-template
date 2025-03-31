set(TARGET your_chess_engine)
set(SRC search.cpp engine.cpp main.cpp)

add_executable(${TARGET}
               ${SRC})

# Link our executable with libuci.
target_link_libraries(${TARGET} PRIVATE libuci)
