cmake_minimum_required(VERSION 3.24)

CanopyGenerate(
  greeting
  greeting/greeting.idl
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_BINARY_DIR}/generated
  ""
  yas_binary
  yas_compressed_binary
  yas_json
  protocol_buffers
  install_dir ${GENERATED_INSTALL_DIR})
