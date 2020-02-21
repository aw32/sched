
OLDPWD="$(pwd)"
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

cd "$DIR"
git clone https://github.com/yaml/libyaml.git
mkdir -p "$DIR/libyaml/install"
cd "$DIR/libyaml"
./bootstrap
./configure "--prefix=$DIR/libyaml/install"
make
make install
cd "$OLDPWD"
