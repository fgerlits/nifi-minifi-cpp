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
import os

from minifi_test_framework.core.hooks import common_after_scenario, common_before_scenario, get_minifi_container_image


def before_feature(context, feature):
    if "SKIP_RPM" in feature.tags and "rpm" in os.environ['MINIFI_TAG_PREFIX']:
        feature.skip("This feature is not yet supported on RPM installed images")

    minifi_image = docker.from_env().images.get(get_minifi_container_image())
    minifi_image.tag("apacheminificpp", "docker_test")


def before_scenario(context, scenario):
    if "skip" in scenario.tags:
        scenario.skip("Skipping this scenario")
        return
    common_before_scenario(context, scenario)


def after_scenario(context, scenario):
    if "skip" in scenario.tags:
        scenario.skip("Skipping this scenario")
        return
    common_after_scenario(context, scenario)
