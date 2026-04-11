add_library(echo_test_doubles
  doubles/fake_provider.c
  doubles/fake_crypto.c
)

target_include_directories(echo_test_doubles PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/doubles
)

add_executable(test_chunker
  unit/test_chunker.c
)

target_link_libraries(test_chunker
  echo_core
  echo_test_doubles
)

add_test(NAME test_chunker COMMAND test_chunker)

add_executable(test_manifest
  unit/test_manifest.c
)

target_compile_definitions(test_manifest PRIVATE
  TEST_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
)

target_link_libraries(test_manifest
  echo_core
  echo_test_doubles
)

add_test(NAME test_manifest COMMAND test_manifest)

add_executable(test_util
  unit/test_util.c
)

target_compile_definitions(test_util PRIVATE
  TEST_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
)

target_link_libraries(test_util
  echo_core
  echo_test_doubles
)

add_test(NAME test_util COMMAND test_util)

add_executable(test_crypto
  unit/test_crypto.c
)

target_link_libraries(test_crypto
  echo_core
  echo_test_doubles
)

add_test(NAME test_crypto COMMAND test_crypto)

add_executable(test_localfs_provider
  integration/test_localfs_provider.c
)

target_link_libraries(test_localfs_provider
  echo_core
  echo_test_doubles
)

add_test(NAME test_localfs_provider COMMAND test_localfs_provider)

add_executable(test_upload_download_roundtrip
  integration/test_upload_download_roundtrip.c
)

target_compile_definitions(test_upload_download_roundtrip PRIVATE
  TEST_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
)

target_link_libraries(test_upload_download_roundtrip
  echo_core
  echo_test_doubles
)

add_test(NAME test_upload_download_roundtrip COMMAND test_upload_download_roundtrip)
