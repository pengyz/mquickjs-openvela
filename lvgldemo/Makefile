############################################################################
# apps/system/mqjs/lvgldemo/Makefile — B0 spike host app (Make track).
#
# The demo's own TUs are only lvgl_backend.c (LvglBackend vtable impl) and
# mqjs_lvgl_demo.c (LVGL init + engine assembly + unified loop). The engine
# C objects, the RIDL stdlib and the aggregate register TU live in the PARENT
# app (../Makefile CSRCS), and the Rust adapter archive (mqjs_rs_ridl_* +
# mqjs_ui_* symbols) is built by the parent's cargo stamp rule — the final
# image link resolves everything image-wide (--start-group).
#
# The stamp rule below duplicates the parent's cargo invocation on the SAME
# rust/ workspace with a SEPARATE stamp file: apps build in parallel sub-makes
# with no inter-app dependency mechanism, so this app must not assume the
# parent's archive/staging state. Concurrent cargo runs serialize on cargo's
# own target-dir lock; both rules are cheap no-ops when up to date.
############################################################################

include $(APPDIR)/Make.defs

MQJS_DIR := $(abspath $(CURDIR)/..)

PROGNAME  = $(CONFIG_MQJS_LVGLDEMO_PROGNAME)
PRIORITY  = $(CONFIG_MQJS_LVGLDEMO_PRIORITY)
STACKSIZE = $(CONFIG_MQJS_LVGLDEMO_STACKSIZE)

MAINSRC = mqjs_lvgl_demo.c
CSRCS   = lvgl_backend.c

# Adapter archive: same build as the parent app (see ../Makefile for the
# target/arch whitelist rationale). Referenced by absolute path so this app
# never stages a second copy (duplicate strong symbols in one image).
ifeq ($(CONFIG_ARCH),sim)
MQJS_RUST_TARGET := x86_64-unknown-linux-gnu
MQJS_RUST_ENV :=
else ifneq ($(filter $(CONFIG_ARCH),arm64),)
MQJS_RUST_TARGET := aarch64-unknown-none
MQJS_RUST_ENV := RUSTFLAGS="-C panic=abort"
else
$(error mqjs_lvgldemo: unsupported CONFIG_ARCH '$(CONFIG_ARCH)')
endif
MQJS_ADAPTER_A := $(MQJS_DIR)/rust/target/$(MQJS_RUST_TARGET)/release/libmqjs.a
MQJS_RUST_STAMP := $(MQJS_DIR)/rust/.mqjs-lvgldemo-stamp-$(MQJS_RUST_TARGET)

MQJS_RUST_DEPS := $(shell find $(MQJS_DIR)/rust \
	\( -name '*.rs' -o -name 'Cargo.toml' -o -name 'Cargo.lock' \) -not -path '*/target/*' 2>/dev/null | sort)

$(MQJS_RUST_STAMP): $(MQJS_RUST_DEPS)
	$(Q) cd $(MQJS_DIR)/rust && $(MQJS_RUST_ENV) cargo build --release --target $(MQJS_RUST_TARGET) -p mqjs
	$(Q) touch $@

# Hook the cargo stamp into this app's compile graph (dependency-only rule,
# same trick as the parent's gen/mquickjs_ridl_register.c): a fresh stamp
# makes the TUs "newer than their objects" and forces recompile + relink.
mqjs_lvgl_demo.c lvgl_backend.c: $(MQJS_RUST_STAMP)

# Final-image link input: the adapter archive (LDLIBS accumulates image-wide).
LDLIBS += $(MQJS_ADAPTER_A)

include $(APPDIR)/Application.mk
