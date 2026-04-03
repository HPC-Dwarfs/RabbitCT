#!/bin/bash
set -euo pipefail

URL="https://hpc-mover.rrze.uni-erlangen.de/HPC-Data/0x7b58aefb/eig7ahyo6fo2bais0ephuf2aitohv1ai/RabbitInput.tgz"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "This will download RabbitInput.tgz (~2.9 GB) and extract it."
read -r -p "Do you want to proceed? [y/N] " response
case "$response" in
    [yY][eE][sS]|[yY])
        ;;
    *)
        echo "Aborted."
        exit 0
        ;;
esac

echo "Downloading RabbitInput.tgz..."
curl -L -o "$SCRIPT_DIR/RabbitInput.tgz" "$URL"

echo "Extracting..."
tar -xzf "$SCRIPT_DIR/RabbitInput.tgz" -C "$SCRIPT_DIR"

echo "Removing archive..."
rm "$SCRIPT_DIR/RabbitInput.tgz"

echo "Done."
