idf_component_register(
  SRCS
    "../../src/model/ir_model.cpp"
    "../../src/ir/ir_sender.cpp"
    "../../src/net/trigger_router.cpp"
    "../../src/storage/config_manager.cpp"
    "../../src/system/boot_manager.cpp"
  INCLUDE_DIRS
    "../../src"
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
