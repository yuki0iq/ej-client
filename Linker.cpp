#include "Linker.h"

bool waitForReplyDone(QNetworkReply *reply, QVector<QUrl> &urls);

Linker::Linker(QEventLoop *loop, bool &isBad, QNetworkReply **pRealReply, QVector<QUrl> &urls, QObject *parent)
	: QObject(parent)
	, lp(loop)
	, isBad(isBad)
	, pRealReply(pRealReply)
	, urls(urls)
{
}

Linker::~Linker()
{
}

void Linker::redirect(QNetworkReply *r)
{
	lp->quit();
	if (QNetworkReply::NoError != r->error())
	{
		isBad = true;
		return;
	}
	int iStatus = r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
	if (iStatus >= 300 && iStatus < 400) // Redirect
	{
		QUrl newUrl = r->url().resolved(r->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl());
		urls.push_back(newUrl);
		QNetworkReply *rr = r->manager()->get(QNetworkRequest(newUrl));
		r->deleteLater();
		pRealReply = &rr;
		waitForReplyDone(*pRealReply, urls);
		isBad = false;
		return;
	}
}

