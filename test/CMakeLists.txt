option(WITH_TEST_INTERNAL "Build and run internal tests" ON)
option(WITH_TEST_CORRECTNESS "Build correctness tests" ON)
option(WITH_TEST_ERROR "Build error tests" ON)
option(WITH_TEST_WARNING "Build warning tests" ON)
option(WITH_TEST_PERFORMANCE "Build performance tests" ON)
option(WITH_TEST_OPENGL "Build OpenGL tests" ON)
option(WITH_TEST_GENERATORS "Build Generator tests" ON)
option(WITH_TEST_RENDERSCRIPT "Build Renderscript tests" ON)

if (MSVC)
else()
  # src/CMakeLists.txt always enables C++11 for non-Windows,
  # so we'll give the same courtesy to tests
  add_definitions("-std=c++11")
  add_definitions("-fno-rtti")
endif()

if (WITH_TEST_INTERNAL)
  message(STATUS "Internal tests enabled")
  halide_project(test_internal internal internal.cpp)
  # Run internal tests after building
  add_custom_command(TARGET test_internal
                     POST_BUILD
                     COMMAND test_internal
                     COMMENT "Running internal tests")
else()
  message(WARNING "Internal tests disabled")
endif()

function(tests folder)
  file(GLOB TESTS RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/${folder}" "${CMAKE_CURRENT_SOURCE_DIR}/${folder}/*.cpp")
  if (WIN32)
    LIST(REMOVE_ITEM TESTS "simd_op_check.cpp") # Relies on shell stuff that doesn't work on windows
  endif()
  foreach(file ${TESTS})
    string(REPLACE ".cpp" "" name "${file}")
    # Test links against libHalide
    halide_project("${folder}_${name}" "${folder}" "${folder}/${file}")
  endforeach()
endfunction(tests)

if (WITH_TEST_CORRECTNESS)
  tests(correctness)
endif()
if (WITH_TEST_ERROR)
  tests(error)
endif()
if (WITH_TEST_WARNING)
  tests(warning)
endif()
if (WITH_TEST_PERFORMANCE)
  tests(performance)
endif()
if (WITH_TEST_OPENGL)
  tests(opengl)
endif()
if (WITH_TEST_RENDERSCRIPT)
  tests(renderscript)
endif()

if (WITH_TEST_GENERATORS)
  # Generator tests for just-in-time mode:
  # For foo_generator.cpp, assume it contains a single generator named 'foo'
  # and make an executable target for it named 'generator_foo'
  file(GLOB TESTS RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/generator" "${CMAKE_CURRENT_SOURCE_DIR}/generator/*_jittest.cpp")
  foreach(test ${TESTS})
    string(REPLACE "_jittest.cpp" "" name "generator_jit_${test}")
    halide_project(${name} "generator" "generator/${test}")
  endforeach()

  # Generator tests for ahead-of-time mode:

  # Create tests for ahead of-time-compiled generators. This will produce two
  # executables, one containing the generator itself
  # (e.g. from example_generator.cpp) and used at build time, and the other, the
  # test that executes the generated code (e.g. from example_aottest.cpp).

  # For foo_generator.cpp, assume it contains a single generator named 'foo'
  # and make an executable target for it named 'generator_foo' that contains the
  # generator executable.

  include(../HalideGenerator.cmake)

  # First create Halide linked executables for the generators using
  # halide_project(...). These are used during the build.
  file(GLOB GENS RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/generator" "${CMAKE_CURRENT_SOURCE_DIR}/generator/*_generator.cpp")

  # Create the object file generator targets ("generator executables")
  set(OBJ_GEN_EXE_SUFFIX ".generator")
  foreach(gen_src ${GENS})
    string(REPLACE "_generator.cpp" "" gen_name "${gen_src}${OBJ_GEN_EXE_SUFFIX}")
    # tiled_blur also needs tiled_blur_blur
    if(gen_name STREQUAL "tiled_blur${OBJ_GEN_EXE_SUFFIX}")
      halide_project(${gen_name} "generator" "${CMAKE_CURRENT_SOURCE_DIR}/../tools/GenGen.cpp" "generator/${gen_src}" "generator/tiled_blur_blur_generator.cpp")
    else()
      halide_project(${gen_name} "generator" "${CMAKE_CURRENT_SOURCE_DIR}/../tools/GenGen.cpp" "generator/${gen_src}")
    endif()
  endforeach()

  # Next create the test case targets. For each target
  # set up dependencies so that
  # - the generator executables will be invoked at build time with the
  #   arguments specific to that particular test case.
  # - the test case is linked against the generated object files.
  file(GLOB TESTS RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/generator" "${CMAKE_CURRENT_SOURCE_DIR}/generator/*_aottest.cpp")
  foreach(TEST_SRC ${TESTS})

    string(REPLACE "_aottest.cpp" "" GEN_NAME "${TEST_SRC}")
    string(REPLACE "_aottest.cpp" "" FUNC_NAME "${TEST_SRC}")

    set(TEST_RUNNER "generator_aot_${GEN_NAME}")

    file(TO_NATIVE_PATH "${CMAKE_CURRENT_BINARY_DIR}/" NATIVE_INT_DIR)
    set(SCRATCH_DIR "${NATIVE_INT_DIR}scratch_${GEN_NAME}")
    file(MAKE_DIRECTORY "${SCRATCH_DIR}")

    add_executable("${TEST_RUNNER}" "generator/${TEST_SRC}")

    # Add special case dependencies certain files that globbed above.
    if (TEST_SRC STREQUAL "tiled_blur_interleaved_aottest.cpp")
      # Some special dependencies here:
      # "tiled_blur_interleaved" is produced by using tiled_blur with different
      # generator args.
      halide_add_generator_dependency(TARGET "${TEST_RUNNER}"
                               GENERATOR_TARGET "tiled_blur${OBJ_GEN_EXE_SUFFIX}"
                               GENERATOR_NAME "tiled_blur"
                               GENERATED_FUNCTION "tiled_blur_interleaved"
                               GENERATOR_ARGS "target=host" "is_interleaved=true")
      # "tiled_blur_blur_interleaved" is produced by using tiled_blur_blur with
      # different generator args.
      halide_add_generator_dependency(TARGET "${TEST_RUNNER}"
                               GENERATOR_TARGET "tiled_blur_blur${OBJ_GEN_EXE_SUFFIX}"
                               GENERATOR_NAME "tiled_blur_blur"
                               GENERATED_FUNCTION "tiled_blur_blur_interleaved"
                               GENERATOR_ARGS "target=host" "is_interleaved=true")
    elseif(TEST_SRC STREQUAL "nested_externs_aottest.cpp")

      halide_add_generator_dependency(TARGET "${TEST_RUNNER}"
                               GENERATOR_TARGET "nested_externs${OBJ_GEN_EXE_SUFFIX}"
                               GENERATOR_NAME "nested_externs_root"
                               GENERATED_FUNCTION "nested_externs_root"
                               GENERATOR_ARGS "target=host")
      halide_add_generator_dependency(TARGET "${TEST_RUNNER}"
                               GENERATOR_TARGET "nested_externs${OBJ_GEN_EXE_SUFFIX}"
                               GENERATOR_NAME "nested_externs_inner"
                               GENERATED_FUNCTION "nested_externs_inner"
                               GENERATOR_ARGS "target=host")
      halide_add_generator_dependency(TARGET "${TEST_RUNNER}"
                               GENERATOR_TARGET "nested_externs${OBJ_GEN_EXE_SUFFIX}"
                               GENERATOR_NAME "nested_externs_combine"
                               GENERATED_FUNCTION "nested_externs_combine"
                               GENERATOR_ARGS "target=host")
      halide_add_generator_dependency(TARGET "${TEST_RUNNER}"
                               GENERATOR_TARGET "nested_externs${OBJ_GEN_EXE_SUFFIX}"
                               GENERATOR_NAME "nested_externs_leaf"
                               GENERATED_FUNCTION "nested_externs_leaf"
                               GENERATOR_ARGS "target=host")
    elseif(TEST_SRC STREQUAL "user_context_aottest.cpp")
      halide_add_generator_dependency(TARGET "${TEST_RUNNER}"
                               GENERATOR_TARGET "${GEN_NAME}${OBJ_GEN_EXE_SUFFIX}"
                               GENERATOR_NAME "${GEN_NAME}"
                               GENERATED_FUNCTION "${FUNC_NAME}"
                               GENERATOR_ARGS "target=host-user_context")
    elseif(TEST_SRC STREQUAL "user_context_insanity_aottest.cpp")
      halide_add_generator_dependency(TARGET "${TEST_RUNNER}"
                               GENERATOR_TARGET "${GEN_NAME}${OBJ_GEN_EXE_SUFFIX}"
                               GENERATOR_NAME "${GEN_NAME}"
                               GENERATED_FUNCTION "${FUNC_NAME}"
                               GENERATOR_ARGS "target=host-user_context")
    elseif(TEST_SRC STREQUAL "multitarget_aottest.cpp")
      # Needs custom target string
      halide_add_generator_dependency(TARGET "${TEST_RUNNER}"
                               GENERATOR_TARGET "${GEN_NAME}${OBJ_GEN_EXE_SUFFIX}"
                               GENERATOR_NAME "${GEN_NAME}"
                               GENERATED_FUNCTION "${FUNC_NAME}"
                               GENERATOR_ARGS "target=host-debug,host")
    # metadata_tester_aottest.cpp depends on two variants of metadata_generator
    elseif(TEST_SRC STREQUAL "metadata_tester_aottest.cpp")
      halide_add_generator_dependency(TARGET "${TEST_RUNNER}"
                               GENERATOR_TARGET "${GEN_NAME}${OBJ_GEN_EXE_SUFFIX}"
                               GENERATOR_NAME "${GEN_NAME}"
                               GENERATED_FUNCTION "${FUNC_NAME}"
                               GENERATOR_ARGS "target=host-register_metadata")
      halide_add_generator_dependency(TARGET "${TEST_RUNNER}"
                               GENERATOR_TARGET "${GEN_NAME}${OBJ_GEN_EXE_SUFFIX}"
                               GENERATOR_NAME "${GEN_NAME}"
                               GENERATED_FUNCTION "${FUNC_NAME}_ucon"
                               GENERATOR_ARGS "target=host-register_metadata-user_context")
    elseif(TEST_SRC STREQUAL "cxx_mangling_aottest.cpp")
      halide_add_generator_dependency(TARGET "${TEST_RUNNER}"
                               GENERATOR_TARGET "${GEN_NAME}${OBJ_GEN_EXE_SUFFIX}"
                               GENERATOR_NAME "${GEN_NAME}"
                               GENERATED_FUNCTION "${FUNC_NAME}"
                               GENERATED_FUNCTION_NAMESPACE "HalideTest::"
                               GENERATOR_ARGS "target=host-c_plus_plus_name_mangling")
    elseif(TEST_SRC STREQUAL "cxx_mangling_define_extern_aottest.cpp")
      halide_add_generator_dependency(TARGET "${TEST_RUNNER}"
                               GENERATOR_TARGET "cxx_mangling${OBJ_GEN_EXE_SUFFIX}"
                               GENERATOR_NAME "cxx_mangling"
                               TARGET_SUFFIX "_2"
                               GENERATED_FUNCTION "cxx_mangling"
                               GENERATED_FUNCTION_NAMESPACE "HalideTest::"
                               GENERATOR_ARGS "target=host-c_plus_plus_name_mangling"
                               OUTPUT_LIB_VAR LIB_cxx_mangling)
      halide_add_generator_dependency(TARGET "${TEST_RUNNER}"
                               GENERATOR_TARGET "${GEN_NAME}${OBJ_GEN_EXE_SUFFIX}"
                               GENERATOR_NAME "${GEN_NAME}"
                               GENERATED_FUNCTION "${FUNC_NAME}"
                               GENERATED_FUNCTION_NAMESPACE "HalideTest::"
                               GENERATOR_ARGS "target=host-c_plus_plus_name_mangling")
      target_link_libraries("${TEST_RUNNER}" ${LIB_cxx_mangling})
    else()
      # All the other foo_test.cpp just depend on foo_generator.cpp
      halide_add_generator_dependency(TARGET "${TEST_RUNNER}"
                               GENERATOR_TARGET "${GEN_NAME}${OBJ_GEN_EXE_SUFFIX}"
                               GENERATOR_NAME "${GEN_NAME}"
                               GENERATED_FUNCTION "${FUNC_NAME}"
                               GENERATOR_ARGS "target=host")
      # tiled_blur_aottest.cpp depends on tiled_blur AND ALSO tiled_blur_blur
      if (TEST_SRC STREQUAL "tiled_blur_aottest.cpp")
        halide_add_generator_dependency(TARGET "${TEST_RUNNER}"
                                 GENERATOR_TARGET "tiled_blur_blur${OBJ_GEN_EXE_SUFFIX}"
                                 GENERATOR_NAME "tiled_blur_blur"
                                 GENERATED_FUNCTION "tiled_blur_blur"
                                 GENERATOR_ARGS "target=host")
      endif()
    endif()
    target_include_directories("${TEST_RUNNER}" PRIVATE "${CMAKE_SOURCE_DIR}/tools")
    target_include_directories("${TEST_RUNNER}" PRIVATE "${CMAKE_SOURCE_DIR}/src/runtime")
    set_target_properties("${TEST_RUNNER}" PROPERTIES FOLDER "generator")
    if (WIN32)
      if (MSVC)
        # /FORCE:multiple allows clobbering the halide runtime symbols in the lib
        set_target_properties("${TEST_RUNNER}" PROPERTIES LINK_FLAGS "/STACK:8388608,1048576 /FORCE:multiple")
      else()
        set_target_properties("${TEST_RUNNER}" PROPERTIES LINK_FLAGS "-Wl,--allow-multiple-definition")
      endif()
    else()
      target_link_libraries("${TEST_RUNNER}" dl pthread z)
      set_target_properties("${TEST_RUNNER}" PROPERTIES LINK_FLAGS "-L ${SCRATCH_DIR}")
    endif()
  endforeach()
endif()
