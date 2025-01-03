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


from ..core.ControllerService import ControllerService


class CouchbaseClusterService(ControllerService):
    def __init__(self, name, connection_string, ssl_context_service=None):
        super(CouchbaseClusterService, self).__init__(name=name)

        self.service_class = 'CouchbaseClusterService'
        self.properties['Connection String'] = connection_string
        if ssl_context_service:
            self.linked_services.append(ssl_context_service)
        if not ssl_context_service or ssl_context_service and 'Client Certificate' not in ssl_context_service.properties:
            self.properties['User Name'] = "Administrator"
            self.properties['User Password'] = "password123"
