#include "application/Session.h"
#include "infrastructure/credentials/CredentialStore.h"
#include "domain/Records.h"
#include "domain/Text.h"
#include "presentation/ChatModel.h"
#include "presentation/FandomModel.h"
#include "presentation/Preferences.h"
#include "presentation/RoomListModel.h"
#include "presentation/Strings.h"
#include "presentation/ThemeController.h"
#include "presentation/VoiceController.h"
#include "presentation/RoomController.h"
#include "infrastructure/api/ApiClient.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>

using namespace katsodi;

namespace {
/** Records what the controller asks of the media engine. */
class FakeVoiceEngine final : public VoiceEngine {
public:
    using VoiceEngine::VoiceEngine;
    bool available() const override { return true; }
    void connectCall(quint64 generation, const QString &, const QString &) override { connects << generation; }
    void setMicrophone(bool on) override { microphone << on; emit microphoneChanged(on); }
    void setDeafened(bool on) override { deafened << on; }
    void disconnectCall() override { ++disconnects; }
    void refreshDevices() override {}
    void setInputDevice(const QString &) override {}
    void setOutputDevice(const QString &) override {}
    QList<quint64> connects;
    QList<bool> microphone, deafened;
    int disconnects = 0;
};

/** Minimal HTTP server: voice tokens, room snapshots and pings. */
class TokenServer : public QObject {
public:
    TokenServer()
    {
        m_server.listen(QHostAddress::LocalHost);
        connect(&m_server, &QTcpServer::newConnection, this, [this] {
            while (QTcpSocket *socket = m_server.nextPendingConnection()) {
                connect(socket, &QTcpSocket::readyRead, socket, [this, socket] {
                    const QByteArray request = socket->readAll();
                    if (!request.contains("/voice-token")) {
                        respond(socket, 200, request.contains("/occupancy") ? QByteArray("{}") : snapshot);
                        return;
                    }
                    ++requests;
                    respond(socket, status, status == 200
                        ? QByteArray(R"({"url":"wss://test.livekit.cloud","token":"t"})")
                        : QByteArray(R"({"error":{"code":"access_denied","messageKey":"native.errorAccess","retryable":false,"requestId":"x"}})"));
                });
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            }
        });
    }
    static void respond(QTcpSocket *socket, int code, const QByteArray &body)
    {
        socket->write("HTTP/1.1 " + QByteArray::number(code) + " X\r\nContent-Type: application/json\r\nContent-Length: "
                      + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
        socket->disconnectFromHost();
    }
    QUrl origin() const { return QUrl(QStringLiteral("http://127.0.0.1:%1").arg(m_server.serverPort())); }
    int requests = 0;
    int status = 200;
    QByteArray snapshot;

private:
    QTcpServer m_server;
};

const QString room1 = QStringLiteral("20000000-0000-4000-8000-000000000001");
const QString user1 = QStringLiteral("10000000-0000-4000-8000-000000000001");

QJsonObject roomJson()
{
    return QJsonObject{{"id", room1}, {"owner_id", user1}, {"name", "Вечер"}, {"description", ""},
                       {"format", "video"}, {"visibility", "public"}, {"url", "https://youtu.be/x"},
                       {"art", "orbit"}, {"closed_at", QJsonValue::Null}, {"created_at", "2026-11-12T10:00:00.123+00:00"},
                       {"fandom_id", QJsonValue::Null}};
}

MessageRecord message(int n, const QString &sender = user1)
{
    MessageRecord m;
    m.id = QStringLiteral("30000000-0000-4000-8000-%1").arg(n, 12, 10, QLatin1Char('0'));
    m.senderId = sender;
    m.body = QStringLiteral("m%1").arg(n);
    m.kind = QStringLiteral("text");
    m.createdAt = QDateTime(QDate(2026, 11, 12), QTime(10, n), QTimeZone::UTC);
    return m;
}

/** A snapshot in which the viewer is a joined member. */
QByteArray joinedSnapshot()
{
    return QJsonDocument(QJsonObject{{"room", roomJson()}, {"joined", true},
                                     {"members", QJsonArray{QJsonObject{{"user_id", user1}, {"role", "member"}, {"status", "active"}}}},
                                     {"profiles", QJsonArray{}}, {"messages", QJsonArray{}}})
        .toJson(QJsonDocument::Compact);
}

QStringList ids(const ChatModel &model)
{
    QStringList out;
    for (int i = 0; i < model.rowCount(); ++i)
        out << model.data(model.index(i), ChatModel::BodyRole).toString();
    return out;
}
} // namespace

class CoreTest : public QObject {
    Q_OBJECT
    QTemporaryDir m_settings;
private slots:
    void initTestCase()
    {
        // Preferences of the test go to a temporary INI file, not the registry.
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settings.path());
    }

    void searchMatchesTheWebsite()
    {
        QCOMPARE(searchText(QStringLiteral("  Ёлка   Café ")), QStringLiteral("елка cafe"));
        QVERIFY(matchesSearch(QStringLiteral("ведьмак ЁЖ"), {QStringLiteral("Ведьмак и ёж")}));
        QVERIFY(matchesSearch(QStringLiteral("star wars"), {QStringLiteral("Star"), QStringLiteral("Wars")}));
        QVERIFY(!matchesSearch(QStringLiteral("star trek"), {QStringLiteral("Star Wars")}));
        QVERIFY(matchesSearch(QString(), {QStringLiteral("anything")}));
    }

    void validatesIdsAndMessages()
    {
        QVERIFY(isUuid(room1));
        QVERIFY(isUuid(room1.toUpper()));
        QVERIFY(!isUuid(QStringLiteral("../admin")));
        QVERIFY(!isUuid(room1 + QStringLiteral("0")));
        QVERIFY(isValidMessage(QStringLiteral(" привет ")));
        QVERIFY(!isValidMessage(QStringLiteral("   ")));
        QVERIFY(isValidMessage(QString(2000, u'a')));
        QVERIFY(!isValidMessage(QString(2001, u'a')));
        QCOMPARE(initials(QStringLiteral("the last of us")), QStringLiteral("TL"));
        QCOMPARE(formatText(QStringLiteral("Hi {name}"), QStringLiteral("name"), QStringLiteral("Ann")),
                 QStringLiteral("Hi Ann"));
    }

    void parsesRoomsStrictly()
    {
        const auto room = parseRoom(roomJson());
        QVERIFY(room.has_value());
        QCOMPARE(room->name, QStringLiteral("Вечер"));
        QVERIFY(room->fandomId.isEmpty()); // optional fandom stays "none"
        QVERIFY(!room->closed);
        for (const auto &[key, value] : std::vector<std::pair<QString, QJsonValue>>{
                 {"format", "podcast"}, {"visibility", "secret"}, {"id", "x"}, {"art", 3},
                 {"created_at", "yesterday"}, {"name", ""}, {"fandom_id", 5}}) {
            QJsonObject bad = roomJson();
            bad.insert(key, value);
            QVERIFY2(!parseRoom(bad).has_value(), qPrintable(key));
        }
        QJsonObject closed = roomJson();
        closed.insert("closed_at", "2026-11-12T11:00:00Z");
        QVERIFY(parseRoom(closed)->closed);
        // PostgreSQL timestamps carry microseconds.
        const auto micro = parseTimestamp(QStringLiteral("2026-11-09T20:47:03.360928+00:00"));
        QVERIFY(micro.has_value());
        QCOMPARE(micro->time().msec(), 360);
        QVERIFY(!parseTimestamp(QStringLiteral("not a date")).has_value());
    }

    void chatAppliesDeltasPrecisely()
    {
        ChatModel model;
        model.replaceAll({message(1), message(2), message(3)});
        QCOMPARE(ids(model), (QStringList{"m1", "m2", "m3"}));

        QSignalSpy inserted(&model, &QAbstractItemModel::rowsInserted);
        QSignalSpy removed(&model, &QAbstractItemModel::rowsRemoved);
        QSignalSpy reset(&model, &QAbstractItemModel::modelReset);
        // m2 deleted, m4 new: one removal, one insertion, no reset.
        model.applyDelta({message(1).id, message(3).id, message(4).id}, {message(4)});
        QCOMPARE(ids(model), (QStringList{"m1", "m3", "m4"}));
        QCOMPARE(removed.count(), 1);
        QCOMPARE(inserted.count(), 1);
        QCOMPARE(reset.count(), 0);

        // A full snapshot replaces the window; unknown delta ids are skipped.
        model.applyDelta({message(9).id, message(4).id}, {});
        QCOMPARE(ids(model), (QStringList{"m4"}));
        model.replaceAll({message(5), message(6)});
        QCOMPARE(ids(model), (QStringList{"m5", "m6"}));
    }

    void chatGroupsAndNamesAuthors()
    {
        ChatModel model;
        model.setViewer(user1);
        const QString other = QStringLiteral("10000000-0000-4000-8000-000000000002");
        model.replaceAll({message(1), message(2), message(3, other)});
        model.setNames({{user1, QStringLiteral("Аня")}});
        QCOMPARE(model.data(model.index(0), ChatModel::AuthorRole).toString(), QStringLiteral("Аня"));
        QCOMPARE(model.data(model.index(1), ChatModel::GroupedRole).toBool(), true);
        QCOMPARE(model.data(model.index(2), ChatModel::GroupedRole).toBool(), false);
        QCOMPARE(model.data(model.index(0), ChatModel::OwnRole).toBool(), true);
        QCOMPARE(model.data(model.index(2), ChatModel::OwnRole).toBool(), false);
    }

    void roomListKeepsOnlyValidOpenRooms()
    {
        ApiClient api(QUrl(QStringLiteral("http://127.0.0.1:1")));
        RoomListModel model(&api);
        QJsonObject music = roomJson();
        music.insert("id", "20000000-0000-4000-8000-000000000002");
        music.insert("format", "music");
        music.insert("name", "Плейлист вечера");
        QJsonObject closed = roomJson();
        closed.insert("id", "20000000-0000-4000-8000-000000000003");
        closed.insert("closed_at", "2026-11-12T11:00:00Z");
        QJsonObject broken = roomJson();
        broken.insert("format", "podcast");
        model.apply(QJsonObject{{"rooms", QJsonArray{roomJson(), music, closed, broken}}});
        QCOMPARE(model.rowCount(), 2);
        model.setFormat(QStringLiteral("music"));
        QCOMPARE(model.rowCount(), 1);
        model.setFormat(QString());
        model.setQuery(QStringLiteral("плейлист"));
        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(model.data(model.index(0), RoomListModel::NameRole).toString(), QStringLiteral("Плейлист вечера"));
    }

    void bundledResourcesLoad()
    {
        Preferences preferences(QStringLiteral("Katsodi Test"));
        ThemeController theme(&preferences);
        QCOMPARE(theme.palettes().size(), 5);
        for (const QString &name : theme.palettes())
            QVERIFY2(theme.paletteColors(name).value("background").value<QColor>().isValid(), qPrintable(name));
        QVERIFY(theme.paletteColors("paper").value("background") != theme.paletteColors("obsidian").value("background"));
        QVERIFY(!theme.load(R"({"obsidian":{"colors":{}}})")); // incomplete palettes are refused

        Strings strings(&preferences);
        preferences.setLocale(QStringLiteral("ru"));
        QCOMPARE(strings.text(QStringLiteral("navHome")), QStringLiteral("Главная"));
        preferences.setLocale(QStringLiteral("en"));
        QCOMPARE(strings.text(QStringLiteral("navHome")), QStringLiteral("Home"));
        QCOMPARE(strings.format(QStringLiteral("accountSignedIn"), QStringLiteral("name"), QStringLiteral("Ann")),
                 QStringLiteral("Signed in as Ann."));
        QVERIFY(!strings.load(R"({"ru":{"a":"1"},"en":{"b":"2"}})"));

        FandomModel fandoms(QUrl(QStringLiteral("file:///art/")));
        QCOMPARE(fandoms.total(), 150);
        fandoms.setCategory(QStringLiteral("anime"));
        QCOMPARE(fandoms.rowCount(), 25);
        fandoms.setCategory(QString());
        fandoms.setQuery(QStringLiteral("star"));
        QVERIFY(fandoms.rowCount() >= 2);
        QCOMPARE(fandoms.nameOf(QStringLiteral("cinema-0")), QStringLiteral("Star Wars"));
    }

    void preferencesValidateInput()
    {
        Preferences preferences(QStringLiteral("Katsodi Test"));
        preferences.setTheme(QStringLiteral("ember"));
        QCOMPARE(preferences.theme(), QStringLiteral("ember"));
        preferences.setTheme(QStringLiteral("neon"));
        QCOMPARE(preferences.theme(), QStringLiteral("ember"));
        preferences.setResourceMode(QStringLiteral("low"));
        QVERIFY(preferences.lowResources());
        preferences.setResourceMode(QStringLiteral("turbo"));
        QCOMPARE(preferences.resourceMode(), QStringLiteral("low"));
        preferences.setResourceMode(QStringLiteral("standard"));
        QVERIFY(!preferences.lowResources());
        QVERIFY(Preferences::autoLow(2ull << 30, 8));
        QVERIFY(Preferences::autoLow(16ull << 30, 2));
        QVERIFY(!Preferences::autoLow(16ull << 30, 8));
        QVERIFY(!Preferences::autoLow(0, 0)); // unknown is not "weak"
    }

    void authUrlMustBeSafe()
    {
        QVERIFY(Session::acceptableAuthUrl(QUrl("https://project.supabase.co"), false));
        QVERIFY(!Session::acceptableAuthUrl(QUrl("http://project.supabase.co"), false));
        QVERIFY(!Session::acceptableAuthUrl(QUrl("http://127.0.0.1:54321"), false));
        QVERIFY(Session::acceptableAuthUrl(QUrl("http://127.0.0.1:54321"), true));
        QVERIFY(!Session::acceptableAuthUrl(QUrl("https://user:pw@project.supabase.co"), false));
        QVERIFY(!Session::acceptableAuthUrl(QUrl("https://project.supabase.co/?x=1"), false));
    }

    void voiceFollowsTheWebsiteRules()
    {
        TokenServer server;
        ApiClient api(server.origin());
        Session session(&api, CredentialStore(QStringLiteral("Katsodi Test/session")));
        Preferences preferences(QStringLiteral("Katsodi Test"));
        RoomController room(&api, &session, &preferences);
        FakeVoiceEngine engine;
        VoiceController voice(&api, &room, &engine);

        // Not in the room yet: no token is requested.
        voice.join();
        QCOMPARE(voice.errorKey(), QStringLiteral("voiceJoinRoom"));
        QCOMPARE(server.requests, 0);

        server.snapshot = joinedSnapshot();
        room.open(room1);
        QTRY_VERIFY(room.joined());
        voice.join();
        QCOMPARE(voice.state(), QStringLiteral("joining"));
        QTRY_COMPARE(engine.connects.size(), 1);
        QCOMPARE(server.requests, 1);
        emit engine.connected(engine.connects.last());
        QCOMPARE(voice.state(), QStringLiteral("connected"));
        QVERIFY(!voice.microphone()); // listen first, as on the website

        voice.toggleMicrophone();
        QVERIFY(voice.microphone());
        voice.toggleDeafen(); // deafen also turns the microphone off...
        QVERIFY(voice.deafened());
        QCOMPARE(engine.microphone, (QList<bool>{true, false}));
        QCOMPARE(engine.deafened, (QList<bool>{true}));
        voice.toggleMicrophone(); // ...and it cannot be turned on while deafened
        QCOMPARE(engine.microphone.size(), 2);
        voice.toggleDeafen(); // ...and comes back afterwards
        QCOMPARE(engine.microphone, (QList<bool>{true, false, true}));
        QVERIFY(!voice.deafened());

        // A network drop: retried with a fresh token after 2 s, microphone off.
        emit engine.ended(engine.connects.last(), VoiceEngine::Ended::Network);
        QCOMPARE(voice.state(), QStringLiteral("reconnecting"));
        QVERIFY(!voice.microphone());
        QTRY_COMPARE_WITH_TIMEOUT(engine.connects.size(), 2, 4000);
        QCOMPARE(server.requests, 2);
        emit engine.connected(engine.connects.first()); // stale attempt: ignored
        QCOMPARE(voice.state(), QStringLiteral("reconnecting"));
        emit engine.connected(engine.connects.last());
        QCOMPARE(voice.state(), QStringLiteral("connected"));

        // Removed by the server: no retry.
        emit engine.ended(engine.connects.last(), VoiceEngine::Ended::Removed);
        QCOMPARE(voice.state(), QStringLiteral("idle"));
        QCOMPARE(voice.errorKey(), QStringLiteral("voiceDisconnected"));
        QTest::qWait(2500);
        QCOMPARE(engine.connects.size(), 2);

        // A refused token stops at once and never reaches the engine.
        server.status = 403;
        voice.join();
        QTRY_COMPARE(voice.state(), QStringLiteral("idle"));
        QCOMPARE(voice.errorKey(), QStringLiteral("voiceAccess"));
        QCOMPARE(engine.connects.size(), 2);

        // Leaving the room ends the call.
        server.status = 200;
        voice.join();
        QTRY_COMPARE(engine.connects.size(), 3);
        emit engine.connected(engine.connects.last());
        const int before = engine.disconnects;
        room.close();
        QCOMPARE(voice.state(), QStringLiteral("idle"));
        QVERIFY(engine.disconnects > before);
    }

    void apiRefusesForeignPaths()
    {
        ApiClient api(QUrl(QStringLiteral("http://127.0.0.1:1")));
        QObject context;
        int calls = 0;
        for (const QString &path : {QStringLiteral("https://evil.example/api/native/v1/x"),
                                    QStringLiteral("/api/rooms/1"), QStringLiteral("/api/native/v1/../../admin"),
                                    QStringLiteral("/api/native/v1/rooms\\x"), QStringLiteral("/api/native/v1/a b")}) {
            api.getPublic(path, &context, [&](const QJsonObject &, const ApiError *error) {
                ++calls;
                QVERIFY(error);
                QCOMPARE(error->code, QStringLiteral("invalid_request"));
            });
        }
        QCOMPARE(calls, 5);
    }
};

QTEST_MAIN(CoreTest)
#include "tst_core.moc"
