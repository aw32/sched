
OLDPWD="$(pwd)"
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

cd "$DIR"

git clone https://github.com/coin-or/Rehearse.git coin-Rehearse 
cd coin-Rehearse
mkdir install
./configure "--prefix=$DIR/coin-Rehearse/install"
make
make install

