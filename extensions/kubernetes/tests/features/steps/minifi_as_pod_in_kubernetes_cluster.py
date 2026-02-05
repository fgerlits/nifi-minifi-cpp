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
import os
import tempfile

from minifi_test_framework.containers.minifi_container import MinifiContainer
from minifi_test_framework.core.minifi_test_context import MinifiTestContext
from kubernetes_proxy import KubernetesProxy


class MinifiAsPodInKubernetesCluster(MinifiContainer):
    def __init__(self, container_name: str, test_context: MinifiTestContext):
        super().__init__(container_name, test_context)
        self._set_custom_properties()
        self.kubernetes_proxy = KubernetesProxy(resources_directory=__file__.resolve().parent / "resources")

    def _set_custom_properties(self):
        ### TODO(fgerlits) do we need these?
        self.properties["nifi.administrative.yield.duration"] = "30 sec"
        self.properties["nifi.bored.yield.duration"] = "100 millis"
        self.properties["nifi.provenance.repository.max.storage.time"] = "1 MIN"
        self.properties["nifi.provenance.repository.max.storage.size"] = "1 MB"
        self.properties["nifi.provenance.repository.class.name"] = "NoOpRepository"
        self.properties["nifi.content.repository.class.name"] = "DatabaseContentRepository"
        self.properties["nifi.c2.root.classes"] = "DeviceInfoNode,AgentInformation,FlowInformation,AssetInformation"
        self.properties["nifi.c2.full.heartbeat"] = "false"

        ### TODO(fgerlits) do we need this? by default, we log to stderr, which may be OK
        self.log_properties["spdlog.pattern"] = "[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] %v"
        self.log_properties["appender.stdout"] = "stdout"
        self.log_properties["logger.root"] = "INFO,stdout"

    def __copy_file_to_kubernetes_container(self, temp_dir: str, file_name: str, file_content: str):
        host_file = os.path.join(temp_dir, file_name)
        container_file = os.path.join("/tmp/kubernetes_config", file_name)
        self._write_content_to_file(host_file, None, file_content)
        self.kubernetes_proxy.copy_file_to_container(host_file, container_file)

    def __copy_minifi_config_to_kubernetes_container(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            self.__copy_file_to_kubernetes_container(temp_dir, "minifi.properties", self._get_properties_file_content())
            self.__copy_file_to_kubernetes_container(temp_dir, "minifi-log.properties", self._get_log_properties_file_content())
            self.__copy_file_to_kubernetes_container(temp_dir, "config.yml", self.flow_definition.to_yaml())

    def deploy(self):
        logging.info('Setting up kubernetes container')

        self.__copy_minifi_config_to_kubernetes_container()

        self.kubernetes_proxy.create_helper_objects()
        self.kubernetes_proxy.load_docker_image("apacheminificpp", "docker_test")
        self.kubernetes_proxy.create_minifi_pod()

        logging.info('Finished setting up kubernetes container')
        return True

    def get_logs(self) -> str:
        logging.debug("Getting logs from container '%s'", self.container_name)
        logs = self.kubernetes_proxy.get_logs('daemon', 'minifi')
        if logs:
            return logs
        else:
            return "No logs found"
