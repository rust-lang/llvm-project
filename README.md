# Rust's fork of LLVM

This repository is a Rust-specific fork of LLVM. This is used as a submodule in
the https://github.com/rust-lang/rust repository. The Rust repository is always
pinned to a branch of this repository, and the `master` branch of this
repository is not used.

Despite this being a "fork" it's more of just a place where we can easily tweak
the LLVM source for Rust's own build. All patches should by default go upstream
first, and then we can cherry-pick them onto our branches if necessary.

To learn more about updating LLVM in the Rust compiler, consult the
[documentation for doing so in
`rustc-guide`](https://rustc-dev-guide.rust-lang.org/backend/updating-llvm.html)
