#ifndef DISCORD_H
#define DISCORD_H
#include <depends>
#include <secrets>
#include <random>

static std::random_device rd;
static std::mt19937 mrand(rd());
static std::mt19937_64 mrand64(rd());

using namespace qt;
class Discord : public object
{
  Q_OBJECT
  int heart;
  json::obj hook, guild;
  json::var seq;
  url server_url;
  net::request request;
  net::websocket * socket;
  net::manager * sender;

public:
  Discord(object * parent = nullptr,
          url _url = string("wss://gateway.discord.gg/?encoding=json&v=6")) :
    object(parent), server_url(_url)
  {
    socket = new net::websocket;
    socket->setParent(this);
    auto config = QSslConfiguration::defaultConfiguration();
    config.setProtocol(QSsl::TlsV1_2OrLater);
    socket->setSslConfiguration(config);
    connect(socket, &net::websocket::connected, []() {
      qDebug() << "OPEN!";
    }); connect(socket, &net::websocket::aboutToClose, [this]() {
      qDebug() << "CLOSED!";
    }); connect(socket, static_cast<void(net::websocket::*)
                (net::socket::SocketError)>(&net::websocket::error),
                [](net::socket::SocketError error){
      qDebug() << error;
    }); connect(socket, &net::websocket::stateChanged,
                [this](net::socket::SocketState state) {
      qDebug() << state;
      if (state == net::socket::UnconnectedState) {
        killTimer(heart);
        socket->open(server_url);
      }
    }); connect(socket, &net::websocket::textMessageReceived,
                this, &Discord::onMessageReceived);

    sender = new net::manager;
    sender->setParent(this);

    request.setSslConfiguration(config);
    request.setRawHeader
        ("User-Agent", string("(%1, %2)")
         .arg(URL, VERSION_NUMBER)
         .toLocal8Bit());
    request.setRawHeader
        ("Authorization", string("%1 %2")
         .arg(TOKEN_TYPE, TOKEN)
         .toLocal8Bit());
    request.setRawHeader
        ("Content-Type", "application/json");
    request.setUrl(url(ENDPOINT));
  }
  ~Discord() {}
  auto start()
  {
    connect(this, &Discord::dispatchEvent, this, &Discord::onDispatchEvent, Qt::QueuedConnection);
    connect(this, &Discord::helloEvent, this, &Discord::onHelloEvent, Qt::QueuedConnection);

    connect(this, &Discord::sendMessage, this, &Discord::onSendMessage, Qt::QueuedConnection);
    connect(this, &Discord::sendMessageAs, this, &Discord::onSendMessageAs, Qt::QueuedConnection);
    connect(this, &Discord::deleteMessage, this, &Discord::onDeleteMessage, Qt::QueuedConnection);
    connect(this, &Discord::pinMessage, this, &Discord::onPinMessage, Qt::QueuedConnection);
    connect(this, &Discord::unpinMessage, this, &Discord::onUnpinMessage, Qt::QueuedConnection);
    connect(this, &Discord::reactToMessage, this, &Discord::onReactToMessage, Qt::QueuedConnection);

    connect(this, &Discord::GUILD_CREATE, this, &Discord::onJoinEvent, Qt::QueuedConnection);
    connect(this, &Discord::ERROR, this, &Discord::onErrorEvent, Qt::QueuedConnection);

    socket->open(server_url);
  }
  net::manager * network()
  {
    return sender;
  }
  net::websocket * gateway()
  {
    return socket;
  }

signals:
  void dispatchEvent(json::var, json::obj);
  void helloEvent(json::var, json::var);

  void sendMessage(quint64 channel, string content, json::obj embed = json::obj());
  void sendMessageAs(quint64 channel, string content, json::arr embeds = json::arr());
  void deleteMessage(quint64 channel, quint64 message);
  void pinMessage(quint64 channel, quint64 message);
  void unpinMessage(quint64 channel, quint64 message);
  void reactToMessage(quint64 channel, quint64 message, quint64 emoji);
  void deleteReaction();

  void createChannel();
  void deleteChannel();

  void createRole();
  void deleteRole();

  void banUser();
  void unbanUser();

  void updateStatus();


private slots:
  void onMessageReceived(const string & _message)
  {
    json::obj message =
        json::doc::fromJson(_message.toUtf8()).object();
    switch (message["op"].toInt()) {
    case 0:
      emit dispatchEvent(message["t"], message["d"].toObject());
      break;
    case 11:
      emit dispatchEvent(json::var("KEEP_ALIVE"), json::obj());
      break;
    case 10:
      emit helloEvent(message["s"], message["d"].toObject()["heartbeat_interval"]);
      break;
    default:
      qDebug() << message;
      break;
    }
  }
  void onDispatchEvent(json::var type, json::obj data)
  {
    qDebug().noquote() << type.toString();
    if (!metaObject()->
        method
        (metaObject()->
         indexOfSignal
         (type
          .toString()
          .append("(json::obj)")
          .toLocal8Bit()))
        .invoke
        (this,
         Qt::QueuedConnection,
         Q_ARG(QJsonObject, data)))
    {
      emit ERROR(data);
    }
  }
  void onHelloEvent(json::var seq, json::var beat)
  {
    heart = startTimer(beat.toInt());
    this->seq = seq;
    socket->
        sendTextMessage
        (string
         (json::doc
          (json::obj {
             {"op", 2},
             {"d", json::obj {
                {"token", TOKEN},
                {"properties", json::obj {
                   {"$browser", "shadow was here"},
                 }
                },
                {"large_threshold", 50}
              }
             }
           })
          .toJson()));
  }
  void onJoinEvent(json::obj data)
  {
    guild = data;
    net::request req(request);
    url newUrl = req.url();
    newUrl.setPath
        (string("%1/guilds/%2/webhooks")
         .arg(req.url().path())
         .arg(guild["id"].toString()));
    req.setUrl(newUrl);
    net::reply * rep = sender->get(req);
    connect(rep, &net::reply::finished, [rep, this] {
      guild["webhooks"] = json::doc::fromJson(rep->readAll()).array();
    });
  }
  void onErrorEvent(json::obj error)
  {
    qDebug() << "ERROR!";
    qDebug().noquote()
        << json::doc(error)
           .toJson(json::doc::Indented);
  }

public slots:
  void onSendMessage(quint64 channel, string content, json::obj embed = json::obj())
  {
    net::request req(request);
    url newUrl = req.url();
    newUrl.setPath
        (string("%1/channels/%2/messages")
         .arg(req.url().path())
         .arg(channel));
    req.setUrl(newUrl);
    json::obj message {
      {"content", content},
      {"embed", embed}
    };
    replyEvent
        (sender->post(req,json::doc(message).toJson()));
  }
  void onSendMessageAs(quint64 channel, string content, json::arr embeds = json::arr())
  {
    net::request req(request);
    url newUrl = req.url();
    if (hook["channel_id"].toVariant().toULongLong() != channel) {
      hook = json::obj();
      for (json::var i : guild["webhooks"].toArray()) {
        json::obj o = i.toObject();
        quint64 ch = o["channel_id"].toVariant().toULongLong();
        if (ch == channel) {
          hook = o;
          break;
        }
      } if (hook["id"].isUndefined()) {
        qDebug() << "You don't have root on this channel.";
        return;
      }
    } newUrl.setPath
        (string("%1/webhooks/%2/%3")
         .arg(req.url().path())
         .arg(hook["id"].toString())
        .arg(hook["token"].toString()));
    req.setUrl(newUrl);
    json::obj message {
      {"username", USERNAME},
      {"avatar_url", AVATAR},
      {"content", content},
      {"embeds", embeds}
    };
    replyEvent
        (sender->post(req,json::doc(message).toJson()));
  }
  void onDeleteMessage(quint64 channel, quint64 message)
  {
    net::request req(request);
    url newUrl = req.url();
    newUrl.setPath
        (string("%1/channels/%2/messages/%3")
         .arg(req.url().path())
         .arg(channel)
         .arg(message));
    req.setUrl(newUrl);
    replyEvent
        (sender->deleteResource(req));
  }
  void onPinMessage(quint64 channel, quint64 message)
  {
    net::request req(request);
    url newUrl = req.url();
    newUrl.setPath
        (string("%1/channels/%2/pins/%3")
         .arg(req.url().path())
         .arg(channel)
         .arg(message));
    req.setUrl(newUrl);
    replyEvent
        (sender->put(req,bytes()));
  }
  void onUnpinMessage(quint64 channel, quint64 message)
  {
    net::request req(request);
    url newUrl = req.url();
    newUrl.setPath
        (string("%1/channels/%2/pins/%3")
         .arg(req.url().path())
         .arg(channel)
         .arg(message));
    req.setUrl(newUrl);
    replyEvent
        (sender->deleteResource(req));
  }
  void onReactToMessage(quint64 channel, quint64 message, quint64 emoji)
  {
    net::request req(request);
    url newUrl = req.url();
    newUrl.setPath
        (string("%1/channels/%2/messages/%3/reactions/%4/@me")
         .arg(req.url().path())
         .arg(channel)
         .arg(message)
         .arg(emoji));
    req.setUrl(newUrl);
    replyEvent
        (sender->put(req,bytes()));
  }


signals:
  void READY(json::obj data);
  void ERROR(json::obj data);
  void KEEP_ALIVE(json::obj);
  void TYPING_START(json::obj data);
  void PRESENCE_UPDATE(json::obj data);

  void VOICE_STATE_UPDATE(json::obj data);
  void VOICE_SERVER_UPDATE(json::obj data);
  void GUILD_CREATE(json::obj data);
  void GUILD_UPDATE(json::obj data);
  void GUILD_DELETE(json::obj data);

  void GUILD_MEMBER_ADD(json::obj data);
  void GUILD_MEMBER_UPDATE(json::obj data);
  void GUILD_MEMBER_REMOVE(json::obj data);
  void GUILD_MEMBERS_CHUNK(json::obj data);

  void GUILD_ROLE_CREATE(json::obj data);
  void GUILD_ROLE_UPDATE(json::obj data);
  void GUILD_ROLE_DELETE(json::obj data);

  void GUILD_BAN_ADD(json::obj data);
  void GUILD_BAN_REMOVE(json::obj data);

  void GUILD_EMOJIS_UPDATE(json::obj data);

  void CHANNEL_CREATE(json::obj data);
  void CHANNEL_UPDATE(json::obj data);
  void CHANNEL_DELETE(json::obj data);

  void CHANNEL_PINS_UPDATE(json::obj data);

  void MESSAGE_CREATE(json::obj data);
  void MESSAGE_UPDATE(json::obj data);
  void MESSAGE_DELETE(json::obj data);
  void MESSAGE_DELETE_BULK(json::obj data);

  void MESSAGE_REACTION_ADD(json::obj data);
  void MESSAGE_REACTION_REMOVE(json::obj data);

protected:
  void timerEvent(time::timerevent *event)
  {
    if (event->timerId() == heart)
    {
      socket->
          sendTextMessage
          (string
           (json::doc
            (json::obj {
               {"op", 1},
               {"d", seq}
             }).toJson()));
    }
  }
  void replyEvent(net::reply * event)
  {
    connect(event, &net::reply::finished, [event]() {
      if (event->bytesAvailable()) {
        qDebug().noquote()
            << json::doc::fromJson
               (event->readAll())
               .toJson(json::doc::Indented);
      } event->deleteLater();
    });
  }
};

#endif // DISCORD_H
