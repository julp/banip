# $FreeBSD$

PORTNAME=       pfban
PORTVERSION=    1.0
CATEGORIES=     security
USE_GITHUB=     yes
GH_ACCOUNT=     julp
GH_COMMIT=      d04562b
GH_TAGNAME=     ${GH_COMMIT}
GH_PROJECT=     libvmod-msgsend
DISTNAME=       ${GH_PROJECT}

USE_RC_SUBR=    pfban
USES=           cmake:outsource
WRKSRC=         ${WRKDIR}/${GH_ACCOUNT}-${GH_PROJECT}-${GH_COMMIT}/consumers/pfban

MAINTAINER=     e2c2be7ed0f2f336@gmail.com
COMMENT=        Interface for few firewalls to dynamically add IP ban

.include <bsd.port.mk>