#!/bin/bash
#
# Массовое индексирование MP3-файлов.
# Использование: ./index_all.sh <путь_к_папке> [--db tracks.db]
#
# Для каждого .mp3 извлекает title и artist из ID3-тегов через ffprobe.
# Если ffprobe недоступен или теги отсутствуют — берёт имя файла как title.
#

set -euo pipefail

INDEXER="./indexer"
DB="tracks.db"
DIR=""

# --- Аргументы ---
while [[ $# -gt 0 ]]; do
    case "$1" in
        --db)   DB="$2"; shift 2 ;;
        --help) echo "Usage: $0 <dir> [--db path]"; exit 0 ;;
        *)      DIR="$1"; shift ;;
    esac
done

if [[ -z "$DIR" ]]; then
    echo "Usage: $0 <dir> [--db path]"
    exit 1
fi

if [[ ! -d "$DIR" ]]; then
    echo "Error: '$DIR' is not a directory"
    exit 1
fi

if [[ ! -x "$INDEXER" ]]; then
    echo "Error: '$INDEXER' not found or not executable"
    echo "Build the project first: cmake --build build"
    exit 1
fi

# --- Проверяем наличие ffprobe ---
HAS_FFPROBE=false
if command -v ffprobe &>/dev/null; then
    HAS_FFPROBE=true
fi

# --- Извлечение метаданных ---
get_tag() {
    local file="$1"
    local tag="$2"
    if $HAS_FFPROBE; then
        ffprobe -v quiet -show_entries "format_tags=$tag" \
                -of default=noprint_wrappers=1:nokey=1 "$file" 2>/dev/null || true
    fi
}

# --- Индексирование ---
count=0
failed=0

find "$DIR" -type f -iname '*.mp3' | sort | while read -r file; do
    title=$(get_tag "$file" "title")
    artist=$(get_tag "$file" "artist")

    # Фоллбэк: имя файла без расширения.
    if [[ -z "$title" ]]; then
        title=$(basename "$file" .mp3)
    fi
    if [[ -z "$artist" ]]; then
        artist="Unknown"
    fi

    echo "--- Indexing: $title — $artist"
    echo "    File: $file"

    if "$INDEXER" --input "$file" --title "$title" --artist "$artist" --db "$DB"; then
        count=$((count + 1))
    else
        echo "    FAILED"
        failed=$((failed + 1))
    fi
    echo ""
done

echo "=== Done: $count indexed, $failed failed ==="
