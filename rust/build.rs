//! Build wiring for the openvela adapter as a RIDL leaf application.
//!
//! Copies this app's RIDL aggregate Rust artifacts (`ridl_symbols.rs`,
//! `ridl_context_ext.rs`, `ridl_bootstrap.rs`) from
//! `gen/ridl/apps/mqjs/aggregate/` into `OUT_DIR`. The aggregate is
//! PRE-GENERATED and checked into the tree (gen/); the location is wired by
//! `.cargo/config.toml` (`MQUICKJS_RIDL_TARGET_DIR = "../gen"`). The
//! code path is unchanged from the upstream adapter (demo repo
//! ports/openvela/rust); regeneration + diff-self-check commands live in
//! ../SYNC.md.
//!
//! Deliberately NO `emit_native_stdlib_link()` here: this crate builds with
//! `MQJS_ENGINE_LINK=external` (.cargo/config.toml) — the engine C objects,
//! including the ridl-variant stdlib TU that defines `js_stdlib`, are compiled
//! into the openvela image at source level. Linking the host-built
//! `libmquickjs_stdlib_ridl.a` would define `js_stdlib` twice (both strong).
fn main() {
    mquickjs_ridl_glue::emit();
}
