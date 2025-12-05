#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
THIRD_PARTY_DIR="$SCRIPT_DIR"
PATCHES_DIR="$THIRD_PARTY_DIR/patches"

if [[ ! -d "$PATCHES_DIR" ]]; then
  echo "No patches directory at: $PATCHES_DIR"
  exit 0
fi

echo "Using third_party: $THIRD_PARTY_DIR"
echo "Using patches:     $PATCHES_DIR"
echo

for PATCH_PROJECT_DIR in "$PATCHES_DIR"/*; do
  [[ -d "$PATCH_PROJECT_DIR" ]] || continue

  PROJECT_NAME="$(basename "$PATCH_PROJECT_DIR")"
  TARGET_DIR="$THIRD_PARTY_DIR/$PROJECT_NAME"

  if [[ ! -d "$TARGET_DIR" ]]; then
    echo "Skipping '$PROJECT_NAME' (no directory '$TARGET_DIR')"
    echo
    continue
  fi

  echo "=== Processing '$PROJECT_NAME' ==="
  pushd "$TARGET_DIR" > /dev/null

  shopt -s nullglob
  PATCH_FILES=("$PATCH_PROJECT_DIR"/*.patch)

  for PATCH_FILE in "${PATCH_FILES[@]}"; do
    echo "  Patch: $(basename "$PATCH_FILE")"

    # If reverse applies, patch is already applied
    if git apply --reverse --check "$PATCH_FILE" >/dev/null 2>&1; then
      echo "    already applied, skipping"
      continue
    fi

    if git apply --check "$PATCH_FILE" >/dev/null 2>&1; then
      git apply "$PATCH_FILE"
      echo "    applied"
    else
      echo "    FAILED: does not apply cleanly"
    fi
  done

  shopt -u nullglob
  popd > /dev/null

  #
  # --- RUN EXECUTABLE SCRIPTS IN THE PATCH DIRECTORY ---
  #
  echo "  Running build / extra scripts for '$PROJECT_NAME'..."
  shopt -s nullglob
  SCRIPT_FILES=("$PATCH_PROJECT_DIR"/*.sh)

  for SCRIPT in "${SCRIPT_FILES[@]}"; do
    echo "    -> Executing $(basename "$SCRIPT")"
    chmod +x "$SCRIPT"

    # Execute with CWD set to the dependency's directory
    (
      cd "$TARGET_DIR"
      "$SCRIPT"
    )
  done

  shopt -u nullglob
  echo "=== Done '$PROJECT_NAME' ==="
  echo
done

echo "All patches and scripts done."
