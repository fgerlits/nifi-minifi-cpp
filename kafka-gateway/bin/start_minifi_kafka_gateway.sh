#!/bin/bash

usage() {
  echo "Usage: $0 -p <listening port> -k <kafka address>"
  exit 1
}

while getopts ":p:k:" opt
do
  case "${opt}" in
    p)
      LISTENING_PORT=${OPTARG}
      ;;
    k)
      KAFKA_ADDRESS=${OPTARG}
      ;;
    *)
      usage
      ;;
  esac
done

shift $((OPTIND-1))

if [ -z "${LISTENING_PORT}" ] || [ -z "${KAFKA_ADDRESS}" ]
then
  usage
fi

echo "listening port: ${LISTENING_PORT}"
echo "kafka address: ${KAFKA_ADDRESS}"

BASE_DIR="$( cd "$(dirname "$0")" >/dev/null 2>&1 ; cd .. ; pwd -P )"
TEMPLATE_FILE=${BASE_DIR}/templates/config.template.yml
CONF_DIR=${BASE_DIR}/home/conf
CONFIG_FILE=${CONF_DIR}/config.yml

mkdir -p ${CONF_DIR}
sed "s/LISTEN_HTTP_PROCESSOR_LISTENING_PORT/${LISTENING_PORT}/;s/PUBLISH_KAFKA_PROCESSOR_BROKER_ADDRESS/${KAFKA_ADDRESS}/" ${TEMPLATE_FILE} > ${CONFIG_FILE}

export MINIFI_HOME=${BASE_DIR}/home
${BASE_DIR}/bin/minifi &
