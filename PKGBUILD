pkgname=iluluc
pkgver=1.0
pkgrel=1
pkgdesc="IluluC compiler CLI with a readable C backend"
arch=('x86_64' 'aarch64')
license=('custom')
depends=('glibc')
makedepends=('gcc' 'make')
provides=('ilulu')
conflicts=('ilulu')
source=()
sha256sums=()

build() {
    cd "${startdir:-$PWD}"
    make VERSION="${pkgver}" PKGREL="${pkgrel}"
}

package() {
    cd "${startdir:-$PWD}"
    make DESTDIR="${pkgdir}" PREFIX=/usr VERSION="${pkgver}" PKGREL="${pkgrel}" install
}
