# $FreeBSD$

PORTNAME=	libvmod-msgsend
PORTVERSION=	1.3
#PORTREVISION=	1
CATEGORIES=	www
PKGNAMEPREFIX=	varnish-
DISTNAME=	banip

COMMENT=	Varnish module (vmod) to send message to POSIX or System V queue

LICENSE=	BSD2CLAUSE

USE_GITHUB=	yes
GH_ACCOUNT=	julp
GH_TAGNAME=	0a46172
GH_PROJECT=	${DISTNAME}

USES=		cmake pkgconfig python:3.4+,build varnish
WRKSRC=		${WRKDIR}/${GH_PROJECT}-${GH_TAGNAME}/clients/varnish4

XFLAGS=		-I${LOCALBASE}/include -I${LOCALBASE}/include/varnish
CFLAGS+=	${XFLAGS}
CPPFLAGS+=	${XFLAGS}

.include <bsd.port.mk>
