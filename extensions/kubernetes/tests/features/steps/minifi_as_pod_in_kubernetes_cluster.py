# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import logging
from pathlib import Path
from typing import override

from minifi_test_framework.containers.minifi_container import MinifiContainer
from minifi_test_framework.core.minifi_test_context import MinifiTestContext
from kubernetes_proxy import KubernetesProxy


class MinifiAsPodInKubernetesCluster(MinifiContainer):
    def __init__(self, container_name: str, test_context: MinifiTestContext):
        super().__init__(container_name, test_context)
        self._set_custom_properties()
        self.container = KubernetesProxy(resources_directory=Path(__file__).resolve().parent.parent / "resources")

    def _set_custom_properties(self):
        # TODO(fgerlits) do we need these?
        self.properties["nifi.administrative.yield.duration"] = "30 sec"
        self.properties["nifi.bored.yield.duration"] = "100 millis"
        self.properties["nifi.provenance.repository.max.storage.time"] = "1 MIN"
        self.properties["nifi.provenance.repository.max.storage.size"] = "1 MB"
        self.properties["nifi.provenance.repository.class.name"] = "NoOpRepository"
        self.properties["nifi.content.repository.class.name"] = "DatabaseContentRepository"
        self.properties["nifi.c2.root.classes"] = "DeviceInfoNode,AgentInformation,FlowInformation,AssetInformation"
        self.properties["nifi.c2.full.heartbeat"] = "false"

        # TODO(fgerlits) do we need this? by default, we log to stderr, which may be OK
        self.log_properties["spdlog.pattern"] = "[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] %v"
        self.log_properties["appender.stdout"] = "stdout"
        self.log_properties["logger.root"] = "INFO,stdout"

    @override
    def deploy(self):
        logging.info('Setting up kubernetes cluster')

        self.container.create_config()
        self.container.start_cluster()
        self.container.write_minifi_conf_file("minifi.properties", self._get_properties_file_content())
        self.container.write_minifi_conf_file("minifi-log.properties", self._get_log_properties_file_content())
        self.container.write_minifi_conf_file("config.yml", self.flow_definition.to_yaml())
        self.container.create_helper_objects()
        self.container.load_docker_image("apacheminificpp", "docker_test")
        self.container.create_minifi_pod()

        logging.info('Finished setting up kubernetes cluster')
        return True

    # TODO: remove (and change in kubernetes_proxy)
    @override
    def exec_run(self, command) -> tuple[int | None, str]:
        logging.info(f"### Executing the command '{command}' in the Kubernetes cluster")
        code, output = self.container.exec_run(command)
        logging.info(f"### Result code: {code}, output: {output}")
        return code, output

    # TODO: remove (and change in kubernetes_proxy)
    @override
    def get_logs(self) -> str:
        logging.info(f"Getting logs from the minifi pod in the Kubernetes container")
        logs = self.container.get_logs('daemon', 'minifi')
        if logs:
            return logs
        else:
            return "No logs found"
