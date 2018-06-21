if [ -f ../../env.sh ]; then
  source ../../env.sh
fi

source ./local-config.mak

if [ "$(id -u)" == "0" ]; then
  unset SUDO
else
  SUDO=sudo 
fi

set -x
./$PROJECT_NAME.out
set +x
