# Will compile the layout called "nantor"
# It is expected to be called from the `qmk_firmware` directory
#
# The keymap should be available under `qmk_firmware/keyboards/cantor/keymaps/nantor`

if [ ! ${1} ]; then
  echo '[31mNo version provided[m'
  exit 1
fi

if [[ "${1}" = v* ]]; then
  version="${1}"
else
  version="v${1}"
fi

root=$(realpath "${0}")
root=$(dirname "${root}")

layout="${root}/../layout/${version}"
if [ ! -d "${layout}" ]; then
  echo '[31mThe version `'"${version}"'` does not exist in `'"${layout}"'`[m'
  exit 1
fi
layout=$(realpath "${layout}")

root="${root}/../qmk_firmware"
root=$(realpath "${root}")

if grep SERIAL_USART_FULL_DUPLEX "${root}/keyboards/cantor/config.h" &> /dev/null; then
  sed -i '' 's/.*SERIAL_USART_FULL_DUPLEX.*//g' "${root}/keyboards/cantor/config.h"
fi

echo 'Docker image id: 9ced503a5714'
docker run -it --rm --volume "${root}:/src" --volume "${layout}:/src/keyboards/cantor/keymaps/nantor" --name qmk qmkfm/qmk_cli bash -c "cd src/; make cantor:nantor"
