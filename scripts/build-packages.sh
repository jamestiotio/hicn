# Copyright (c) 2017-2019 Cisco and/or its affiliates.
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at:
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

#!/bin/bash
set -euxo pipefail

SCRIPT_PATH=$( cd "$(dirname "${BASH_SOURCE}")" ; pwd -P )
source ${SCRIPT_PATH}/functions.sh

# Parameters:
# $1 = Package name
#
function build_package() {
    setup

    echo "*******************************************************************"
    echo "********************* STARTING PACKAGE BUILD **********************"
    echo "*******************************************************************"

    # Make the package
    make -C ${SCRIPT_PATH}/.. INSTALL_PREFIX=/usr test package-release

    pushd ${SCRIPT_PATH}/../build-release-${ID}
      find . -not -name '*.deb' -not -name '*.rpm' -print0 | xargs -0 rm -rf -- || true
      rm *Unspecified* *Development* *development* || true
    popd

    echo "*******************************************************************"
    echo "*****************  BUILD COMPLETED SUCCESSFULLY *******************"
    echo "*******************************************************************"
}

build_sphinx() {
    setup

    echo "*******************************************************************"
    echo "********************* STARTING DOC BUILD **************************"
    echo "*******************************************************************"

    make doc

    echo "*******************************************************************"
    echo "*****************  BUILD COMPLETED SUCCESSFULLY *******************"
    echo "*******************************************************************"
}

function usage() {
    echo "Usage: ${0} [sphinx|packages]"
    exit 1
}

if [ -z ${1+x} ]; then
    set -- "packages"
fi

case "${1}" in
  sphinx)
    build_sphinx
    ;;
  packages)
    build_package
    ;;
  *)
    usage
esac

exit 0
