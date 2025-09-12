#!/usr/bin/env bash

root=$(dirname "${0}")
root=$(realpath "${root}")

cd "${root}"

function clean {
  docker ps -a | grep vsc-zmk- | awk '{system("docker stop " $1); system("docker rm " $1)}'
  docker images | grep vsc-zmk- | awk '{system("docker rmi " $3)}'
  docker images | grep zmkfirmware | awk '{system("docker rmi " $3)}'
  docker volume ls | grep zmk- | awk '{system("docker volume rm " $2)}'
  rm -rf "${root}/zmk"
  git checkout -- "${root}/zmk"
}

function check_volume {
  if zmk_config=$(docker volume inspect zmk-config 2> /dev/null); then
    if [[ $(jq -r '.[].Options' <<<${zmk_config}) == "null" ]] || [[ $(jq -r '.[].Options.device' <<<${zmk_config}) != "${root}/shared" ]]; then
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
  if (git submodule status | grep zmk | grep '^-' &> /dev/null); then
    git submodule update --init --recursive -- "${root}/zmk"
  fi
}

function check_yarn {
  if ! yarn devcontainer &> /dev/null; then
    yarn
  fi
}

function check_all {
  check_submodule \
    && check_volume \
    && check_yarn
}

function init {
  check_all \
    && yarn devcontainer up --workspace-folder "${root}/zmk" \
    && yarn devcontainer exec --workspace-folder "${root}/zmk" bash -c 'west init -l app/; west update'
}

function run {
  if ! check_all; then
    return 1
  fi

  if [ "${1}" ]; then
    command="${1}"
  fi
  shift

  yarn run devcontainer "${command}" --workspace-folder "${root}/zmk" "${@}"
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
    echo 'west build '${pristine}' -d /workspaces/zmk-config/build/'${1}' -b pillbug -- -DSHIELD=nantor_'${1}' -DZMK_CONFIG=/workspaces/zmk-config/config -DZMK_EXTRA_MODULES=/workspaces/zmk-config/modules/status_led'
  }

  yarn devcontainer exec --workspace-folder "${root}/zmk" bash -c "cd app; $(make_cmd left) && $(make_cmd right)"
}

case "${1}" in
  "update")
    echo '[31mNot yet implemented[m'
    exit 1
    ;;
  "clean") clean ;;
  "init") init ;;
  "make") make "${2}" ;;
  "all")
    clean \
      && init \
      && make p
    ;;
  *) run "${@}" ;;
esac
