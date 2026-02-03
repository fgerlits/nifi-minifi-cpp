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

import docker
import logging

from minifi_test_framework.containers.minifi_container import MinifiContainer
from minifi_test_framework.core.minifi_test_context import MinifiTestContext
from kubernetes_proxy import KubernetesProxy


class MinifiAsPodInKubernetesCluster(MinifiContainer):
    def __init__(self, container_name: str, test_context: MinifiTestContext):
        super().__init__(container_name, test_context)
        self._set_custom_properties()
        self.kubernetes_proxy = KubernetesProxy()

    def _set_custom_properties(self):
        self.properties["nifi.administrative.yield.duration"] = "30 sec"
        self.properties["nifi.bored.yield.duration"] = "100 millis"
        self.properties["nifi.provenance.repository.max.storage.time"] = "1 MIN"
        self.properties["nifi.provenance.repository.max.storage.size"] = "1 MB"
        self.properties["nifi.provenance.repository.class.name"] = "NoOpRepository"
        self.properties["nifi.content.repository.class.name"] = "DatabaseContentRepository"
        self.properties["nifi.c2.root.classes"] = "DeviceInfoNode,AgentInformation,FlowInformation,AssetInformation"
        self.properties["nifi.c2.full.heartbeat"] = "false"

        self.log_properties["spdlog.pattern"] = "[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] %v"
        self.log_properties["appender.stdout"] = "stdout"
        self.log_properties["logger.root"] = "INFO,stdout"

    def _create_container_config_dir(self, config_dir):
        return config_dir

    def deploy(self):
        if not self.set_deployed():
            return

        logging.info('Setting up container: %s', self.name)

        self._create_config()
        self.kubernetes_proxy.create_helper_objects()
        self.kubernetes_proxy.load_docker_image(MinifiAsPodInKubernetesCluster.MINIFI_IMAGE_NAME, MinifiAsPodInKubernetesCluster.MINIFI_IMAGE_TAG)
        self.kubernetes_proxy.create_minifi_pod()

        logging.info('Finished setting up container: %s', self.name)

    def get_app_log(self):
        return 'OK', self.kubernetes_proxy.get_logs('daemon', 'minifi')

    def cleanup(self):
        # cleanup is done through the kubernetes cluster in the environment.py
        pass
