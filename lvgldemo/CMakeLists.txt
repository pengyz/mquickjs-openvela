############################################################################
# apps/system/mqjs/lvgldemo/CMakeLists.txt — B0 spike host app (CMake track)
#
# Mirrors lvgldemo: the app's own TUs only; LVGL comes in via DEPENDS lvgl.
# The engine/stdlib/register objects live in the PARENT app (system/mqjs),
# and the Rust adapter archive is built by the parent's mqjs_rust_adapter
# custom target — DEPENDS on it guarantees cargo runs before this app's
# part of libapps.a (and the archive enters the final image link through
# the parent's nuttx_add_extra_library).
############################################################################

if(CONFIG_MQJS_LVGLDEMO)

  # openvela cmake processes app dirs in multiple passes; guard duplicates.
  if(TARGET apps_${CONFIG_MQJS_LVGLDEMO_PROGNAME})
    return()
  endif()

  nuttx_add_application(
    NAME
      ${CONFIG_MQJS_LVGLDEMO_PROGNAME}
    PRIORITY
      ${CONFIG_MQJS_LVGLDEMO_PRIORITY}
    STACKSIZE
      ${CONFIG_MQJS_LVGLDEMO_STACKSIZE}
    SRCS
      mqjs_lvgl_demo.c
      lvgl_backend.c
    DEPENDS
      lvgl
      mqjs_rust_adapter)

endif()
