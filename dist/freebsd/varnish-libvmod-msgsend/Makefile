# $FreeBSD$

PORTNAME=	libvmod-msgsend
PORTVERSION=	1.1
PORTREVISION=	1
CATEGORIES=	www
PKGNAMEPREFIX=	varnish-
DISTNAME=	banip

MAINTAINER=	e2c2be7ed0f2f336@gmail.com
COMMENT=	Varnish module (vmod) to send message to POSIX or System V queue

LICENSE=	BSD2CLAUSE

#BUILD_DEPENDS=	${LOCALBASE}/include/varnish/varnishapi.h:${PORTSDIR}/www/varnish
MY_DEPENDS=	${NONEXISTENT}:${PORTSDIR}/www/varnish4
RUN_DEPENDS=	${MY_DEPENDS}
BUILD_DEPENDS=	${MY_DEPENDS}

USE_GITHUB=	yes
GH_ACCOUNT=	julp
GH_TAGNAME=	e7e6b50
GH_PROJECT=	${DISTNAME}

USES=		cmake:outsource
WRKSRC=		${WRKDIR}/${GH_PROJECT}-${GH_TAGNAME}/clients/varnish4

XFLAGS=		-I${LOCALBASE}/include -I${LOCALBASE}/include/varnish
CFLAGS+=	${XFLAGS}
CPPFLAGS+=	${XFLAGS}

.include <bsd.port.mk>