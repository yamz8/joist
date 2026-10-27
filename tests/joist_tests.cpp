#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include "backend.h"
#include "filepicker.h"
#include "labels.h"
#include "notifier.h"
#include "pdfexport.h"
#include "project.h"
#include "theme.h"
#include "translator.h"

// Hands back whatever file the test names, instead of asking the portal.
class FakePicker : public FilePicker {
public:
    QString open, save;
    void openPdf(const QString &) override { emit openSelected(QUrl::fromLocalFile(open)); }
    void savePdf(const QUrl &, const QString &) override { emit saveSelected(QUrl::fromLocalFile(save)); }
};

class JoistTests : public QObject {
    Q_OBJECT

private:
    static void write(const QString &path, const QByteArray &contents)
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(contents);
    }

    static QString script(const QTemporaryDir &dir, const QString &name, const QByteArray &body)
    {
        const QString path = dir.filePath(name);
        write(path, "#!/bin/sh\n" + body);
        QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
        return path;
    }

    const QDate today{2026, 10, 24};

private slots:
    void codesParse()
    {
        QString type;
        int number = 0;
        QVERIFY(parseCode("RFI-007", &type, &number));
        QCOMPARE(type, QString("rfi"));
        QCOMPARE(number, 7);
        QVERIFY(parseCode("sub 12", &type, &number));
        QCOMPARE(type, QString("submittal"));
        QCOMPARE(number, 12);
        type = "rfi";
        QVERIFY(parseCode("3", &type, &number));
        QCOMPARE(number, 3);
        QVERIFY(!parseCode("A-101", &type, &number));
    }

    void projectsRoundTrip()
    {
        QTemporaryDir dir;
        Project project;
        project.name = "Harbor Street Clinic";
        project.number = "2417";
        project.parties = {"Contractor", "Architect"};
        Item item;
        item.type = "rfi";
        item.number = 3;
        item.title = "Slab edge";
        item.created = today;
        item.due = today.addDays(7);
        item.ballInCourt = "Architect";
        item.history.append(Event{today, "Contractor", "Architect", "note", "submitted"});
        item.sheets.append(SheetLink{"/plans/set.pdf", "A-101"});
        project.items.append(item);

        const QString path = ProjectStore::pathFor(dir.path(), project);
        QCOMPARE(QFileInfo(path).fileName(), QString("2417-harbor-street-clinic.json"));
        QString error;
        QVERIFY(ProjectStore::save(path, project, &error));
        Project loaded;
        QVERIFY(ProjectStore::load(path, loaded, &error));
        QCOMPARE(loaded.toJson(), project.toJson());
        QCOMPARE(loaded.items.first().code(), QString("RFI-003"));
        QCOMPARE(ProjectStore::find(dir.path(), "2417"), path);
        QCOMPARE(ProjectStore::find(dir.path(), "harbor street clinic"), path);
    }

    void itemsTravelBetweenParties()
    {
        QTemporaryDir dir;
        Backend backend(nullptr, dir.path());
        backend.setToday(today);
        QVERIFY(backend.createProject("Clinic", "2417"));
        QCOMPARE(backend.parties(), QStringList({"Contractor", "Architect", "Owner"}));

        const QString code = backend.addItem("rfi", "Slab edge");
        QCOMPARE(code, QString("RFI-001"));
        QVariantMap item = backend.item(code);
        QCOMPARE(item["ballInCourt"].toString(), QString("Architect"));
        QCOMPARE(item["due"].toString(), today.addDays(7).toString(Qt::ISODate));
        QCOMPARE(backend.suggestedRecipient(code), QString("Contractor"));

        backend.passTo(code, "Owner", 3, "Owner's call");
        item = backend.item(code);
        QCOMPARE(item["ballInCourt"].toString(), QString("Owner"));
        QCOMPARE(item["daysLeft"].toInt(), 3);
        QCOMPARE(backend.suggestedRecipient(code), QString("Architect"));

        backend.closeItem(code, "answered", "Use 5/S-302.", "");
        item = backend.item(code);
        QVERIFY(!item["open"].toBool());
        QCOMPARE(item["answer"].toString(), QString("Use 5/S-302."));
        QCOMPARE(item["ballInCourt"].toString(), QString());

        backend.reopen(code);
        item = backend.item(code);
        QVERIFY(item["open"].toBool());
        QCOMPARE(item["ballInCourt"].toString(), QString("Owner"));
        QCOMPARE(item["history"].toList().size(), 4);
    }

    void resubmittalsComeBackAsTheNextRevision()
    {
        QTemporaryDir dir;
        Backend backend(nullptr, dir.path());
        backend.setToday(today);
        QVERIFY(backend.createProject("Clinic", ""));
        const QString code = backend.addItem("submittal", "Rebar");
        QCOMPARE(code, QString("SUB-001"));
        backend.closeItem(code, "revise-resubmit", "Laps", "");
        QVariantMap item = backend.item(code);
        QVERIFY(item["open"].toBool());
        QCOMPARE(item["revision"].toInt(), 1);
        QCOMPARE(item["ballInCourt"].toString(), QString("Contractor"));
        QCOMPARE(item["due"].toString(), today.addDays(14).toString(Qt::ISODate));
        // An RFI can't be approved.
        const QString rfi = backend.addItem("rfi");
        backend.closeItem(rfi, "approved", "", "");
        QVERIFY(backend.item(rfi)["open"].toBool());
    }

    void overdueCounts()
    {
        QTemporaryDir dir;
        Backend backend(nullptr, dir.path());
        backend.setToday(today);
        QVERIFY(backend.createProject("Clinic", ""));
        const QString code = backend.addItem("rfi", "Late", {}, {}, 2);
        backend.setToday(today.addDays(5));
        QVERIFY(backend.item(code)["overdue"].toBool());
        QCOMPARE(backend.item(code)["daysLeft"].toInt(), -3);
        QCOMPARE(backend.projects().first().toMap()["overdue"].toInt(), 1);
        const QVariantList board = backend.board();
        QCOMPARE(board.at(1).toMap()["party"].toString(), QString("Architect"));
        QCOMPARE(board.at(1).toMap()["overdue"].toInt(), 1);
    }

    void changesSaveAndUndo()
    {
        QTemporaryDir dir;
        Backend backend(nullptr, dir.path());
        backend.setToday(today);
        QVERIFY(backend.createProject("Clinic", ""));
        const QString code = backend.addItem("rfi", "First");
        backend.setField(code, "title", "Second");
        QVERIFY(backend.canUndo());

        Project saved;
        QVERIFY(ProjectStore::load(backend.projectFile(), saved, nullptr));
        QCOMPARE(saved.items.first().title, QString("Second"));

        backend.undo();
        QCOMPARE(backend.item(code)["title"].toString(), QString("First"));
        backend.redo();
        QCOMPARE(backend.item(code)["title"].toString(), QString("Second"));
        backend.undo();
        backend.undo();
        QVERIFY(backend.item(code).isEmpty());
        QVERIFY(ProjectStore::load(backend.projectFile(), saved, nullptr));
        QVERIFY(saved.items.isEmpty());
    }

    void partiesRenameThroughHistory()
    {
        QTemporaryDir dir;
        Backend backend(nullptr, dir.path());
        backend.setToday(today);
        QVERIFY(backend.createProject("Clinic", ""));
        const QString code = backend.addItem("rfi", "Slab");
        QVERIFY(backend.renameParty("Architect", "Studio North"));
        QCOMPARE(backend.item(code)["ballInCourt"].toString(), QString("Studio North"));
        QCOMPARE(backend.item(code)["history"].toList().first().toMap()["to"].toString(), QString("Studio North"));
        // Someone holding an item can't leave the team.
        QVERIFY(!backend.removeParty("Studio North"));
        QVERIFY(!backend.error().isEmpty());
        QVERIFY(backend.removeParty("Owner"));
    }

    void outsideChangesReload()
    {
        QTemporaryDir dir;
        Backend backend(nullptr, dir.path());
        backend.setToday(today);
        QVERIFY(backend.createProject("Clinic", ""));
        backend.addItem("rfi", "Mine");

        // What `joist add` from a script does.
        Backend other(nullptr, dir.path());
        QVERIFY(other.openProject(backend.projectFile(), false));
        other.addItem("rfi", "Theirs");

        QTRY_COMPARE(backend.items().size(), 2);
    }

    void sheetsComeFromLintel()
    {
        QTemporaryDir dir;
        const QString lintel = script(dir, "lintel",
            "echo '{\"sheets\":[{\"number\":1,\"label\":\"A-101\"},{\"number\":2,\"label\":\"\"}]}'\n");
        write(dir.filePath("set.pdf"), "%PDF");
        FakePicker picker;
        picker.open = dir.filePath("set.pdf");
        Backend backend(&picker, dir.filePath("projects"));
        backend.setLintelCommand(lintel);
        QVERIFY(backend.createProject("Clinic", ""));
        const QString code = backend.addItem("rfi");

        QSignalSpy listed(&backend, &Backend::sheetsListed);
        backend.linkSheet(code);
        QVERIFY(listed.wait());
        QCOMPARE(listed.first().at(2).toStringList(), QStringList({"A-101", "2"}));

        backend.addSheetLink(code, picker.open, "A-101");
        backend.addSheetLink(code, picker.open, "A-101");
        QCOMPARE(backend.item(code)["sheets"].toList().size(), 1);
    }

    void missingLintelIsReported()
    {
        QTemporaryDir dir;
        write(dir.filePath("set.pdf"), "%PDF");
        FakePicker picker;
        picker.open = dir.filePath("set.pdf");
        Backend backend(&picker, dir.filePath("projects"));
        backend.setLintelCommand(dir.filePath("no-such-lintel"));
        QVERIFY(backend.createProject("Clinic", ""));
        backend.linkSheet(backend.addItem("rfi"));
        QTRY_VERIFY(backend.error().contains("Lintel"));
    }

    void exportsWritePdfs()
    {
        QTemporaryDir dir;
        FakePicker picker;
        Backend backend(&picker, dir.filePath("projects"));
        backend.setToday(today);
        QVERIFY(backend.createProject("Clinic", "2417"));
        for (int i = 0; i < 80; ++i)
            backend.addItem(i % 2 ? "rfi" : "submittal", QString("Item %1 with a long enough subject to wrap in the log").arg(i));
        backend.setField("RFI-001", "question", QString("A long question. ").repeated(400));

        for (const QString kind : {"rfi", "submittal", "board", "RFI-001"}) {
            picker.save = dir.filePath(kind + ".pdf");
            QSignalSpy exported(&backend, &Backend::exported);
            if (kind.startsWith("RFI-"))
                backend.exportForm(kind);
            else
                backend.exportLog(kind);
            QCOMPARE(exported.size(), 1);
            QFile pdf(picker.save);
            QVERIFY(pdf.open(QIODevice::ReadOnly));
            QVERIFY(pdf.read(5) == "%PDF-");
        }
        QCOMPARE(PdfExport::suggestedName(backend.project(), "rfi", today), QString("2417 RFI log 2026-10-24.pdf"));
    }

    void notificationsGoOutOncePerDay()
    {
        QTemporaryDir dir;
        const QString log = dir.filePath("sent.log");
        const QString command = script(dir, "notify-send", "echo \"$@\" >> '" + log.toUtf8() + "'\n");

        Backend backend(nullptr, dir.filePath("projects"));
        backend.setToday(today);
        QVERIFY(backend.createProject("Clinic", ""));
        backend.addItem("rfi", "Late one", {}, {}, 0);
        backend.addItem("rfi", "Fine", {}, {}, 9);
        const QDate later = today.addDays(2);

        const auto overdue = Notifier::overdueItems(ProjectStore::files(dir.filePath("projects")), later);
        QCOMPARE(overdue.size(), 1);
        QCOMPARE(overdue.first().daysLate, 2);

        Notifier notifier(dir.filePath("state.json"), command);
        QString error;
        QVERIFY(notifier.notify(overdue, later, &error));
        QVERIFY(notifier.pending(overdue, later).isEmpty());
        QVERIFY(notifier.notify(overdue, later, &error));
        QFile sent(log);
        QVERIFY(sent.open(QIODevice::ReadOnly));
        const QString text = QString::fromUtf8(sent.readAll());
        QCOMPARE(text.count("RFI-001"), 1);
        QVERIFY(text.contains("2 days overdue"));
        // A new day announces it again.
        QCOMPARE(notifier.pending(overdue, later.addDays(1)).size(), 1);

        Notifier broken(dir.filePath("state2.json"), dir.filePath("missing-notify-send"));
        QVERIFY(!broken.notify(overdue, later, &error));
        QVERIFY(error.contains("not installed"));
    }

    void themeFallsBack()
    {
        QTemporaryDir dir;
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#FFD60A"));
        QCOMPARE(theme.accentForeground(), QString("black"));
    }

    void themeReadsAccent()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        write(dir.path() + "/theme/colors.toml", "accent = \"#112233\"\n");
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#112233"));
        QCOMPARE(theme.accentForeground(), QString("white"));
    }

    void translatorFollowsTheLanguageFile()
    {
        QTemporaryDir dir;
        write(dir.path() + "/language", "pt-BR\n");
        write(dir.path() + "/pt.json", R"({"Save": "Guardar", "Open": "Abrir"})");
        write(dir.path() + "/pt-BR.json", R"({"Save": "Salvar"})");
        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("pt-BR"));
        QCOMPARE(translator.translate("Main", "Save"), QString("Salvar"));
        QCOMPARE(translator.translate("Main", "Open"), QString("Abrir"));
        QVERIFY(translator.translate("Main", "Quit").isNull());
    }

    void translatorTellsMeaningsApart()
    {
        QTemporaryDir dir;
        write(dir.path() + "/language", "da\n");
        write(dir.path() + "/da.json", R"({"Open": "Åbn", "Open|status": "Åben"})");
        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.translate("Labels", "Open", "status"), QString("Åben"));
        QCOMPARE(translator.translate("ItemPanel", "Open"), QString("Åbn"));
        // A meaning the catalog doesn't separate falls back to the word.
        QCOMPARE(translator.translate("Labels", "Open", "verb"), QString("Åbn"));
    }

    void statusesReadInEnglishWithoutCatalogs()
    {
        Item item;
        item.due = today.addDays(1);
        QCOMPARE(Labels::status(item, today), QString("Open"));
        QCOMPARE(Labels::status(item, today.addDays(2)), QString("Overdue"));
        item.closed = today;
        item.action = "approved-as-noted";
        QCOMPARE(Labels::status(item, today), QString("Approved as noted"));
    }

    void bundledCatalogsAreValid()
    {
        // Every shipped catalog parses, keeps each placeholder its English
        // has, and gives every plural form the language's counts can need.
        const QDir catalogs(QStringLiteral(SRCDIR) + "/i18n");
        const QStringList files = catalogs.entryList({"*.json"});
        QCOMPARE(files.size(), 50);
        for (const QString &name : files) {
            const QString language = name.chopped(5);
            QFile file(catalogs.filePath(name));
            QVERIFY(file.open(QIODevice::ReadOnly));
            QJsonParseError parse;
            const QJsonObject object = QJsonDocument::fromJson(file.readAll(), &parse).object();
            QVERIFY2(parse.error == QJsonParseError::NoError, qPrintable(name + ": " + parse.errorString()));

            QSet<QString> categories;
            for (int n = 0; n <= 2000; ++n)
                categories.insert(Translator::pluralCategory(language, n));

            for (auto it = object.begin(); it != object.end(); ++it) {
                const QString key = it.key().section('|', 0, 0);
                const auto placeholders = [&](const QString &text, const QString &where) {
                    for (const QString &marker : {"%1", "%2", "%3", "%n"})
                        QVERIFY2(key.contains(marker) == text.contains(marker),
                                 qPrintable(name + ": " + where + " " + it.key() + " → " + text));
                };
                if (it.value().isString()) {
                    placeholders(it.value().toString(), "string");
                    continue;
                }
                QVERIFY2(it.value().isObject(), qPrintable(name + ": " + it.key()));
                const QJsonObject forms = it.value().toObject();
                for (const QString &category : categories)
                    QVERIFY2(!forms.value(category).toString().isEmpty(),
                             qPrintable(name + ": " + it.key() + " lacks " + category));
                for (auto form = forms.begin(); form != forms.end(); ++form)
                    placeholders(form.value().toString(), form.key());
            }
        }
    }

    void everyCatalogIsCompiledIn()
    {
        const QStringList shipped = QDir(QStringLiteral(SRCDIR) + "/i18n").entryList({"*.json"});
        const QStringList compiled = QDir(":/i18n").entryList({"*.json"});
        QCOMPARE(compiled, shipped);
    }

    void launcherCommentsMatchTheCatalogs()
    {
        // bin/desktop-comments writes these; this catches a catalog changed
        // without rerunning it.
        QFile desktop(QStringLiteral(SRCDIR) + "/../pkgbuild/joist.desktop");
        QVERIFY(desktop.open(QIODevice::ReadOnly));
        QHash<QString, QString> comments;
        for (const QString &line : QString::fromUtf8(desktop.readAll()).split('\n'))
            if (line.startsWith("Comment["))
                comments.insert(line.section('[', 1).section(']', 0, 0), line.section('=', 1));

        const QDir catalogs(QStringLiteral(SRCDIR) + "/i18n");
        for (const QString &name : catalogs.entryList({"*.json"})) {
            const QString code = name.chopped(5);
            if (code == "en")
                continue;
            QFile file(catalogs.filePath(name));
            QVERIFY(file.open(QIODevice::ReadOnly));
            QString text = QJsonDocument::fromJson(file.readAll()).object()
                               .value("RFI and submittal logs for construction projects.").toString().trimmed();
            text.remove(QRegularExpression("[.。।۔]\\s*$"));
            const QString key = code == "no" ? QString("nb") : QString(code).replace('-', '_');
            QVERIFY2(comments.value(key) == text, qPrintable(key + ": run bin/desktop-comments"));
        }
    }
};

QTEST_MAIN(JoistTests)
#include "joist_tests.moc"
