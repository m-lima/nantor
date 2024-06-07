#!/usr/bin/env bash

root=$(dirname "${0}")
root=$(realpath "${root}")

cd "${root}"

function clean {
  docker ps -a | grep vsc-zmk- | awk '{system("docker stop " $1); system("docker rm " $1)}'
  docker volume ls | grep zmk- | awk '{system("docker volume rm " $2)}'
}

function check_volume {
  if docker volume inspect zmk-config &> /dev/null; then
    if [[ $(docker volume inspect zmk-config | jq -r '.[].Options') == "null" ]]; then
      echo "[32mVolume zmk-config is already created without referencing[m ${root}/shared"
      echo -n 'Continue? [y/N]'
      read input
      case "${input}" in
        [yY]) ;;
        *) exit ;;
      esac
    fi
  else
    docker volume create -o o=bind -o type=none -o device="${root}/shared" zmk-config
  fi
}

function check_submodule {
  if ! (git submodule status | grep heads/main &> /dev/null); then
    git submodule update --init --recursive -- "${root}/zmk"
  fi
}

function init {
  check_submodule \
    && check_volume \
    && yarn run devcontainer up --workspace-folder "${root}/zmk" \
    && yarn run devcontainer exec --workspace-folder "${root}/zmk" bash -c 'west init -l app/; west update'
}

function run {
  check_submodule && check_volume

  shift
  if [ "${1}" ]; then
    command="${1}"
  else
    command='help'
  fi
  shift

  yarn run devcontainer "${command}" --workspace-folder "${root}/zmk" "${@}"
}

function build {
  if [[ "${1}" == "p" ]]; then
    pristine='-p'
  else
    pristine=''
  fi
  check_submodule \
    && check_volume \
    && yarn run devcontainer exec --workspace-folder "${root}/zmk" bash -c 'cd app; west build '${pristine}' -d /workspaces/zmk-config/build/left -b pillbug -- -DSHIELD=nantor_left -DZMK_CONFIG="/workspaces/zmk-config/config"; west build '${pristine}' -d /workspaces/zmk-config/build/right -b pillbug -- -DSHIELD=nantor_right -DZMK_CONFIG="/workspaces/zmk-config/config"'
}

case "${1}" in
  "update")
    echo '[31mNot yet implemented[m'
    exit 1
    ;;
  "clean") clean ;;
  "init") init ;;
  "build") build "${2}" ;;
  "all")
    clean
    init
    build p
    ;;
  *) run "${@}" ;;
esac
