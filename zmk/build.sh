#!/usr/bin/env bash

root=$(dirname "${0}")
root=$(realpath "${root}")

cd "${root}"

function clean {
  docker stop zmk
  docker images | grep zmkfirmware | awk '{system("docker rmi " $3)}'
  rm -rf "${root}/zmk"
  git checkout -- "${root}/zmk"
}

function prepare_image {
  cd "${root}/zmk/.devcontainer"
  docker build -t zmk:latest .
}

function check_submodule {
  if (git submodule status | grep zmk | grep '^-' &> /dev/null); then
    git submodule update --init --recursive -- "${root}/zmk"
  fi
}

function check_image {
  if ! docker images --format '{{ .Repository }}:{{ .Tag }}' | grep 'zmk:latest' &>/dev/null; then
    prepare_image
  fi
}

function check_all {
  check_submodule && check_image
}

function west {
  docker \
    run \
    --rm \
    --name zmk \
    --workdir /zmk \
    --volume "${root}/shared":/shared \
    --volume "${root}/zmk":/zmk \
    zmk \
    ${@}
}

function init {
  enforce_image
  west west init
  west west update
}

function make {
  if ! check_all; then
    return 1
  fi

  if [[ "${1}" == "p" ]]; then
    pristine='--pristine'
  else
    pristine=''
  fi

  function make_cmd {
    echo 'west build '${pristine}' -d /shared/build/'${1}' -b pillbug -- -DSHIELD=nantor_'${1}' -DZMK_CONFIG=/shared/config -DZMK_EXTRA_MODULES=/shared/modules/status_led'
  }

  west bash -c "cd app; $(make_cmd left) && $(make_cmd right)"
}

case "${1}" in
  "clean") clean ;;
  "init") init ;;
  "make") make "${2}" ;;
  "full") clean && init && make p ;;
  *)
    echo "Unkonwn command ${1}" >&2
    exit 1
    ;;
esac
