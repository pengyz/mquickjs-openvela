############################################################################
# apps/system/mqjs/CMakeLists.txt — mquickjs `js` NSH builtin (CMake track)
#
# Mirrors the Makefile (Make track): the engine's .c files are compiled
# HERE by the NuttX toolchain so every libc binding stays on the NuttX
# side; sources come from the mquickjs-rs-sdk checkout that the repo
# manifest places at <tree>/external/mquickjs-rs-sdk. Generated artifacts
# are checked in under gen/ (atom header, ridl stdlib table, rs hook
# declarations, aggregate register TUs, framework bindgen headers).
#
# Rust adapter (RS=y): a custom target runs the same crate-local cargo
# build as the Make track (cwd = rust/, so .cargo/config.toml applies --
# MQJS_ENGINE_LINK=external keeps engine objects out of the archive);
# the archive is linked via nuttx_add_extra_library, and DEPENDS on the
# app target orders cargo before the final link.
############################################################################

if(CONFIG_MQJS_JS)

  # The openvela cmake flow processes app directories in multiple passes
  # (kconfig pre/post generation). Guard against duplicate target creation.
  if(TARGET apps_${CONFIG_MQJS_JS_PROGNAME})
    return()
  endif()

  # SDK layout (repo manifest guarantees external/mquickjs-rs-sdk).
  get_filename_component(MQJS_SDK_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../external/mquickjs-rs-sdk" ABSOLUTE)
  if(NOT EXISTS "${MQJS_SDK_DIR}/Cargo.toml")
    message(FATAL_ERROR
      "mqjs: mquickjs-rs-sdk not found at ${MQJS_SDK_DIR} -- sync the repo "
      "manifest (project external/mquickjs-rs-sdk) or clone it there")
  endif()
  set(MQJS_ENGINE_DIR "${MQJS_SDK_DIR}/deps/mquickjs")
  set(MQJS_RS_DIR     "${MQJS_SDK_DIR}/deps/mquickjs-rs")

  # RS switch (mirrors the Makefile): RS=y builds the Rust-adapter form
  # (js_main.c + adapter archive); RS=n builds the C-only checkpoint
  # (js_main_c.c, direct JS_NewContext/JS_Eval + C fallback stubs for the
  # adapter-owned table entry points -- no cargo). CSRCS/CFLAGS are
  # identical in both forms; only MAINSRC and the adapter archive differ.
  if(CONFIG_MQJS_RS)
    set(MQJS_MAINSRC js_main.c)
  else()
    set(MQJS_MAINSRC js_main_c.c)
  endif()

  # Rust target follows the board architecture (dual-mode adapter): std on
  # host Linux (sim), aarch64-unknown-none + panic=abort elsewhere
  # (bare-metal no_std). Explicit whitelist -- anything else fails loudly.
  if(CONFIG_ARCH STREQUAL "arm64")
    set(MQJS_RUST_TARGET aarch64-unknown-none)
    set(MQJS_CARGO_PREFIX ${CMAKE_COMMAND} -E env RUSTFLAGS=-Cpanic=abort)
  elseif(CONFIG_ARCH STREQUAL "sim")
    set(MQJS_RUST_TARGET x86_64-unknown-linux-gnu)
    set(MQJS_CARGO_PREFIX "")
  else()
    message(FATAL_ERROR
      "mqjs: unsupported CONFIG_ARCH '${CONFIG_ARCH}' for the Rust bridge "
      "-- extend the whitelist with an explicit target")
  endif()
  set(MQJS_ADAPTER_A
    "${CMAKE_CURRENT_SOURCE_DIR}/rust/target/${MQJS_RUST_TARGET}/release/libmqjs.a")

  set(MQJS_APP_DEPENDS)
  if(CONFIG_MQJS_RS)
    # Adapter archive: crate-local cargo (cwd = rust/ so .cargo/config.toml
    # applies). Rerun is a cheap no-op when the crate is up to date.
    find_program(MQJS_CARGO_EXEC cargo REQUIRED)
    # Source watch: the adapter leaf's declared path deps (Cargo.toml) —
    # the SDK checkout AND the mquickjs-ui sibling. A change inside any of
    # them must re-run cargo; the glob below must mirror every path-dep
    # root added to rust/Cargo.toml (cargo's own fingerprint alone is NOT
    # consulted unless the custom command re-runs).
    file(GLOB_RECURSE MQJS_RUST_DEPS
      "${CMAKE_CURRENT_SOURCE_DIR}/rust/*.rs"
      "${CMAKE_CURRENT_SOURCE_DIR}/rust/Cargo.toml"
      "${MQJS_SDK_DIR}/deps/mquickjs-rs/src/*.rs"
      "${MQJS_SDK_DIR}/deps/mquickjs-ridl-glue/src/*.rs"
      "${MQJS_SDK_DIR}/ridl-modules/stdlib/src/*.rs"
      "${MQJS_SDK_DIR}/ridl-modules/stdlib/generated/*.rs"
      "${CMAKE_CURRENT_SOURCE_DIR}/../../../external/mquickjs-ui/src/*.rs")
    list(FILTER MQJS_RUST_DEPS EXCLUDE REGEX "/target/")

    # LIBCLANG_PATH for bindgen (mirrors the Makefile rule): keep an
    # externally provided LIBCLANG_PATH only when it really contains a
    # libclang shared library; otherwise resolve one from the system linker
    # cache. envsetup's prebuilts value fails in partial checkouts.
    set(MQJS_CARGO_ENV)
    if(CONFIG_ARCH STREQUAL "arm64")
      list(APPEND MQJS_CARGO_ENV "RUSTFLAGS=-Cpanic=abort")
    endif()
    set(MQJS_LIBCLANG_DIR "")
    if(DEFINED ENV{LIBCLANG_PATH}
       AND EXISTS "$ENV{LIBCLANG_PATH}"
       AND EXISTS "$ENV{LIBCLANG_PATH}/libclang.so")
      set(MQJS_LIBCLANG_DIR "$ENV{LIBCLANG_PATH}")
    else()
      execute_process(
        COMMAND sh -c "ldconfig -p 2>/dev/null | awk '$1 ~ /^libclang/ && $1 !~ /cpp/ {print $NF; exit}'"
        OUTPUT_VARIABLE MQJS_LIBCLANG_SO
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
      if(MQJS_LIBCLANG_SO)
        get_filename_component(MQJS_LIBCLANG_DIR "${MQJS_LIBCLANG_SO}" DIRECTORY)
      endif()
    endif()
    if(MQJS_LIBCLANG_DIR)
      list(APPEND MQJS_CARGO_ENV "LIBCLANG_PATH=${MQJS_LIBCLANG_DIR}")
    endif()
    # (no LIBCLANG_DIR at all -> plain env; the cargo error names the knob)

    add_custom_command(
      OUTPUT "${MQJS_ADAPTER_A}"
      COMMAND ${CMAKE_COMMAND} -E chdir
              "${CMAKE_CURRENT_SOURCE_DIR}/rust"
              ${CMAKE_COMMAND} -E env ${MQJS_CARGO_ENV}
              ${MQJS_CARGO_EXEC} build --release --target ${MQJS_RUST_TARGET}
      DEPENDS ${MQJS_RUST_DEPS}
      VERBATIM)
    add_custom_target(mqjs_rust_adapter ALL DEPENDS "${MQJS_ADAPTER_A}")
    nuttx_add_extra_library("${MQJS_ADAPTER_A}")
    set(MQJS_APP_DEPENDS mqjs_rust_adapter)
  endif()

  nuttx_add_application(
    NAME ${CONFIG_MQJS_JS_PROGNAME}
    SRCS
      ${MQJS_MAINSRC}
      ${MQJS_ENGINE_DIR}/mquickjs.c
      ${MQJS_ENGINE_DIR}/cutils.c
      ${MQJS_ENGINE_DIR}/dtoa.c
      ${MQJS_ENGINE_DIR}/libm.c
      # RIDL-variant stdlib runtime TUs: mqjs_stdlib_impl.c defines the
      # strong js_stdlib (RIDL tables incl. console), require.c implements
      # require(); the register TU comes from the app aggregate (gen/).
      ${MQJS_RS_DIR}/mqjs_stdlib_impl.c
      ${MQJS_RS_DIR}/require.c
      gen/mquickjs_ridl_register.c
    STACKSIZE
      ${CONFIG_MQJS_JS_STACKSIZE}
    PRIORITY
      ${CONFIG_MQJS_JS_PRIORITY}
    INCLUDE_DIRECTORIES
      ${CMAKE_CURRENT_SOURCE_DIR}/gen
      ${MQJS_ENGINE_DIR}
    DEPENDS
      ${MQJS_APP_DEPENDS}
    COMPILE_FLAGS
      -fno-math-errno
      -fno-trapping-math
      -DMQUICKJS_ENABLE_RIDL_EXTENSIONS)

  # RIDL api declarations + rs hook declarations, force-included into every
  # app TU (mirrors the Makefile CFLAGS). SHELL: keeps each -include pair as
  # two real argv items (a plain multi-word entry gets shell-quoted into one
  # filename-with-space).
  target_compile_options(apps_${CONFIG_MQJS_JS_PROGNAME} PRIVATE
    "SHELL:-include mquickjs_ridl_api.h"
    "SHELL:-include mqjs_rs_hooks.h")

  # Nested demo app (kconfig/mkkconfig scan one level, so lvgldemo/Kconfig
  # is sourced from this dir's Kconfig; its CMakeLists DEPENDS on this
  # dir's mqjs_rust_adapter custom target, which exists when MQJS_RS=y --
  # the demo's Kconfig depends on MQJS_JS && MQJS_RS).
  if(CONFIG_MQJS_LVGLDEMO)
    add_subdirectory(lvgldemo)
  endif()

endif()
