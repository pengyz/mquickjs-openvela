//! Tree-in vendored adaptation of ridl-modules/stdlib's build script.
//!
//! UPSTREAM (mquickjs-rs-demo repo, ridl-modules/stdlib/build.rs) invokes the
//! `ridl-tool module src/stdlib.ridl <out_dir>` HOST BINARY (resolved via the
//! MQUICKJS_RIDL_TOOL env var) to generate the module glue (api.rs + glue.rs)
//! into OUT_DIR. That would make every in-tree build depend on a prebuilt
//! host tool, which the tree-in integration wants to avoid.
//!
//! THIS VENDORED VERSION instead copies the PRE-GENERATED glue from
//! `generated/` (checked in next to this file) into OUT_DIR. The generation
//! is a pure function of src/stdlib.ridl (verified byte-deterministic), so
//! the checked-in glue stays valid as long as the .ridl source is unchanged.
//! Regeneration + diff-self-check commands live in ../SYNC.md.

use std::{env, path::PathBuf};

fn main() {
    println!("cargo:rerun-if-changed=src/stdlib.ridl");
    println!("cargo:rerun-if-changed=generated/api.rs");
    println!("cargo:rerun-if-changed=generated/glue.rs");

    let out_dir = PathBuf::from(env::var("OUT_DIR").expect("OUT_DIR not set"));

    let manifest_dir = env::var("CARGO_MANIFEST_DIR").expect("CARGO_MANIFEST_DIR not set");
    let generated_dir = PathBuf::from(&manifest_dir).join("generated");

    for file in ["api.rs", "glue.rs"] {
        let src = generated_dir.join(file);
        let dst = out_dir.join(file);
        std::fs::copy(&src, &dst)
            .unwrap_or_else(|e| panic!("failed to copy {} -> {}: {e}", src.display(), dst.display()));
    }
}
