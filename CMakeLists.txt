############################################################################
# apps/system/mqjs/CMakeLists.txt — mquickjs `js` NSH builtin (CMake track)
#
# PLACEHOLDER: the CMake track is NOT part of the Phase 3.1 acceptance
# (Make track only, sim mechanism verification). This file mirrors the
# Makefile structurally and keeps the CMake build from silently dropping
# the directory; it must be re-validated under the official Vela CMake
# flow before any CMake-track use (same deferral decision as the
# ports/openvela CMake port, M2 scope).
#
# Differences vs the Makefile wiring (kept intentionally explicit here):
# - the Rust adapter archive must be staged into ${APPDIR}/staging (the
#   CMake link consumes it like any other extra library) — build it with:
#     cd rust && cargo build --release --target x86_64-unknown-linux-gnu -p mqjs
#     cp rust/target/x86_64-unknown-linux-gnu/release/libmqjs.a ${APPDIR}/staging/
# - generated artifacts are checked in under gen/, so no framework.* include
#   is needed; include dirs and force-includes mirror the Makefile CFLAGS.
############################################################################

if(CONFIG_MQJS_JS)

  # RS switch (mirrors the Makefile): RS=y builds the Rust-adapter form
  # (js_main.c, adapter archive staged into ${APPDIR}/staging, see header
  # comment); RS=n builds the C-only checkpoint (Phase 3.3: js_main_c.c,
  # direct JS_NewContext/JS_Eval + C fallback stubs for the adapter-owned
  # table entry points — no cargo, no staging). CSRCS/CFLAGS are identical
  # in both forms; only MAINSRC differs.
  if(CONFIG_MQJS_RS)
    set(MQJS_MAINSRC js_main.c)
  else()
    set(MQJS_MAINSRC js_main_c.c)
  endif()

  # The openvela cmake flow processes app directories in multiple passes
  # (kconfig pre/post generation). Guard against duplicate target creation:
  # once apps_<progname> exists from an earlier pass, re-adding it here
  # fails with "cannot create target".
  if(TARGET apps_${CONFIG_MQJS_JS_PROGNAME})
    return()
  endif()

  nuttx_add_application(
    NAME ${CONFIG_MQJS_JS_PROGNAME}
    SRCS
      ${MQJS_MAINSRC}
      engine/mquickjs.c
      engine/cutils.c
      engine/dtoa.c
      engine/libm.c
      # RIDL-variant stdlib runtime TUs: mqjs_stdlib_impl.c defines the
      # strong js_stdlib (RIDL tables incl. console), the register TU comes
      # from the app aggregate (gen/), mqjs_require.c implements require().
      engine/mqjs_stdlib_impl.c
      engine/mqjs_require.c
      gen/mquickjs_ridl_register.c
    STACKSIZE
      ${CONFIG_MQJS_JS_STACKSIZE}
    PRIORITY
      ${CONFIG_MQJS_JS_PRIORITY}
    INCLUDE_DIRECTORIES
      ${CMAKE_CURRENT_SOURCE_DIR}/gen
      ${CMAKE_CURRENT_SOURCE_DIR}/engine
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

endif()
