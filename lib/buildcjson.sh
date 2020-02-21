
OLDPWD="$(pwd)"
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

cd "$DIR"
git clone https://github.com/DaveGamble/cJSON.git
mkdir -p "$DIR/cJSON/build"
mkdir -p "$DIR/cJSON/install"
cd "$DIR/cJSON/build"
cmake .. -DENABLE_CJSON_UTILS=On -DENABLE_CJSON_TEST=Off "-DCMAKE_INSTALL_PREFIX=$DIR/cJSON/install"
make
make install
cd "$OLDPWD"
