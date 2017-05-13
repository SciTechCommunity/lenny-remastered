#include <depends>
#include "discord.h"
#include <QColor>

using namespace qt;

net::request request(url("http://freecomputerbooks.com/cgi-bin/searchBooks"));

float map(float value,
        float istart,
        float istop,
        float ostart,
        float ostop) {
  return ostart + (ostop - ostart) * ((value - istart) / (istop - istart));
}

pair<int,string> parseMessage(json::obj data) {
  int i = -1; stringlist s = data["content"].toString().split(' ');
  if (s.length() < 2) return qMakePair(i, string());
  if (s.takeFirst().compare("!ebooks", Qt::CaseInsensitive) == 0) {
    if (s.length() == 0) return qMakePair(--i, string());
    else if (s.length() > 1 && s.first().toInt() > 0) {
      i = s.takeFirst().toInt();
      return qMakePair(i, s.join("+"));
    } else return qMakePair(0, s.join("+"));
    return qMakePair(true, data["content"].toString().remove(0,8));
  } else return qMakePair(i, string());
}

net::reply * query(string s, net::manager * network) {
  return network->post
      (request, string("sitesearch=Books&keywords=%1")
       .arg(s)
       .toUtf8());
}

void sendHelp(Discord & d, json::var ch) {
  d.sendMessageAs(ch.toVariant().toULongLong(), "Help Menu in Progress.");
}

list< pair<string,string> > parsePage(string page) {
  string table
      = page
        .simplified()
        .split("<table id=\"bookcontent\">", string::SkipEmptyParts, Qt::CaseInsensitive)
        .last()
        .split("</table>", string::SkipEmptyParts, Qt::CaseInsensitive)
        .first();
  list< pair<string,string> > rows = [table] {
    list< pair<string,string> > r;
    auto t = table.split("<tr>");
    t.removeFirst();
    for (string s1 : t) {
      s1.chop(6);
      auto i =
          s1
          .split("</a>")
          .first()
          .split("href=", string::SkipEmptyParts, Qt::CaseInsensitive)
          .last()
          .split('>');
      s1 = i.first().split('"', string::SkipEmptyParts).first();
      if (s1.startsWith("java", Qt::CaseInsensitive)) {
        s1.chop(4); s1.remove(0, string("javascript:j('").length());
      } if (s1.startsWith('/')) s1.prepend("http://freecomputerbooks.com");
      if (s1.startsWith("ftp", Qt::CaseInsensitive)) continue;
      string s2 = i.last().remove("&copy");
      if (s2.isEmpty()) continue;
      s1 = s1.simplified().remove(' ');
      if (r.contains(qMakePair(s1,s2))) continue;
      r << qMakePair(s1,s2);
    } return r;
  }();
  return rows;
}

int main(int argc, char *argv[])
{
  core loop(argc, argv);
  Discord d;
  d.start();
  request.setRawHeader("Content-Type", "application/x-www-form-urlencoded");

  core::connect(&d, &Discord::MESSAGE_CREATE, [=, &d] (json::obj data) {
    pair<int, string> input = parseMessage(data);
    if (input.first == -1) return;
    else if (input.first == -2) sendHelp(d, data["channel_id"]);
    else {
      net::reply * rep =
          query(input.second, d.network());
      core::connect(rep, &net::reply::finished, [=, &d] {
        list< pair<string,string> >  rows = parsePage(string(rep->readAll()));
        qDebug() << rows.size();
        json::arr books; int page = 10 * input.first;
        for (int i = page; i < page+10 && i < rows.size(); ++i) {
          //          json::var color(qint64((::map(i, 0, rows.size(), 50, 200) << 16) + (0 << 8) + ::map(-i, -rows.size(), 0, 50, 200)));
          //          quint32 = c.rgb() - 0xFF000000;
          json::var color(qint64(qRgb(::map(i, 0, rows.size(), 50,200),0,::map(rows.size()-i, 0, rows.size(), 50, 200)) - 0xFF000000));
          books.append
              (json::obj {
                 {"title", rows[i].second},
                 {"url", rows[i].first},
                 {"color", color}
               });
          qDebug().noquote() << i << books.last().toObject();
        } string response =
            string("**%1**  books found matching ***%2*** showing page **%3**")
            .arg(rows.size())
            .arg(input.second)
            .arg(input.first);
        quint64 channel_id = data["channel_id"].toVariant().toULongLong();
        d.sendMessageAs(channel_id, response, books);
      });
    }
  });

  return loop.exec();
}































































































