#pragma once

#include <QtCore>
#include <QtNetwork>

class Linker : public QObject
{
	Q_OBJECT;
	QEventLoop *lp;
	bool isBad;
	QNetworkReply **pRealReply;
	QVector<QUrl> &urls;

public:
	Linker(QEventLoop *loop, bool &isBad, QNetworkReply **pRealReply, QVector<QUrl> &urls, QObject *parent = 0);
	~Linker();

public slots:
	void redirect(QNetworkReply *r);
};
