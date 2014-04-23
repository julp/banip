# $FreeBSD$

PORTNAME=       libvmod-msgsend
PKGNAMEPREFIX=  varnish-
PORTVERSION=    1.0
CATEGORIES=     www
USE_GITHUB=     yes
GH_ACCOUNT=     julp
GH_COMMIT=      d04562b
GH_TAGNAME=     ${GH_COMMIT}
# GH_PROJECT=     libvmod-msgsend
DISTNAME=       ${GH_PROJECT}

USES=           cmake:outsource
BUILD_DEPENDS=  ${LOCALBASE}/include/varnish/varnishapi.h:${PORTSDIR}/www/varnish
CMAKE_ARGS+=    -DVARNISHSRC:PATH=${PORTSDIR}/www/varnish/work/varnish-3.0.5

MAINTAINER=     e2c2be7ed0f2f336@gmail.com
COMMENT=        Varnish module (vmod) to send POSIX message

XFLAGS=         -I${LOCALBASE}/include -I${LOCALBASE}/include/varnish
CFLAGS+=        ${XFLAGS}
CPPFLAGS+=      ${XFLAGS}

.include <bsd.port.mk>