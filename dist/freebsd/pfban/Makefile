# $FreeBSD$

PORTNAME=       pfban
PORTVERSION=    1.0
PORTREVISION=   2
CATEGORIES=     security
DISTNAME=       libvmod-msgsend

MAINTAINER=     e2c2be7ed0f2f336@gmail.com
COMMENT=        Interface for few firewalls to dynamically add IP ban

LICENSE=        BSD

USE_GITHUB=     yes
GH_ACCOUNT=     julp
GH_COMMIT=      453d34d
GH_TAGNAME=     ${GH_COMMIT}
GH_PROJECT=     ${DISTNAME}

USE_RC_SUBR=    pfban
USES=           cmake:outsource
WRKSRC=         ${WRKDIR}/${GH_ACCOUNT}-${GH_PROJECT}-${GH_COMMIT}/consumers/pfban

.include <bsd.port.mk>