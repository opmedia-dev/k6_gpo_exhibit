#!/usr/bin/env bash
#
# K6 GPO Exhibit — batch audio level normalizer
# ============================================================================
# Brings every track (history, numbers, system prompts) to a consistent
# loudness so no clip is noticeably louder or quieter than the others when
# played through the telephone earpiece.
#
# Run this on your PC over the folder that will be copied to the SD card,
# BEFORE copying — the ESP32 does no runtime loudness matching.
#
# Two methods:
#   mp3gain   (default) — lossless, no re-encode, fast. MP3 only.
#   loudnorm  — ffmpeg EBU R128 two-pass, most precise, re-encodes. Any format.
#
# Usage:
#   ./normalize_audio.sh [SD_ROOT] [method] [target]
#
#   SD_ROOT   folder containing /history /numbers /system  (default: current dir)
#   method    mp3gain | loudnorm                            (default: mp3gain)
#   target    mp3gain: target dB (default 89.0 ≈ ReplayGain)
#             loudnorm: integrated LUFS   (default -16)
#
# Examples:
#   ./normalize_audio.sh /media/usb/sdcard
#   ./normalize_audio.sh /media/usb/sdcard loudnorm -16
#   ./normalize_audio.sh . mp3gain 89.0
#
# Notes:
#   * ALWAYS work on a copy — mp3gain modifies files in place (though it is
#     lossless and reversible with `mp3gain -u`). loudnorm rewrites files.
#   * -16 LUFS is a good target for a telephone earpiece: loud and clear
#     without clipping. Raise toward -14 for louder, lower toward -18 for
#     quieter/headroom.
# ============================================================================

set -euo pipefail

SD_ROOT="${1:-.}"
METHOD="${2:-mp3gain}"
TARGET="${3:-}"

DIRS=(history numbers system)

if [[ ! -d "$SD_ROOT" ]]; then
    echo "error: SD_ROOT '$SD_ROOT' not found" >&2
    exit 1
fi

# Collect every .mp3 across the expected sub-folders (and the root, for any
# stray files). NUL-delimited to survive spaces in names.
collect_mp3s() {
    local d
    for d in "${DIRS[@]}"; do
        [[ -d "$SD_ROOT/$d" ]] && find "$SD_ROOT/$d" -maxdepth 1 -type f \
            \( -iname '*.mp3' \) -print0
    done
    find "$SD_ROOT" -maxdepth 1 -type f \( -iname '*.mp3' \) -print0
}

count=$(collect_mp3s | tr -dc '\0' | wc -c | tr -d ' ')
if [[ "$count" == "0" ]]; then
    echo "No .mp3 files found under $SD_ROOT/{${DIRS[*]}} — nothing to do." >&2
    exit 0
fi
echo "Found $count MP3 file(s) under $SD_ROOT"

case "$METHOD" in
  mp3gain)
    command -v mp3gain >/dev/null 2>&1 || {
        echo "error: mp3gain not installed."                       >&2
        echo "  Debian/Ubuntu: sudo apt-get install mp3gain"       >&2
        echo "  macOS:         brew install mp3gain"               >&2
        exit 1
    }
    TARGET="${TARGET:-89.0}"
    echo "Method: mp3gain (lossless)  target: ${TARGET} dB"
    echo "----------------------------------------------------------"
    # -r  apply Track gain (each file to the same loudness)
    # -k  prevent clipping (auto-lowers if a track would clip)
    # -d  target offset relative to the 89 dB reference
    #     e.g. TARGET 91  -> -d 2 ; TARGET 89 -> -d 0
    offset=$(awk -v t="$TARGET" 'BEGIN{printf "%.1f", t-89.0}')
    # xargs -0 to feed NUL-delimited paths safely.
    collect_mp3s | xargs -0 mp3gain -r -k -d "$offset"
    echo "----------------------------------------------------------"
    echo "Done. mp3gain applied lossless track gain to $count file(s)."
    echo "Undo at any time with:  mp3gain -u <file.mp3>"
    ;;

  loudnorm)
    command -v ffmpeg  >/dev/null 2>&1 || { echo "error: ffmpeg not installed" >&2; exit 1; }
    command -v ffprobe >/dev/null 2>&1 || { echo "error: ffprobe not installed" >&2; exit 1; }
    TARGET="${TARGET:--16}"
    echo "Method: ffmpeg loudnorm (EBU R128, two-pass)  target: ${TARGET} LUFS"
    echo "----------------------------------------------------------"
    tmpdir="$(mktemp -d)"
    trap 'rm -rf "$tmpdir"' EXIT

    # Two-pass loudnorm per file: pass 1 measures, pass 2 corrects.
    while IFS= read -r -d '' f; do
        echo "  normalizing: $f"
        # --- pass 1: measure ---
        measured="$(ffmpeg -hide_banner -nostats -i "$f" \
            -af "loudnorm=I=${TARGET}:TP=-1.5:LRA=11:print_format=json" \
            -f null - 2>&1 | awk '/^\{/{c=1} c{print} /^\}/{c=0}')"

        get() { printf '%s' "$measured" | grep "\"$1\"" | sed -E 's/.*: *"?([-0-9.]+|inf)"?.*/\1/'; }
        I_in="$(get input_i)";  TP_in="$(get input_tp)"
        LRA_in="$(get input_lra)"; THR_in="$(get input_thresh)"
        OFF="$(get target_offset)"

        # --- pass 2: apply, preserve original bitrate ---
        br="$(ffprobe -v error -select_streams a:0 -show_entries stream=bit_rate \
              -of default=nw=1:nk=1 "$f" 2>/dev/null || true)"
        [[ -z "$br" || "$br" == "N/A" ]] && br=128000

        out="$tmpdir/$(basename "$f")"
        ffmpeg -hide_banner -loglevel error -y -i "$f" \
            -af "loudnorm=I=${TARGET}:TP=-1.5:LRA=11:measured_I=${I_in}:measured_TP=${TP_in}:measured_LRA=${LRA_in}:measured_thresh=${THR_in}:offset=${OFF}:linear=true:print_format=summary" \
            -c:a libmp3lame -b:a "$br" "$out"
        mv -f "$out" "$f"
    done < <(collect_mp3s)

    echo "----------------------------------------------------------"
    echo "Done. loudnorm normalized $count file(s) to ${TARGET} LUFS."
    ;;

  *)
    echo "error: unknown method '$METHOD' (use: mp3gain | loudnorm)" >&2
    exit 1
    ;;
esac
