# Will compile the layout called "nantor"
# It is expected to be called from the `qmk_firmware` directory
#
# The keymap should be available under `qmk_firmware/keyboards/cantor/keymaps/nantor`

if [ ! ${1} ]; then
  echo '[33mNo version provided[m'
fi
docker run -it --rm --volume ${PWD}:/src --name qmk qmkfm/qmk_cli bash -c "cd src/; make cantor:nantor${1}"
