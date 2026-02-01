add_executable(wav-verify wav_verify_main.cpp)
target_compile_features(wav-verify PUBLIC cxx_std_20)
target_compile_options(wav-verify PUBLIC -I${CMAKE_SOURCE_DIR}/tests)
install(TARGETS wav-verify RUNTIME DESTINATION bin COMPONENT tools)
