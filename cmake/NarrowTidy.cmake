include_guard(GLOBAL)

# Opt an existing application/library target into the bundled Narrow policy.
function(narrow_enable_tidy target)
  if(CMAKE_VERSION VERSION_LESS 3.27)
    message(FATAL_ERROR "narrow_enable_tidy requires CMake 3.27+ for CXX_CLANG_TIDY generator expressions")
  endif()
  if(NOT CMAKE_GENERATOR MATCHES "Ninja|Makefiles")
    message(FATAL_ERROR "narrow_enable_tidy requires a Ninja or Makefiles generator")
  endif()
  if(NOT TARGET NarrowTidy)
    message(FATAL_ERROR "narrow_enable_tidy requires NARROW_BUILD_PLUGIN=ON")
  endif()
  if(NOT TARGET "${target}")
    message(FATAL_ERROR "narrow_enable_tidy: target '${target}' does not exist")
  endif()

  get_target_property(aliased "${target}" ALIASED_TARGET)
  if(aliased)
    set(target "${aliased}")
  endif()
  get_target_property(imported "${target}" IMPORTED)
  get_target_property(type "${target}" TYPE)
  set(supported_types EXECUTABLE STATIC_LIBRARY SHARED_LIBRARY MODULE_LIBRARY OBJECT_LIBRARY)
  if(imported OR NOT type IN_LIST supported_types OR target STREQUAL "NarrowTidy")
    message(FATAL_ERROR "narrow_enable_tidy requires a local application or library target")
  endif()

  # Read settings from the module target, rather than depending on variables in
  # the caller's directory scope (which differs under FetchContent).
  get_target_property(tidy NarrowTidy NARROW_TIDY_EXECUTABLE)
  get_target_property(source_dir NarrowTidy SOURCE_DIR)
  set(policy "${source_dir}/.clang-tidy")

  if(NOT TARGET NarrowTidyVerify)
    # Always verify before a selected target builds, including otherwise
    # up-to-date builds: LLVM can ignore dlopen failures and exit successfully.
    # TARGET_FILE also makes verification depend on the module's build.
    add_custom_target(NarrowTidyVerify
      COMMAND "${tidy}" "--load=$<TARGET_FILE:NarrowTidy>"
        "--config-file=${policy}" --verify-config
      COMMENT "Verifying the Narrow C++ clang-tidy module"
      VERBATIM)
  endif()

  # A custom-command dependency is also an ordering barrier for Ninja, so the
  # module has finished linking before clang-tidy runs during compilation.
  add_dependencies("${target}" NarrowTidyVerify)
  set_target_properties("${target}" PROPERTIES
    CXX_CLANG_TIDY "${tidy};--load=$<TARGET_FILE:NarrowTidy>;--config-file=${policy}")
endfunction()
