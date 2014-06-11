# $FreeBSD$

PORTNAME=	libvmod-msgsend
PORTVERSION=	1.1
CATEGORIES=	www
PKGNAMEPREFIX=	varnish-
DISTNAME=	banip

MAINTAINER=	e2c2be7ed0f2f336@gmail.com
COMMENT=	Varnish module (vmod) to send message to POSIX or System V queue

LICENSE=	BSD

# Disable dependency on www/varnish for now as it is still version 3.0.5
#BUILD_DEPENDS=	${LOCALBASE}/include/varnish/varnishapi.h:${PORTSDIR}/www/varnish

USE_GITHUB=	yes
GH_ACCOUNT=	julp
GH_COMMIT=	453d34d
GH_TAGNAME=	${GH_COMMIT}

USES=		cmake:outsource
WRKSRC=		${WRKDIR}/${GH_ACCOUNT}-${GH_PROJECT}-${GH_COMMIT}/clients/varnish4

XFLAGS=		-I${LOCALBASE}/include -I${LOCALBASE}/include/varnish
CFLAGS+=	${XFLAGS}
CPPFLAGS+=	${XFLAGS}

.include <bsd.port.mk>