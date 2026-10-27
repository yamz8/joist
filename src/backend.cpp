#include "backend.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QStandardPaths>
#include <QUrl>
#include <algorithm>

#include "filepicker.h"
#include "labels.h"
#include "notifier.h"
#include "pdfexport.h"

Backend::Backend(FilePicker *picker, const QString &directory, QObject *parent)
    : QObject(parent), m_picker(picker),
      m_directory(directory.isEmpty() ? ProjectStore::defaultDirectory() : directory) {
    QDir().mkpath(m_directory);

    // A save replaces the file, so watch the folder too and re-arm each time.
    m_reload.setSingleShot(true);
    m_reload.setInterval(150);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { m_reload.start(); });
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this] { m_reload.start(); });
    connect(&m_reload, &QTimer::timeout, this, &Backend::reloadFromDisk);

    // Due dates count in days, so a new day re-renders every countdown.
    m_clock.setInterval(60 * 1000);
    connect(&m_clock, &QTimer::timeout, this, [this, day = QDate::currentDate()]() mutable {
        if (!m_today.isValid() && QDate::currentDate() != day) {
            day = QDate::currentDate();
            refreshProjects();
            emit projectChanged();
            checkOverdue();
        }
    });
    m_clock.start();
    m_overdue.setInterval(30 * 60 * 1000);
    connect(&m_overdue, &QTimer::timeout, this, &Backend::checkOverdue);

    if (m_picker) {
        connect(m_picker, &FilePicker::failed, this, &Backend::reportError);
        connect(m_picker, &FilePicker::openSelected, this, [this](const QUrl &url) {
            if (!m_pendingLink.isEmpty())
                listSheets(m_pendingLink, url.toLocalFile());
            m_pendingLink.clear();
        });
        connect(m_picker, &FilePicker::saveSelected, this, &Backend::saveTo);
    }

    refreshProjects();
    watch();
}

void Backend::setToday(const QDate &today) {
    m_today = today;
    refreshProjects();
    emit projectChanged();
}

void Backend::setNotifier(Notifier *notifier) {
    m_notifier = notifier;
    if (m_notifier)
        m_overdue.start();
}

// Projects -------------------------------------------------------------------

void Backend::refreshProjects() {
    QVariantList list;
    for (const QString &path : ProjectStore::files(m_directory)) {
        Project project;
        if (!ProjectStore::load(path, project, nullptr))
            continue;
        int open = 0, overdue = 0;
        for (const Item &item : project.items) {
            open += item.isOpen();
            overdue += item.isOverdue(today());
        }
        list.append(QVariantMap{{"file", path}, {"name", project.name}, {"number", project.number},
                                {"open", open}, {"overdue", overdue},
                                {"parties", project.parties}});
    }
    std::stable_sort(list.begin(), list.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap()["name"].toString().localeAwareCompare(b.toMap()["name"].toString()) < 0;
    });
    if (list != m_projects) {
        m_projects = list;
        emit projectsChanged();
    }
}

void Backend::watch() {
    const QStringList paths = m_watcher.files() + m_watcher.directories();
    if (!paths.isEmpty())
        m_watcher.removePaths(paths);
    m_watcher.addPath(m_directory);
    if (!m_file.isEmpty() && QFileInfo::exists(m_file))
        m_watcher.addPath(m_file);
}

void Backend::reloadFromDisk() {
    refreshProjects();
    if (!m_file.isEmpty()) {
        QFile file(m_file);
        if (file.open(QIODevice::ReadOnly)) {
            const QByteArray contents = file.readAll();
            // Our own saves come back here too; only someone else's reload.
            if (contents != m_saved) {
                Project project;
                QString error;
                if (ProjectStore::load(m_file, project, &error)) {
                    m_project = project;
                    m_saved = contents;
                    m_undo.clear();
                    m_redo.clear();
                    emit projectChanged();
                }
            }
        }
    }
    watch();
}

void Backend::openLast() {
    QFile last(m_directory + "/.last");
    QString path = last.open(QIODevice::ReadOnly) ? QString::fromUtf8(last.readAll()).trimmed() : QString();
    if (!QFileInfo::exists(path) && m_projects.size() == 1)
        path = m_projects.first().toMap()["file"].toString();
    if (QFileInfo::exists(path))
        openProject(path);
}

bool Backend::createProject(const QString &name, const QString &number) {
    if (name.trimmed().isEmpty()) {
        reportError(tr("Give the project a name."));
        return false;
    }
    Project project;
    project.name = name.trimmed();
    project.number = number.trimmed();
    // The usual three, in the order an RFI travels; the team dialog edits them.
    project.parties = {tr("Contractor"), tr("Architect"), tr("Owner")};
    const QString path = ProjectStore::pathFor(m_directory, project);
    QString error;
    if (!ProjectStore::save(path, project, &error)) {
        reportError(error);
        return false;
    }
    refreshProjects();
    return openProject(path);
}

bool Backend::openProject(const QString &file, bool remember) {
    Project project;
    QString error;
    if (!ProjectStore::load(file, project, &error)) {
        reportError(error);
        return false;
    }
    QFile source(file);
    m_saved = source.open(QIODevice::ReadOnly) ? source.readAll() : QByteArray();
    m_file = QFileInfo(file).absoluteFilePath();
    m_project = project;
    m_undo.clear();
    m_redo.clear();
    QFile last(m_directory + "/.last");
    if (remember && last.open(QIODevice::WriteOnly))
        last.write(m_file.toUtf8());
    watch();
    emit projectChanged();
    return true;
}

void Backend::renameProject(const QString &name, const QString &number) {
    if (name.trimmed().isEmpty())
        return;
    change([&](Project &project) {
        project.name = name.trimmed();
        project.number = number.trimmed();
        return true;
    });
}

// Changes --------------------------------------------------------------------

bool Backend::change(const std::function<bool(Project &)> &edit) {
    if (!hasProject())
        return false;
    const QJsonObject before = m_project.toJson();
    Project edited = m_project;
    if (!edit(edited) || edited.toJson() == before)
        return false;
    m_project = edited;
    m_undo.append(before);
    m_redo.clear();
    save();
    emit projectChanged();
    return true;
}

bool Backend::save() {
    QString error;
    if (!ProjectStore::save(m_file, m_project, &error)) {
        reportError(error);
        return false;
    }
    QFile file(m_file);
    m_saved = file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    refreshProjects();
    watch();
    return true;
}

void Backend::undo() {
    if (m_undo.isEmpty())
        return;
    m_redo.append(m_project.toJson());
    m_project = Project::fromJson(m_undo.takeLast());
    save();
    emit projectChanged();
}

void Backend::redo() {
    if (m_redo.isEmpty())
        return;
    m_undo.append(m_project.toJson());
    m_project = Project::fromJson(m_redo.takeLast());
    save();
    emit projectChanged();
}

Item *Backend::find(Project &project, const QString &code) {
    const int index = project.indexOf(code);
    return index < 0 ? nullptr : &project.items[index];
}

// Parties --------------------------------------------------------------------

bool Backend::addParty(const QString &name) {
    const QString party = name.trimmed();
    if (party.isEmpty())
        return false;
    return change([&](Project &project) {
        if (project.parties.contains(party))
            return false;
        project.parties.append(party);
        return true;
    });
}

bool Backend::removeParty(const QString &name) {
    for (const Item &item : m_project.items)
        if (item.isOpen() && item.ballInCourt == name) {
            reportError(tr("%1 still holds %2. Pass it on before removing them.").arg(name, item.code()));
            return false;
        }
    return change([&](Project &project) { return project.parties.removeAll(name) > 0; });
}

bool Backend::renameParty(const QString &from, const QString &to) {
    const QString name = to.trimmed();
    if (name.isEmpty() || name == from)
        return false;
    if (m_project.parties.contains(name)) {
        reportError(tr("%1 is already on the team.").arg(name));
        return false;
    }
    // A new company name carries through every item and its history.
    return change([&](Project &project) {
        const int index = project.parties.indexOf(from);
        if (index < 0)
            return false;
        project.parties[index] = name;
        for (Item &item : project.items) {
            if (item.ballInCourt == from)
                item.ballInCourt = name;
            for (Event &event : item.history) {
                if (event.from == from)
                    event.from = name;
                if (event.to == from)
                    event.to = name;
            }
        }
        return true;
    });
}

void Backend::moveParty(const QString &name, int step) {
    change([&](Project &project) {
        const int from = project.parties.indexOf(name);
        const int to = from + step;
        if (from < 0 || to < 0 || to >= project.parties.size())
            return false;
        project.parties.move(from, to);
        return true;
    });
}

// Items ----------------------------------------------------------------------

QString Backend::addItem(const QString &type, const QString &title, const QString &from, const QString &to,
                         int days) {
    if (!hasProject() || !isItemType(type))
        return {};
    QString code;
    change([&](Project &project) {
        Item item;
        item.type = type;
        item.number = project.nextNumber(type);
        item.title = title.trimmed();
        item.created = today();
        // The first party raises items and the second answers them, which is
        // the contractor and the architect unless the team says otherwise.
        const QString sender = from.isEmpty() ? project.parties.value(0) : from;
        const QString receiver = !to.isEmpty() ? to : project.parties.value(1, sender);
        item.ballInCourt = receiver;
        item.due = today().addDays(days >= 0 ? days : project.defaultDays(type));
        item.history.append({today(), sender, receiver, {}, "submitted"});
        project.items.append(item);
        code = item.code();
        return true;
    });
    if (!code.isEmpty())
        emit itemAdded(code);
    return code;
}

QVariantMap Backend::itemMap(const Item &item) const {
    QVariantList history;
    for (const Event &event : item.history)
        history.append(QVariantMap{{"date", event.date.toString(Qt::ISODate)}, {"from", event.from},
                                   {"to", event.to}, {"note", event.note}, {"action", event.action}});
    QVariantList sheets;
    for (const SheetLink &link : item.sheets)
        sheets.append(QVariantMap{{"file", link.file}, {"fileName", QFileInfo(link.file).fileName()},
                                  {"sheet", link.sheet}, {"exists", QFileInfo::exists(link.file)}});
    const QDate held = item.history.isEmpty() ? item.created : item.history.last().date;
    return {{"code", item.code()},
            {"type", item.type},
            {"number", item.number},
            {"revision", item.revision},
            {"title", item.title},
            {"question", item.question},
            {"answer", item.answer},
            {"spec", item.spec},
            {"created", item.created.toString(Qt::ISODate)},
            {"due", item.due.toString(Qt::ISODate)},
            {"closed", item.closed.toString(Qt::ISODate)},
            {"heldSince", held.toString(Qt::ISODate)},
            {"ballInCourt", item.ballInCourt},
            {"originator", item.originator()},
            {"action", item.action},
            {"open", item.isOpen()},
            {"overdue", item.isOverdue(today())},
            {"daysLeft", item.due.isValid() ? QVariant(int(today().daysTo(item.due))) : QVariant()},
            {"history", history},
            {"sheets", sheets}};
}

QVariantList Backend::items() const {
    QList<const Item *> sorted;
    for (const Item &item : m_project.items)
        sorted.append(&item);
    std::stable_sort(sorted.begin(), sorted.end(), [](const Item *a, const Item *b) {
        return a->type != b->type ? a->type < b->type : a->number < b->number;
    });
    QVariantList list;
    for (const Item *item : sorted)
        list.append(itemMap(*item));
    return list;
}

QVariantList Backend::board() const {
    QStringList holders = m_project.parties;
    for (const Item &item : m_project.items)
        if (item.isOpen() && !holders.contains(item.ballInCourt))
            holders.append(item.ballInCourt);
    QVariantList columns;
    for (const QString &party : holders) {
        QList<const Item *> held;
        for (const Item &item : m_project.items)
            if (item.isOpen() && item.ballInCourt == party)
                held.append(&item);
        // Most urgent first; items with no due date last.
        std::stable_sort(held.begin(), held.end(), [](const Item *a, const Item *b) {
            if (a->due.isValid() != b->due.isValid())
                return a->due.isValid();
            return a->due < b->due;
        });
        QVariantList list;
        int overdue = 0;
        for (const Item *item : held) {
            list.append(itemMap(*item));
            overdue += item->isOverdue(today());
        }
        columns.append(QVariantMap{{"party", party}, {"items", list}, {"overdue", overdue}});
    }
    return columns;
}

QVariantMap Backend::item(const QString &code) const {
    const int index = m_project.indexOf(code);
    return index < 0 ? QVariantMap() : itemMap(m_project.items[index]);
}

void Backend::setField(const QString &code, const QString &field, const QString &value) {
    change([&](Project &project) {
        Item *item = find(project, code);
        if (!item)
            return false;
        if (field == "title") item->title = value;
        else if (field == "question") item->question = value;
        else if (field == "answer") item->answer = value;
        else if (field == "spec") item->spec = value.trimmed();
        else if (field == "due") {
            const QDate due = QDate::fromString(value, Qt::ISODate);
            if (!due.isValid())
                return false;
            item->due = due;
        } else
            return false;
        return true;
    });
}

void Backend::shiftDue(const QString &code, int days) {
    change([&](Project &project) {
        Item *item = find(project, code);
        if (!item || !item->isOpen())
            return false;
        item->due = (item->due.isValid() ? item->due : today()).addDays(days);
        return true;
    });
}

QString Backend::suggestedRecipient(const QString &code) const {
    const int index = m_project.indexOf(code);
    if (index < 0)
        return m_project.parties.value(0);
    const Item &item = m_project.items[index];
    // Back to whoever passed it here, else the next party along.
    for (auto it = item.history.crbegin(); it != item.history.crend(); ++it)
        if (it->to == item.ballInCourt && !it->from.isEmpty() && it->from != item.ballInCourt)
            return it->from;
    const int at = m_project.parties.indexOf(item.ballInCourt);
    return m_project.parties.value((at + 1) % qMax(1, int(m_project.parties.size())));
}

void Backend::passTo(const QString &code, const QString &party, int days, const QString &note) {
    if (party.isEmpty())
        return;
    change([&](Project &project) {
        Item *item = find(project, code);
        if (!item || !item->isOpen())
            return false;
        item->history.append({today(), item->ballInCourt, party, note.trimmed(), {}});
        item->ballInCourt = party;
        item->due = today().addDays(qMax(0, days));
        return true;
    });
}

void Backend::closeItem(const QString &code, const QString &action, const QString &answer, const QString &note) {
    change([&](Project &project) {
        Item *item = find(project, code);
        if (!item || !item->isOpen() || !::closingActions(item->type).contains(action))
            return false;
        item->answer = answer;
        const QString back = item->originator().isEmpty() ? project.parties.value(0) : item->originator();
        item->history.append({today(), item->ballInCourt, back, note.trimmed(), action});
        // A submittal sent back for revision stays open as the next revision,
        // in the hands of whoever submitted it.
        if (action == "revise-resubmit") {
            ++item->revision;
            item->ballInCourt = back;
            item->due = today().addDays(project.defaultDays(item->type));
            return true;
        }
        item->action = action;
        item->closed = today();
        item->ballInCourt.clear();
        return true;
    });
}

void Backend::reopen(const QString &code) {
    change([&](Project &project) {
        Item *item = find(project, code);
        if (!item || item->isOpen())
            return false;
        // Back with whoever closed it.
        const QString holder = item->history.isEmpty() ? project.parties.value(1) : item->history.last().from;
        item->history.append({today(), {}, holder, {}, "reopened"});
        item->ballInCourt = holder;
        item->closed = QDate();
        item->action.clear();
        item->due = today().addDays(project.defaultDays(item->type));
        return true;
    });
}

void Backend::deleteItem(const QString &code) {
    change([&](Project &project) {
        const int index = project.indexOf(code);
        if (index < 0)
            return false;
        project.items.removeAt(index);
        return true;
    });
}

QStringList Backend::closingActions(const QString &type) const { return ::closingActions(type); }

QString Backend::actionLabel(const QString &action) const { return Labels::action(action); }

QString Backend::typeLabel(const QString &type) const { return Labels::type(type); }

QString Backend::statusLabel(const QVariantMap &item) const {
    return Labels::status(item["open"].toBool(), item["overdue"].toBool(), item["action"].toString());
}

// Lintel ---------------------------------------------------------------------

void Backend::linkSheet(const QString &code) {
    if (m_project.indexOf(code) < 0 || !m_picker)
        return;
    m_pendingLink = code;
    m_picker->openPdf(tr("Pick a drawing set"));
}

void Backend::listSheets(const QString &code, const QString &file) {
    if (file.isEmpty())
        return;
    auto *process = new QProcess(this);
    m_listingSheets = true;
    emit listingSheetsChanged();
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        reportError(tr("Joist reads drawing sets with Lintel, which isn't installed."));
        m_listingSheets = false;
        emit listingSheetsChanged();
        process->deleteLater();
    });
    connect(process, &QProcess::finished, this, [this, process, code, file](int exitCode, QProcess::ExitStatus status) {
        process->deleteLater();
        m_listingSheets = false;
        emit listingSheetsChanged();
        if (status != QProcess::NormalExit || exitCode != 0) {
            const QString message = QString::fromLocal8Bit(process->readAllStandardError()).trimmed();
            reportError(tr("Lintel could not read %1: %2").arg(QFileInfo(file).fileName(), message));
            return;
        }
        QStringList sheets;
        const QJsonArray list = QJsonDocument::fromJson(process->readAllStandardOutput()).object()["sheets"].toArray();
        for (const QJsonValue &value : list) {
            const QJsonObject sheet = value.toObject();
            const QString label = sheet["label"].toString();
            sheets.append(label.isEmpty() ? QString::number(sheet["number"].toInt()) : label);
        }
        if (sheets.isEmpty()) {
            reportError(tr("%1 has no sheets.").arg(QFileInfo(file).fileName()));
            return;
        }
        emit sheetsListed(code, file, sheets);
    });
    process->start(m_lintel, {"sheets", file, "--json"});
}

void Backend::addSheetLink(const QString &code, const QString &file, const QString &sheet) {
    change([&](Project &project) {
        Item *item = find(project, code);
        if (!item || sheet.isEmpty())
            return false;
        for (const SheetLink &link : item->sheets)
            if (link.file == file && link.sheet == sheet)
                return false;
        item->sheets.append({file, sheet});
        return true;
    });
}

void Backend::removeSheetLink(const QString &code, int index) {
    change([&](Project &project) {
        Item *item = find(project, code);
        if (!item || index < 0 || index >= item->sheets.size())
            return false;
        item->sheets.removeAt(index);
        return true;
    });
}

void Backend::openSheet(const QString &file, const QString &sheet) {
    if (!QFileInfo::exists(file)) {
        reportError(tr("%1 isn't there any more.").arg(file));
        return;
    }
    // Lintel opens straight to the sheet where it knows how; otherwise it
    // opens the set.
    if (m_lintelJumps < 0) {
        QProcess help;
        help.start(m_lintel, {"open", "--help"});
        m_lintelJumps = help.waitForFinished(3000) && help.readAllStandardOutput().contains("--sheet") ? 1 : 0;
    }
    QStringList arguments{"open"};
    if (m_lintelJumps == 1 && !sheet.isEmpty())
        arguments << "--sheet" << sheet;
    arguments << file;
    if (!QProcess::startDetached(m_lintel, arguments))
        reportError(tr("Joist opens drawing sets with Lintel, which isn't installed."));
}

// Exports --------------------------------------------------------------------

void Backend::exportLog(const QString &kind) {
    if (!hasProject() || !m_picker)
        return;
    m_pendingExport = kind;
    const QString folder = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    m_picker->savePdf(QUrl::fromLocalFile(QDir(folder).filePath(PdfExport::suggestedName(m_project, kind, today()))),
                      tr("Export to PDF"));
}

void Backend::exportForm(const QString &code) {
    const int index = m_project.indexOf(code);
    if (index < 0 || !m_picker)
        return;
    m_pendingExport = code;
    const QString folder = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    m_picker->savePdf(
        QUrl::fromLocalFile(QDir(folder).filePath(PdfExport::suggestedName(m_project, m_project.items[index]))),
        tr("Export to PDF"));
}

void Backend::saveTo(const QUrl &url) {
    const QString kind = m_pendingExport;
    m_pendingExport.clear();
    QString path = url.toLocalFile();
    if (kind.isEmpty() || path.isEmpty())
        return;
    if (!path.endsWith(".pdf", Qt::CaseInsensitive))
        path += ".pdf";
    QString error;
    bool written;
    if (kind == "rfi" || kind == "submittal" || kind == "board") {
        written = PdfExport::writeLog(m_project, kind, today(), path, &error);
    } else {
        const int index = m_project.indexOf(kind);
        if (index < 0)
            return;
        written = PdfExport::writeForm(m_project, m_project.items[index], today(), path, &error);
    }
    if (written)
        emit exported(path);
    else
        reportError(error);
}

// Errors and notifications ---------------------------------------------------

void Backend::reportError(const QString &message) {
    if (m_error != message) {
        m_error = message;
        emit errorChanged();
    }
}

void Backend::clearError() { reportError({}); }

void Backend::checkOverdue() {
    if (!m_notifier)
        return;
    QString error;
    if (!m_notifier->notify(Notifier::overdueItems(ProjectStore::files(m_directory), today()), today(), &error))
        reportError(error);
}
