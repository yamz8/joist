#include "project.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>

namespace {

QString isoDate(const QDate &date) { return date.isValid() ? date.toString(Qt::ISODate) : QString(); }
QDate parseDate(const QJsonValue &value) { return QDate::fromString(value.toString(), Qt::ISODate); }

} // namespace

QString Item::code() const {
    return QString("%1-%2").arg(type == "rfi" ? "RFI" : "SUB").arg(number, 3, 10, QChar('0'));
}

QJsonObject Item::toJson() const {
    QJsonArray events;
    for (const Event &event : history)
        events.append(QJsonObject{{"date", isoDate(event.date)}, {"from", event.from}, {"to", event.to},
                                  {"note", event.note}, {"action", event.action}});
    QJsonArray links;
    for (const SheetLink &link : sheets)
        links.append(QJsonObject{{"file", link.file}, {"sheet", link.sheet}});
    return {{"type", type},
            {"number", number},
            {"revision", revision},
            {"title", title},
            {"question", question},
            {"answer", answer},
            {"spec", spec},
            {"created", isoDate(created)},
            {"due", isoDate(due)},
            {"closed", isoDate(closed)},
            {"ballInCourt", ballInCourt},
            {"action", action},
            {"history", events},
            {"sheets", links}};
}

Item Item::fromJson(const QJsonObject &object) {
    Item item;
    item.type = object["type"].toString() == "rfi" ? "rfi" : "submittal";
    item.number = object["number"].toInt();
    item.revision = object["revision"].toInt();
    item.title = object["title"].toString();
    item.question = object["question"].toString();
    item.answer = object["answer"].toString();
    item.spec = object["spec"].toString();
    item.created = parseDate(object["created"]);
    item.due = parseDate(object["due"]);
    item.closed = parseDate(object["closed"]);
    item.ballInCourt = object["ballInCourt"].toString();
    item.action = object["action"].toString();
    for (const QJsonValue &value : object["history"].toArray()) {
        const QJsonObject event = value.toObject();
        item.history.append({parseDate(event["date"]), event["from"].toString(), event["to"].toString(),
                             event["note"].toString(), event["action"].toString()});
    }
    for (const QJsonValue &value : object["sheets"].toArray()) {
        const QJsonObject link = value.toObject();
        item.sheets.append({link["file"].toString(), link["sheet"].toString()});
    }
    return item;
}

int Project::indexOf(const QString &code) const {
    QString type;
    int number = 0;
    if (!parseCode(code, &type, &number))
        return -1;
    for (int i = 0; i < items.size(); ++i)
        if (items[i].type == type && items[i].number == number)
            return i;
    return -1;
}

int Project::nextNumber(const QString &type) const {
    int highest = 0;
    for (const Item &item : items)
        if (item.type == type)
            highest = qMax(highest, item.number);
    return highest + 1;
}

QJsonObject Project::toJson() const {
    QJsonArray list;
    for (const Item &item : items)
        list.append(item.toJson());
    return {{"name", name},
            {"number", number},
            {"parties", QJsonArray::fromStringList(parties)},
            {"rfiDays", rfiDays},
            {"submittalDays", submittalDays},
            {"items", list}};
}

Project Project::fromJson(const QJsonObject &object) {
    Project project;
    project.name = object["name"].toString();
    project.number = object["number"].toString();
    for (const QJsonValue &value : object["parties"].toArray())
        project.parties.append(value.toString());
    project.rfiDays = object["rfiDays"].toInt(7);
    project.submittalDays = object["submittalDays"].toInt(14);
    for (const QJsonValue &value : object["items"].toArray())
        project.items.append(Item::fromJson(value.toObject()));
    return project;
}

QString ProjectStore::defaultDirectory() {
    // A separate set of projects, for trying things out or for tests.
    const QString override = qEnvironmentVariable("JOIST_PROJECTS");
    if (!override.isEmpty())
        return override;
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/joist/projects";
}

QStringList ProjectStore::files(const QString &directory) {
    QStringList paths;
    const QDir dir(directory);
    for (const QString &name : dir.entryList({"*.json"}, QDir::Files, QDir::Name))
        paths.append(dir.filePath(name));
    return paths;
}

bool ProjectStore::load(const QString &path, Project &project, QString *error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QCoreApplication::translate("ProjectStore", "Could not read %1: %2").arg(path, file.errorString());
        return false;
    }
    QJsonParseError parse;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parse);
    if (!document.isObject()) {
        if (error)
            *error = QCoreApplication::translate("ProjectStore", "%1 is not a Joist project: %2").arg(path, parse.errorString());
        return false;
    }
    project = Project::fromJson(document.object());
    return true;
}

bool ProjectStore::save(const QString &path, const Project &project, QString *error) {
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        if (error)
            *error = QCoreApplication::translate("ProjectStore", "Could not create the folder for %1.").arg(path);
        return false;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)
        || file.write(QJsonDocument(project.toJson()).toJson()) < 0 || !file.commit()) {
        if (error)
            *error = QCoreApplication::translate("ProjectStore", "Could not save %1: %2").arg(path, file.errorString());
        return false;
    }
    return true;
}

QString ProjectStore::pathFor(const QString &directory, const Project &project) {
    QString slug = (project.number.isEmpty() ? project.name : project.number + " " + project.name).toLower();
    slug.replace(QRegularExpression("[^\\w]+", QRegularExpression::UseUnicodePropertiesOption), "-");
    slug = slug.section('-', 0, -1, QString::SectionSkipEmpty);
    if (slug.isEmpty())
        slug = "project";
    const QDir dir(directory);
    QString path = dir.filePath(slug + ".json");
    for (int n = 2; QFileInfo::exists(path); ++n)
        path = dir.filePath(QString("%1-%2.json").arg(slug).arg(n));
    return path;
}

QString ProjectStore::find(const QString &directory, const QString &name) {
    if (name.endsWith(".json") && QFileInfo::exists(name))
        return QFileInfo(name).absoluteFilePath();
    QStringList matches;
    for (const QString &path : files(directory)) {
        Project project;
        if (QFileInfo(path).completeBaseName().compare(name, Qt::CaseInsensitive) == 0) {
            matches = {path};
            break;
        }
        if (load(path, project, nullptr)
            && (project.name.compare(name, Qt::CaseInsensitive) == 0
                || (!project.number.isEmpty() && project.number.compare(name, Qt::CaseInsensitive) == 0)))
            matches.append(path);
    }
    return matches.size() == 1 ? matches.first() : QString();
}

QStringList closingActions(const QString &type) {
    if (type == "rfi")
        return {"answered", "withdrawn"};
    return {"approved", "approved-as-noted", "revise-resubmit", "rejected", "withdrawn"};
}

bool isItemType(const QString &type) { return type == "rfi" || type == "submittal"; }

bool parseCode(const QString &code, QString *type, int *number) {
    static const QRegularExpression expression("^\\s*(?:(rfi|sub|submittal)[-\\s#]*)?0*(\\d+)\\s*$",
                                               QRegularExpression::CaseInsensitiveOption);
    const auto match = expression.match(code);
    if (!match.hasMatch())
        return false;
    const QString prefix = match.captured(1).toLower();
    if (!prefix.isEmpty())
        *type = prefix == "rfi" ? "rfi" : "submittal";
    else if (!isItemType(*type))
        return false;
    *number = match.captured(2).toInt();
    return *number > 0;
}
