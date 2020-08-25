#include <QtCore>
#include <QtNetwork>
#include <utility>
#include <iostream>
#include "linker.h"

using namespace std;

enum EAction
{
	List_Subs,
	List_Tasks,
	Get_Prob,
	Submit_Prob,
	Get_SubID,
	Get_SubID_Code,
	Get_Code,
	Invalid
};

struct SSubRow
{
	unsigned short SubID; // 0..65535 e.g. SubID=64
	unsigned char SubTime[20]; // e.g. "2020/01/31 14:54"
	unsigned char Task; // 'A', 'B', and so on
	unsigned char Status[2]; // OK, WA and so on
	unsigned char WrongTest; // 0..255
};

EAction parseAction(char *action);
bool waitForReplyDone(QNetworkReply *reply, QVector<QUrl> &urls);
QVector<QUrl> doAction(QVector<QVariant> &args, const QString &sid, int id, QNetworkAccessManager *qnam);
QString readAll(QUrl url, QNetworkAccessManager *qnam);
QString split(const QString &x, const QString &left, const QString &right);
pair<QString, long /*after right*/> split2(const QString &x, const QString &left, const QString &right, long pos);
unsigned char *getStatus(const QString &s);

bool ej_login(QNetworkAccessManager *qnam, QString &sid, const QString &login, const QString &pass, const QString &cid);
bool ej_list_subs(QNetworkAccessManager *qnam, const QString &sid);
bool ej_list_tasks(QNetworkAccessManager *qnam, const QString &sid);
bool ej_get_prob(QNetworkAccessManager *qnam, const QString &sid, const QString &probid);
bool ej_submit_prob(QNetworkAccessManager *qnam, const QString &sid, const QString &pid, const QString &file);
bool ej_get_subid(QNetworkAccessManager *qnam, const QString &sid, const QString &pid);
bool ej_get_subid_code(QNetworkAccessManager *qnam, const QString &sid, const QString &subid, const QString &fileName);
bool ej_logout(QNetworkAccessManager *qnam, QString &sid);
bool do_interactive(QNetworkAccessManager *qnam);
bool do_cmdline(QNetworkAccessManager *qnam, int argc, char **argv);

QString sHost = "http://ejudge.algocode.ru/";

int main(int argc, char *argv[])
{
	QCoreApplication a(argc, argv);
	QNetworkAccessManager *qnam = new QNetworkAccessManager;

	if (argc > 1)
	{
		sHost = QString::fromUtf8(argv[1]);
	}
	else
	{
		printf("Using default ejudge host: %s\n", sHost.toUtf8().data());
	}

	int iRes = 0;

	if (argc > 2)
	{
		iRes = do_cmdline(qnam, argc, argv + 1) - 1;
	}
	else
	{
		iRes = do_interactive(qnam) - 1;
	}

	return iRes;
}

EAction parseAction(char *action)
{
	QString sAction = QString::fromLatin1(action).toLower();
	if (sAction == "list_subs") return List_Subs;
	if (sAction == "list_tasks") return List_Tasks;
	if (sAction == "get_prob") return Get_Prob;
	if (sAction == "submit_prob") return Submit_Prob;
	if (sAction == "get_subid") return Get_SubID;
	if (sAction == "get_subid_code") return Get_SubID_Code;
	if (sAction == "get_code") return Get_Code;
	return Invalid;
}

bool waitForReplyDone(QNetworkReply *reply, QVector<QUrl> &urls)
{
	QTimer timer;
	QEventLoop eventLoop;
	bool isBad = false;
	Linker linker(&eventLoop, isBad, &reply, urls);
	eventLoop.connect(reply->manager(), SIGNAL(finished(QNetworkReply*)), &linker, SLOT(redirect(QNetworkReply*)));
	eventLoop.connect(&timer, SIGNAL(timeout()), &eventLoop, SLOT(quit()));
	timer.setSingleShot(true);
	timer.start(5000);
	eventLoop.exec();
	timer.stop();
	return !isBad;
}

QVector<QUrl> doAction(QVector<QVariant> &args, const QString &sid, int id, QNetworkAccessManager *qnam)
{
	QString req = sHost + QString("cgi-bin/new-client?SID=%1&action=%2").arg(sid).arg(id);
	switch (id)
	{
		// For simple actions that require no params, do nothing
		case 140: // list_subs
			req += "&all_runs=1";
		case 137: // info
		case 74: // logout
			break;
		case 139: // get_prob
			req += QString("&prob_id=%1").arg(args[0].toString());
			break;
		case 37: // subid_info
		case 91: // subid_code
			req += QString("&run_id=%1").arg(args[0].toString());
			break;
		default:
			puts("wip!");
			return QVector<QUrl>();
	}

	QVector<QUrl> urls;
	urls.push_back(req);
	if(waitForReplyDone(qnam->get(QNetworkRequest(QUrl(req))), urls))
		return urls;
	return QVector<QUrl>();
}

QString readAll(QUrl url, QNetworkAccessManager *qnam)
{
	QNetworkReply *r = qnam->get(QNetworkRequest(url));
	QTimer timer;
	QEventLoop eventLoop;
	eventLoop.connect(r, SIGNAL(finished()), &eventLoop, SLOT(quit()));
	eventLoop.connect(&timer, SIGNAL(timeout()), &eventLoop, SLOT(quit()));
	timer.setSingleShot(true);
	timer.start(5000);
	eventLoop.exec();
	if (!timer.isActive()) return "";
	timer.stop();
	return r->readAll();
}

QString split(const QString &x, const QString &left, const QString &right)
{
	return split2(x, left, right, 0).first;
}

pair<QString, long> split2(const QString &x, const QString &left, const QString &right, long pos)
{
	int idx_start = x.indexOf(left, pos) + left.size();
	QString x_right = x.right(x.size() - idx_start);
	int idx_end = x_right.indexOf(right);
	return { x_right.left(idx_end), idx_start + idx_end + right.size() };
}

unsigned char sZZ[2] = { 'Z', 'Z' };
unsigned char *getStatus(const QString &s)
{
	static const pair<const char*, QString> str_to_status_table[] =
	{
		{ "OK", "OK" },
		{ "CE", "Compilation error" },
		{ "RT", "Run-time error" },
		{ "TL", "Time-limit exceeded" },
		{ "PE", "Presentation error" },
		{ "WF", "Wrong output format" },
		{ "WA", "Wrong answer" },
		{ "CF", "Check failed" },
		{ "PT", "Partial solution" },
		{ "AC", "Accepted for testing" },
		{ "IG", "Ignored" },
		{ "DQ", "Disqualified" },
		{ "PD", "Pending check" },
		{ "ML", "Memory limit exceeded" },
		{ "SE", "Security violation" },
		{ "SY", "Synchronization error" },
		{ "SV", "Coding style violation" },
		{ "WT", "Wall time-limit exceeded" },
		{ "PR", "Pending review" },
		{ "SM", "Summoned for defence" },
		{ "RJ", "Rejected" },
		{ "SK", "Skipped" },
		{ "RU", "Running..." },
		{ "CD", "Compiled" },
		{ "CG", "Compiling..." },
		{ "AV", "Available" },
		{ "EM", "EMPTY" },
		{ "VS", "Virtual start" },
		{ "VT", "Virtual stop" }
	};
	for (int i = 0; i < sizeof(str_to_status_table) / sizeof(str_to_status_table[0]); ++i)
	{
		if (s == str_to_status_table[i].second)
		{
			return (unsigned char *)str_to_status_table[i].first;
		}
	}

	return sZZ;
}

bool ej_login(QNetworkAccessManager *qnam, QString &sid, const QString &login, const QString &pass, const QString &cid)
{
	QNetworkRequest loginreq(QUrl(sHost + "cgi-bin/new-client"));
	loginreq.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
	QNetworkReply *reply = qnam->post(loginreq, (
		QString("?role=0&prob_name=&locale_id=0&action_2=Log+in")
		+ QString("&contest_id=%1&login=%2&password=%3")
		.arg(cid).arg(login).arg(pass)
		).toLatin1());

	QVector<QUrl> urls;
	urls.push_back(reply->url());
	if (!waitForReplyDone(reply, urls))
		return false;

	QString url = urls.back().toString();
	if (url.indexOf("SID=") == -1)
		return false;

	sid = split(url, "SID=", "&");
	return true;
}

bool ej_list_subs(QNetworkAccessManager *qnam, const QString &sid)
{
	QVector<QUrl> vec;
	if ((vec = doAction(QVector<QVariant>(), sid, 140, qnam)).isEmpty())
		return false;
	QString html = readAll(vec.back(), qnam);

	QString innerTable = split(html, "<table class=\"table\">", "</table>").replace("\n", "");

	QString list3 = "SubID Time             Task St Test\n";
	SSubRow subrow;
	subrow.SubTime[19] = 0;
	long pos = innerTable.indexOf("</tr>");
	while (pos < innerTable.size())
	{
		pair<QString, long> row = split2(innerTable, "<tr>", "</tr>", pos);
		pos = row.second;

		// SubID Time Size Task Lang Status Test Source Proto

		QString s = row.first;
		long pos2 = 0;

		row = split2(s, ">", "</td>", pos2);
		subrow.SubID = row.first.toUShort();
		pos2 = row.second;

		row = split2(s, ">", "</td>", pos2);
		memcpy(subrow.SubTime, row.first.toLatin1().data(), row.first.toLatin1().size());
		pos2 = row.second;

		row = split2(s, ">", "</td>", pos2);
		pos2 = row.second;

		row = split2(s, ">", "</td>", pos2);
		subrow.Task = row.first[0].cell();
		pos2 = row.second;

		row = split2(s, ">", "</td>", pos2);
		pos2 = row.second;

		row = split2(s, ">", "</td>", pos2);
		memcpy(subrow.Status, getStatus(row.first), 2);
		pos2 = row.second;

		row = split2(s, ">", "</td>", pos2);
		subrow.WrongTest = (unsigned char)(row.first.toUShort() & 0xFF);
		pos2 = row.second;

		list3 += QString("%1 %2 %3 %4%5 %6\n")
			.arg(subrow.SubID, 5)
			.arg((char*)subrow.SubTime)
			.arg(QLatin1Char(subrow.Task))
			.arg(QLatin1Char(subrow.Status[0]))
			.arg(QLatin1Char(subrow.Status[1]))
			.arg((short)subrow.WrongTest);
	}

	printf(list3.toLatin1().data());
	return true;
}

bool ej_list_tasks(QNetworkAccessManager *qnam, const QString &sid)
{
	QVector<QUrl> vec;
	if ((vec = doAction(QVector<QVariant>(), sid, 137, qnam)).isEmpty())
		return false;
	QString html = readAll(vec.back(), qnam);
	
	QString innerHtml = split(html, "<table class=\"table\">", "</table>");
	long pos = (innerHtml.indexOf("</tr>") + 5);
	innerHtml = innerHtml.right(innerHtml.size() - pos)
		.replace(QRegExp("\\s\\s+"), "")
		.trimmed()
		.replace(QRegularExpression("\\s*class=\".*?\"\\s*"), "")
		.replace("&nbsp;", "")
		.replace(QRegularExpression("\\s>"), ">");

	QString text = "PID St Test SubID Name\n";
	pos = 0;
	while (pos < innerHtml.size())
	{
		pair<QString, long> row = split2(innerHtml, "<tr>", "</tr>", pos);
		QString s = row.first;
		pos = row.second;
		long pos2 = 0;
		// ID link+NAME Status Test SubID
		QString id, lnk_nm, status, test, subid;

		row = split2(s, "<td>", "</td>", pos2);
		pos2 = row.second;

		row = split2(s, "<td>", "</td>", pos2);
		lnk_nm = row.first;
		pos2 = row.second;

		row = split2(s, "<td>", "</td>", pos2);
		status = row.first;
		pos2 = row.second;

		row = split2(s, "<td>", "</td>", pos2);
		test = row.first.isEmpty() ? "    " : QString("%1").arg(row.first.toUShort(), 4);
		pos2 = row.second;

		row = split2(s, "<td>", "</td>", pos2);
		subid = split(row.first, ">", "</a");
		subid = subid.isEmpty() ? "    " : QString("%1").arg(subid.toUShort(), 4);
		pos2 = row.second;

		QString pid = split(lnk_nm, "prob_id=", "\"");
		QString name = split(lnk_nm, ">", "</a>");

		unsigned char Status[2];
		memcpy(Status, getStatus(status), 2);

		text += QString("%1 %2%3 %4 %5 %6\n")
			.arg(pid.toUShort(), 3)
			.arg(QLatin1Char(Status[0]))
			.arg(QLatin1Char(Status[1]))
			.arg(test)
			.arg(subid.toUShort(), 5)
			.arg(name);
	}

	printf(text.toUtf8().data());
	return true;
}

bool ej_get_prob(QNetworkAccessManager *qnam, const QString &sid, const QString &probid)
{
	QVector<QUrl> vec;
	QVector<QVariant> arg;
	arg.push_back(probid);
	if ((vec = doAction(arg, sid, 139, qnam)).isEmpty())
		return false;
	QString html = readAll(vec.back(), qnam);
	QString innerHtml = split(html, "<div id=\"probNavTaskArea-ins\">", "<h3>Submit a solution</h3>").trimmed();
	QString taskName = split(html, "<h2>Submit a solution for ", "</h2>").trimmed();
	QString text = "Problem " + taskName + "\n";

	long pos = 0;
	pair<QString, long> constraints2 = split2(innerHtml, "<table class=\"line-table-wb\">", "</table>", 0);
	pos = constraints2.second;
	text += constraints2.first.replace("\n", "").replace("  <tr><td><b>", "").replace("</b></td><td><tt>", " ").replace("</tt></td></tr>", "\n");

	pos = innerHtml.indexOf("</h3>", pos) + 5;
	QString innerHtml2 = innerHtml.right(innerHtml.size() - pos).trimmed();
	pos = 0;

	while (pos < innerHtml2.size())
	{
		long pos_pre = innerHtml2.indexOf("<pre>", pos);
		long pos_pre1 = pos_pre + 5;
		long pos_pre2 = innerHtml2.indexOf("</pre>", pos_pre);
		QString s1 = innerHtml2.left(pos_pre).right(pos_pre - pos)
			.replace(QRegularExpression("(<(h[34]|p|[\\/]?span.*?)>|\\n)"), "")
			.replace(QRegularExpression("<\\/(h[34]|p)>"), "\n");
		text += s1;
		text += innerHtml2.left(pos_pre2).right(pos_pre2 - pos_pre1);
		pos = pos_pre2 + 6;
	}

	printf(text.toUtf8().data());
	return true;
}

bool ej_submit_prob(QNetworkAccessManager *qnam, const QString &sid, const QString &pid, const QString &file)
{
	QString file_data;
	{
		QFile f(file);
		if (!f.open(QIODevice::ReadOnly))
		{
			printf("File can't be opened!\n");
			return false;
		}
		file_data = f.readAll();
	}
	bool isJavaCode = (-1 != file.indexOf(".java"));
	file_data.replace("\n", "\r\n");

	QString data;
	data += QString("--123\r\nContent-Disposition: form-data; name=\"SID\"\r\n\r\n%1\r\n").arg(sid);
	data += QString("--123\r\nContent-Disposition: form-data; name=\"prob_id\"\r\n\r\n%1\r\n").arg(pid);
	data += QString("--123\r\nContent-Disposition: form-data; name=\"lang_id\"\r\n\r\n%1\r\n").arg(isJavaCode ? 18 : 3);
	data += QString("--123\r\nContent-Disposition: form-data; name=\"eoln_type\"\r\n\r\n1\r\n");
	data += QString("--123\r\nContent-Disposition: form-data; name=\"file\"\r\nContent-Type: application/octet-stream\r\n\r\n%1\r\n").arg(file_data);
	data += QString("--123\r\nContent-Disposition: form-data; name=\"action_40\"\r\n\r\nSend!\r\n");
	data += QString("--123--\r\n");

	QNetworkRequest request(QUrl(sHost + "cgi-bin/new-client"));
	request.setRawHeader("Content-Type", "multipart/form-data; boundary=123");
	request.setRawHeader("Content-Length", QString::number(data.toUtf8().size()).toUtf8());

	QNetworkReply *reply = qnam->post(request, data.toUtf8());

	QVector<QUrl> urls;
	urls.push_back(reply->url());
	return waitForReplyDone(reply, urls);
}

bool ej_get_subid(QNetworkAccessManager *qnam, const QString &sid, const QString &pid)
{
	QVector<QUrl> vec;
	QVector<QVariant> arg;
	arg.push_back(pid);
	if ((vec = doAction(arg, sid, 37, qnam)).isEmpty())
		return false;
	QString html = readAll(vec.back(), qnam);
	if (-1 != html.indexOf("Operation completed with errors"))
	{
		printf("No such SubID found\n");
		return false;
	}

	QString innerHtml = split(html, "<div class=\"l14\">", "</div>\n<div id=\"footer\">").trimmed();

	if (-1 != innerHtml.indexOf("Resubmit"))
	{
		long pos = innerHtml.indexOf("</p>");
		innerHtml = innerHtml.right(innerHtml.size() - (pos + 4));
	}

	long pos = 0;
	QString text;
	while (pos < innerHtml.size())
	{
		long pos_pre = innerHtml.indexOf("<pre>", pos), pos_pre1, pos_pre2, pos_pre3;
		if (pos_pre == -1)
		{
			pos_pre1 = pos_pre2 = pos_pre3 = pos_pre = innerHtml.size();
		}
		else
		{
			pos_pre1 = pos_pre + 5;
			pos_pre2 = innerHtml.indexOf("</pre>", pos_pre);
			pos_pre3 = pos_pre2 + 6;
		}
		// Here we strip all html tags from 's1'
		QString s1 = innerHtml.left(pos_pre).right(pos_pre - pos)
			.replace(QRegularExpression("(<(h[234].*?|p.*?|\\/t(d|able)|tr|[\\/]?(span|div|font|big).*?)>|\\n)"), "")
			.replace(QRegularExpression("<(\\/(h[234]|p)|br[\\s+[\\/]?]?|\\/tr)>"), "\n")
			.replace(QRegExp("\\s\\s+"), "")
			.replace("<table class=\"message-table\"><tr class=\"mes-top\"><td>Author<td>Run comment", "")
			.replace("<table class=\"table\"><th class=\"b1\">N</th><th class=\"b1\">Result</th><th class=\"b1\">Time (sec)</th>", "")
			.replace("\n<td class=\"profile\"><b>Judge</b>", "")
			.replace("<td>", "\n")
			.replace("<td class=\"b1\">", " ")
			.replace("&gt;", ">")
			.replace(QRegularExpression("^\\s+", QRegularExpression::MultilineOption), "");
		text += s1;
		text += innerHtml.left(pos_pre2).right(pos_pre2 - pos_pre1);
		pos = pos_pre3;
	}
	if (-1 != text.indexOf("Permission denied"))
	{
		printf("Permission denied\n");
		return false;
	}
	if (-1 != text.indexOf("Report is not available"))
	{
		printf(
			"Report for given SubID is not available.\n"
			"This may be because this submission hadn't checked yet or has a status of CF.\n"
		);
		return false;
	}

	printf(text.toUtf8().data());
	return true;
}

bool ej_get_subid_code(QNetworkAccessManager *qnam, const QString &sid, const QString &subid, const QString &fileName)
{
	QVector<QUrl> vec;
	QVector<QVariant> arg;
	arg.push_back(subid);
	if ((vec = doAction(arg, sid, 91, qnam)).isEmpty())
		return false;
	QString html = readAll(vec.back(), qnam);
	QFile file(fileName);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
		return false;
	file.write(html.toUtf8());
	return true;
}

bool ej_get_code(QNetworkAccessManager *qnam, const QString &sid, const QString &subid)
{
	QVector<QUrl> vec;
	QVector<QVariant> arg;
	arg.push_back(subid);
	if ((vec = doAction(arg, sid, 91, qnam)).isEmpty())
		return false;
	printf(readAll(vec.back(), qnam).toUtf8().data());
	return true;
}

bool ej_logout(QNetworkAccessManager *qnam, QString &sid)
{
	if (!doAction(QVector<QVariant>(), sid, 74, qnam).isEmpty())
	{
		sid.clear();
		return true;
	}
	return false;
}

bool do_interactive(QNetworkAccessManager *qnam)
{
	printf(
		"ej-client: ejudge console client, 1.1. Licensed under 3-clause BSD license\n"
		"    a Qt-based console client for submitting solutions and viewing problems for ejudge\n"
		"  ej-client (C) 2020 developerxyz. Contact me on telegram [t.me/developerxyz]\n"
		"  ejudge (C) 2000-2020 Alexander Chernov\n"
		"    Don't know how to start? Try writing \"help\" down!\n"
	);
	QString sid, cmd;
	QTextStream tin(stdin), tout(stdout);
	while (true)
	{
		printf("\n> ");
		tin >> cmd;
		cmd.toLower();
		if (cmd == "exit")
			break;
		else if (cmd == "help")
		{
			printf(
				"Available commands:\n"
				"  help: view this help\n"
				"  exit: close ej-client\n"
				"  host: print current ejudge host url\n"
				"  set_host <address>:\n"
				"    set ejudge host (with slash!!!). Default: 'http://ejudge.algocode.ru/'\n"
				"  login <login> <password> <contest_id>:\n"
				"    login to contest <contest_id> with given creds\n"
				"  logout: do logout\n"
				"  list_tasks: list available tasks\n"
				"  list_subs: list all your submissions\n"
				"  get_prob <prob_id>:\n"
				"    get problem <prob_id> constraints and story with examples\n"
				"    e.g. for pasting into vim\n"
				"  submit_prob <prob_id> <code>:\n"
				"    submit <code> to <prob_id>. Language is always g++ except\n"
				"    for '.java' extension.\n"
				"  get_subid <subid>: get all info for submission <subid>\n"
				"  get_subid_code <subid> <file>: download source code for <subid>\n"
				"  get_code <subid>: print source code for <subid>\n"
				"Warning! path to any file SHOULD NOT contain spaces!\n"
				"ej-client supports simplified syntax for launching single command:\n"
				"  ej-client <host> <command> <login> <password> <contest> <args...>\n"
				"where <command> is not in { login, logout }\n"
			);
		}
		else if (cmd == "login")
		{
			QString l, p, c;
			tin >> l >> p >> c;
			if (!ej_login(qnam, sid, l, p, c))
			{
				printf(
					"Error: could not login!\n"
					"Check internet connection, ping ejudge and/or\n"
					"  check credentials and contest id.\n"
				);
			}
		}
		else if (cmd == "logout")
		{
			if (!ej_logout(qnam, sid))
			{
				printf(
					"Error: could not logout!\n"
					"Check internet connection or ping ejudge.\n"
					"You may need to relogin\n"
				);
			}
		}
		else if (cmd == "list_tasks")
		{
			if (!ej_list_tasks(qnam, sid))
			{
				printf(
					"Error: could not get tasks list!\n"
					"Check internet connection or ping ejudge.\n"
					"You may need to relogin\n"
				);
			}
		}
		else if (cmd == "list_subs")
		{
			if (!ej_list_subs(qnam, sid))
			{
				printf(
					"Error: could not get submissions list!\n"
					"Check internet connection or ping ejudge.\n"
					"You may need to relogin\n"
				);
			}
		}
		else if (cmd == "get_prob")
		{
			QString s;
			tin >> s;
			if (!ej_get_prob(qnam, sid, s))
			{
				printf(
					"Error: could not get problem text!\n"
					"Check internet connection or ping ejudge.\n"
					"You may need to relogin\n"
				);
			}
		}
		else if (cmd == "submit_prob")
		{
			QString pid, fn;
			tin >> pid >> fn;
			if (!ej_submit_prob(qnam, sid, pid, fn))
			{
				printf(
					"Error: could not submit problem!\n"
					"Check internet connection or ping ejudge.\n"
					"There may also be a disk error, check your file\n"
					"You may need to relogin\n"
				);
			}
		}
		else if (cmd == "get_subid")
		{
			QString pid;
			tin >> pid;
			if (!ej_get_subid(qnam, sid, pid))
			{
				printf(
					"Error: could not get submission info!\n"
					"Check internet connection or ping ejudge.\n"
					"There may also be a disk error, check your file\n"
					"You may need to relogin\n"
				);
			}
		}
		else if (cmd == "get_subid_code")
		{
			QString subid, fn;
			tin >> subid >> fn;
			if (!ej_get_subid_code(qnam, sid, subid, fn))
			{
				printf(
					"Error: could not get problem text!\n"
					"Check internet connection or ping ejudge.\n"
					"There may also be a disk error, check your file\n"
					"You may need to relogin\n"
				);
			}
		}
		else if (cmd == "get_code")
		{
			QString subid;
			tin >> subid;
			if (!ej_get_code(qnam, sid, subid))
			{
				printf(
					"Error: could not get problem text!\n"
					"Check internet connection or ping ejudge.\n"
					"You may need to relogin\n"
				);
			}
		}
		else if (cmd == "host")
		{
			printf("Current host is '%s'\n", sHost.toUtf8().data());
		}
		else if (cmd == "set_host")
		{
			QString host;
			tin >> host;
			printf("Logging out...\n");
			if (sid.isEmpty() || ej_logout(qnam, sid))
			{
				printf("Using host %s\n", host.toUtf8().data());
				sHost = host;
			}
		}
		else
		{
			printf(
				"Error: entered unknown command!\n"
				"Try writing \"help\" and check if it exists.\n"
				"If you believe this is not an error or you know how to make ej-client better,\n"
				"contact me at telegram: t.me/develoeprxyz\n"
			);
		}
	}

	ej_logout(qnam, sid);
	return true;
}

bool do_cmdline(QNetworkAccessManager *qnam, int argc, char **argv)
{
	EAction action = parseAction(argv[1]);
	if (action == Invalid)
		return false;

	QString sid;
	ej_login(qnam, sid, QString(argv[2]), QString(argv[3]), QString(argv[4]));

	switch (action)
	{
		case List_Subs:
			return ej_list_subs(qnam, sid);
		break;
		case List_Tasks:
			return ej_list_tasks(qnam, sid);
		case Get_Prob:
			return ej_get_prob(qnam, sid, QString::fromLatin1(argv[5]));
		case Submit_Prob:
			return ej_submit_prob(qnam, sid, QString::fromLatin1(argv[5]), QString::fromLatin1(argv[6]));
		case Get_SubID:
			return ej_get_subid(qnam, sid, QString::fromLatin1(argv[5]));
		case Get_SubID_Code:
			return ej_get_subid_code(qnam, sid, QString::fromLatin1(argv[5]), QString::fromLatin1(argv[6]));
		case Get_Code:
			return ej_get_code(qnam, sid, QString::fromLatin1(argv[5]));
		default:
			break;
	}

	ej_logout(qnam, sid);

	return true;
}

