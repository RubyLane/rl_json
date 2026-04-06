#!/bin/sh
# Called by meson.add_dist_script() during "meson dist".
# MESON_DIST_ROOT   — the dist staging directory
# MESON_BUILD_ROOT  — the build directory (has generated files)
# MESON_SOURCE_ROOT — the source directory

set -e

dist="$MESON_DIST_ROOT"
build="$MESON_BUILD_ROOT"

# Copy generated README.md from the build tree (built by pandoc)
if [ -f "$build/doc/README.md" ]; then
    cp "$build/doc/README.md" "$dist/README.md"
fi

# Bake generated manpage for users who don't have pandoc
if [ -f "$build/doc/rl_json.n" ]; then
    cp "$build/doc/rl_json.n" "$dist/doc/json.n"
fi

# Bake autotools configure script so users don't need autoconf
if [ -f "$MESON_SOURCE_ROOT/configure" ]; then
    cp "$MESON_SOURCE_ROOT/configure" "$dist/configure"
elif command -v autoconf >/dev/null 2>&1; then
    (cd "$dist" && autoconf)
fi

# Remove .github — not useful in release tarballs
rm -rf "$dist/.github"

# Remove JSONTestSuite submodule — 167 MB of test corpus not needed by users
rm -rf "$dist/tests/JSONTestSuite"

# Replace symlinks with copies of their targets so the tarball works
# on Windows (which doesn't support symlinks without special permissions)
find "$dist" -type l | while read -r link; do
    target=$(readlink -f "$link")
    if [ -e "$target" ]; then
        rm "$link"
        if [ -d "$target" ]; then
            cp -a "$target" "$link"
        else
            cp "$target" "$link"
        fi
    fi
done
