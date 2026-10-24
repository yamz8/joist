#include "cli.h"

#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <cstdio>

#include "backend.h"
#include "notifier.h"
#include "pdfexport.h"
#include "project.h"

namespace {

void print(FILE *stream, const QString &text) { fprintf(stream, "%s\n", qPrintable(text)); }
void print(const QJsonValue &value) {
    fputs((value.isArray() ? QJsonDocument(value.toArray()) : QJsonDocument(value.toObject())).toJson().constData(),
          stdout);
}

int fail(const QString &message) {
    print(stderr, message);
    return 1;
}

struct Command {
    QCommandLineParser parser;
    Command(const QString &name, const QString &description, const QStringList &arguments = {}) {
        parser.setApplicationDescription(description);
        parser.addHelpOption();
        parser.addPositionalArgument(name, description, name);
        for (const QString &argument : arguments)
            parser.addPositionalArgument(argument, argument, "<" + argument + ">");
    }
    void json() { parser.addOption({"json", "Print the result as JSON"}); }
    void project() {
        parser.addOption({{"p", "project"}, "The project, by name, number, or file; needed when there are several",
                          "project"});
    }
    bool wantsJson() const { return parser.isSet("json"); }
    QString argument(int index) const { return parser.positionalArguments().value(index + 1); }
};

// The named project, or the only one there is, opened in the backend.
bool openProject(Backend &backend, const Command &command) {
    const QString directory = ProjectStore::defaultDirectory();
    const QString name = command.parser.value("project");
    QString path;
    if (!name.isEmpty()) {
        path = ProjectStore::find(directory, name);
        if (path.isEmpty())
            return fail(QString("No single project matches \"%1\". joist projects lists them.").arg(name)), false;
    } else {
        const QStringList files = ProjectStore::files(directory);
        if (files.isEmpty())
            return fail("There are no projects yet. Make one with: joist new <name>"), false;
        if (files.size() > 1)
            return fail("There are several projects; name one with --project. joist projects lists them."), false;
        path = files.first();
    }
    if (!backend.openProject(path, false))
        return fail(backend.error()), false;
    return true;
}

// The party as the team spells it, or empty with an error.
QString party(const Backend &backend, const QString &name) {
    for (const QString &party : backend.parties())
        if (party.compare(name, Qt::CaseInsensitive) == 0)
            return party;
    fail(QString("%1 isn't on the team: %2.").arg(name, backend.parties().join(", ")));
    return {};
}

QString code(const Backend &backend, const QString &code) {
    const int index = backend.project().indexOf(code);
    if (index < 0) {
        fail(QString("There is no %1. Write RFI-3 or SUB-12.").arg(code));
        return {};
    }
    return backend.project().items[index].code();
}

QJsonObject itemJson(const Item &item, const QDate &today) {
    QJsonObject object = item.toJson();
    object.insert("code", item.code());
    object.insert("open", item.isOpen());
    object.insert("overdue", item.isOverdue(today));
    if (item.isOpen() && item.due.isValid())
        object.insert("daysLeft", int(today.daysTo(item.due)));
    return object;
}

QString itemLine(const Item &item, const QDate &today) {
    QString state;
    if (!item.isOpen())
        state = "closed, " + item.action;
    else if (item.isOverdue(today))
        state = QString("%1, %2 days overdue").arg(item.ballInCourt).arg(item.due.daysTo(today));
    else
        state = QString("%1, due %2").arg(item.ballInCourt, item.due.toString(Qt::ISODate));
    return QString("%1\t%2\t%3").arg(item.code(), state, item.title);
}

int printItem(const Backend &backend, const QString &itemCode, bool json) {
    const Item &item = backend.project().items[backend.project().indexOf(itemCode)];
    if (json)
        print(itemJson(item, backend.today()));
    else
        print(stdout, itemLine(item, backend.today()));
    return 0;
}

int projects(const QStringList &arguments) {
    Command command("projects", "List the projects.");
    command.json();
    command.parser.process(arguments);
    Backend backend(nullptr);
    if (command.wantsJson()) {
        print(QJsonArray::fromVariantList(backend.projects()));
        return 0;
    }
    for (const QVariant &value : backend.projects()) {
        const QVariantMap project = value.toMap();
        print(stdout, QString("%1\t%2\t%3 open, %4 overdue\t%5")
                          .arg(project["number"].toString(), project["name"].toString())
                          .arg(project["open"].toInt()).arg(project["overdue"].toInt())
                          .arg(project["file"].toString()));
    }
    return 0;
}

int create(const QStringList &arguments) {
    Command command("new", "Start a project.", {"name"});
    command.parser.addOption({{"n", "number"}, "The project number", "number"});
    command.parser.addOption({"party", "A party on the team, in order: who raises items first, who answers second. "
                                       "Repeat for each; Contractor, Architect, Owner by default", "party"});
    command.json();
    command.parser.process(arguments);
    Backend backend(nullptr);
    if (!backend.createProject(command.argument(0), command.parser.value("number")))
        return fail(backend.error());
    const QStringList parties = command.parser.values("party");
    if (!parties.isEmpty()) {
        for (const QString &party : backend.parties())
            backend.removeParty(party);
        for (const QString &party : parties)
            backend.addParty(party);
    }
    if (command.wantsJson())
        print(QJsonObject{{"file", backend.projectFile()}, {"name", backend.projectName()},
                          {"parties", QJsonArray::fromStringList(backend.parties())}});
    else
        print(stdout, backend.projectFile());
    return 0;
}

int list(const QStringList &arguments) {
    Command command("list", "List a project's RFIs and submittals.");
    command.project();
    command.parser.addOption({{"t", "type"}, "Only rfi or submittal", "type"});
    command.parser.addOption({"open", "Only open items"});
    command.parser.addOption({"overdue", "Only overdue items"});
    command.parser.addOption({"holder", "Only items this party holds", "party"});
    command.json();
    command.parser.process(arguments);
    Backend backend(nullptr);
    if (!openProject(backend, command))
        return 1;
    const QString type = command.parser.value("type");
    if (!type.isEmpty() && !isItemType(type))
        return fail("The type is rfi or submittal.");
    QJsonArray json;
    for (const Item &item : backend.project().items) {
        if ((!type.isEmpty() && item.type != type) || (command.parser.isSet("open") && !item.isOpen())
            || (command.parser.isSet("overdue") && !item.isOverdue(backend.today()))
            || (command.parser.isSet("holder")
                && item.ballInCourt.compare(command.parser.value("holder"), Qt::CaseInsensitive) != 0))
            continue;
        if (command.wantsJson())
            json.append(itemJson(item, backend.today()));
        else
            print(stdout, itemLine(item, backend.today()));
    }
    if (command.wantsJson())
        print(QJsonObject{{"project", backend.projectName()}, {"items", json}});
    return 0;
}

int show(const QStringList &arguments) {
    Command command("show", "Show one item, with its history and linked sheets.", {"item"});
    command.project();
    command.json();
    command.parser.process(arguments);
    Backend backend(nullptr);
    if (!openProject(backend, command))
        return 1;
    const QString item = code(backend, command.argument(0));
    if (item.isEmpty())
        return 1;
    if (command.wantsJson())
        return printItem(backend, item, true);
    const Item &found = backend.project().items[backend.project().indexOf(item)];
    print(stdout, itemLine(found, backend.today()));
    if (!found.spec.isEmpty())
        print(stdout, "Spec: " + found.spec);
    if (!found.question.isEmpty())
        print(stdout, "\n" + found.question);
    if (!found.answer.isEmpty())
        print(stdout, "\nAnswer: " + found.answer);
    for (const SheetLink &link : found.sheets)
        print(stdout, QString("Sheet: %1 in %2").arg(link.sheet, link.file));
    for (const Event &event : found.history)
        print(stdout, QString("%1\t%2 -> %3\t%4").arg(event.date.toString(Qt::ISODate), event.from, event.to,
                                                      QStringList{event.action, event.note}.join(' ').trimmed()));
    return 0;
}

int add(const QStringList &arguments) {
    Command command("add", "Log a new RFI or submittal.", {"type", "title"});
    command.project();
    command.parser.addOption({{"q", "question"}, "The RFI's question, or what the submittal covers", "text"});
    command.parser.addOption({"spec", "Specification section", "section"});
    command.parser.addOption({"from", "Who raises it; the team's first party by default", "party"});
    command.parser.addOption({"to", "Who holds it first; the team's second party by default", "party"});
    command.parser.addOption({"due-in", "Days to answer; the project's usual days by default", "days"});
    command.json();
    command.parser.process(arguments);
    const QString type = command.argument(0);
    if (!isItemType(type))
        return fail("The type is rfi or submittal.");
    if (command.argument(1).trimmed().isEmpty())
        return fail("Give it a title.");
    Backend backend(nullptr);
    if (!openProject(backend, command))
        return 1;
    QString from, to;
    if (command.parser.isSet("from") && (from = party(backend, command.parser.value("from"))).isEmpty())
        return 1;
    if (command.parser.isSet("to") && (to = party(backend, command.parser.value("to"))).isEmpty())
        return 1;
    const int days = command.parser.isSet("due-in") ? command.parser.value("due-in").toInt() : -1;
    const QString item = backend.addItem(type, command.argument(1), from, to, days);
    if (item.isEmpty())
        return fail(backend.error().isEmpty() ? "Could not add it." : backend.error());
    if (command.parser.isSet("question"))
        backend.setField(item, "question", command.parser.value("question"));
    if (command.parser.isSet("spec"))
        backend.setField(item, "spec", command.parser.value("spec"));
    return printItem(backend, item, command.wantsJson());
}

int pass(const QStringList &arguments) {
    Command command("pass", "Pass an item to another party: the ball is in their court.", {"item", "party"});
    command.project();
    command.parser.addOption({"due-in", "Days they have; the project's usual days by default", "days"});
    command.parser.addOption({"note", "Why, for the history", "text"});
    command.json();
    command.parser.process(arguments);
    Backend backend(nullptr);
    if (!openProject(backend, command))
        return 1;
    const QString item = code(backend, command.argument(0));
    const QString to = item.isEmpty() ? QString() : party(backend, command.argument(1));
    if (item.isEmpty() || to.isEmpty())
        return 1;
    if (!backend.item(item)["open"].toBool())
        return fail(item + " is closed. Reopen it first with: joist reopen " + item);
    const QString type = backend.item(item)["type"].toString();
    backend.passTo(item, to,
                   command.parser.isSet("due-in") ? command.parser.value("due-in").toInt() : backend.defaultDays(type),
                   command.parser.value("note"));
    if (!backend.error().isEmpty())
        return fail(backend.error());
    return printItem(backend, item, command.wantsJson());
}

int close(const QStringList &arguments) {
    Command command("close", "Close an item: answered or withdrawn for an RFI; approved, approved-as-noted, "
                             "rejected, or withdrawn for a submittal. revise-resubmit sends a submittal back as its "
                             "next revision.", {"item", "action"});
    command.project();
    command.parser.addOption({"answer", "The answer, or the review comments", "text"});
    command.parser.addOption({"note", "A note for the history", "text"});
    command.json();
    command.parser.process(arguments);
    Backend backend(nullptr);
    if (!openProject(backend, command))
        return 1;
    const QString item = code(backend, command.argument(0));
    if (item.isEmpty())
        return 1;
    const QVariantMap found = backend.item(item);
    const QStringList actions = closingActions(found["type"].toString());
    if (!actions.contains(command.argument(1)))
        return fail(QString("Close %1 as one of: %2.").arg(item, actions.join(", ")));
    if (!found["open"].toBool())
        return fail(item + " is already closed.");
    backend.closeItem(item, command.argument(1),
                      command.parser.isSet("answer") ? command.parser.value("answer") : found["answer"].toString(),
                      command.parser.value("note"));
    if (!backend.error().isEmpty())
        return fail(backend.error());
    return printItem(backend, item, command.wantsJson());
}

int reopen(const QStringList &arguments) {
    Command command("reopen", "Reopen a closed item, back with whoever closed it.", {"item"});
    command.project();
    command.json();
    command.parser.process(arguments);
    Backend backend(nullptr);
    if (!openProject(backend, command))
        return 1;
    const QString item = code(backend, command.argument(0));
    if (item.isEmpty())
        return 1;
    if (backend.item(item)["open"].toBool())
        return fail(item + " is open.");
    backend.reopen(item);
    return printItem(backend, item, command.wantsJson());
}

int link(const QStringList &arguments) {
    Command command("link", "Link an item to a sheet in a Lintel drawing set.", {"item", "set.pdf", "sheet"});
    command.project();
    command.json();
    command.parser.process(arguments);
    Backend backend(nullptr);
    if (!openProject(backend, command))
        return 1;
    const QString item = code(backend, command.argument(0));
    if (item.isEmpty())
        return 1;
    const QFileInfo set(command.argument(1));
    if (!set.exists())
        return fail(QString("%1 isn't there.").arg(command.argument(1)));
    if (command.argument(2).isEmpty())
        return fail("Name the sheet, such as A-101. lintel sheets set.pdf lists them.");
    backend.addSheetLink(item, set.absoluteFilePath(), command.argument(2));
    if (!backend.error().isEmpty())
        return fail(backend.error());
    return printItem(backend, item, command.wantsJson());
}

int exportPdf(const QStringList &arguments) {
    Command command("export", "Export a log (rfi, submittal), the ball-in-court report (board), or one item's form "
                              "(RFI-3) as PDF.", {"what"});
    command.project();
    command.parser.addOption({{"o", "output"}, "The PDF to write", "file.pdf"});
    command.json();
    command.parser.process(arguments);
    const QString output = command.parser.value("output");
    if (output.isEmpty())
        return fail("Say where to write it with -o file.pdf.");
    Backend backend(nullptr);
    if (!openProject(backend, command))
        return 1;
    const QString what = command.argument(0);
    QString error;
    bool written;
    if (what == "rfi" || what == "submittal" || what == "board") {
        written = PdfExport::writeLog(backend.project(), what, backend.today(), output, &error);
    } else {
        const QString item = code(backend, what);
        if (item.isEmpty())
            return 1;
        written = PdfExport::writeForm(backend.project(), backend.project().items[backend.project().indexOf(item)],
                                       backend.today(), output, &error);
    }
    if (!written)
        return fail(error);
    if (command.wantsJson())
        print(QJsonObject{{"file", QFileInfo(output).absoluteFilePath()}});
    else
        print(stdout, QFileInfo(output).absoluteFilePath());
    return 0;
}

int notify(const QStringList &arguments) {
    Command command("notify", "Send a desktop notification for each overdue item not yet announced today, across "
                              "every project. The systemd user timer joist-notify.timer runs this.");
    command.parser.addOption({"dry-run", "List what would be announced, and send nothing"});
    command.json();
    command.parser.process(arguments);
    const QDate today = QDate::currentDate();
    Notifier notifier;
    const QList<Overdue> overdue = Notifier::overdueItems(ProjectStore::files(ProjectStore::defaultDirectory()), today);
    const QList<Overdue> pending = notifier.pending(overdue, today);
    if (!command.parser.isSet("dry-run")) {
        QString error;
        if (!notifier.notify(overdue, today, &error))
            return fail(error);
    }
    if (command.wantsJson()) {
        QJsonArray list;
        for (const Overdue &item : pending)
            list.append(QJsonObject{{"project", item.project}, {"code", item.code}, {"title", item.title},
                                    {"holder", item.holder}, {"daysLate", item.daysLate}});
        print(QJsonObject{{"announced", list}, {"overdue", int(overdue.size())}});
    } else {
        for (const Overdue &item : pending)
            print(stdout, QString("%1\t%2\t%3\t%4 days late").arg(item.project, item.code, item.holder).arg(item.daysLate));
    }
    return 0;
}

int skill(const QStringList &arguments) {
    Command command("skill", "Print the agent skill, or install it for your coding agents with: skill install.");
    command.parser.addPositionalArgument("install", "Copy to ~/.agents/skills/joist and link into ~/.claude/skills",
                                         "[install]");
    command.parser.process(arguments);
    QFile bundled(":/skill.md");
    if (!bundled.open(QIODevice::ReadOnly))
        return fail("The skill is missing from this build.");
    const QByteArray text = bundled.readAll();
    if (command.argument(0).isEmpty()) {
        fputs(text.constData(), stdout);
        return 0;
    }
    if (command.argument(0) != "install")
        return fail("Use joist skill to print the skill, or joist skill install.");

    const QDir home = QDir::home();
    const QString directory = home.filePath(".agents/skills/joist");
    QFile file(directory + "/SKILL.md");
    if (!QDir().mkpath(directory) || !file.open(QIODevice::WriteOnly) || file.write(text) != text.size())
        return fail("Could not write " + file.fileName());
    file.close();
    const QString link = home.filePath(".claude/skills/joist");
    if (!QFileInfo(link).isSymLink() && !QFileInfo::exists(link)) {
        if (!home.mkpath(".claude/skills") || !QFile::link("../../.agents/skills/joist", link))
            return fail("Could not link " + link);
    }
    print(stdout, "Installed " + file.fileName());
    return 0;
}

} // namespace

QString cliSummary() {
    return "Commands:\n"
           "  open [project]                    Open the window\n"
           "  projects                          List the projects\n"
           "  new <name> [-n number]            Start a project\n"
           "  list [--open] [--overdue]         List RFIs and submittals\n"
           "  show <item>                       One item, its history and sheets\n"
           "  add <rfi|submittal> <title>       Log a new item\n"
           "  pass <item> <party>               Put the ball in their court\n"
           "  close <item> <action>             Answer, approve, reject, or withdraw\n"
           "  reopen <item>                     Reopen a closed item\n"
           "  link <item> <set.pdf> <sheet>     Link a Lintel drawing sheet\n"
           "  export <rfi|submittal|board|item> -o file.pdf\n"
           "  notify                            Announce overdue items\n"
           "  skill [install]                   Print the skill for coding agents, or install it\n"
           "\nMost take --json and --project. joist <command> --help says more.";
}

bool isCliCommand(const QString &word) {
    return QStringList{"projects", "new",  "list",   "show",   "add",    "pass",  "close",
                       "reopen",   "link", "export", "notify", "skill", "help"}
        .contains(word);
}

int runCli(const QStringList &arguments) {
    const QString command = arguments.value(1);
    return command == "projects" ? projects(arguments)
         : command == "new"      ? create(arguments)
         : command == "list"     ? list(arguments)
         : command == "show"     ? show(arguments)
         : command == "add"      ? add(arguments)
         : command == "pass"     ? pass(arguments)
         : command == "close"    ? close(arguments)
         : command == "reopen"   ? reopen(arguments)
         : command == "link"     ? link(arguments)
         : command == "export"   ? exportPdf(arguments)
         : command == "notify"   ? notify(arguments)
         : command == "skill"    ? skill(arguments)
         : (print(stdout, "Joist: RFI and submittal logs for construction projects.\n\n" + cliSummary()),
            command.isEmpty() || command == "help" ? 0 : 1);
}
