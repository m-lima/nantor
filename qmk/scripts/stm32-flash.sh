#!/bin/bash

# Unplug the board
# Hold down the top left key
# Plug it back in

if ! [ "${1}" ]
then
  echo 'Expected a binary file to upload' >&2
  exit -1
fi

if ! [ -f "${1}" ]
then
  echo "Could not find ${1}" >&2
  exit -1
fi

FILE="${1}"

BOARDS=`dfu-util -l | grep '^Found DFU:'`
if ! [ "${BOARDS}" ]
then
  echo 'No DFU compatible board found' >&2
  exit -1
fi

echo "${BOARDS}"
BOARDS=`echo "${BOARDS}" | grep '^Found DFU:' | grep '@Internal Flash'`

UNIQUE_BOARDS=(`echo "${BOARDS}" | awk '{print $3}' | sort -u`)
if (( ${#UNIQUE_BOARDS[@]} > 1 ))
then
  echo "Multiple boards found:"
  for i in ${!UNIQUE_BOARDS[@]}
  do
    echo "${i}	${UNIQUE_BOARDS[${i}]}"
  done

  echo -n 'Select board: '
  read input
  BOARD_ID="${UNIQUE_BOARDS[${input}]}"

  if ! [ "${BOARD_ID}" ]
  then
    echo 'Invalid selection' >&2
    exit -1
  fi

  unset input
else
  BOARD_ID="${UNIQUE_BOARDS[0]}"
fi

BOARD_ID=`sed 's/[]\[]//g' <<< ${BOARD_ID}`
BOARD_STRING=`echo "${BOARDS}" | grep "${BOARD_ID}"`
if ! [ "${BOARD_STRING}" ]
then
  echo 'Invalid board' >&2
  exit -1
fi

function extract_board_params() {
  local string=$1
  local regex='([a-z]+)=([0-9]+|".*?")'
  while [[ $string =~ $regex ]]
  do
    echo "${BASH_REMATCH[1]}"
    string=${string#*"{BASH_REMATCH[1]}"}
  done
}

ALT=(`perl -ne 'print "$1\n" if /alt=([0-9]+)/;' <<< ${BOARD_STRING}`)
ADDRESS=(`perl -ne 'print "$1\n" if /name="\@Internal Flash \/(0x[0-9]+)\/.*"/;' <<< ${BOARD_STRING}`)
echo "dfu-util -d ${BOARD_ID} -s ${ADDRESS}:leave -a ${ALT} -D ${FILE}"
echo -n "Ok? [y/n] "
read input

case $input in
  [yY] ) dfu-util -d "${BOARD_ID}" -s "${ADDRESS}:leave" -a "${ALT}" -D "${FILE}"
esac
