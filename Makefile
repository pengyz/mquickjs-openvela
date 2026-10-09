############################################################################
# apps/system/mqjs/Makefile — mquickjs `js` NSH builtin (tree-in).
#
# SOURCE-LEVEL integration (mirrors apps/interpreters/quickjs): the engine's
# .c files are compiled here by the NuttX toolchain, so every libc binding
# (setjmp/longjmp, malloc/free, ...) resolves to the NuttX side consistently.
#
# stdlib = RIDL variant: the runtime TU <sdk>/deps/mquickjs-rs/mqjs_stdlib_impl.c defines
# the strong `js_stdlib` with the RIDL extension tables (console -> Rust
# adapter), gen/mquickjs_ridl_register.c is the app aggregate's register TU
# (JS_RIDL_StdlibInit + require table), and <sdk>/deps/mquickjs-rs/require.c implements
# require(). The base engine mqjs_stdlib.c is NOT compiled here.
#
# The generated stdlib table always contains the adapter-owned entry points
# (console singleton trio + rsVersion/rsSelfTest probes, baked in by the
# template overlay). Who DEFINES them is the RS switch:
# - CONFIG_MQJS_RS=y: the Rust adapter archive (staged below) provides them;
#   MAINSRC = js_main.c (adapter trio owns context create/eval/free).
# - CONFIG_MQJS_RS=n: C-only checkpoint (Phase 3.3, QEMU); MAINSRC =
#   js_main_c.c (direct JS_NewContext/JS_Eval + C fallback stubs for the
#   adapter-owned entry points). No cargo, no stamp, no staging.
# In both cases CSRCS/CFLAGS are identical: same engine TUs, same generated
# table, same RIDL/force-include flags.
#
# Generated artifacts are checked in under gen/ (atom header, ridl stdlib
# table, rs hook declarations, aggregate register TUs, framework bindgen
# headers consumed by the Rust adapter). Regeneration commands
# and source revisions: see SYNC.md.
#
# Rust adapter (RS=y only): built by the `context::` rule below with the
# SDK-adjacent crate-local cargo (cwd = rust/, so the crate-local
# .cargo/config.toml applies — MQJS_ENGINE_LINK=external keeps engine
# objects OUT of the archive to avoid duplicate strong symbols with the
# CSRCS below). The archive is staged into $(APPDIR)/staging and linked by
# the generic apps rule (Application.mk: LDLIBS += $(wildcard
# $(APPDIR)/staging/*$(LIBEXT))).
############################################################################

include $(APPDIR)/Make.defs

# SDK layout: the mquickjs-rs-sdk checkout that the repo manifest places at
# <tree>/external/mquickjs-rs-sdk. Everything non-integration comes from
# there (engine C, stdlib runtime TUs, Rust crates via the adapter's cargo
# path deps); this app only owns js_main forms, gen/ artifacts and wiring.
MQJS_SDK_DIR := $(abspath $(CURDIR)/../../../../external/mquickjs-rs-sdk)
ifeq ($(wildcard $(MQJS_SDK_DIR)/Cargo.toml),)
$(error mqjs: mquickjs-rs-sdk not found at $(MQJS_SDK_DIR) -- sync the repo \
 manifest (project external/mquickjs-rs-sdk) or clone it there)
endif
MQJS_ENGINE_DIR := $(MQJS_SDK_DIR)/deps/mquickjs
MQJS_RS_DIR := $(MQJS_SDK_DIR)/deps/mquickjs-rs

# Engine compile flags (mirrors deps/mquickjs/Makefile target CFLAGS minus
# host-specific parts) plus gen/ (generated headers + aggregate register TU)
# and the SDK engine dir (private headers).
# -DMQUICKJS_ENABLE_RIDL_EXTENSIONS selects the strong-`js_stdlib` linkage
# and the RIDL code paths; the force-included api header declares the RIDL
# js_* entry points referenced by the generated stdlib table inside
# mqjs_stdlib_impl.o, and the hooks header declares the rs probe overlay
# hooks baked into the same table (js_rs_* are defined non-static in the
# selected MAINSRC — cross-TU references).
CFLAGS += -Igen
CFLAGS += -I$(MQJS_ENGINE_DIR)
CFLAGS += -DMQUICKJS_ENABLE_RIDL_EXTENSIONS
CFLAGS += -include mquickjs_ridl_api.h
CFLAGS += -include mqjs_rs_hooks.h
CFLAGS += -fno-math-errno -fno-trapping-math

# Engine core TUs (SDK deps/mquickjs) + ridl-variant stdlib runtime TUs
# (SDK deps/mquickjs-rs: mqjs_stdlib_impl.c defines the strong `js_stdlib`,
# require.c implements require()) + the app aggregate's register TU (gen/).
MQJS_ENGINE_SRCS = mquickjs.c cutils.c dtoa.c libm.c
MQJS_RS_RT_SRCS = mqjs_stdlib_impl.c mqjs_require.c

CSRCS += $(addprefix $(MQJS_ENGINE_DIR)/,$(MQJS_ENGINE_SRCS))
CSRCS += $(addprefix $(MQJS_RS_DIR)/,$(MQJS_RS_RT_SRCS))
CSRCS += gen/mquickjs_ridl_register.c

PROGNAME  = $(CONFIG_MQJS_JS_PROGNAME)
PRIORITY  = $(CONFIG_MQJS_JS_PRIORITY)
STACKSIZE = $(CONFIG_MQJS_JS_STACKSIZE)

ifeq ($(CONFIG_MQJS_RS),y)

MAINSRC = js_main.c

# context:: — build the vendored Rust adapter (host target, std mode) and
# stage the staticlib for the flat-image link. cargo runs with cwd = rust/
# so the crate-local .cargo/config.toml (MQJS_ENGINE_LINK=external +
# pre-generated artifact wiring) is in effect. Incremental cargo: a no-op
# rebuild is cheap.
#
# LIBCLANG_PATH: bindgen (mquickjs-rs build-dep) needs libclang at build
# time. openvela's envsetup.sh exports LIBCLANG_PATH into the prebuilts
# rust pack (prebuilts/rust/linux/extra_libs) AND prepends prebuilts clang
# to PATH, so neither the env value nor PATH-derived resolution can be
# trusted in partial checkouts. Rule: keep an externally provided
# LIBCLANG_PATH only when it actually contains a libclang shared library;
# otherwise resolve one from the system linker cache. No machine-local
# path is hardcoded; when no libclang exists at all, the cargo error
# message names the knob (documented in SYNC.md as an integration-machine
# requirement).
# Rust target/mode follows the board architecture (dual-mode adapter,
# Phase 3.4): the adapter's target-specific dependency tables select
# std mode on host Linux (sim) and no-std mode elsewhere (bare-metal
# aarch64-unknown-none), so make only picks the triple and, for the
# freestanding target, the panic strategy:
# - sim  (CONFIG_ARCH=sim):   x86_64-unknown-linux-gnu, std mode.
# - arm64 (CONFIG_ARCH=arm64): aarch64-unknown-none + -C panic=abort
#   (no_std; NuttX malloc/free bridge + FFI printf console). RUSTFLAGS is
#   used instead of a profile so the sim build's panic strategy is untouched.
# Explicit whitelist: any OTHER arch must fail loudly instead of silently
# falling into the host-target (std) branch.
ifneq ($(filter $(CONFIG_ARCH),arm64),)
MQJS_RUST_TARGET := aarch64-unknown-none
MQJS_RUST_ENV := RUSTFLAGS="-C panic=abort"
else ifeq ($(CONFIG_ARCH),sim)
MQJS_RUST_TARGET := x86_64-unknown-linux-gnu
MQJS_RUST_ENV :=
else
$(error mqjs: unsupported CONFIG_ARCH '$(CONFIG_ARCH)' for the Rust bridge — extend the whitelist with an explicit target)
endif
MQJS_ADAPTER_A := rust/target/$(MQJS_RUST_TARGET)/release/libmqjs.a

MQJS_LIBCLANG_SYSTEM := $(shell p=$$(ldconfig -p 2>/dev/null | awk '$$1 ~ /^libclang/ && $$1 !~ /cpp/ {print $$NF; exit}'); [ -n "$$p" ] && dirname "$$p")
ifneq ($(wildcard $(LIBCLANG_PATH)/libclang*.so*),)
export LIBCLANG_PATH := $(LIBCLANG_PATH)
else
export LIBCLANG_PATH := $(MQJS_LIBCLANG_SYSTEM)
endif

# Rust source manifest for rebuild stamping: cargo's own incrementality is
# NOT visible to make, and the context pass only reruns when .config changes
# (Unix.mk gates %.context on include/nuttx/config.h) — so a context-only
# rule would silently link a stale archive after editing rust/ (and
# `make clean && make` would NOT rebuild it: clean wipes rust/target but a
# context-only stamp stays fresh). The stamp below is therefore hooked into
# the NORMAL build graph: gen/mquickjs_ridl_register.c (our TU, always
# compiled) depends on it, so any rust/ source/manifest change forces
# cargo + re-stage before the image links; an up-to-date tree makes cargo a
# cheap no-op. CURDIR (not env PWD — stale in sub-makes) is the app dir.
# The stamp name embeds the Rust target: switching board configs (sim ↔
# qemu-armv8a on the same build dir) re-runs cargo for the new triple
# instead of re-staging the other target's stale archive (cargo caches
# per-triple under rust/target/, so the re-run is a normal incremental
# build).
MQJS_RUST_DEPS := $(shell find $(CURDIR)/rust \
	\( -name '*.rs' -o -name 'Cargo.toml' -o -name 'Cargo.lock' \) -not -path '*/target/*' 2>/dev/null | sort)
MQJS_RUST_STAMP := rust/.mqjs-context-stamp-$(MQJS_RUST_TARGET)

$(MQJS_RUST_STAMP): $(MQJS_RUST_DEPS)
	$(Q) cd rust && $(MQJS_RUST_ENV) cargo build --release --target $(MQJS_RUST_TARGET) -p mqjs
	$(Q) mkdir -p $(APPDIR)/staging
	$(Q) cp -f $(MQJS_ADAPTER_A) $(APPDIR)/staging/
	$(Q) touch $@

# Hook the stamp into the normal build graph (see rationale above). This
# file is in CSRCS below, so every image build pulls the dependency chain.
gen/mquickjs_ridl_register.c: $(MQJS_RUST_STAMP)

context:: $(MQJS_RUST_STAMP)

clean::
	$(Q) rm -rf rust/target rust/.mqjs-context-stamp-*

else # CONFIG_MQJS_RS=n — C-only checkpoint (Phase 3.3)

MAINSRC = js_main_c.c

endif

include $(APPDIR)/Application.mk
