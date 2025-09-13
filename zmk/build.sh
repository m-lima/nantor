#!/usr/bin/env bash

root=$(dirname "${0}")
root=$(realpath "${root}")

cd "${root}"

function log {
  echo -n '[34m['"${1}"'][m ' >&2
  shift
  echo "${@}" >&2
}

function clean {
  log clean "Stopping container"
  docker stop zmk
  log clean "Deleting images"
  docker images | grep zmkfirmware | awk '{system("docker rmi " $3)}'
  log clean "Resetting ZMK directory"
  rm -rf "${root}/zmk"
  git checkout -- "${root}/zmk"
}

function prepare_image {
  log prepare_image "Building zmk:latest"
  cd "${root}/zmk/.devcontainer"
  docker build -t zmk:latest .
}

function check_submodule {
  log check_submodule "Checkig submodule status"
  if (git submodule status | grep zmk | grep '^-' &> /dev/null); then
    log check_submodule "Initializing ZMK"
    git submodule update --init --recursive -- "${root}/zmk"
  fi
}

function check_image {
  log check_image "Checking docker image existance"
  if ! docker images --format '{{ .Repository }}:{{ .Tag }}' | grep 'zmk:latest' &>/dev/null; then
    prepare_image
  fi
}

function check_all {
  check_submodule && check_image
}

function west {
  run west ${@}
}

function run {
  if ! check_all; then
    return 1
  fi

  log run "Running: [37m${@}[m"
  docker \
    run \
    -it \
    --rm \
    --name zmk \
    --workdir /zmk \
    --volume "${root}/shared":/shared \
    --volume "${root}/zmk":/zmk \
    zmk \
    ${@}
}

function init {
  if ! check_all; then
    check_all
  fi

  west init -l app
  west update --fetch-opt=--filter=tree:0
  west zephyr-export
}

function make {
  if ! check_all; then
    return 1
  fi

  if [[ "${1}" == "p" ]]; then
    log make "Got pristine flag"
    pristine='--pristine'
  else
    pristine=''
  fi

  function make_cmd {
    echo 'build -s app '${pristine}' -d /shared/build/'${1}' -b pillbug -- -DSHIELD=nantor_'${1}' -DZMK_CONFIG=/shared/config -DZMK_EXTRA_MODULES=/shared/modules/status_led'
  }

  log make "Building firmware"
  west $(make_cmd left) && west $(make_cmd right)
}

case "${1}" in
  "clean") clean ;;
  "init") init ;;
  "make") make "${2}" ;;
  "full") clean && init && make p ;;
  "run")
    shift
    run ${@}
    ;;
  *)
    shift
    echo "Unkonwn command ${@}" >&2
    exit 1
    ;;
esac
