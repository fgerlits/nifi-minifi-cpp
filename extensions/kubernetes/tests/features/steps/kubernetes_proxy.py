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
import glob
import logging
import os
import platform
import re
import shutil
import stat
import subprocess
import tempfile
import time
from textwrap import dedent


KUBERNETES_CONTAINER_NAME = "kind-control-plane"


class KubernetesProxy:
    def __init__(self, resources_directory):
        self.temp_directory = tempfile.TemporaryDirectory()

        self.kind_binary_path = os.path.join(self.temp_directory.name, 'kind')
        self.kind_config_path = os.path.join(self.temp_directory.name, 'kind-config.yml')
        self.__download_kind()

        self.resources_directory = os.path.join(self.temp_directory.name, 'resources')
        shutil.copytree(resources_directory, self.resources_directory)

        self.minifi_conf_dir = os.path.join(self.temp_directory.name, 'kubernetes_config')
        os.mkdir(self.minifi_conf_dir)

        self.output_dir = os.path.join(self.temp_directory.name, 'output')
        os.mkdir(self.output_dir)

        self.docker_client = docker.from_env()
        self.status = "initialized"

    def cleanup(self):
        if os.path.exists(self.kind_binary_path):
            subprocess.run([self.kind_binary_path, 'delete', 'cluster'])

    def reload(selfs):
        pass

    def __download_kind(self):
        is_x86 = platform.machine() in ("i386", "AMD64", "x86_64")
        download_link = 'https://kind.sigs.k8s.io/dl/v0.18.0/kind-linux-amd64'
        if not is_x86:
            if 'Linux' in platform.system():
                download_link = 'https://kind.sigs.k8s.io/dl/v0.18.0/kind-linux-arm64'
            else:
                download_link = 'https://kind.sigs.k8s.io/dl/v0.18.0/kind-darwin-arm64'

        if not os.path.exists(self.kind_binary_path):
            if subprocess.run(['curl', '-Lo', self.kind_binary_path, download_link]).returncode != 0:
                raise Exception("Could not download kind")
            os.chmod(self.kind_binary_path, stat.S_IXUSR)

    def create_config(self):
        kind_config = dedent(f"""\
                apiVersion: kind.x-k8s.io/v1alpha4
                kind: Cluster
                nodes:
                  - role: control-plane
                    extraMounts:
                      - hostPath: {self.minifi_conf_dir}
                        containerPath: /tmp/kubernetes_config
                      - hostPath: {self.resources_directory}
                        containerPath: /var/tmp
                      - hostPath: {self.output_dir}
                        containerPath: /tmp/output
                """)

        with open(self.kind_config_path, 'wb') as config_file:
            config_file.write(kind_config.encode('utf-8'))

    def write_minifi_conf_file(self, file_name: str, content: str):
        file_path = os.path.join(self.minifi_conf_dir, file_name)
        with open(file_path, "w") as file:
            file.write(content)

    def start_cluster(self):
        subprocess.run([self.kind_binary_path, 'delete', 'cluster'])

        if subprocess.run([self.kind_binary_path, 'create', 'cluster', '--config=' + self.kind_config_path]).returncode != 0:
            raise Exception("Could not start the kind cluster")

        self.status = "running"

    def load_docker_image(self, image_name, image_tag):
        if subprocess.run([self.kind_binary_path, 'load', 'docker-image', image_name + ':' + image_tag]).returncode != 0:
            raise Exception("Could not load the %s docker image into the kind cluster" % image_name)

    def create_helper_objects(self):
        self.__wait_for_default_service_account('default')
        namespaces = self.__create_objects_of_type('namespace')
        for namespace in namespaces:
            self.__wait_for_default_service_account(namespace)

        self.__create_objects_of_type('dependencies')
        self.__create_objects_of_type('helper-pod')
        self.__create_objects_of_type('clusterrole')
        self.__create_objects_of_type('clusterrolebinding')

        self.__wait_for_pod_startup('default', 'hello-world-one')
        self.__wait_for_pod_startup('default', 'hello-world-two')
        self.__wait_for_pod_startup('kube-system', 'metrics-server')

    def create_minifi_pod(self):
        self.__create_objects_of_type('test-pod')
        self.__wait_for_pod_startup('daemon', 'minifi')

    def delete_pods(self):
        self.__delete_objects_of_type('test-pod')
        self.__delete_objects_of_type('helper-pod')

    def __wait_for_pod_startup(self, namespace, pod_name):
        for _ in range(120):
            (code, output) = self.docker_client.containers.get(KUBERNETES_CONTAINER_NAME).exec_run(['kubectl', '-n', namespace, 'get', 'pods'])
            if code == 0 and re.search(f'{pod_name}.*Running', output.decode('utf-8')):
                return
            time.sleep(1)
        raise Exception(f"The pod {namespace}:{pod_name} in the Kubernetes cluster failed to start up")

    def __wait_for_default_service_account(self, namespace):
        for _ in range(120):
            (code, output) = self.docker_client.containers.get(KUBERNETES_CONTAINER_NAME).exec_run(['kubectl', '-n', namespace, 'get', 'serviceaccount', 'default'])
            if code == 0:
                return
            time.sleep(1)
        raise Exception("Default service account for namespace '%s' not found" % namespace)

    def __create_objects_of_type(self, type):
        found_objects = []
        for full_file_name in glob.iglob(os.path.join(self.resources_directory, f'*.{type}.yml')):
            file_name = os.path.basename(full_file_name)
            file_name_in_container = os.path.join('/var/tmp', file_name)

            (code, output) = self.docker_client.containers.get('kind-control-plane').exec_run(['kubectl', 'apply', '-f', file_name_in_container])
            if code != 0:
                raise Exception("Could not create kubernetes object from file '%s': %s" % full_file_name, output.decode('utf-8'))

            object_name = file_name.replace(f'.{type}.yml', '')
            found_objects.append(object_name)
        return found_objects

    def __delete_objects_of_type(self, type):
        for full_file_name in glob.iglob(os.path.join(self.resources_directory, f'*.{type}.yml')):
            file_name = os.path.basename(full_file_name)
            file_name_in_container = os.path.join('/var/tmp', file_name)

            (code, output) = self.docker_client.containers.get('kind-control-plane').exec_run(['kubectl', 'delete', '-f', file_name_in_container, '--grace-period=0', '--force'])
            if code == 0:
                logging.info("Created component from file '%s': %s", full_file_name, output.decode('utf-8'))
            else:
                raise Exception("Could not delete kubernetes object from file '%s': %s", full_file_name, output.decode('utf-8'))

    def copy_file_to_container(self, host_file, container_file):
        if subprocess.run(['docker', 'cp', host_file, KUBERNETES_CONTAINER_NAME + ':' + container_file]).returncode != 0:
            raise Exception("Could not copy the file '%s' into the kubernetes container as '%s'" % (host_file, container_file))

    def exec_run(self, command) -> tuple[int | None, str]:
        container = self.docker_client.containers.get("kind-control-plane")
        if container:
            (code, output) = container.exec_run(command)
            return code, output.decode("utf-8")
        return None, "The kind Kubernetes cluster is not running."

    def get_logs(self, namespace, pod_name):
        (code, output) = self.docker_client.containers.get('kind-control-plane').exec_run(['kubectl', '-n', namespace, 'logs', pod_name])
        if code == 0:
            return output.decode('utf-8')
        else:
            return None
